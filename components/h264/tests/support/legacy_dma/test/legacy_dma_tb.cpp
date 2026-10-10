#include <h264/legacy_dma/dma_arbiter.h>
#include <h264/legacy_dma/dma_bridge.h>
#include <h264/platform/ddr_memory.h>
#include <iostream>
using namespace sc_core;
struct Bench:sc_module {
    h264::legacy::DmaArbiter arb{"arb"};
    h264::legacy::DmaBridge bridge{"bridge"};
    h264::DdrMemory mem{"mem",65536};
    tlm_utils::simple_initiator_socket<Bench> low{"low"},high{"high"};
    bool passed=false,high_done=false; sc_event go;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n):sc_module(n) {
        low.bind(arb.clients); high.bind(arb.clients);
        arb.memory.bind(bridge.input); bridge.memory.bind(mem.socket);
        arb.set_priority(0,5); arb.set_priority(1,0);
        SC_THREAD(run); SC_THREAD(worker); SC_THREAD(watchdog);
    }
    void send(tlm_utils::simple_initiator_socket<Bench>& port) {
        unsigned char data[4]={1,2,3,4}; tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND); tx.set_address(0x100);
        tx.set_data_ptr(data); tx.set_data_length(4); tx.set_streaming_width(4);
        sc_time delay=SC_ZERO_TIME; port->b_transport(tx,delay);
        if(tx.is_response_error()) throw std::runtime_error("legacy write");
    }
    void worker() { wait(go); for(unsigned i=0;i<12;++i) send(high); high_done=true; }
    void run() {
        try {
            go.notify(SC_ZERO_TIME); send(low);
            while(!high_done) wait(1,SC_NS);
            if(arb.trace.size()!=13 || arb.trace.front().owner!=1) throw std::runtime_error("legacy priority");
            unsigned pos=0; while(pos<arb.trace.size() && arb.trace[pos].owner!=0) ++pos;
            if(pos>8) throw std::runtime_error("legacy aging");
            passed=true;
        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; }
        sc_stop();
    }
    void watchdog() { wait(100,SC_US); sc_stop(); }
};
int sc_main(int,char**) { Bench b("bench"); sc_start(); return b.passed?0:1; }
