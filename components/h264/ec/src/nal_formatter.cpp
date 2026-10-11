#include "nal_formatter.h"
#include <algorithm>
namespace h264::ec {
std::vector<std::uint8_t> NalFormatter::format_rbsp(const std::uint8_t* rbsp,std::size_t bytes,
        unsigned ref,unsigned type,std::size_t capacity) {
    if(ref>3 || type==0 || type>31 || (!rbsp && bytes)) throw std::invalid_argument("NAL fields/buffer");
    if(capacity<5 || bytes>capacity-5) throw std::overflow_error("NAL capacity");
    std::vector<std::uint8_t> out{0,0,0,1,static_cast<std::uint8_t>((ref<<5)|type)};
    unsigned zeros=0;
    for(std::size_t i=0;i<bytes;++i) {
        unsigned v=rbsp[i];
        if(zeros==2 && v<=3) {if(out.size()==capacity) throw std::overflow_error("NAL escape capacity");out.push_back(3);zeros=0;}
        if(out.size()==capacity) throw std::overflow_error("NAL capacity");
        out.push_back(v);zeros=v==0?zeros+1:0;
    }
    return out;
}
void NalFormatter::wrap_nal_unit(EcResult& res,std::uint8_t ref,std::uint8_t type) {
    if(res.stream_length>res.nal_stream.size()) throw std::overflow_error("NAL input length");
    auto out=format_rbsp(res.nal_stream.data(),res.stream_length,ref,type,res.nal_stream.size());
    while(out.size()%4) out.push_back(0); // DMA padding, not RBSP termination.
    if(out.size()>res.nal_stream.size()) throw std::overflow_error("NAL word capacity");
    std::copy(out.begin(),out.end(),res.nal_stream.begin());res.stream_length=out.size();
}
} // namespace h264::ec
