#include <h264/top/encoder_vp.h>
#include <h264/platform/host_driver.h>
#include <h264/platform/ddr_memory.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <iostream>
using namespace h264;
using namespace sc_core;
namespace fs=std::filesystem;
#ifdef H264_CONTRACT_FIXTURE
namespace h264 { std::unique_ptr<FrameExecutorIf> make_contract_pipeline(sc_module_name,ResetDomain&,DmaArbiter&); }
#endif
struct TestCase {
    std::map<std::string,std::string> values;
    fs::path directory;
    explicit TestCase(const fs::path& path) : directory(fs::absolute(path).parent_path()) {
        std::ifstream file(path);
        if(!file) throw std::runtime_error("cannot open case: "+path.string());
        std::string line;
        while(std::getline(file,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.empty() || line[0]=='#') continue;
            auto pos=line.find('=');
            if(pos==std::string::npos || pos==0 || pos+1==line.size()) throw std::runtime_error("invalid case line: "+line);
            auto key=line.substr(0,pos);
            const std::set<std::string> keys={"width","height","frames","qp","bus_width","latency_ns","timeout_ns",
                "nal_capacity","repeat","source","expected","expected_ref","reference_offset","spara0","spara1","spara2","dfcon"};
            if(!keys.count(key) && key.rfind("source.",0)!=0 && key.rfind("expected.",0)!=0)
                throw std::runtime_error("unknown case key: "+key);
            if(!values.emplace(key,line.substr(pos+1)).second) throw std::runtime_error("duplicate case key: "+key);
        }
    }
    unsigned number(const std::string& key,unsigned fallback) const {
        auto it=values.find(key); if(it==values.end()) return fallback;
        size_t consumed=0;
        if(it->second[0]=='-') throw std::runtime_error("negative number: "+key);
        auto n=std::stoull(it->second,&consumed,0);
        if(consumed!=it->second.size() || n>0xffffffffULL) throw std::runtime_error("invalid number: "+key);
        return static_cast<unsigned>(n);
    }
    std::vector<unsigned char> data(const std::string& key,unsigned activation=0) const {
        auto it=values.find(key+"."+std::to_string(activation));
        if(it==values.end()) it=values.find(key);
        if(it==values.end()) throw std::runtime_error("missing vector: "+key);
        auto path=directory/it->second;
        std::ifstream file(path,std::ios::binary);
        if(!file) throw std::runtime_error("cannot open vector: "+path.string());
        if(path.extension()!=".hex") return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        std::vector<unsigned char> result; std::string token;
        while(file>>token) {
            size_t used=0; auto byte=std::stoul(token,&used,16);
            if(used!=token.size() || byte>255) throw std::runtime_error("invalid hex byte");
            result.push_back(static_cast<unsigned char>(byte));
        }
        return result;
    }
};
class FullPipelineBench : public sc_module {
public:
    static constexpr uint32_t CMB=0x1000,REFM=0x01000000,NAL=0x03000000;
    sc_signal<bool> rstn{"rstn"},irq{"irq"};
    EncoderVp encoder;
    HostDriver host{"host"};
    DdrMemory memory{"memory",64*1024*1024};
    bool passed=false;
    SC_HAS_PROCESS(FullPipelineBench);
    FullPipelineBench(sc_module_name name,const TestCase& c,PipelineFactory factory,fs::path output)
        : sc_module(name),encoder("encoder",c.number("bus_width",32),
          SequenceParameters{c.number("frames",1),c.number("qp",26),c.number("nal_capacity",65536)},
          sc_time(8,SC_NS),factory),case_(c),output_(std::move(output)) {
        encoder.rstn(rstn); encoder.irq(irq);
        host.registers.bind(encoder.registers.socket); host.memory.bind(memory.socket);
        encoder.bridge.memory.bind(memory.socket);
        memory.latency=sc_time(c.number("latency_ns",40),SC_NS);
        host.programming=SequenceParameters{c.number("frames",1),c.number("qp",26),c.number("nal_capacity",65536)};
        SC_THREAD(run);
    }
private:
    const TestCase& case_;
    fs::path output_;
    void require(bool good,const std::string& message) { if(!good) throw std::runtime_error(message); }
    void write(uint32_t address,uint32_t value) {
        require(host.write(address,value)==tlm::TLM_OK_RESPONSE,"register write failed");
    }
    void compare(const std::vector<unsigned char>& actual,const std::vector<unsigned char>& expected,const char* kind) {
        require(actual.size()==expected.size(),std::string(kind)+" length mismatch");
        for(size_t i=0;i<actual.size();++i) require(actual[i]==expected[i],std::string(kind)+" first mismatch at byte "+std::to_string(i));
    }
    void run() {
        try {
            const unsigned w=case_.number("width",16),h=case_.number("height",16),frames=case_.number("frames",1);
            const unsigned capacity=case_.number("nal_capacity",65536),repeat=case_.number("repeat",1);
            const unsigned timeout=case_.number("timeout_ns",10000000);
            require(w && h && w<=1920 && h<=1088 && w%16==0 && h%16==0,"invalid dimensions");
            require(frames && frames<=65535 && repeat && timeout && capacity && capacity%4==0,"invalid case bounds");
            const uint64_t frame_bytes=uint64_t(w)*h*3/2;
            require(frame_bytes*frames<=REFM-CMB && frame_bytes*3<=NAL-REFM-64 && capacity<=8*1024*1024,"fixture memory capacity");
            rstn=false; wait(8,SC_NS); rstn=true; wait(24,SC_NS);
            if(!output_.empty()) fs::create_directories(output_);
            for(unsigned activation=0;activation<repeat;++activation) {
                auto source=case_.data("source",activation),expected=case_.data("expected",activation);
                require(source.size()==frame_bytes*frames,"source must contain all coded planar frames");
                require(!expected.empty() && expected.size()%4==0 && expected.size()<=capacity,"golden must contain exact STM_LEN*4 bytes");
                host.transfer(true,CMB,source);
                std::vector<unsigned char> protected_output(capacity+128,0xa7);
                host.transfer(true,NAL-64,protected_output);
                host.configure(w,h,CMB,REFM,NAL);
                for(auto pair:{std::pair<const char*,uint32_t>{"spara0",reg::SPARA0},{"spara1",reg::SPARA1},
                    {"spara2",reg::SPARA2},{"dfcon",reg::DFCON}}) write(pair.second,case_.number(pair.first,host.read(pair.second)));
                const auto trace_begin=memory.trace.size();
                write(reg::SCON,reg::ENABLE|reg::GIE);
                const auto deadline=sc_time_stamp()+sc_time(timeout,SC_NS);
                while(!irq.read() && sc_time_stamp()<deadline) wait(deadline-sc_time_stamp(),irq.posedge_event());
                if(!irq.read()) {
                    host.wait_for_completion(irq,sc_time(1,SC_NS));
                    host.save_diagnostics(output_.empty()?"full_pipeline_timeout":output_.string());
                    throw std::runtime_error("activation timeout; snapshot and recovery diagnostics saved");
                }
                require(!memory.in_flight,"IRQ while memory response is pending");
                require(!encoder.registers.busy(),"IRQ with BUSY set");
                const auto completed=sc_time_stamp();
                write(reg::SCON,0);
                const auto status=host.read(reg::STAT),words=host.read(reg::STM_LEN);
                require(status==(reg::NORMAL|frames),"activation error/status mismatch");
                require(uint64_t(words)*4==expected.size(),"STM_LEN differs from golden");
                // All accelerator writes must target allocated REFM/NAL regions.
                for(size_t i=trace_begin;i<memory.trace.size();++i) {
                    const auto& tx=memory.trace[i];
                    require(tx.completed<=completed,"memory completed after IRQ");
                    if(tx.write) require((tx.address>=REFM && tx.address+tx.bytes<=REFM+3*frame_bytes) ||
                        (tx.address>=NAL && tx.address+tx.bytes<=NAL+capacity),"DMA write outside owned region");
                }
                std::vector<unsigned char> actual(words*4);
                host.transfer(false,NAL,actual);
                if(!output_.empty()) {
                    std::ofstream file(output_/("activation_"+std::to_string(activation)+".bin"),std::ios::binary);
                    file.write(reinterpret_cast<const char*>(actual.data()),actual.size());
                    require(bool(file),"cannot save actual output");
                }
                compare(actual,expected,"bitstream");
                host.transfer(false,NAL-64,protected_output);
                for(unsigned i=0;i<64;++i) require(protected_output[i]==0xa7 && protected_output[64+capacity+i]==0xa7,"NAL guard overwritten");
                if(case_.values.count("expected_ref")) {
                    auto ref=case_.data("expected_ref");
                    auto offset=case_.number("reference_offset",0);
                    require(uint64_t(offset)+ref.size()<=3*frame_bytes && !ref.empty(),"reference golden bounds");
                    std::vector<unsigned char> got(ref.size()); host.transfer(false,REFM+offset,got);
                    compare(got,ref,"reference");
                }
                wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
                require(!irq.read(),"interrupt acknowledge failed");
                const auto count=memory.trace.size(); wait(100,SC_NS);
                require(memory.trace.size()==count,"late DMA after completed activation");
                std::cout<<"activation="<<activation<<" words="<<words<<" completed="<<completed<<'\n';
            }
            passed=true; std::cout<<"PASS full-pipeline harness\n";
        } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; }
        sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    try {
        if(argc<3 || argc>4) throw std::runtime_error("usage: full_pipeline_tb stub|real case.cfg [output-directory]");
        const std::string backend=argv[1]; PipelineFactory factory;
        if(backend=="stub") factory=make_stub_pipeline;
#ifdef H264_HAVE_RELEASED_PIPELINE
        else if(backend=="real") factory=make_released_pipeline;
#endif
#ifdef H264_CONTRACT_FIXTURE
        else if(backend=="contract") factory=make_contract_pipeline;
#endif
        else throw std::runtime_error("backend unavailable: "+backend+"; no fallback to stub");
        std::cout<<"Backend: "<<backend<<(backend=="real"?" (team release)":" (test fixture, NOT H.264)")<<'\n';
        TestCase config(argv[2]);
        FullPipelineBench bench("bench",config,factory,argc==4?fs::path(argv[3]):fs::path{});
        sc_start(); return bench.passed?0:1;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
