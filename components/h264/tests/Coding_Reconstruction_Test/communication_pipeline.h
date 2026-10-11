#pragma once
#include "../Prediction_Test/prediction_fixture.h"
#include "tq_top.h"
#include "ec_top.h"
#include "df_top.h"
namespace communication_test {
using namespace prediction_test;
// Test assembly for HAS 3.4/3.5: real module protocols, explicit non-codec EC payload.
// Reuse only the existing request pump; override workload and prediction feedback.
class Pipeline: public prediction_test::Pipeline {
public:
    h264::tq::TqTop tq{"tq"};
    h264::ec::EcTop ec;
    h264::df::DfTop filter{"filter"};
    tlm_utils::simple_initiator_socket<Pipeline> tq_port{"tq_port"},ec_port{"ec_port"},filter_port{"filter_port"};
    bool full,behavior;
    unsigned size,frame_count,mb_x=0,mb_y=0,current_frame=0,origin_x=0,origin_y=0;
    bool filter_enabled=false,stop_requested=false;
    int alpha_offset=0,beta_offset=0;
    std::string checkpoint_name,hold_at;
    sc_core::sc_event checkpoint_event,release_checkpoint;
    std::vector<unsigned> record_lengths;
    struct BlockEvent {unsigned frame,mbx,mby,plane,x,y;};
    std::vector<BlockEvent> block_events;
    std::array<std::vector<bool>,3> nonzero;
    void checkpoint(const char* name,uint64_t g) {
        checkpoint_name=name;checkpoint_event.notify(sc_core::SC_ZERO_TIME);
        if(hold_at==name) {
            wait(release_checkpoint | domain.changed);
            require(domain.valid(g),"checkpoint reset");
            require(!stop_requested,"injected pipeline error");
        }
    }
    std::vector<unsigned char> expected_output;
    std::vector<std::vector<unsigned char>> expected_reference, source_seen;
    unsigned coded_blocks=0,filter_edges=0,intra_feedback=0,inter_samples=0;
    unsigned output_capacity=0;
    size_t filter_pass_begin=0;
    unsigned changed_filter_samples=0;
    Pipeline(sc_module_name n,h264::ResetDomain& r,h264::DmaTransport& dma,bool f,unsigned edge=16,unsigned count=2,bool behavioral=false)
      :prediction_test::Pipeline(n,r,dma,4,edge,edge),ec("ec",true,behavioral),full(f),behavior(behavioral),size(edge),frame_count(count) {
        tq_port.bind(tq.socket); ec_port.bind(ec.socket); filter_port.bind(filter.socket);
    }
    void reset() override {
        prediction_test::Pipeline::reset(); tq.reset(); ec.reset(); filter.reset();
    }
    void begin_activation(const h264::FrameConfig& c,uint64_t g) override {
        require(domain.valid(g),"activation generation");
        require(c.width==size && c.height==size && c.activation_frames==frame_count && frame_count<=4,"configured test geometry");
        require(behavior || !c.filter_enabled,"filter requires behavior assembly");
        alpha_offset=c.alpha; beta_offset=c.beta;
        filter_enabled=c.filter_enabled; stop_requested=false;
        record_lengths.clear();block_events.clear();filter_pass_begin=df.pass_log().size();changed_filter_samples=0;
        require(!queue.busy(),"old requests must drain before begin");
        enable.write(false);wait(SC_ZERO_TIME);wait(SC_ZERO_TIME);
        enable.write(true);wait(SC_ZERO_TIME);wait(SC_ZERO_TIME);
        sw.set_refm_base(c.refm);sw.invalidate_all();
        df.set_dims({size,size});df.set_refm_base(c.refm);
        df.latch_df_enable(false);
        output_capacity=c.sequence.nal_capacity_bytes;nal.on_disable();nal.configure(c.nal,output_capacity);
        reset(); expected_output.clear(); expected_reference.clear(); source_seen.clear();
        coded_blocks=filter_edges=intra_feedback=inter_samples=0;
    }
    using Port=tlm_utils::simple_initiator_socket<Pipeline>;
    tlm::tlm_response_status raw(Port& port,uint64_t address,bool wr,unsigned char* data,unsigned n) {
        tlm::tlm_generic_payload tx;
        tx.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(address);tx.set_data_ptr(data);tx.set_data_length(n);tx.set_streaming_width(n);
        sc_time delay=SC_ZERO_TIME;port->b_transport(tx,delay);h264::consume_delay(delay);
        return tx.get_response_status();
    }
    void io(Port& port,uint64_t address,bool wr,unsigned char* data,unsigned n,uint64_t g) {
        require(domain.valid(g),"stale block before transport");
        auto status=raw(port,address,wr,data,n);
        require(status==tlm::TLM_OK_RESPONSE,"block transaction rejected");
        require(domain.valid(g),"stale block after transport");
    }
    void byte(Port& p,unsigned a,unsigned char v,uint64_t g) {io(p,a,true,&v,1,g);}
    void valid(Port& p,uint64_t g) {
        unsigned char v[4]={};io(p,0x40,false,v,4,g);require(h264::load_le(v)==1,"block not valid");
    }
    virtual std::vector<unsigned char> code(const std::vector<unsigned char>& src,
            const std::vector<unsigned char>& pred,unsigned n,bool intra_mode,unsigned plane,
            unsigned qp,uint64_t g) {
        require(src.size()==n*n && pred.size()==src.size(),"block sizes");
        std::vector<unsigned char> recon(n*n);
        for(unsigned by=0;by<n;by+=4) for(unsigned bx=0;bx<n;bx+=4) {
            std::array<unsigned char,32> residual{},levels{};
            std::array<unsigned char,16> predictor{},out{};
            for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
                unsigned k=y*4+x,j=(by+y)*n+bx+x;
                auto v=uint16_t(int(src[j])-int(pred[j]));
                residual[2*k]=v&255;residual[2*k+1]=v>>8;predictor[k]=pred[j];
            }
            io(tq_port,0,true,residual.data(),32,g);io(tq_port,4,true,predictor.data(),16,g);
            byte(tq_port,8,qp,g);byte(tq_port,9,intra_mode,g);
            byte(tq_port,12,0x80|(plane?3:0),g);
            valid(tq_port,g);checkpoint("tq",g);io(tq_port,16,false,levels.data(),32,g);
            io(tq_port,48,false,out.data(),16,g);
            // Output remains stable while its consumer stalls; no next START yet.
            wait(3,SC_NS);
            std::array<unsigned char,32> held{};io(tq_port,16,false,held.data(),32,g);
            require(held==levels,"TQ output overwritten before consumption");
            unsigned char repeated=0x80|(plane?3:0);
            require(raw(tq_port,12,true,&repeated,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"TQ duplicate START accepted");
            for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x)
                recon[(by+y)*n+bx+x]=out[y*4+x];
            io(ec_port,0,true,levels.data(),32,g);byte(ec_port,0x20,qp,g);
            byte(ec_port,0x24,0,g);byte(ec_port,0x28,0x80,g);valid(ec_port,g);
            unsigned char length[4]={};io(ec_port,0xd0,false,length,4,g);
            const unsigned count=h264::load_le(length);require(count && count<=128,"EC output bounds");checkpoint("ec",g);
            std::array<unsigned char,128> output{};io(ec_port,0x50,false,output.data(),128,g);
            std::vector<unsigned char> record={'T',static_cast<unsigned char>(behavior?'V':'C'),static_cast<unsigned char>(qp),0};
            if(behavior) {
                unsigned nz=0;
                for(unsigned k=0;k<16;++k) if(levels[2*k] || levels[2*k+1]) {
                    record.push_back(k);record.push_back(levels[2*k]);record.push_back(levels[2*k+1]);++nz;
                }
                record[3]=nz;
            } else record.insert(record.end(),levels.begin(),levels.end());
            require(count==record.size(),"EC variable byte length");
            unsigned char repeated_ec=0x80;
            require(raw(ec_port,0x28,true,&repeated_ec,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"EC duplicate START accepted");
            record_lengths.push_back(count);
            require(std::equal(record.begin(),record.end(),output.begin()),"TQ to EC data/order");
            while(record.size()%4) record.push_back(0);
            expected_output.insert(expected_output.end(),record.begin(),record.end());
            // Preflight the complete record so capacity errors leave no half-staged record.
            require(nal.byte_count()+record.size()<=output_capacity,"NAL record exceeds allocation");
            checkpoint("nal",g);
            for(unsigned i=0;i<record.size();i+=4) {
                nal.accept_word(h264::load_le(record.data()+i));nal_words_accepted(g,1);
            }
            nal.flush_chunk();pump(g);
            require(nal.final_b_accepted(),"NAL response not committed");
            unsigned stride=(plane?size/2:size)/4;
            unsigned gx=mb_x*(plane?2:4)+(origin_x+bx)/4,gy=mb_y*(plane?2:4)+(origin_y+by)/4;
            nonzero[plane][gy*stride+gx]=std::any_of(levels.begin(),levels.end(),[](auto v){return v!=0;});
            block_events.push_back({current_frame,mb_x,mb_y,plane,gx,gy});
            ++coded_blocks;
        }
        return recon;
    }
    static std::vector<unsigned char> flatten(const h264::MacroblockPixels& p) {
        std::vector<unsigned char> b(p.y.begin(),p.y.end());
        b.insert(b.end(),p.u.begin(),p.u.end());b.insert(b.end(),p.v.begin(),p.v.end());return b;
    }
    virtual void configure_prediction(h264::intra::Extension&) {}
    virtual void record_prediction(unsigned,unsigned,unsigned) {}
    void intra_path(const h264::MacroblockPixels& pixels,h264::MacroblockPixels& result,unsigned qp,uint64_t g) {
        const unsigned scan[16]={0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15};
        for(unsigned b=0;b<18;++b) {
            unsigned p=b<16?0:b-15,n=p?8:4,x=p?0:scan[b]%4*4,y=p?0:scan[b]/4*4,stride=p?8:16;
            const auto* src=p==0?pixels.y.data():p==1?pixels.u.data():pixels.v.data();
            auto* dst=p==0?result.y.data():p==1?result.u.data():result.v.data();
            std::vector<unsigned char> original(n*n),pred(n*n);
            for(unsigned j=0;j<n;++j) for(unsigned i=0;i<n;++i) original[j*n+i]=src[(y+j)*stride+x+i];
            h264::intra::Extension ext;
            ext.block={static_cast<h264::intra::Plane>(p),p?h264::intra::Kind::Chroma8x8:h264::intra::Kind::Luma4x4,mb_x*(p?8:16)+x,mb_y*(p?8:16)+y};
            configure_prediction(ext);
            auto data=original;op(intra_port,0,true,data,ext,g);
            require(bool(ext.decision),"intra decision");
            record_prediction(p,b,ext.decision->mode);
            op(intra_port,1,false,pred,ext,g);
            for(unsigned i=0;i<pred.size();++i) require(ext.decision->residual[i]==int(original[i])-pred[i],"intra residual link");
            checkpoint("prediction",g);origin_x=x;origin_y=y;
            auto decoded=code(original,pred,n,true,p,qp,g);
            // HAS 3.4: actual TQ reconstruction feeds intra neighbors.
            op(intra_port,2,true,decoded,ext,g);require(ext.block_done,"intra feedback accepted");
            for(unsigned j=0;j<n;++j) for(unsigned i=0;i<n;++i) dst[(y+j)*stride+x+i]=decoded[j*n+i];
            ++intra_feedback;
        }
    }
    h264::MacroblockPixels inter_predict(const h264::FrameConfig& c,const h264::MacroblockPixels& pixels,uint64_t g) {
        using namespace h264::inter;
        sw.set_ref_slot(h264::RefList::List0,current_frame%3,current_frame);sw.fill_window(h264::RefList::List0,0,0);pump(g);
        require(sw.ready(h264::RefList::List0),"SW completion before prediction");
        Reference ref{List::L0,{current_frame,current_frame%3}};inter.retag(ref.list,ref.tag,size,size);
        for(unsigned p=0;p<3;++p) {
            const auto& v=sw.plane(h264::RefList::List0,p);
            inter.refill(ref,static_cast<Plane>(p),v.x,v.y,v.width,v.height,v.pixels);
        }
        Extension ext;ext.request.mb_x=mb_x*16;ext.request.mb_y=mb_y*16;
        ext.request.integer_candidates={{ref,{0,0},0}};
        std::vector<unsigned char> data(pixels.y.begin(),pixels.y.end());op(inter_port,0,true,data,ext,g);
        require(ext.syntax && ext.decision,"inter syntax/predictor ready");
        data.clear();op(inter_port,1,true,data,ext,g);
        h264::MacroblockPixels predictor;std::array<bool,384> seen{};
        for(unsigned i=0;i<384;++i) {
            data.assign(1,0);op(inter_port,2,false,data,ext,g);require(bool(ext.sample),"MC sample");
            auto s=*ext.sample;unsigned p=unsigned(s.plane),stride=p?8:16;
            require(s.x>=mb_x*stride && s.x<(mb_x+1)*stride && s.y>=mb_y*stride && s.y<(mb_y+1)*stride,"MC absolute coordinates");
            unsigned j=(s.y-mb_y*stride)*stride+s.x-mb_x*stride,k=(p==0?0:p==1?256:320)+j;
            require(k<384 && !seen[k] && s.sequence==i && s.last==(i==383),"MC metadata/order");
            seen[k]=true;auto* dst=p==0?predictor.y.data():p==1?predictor.u.data():predictor.v.data();
            dst[j]=data[0];auto held=data;wait(2,SC_NS);op(inter_port,2,false,held,ext,g);
            require(held==data && ext.sample->sequence==i,"MC held sample");
            ext.accept_sequence=i;op(inter_port,3,true,data,ext,g);++inter_samples;
        }
        require(inter.done(),"inter final accept");
        require(flatten(predictor)==flatten(tile(expected_reference.at(current_frame-1),mb_x,mb_y)),"SW/zero-MV predictor differs from previous completed reference");
        checkpoint("prediction",g);
        return predictor;
    }
    virtual void blocks_path(const h264::MacroblockPixels& pixels,const h264::MacroblockPixels& pred,
                     h264::MacroblockPixels& result,unsigned qp,uint64_t g) {
        for(unsigned p=0;p<3;++p) {
            unsigned stride=p?8:16;
            const auto* src=p==0?pixels.y.data():p==1?pixels.u.data():pixels.v.data();
            const auto* pp=p==0?pred.y.data():p==1?pred.u.data():pred.v.data();
            auto* dst=p==0?result.y.data():p==1?result.u.data():result.v.data();
            for(unsigned y=0;y<stride;y+=4) for(unsigned x=0;x<stride;x+=4) {
                std::vector<unsigned char> a(16),b(16);
                for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) {
                    a[j*4+i]=src[(y+j)*stride+x+i];b[j*4+i]=pp[(y+j)*stride+x+i];
                }
                origin_x=x;origin_y=y;auto rec=code(a,b,4,!full,p,qp,g);
                for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) dst[(y+j)*stride+x+i]=rec[j*4+i];
            }
        }
    }
    h264::MacroblockPixels tile(const std::vector<unsigned char>& frame,unsigned x,unsigned y) const {
        h264::MacroblockPixels result;
        for(unsigned p=0;p<3;++p) {
            unsigned n=p?8:16,stride=p?size/2:size,base=p==0?0:p==1?size*size:size*size*5/4;
            auto* dst=p==0?result.y.data():p==1?result.u.data():result.v.data();
            for(unsigned j=0;j<n;++j) for(unsigned i=0;i<n;++i) dst[j*n+i]=frame[base+(y*n+j)*stride+x*n+i];
        }
        return result;
    }
    void place(std::vector<unsigned char>& frame,const h264::MacroblockPixels& data,unsigned x,unsigned y) const {
        for(unsigned p=0;p<3;++p) {
            unsigned n=p?8:16,stride=p?size/2:size,base=p==0?0:p==1?size*size:size*size*5/4;
            const auto* src=p==0?data.y.data():p==1?data.u.data():data.v.data();
            for(unsigned j=0;j<n;++j) for(unsigned i=0;i<n;++i) frame[base+(y*n+j)*stride+x*n+i]=src[j*n+i];
        }
    }
    virtual unsigned edge_strength(unsigned plane,unsigned bx,unsigned by,unsigned n,unsigned axis,unsigned local) {
        bool nz=nonzero[plane][by*(n/4)+bx] || nonzero[plane][(by-(axis?1:0))*(n/4)+bx-(axis?0:1)];
        h264::df::EdgeSide a{},b{};a.intra=b.intra=(!full || current_frame==0);a.nonzero=nz;
        return filter_enabled?h264::df::boundary_strength(a,b,local==0):0;
    }
    virtual unsigned sample_strength(unsigned plane,unsigned bx,unsigned by,unsigned n,unsigned axis,unsigned local,unsigned row) {
        return edge_strength(plane,bx,by,n,axis,local);
    }
    void filter_frame(std::vector<unsigned char>& frame,unsigned qp,uint64_t g) {
        auto before=frame;
        // Raster MB order, vertical then horizontal, including left/top external edges.
        for(unsigned my=0;my<size/16;++my) for(unsigned mx=0;mx<size/16;++mx)
        for(unsigned axis=0;axis<2;++axis) for(unsigned p=0;p<3;++p) {
            unsigned n=p?size/2:size,mb=p?8:16,base=p==0?0:p==1?size*size:size*size*5/4;
            for(unsigned local=0;local<mb;local+=4) {
                unsigned edge=(axis?my:mx)*mb+local;if(!edge) continue;
                for(unsigned along=(axis?mx:my)*mb;along<(axis?mx+1:my+1)*mb;along+=4) {
                    std::array<unsigned char,16> a{},b{},outa{},outb{};
                    auto idx=[&](unsigned i,unsigned j,bool q) {
                        return base+(axis?(edge-(q?0:4)+i)*n+along+j:(along+j)*n+edge-(q?0:4)+i);
                    };
                    for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) {a[j*4+i]=frame[idx(i,j,false)];b[j*4+i]=frame[idx(i,j,true)];}
                    unsigned bx=axis?along/4:edge/4,by=axis?edge/4:along/4;
                    std::array<unsigned,4> strengths{};
                    for(unsigned row=0;row<4;++row) strengths[row]=sample_strength(p,bx,by,n,axis,local,row);
                    bool split=!std::all_of(strengths.begin(),strengths.end(),[&](auto value){return value==strengths[0];});
                    for(unsigned part=0;part<(split?4u:1u);++part) {
                    unsigned bs=strengths[part];
                    io(filter_port,0,true,a.data(),16,g);io(filter_port,16,true,b.data(),16,g);
                    unsigned edge_qp=p ? h264::df::chroma_qp(qp) : qp;
                    byte(filter_port,44,p!=0,g);byte(filter_port,48,alpha_offset,g);byte(filter_port,52,beta_offset,g);
                    byte(filter_port,32,bs,g);byte(filter_port,36,edge_qp,g);byte(filter_port,40,0x80,g);valid(filter_port,g);
                    checkpoint("df",g);
                    std::array<unsigned char,16> filtered_a{},filtered_b{};
                    io(filter_port,80,false,filtered_a.data(),16,g);io(filter_port,96,false,filtered_b.data(),16,g);
                    if(!split) {outa=filtered_a;outb=filtered_b;}
                    else for(unsigned k=0;k<4;++k) {outa[part*4+k]=filtered_a[part*4+k];outb[part*4+k]=filtered_b[part*4+k];}
                    unsigned char repeated_df=0x80;
                    require(raw(filter_port,40,true,&repeated_df,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"DF duplicate START accepted");
                    }
                    if(!filter_enabled) require(a==outa && b==outb,"DF bypass data");
                    for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) {frame[idx(i,j,false)]=outa[j*4+i];frame[idx(i,j,true)]=outb[j*4+i];}
                    ++filter_edges;
                }
            }
        }
        for(size_t i=0;i<frame.size();++i) changed_filter_samples+=frame[i]!=before[i];
        if(!filter_enabled) require(frame==before,"DF bypass frame");
    }
    uint32_t execute_picture(const h264::FrameConfig& c,uint64_t g,const h264::PictureTask& task) override {
        require(task.coding_index==task.source_slot && task.source_slot<frame_count,"fixture schedule");
        require(task.type==(full && task.source_slot?h264::PictureType::P:h264::PictureType::I),"picture type schedule");
        return execute(c,g,task.source_slot);
    }
    virtual uint32_t execute(const h264::FrameConfig& c,uint64_t g,unsigned frame) override {
        current_frame=frame;
        const unsigned previous=words,bytes=size*size*3/2;
        enable.write(false);wait(SC_ZERO_TIME);wait(SC_ZERO_TIME);
        enable.write(true);wait(SC_ZERO_TIME);wait(SC_ZERO_TIME);
        for(unsigned p=0;p<3;++p) {unsigned n=p?size/2:size;nonzero[p].assign(n*n/16,false);}
        cmb.update_enable(false);cmb.configure({size,size},c.cmb+frame*bytes,h264::CmbFrameMode::WorkingSet,bytes);
        cmb.update_enable(true);
        std::vector<unsigned char> reconstructed(bytes),source(bytes);
        unsigned qp=c.sequence.qp+c.slice_qp_delta;
        for(mb_y=0;mb_y<size/16;++mb_y) for(mb_x=0;mb_x<size/16;++mb_x) {
            checkpoint("source",g);
            cmb.fetch_macroblock(mb_x,mb_y);pump(g);
            require(cmb.done() && !cmb.failed() && domain.valid(g),"CMB source response");
            auto pixels=cmb.pixels();place(source,pixels,mb_x,mb_y);h264::MacroblockPixels result{};
            if(full && frame==0) intra_path(pixels,result,qp,g);
            else {
                h264::MacroblockPixels pred{};
                if(full) pred=inter_predict(c,pixels,g);
                else {pred.y.fill(128);pred.u.fill(128);pred.v.fill(128);}
                blocks_path(pixels,pred,result,qp,g);
            }
            place(reconstructed,result,mb_x,mb_y);
        }
        filter_frame(reconstructed,qp,g);
        df.latch_df_enable(filter_enabled);df.on_sofm();df.set_ref_slot((frame+1)%3);
        for(unsigned y=0;y<size/16;++y) for(unsigned x=0;x<size/16;++x)
            df.schedule_macroblock(x,y,tile(reconstructed,x,y),true);
        df.dma_fmdone();df.consume_done();pump(g);
        require(df.frame_complete() && nal.final_b_accepted(),"output/reference drain");
        source_seen.push_back(source);expected_reference.push_back(reconstructed);
        checkpoint("reference_done",g);
        words=nal.stm_len();return words-previous;
    }
    uint32_t end_activation(const h264::FrameConfig&,uint64_t g,uint32_t) override {
        checkpoint("eos",g);
        require(nal.byte_count()+4<=output_capacity,"EOS allocation");
        nal.accept_eos();nal_words_accepted(g,1);nal.flush_chunk();pump(g);
        require(nal.final_b_accepted() && domain.valid(g),"final EOS response");
        expected_output.insert(expected_output.end(),{0,0,1,0x0b});
        words=nal.stm_len();return words;
    }
};
}
