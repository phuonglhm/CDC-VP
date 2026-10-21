#include "fu3/qspi/qspi.h"
#include "fu3/qspi/a2h_qspi.h"
#include "fu3_bus/bus_system.h"
#include "fu3/qspi/qspi_flash_model.h"
#include <iostream>
#include <stdexcept>

using namespace sc_core;
using namespace fu3::qspi;
// Independent known bytes, including RV32 instructions, loaded into the NOR model.
const std::vector<unsigned char> image_bytes={0x13,0x05,0x10,0x00,0x93,0x05,0x20,0x00,
                                            0x33,0x06,0xb5,0x00,0x73,0x00,0x10,0x00};
struct Platform : sc_module {
    static fu3_bus::BusConfig config() {
        auto c=fu3_bus::BusConfig::fu3();
        for(auto& t:c.targets) t.enabled=t.name=="AP_QSPIC" || t.name=="QSPI_XIP";
        return c;
    }
    fu3_bus::BusSystem fabric{"fabric",config()};
    tlm_utils::simple_initiator_socket<Platform> cpu{"cpu"}, dma{"dma"};
    sc_clock clk{"clk",10,SC_NS}, pclk{"pclk",14,SC_NS}, hclk{"hclk",22,SC_NS};
    sc_signal<bool> rst{"rst"}, clr{"clr"}, irq{"irq"}, rxreq{"rxreq"}, txreq{"txreq"};
    Qspi qspi{"qspi"}; A2hQspi a2h{"a2h"}; QspiFlashModel image{"image"};
    bool passed=false;
    unsigned checks=0;
    SC_HAS_PROCESS(Platform);
    explicit Platform(sc_module_name n):sc_module(n) {
        cpu.bind(fabric.initiator("CVA6")); dma.bind(fabric.initiator("DMA0"));
        fabric.target("AP_QSPIC").bind(qspi.ap_qspic);
        fabric.target("QSPI_XIP").bind(a2h.socket); a2h.ahb.bind(qspi.xip);
        image.reset_n(rst); image.load(image_bytes);
        qspi.flash.bind(image.socket); qspi.clk(clk); qspi.pclk(pclk); qspi.hclk(hclk); qspi.dma_clk(pclk);
        qspi.reset_n(rst); qspi.preset_n(rst); qspi.hreset_n(rst); qspi.dma_reset_n(rst);
        qspi.dma_rx_clr(clr); qspi.dma_tx_clr(clr); qspi.irq(irq); qspi.dma_rx_req(rxreq); qspi.dma_tx_req(txreq);
        SC_THREAD(run);
    }
    void check(bool value,const char* text) { ++checks; if(!value) throw std::runtime_error(text); }
    tlm::tlm_response_status access(bool from_dma,bool write,std::uint64_t addr,unsigned char* bytes,
                                  unsigned length=4,unsigned beat=4,bool fixed=false,bool fetch=false) {
        tlm::tlm_generic_payload tx;
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(addr); tx.set_data_ptr(bytes); tx.set_data_length(length); tx.set_streaming_width(length);
        fu3_bus::ScopedAccess metadata(tx,beat,fixed?fu3_bus::Burst::Fixed:fu3_bus::Burst::Increment,fetch);
        sc_time delay=SC_ZERO_TIME;
        if(from_dma) dma->b_transport(tx,delay); else cpu->b_transport(tx,delay);
        if(delay!=SC_ZERO_TIME) wait(delay);
        check(tx.get_address()==addr && tx.get_data_length()==length,"fabric restores payload");
        return tx.get_response_status();
    }
    void wr(unsigned off,std::uint32_t v) {
        unsigned char bytes[4]{}; for(unsigned i=0;i<4;++i) bytes[i]=static_cast<unsigned char>(v>>(8*i));
        check(access(false,true,0x44000000+off,bytes)==tlm::TLM_OK_RESPONSE,"AP_QSPIC route");
    }
    std::uint32_t rd(unsigned off) {
        unsigned char bytes[4]{};
        check(access(false,false,0x44000000+off,bytes)==tlm::TLM_OK_RESPONSE,"AP read route");
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i) value|=std::uint32_t(bytes[i])<<(8*i);
        return value;
    }
    void complete() {
        for(unsigned i=0;i<2000;++i) {
            const auto s=rd(8); check(!(s&1),"no controller transfer error");
            if(s&2) return;
        }
        check(false,"controller timeout");
    }
    void command(unsigned opcode) { wr(12,63); wr(20,0x100|opcode); complete(); }
    void poll_flash() {
        wr(12,63); wr(0,0x00400001); // APMS + EN
        wr(16,0); wr(36,1); wr(40,0); wr(44,10);
        wr(20,0x09000105); // Automatic status polling of WIP.
        for(unsigned i=0;i<10000;++i) {
            const auto s=rd(8); check(!(s&1),"polling flash response");
            if((s&0x50)==0x10) return;
        }
        check(false,"flash polling timeout");
    }
    void run() {
        try {
            rst=false; wait(31,SC_NS); rst=true; wait(20,SC_NS);
            wr(4,16u<<16); wr(0,0x00000001); wr(20,0x0d002503);
            unsigned char data[16]{};
            check(access(false,false,0x30000000,data,16,4,false,true)==tlm::TLM_OK_RESPONSE,"CVA6 four-beat instruction fetch");
            for(unsigned i=0;i<16;++i) check(data[i]==image_bytes[i],"XIP image bytes");
            check(access(false,false,0x30000000,data,1,1)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"narrow fetch rejected at word-only AHB");
            check(access(false,true,0x44000004,data,1,1)==tlm::TLM_BURST_ERROR_RESPONSE,"narrow AP register write rejected");
            check(access(false,false,0x30020000,data)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"FU3 aperture boundary");
            wr(16,7); wr(20,0x05002503); wr(24,0); wait(3,SC_US);
            check(access(true,false,0x44000020,data,8,4,true)==tlm::TLM_OK_RESPONSE,"DMA FIXED DR burst");
            for(unsigned i=0;i<8;++i) check(data[i]==image_bytes[i],"DMA FIFO byte order");
            // CPU WREN/program, controller polling of flash WIP, then XIP fetch.
            command(6); wr(12,63); wr(16,7); wr(20,0x01002502); wr(24,256);
            wr(32,0x12345678); wr(32,0xabcdef01); complete();
            check(image.busy(),"controller TCF precedes flash program completion");
            poll_flash(); check(!image.busy(),"polling waits for NOR completion");
            wr(20,0x0d002503);
            check(access(false,false,0x30000100,data,8,4,false,true)==tlm::TLM_OK_RESPONSE,"XIP fetch programmed image");
            const unsigned char expected[]={0x78,0x56,0x34,0x12,1,0xef,0xcd,0xab};
            for(unsigned i=0;i<8;++i) check(data[i]==expected[i],"NOR program through bus");
            command(6); wr(12,63); wr(20,0x00002520); wr(24,256); complete();
            poll_flash(); wr(20,0x0d002503);
            check(access(false,false,0x30000100,data)==tlm::TLM_OK_RESPONSE,"XIP after sector erase");
            for(unsigned i=0;i<4;++i) check(data[i]==255,"erased bytes");
            // Enable QE through the controller and exercise two SIOO frames.
            command(6); wr(12,63); wr(16,0); wr(20,0x01000131); wr(32,2); complete(); poll_flash();
            wr(28,0xa0); wr(20,0x1f10edeb);
            check(access(false,false,0x30000000,data,8,4,false,true)==tlm::TLM_OK_RESPONSE,"quad IO continuous XIP across frames");
            for(unsigned i=0;i<8;++i) check(data[i]==255,"continuous read data");
            passed=true;
            std::cout<<"PASS bus assertions="<<checks<<" time="<<sc_time_stamp()<<'\n';
        } catch(const std::exception& e) { std::cerr<<"FAIL bus: "<<e.what()<<'\n'; }
        sc_stop();
    }
};
int sc_main(int,char**) { Platform p{"platform"}; sc_start(sc_time(1,SC_MS)); return p.passed?0:1; }
