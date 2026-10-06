#include <bus/bus_system.h>
#include <iostream>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

struct InstalledTarget : sc_core::sc_module {
    tlm_utils::simple_target_socket<InstalledTarget> socket{"socket"};
    explicit InstalledTarget(sc_core::sc_module_name n):sc_module(n){socket.register_b_transport(this,&InstalledTarget::transport);}
    void transport(tlm::tlm_generic_payload& tx,sc_core::sc_time&){
        if(tx.get_address()!=4){tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);return;}
        tx.get_data_ptr()[0]=0xA5;tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};
struct Consumer : sc_core::sc_module {
    bus::BusSystem fabric;
    InstalledTarget ip{"ip"};
    tlm_utils::simple_initiator_socket<Consumer> cpu{"cpu"};
    bool passed=false;
    SC_HAS_PROCESS(Consumer);
    static bus::BusConfig config(){bus::BusConfig c;c.initiators={{"CPU"}};c.targets={{"IP",0x30000000,0x1000,bus::TargetPath::SysBus0Axi,true}};return c;}
    explicit Consumer(sc_core::sc_module_name n):sc_module(n),fabric("fabric",config()){
        cpu.bind(fabric.initiator("CPU"));fabric.target("IP").bind(ip.socket);SC_THREAD(run);
    }
    void run(){unsigned char data[4]={};tlm::tlm_generic_payload tx;tx.set_command(tlm::TLM_READ_COMMAND);tx.set_address(0x30000004);tx.set_data_ptr(data);tx.set_data_length(4);tx.set_streaming_width(4);sc_core::sc_time d=sc_core::SC_ZERO_TIME;cpu->b_transport(tx,d);passed=tx.is_response_ok()&&data[0]==0xA5&&tx.get_address()==0x30000004;sc_core::sc_stop();}
};
int sc_main(int,char**){Consumer c{"consumer"};sc_core::sc_start();std::cout<<"Installed bus: "<<(c.passed?"PASS":"FAIL")<<'\n';return c.passed?0:1;}
