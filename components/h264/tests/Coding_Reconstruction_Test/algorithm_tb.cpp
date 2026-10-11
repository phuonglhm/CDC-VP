#include "ftq.h"
#include "itq.h"
#include "df_filter.h"
#include "df_top.h"
#include "exp_golomb.h"
#include "nal_formatter.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <string>

static void check(bool b,const char* what) {if(!b) throw std::runtime_error(what);}
template<class F> void rejects(F fn) {bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught,"invalid operation accepted");}
using Pixels=std::array<std::uint8_t,16>;
static Pixels flat(unsigned v) {Pixels p{};p.fill(v);return p;}

static void tq_test() {
    using namespace h264::tq;
    TransposeRam ram("ram"); Ftq ftq(ram); Itq itq;
    // Direct matrix multiplication oracle, independent of butterfly/transpose code.
    const int matrix[4][4]={{1,1,1,1},{2,1,-1,-2},{1,-1,-1,1},{1,-2,2,-1}};
    std::mt19937 gen(0x264);
    for(unsigned test=0;test<128;++test) {
        std::array<std::int16_t,16> input{};
        for(auto& v:input) v=int(gen()%511)-255;
        auto actual=ftq.transform(input,BlockClass::Luma4x4);
        for(int y=0;y<4;++y) for(int x=0;x<4;++x) {
            int expected=0;
            for(int j=0;j<4;++j) for(int i=0;i<4;++i) expected+=matrix[y][j]*input[j*4+i]*matrix[x][i];
            check(actual[y*4+x]==expected,"forward transform matrix oracle");
        }
    }
    // Constant gathered DC: H*[16,16;16,16]*H gives [64,0,0,0].
    auto chroma=ftq.quantize_chroma_dc({16,16,16,16},0,true);
    check(chroma==std::array<std::int16_t,4>{13,0,0,0},"forward chroma Hadamard and DC quantization");
    auto inverse=itq.inverse_chroma_dc(chroma,0);
    check(inverse==std::array<std::int32_t,4>{65,65,65,65},"restore four chroma DC blocks");
    rejects([&]{ftq.quantize_chroma_dc({16,16,16,16},52,true);});
    // Analytic DC-only inverse: every output must equal round(level*V*2^q/64).
    const int scales[6]={10,11,13,14,16,18};
    for(unsigned qp=0;qp<52;++qp) for(int value:{-100,-1,0,1,100}) {
        std::array<std::int16_t,16> levels{};levels[0]=value;
        auto actual=itq.inverse_transform(levels,qp);
        int num=value*scales[qp%6]*(1<<(qp/6))+32;
        int expected=num>=0 ? num/64 : -((-num+63)/64);
        for(auto x:actual) check(x==expected,"inverse DC analytical oracle");
    }
    std::array<std::int32_t,16> c{};c[0]=100;c[5]=-100;c[1]=100;
    auto q=ftq.quantize(c,0,BlockClass::Luma4x4);
    check(q[0]==40 && q[5]==-16 && q[1]==24,"quantizer coefficient classes");
    c.fill(0);c[0]=2;
    check(ftq.quantize(c,0,BlockClass::Luma4x4,true)[0]==1 && ftq.quantize(c,0,BlockClass::Luma4x4,false)[0]==0,"intra/inter rounding");
    c[0]=std::numeric_limits<std::int32_t>::min();
    rejects([&]{ftq.quantize(c,0,BlockClass::Luma4x4);});
    rejects([&]{ftq.quantize(c,52,BlockClass::Luma4x4);});
    std::array<std::int16_t,16> residual{};residual.fill(-255);
    check(itq.reconstruct(residual,flat(128))==flat(0),"negative reconstruction clip");
    residual.fill(255);check(itq.reconstruct(residual,flat(128))==flat(255),"positive reconstruction clip");
    rejects([&]{itq.inverse_transform(residual,52);});
    residual.fill(32767);rejects([&]{itq.inverse_transform(residual,51);});
}

static void df_test(h264::df::DfFilter& filter) {
    using namespace h264::df;
    EdgeSide a{},b{};
    check(boundary_strength(a,b,false)==0,"matching P edge bS=0");
    b.mv_x=3;check(boundary_strength(a,b,false)==0,"subpixel MV difference");
    b.mv_x=4;check(boundary_strength(a,b,false)==1,"one-pixel MV threshold");
    b.mv_x=0;b.mv_y=-4;check(boundary_strength(a,b,false)==1,"negative vertical MV threshold");
    b.mv_y=0;b.reference=1;check(boundary_strength(a,b,false)==1,"different reference");
    a.nonzero=true;check(boundary_strength(a,b,false)==2,"residual has priority");
    b.intra=true;check(boundary_strength(a,b,false)==3 && boundary_strength(a,b,true)==4,"intra and external edge priority");
    for(unsigned qp=0;qp<30;++qp) check(chroma_qp(qp)==qp,"low chroma QP identity");
    check(chroma_qp(30)==29 && chroma_qp(34)==32 && chroma_qp(51)==39,"nonlinear chroma QP");
    rejects([&]{chroma_qp(52);});
    auto p=flat(100),q=flat(120);
    filter.apply_filter(p,q,1,36,true);
    // indexA=36: tc0(bS=1)=2, chroma tc=3. Only p0/q0 change.
    for(int i=0;i<16;++i) {check(p[i]==(i%4==3?103:100),"chroma weak p");check(q[i]==(i%4==0?117:120),"chroma weak q");}
    p=flat(100);q=flat(110);filter.apply_filter(p,q,4,36,true);
    for(int i=0;i<16;++i) {check(p[i]==(i%4==3?103:100),"chroma strong p");check(q[i]==(i%4==0?108:110),"chroma strong q");}
    // Strong luma also modifies p1/p2 and q1/q2, unlike chroma.
    p=flat(100);q=flat(110);filter.apply_filter(p,q,4,36);
    check(p[3]==104 && p[2]==103 && p[1]==101 && q[0]==106 && q[1]==108 && q[2]==109,"luma strong golden");
    p=flat(100);q=flat(104);filter.apply_filter(p,q,3,10,true);
    check(p==flat(100) && q==flat(104),"low QP no filtering");
    filter.apply_filter(p,q,3,10,true,15,15);
    check(p[3]==102 && q[0]==102,"positive offset activates thresholds");
    p=flat(100);q=flat(104);filter.apply_filter(p,q,3,26,true,-16,-16);
    check(p==flat(100) && q==flat(104),"negative offset disables thresholds");
    // Large correction exposes wrong tc0 rows, including the final row.
    p=flat(80);q=flat(160);filter.apply_filter(p,q,3,51,true);
    check(p[3]==106 && q[0]==134,"tc0 final row is 25, chroma tc 26");
    p=flat(100);q=flat(110);auto oldp=p,oldq=q;
    rejects([&]{filter.apply_filter(p,q,5,26);});
    rejects([&]{filter.apply_filter(p,q,2,52);});
    rejects([&]{filter.apply_filter(p,q,2,26,true,16);});
    check(p==oldp && q==oldq,"invalid filter must not mutate");
}

static std::uint64_t read_ue(const std::array<std::uint8_t,128>& data,unsigned bits) {
    unsigned position=0;
    auto bit=[&](){check(position<bits,"truncated Exp-Golomb");auto b=(data[position/8]>>(7-position%8))&1;++position;return b;};
    unsigned zeros=0;while(!bit()) ++zeros;
    std::uint64_t code=1;for(unsigned i=0;i<zeros;++i) code=2*code+bit();
    check(position==bits,"unexpected suffix bits");return code-1;
}
static void ec_test() {
    using namespace h264::ec;
    std::array<std::uint8_t,128> data{};std::uint32_t bytes=0;
    for(int value=-32768;value<=32767;++value) {
        BitWriter w(data,bytes);ExpGolomb::encode_se(value,w);
        auto n=read_ue(data,bytes*8+w.bit_offset);
        auto decoded=(n&1)? std::int64_t((n+1)/2):-std::int64_t(n/2);
        check(decoded==value,"signed Exp-Golomb roundtrip");
    }
    for(std::uint32_t v:{0u,1u,2u,255u,65535u,0xffffffffu}) {
        BitWriter w(data,bytes);ExpGolomb::encode_ue(v,w);
        check(read_ue(data,bytes*8+w.bit_offset)==v,"unsigned Exp-Golomb roundtrip");
    }
    {BitWriter w(data,bytes);ExpGolomb::encode_ue(0,w);ExpGolomb::encode_ue(1,w);ExpGolomb::encode_ue(2,w);w.rbsp_trailing_bits();check(bytes==1 && data[0]==0xa7,"known ue concatenation and stop bit");}
    for(int offset=0;offset<8;++offset) {
        BitWriter w(data,bytes);w.write_bits(0,offset);w.rbsp_trailing_bits();
        check(bytes==1 && data[0]==(0x80>>offset),"RBSP termination at every bit offset");
    }
    {BitWriter w(data,bytes);w.write_bits(0xab,8);w.rbsp_trailing_bits();check(bytes==2 && data[0]==0xab && data[1]==0x80,"aligned RBSP needs stop byte");}
    {BitWriter w(data,bytes);for(int i=0;i<127;++i) w.write_bits(0x55,8);w.write_bits(1,7);auto before=data;auto count=bytes;auto offset=w.bit_offset;
     rejects([&]{w.write_bits(3,2);});check(data==before && bytes==count && w.bit_offset==offset,"reservoir rejection is atomic");
     rejects([&]{w.write_bits(0,33);});w.rbsp_trailing_bits();check(bytes==128,"last available stop bit");}
    EcResult result{};const std::uint8_t input[]={0,0,0,0,1,0,0,2,0,0,3,0,0,4,0x80};
    std::copy(std::begin(input),std::end(input),result.nal_stream.begin());result.stream_length=sizeof(input);
    NalFormatter::wrap_nal_unit(result,3,5);
    const std::uint8_t expected[]={0,0,0,1,0x65,0,0,3,0,0,3,1,0,0,3,2,0,0,3,3,0,0,4,0x80};
    check(result.stream_length==sizeof(expected) && std::equal(std::begin(expected),std::end(expected),result.nal_stream.begin()),"NAL escaping exact bytes");
    auto snapshot=result;rejects([&]{NalFormatter::wrap_nal_unit(result,4,5);});check(result.nal_stream==snapshot.nal_stream,"bad header preserves buffer");
    result={};result.stream_length=128;auto old=result;
    rejects([&]{NalFormatter::wrap_nal_unit(result,3,5);});check(result.nal_stream==old.nal_stream && result.stream_length==128,"NAL overflow preserves buffer");
}

struct ProtocolBench:sc_core::sc_module {
    tlm_utils::simple_initiator_socket<ProtocolBench> socket{"socket"};h264::df::DfTop df{"df"};bool passed=false;
    SC_CTOR(ProtocolBench) {socket.bind(df.socket);SC_THREAD(run);}
    tlm::tlm_response_status io(unsigned a,bool wr,unsigned char* d,unsigned n) {
        tlm::tlm_generic_payload t;t.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);t.set_address(a);t.set_data_ptr(d);t.set_data_length(n);t.set_streaming_width(n);
        sc_core::sc_time delay=sc_core::SC_ZERO_TIME;socket->b_transport(t,delay);wait(delay);return t.get_response_status();
    }
    void byte(unsigned a,unsigned char v,bool ok=true){check((io(a,true,&v,1)==tlm::TLM_OK_RESPONSE)==ok,"DF configuration protocol");}
    void run() {
        try {
            auto p=flat(100),q=flat(104);check(io(0,true,p.data(),16)==tlm::TLM_OK_RESPONSE,"load p");check(io(16,true,q.data(),16)==tlm::TLM_OK_RESPONSE,"load q");
            byte(32,3);byte(36,10);byte(44,1);byte(48,15);byte(52,15);byte(48,16,false);byte(44,2,false);byte(40,128);
            Pixels output{};check(io(80,false,output.data(),16)==tlm::TLM_OK_RESPONSE && output[3]==102,"offset/chroma through TLM");
            df.reset();byte(40,128,false);
            io(0,true,p.data(),16);io(16,true,q.data(),16);byte(32,3);byte(36,10);byte(40,128);
            io(80,false,output.data(),16);check(output==p,"reset clears offsets/chroma");passed=true;
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';}sc_core::sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    try {std::string scenario=argc>1?argv[1]:"tq";
        if(scenario=="tq") tq_test();
        else if(scenario=="df") {h264::df::DfFilter f("filter");df_test(f);}
        else if(scenario=="ec") ec_test();
        else if(scenario=="protocol") {ProtocolBench b("bench");sc_core::sc_start();check(b.passed,"protocol failed");}
        else throw std::invalid_argument("scenario");
        std::cout<<"PASS algorithm "<<scenario<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
