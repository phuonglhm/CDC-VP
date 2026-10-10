#include <h264/stubs/frame_pipeline_stub.h>
#include <h264/top/pipeline_factory.h>
namespace h264 {
ProcessedBlock ProcessingStub::process(const Macroblock& input) {
    sc_core::wait(latency);
    uint32_t hash=2166136261u;
    for(auto byte:input.pixels) hash=(hash^byte)*16777619u;
    ProcessedBlock out{input,std::vector<unsigned char>(4)};
    store_le(out.output.data(),hash);
    return out;
}
FramePipelineStub::FramePipelineStub(sc_core::sc_module_name name,ResetDomain& reset,std::unique_ptr<ProcessingIf> processor)
    : sc_module(name),reset_(reset),processor_(std::move(processor)) {}
void FramePipelineStub::begin_activation(const FrameConfig& c,uint64_t generation) {
    const uint64_t bytes=uint64_t(c.width/16)*(c.height/16)*4*c.activation_frames+4;
    if (!reset_.valid(generation) || bytes>c.sequence.nal_capacity_bytes)
        throw std::runtime_error("stub output capacity/reset");
}
void FramePipelineStub::transfer(tlm_utils::simple_initiator_socket<FramePipelineStub>& port,
    bool write,uint64_t address,unsigned char* data,unsigned bytes,uint64_t generation) {
    if(!reset_.valid(generation)) throw std::runtime_error("stale frame");
    if (&port==&nal && write) nal_words_accepted(generation,bytes/4);
    DmaClientExtension client; client.client=(&port==&nal)?ClientId::NAL:(&port==&reference)?ClientId::DF:ClientId::CMB;
    EpochExtension epoch(reset_); epoch.generation=generation;
    tlm::tlm_generic_payload tx;
    tx.set_extension(&epoch); tx.set_extension(&client); tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
    tx.set_address(address); tx.set_data_ptr(data); tx.set_data_length(bytes); tx.set_streaming_width(bytes);
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    port->b_transport(tx,delay); consume_delay(delay);
    tx.clear_extension<EpochExtension>(); tx.clear_extension<DmaClientExtension>();
    if(tx.is_response_error() || !reset_.valid(generation)) throw std::runtime_error("DMA/reset failure");
}
void FramePipelineStub::tile(const FrameConfig& c,Macroblock& mb,bool write,uint64_t base,uint64_t generation) {
    const uint64_t luma=uint64_t(c.width)*c.height;
    unsigned offset=0;
    for(unsigned plane=0;plane<3;++plane) {
        const unsigned size=plane?8:16, stride=plane?c.width/2:c.width;
        const uint64_t plane_base=plane==0?0:plane==1?luma:luma+luma/4;
        for(unsigned row=0;row<size;++row) {
            const uint64_t address=base+plane_base+(mb.y*size+row)*stride+mb.x*size;
            transfer(write?reference:cmb,write,address,mb.pixels.data()+offset,size,generation);
            offset+=size;
        }
    }
}
uint32_t FramePipelineStub::execute(const FrameConfig& c,uint64_t generation,unsigned frame) {
    const unsigned mb_count=(c.width/16)*(c.height/16);
    unsigned index=0;
    for(unsigned y=0;y<c.height/16;++y) for(unsigned x=0;x<c.width/16;++x) {
        Macroblock mb; mb.frame=frame; mb.x=x; mb.y=y;
        tile(c,mb,false,uint64_t(c.cmb)+frame*c.frame_bytes(),generation);
        auto result=processor_->process(mb);
        if(result.output.size()!=4) throw std::runtime_error("stub contract: one word per macroblock");
        tile(c,result.reconstructed,true,uint64_t(c.refm)+((frame+1)%3)*c.frame_bytes(),generation);
        transfer(nal,true,uint64_t(c.nal)+4*(uint64_t(frame)*mb_count+index),result.output.data(),4,generation);
        ++index;
    }
    return index;
}
uint32_t FramePipelineStub::end_activation(const FrameConfig& c,uint64_t generation,uint32_t words) {
    unsigned char eos[4]={0,0,1,0x0b};
    transfer(nal,true,uint64_t(c.nal)+uint64_t(words)*4,eos,4,generation);
    return words+1;
}
std::unique_ptr<FrameExecutorIf> make_stub_pipeline(sc_core::sc_module_name name,ResetDomain& reset,DmaTransport& arbiter) {
    auto pipeline=std::make_unique<FramePipelineStub>(name,reset,std::make_unique<ProcessingStub>());
    pipeline->cmb.bind(arbiter.clients); pipeline->reference.bind(arbiter.clients); pipeline->nal.bind(arbiter.clients);
    return pipeline;
}
}
