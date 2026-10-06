#include "bus/bus_system.h"
#include "../support/test_helpers.h"
#include <memory_tlm.h>
#include <cdc/cpu/elf_loader.h>
#include <array>
#include <fstream>
#include <memory>
#include <vector>

// ELF32 PT_LOAD fixture: real CDC ELF loader, no CPU execution required.
static void make_elf() {
    std::vector<unsigned char> bytes(0x108,0);
    auto put=[&](unsigned offset,unsigned value,unsigned n){
        for(unsigned i=0;i<n;++i)bytes.at(offset+i)=static_cast<unsigned char>(value>>(8*i));
    };
    bytes[0]=0x7F;bytes[1]='E';bytes[2]='L';bytes[3]='F';bytes[4]=1;bytes[5]=1;bytes[6]=1;
    put(16,2,2);put(18,243,2);put(20,1,4);put(28,52,4);
    put(40,52,2);put(42,32,2);put(44,1,2);
    put(52,1,4);put(56,0x100,4);put(68,8,4);put(72,16,4);put(76,5,4);put(80,4,4);
    bytes[0x100]=0x13;bytes[0x104]=0x73;
    std::ofstream out("bus_fixture.elf",std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!out)throw std::runtime_error("Cannot create ELF fixture");
}
struct Integration : sc_core::sc_module, Checks {
    bus::BusConfig cfg;
    bus::BusSystem fabric;
    Master cpu{"cpu"};
    std::vector<std::unique_ptr<cdc::components::memory_tlm>> ips;
    std::vector<bus::TargetConfig> active;
    bool finished=false;
    SC_HAS_PROCESS(Integration);
    static bus::BusConfig config() {
        auto c=bus::BusConfig::fx1();
        c.targets.push_back({"AES",0x50000000,0x1000,bus::TargetPath::SysBus0Axi,true});
        for(auto& t:c.targets)if(t.name=="ISP_CSR"){t.base=0x40000000;t.size=0x1000;t.enabled=true;}
        return c;
    }
    explicit Integration(sc_core::sc_module_name n):sc_module(n),cfg(config()),fabric("fabric",cfg) {
        cpu.socket.bind(fabric.initiator("CPU1"));
        // Small IP storage is intentional: verify downstream rejects addresses
        // within the bus aperture but beyond the model's implemented memory.
        for(const auto& t:cfg.targets)if(t.enabled){
            active.push_back(t);
            auto ip=std::make_unique<cdc::components::memory_tlm>(sc_core::sc_gen_unique_name("ip"),4096,t.name=="BootROM");
            fabric.target(t.name).bind(ip->socket);ips.push_back(std::move(ip));
        }
        std::array<std::uint8_t,16> poison;poison.fill(0xFF);ips[0]->load(poison.data(),poison.size());
        SC_THREAD(run);SC_THREAD(watchdog);
    }
    void start_of_simulation()override {
        expect(cdc::cpu::load_elf(cpu.socket,"bus_fixture.elf")==0,"ELF entry");
        expect(sc_core::sc_time_stamp()==sc_core::SC_ZERO_TIME,"ELF debug does not wait");
    }
    void run() {
        std::array<unsigned char,16> data{};tlm::tlm_generic_payload tx;
        payload(tx,tlm::TLM_READ_COMMAND,0,data.data(),16);
        sc_core::sc_time delay(7,sc_core::SC_NS);auto start=sc_core::sc_time_stamp();
        cpu.socket->b_transport(tx,delay);
        expect(tx.is_response_ok()&&data[0]==0x13&&data[4]==0x73,"ELF data via functional read");
        // The current CDC loader copies p_filesz; bus must not silently clear BSS.
        bool untouched=true;for(unsigned i=8;i<16;++i)untouched&=data[i]==0xFF;
        expect(untouched,"bus leaves BSS initialization to loader/startup");
        expect(sc_core::sc_time_stamp()-start==sc_core::sc_time(19,sc_core::SC_NS),"annotated delay consumed once");
        expect(delay==sc_core::SC_ZERO_TIME&&!tx.is_dmi_allowed(),"CDC DMI suppressed and delay zero");
        tx.set_command(tlm::TLM_WRITE_COMMAND);cpu.socket->b_transport(tx,delay);
        expect(tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE,"real ROM refuses functional writes");
        for(const auto& target:active)if(target.name!="BootROM"){
            payload(tx,tlm::TLM_WRITE_COMMAND,target.base+32,data.data(),4);data[0]=0xA5;
            cpu.socket->b_transport(tx,delay);expect(tx.is_response_ok(),target.name+" write");
            data[0]=0;tx.set_command(tlm::TLM_READ_COMMAND);cpu.socket->b_transport(tx,delay);
            expect(tx.is_response_ok()&&data[0]==0xA5,target.name+" readback");
            expect(tx.get_address()==target.base+32&&!tx.is_dmi_allowed(),target.name+" address restored/DMI suppressed");
        }
        payload(tx,tlm::TLM_READ_COMMAND,0x80002000,data.data(),4);cpu.socket->b_transport(tx,delay);
        expect(tx.get_response_status()==tlm::TLM_ADDRESS_ERROR_RESPONSE,"downstream short memory error propagates");
        payload(tx,tlm::TLM_WRITE_COMMAND,0x40000003,data.data(),16);start=sc_core::sc_time_stamp();
        expect(cpu.socket->transport_dbg(tx)==16,"debug through APB1 bypasses functional beat restriction");
        expect(sc_core::sc_time_stamp()==start&&tx.get_address()==0x40000003,"debug time/address preserved");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"CDC integration watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**){make_elf();Integration t{"test"};sc_core::sc_start();std::cout<<"CDC integration: "<<t.checks<<" checks, "<<t.failures<<" failures\n";return t.finished&&!t.failures?0:1;}
