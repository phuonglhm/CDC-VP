#include "h264_dma_tlm.h"
#include "bus_router.h"
#include "memory_tlm.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <string>
using namespace sc_core;
using namespace cdc::components;
using Device = h264_dma_tlm;
constexpr uint64_t RAM=0x80000000, MMIO=0x10080000;
constexpr uint32_t CMB_BASE=RAM+0x1000, REF_BASE=RAM+0x10000, NAL_BASE=RAM+0x30000;
constexpr size_t FRAME_BYTES=768;

// Faults are downstream TLM responses, not a private DMA DDR backdoor.
struct FaultMemory : sc_module {
    tlm_utils::simple_target_socket<FaultMemory> target{"target"};
    tlm_utils::simple_initiator_socket<FaultMemory> master{"master"};
    bool fail_reads=false, fail_writes=false;
    bool fail_nal=false;
    sc_time extra_delay=SC_ZERO_TIME;
    explicit FaultMemory(sc_module_name n):sc_module(n) {
        target.register_b_transport(this,&FaultMemory::transport);
    }
    void transport(tlm::tlm_generic_payload& t,sc_time& d) {
        if ((fail_reads && t.is_read()) || (fail_writes && t.is_write()) ||
            (fail_nal && t.is_write() && t.get_address()>=NAL_BASE-RAM)) {
            d+=sc_time(50,SC_NS); t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        } else { master->b_transport(t,d); d+=extra_delay; }
    }
};
struct Driver : sc_module {
    tlm_utils::simple_initiator_socket<Driver> socket{"socket"};
    sc_out<bool> reset_n{"reset_n"};
    sc_in<bool> irq{"irq"};
    Device& dma; FaultMemory& faults;
    unsigned passed=0,failed=0; bool finished=false;
    bool timeout_only;
    sc_time irq_raised=SC_ZERO_TIME;
    SC_HAS_PROCESS(Driver);
    Driver(sc_module_name n,Device& dev,FaultMemory& f,bool timeout=false)
        :sc_module(n),dma(dev),faults(f),timeout_only(timeout) {
        SC_THREAD(run); SC_THREAD(watchdog);
        SC_METHOD(irq_changed); sensitive << irq.pos(); dont_initialize();
    }
    void irq_changed(){irq_raised=sc_time_stamp();}
    void check(bool condition,const char* name) {
        if(condition) ++passed;
        else {++failed; std::cerr<<"FAIL: "<<name<<" at "<<sc_time_stamp()<<"\n";}
    }
    tlm::tlm_response_status access(bool write,uint64_t address,uint8_t* data,unsigned n,
                                    uint8_t* be=nullptr,unsigned be_n=0) {
        tlm::tlm_generic_payload t;
        t.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        t.set_address(address);t.set_data_ptr(data);t.set_data_length(n);t.set_streaming_width(n);
        t.set_byte_enable_ptr(be);t.set_byte_enable_length(be_n);
        t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time delay=SC_ZERO_TIME;socket->b_transport(t,delay);wait(delay);
        return t.get_response_status();
    }
    tlm::tlm_response_status wr(unsigned reg,uint32_t value,uint8_t* be=nullptr,unsigned be_n=0) {
        uint8_t bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=uint8_t(value>>(8*i));
        return access(true,MMIO+reg,bytes,4,be,be_n);
    }
    uint32_t rd(unsigned reg,bool debug=false) {
        uint8_t b[4]{};
        if(debug) {
            tlm::tlm_generic_payload t;t.set_read();t.set_address(MMIO+reg);
            t.set_data_ptr(b);t.set_data_length(4);t.set_streaming_width(4);
            if(socket->transport_dbg(t)!=4) throw std::runtime_error("debug read failed");
        } else if(access(false,MMIO+reg,b,4)!=tlm::TLM_OK_RESPONSE)
            throw std::runtime_error("MMIO read failed");
        uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(b[i])<<(8*i);return v;
    }
    void put(uint64_t addr,std::vector<uint8_t> data) {
        if(access(true,addr,data.data(),data.size())!=tlm::TLM_OK_RESPONSE)
            throw std::runtime_error("CPU shared memory write failed");
    }
    std::vector<uint8_t> get(uint64_t addr,size_t n) {
        std::vector<uint8_t> data(n);
        if(access(false,addr,data.data(),data.size())!=tlm::TLM_OK_RESPONSE)
            throw std::runtime_error("CPU shared memory read failed");
        return data;
    }
    void reset() {
        reset_n.write(false);wait(20,SC_NS);reset_n.write(true);wait(20,SC_NS);
    }
    void configure(uint32_t fmsize=(32u<<16)|16u,uint32_t dfcon=0) {
        check(wr(Device::SCON,0)==tlm::TLM_OK_RESPONSE,"disable");
        check(wr(Device::FMSIZE,fmsize)==tlm::TLM_OK_RESPONSE,"FMSIZE write");
        check(wr(Device::DFCON,dfcon)==tlm::TLM_OK_RESPONSE,"DFCON write");
        check(wr(Device::SPARA0,(6u<<21)|(1u<<9)|0x12C)==tlm::TLM_OK_RESPONSE,"SPARA0 write");
        check(wr(Device::SPARA1,(1u<<25)|1)==tlm::TLM_OK_RESPONSE,"SPARA1 write");
        check(wr(Device::SPARA2,0)==tlm::TLM_OK_RESPONSE,"SPARA2 write");
        check(wr(Device::CMB,CMB_BASE)==tlm::TLM_OK_RESPONSE,"CMB write");
        check(wr(Device::REFM,REF_BASE)==tlm::TLM_OK_RESPONSE,"REFM write");
        check(wr(Device::NAL,NAL_BASE)==tlm::TLM_OK_RESPONSE,"NAL write");
    }
    H264ActivationWorkload plan(bool b_picture=false,bool samples=false) {
        H264ActivationWorkload p;H264FrameWorkload f;
        f.reference_slot=1;f.is_b_picture=b_picture;
        for(unsigned x=0;x<2;++x) {
            H264MacroblockJob mb;mb.x=x;
            mb.replay_source_for_bypass=true;
            if(b_picture)mb.references={{h264::RefList::List0,0,10},{h264::RefList::List1,2,20}};
            if(samples) {
                h264::MacroblockPixels pixels;pixels.y.fill(0x71);pixels.u.fill(0x82);pixels.v.fill(0x93);
                mb.reconstructed=pixels;mb.already_filtered=false;
            }
            f.macroblocks.push_back(mb);
        }
        f.nal_words={0x67452301,0xEFCDAB89};p.frames.push_back(f);p.eos=true;return p;
    }
    uint32_t completion(bool expect_irq=true) {
        const auto deadline=sc_time_stamp()+sc_time(2,SC_MS);
        while(rd(Device::STAT,true)&Device::BUSY) {
            if(sc_time_stamp()>deadline)throw std::runtime_error("completion deadline");
            wait(100,SC_NS);
        }
        wait(SC_ZERO_TIME);wait(SC_ZERO_TIME);
        check(irq.read()==expect_irq,"completion GIE gates IRQ");
        return rd(Device::STAT,true);
    }
    void start(H264ActivationWorkload p,uint32_t scon=3) {
        dma.enqueue_workload(std::move(p));
        check(wr(Device::SCON,scon)==tlm::TLM_OK_RESPONSE,"start via MMIO");
    }
    void run() {
        try {
            reset();
            if (timeout_only) {
                configure(); start(plan());
                check(completion()==Device::ERROR && rd(Device::STM_LEN)==0 &&
                    dma.last_error().find("timeout")!=std::string::npos,
                    "stalled external channel terminates bounded activation with ERROR");
                finished=true;
                std::cout<<"H264 timeout checks: "<<passed<<" passed, "<<failed<<" failed\n";
                sc_stop(); return;
            }
            check(rd(Device::SPARA0)==0x12C,"HAS reset SPARA0");
            check(rd(Device::STAT)==0 && rd(Device::STM_LEN)==0 && !irq.read(),"reset status/count/IRQ");
            uint8_t data[4]{};uint8_t bad_be[1]={1};
            check(access(false,MMIO+1,data,4)==tlm::TLM_BURST_ERROR_RESPONSE,"reject unaligned MMIO");
            check(access(false,MMIO,data,2)==tlm::TLM_BURST_ERROR_RESPONSE,"reject short MMIO");
            check(access(false,MMIO+0x2C,data,4)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"undefined register policy");
            check(wr(Device::STAT,1)==tlm::TLM_COMMAND_ERROR_RESPONSE,"STAT read only");
            check(wr(Device::STM_LEN,1)==tlm::TLM_COMMAND_ERROR_RESPONSE,"STM_LEN read only");
            check(wr(Device::SCON,4)==tlm::TLM_GENERIC_ERROR_RESPONSE,"reserved bits rejected");
            check(wr(Device::CMB,1,bad_be,1)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"invalid byte enable");
            configure();
            uint8_t low_byte[4]={0xFF,0,0,0};
            check(wr(Device::CMB,0x12345604,low_byte,4)==tlm::TLM_OK_RESPONSE &&
                  rd(Device::CMB)==CMB_BASE+4,"masked byte write preserves remaining bytes");
            wr(Device::CMB,CMB_BASE);
            std::vector<uint8_t> source(FRAME_BYTES);
            for(size_t i=0;i<source.size();++i)source[i]=uint8_t(i*37+11);
            put(CMB_BASE,source);put(REF_BASE,std::vector<uint8_t>(FRAME_BYTES,0xAA));
            put(REF_BASE+2*FRAME_BYTES,std::vector<uint8_t>(FRAME_BYTES,0xBB));
            put(NAL_BASE,std::vector<uint8_t>(32,0xCC));
            const size_t begin=dma.transfers().size();const auto started=sc_time_stamp();
            start(plan(true));
            check(rd(Device::STAT,true)&Device::BUSY,"BUSY reserves activation immediately");
            check(wr(Device::CMB,CMB_BASE+16)==tlm::TLM_GENERIC_ERROR_RESPONSE,"configuration protected while busy");
            check(wr(Device::SCON,3)==tlm::TLM_OK_RESPONSE,"repeated enable is not a second activation");
            auto status=completion();
            check(status==(Device::NORMAL|1),"NORMAL after complete picture and NAL");
            check(!dma.transfers().empty() &&
                irq_raised>=dma.transfers().back().completed &&
                dma.transfers().back().client==h264::ClientId::NAL,
                "IRQ rising edge follows final NAL memory response");
            check(get(REF_BASE+FRAME_BYTES,FRAME_BYTES)==source,"CPU sees DMA planar Y/U/V in selected shared reference slot");
            check(get(NAL_BASE,12)==std::vector<uint8_t>({1,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0,0,1,0x0B}),"NAL little endian and EOS through shared DDR");
            check(rd(Device::STM_LEN,true)==3 && irq.read(),"debug read leaves IRQ pending");
            bool seen[4]{};bool latency=true;
            for(size_t i=begin;i<dma.transfers().size();++i) {
                const auto& t=dma.transfers()[i];
                const auto c=static_cast<int>(t.client);if(c>=0 && c<4)seen[c]=true;
                latency=latency && t.completed-t.issued>=sc_time(50,SC_NS) && !t.cancelled;
            }
            check(std::all_of(std::begin(seen),std::end(seen),[](bool b){return b;}),"all four DMA clients use master socket");
            check(latency && sc_time_stamp()>started,"returned DDR latency is consumed");
            check(dma.transfers().back().completed<=sc_time_stamp(),"completion follows last memory response");
            check(rd(Device::STM_LEN)==3 && !irq.read(),"STM_LEN read acknowledges IRQ");
            check(rd(Device::STAT)==(Device::NORMAL|1),"IRQ acknowledge retains status");
            wr(Device::SCON,0);check(rd(Device::STM_LEN)==3,"disable retains committed length");
            configure();start(plan(),1);status=completion(false);
            check(status==(Device::NORMAL|1) && rd(Device::STM_LEN,true)==3,"GIE disabled still completes with fresh stream length");
            wr(Device::SCON,3);wait(1,SC_NS);check(irq.read(),"enable GIE exposes retained pending completion");
            rd(Device::STAT);check(!irq.read(),"STAT read acknowledges IRQ");
            // Enabled DF accepts producer samples and runs vertical before horizontal.
            configure((32u<<16)|16u,1);unsigned passes=0;
            dma.set_filter([&](h264::FilterPass p,h264::MacroblockPixels& pixels){
                check(p==((passes%2)?h264::FilterPass::Horizontal:h264::FilterPass::Vertical),"DF pass order");
                ++passes;for(auto& v:pixels.y)++v;
            });
            start(plan(false,true));check(completion()==(Device::NORMAL|1),"producer/filter activation completes");
            auto filtered=get(REF_BASE+FRAME_BYTES,FRAME_BYTES);
            check(passes==4 && std::all_of(filtered.begin(),filtered.begin()+512,[](uint8_t v){return v==0x73;}),"filtered samples committed");
            check(filtered[512]==0x82 && filtered[640]==0x93,"producer U/V samples committed");
            // Invalid configuration fails before issuing memory traffic.
            configure((31u<<16)|16u);const auto invalid_begin=dma.transfers().size();start(plan());
            check(completion()==Device::ERROR && dma.transfers().size()==invalid_begin,"invalid dimensions cannot signal NORMAL or issue DMA");
            configure();wr(Device::SPARA0,(52u<<21)|(1u<<9)|0x12C);start(plan());
            check(completion()==Device::ERROR,"invalid QP rejected");
            configure();wr(Device::SPARA1,1u<<25);start(plan());check(completion()==Device::ERROR,"zero FMENC rejected");
            configure();wr(Device::REFM,CMB_BASE);start(plan());check(completion()==Device::ERROR,"overlapping physical regions rejected");
            configure();auto partial=plan();partial.frames[0].macroblocks.pop_back();start(partial);
            check(completion()==Device::ERROR,"incomplete picture cannot complete");
            configure();auto duplicate=plan();duplicate.frames[0].macroblocks[1].x=0;start(duplicate);
            check(completion()==Device::ERROR,"duplicate macroblock cannot complete");
            configure();check(wr(Device::SCON,3)==tlm::TLM_OK_RESPONSE,"start without producer");
            check(completion()==Device::ERROR && dma.last_error().find("producer")!=std::string::npos,"no fabricated codec completion");
            configure();bool provider_called=false;
            dma.set_workload_provider([&](const H264ActivationConfig& c) {
                provider_called=c.dims.Wc==32 && c.bases.REG_CMB==CMB_BASE;
                return plan();
            });
            wr(Device::SCON,3);
            check(completion()==(Device::NORMAL|1) && provider_called,"provider receives latched MMIO configuration");
            configure();dma.set_workload_provider({});
            // Two working-set source slots and separate selected reference slots.
            wr(Device::SPARA0,(6u<<21)|0x12C);wr(Device::SPARA1,(2u<<25)|2);
            put(CMB_BASE+FRAME_BYTES,std::vector<uint8_t>(FRAME_BYTES,0x5B));
            auto multiple=plan();multiple.frames.push_back(multiple.frames.front());
            multiple.frames[1].source_slot=1;multiple.frames[1].reference_slot=2;
            start(multiple);
            check(completion()==(Device::NORMAL|2) && rd(Device::STM_LEN)==5,"working-set activation completes both pictures and appended NAL");
            check(get(REF_BASE+2*FRAME_BYTES,FRAME_BYTES)==std::vector<uint8_t>(FRAME_BYTES,0x5B),"second working-set source maps to selected reference slot");
            configure();faults.fail_reads=true;start(plan());
            check(completion()==Device::ERROR && rd(Device::STM_LEN)==0,"failed read propagates ERROR");faults.fail_reads=false;
            configure();faults.fail_writes=true;start(plan());
            check(completion()==Device::ERROR && rd(Device::STM_LEN)==0,"failed DF write cannot commit NAL length");faults.fail_writes=false;
            configure();faults.fail_nal=true;start(plan());
            check(completion()==Device::ERROR && rd(Device::STM_LEN)==0,"failed NAL response cannot report NORMAL or commit length");faults.fail_nal=false;
            configure((32u<<16)|16u,1);start(plan());
            check(completion()==Device::ERROR,"enabled filter requires reconstructed producer samples");
            configure();auto absent=plan();
            for(auto& mb:absent.frames[0].macroblocks)mb.replay_source_for_bypass=false;
            start(absent);check(completion()==Device::ERROR,"bypass also requires producer data unless replay explicitly selected");
            configure();auto missing_list=plan();missing_list.frames[0].is_b_picture=true;
            start(missing_list);check(completion()==Device::ERROR,"B-picture cannot reuse absent/stale reference-list residency");
            configure();start(plan());wr(Device::SCON,2);
            check(completion()==Device::ERROR && dma.last_error().find("stop")!=std::string::npos,"controlled stop terminates with ERROR");
            // Reset while a timed read is in flight; immediately queue a new activation.
            configure();faults.extra_delay=sc_time(500,SC_NS);
            const size_t reset_begin=dma.transfers().size();start(plan());
            reset_n.write(false);wait(1,SC_NS);
            check(!irq.read() && rd(Device::STAT,true)==0 && rd(Device::SPARA0,true)==0x12C,"in-flight reset clears architectural state");
            reset_n.write(true);wait(1,SC_NS);configure();start(plan());
            faults.extra_delay=SC_ZERO_TIME;
            check(completion()==(Device::NORMAL|1),"new activation survives cancelled worker wakeup");
            bool cancelled=false;
            for(size_t i=reset_begin;i<dma.transfers().size();++i)cancelled|=dma.transfers()[i].cancelled;
            check(cancelled,"reset marks accepted in-flight transfer cancelled");
            rd(Device::STAT);reset();check(!irq.read() && rd(Device::STM_LEN)==0,"final reset clears stale IRQ and length");
        } catch(const std::exception& e) {++failed;std::cerr<<"Driver exception: "<<e.what()<<"\n";}
        finished=true;std::cout<<"H264 shared-bus checks: "<<passed<<" passed, "<<failed<<" failed\n";sc_stop();
    }
    void watchdog(){wait(10,SC_MS);if(!finished){++failed;std::cerr<<"Watchdog timeout\n";sc_stop();}}
};
int sc_main(int argc,char** argv) {
    H264DmaOptions options;options.memory_bytes=1<<20;options.nal_capacity=4096;
    bool timeout=false;
    for(int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if(argument=="--timeout")timeout=true;
        else options.bridge.data_width_bits=static_cast<uint32_t>(std::stoul(argument));
    }
    if(timeout){options.bridge.rvalid_delay=10000;options.max_service_steps=64;}
    Device dma("h264",options);bus_router bus("bus",2,2);
    memory_tlm ram("ram",1<<20,false,sc_time(50,SC_NS));FaultMemory faults("faults");
    Driver driver("driver",dma,faults,timeout);sc_signal<bool> reset_n("reset_n"),irq("irq");
    driver.reset_n(reset_n);driver.irq(irq);dma.reset_n(reset_n);dma.irq(irq);
    driver.socket.bind(bus.cpu_port(0));dma.master_socket.bind(bus.cpu_port(1));
    bus.add_target(MMIO,0x1000).bind(dma.target_socket);
    bus.add_target(RAM,1<<20).bind(faults.target);faults.master.bind(ram.socket);
    sc_start();return driver.failed?1:0;
}
