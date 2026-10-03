#include "bus/bus_system.h"
#include <memory_tlm.h>
#include <cdc/cpu/elf_loader.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>

// ELF32 PT_LOAD with p_memsz > p_filesz exercises the actual CDC loader.
static void make_elf() {
    std::vector<unsigned char> b(0x108, 0);
    auto put = [&](unsigned off, unsigned value, unsigned n) {
        for (unsigned i=0; i<n; ++i) b.at(off+i) = static_cast<unsigned char>(value >> (8*i));
    };
    b[0]=0x7f; b[1]='E'; b[2]='L'; b[3]='F'; b[4]=1; b[5]=1; b[6]=1;
    put(16,2,2); put(18,243,2); put(20,1,4); put(24,0,4); put(28,52,4);
    put(40,52,2); put(42,32,2); put(44,1,2);
    put(52,1,4); put(56,0x100,4); put(60,0,4); put(64,0,4);
    put(68,8,4); put(72,16,4); put(76,5,4); put(80,4,4);
    b[0x100]=0x13; b[0x104]=0x73;
    std::ofstream f("bus_fixture.elf", std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), b.size());
    if (!f) throw std::runtime_error("Cannot create ELF fixture");
}

class Integration : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<Integration> initiator{"initiator"};
    bus::BusSystem fabric;
    cdc::components::memory_tlm rom, isram, ram, aes, qspi;
    std::vector<std::unique_ptr<cdc::components::memory_tlm>> peripheral;
    unsigned failures=0, checks=0;
    bool finished=false;
    SC_HAS_PROCESS(Integration);
    static bus::BusConfig cfg() {
        bus::BusConfig c;
        // Demonstrate top-owned RAM address and asymmetric peripheral counts.
        c.sysbus1[2] = {"RAM", 0x80000000, 0x80020000, 2, true};
        c.pp1_ports=6; c.pp0_ports=2;
        c.peribus1.clear(); c.peribus0.clear();
        for (unsigned i=0;i<6;++i)
            c.peribus1.push_back({"PP1", 0x40000000+i*0x1000ULL, 0x40001000+i*0x1000ULL, i,true});
        for (unsigned i=0;i<2;++i)
            c.peribus0.push_back({"PP0", 0x52000000+i*0x1000ULL, 0x52001000+i*0x1000ULL, i,true});
        return c;
    }
    explicit Integration(sc_core::sc_module_name name)
        : sc_module(name), fabric("fabric",cfg()), rom("rom",0x10000,true),
          isram("isram",0x20000),ram("ram",0x20000),aes("aes",0x10000),qspi("qspi",0x10000) {
        initiator.bind(fabric.target);
        std::array<std::uint8_t,16> poison;
        poison.fill(0xFF);
        rom.load(poison.data(), poison.size()); // Detect which bytes the loader actually writes.
        fabric.rom.socket.bind(rom.socket); fabric.isram.socket.bind(isram.socket);
        fabric.dsram.socket.bind(ram.socket); fabric.aes.socket.bind(aes.socket); fabric.qspi.socket.bind(qspi.socket);
        for (unsigned i=0;i<8;++i) {
            peripheral.push_back(std::make_unique<cdc::components::memory_tlm>(sc_core::sc_gen_unique_name("peripheral"),0x1000));
            if(i<6) fabric.pp1[i].socket.bind(peripheral.back()->socket);
            else fabric.pp0[i-6].socket.bind(peripheral.back()->socket);
        }
        SC_THREAD(run);
    }
    void check(bool ok, const char* message) {
        ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << message << '\n'; }
    }
    void start_of_simulation() override {
        check(cdc::cpu::load_elf(initiator,"bus_fixture.elf")==0,"ELF entry");
        check(sc_core::sc_time_stamp()==sc_core::SC_ZERO_TIME,"ELF loader never waits");
    }
    void run() {
        std::array<unsigned char,16> data{};
        tlm::tlm_generic_payload tx;
        tx.set_address(0); tx.set_data_ptr(data.data()); tx.set_data_length(16);
        tx.set_streaming_width(16); tx.set_command(tlm::TLM_READ_COMMAND);
        sc_core::sc_time delay(7,sc_core::SC_NS);
        auto start=sc_core::sc_time_stamp();
        initiator->b_transport(tx,delay);
        check(tx.is_response_ok() && data[0]==0x13 && data[4]==0x73,"ELF payload in CDC ROM");
        // Current CDC loader copies p_filesz only; BSS initialization is not bus policy.
        bool untouched=true; for(unsigned i=8;i<16;++i) untouched &= data[i]==0xFF;
        check(untouched,"bytes beyond ELF file data are not silently modified by bus");
        check(sc_core::sc_time_stamp()-start==sc_core::sc_time(19,sc_core::SC_NS),"CDC annotated latency consumed once");
        check(delay==sc_core::SC_ZERO_TIME && !tx.is_dmi_allowed(),"delay reset and CDC DMI suppressed");
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        initiator->b_transport(tx,delay);
        check(tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE,"functional ROM write forbidden");
        tx.set_data_length(4);tx.set_streaming_width(4);
        for(auto address : {0x80000020ULL,0x40005000ULL,0x52001000ULL}) {
            data[0]=0xA5; tx.set_address(address); tx.set_command(tlm::TLM_WRITE_COMMAND);
            initiator->b_transport(tx,delay); check(tx.is_response_ok(),"custom map write");
            data[0]=0; tx.set_command(tlm::TLM_READ_COMMAND);
            initiator->b_transport(tx,delay);
            check(tx.is_response_ok() && data[0]==0xA5,"custom map readback");
            check(tx.get_address()==address,"external target address restored");
        }
        finished=true;
        std::cout << "CDC integration: " << checks << " checks, " << failures << " failures\n";
        sc_core::sc_stop();
    }
};
int sc_main(int,char**) {
    make_elf();
    Integration test("test");
    sc_core::sc_start();
    return test.finished && !test.failures ? 0 : 1;
}
