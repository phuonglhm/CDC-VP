#include <h264/top/encoder_vp.h>
#include <h264/platform/host_driver.h>
#include <bus/bus_system.h>
#include <fx1/fx1_memory_map.h>
#include <fx1/sparse_ram.h>
#include <fx1/exclusive_monitor.h>
#include <fx1/plic.h>
#include <iostream>

using namespace sc_core;
using namespace h264;
namespace {
void require(bool ok,const char* what) { if(!ok) throw std::runtime_error(what); }
bus::BusConfig configuration() {
    auto cfg=bus::BusConfig::fx1();
    for(auto& t:cfg.targets) {
        t.enabled=t.name=="MEMCTL_DDR" || t.name=="PLIC" || t.name=="H264_H265_CSR";
        if(t.name=="H264_H265_CSR") {
            t.base=FX1_H264_H265_CSR_BASE; t.size=FX1_APB_SLOT_SIZE;
        }
    }
    return cfg;
}
class Bench : public sc_module {
public:
    bus::BusSystem fabric{"fabric",configuration()};
    fx1::SparseRam ddr{"ddr",FX1_DDR_SIZE,sc_time(30,SC_NS)};
    fx1::ExclusiveMonitor monitor{FX1_NUM_HARTS,FX1_RESERVATION_GRANULE};
    fx1::WriteGuard guard{"h264_dma_guard",monitor,0x102};
    fx1::Plic plic{"plic",FX1_PLIC_NUM_SOURCES,FX1_NUM_HARTS};
    sc_vector<sc_signal<bool>> lines{"lines",FX1_PLIC_NUM_SOURCES-1};
    sc_vector<sc_signal<bool>> eip{"eip",FX1_NUM_HARTS};
    sc_signal<bool> rstn{"rstn"};
    EncoderVp encoder{"encoder"};
    HostDriver host{"host",FX1_H264_H265_CSR_BASE};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    bool passed=false;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name name):sc_module(name) {
        fabric.target("H264_H265_CSR").bind(encoder.registers.socket);
        fabric.target("MEMCTL_DDR").bind(ddr.socket);
        fabric.target("PLIC").bind(plic.socket);
        host.registers.bind(fabric.initiator("CPU1"));
        host.memory.bind(fabric.initiator("CPU2"));
        cpu.bind(fabric.initiator("SYS_DMA")); // test MMIO initiator, no CPU emulation
        encoder.bridge.memory.bind(guard.target);
        guard.out.bind(fabric.initiator("H264_H265_DMA"));
        encoder.rstn(rstn); encoder.irq(lines[FX1_IRQ_H264_H265-1]);
        for(unsigned i=0;i<lines.size();++i) plic.irq_in[i](lines[i]);
        for(unsigned i=0;i<eip.size();++i) plic.eip[i](eip[i]);
        SC_THREAD(run); SC_THREAD(watchdog);
    }
    uint32_t plic_access(unsigned offset,bool write=false,uint32_t value=0) {
        unsigned char data[4]; store_le(data,value);
        tlm::tlm_generic_payload tx;
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(uint64_t(FX1_PLIC_BASE)+offset);
        tx.set_data_ptr(data); tx.set_data_length(4); tx.set_streaming_width(4);
        sc_time delay=SC_ZERO_TIME; cpu->b_transport(tx,delay); consume_delay(delay);
        require(tx.is_response_ok(),"PLIC MMIO failed"); return load_le(data);
    }
    void watchdog() { wait(1,SC_MS); SC_REPORT_FATAL("cdc_fx1","timeout"); }
    void run() {
        try {
            rstn=false; wait(20,SC_NS); rstn=true; wait(20,SC_NS);
            require(host.read(reg::SPARA0)==0x12c,"PDF reset value through APB");
            require(host.write(reg::FMSIZE,0x00100010)==tlm::TLM_OK_RESPONSE,"CSR write");
            require(host.write(reg::FMSIZE,0x00200000,4)==tlm::TLM_OK_RESPONSE,"WSTRB write");
            require(host.read(reg::FMSIZE)==0x00200010,"WSTRB lane translation");
            plic_access(FX1_PLIC_PRIORITY_OFF+4*FX1_IRQ_H264_H265,true,1);
            plic_access(FX1_PLIC_ENABLE_OFF,true,1u<<FX1_IRQ_H264_H265);
            const uint32_t cmb=FX1_DDR_BASE+0x1000, refm=FX1_DDR_BASE+0x10000;
            const uint32_t nal=FX1_DDR_BASE+0x20000;
            std::vector<unsigned char> source(384,0x5a);
            host.transfer(true,cmb,source);
            // A real external RAM does not implement our EpochExtension. Hold a
            // NAL write at the CPU atomic bracket, reset, then explicitly drain.
            host.configure(16,16,cmb,refm,nal);
            monitor.atomic_begin(0,nal,4);
            require(host.write(reg::SCON,reg::ENABLE|reg::GIE)==tlm::TLM_OK_RESPONSE,"reset scenario start");
            while(!guard.writes_held()) wait(10,SC_NS);
            rstn=false; wait(20,SC_NS);
            require(!encoder.arbiter.wait_idle(sc_time(50,SC_NS)),"drain must time out on live write");
            monitor.atomic_end(0);
            require(encoder.arbiter.wait_idle(sc_time(10,SC_US)),"reset DMA drain");
            wait(1,SC_NS);
            require(!lines[FX1_IRQ_H264_H265-1].read(),"stale IRQ after reset");
            rstn=true; wait(20,SC_NS);
            require(host.read(reg::STAT)==0 && host.read(reg::STM_LEN)==0,"stale completion after reset");
            for(unsigned activation=0;activation<2;++activation) {
                host.configure(16,16,cmb,refm,nal);
                monitor.load_reserved(0,nal,4);
                require(host.write(reg::SCON,reg::ENABLE|reg::GIE)==tlm::TLM_OK_RESPONSE,"start");
                while(!eip[0].read()) wait(eip[0].posedge_event());
                require(monitor.writes_in_flight()==0,"IRQ before DMA response");
                require(!monitor.reserved(0),"DMA must invalidate CPU LR reservation");
                require(plic_access(FX1_PLIC_CONTEXT_OFF+4)==FX1_IRQ_H264_H265,"wrong PLIC claim");
                const auto result=host.wait_for_completion(lines[FX1_IRQ_H264_H265-1],sc_time(1,SC_US));
                require(!result.timed_out && result.quiescent,"completion/drain");
                require((result.status&reg::NORMAL) && !(result.status&(reg::ERROR|reg::BUSY)),"normal status");
                require(result.words==2 && result.output.size()==8,"activation LEN");
                uint32_t hash=2166136261u;
                for(auto b:source) hash=(hash^b)*16777619u;
                require(load_le(result.output.data())==hash,"DDR fixture checksum");
                require(load_le(result.output.data()+4)==0x0b010000,"DDR fixture EOS");
                std::vector<unsigned char> reconstructed(384);
                host.transfer(false,refm+384,reconstructed);
                require(reconstructed==source,"reference DMA data");
                plic_access(FX1_PLIC_CONTEXT_OFF+4,true,FX1_IRQ_H264_H265);
                wait(1,SC_NS);
                require(!eip[0].read() && !lines[FX1_IRQ_H264_H265-1].read(),"IRQ acknowledge");
            }
            rstn=false; wait(20,SC_NS); rstn=true; wait(20,SC_NS);
            require(host.read(reg::STAT)==0 && host.read(reg::STM_LEN)==0,"reset state");
            require(host.read(reg::SPARA0)==0x12c,"reset defaults");
            passed=true;
            std::cout<<"PASS: external FX1 bus/RAM/PLIC/monitor with H264 stub (no CPU or real codec)\n";
        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; }
        sc_stop();
    }
};
}
int sc_main(int,char**) { Bench bench("bench"); sc_start(); return bench.passed?0:1; }
