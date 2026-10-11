#pragma once
#include "communication_pipeline.h"
#include "slice_encoder.h"
#include "ftq.h"
#include "itq.h"
namespace codec_integration {
using prediction_test::require;
class Pipeline:public communication_test::Pipeline {
public:
    h264::tq::TransposeRam transpose{"codec_transpose"};
    h264::tq::Ftq ftq{transpose};h264::tq::Itq itq;
    h264::ec::SequenceSyntax sequence;
    std::vector<h264::ec::CodedMacroblock> macroblocks;
    std::vector<unsigned char> pending;
    unsigned chroma_groups=0;
    Pipeline(sc_core::sc_module_name n,h264::ResetDomain& r,h264::DmaTransport& d)
        :communication_test::Pipeline(n,r,d,true,32,4,true) {}
    h264::SyntaxRequirements syntax_requirements(const h264::FrameConfig& c) const override {
        return {4,4,c.sequence.log2_fn,c.sequence.log2_poc};
    }
    void reset() override {communication_test::Pipeline::reset();pending.clear();macroblocks.clear();}
    void emit(const std::vector<unsigned char>& bytes,uint64_t g) {
        require(nal.byte_count()+pending.size()+bytes.size()+7<=output_capacity,"codec output capacity");
        expected_output.insert(expected_output.end(),bytes.begin(),bytes.end());
        pending.insert(pending.end(),bytes.begin(),bytes.end());
        unsigned consumed=0;
        while(pending.size()-consumed>=4) {
            nal.accept_word(h264::load_le(pending.data()+consumed));nal_words_accepted(g,1);consumed+=4;
        }
        pending.erase(pending.begin(),pending.begin()+consumed);
        nal.flush_chunk();pump(g);require(nal.final_b_accepted() && domain.valid(g),"codec output drain");
    }
    void begin_activation(const h264::FrameConfig& c,uint64_t g) override {
        require(c.sequence.gop_n==1,"codec test GOP");
        require(!c.filter_enabled || (c.alpha%2==0 && c.beta%2==0 && c.alpha>=-12 && c.alpha<=12 && c.beta>=-12 && c.beta<=12),"DF offsets must be exactly representable in slice syntax");
        communication_test::Pipeline::begin_activation(c,g);
        sequence={};sequence.width=c.width;sequence.height=c.height;sequence.qp=c.sequence.qp;
        sequence.frame_num_bits=c.sequence.log2_fn;sequence.poc_bits=c.sequence.log2_poc;sequence.crop_bottom=c.crop_bottom;
        h264::ec::SliceSyntax validation;validation.qp_delta=c.slice_qp_delta;
        h264::ec::SyntaxBuilder::validate(sequence,validation);chroma_groups=0;
        emit(h264::ec::SliceEncoder::parameter_sets(sequence),g);
    }
    void configure_prediction(h264::intra::Extension& e) override {
        // H264 signals one chroma mode for both U/V. Select legal DC for both
        // through the real predictor's mode-decision interface.
        if(e.block.plane!=h264::intra::Plane::Y) {e.mode_penalty.fill(1u<<30);e.mode_penalty[0]=0;}
    }
    void record_prediction(unsigned plane,unsigned b,unsigned mode) override {
        auto& mb=macroblocks[mb_y*(size/16)+mb_x];
        if(!plane) mb.intra_modes[b]=mode;else {require(mode==0,"shared chroma DC mode");mb.chroma_mode=mode;}
    }
    std::vector<unsigned char> code(const std::vector<unsigned char>& src,const std::vector<unsigned char>& pred,
            unsigned n,bool intra,unsigned plane,unsigned qp,uint64_t g) override {
        auto& mb=macroblocks[mb_y*(size/16)+mb_x];mb.intra=intra;
        if(!plane) {
            require(n==4,"luma block size");std::array<unsigned char,32> residual{},levels{};std::array<unsigned char,16> predictor{},out{};
            for(unsigned i=0;i<16;++i){auto v=uint16_t(int(src[i])-pred[i]);residual[2*i]=v&255;residual[2*i+1]=v>>8;predictor[i]=pred[i];}
            io(tq_port,0,true,residual.data(),32,g);io(tq_port,4,true,predictor.data(),16,g);byte(tq_port,8,qp,g);byte(tq_port,9,intra,g);byte(tq_port,12,128,g);
            valid(tq_port,g);io(tq_port,16,false,levels.data(),32,g);io(tq_port,48,false,out.data(),16,g);
            unsigned x=origin_x/4,y=origin_y/4,index=(y/2)*8+(x/2)*4+(y%2)*2+x%2;
            for(unsigned i=0;i<16;++i)mb.luma[index][i]=static_cast<std::int16_t>(levels[2*i]|unsigned(levels[2*i+1])<<8);
            nonzero[0][(mb_y*4+origin_y/4)*(size/4)+mb_x*4+origin_x/4]=std::any_of(mb.luma[index].begin(),mb.luma[index].end(),[](auto v){return v!=0;});
            checkpoint("tq",g);++coded_blocks;return {out.begin(),out.end()};
        }
        require(n==8,"gather complete chroma macroblock before DC quantization");
        unsigned qpc=h264::df::chroma_qp(qp);
        std::array<std::array<std::int32_t,16>,4> coeff{};std::array<std::array<unsigned char,16>,4> predictors{};
        std::array<std::int32_t,4> dc{};
        for(unsigned b=0;b<4;++b){std::array<std::int16_t,16> residual{};
            for(unsigned j=0;j<4;++j)for(unsigned i=0;i<4;++i){unsigned k=(b/2*4+j)*8+b%2*4+i;predictors[b][j*4+i]=pred[k];residual[j*4+i]=int(src[k])-pred[k];}
            coeff[b]=ftq.transform(residual,h264::tq::BlockClass::Luma4x4);dc[b]=coeff[b][0];coeff[b][0]=0;
            mb.chroma_ac[plane-1][b]=ftq.quantize(coeff[b],qpc,h264::tq::BlockClass::ChromaAc,intra);
        }
        mb.chroma_dc[plane-1]=ftq.quantize_chroma_dc(dc,qpc,intra);
        auto restored=itq.inverse_chroma_dc(mb.chroma_dc[plane-1],qpc);
        std::vector<unsigned char> result(64);
        for(unsigned b=0;b<4;++b){auto values=itq.reconstruct(itq.inverse_transform(mb.chroma_ac[plane-1][b],qpc,restored[b]),predictors[b]);
            for(unsigned j=0;j<4;++j)for(unsigned i=0;i<4;++i)result[(b/2*4+j)*8+b%2*4+i]=values[j*4+i];}
        coded_blocks+=4;++chroma_groups;checkpoint("tq",g);return result;
    }
    void blocks_path(const h264::MacroblockPixels& pixels,const h264::MacroblockPixels& pred,h264::MacroblockPixels& result,unsigned qp,uint64_t g) override {
        for(unsigned p=0;p<3;++p){unsigned stride=p?8:16,n=p?8:4;
            const auto* src=p==0?pixels.y.data():p==1?pixels.u.data():pixels.v.data();const auto* pp=p==0?pred.y.data():p==1?pred.u.data():pred.v.data();auto* dst=p==0?result.y.data():p==1?result.u.data():result.v.data();
            for(unsigned y=0;y<stride;y+=n)for(unsigned x=0;x<stride;x+=n){std::vector<unsigned char> a(n*n),b(n*n);
                for(unsigned j=0;j<n;++j)for(unsigned i=0;i<n;++i){a[j*n+i]=src[(y+j)*stride+x+i];b[j*n+i]=pp[(y+j)*stride+x+i];}
                origin_x=x;origin_y=y;auto rec=code(a,b,n,false,p,qp,g);
                for(unsigned j=0;j<n;++j)for(unsigned i=0;i<n;++i)dst[(y+j)*stride+x+i]=rec[j*n+i];}
        }
    }
    unsigned edge_strength(unsigned plane,unsigned bx,unsigned by,unsigned n,unsigned axis,unsigned local) override {
        return communication_test::Pipeline::edge_strength(0,plane?bx*2:bx,plane?by*2:by,size,axis,local);
    }
    unsigned sample_strength(unsigned plane,unsigned bx,unsigned by,unsigned n,unsigned axis,unsigned local,unsigned row) override {
        if(!plane) return edge_strength(plane,bx,by,n,axis,local);
        return communication_test::Pipeline::edge_strength(0,bx*2+(axis?row/2:0),by*2+(axis?0:row/2),size,axis,local);
    }
    uint32_t execute(const h264::FrameConfig& c,uint64_t g,unsigned f) override {
        unsigned before=nal.stm_len();macroblocks.assign(4,{});
        communication_test::Pipeline::execute(c,g,f);
        h264::ec::SliceSyntax syntax;syntax.idr=f==0;syntax.p_slice=f!=0;syntax.frame_num=f;syntax.poc=2*f;syntax.qp_delta=c.slice_qp_delta;
        syntax.disable_deblocking=c.filter_enabled?0:1;syntax.alpha_offset_div2=c.alpha/2;syntax.beta_offset_div2=c.beta/2;
        emit(h264::ec::SliceEncoder::picture(sequence,syntax,macroblocks,output_capacity),g);
        words=nal.stm_len();return words-before;
    }
    uint32_t end_activation(const h264::FrameConfig& c,uint64_t g,uint32_t total) override {
        if(!pending.empty())emit(std::vector<unsigned char>(4-pending.size(),0),g);
        require(pending.empty(),"NAL byte tail");return communication_test::Pipeline::end_activation(c,g,total);
    }
};
}
