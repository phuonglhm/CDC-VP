#include <h264/top/dma_transport.h>
#include "memory_map.h"
#include <tlm_utils/simple_target_socket.h>
#include <iostream>
#include <array>
using namespace sc_core;
struct DelayedMemory:sc_module {
    tlm_utils::simple_target_socket<DelayedMemory> input{"input"};
    tlm_utils::simple_initiator_socket<DelayedMemory> output{"output"};
    unsigned extra;
    DelayedMemory(sc_module_name n,unsigned delay):sc_module(n),extra(delay){input.register_b_transport(this,&DelayedMemory::transport);}
    void transport(tlm::tlm_generic_payload& tx,sc_time& d) {
        output->b_transport(tx,d);d+=sc_time(extra,SC_NS);h264::consume_delay(d);
    }
};
class Bench:public sc_module {
public:
    h264::DmaTransport dma;
    h264::mem::MemoryMap memory{"memory"};
    DelayedMemory bus;
    tlm_utils::simple_initiator_socket<Bench> host{"host"},cmb{"cmb"},sw{"sw"},nal{"nal"},df{"df"};
    sc_event go,completed;
    unsigned active=0,peak=0,finished=0;
    bool good=true,passed=false;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n,unsigned width,unsigned latency):sc_module(n),dma("dma",width),bus("bus",latency) {
        host.bind(memory.socket);dma.memory.bind(bus.input);bus.output.bind(memory.socket);
        cmb.bind(dma.clients);sw.bind(dma.clients);nal.bind(dma.clients);df.bind(dma.clients);
        SC_THREAD(run);SC_THREAD(cmb_run);SC_THREAD(sw_run);SC_THREAD(nal_run);SC_THREAD(df_run);SC_THREAD(watchdog);
    }
    static uint64_t base(unsigned id){const uint64_t b[4]={0x1ff0,0x4ff0,0x8ff0,0xbff0};return b[id];}
    static unsigned length(unsigned i){const unsigned n[3]={288,31,4};return n[i%3];}
    static std::vector<unsigned char> bytes(unsigned id,unsigned seq) {
        std::vector<unsigned char> v(length(seq));
        for(unsigned i=0;i<v.size();++i)v[i]=(id*61+seq*17+i*13)&255;
        return v;
    }
    void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
    void host_tx(bool write,uint64_t a,std::vector<unsigned char>& v) {
        tlm::tlm_generic_payload tx;tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(a);tx.set_data_ptr(v.data());tx.set_data_length(v.size());tx.set_streaming_width(v.size());
        sc_time d=SC_ZERO_TIME;host->b_transport(tx,d);h264::consume_delay(d);check(tx.is_response_ok(),"host transfer");
    }
    void worker(unsigned id,tlm_utils::simple_initiator_socket<Bench>& port) {
        wait(go);
        try {
            for(unsigned i=0;i<16;++i) {
                auto expected=bytes(id,i),v=id<2?std::vector<unsigned char>(length(i),0):expected;
                h264::DmaClientExtension ext;ext.client=static_cast<h264::ClientId>(id);
                tlm::tlm_generic_payload tx;tx.set_extension(&ext);tx.set_command(id<2?tlm::TLM_READ_COMMAND:tlm::TLM_WRITE_COMMAND);
                tx.set_address(base(id)+320*i);tx.set_data_ptr(v.data());tx.set_data_length(v.size());tx.set_streaming_width(v.size());
                ++active;peak=std::max(peak,active);
                sc_time d=SC_ZERO_TIME;port->b_transport(tx,d);h264::consume_delay(d);--active;tx.clear_extension<h264::DmaClientExtension>();
                check(tx.is_response_ok(),"contended DMA response");check(v==expected,"DMA response routed to wrong client");
            }
        }catch(const std::exception& e){good=false;std::cerr<<e.what()<<"\n";}
        ++finished;completed.notify(SC_ZERO_TIME);
    }
    void cmb_run(){worker(0,cmb);}void sw_run(){worker(1,sw);}void nal_run(){worker(2,nal);}void df_run(){worker(3,df);}
    void watchdog(){wait(5,SC_MS);std::cerr<<"contention watchdog\n";sc_stop();}
    void run(){
        try {
            for(unsigned id=0;id<2;++id)for(unsigned i=0;i<16;++i){auto v=bytes(id,i);host_tx(true,base(id)+320*i,v);}
            go.notify(SC_ZERO_TIME);while(finished<4)wait(completed);
            check(good && peak==4 && !active && dma.idle(),"four-client completion/quiescence");
            check(dma.trace.size()==64,"lost/duplicate grant");
            std::array<unsigned,4> next{};
            for(unsigned i=0;i<dma.trace.size();++i) {
                auto t=dma.trace[i];check(t.owner>=0 && t.owner<4,"grant owner");
                auto seq=next[t.owner]++;
                check(t.address==base(t.owner)+320*seq && t.length==length(seq),"per-client order/address/length");
                if(i)check(t.begin>=dma.trace[i-1].end,"active request preempted");
            }
            for(auto n:next)check(n==16,"client starved");
            for(const auto& segment:dma.segments)
                check(segment.address/4096==(segment.address+segment.bytes-1)/4096 && segment.beats<=dma.max_beats,"burst boundary");
            for(unsigned id=2;id<4;++id)for(unsigned i=0;i<16;++i) {
                auto expected=bytes(id,i),v=std::vector<unsigned char>(length(i));
                host_tx(false,base(id)+320*i,v);check(v==expected,"contended write corruption");
            }
            passed=true;std::cout<<"PASS 4 concurrent clients / 64 requests / nonpreemptive grants\n";
        }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";}
        sc_stop();
    }
};
int sc_main(int argc,char** argv){
    try{Bench bench("bench",argc>1?std::stoul(argv[1]):32,argc>2?std::stoul(argv[2]):100);sc_start();return bench.passed?0:1;}
    catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
