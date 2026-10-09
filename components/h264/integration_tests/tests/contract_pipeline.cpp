// Test-only replacement: variable frame length, delayed final drain, no EOS convention.
#include <h264/top/pipeline_factory.h>
#include <tlm_utils/simple_initiator_socket.h>
namespace h264 {
class ContractPipeline : public sc_core::sc_module,public FrameExecutorIf {
public:
    tlm_utils::simple_initiator_socket<ContractPipeline> dma{"dma"};
    ResetDomain& domain;
    unsigned count=0;
    ContractPipeline(sc_core::sc_module_name name,ResetDomain& reset):sc_module(name),domain(reset) {}
    void reset() override { count=0; }
    SyntaxRequirements syntax_requirements(const FrameConfig&) const override { return {0,0,0,0}; }
    bool abort_and_drain(uint64_t) override { return true; }
    void begin_activation(const FrameConfig&,uint64_t) override { count=0; }
    uint32_t execute(const FrameConfig&,uint64_t,unsigned frame) override {
        count+=frame+2; return 0; // Data buffered until final drain.
    }
    uint32_t end_activation(const FrameConfig& c,uint64_t generation,uint32_t) override {
        sc_core::wait(75,sc_core::SC_NS);
        if(!domain.valid(generation) || count*4>c.sequence.nal_capacity_bytes) throw std::runtime_error("contract capacity/reset");
        std::vector<unsigned char> bytes(count*4);
        nal_words_accepted(generation,count);
        for(unsigned i=0;i<bytes.size();++i) bytes[i]=static_cast<unsigned char>(i+1);
        EpochExtension epoch(domain); epoch.generation=generation;
        tlm::tlm_generic_payload tx;
        tx.set_extension(&epoch); tx.set_command(tlm::TLM_WRITE_COMMAND); tx.set_address(c.nal);
        tx.set_data_ptr(bytes.data()); tx.set_data_length(bytes.size()); tx.set_streaming_width(bytes.size());
        sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
        dma->b_transport(tx,delay); consume_delay(delay); tx.clear_extension<EpochExtension>();
        if(tx.is_response_error() || !domain.valid(generation)) throw std::runtime_error("contract DMA");
        return count;
    }
};
std::unique_ptr<FrameExecutorIf> make_contract_pipeline(sc_core::sc_module_name name,ResetDomain& reset,DmaArbiter& arbiter) {
    auto p=std::make_unique<ContractPipeline>(name,reset); p->dma.bind(arbiter.clients); return p;
}
}
