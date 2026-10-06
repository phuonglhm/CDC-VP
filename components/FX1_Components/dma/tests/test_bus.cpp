#include "dma/dma.h"
#include "bus/bus_system.h"
#include <algorithm>
#include <iostream>
#include <vector>

using namespace sc_core;
using namespace fx1::dma;
using namespace fx1::dma::reg;

namespace {
class Ram : public sc_module {
public:
    tlm_utils::simple_target_socket<Ram> target{"target"};
    std::vector<unsigned char> bytes = std::vector<unsigned char>(0x20000);
    explicit Ram(sc_module_name name) : sc_module(name) { target.register_b_transport(this, &Ram::transport); }
    void transport(tlm::tlm_generic_payload& tx, sc_time& delay) {
        const auto address = tx.get_address();
        if (address >= bytes.size() || tx.get_data_length() > bytes.size()-address) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
        }
        if (tx.is_read()) std::copy_n(bytes.data()+address, tx.get_data_length(), tx.get_data_ptr());
        else if (tx.is_write()) std::copy_n(tx.get_data_ptr(), tx.get_data_length(), bytes.data()+address);
        else { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
        delay += sc_time(4, SC_NS);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class Driver : public sc_module {
public:
    tlm_utils::simple_initiator_socket<Driver> cpu{"cpu"};
    sc_signal<bool> reset_n{"reset_n"}, irq{"irq"};
    sc_signal<std::uint32_t> rx_request{"rx_request"}, tx_request{"tx_request"};
    sc_signal<std::uint32_t> rx_clear{"rx_clear"}, tx_clear{"tx_clear"};
    unsigned errors = 0;
    SC_HAS_PROCESS(Driver);
    Driver(sc_module_name name, Ram& source, Ram& destination)
        : sc_module(name), source_(source), destination_(destination) { SC_THREAD(run); }
private:
    Ram& source_; Ram& destination_;
    static constexpr std::uint64_t BASE = 0x40000000;
    std::uint32_t access(unsigned offset, bool write, std::uint32_t value=0,
                         tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE) {
        unsigned char bytes[4];
        for(unsigned i=0;i<4;++i) bytes[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(BASE+offset); tx.set_data_ptr(bytes); tx.set_data_length(4); tx.set_streaming_width(4);
        sc_time delay=SC_ZERO_TIME; cpu->b_transport(tx,delay); if(delay!=SC_ZERO_TIME) wait(delay);
        if(tx.get_response_status()!=expected) { ++errors; std::cerr<<"Unexpected bus response at "<<offset<<'\n'; }
        return std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
    }
    void run() {
        reset_n.write(false); wait(5,SC_NS); reset_n.write(true); wait(5,SC_NS);
        for(unsigned i=0;i<256;++i) source_.bytes[0x1000+i]=static_cast<unsigned char>(i^0xab);
        access(CH_ENABLE,true,0);
        access(CH_CMD_READ_ADDR,true,0x10001000); access(CH_CMD_WRITE_ADDR,true,0x20002000);
        access(CH_CMD_TRANSFER_SIZE,true,256); access(CH_CMD_CONTROL,true,3);
        access(CH_READ_CONFIG,true,0xc4010008); access(CH_WRITE_CONFIG,true,0xc4010008);
        access(CH_ENABLE,true,1); access(CORE_CHANNEL_START,true,1);
        unsigned polls=0;
        while(access(CH_ACTIVE_STATUS,false) && polls++<1000) wait(1,SC_NS);
        if(polls>=1000 || !std::equal(source_.bytes.begin()+0x1000,source_.bytes.begin()+0x1100,destination_.bytes.begin()+0x2000)) ++errors;
        if(access(CORE_STATUS,false)!=1 || !irq.read() || access(CH_TRANSFER_COUNT,false)!=0x10001) ++errors;
        access(CH_INTERRUPT_CLEAR,true,1); wait(1,SC_NS); if(irq.read()) ++errors;
        // Both register holes and the unused channel aperture propagate an APB slave error.
        access(0x28,false,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(0x800,false,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        if(access(CORE_CAPABILITY_STATUS0,false)!=0x0a602258) ++errors;
        std::cout<<"[FX1 DMA] bus integration: "<<(errors?"FAIL":"PASS")<<" ("<<errors<<" errors)\n";
        sc_stop();
    }
};
}

int sc_main(int, char**) {
    bus::BusConfig cfg;
    // DMA uses 0x1100 bytes; a default 4 KB peripheral slot is insufficient.
    cfg.pp1_ports=1;
    cfg.peribus1={{"DMA",0x40000000,0x40001100,0,true}};
    cfg.pp0_ports=1;
    cfg.peribus0={{"test target",0x52000000,0x52001000,0,true}};
    bus::BusSystem fabric{"fabric",cfg};
    Ram rom{"rom"}, source{"source"}, destination{"destination"}, aes{"aes"}, qspi{"qspi"}, pp0{"pp0"};
    Driver driver{"driver",source,destination}; Dma dma{"dma"};
    driver.cpu.bind(fabric.target); dma.master_socket.bind(fabric.target);
    fabric.rom.socket.bind(rom.target); fabric.isram.socket.bind(source.target);
    fabric.dsram.socket.bind(destination.target); fabric.aes.socket.bind(aes.target); fabric.qspi.socket.bind(qspi.target);
    fabric.pp1[0].socket.bind(dma.target_socket); fabric.pp0[0].socket.bind(pp0.target);
    dma.reset_n(driver.reset_n); dma.irq(driver.irq); dma.rx_request(driver.rx_request); dma.tx_request(driver.tx_request);
    dma.rx_clear(driver.rx_clear); dma.tx_clear(driver.tx_clear);
    sc_start(1,SC_MS);
    return driver.errors || !sc_end_of_simulation_invoked()?1:0;
}
