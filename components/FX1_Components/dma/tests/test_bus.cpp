// FX1 DMA through the FX1 bus (named-port API): CPU-side register access over
// SYSBUS_1 -> SYSBUS_0 -> A2P -> PERIBUS_0 to SYS_DMA_CSR, DMA master traffic
// over the SYS_DMA initiator port to two RAM targets.
#include "dma/dma.h"
#include "bus/bus_system.h"
#include "fx1/fx1_memory_map.h"
#include <algorithm>
#include <iostream>
#include <vector>

using namespace sc_core;
using namespace fx1::dma;
using namespace fx1::dma::reg;

namespace {
constexpr std::uint64_t kSourceBase = 0x80000000, kDestBase = 0x80100000, kRamSize = 0x20000;

class Ram : public sc_module {
public:
    tlm_utils::simple_target_socket<Ram> target{"target"};
    std::vector<unsigned char> bytes = std::vector<unsigned char>(kRamSize);
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
    static constexpr std::uint64_t BASE = FX1_SYS_DMA_CSR_BASE;
    std::uint32_t access(unsigned offset, bool write, std::uint32_t value=0,
                         tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE, unsigned length=4) {
        unsigned char bytes[4];
        for(unsigned i=0;i<4;++i) bytes[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(BASE+offset); tx.set_data_ptr(bytes); tx.set_data_length(length); tx.set_streaming_width(length);
        sc_time delay=SC_ZERO_TIME; cpu->b_transport(tx,delay); if(delay!=SC_ZERO_TIME) wait(delay);
        if(tx.get_response_status()!=expected) {
            ++errors; std::cerr<<"Unexpected bus response "<<tx.get_response_string()<<" at 0x"<<std::hex<<offset<<std::dec<<'\n';
        }
        return std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
    }
    void run() {
        reset_n.write(false); wait(5,SC_NS); reset_n.write(true); wait(5,SC_NS);
        for(unsigned i=0;i<256;++i) source_.bytes[0x1000+i]=static_cast<unsigned char>(i^0xab);
        access(CH_ENABLE,true,0);
        access(CH_CMD_READ_ADDR,true,kSourceBase+0x1000); access(CH_CMD_WRITE_ADDR,true,kDestBase+0x2000);
        access(CH_CMD_TRANSFER_SIZE,true,256); access(CH_CMD_CONTROL,true,3);
        access(CH_READ_CONFIG,true,0xc4010008); access(CH_WRITE_CONFIG,true,0xc4010008);
        access(CH_ENABLE,true,1); access(CORE_CHANNEL_START,true,1);
        unsigned polls=0;
        while(access(CH_ACTIVE_STATUS,false) && polls++<1000) wait(1,SC_NS);
        if(polls>=1000 || !std::equal(source_.bytes.begin()+0x1000,source_.bytes.begin()+0x1100,destination_.bytes.begin()+0x2000)) {
            ++errors; std::cerr<<"M2M copy through the bus failed\n";
        }
        if(access(CORE_STATUS,false)!=1 || !irq.read() || access(CH_TRANSFER_COUNT,false)!=0x10001) {
            ++errors; std::cerr<<"completion status/IRQ wrong\n";
        }
        access(CH_INTERRUPT_CLEAR,true,1); wait(1,SC_NS); if(irq.read()) ++errors;
        // Register holes and the unused channel aperture propagate the DMA's
        // decode error through both bus layers and the APB bridge.
        access(0x28,false,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(0x800,false,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        // The whole 64 KiB APB slot decodes to the DMA; beyond its 0x1100 bytes it refuses.
        access(0x1100,false,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        // APB3 register access is 32-bit only: a byte access is refused (the
        // FX1 CPU port turns this into a guest access fault).
        access(CORE_STATUS,false,0,tlm::TLM_BURST_ERROR_RESPONSE,1);
        if(access(CORE_CAPABILITY_STATUS0,false)!=0x0a602258) ++errors;
        std::cout<<"[FX1 DMA] bus integration: "<<(errors?"FAIL":"PASS")<<" ("<<errors<<" errors)\n";
        sc_stop();
    }
};

bus::BusConfig config() {
    bus::BusConfig cfg;
    cfg.initiators = {{"CPU1"}, {"SYS_DMA"}};
    cfg.targets = {
        {"SOURCE", kSourceBase, kRamSize, bus::TargetPath::SysBus1Axi, true},
        {"DEST", kDestBase, kRamSize, bus::TargetPath::SysBus1Axi, true},
        {"SYS_DMA_CSR", FX1_SYS_DMA_CSR_BASE, FX1_APB_SLOT_SIZE, bus::TargetPath::Peribus0Apb, true}};
    return cfg;
}
} // namespace

int sc_main(int, char**) {
    bus::BusSystem fabric{"fabric", config()};
    Ram source{"source"}, destination{"destination"};
    Driver driver{"driver",source,destination}; Dma dma{"dma"};
    driver.cpu.bind(fabric.initiator("CPU1"));
    dma.master_socket.bind(fabric.initiator("SYS_DMA"));
    fabric.target("SOURCE").bind(source.target);
    fabric.target("DEST").bind(destination.target);
    fabric.target("SYS_DMA_CSR").bind(dma.target_socket);
    dma.reset_n(driver.reset_n); dma.irq(driver.irq); dma.rx_request(driver.rx_request); dma.tx_request(driver.tx_request);
    dma.rx_clear(driver.rx_clear); dma.tx_clear(driver.tx_clear);
    sc_start(1,SC_MS);
    return driver.errors || !sc_end_of_simulation_invoked()?1:0;
}
