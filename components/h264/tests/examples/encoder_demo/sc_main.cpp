#include <h264/top/encoder_vp.h>
#include <h264/platform/ddr_memory.h>
#include <h264/platform/host_driver.h>
#include <iostream>
using namespace sc_core;
using namespace h264;
class Demo : public sc_module {
public:
    sc_signal<bool> rstn{"rstn"}, irq{"irq"};
    EncoderVp encoder{"encoder"};
    DdrMemory ddr{"ddr",1<<20};
    HostDriver host{"host"};
    bool passed=false;
    SC_HAS_PROCESS(Demo);
    explicit Demo(sc_module_name name):sc_module(name) {
        encoder.rstn(rstn); encoder.irq(irq);
        host.registers.bind(encoder.registers.socket); host.memory.bind(ddr.socket);
        encoder.bridge.memory.bind(ddr.socket);
        SC_THREAD(run); SC_THREAD(watchdog);
    }
    void watchdog() { wait(1,SC_MS); SC_REPORT_FATAL("demo","simulation timeout"); }
    void run() {
        rstn=false; wait(8,SC_NS); rstn=true; wait(24,SC_NS);
        const unsigned width=176,height=144;
        std::vector<unsigned char> source(width*height*3/2);
        for(unsigned i=0;i<source.size();++i) source[i]=static_cast<unsigned char>(i*13+7);
        host.transfer(true,0x1000,source);
        for(unsigned activation=0;activation<2;++activation) {
            host.configure(width,height,0x1000,0x20000,0x60000);
            if(host.write(reg::SCON,reg::ENABLE|reg::GIE)!=tlm::TLM_OK_RESPONSE)
                throw std::runtime_error("start failed");
            while(!irq.read()) wait(irq.posedge_event());
            host.write(reg::SCON,0);
            const auto status=host.read(reg::STAT),words=host.read(reg::STM_LEN);
            if(!(status&reg::NORMAL) || words!=100) throw std::runtime_error("completion mismatch");
            std::vector<unsigned char> output(words*4);
            host.transfer(false,0x60000,output);
            std::cout<<"Activation "<<activation<<": "<<words<<" words, completed at "<<sc_time_stamp()<<'\n';
        }
        std::cout<<"PASS: control/DMA integration demo (stub output, NOT H.264 video)\n";
        passed=true; sc_stop();
    }
};
int sc_main(int,char**) {
    Demo demo("demo"); sc_start(); return demo.passed?0:1;
}
