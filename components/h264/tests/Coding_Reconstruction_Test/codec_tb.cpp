#include "slice_encoder.h"
#include "cavlc_tables.h"
#include "ftq.h"
#include "itq.h"
#include "df_filter.h"
#include <fstream>
#include <random>
#include <iostream>
#include <limits>
using namespace h264::ec;
static void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
struct Reader {
    const std::vector<std::uint8_t>& data;unsigned at=0,limit;
    unsigned bit(){check(at<limit,"truncated code");unsigned b=(data[at/8]>>(7-at%8))&1;++at;return b;}
    unsigned read(unsigned n){unsigned v=0;while(n--)v=2*v+bit();return v;}
    unsigned ue(){unsigned z=0;while(!bit()){++z;check(z<31,"UE limit");}return ((1u<<z)-1)+read(z);}
    int se(){unsigned n=ue();return n&1?int((n+1)/2):-int(n/2);}
    unsigned vlc(const std::uint8_t* len,const std::uint8_t* codes,unsigned size){unsigned value=0;for(unsigned n=1;n<=16;++n){value=value*2+bit();for(unsigned i=0;i<size;++i)if(len[i]==n && codes[i]==value)return i;}throw std::runtime_error("invalid VLC");}
};
static Coefficients decode(Reader& r,unsigned max,int nc) {
    unsigned table=nc<2?0:nc<4?1:nc<8?2:3;
    unsigned tok=max==4?r.vlc(vlc::chroma_dc_coeff_token_len,vlc::chroma_dc_coeff_token_bits,20):r.vlc(vlc::coeff_token_len[table],vlc::coeff_token_bits[table],68);
    unsigned count=tok/4,trailing=tok%4;check(count<=max && trailing<=count,"token bounds");Coefficients out{};if(!count)return out;
    std::vector<int> levels(count);for(unsigned i=0;i<trailing;++i)levels[i]=r.bit()?-1:1;
    unsigned suffix=count>10 && trailing<3?1:0;
    for(unsigned i=trailing;i<count;++i){unsigned prefix=0;while(!r.bit()){++prefix;check(prefix<32,"level prefix");}
        unsigned size=prefix==14 && !suffix?4:prefix>=15?prefix-3:suffix;
        unsigned code=(std::min(prefix,15u)<<suffix)+r.read(size);
        if(prefix>=15 && !suffix)code+=15;if(prefix>=16)code+=(1u<<(prefix-3))-4096;
        if(i==trailing && trailing<3)code+=2;
        levels[i]=(code&1)?-int((code+1)/2):int((code+2)/2);
        if(!suffix)suffix=1;if(suffix<6 && unsigned(std::abs(levels[i]))>(3u<<(suffix-1)))++suffix;
    }
    unsigned zeros=count==max?0:max==4?r.vlc(vlc::chroma_dc_total_zeros_len[count-1],vlc::chroma_dc_total_zeros_bits[count-1],5-count):r.vlc(vlc::total_zeros_len[count-1],vlc::total_zeros_bits[count-1],17-count);
    int pos=int(count+zeros)-1;out[pos]=levels[0];
    for(unsigned i=1;i<count;++i){unsigned run=zeros?r.vlc(vlc::run_len[std::min(zeros,7u)-1],vlc::run_bits[std::min(zeros,7u)-1],zeros+1):0;check(run<=zeros,"run bounds");zeros-=run;pos-=run+1;check(pos>=0,"coefficient position");out[pos]=levels[i];}
    return out;
}
static void cavlc_tests(){std::mt19937 gen(264);unsigned cases=0;
    for(unsigned max:{4u,15u,16u})for(int nc:(max==4?std::vector<int>{-1}:std::vector<int>{0,1,2,3,4,7,8,16}))for(unsigned trial=0;trial<300;++trial){
        Coefficients scan{};for(unsigned i=0;i<max;++i){if(trial%5==0)scan[i]=int(gen()%3)-1;else if(gen()%3)scan[i]=trial%7==0?static_cast<std::int16_t>(gen()):int(gen()%201)-100;}
        if(trial==0)scan={};if(trial==1){scan={};scan[0]=1;}if(trial==2){scan={};scan[max-1]=-1;}
        std::vector<std::uint8_t> data(128);std::uint32_t bytes=0;BitWriter bw(data,bytes);auto total=Cavlc::encode_block(scan,max,nc,bw);
        Reader reader{data,0,unsigned(bw.bits())};check(decode(reader,max,nc)==scan && reader.at==reader.limit,"CAVLC roundtrip");check(total==unsigned(std::count_if(scan.begin(),scan.begin()+max,[](auto v){return v!=0;})),"TotalCoeff");++cases;
    }
    std::vector<std::uint8_t> data(128);std::uint32_t bytes=0;BitWriter bw(data,bytes);Coefficients one{};one[0]=1;Cavlc::encode_block(one,16,0,bw);check(bw.bits()==4 && data[0]==0x50,"known +1 CAVLC = 0101");
    auto before=data;auto bits=bw.bits();bool rejected=false;try{Cavlc::encode_block(one,4,0,bw);}catch(const std::invalid_argument&){rejected=true;}check(rejected && data==before && bw.bits()==bits,"invalid context atomic");
    std::vector<std::uint8_t> tiny(1);BitWriter small(tiny,bytes);small.write_bits(0x7f,7);rejected=false;try{Cavlc::encode_block(one,16,0,small);}catch(const std::overflow_error&){rejected=true;}check(rejected && small.bits()==7 && tiny[0]==0xfe,"capacity atomic");
    std::cout<<cases<<" CAVLC vectors passed\n";
}
static void syntax_tests(){SequenceSyntax seq;seq.width=32;seq.height=32;std::vector<std::uint8_t> data(128);std::uint32_t n=0;BitWriter b(data,n);SyntaxBuilder::build_sps(seq,b);Reader r{data,0,unsigned(b.bits())};
    check(r.read(8)==66 && r.read(8)==0 && r.read(8)==40,"SPS profile");check(r.ue()==0 && r.ue()==0 && r.ue()==0 && r.ue()==0 && r.ue()==1 && r.bit()==0 && r.ue()==1 && r.ue()==1,"SPS geometry/POC");check(r.read(4)==12 && r.at==r.limit,"SPS frame flags");
    BitWriter p(data,n);SyntaxBuilder::build_pps(seq,p);Reader q{data,0,unsigned(p.bits())};check(q.ue()==0 && q.ue()==0 && q.read(2)==0 && q.ue()==0 && q.ue()==0 && q.ue()==0 && q.read(3)==0 && q.se()==0 && q.se()==0 && q.se()==0 && q.read(3)==4,"PPS CAVLC flags");
    auto sets=SliceEncoder::parameter_sets(seq);check(sets.size()>10,"parameter sets");std::vector<CodedMacroblock> mbs(4);auto frame=SliceEncoder::picture(seq,{},mbs);check(frame.size()>6,"zero picture");
    check(SliceEncoder::picture(seq,{},mbs)==frame,"fresh slice contexts deterministic");bool rejected=false;try{SliceEncoder::picture(seq,{},mbs,16);}catch(const std::exception&){rejected=true;}check(rejected,"bounded slice rejection");
    seq.width=17;rejected=false;try{SliceEncoder::parameter_sets(seq);}catch(const std::exception&){rejected=true;}check(rejected,"unsupported geometry");
}
static void write(std::ofstream& f,const std::vector<std::uint8_t>& v){f.write(reinterpret_cast<const char*>(v.data()),v.size());check(bool(f),"output file");}
static void generate(const std::string& prefix,unsigned qp){
    using namespace h264::tq;TransposeRam ram("ram");Ftq ftq(ram);Itq itq;
    SequenceSyntax seq;seq.width=32;seq.height=32;seq.qp=qp;
    std::ofstream out(prefix+".h264",std::ios::binary),gold(prefix+".yuv",std::ios::binary);write(out,SliceEncoder::parameter_sets(seq));
    std::vector<std::uint8_t> previous(1536,128);
    for(unsigned frame=0;frame<4;++frame){std::vector<std::uint8_t> recon(1536,128);std::vector<CodedMacroblock> mbs(4);
        for(unsigned address=0;address<4;++address){auto& mb=mbs[address];unsigned mx=address%2,my=address/2;mb.intra=frame==0;
            for(unsigned i=0;i<16;++i){unsigned x=mx*16+(((i>>2)&1)*2+(i&1))*4,y=my*16+((i>>3)*2+((i>>1)&1))*4;
                unsigned mode=y && i%3==0?0:x && i%3==1?1:2;mb.intra_modes[i]=mode;
                std::array<std::uint8_t,16> pred{};std::array<std::int16_t,16> residual{};
                unsigned sum=0;if(y)for(unsigned k=0;k<4;++k)sum+=recon[(y-1)*32+x+k];if(x)for(unsigned k=0;k<4;++k)sum+=recon[(y+k)*32+x-1];
                unsigned dc=x && y?(sum+4)/8:x || y?(sum+2)/4:128;
                for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k){unsigned pos=(y+j)*32+x+k;pred[j*4+k]=frame?previous[pos]:mode==0?recon[(y-1)*32+x+k]:mode==1?recon[(y+j)*32+x-1]:dc;
                    unsigned source=(x+k)*3+(y+j)*5+frame*7+(((x+k)*(y+j))%17);source=32+source%192;residual[j*4+k]=int(source)-pred[j*4+k];}
                auto coeff=ftq.transform(residual,BlockClass::Luma4x4);mb.luma[i]=ftq.quantize(coeff,qp,BlockClass::Luma4x4,!frame);auto decoded=itq.reconstruct(itq.inverse_transform(mb.luma[i],qp),pred);
                for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)recon[(y+j)*32+x+k]=decoded[j*4+k];
            }
            for(unsigned plane=0;plane<2;++plane){mb.chroma_dc[plane]={static_cast<std::int16_t>(plane?-4:4),0,0,0};auto dc=itq.inverse_chroma_dc(mb.chroma_dc[plane],h264::df::chroma_qp(qp));
                // Include nonzero chroma AC to exercise maxNumCoeff=15 and nC.
                for(unsigned i=0;i<4;++i){mb.chroma_ac[plane][i][1]=(i&1)?-1:1;unsigned x=mx*8+(i%2)*4,y=my*8+(i/2)*4,base=1024+plane*256;std::array<std::uint8_t,16> pred{};
                    // Chroma DC prediction is evaluated once for the 8x8 macroblock.
                    unsigned top0=0,top1=0,left0=0,left1=0;
                    if(my)for(unsigned k=0;k<8;++k)(k<4?top0:top1)+=recon[base+(my*8-1)*16+mx*8+k];
                    if(mx)for(unsigned k=0;k<8;++k)(k<4?left0:left1)+=recon[base+(my*8+k)*16+mx*8-1];
                    unsigned value=128;
                    if(mx && my){if(i==0)value=(top0+left0+4)/8;else if(i==1)value=(top1+2)/4;else if(i==2)value=(left1+2)/4;else value=(top1+left1+4)/8;}
                    else if(my)value=((i%2?top1:top0)+2)/4;else if(mx)value=((i/2?left1:left0)+2)/4;
                    for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)pred[j*4+k]=frame?previous[base+(y+j)*16+x+k]:value;
                    auto samples=itq.reconstruct(itq.inverse_transform(mb.chroma_ac[plane][i],h264::df::chroma_qp(qp),dc[i]),pred);
                    for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)recon[base+(y+j)*16+x+k]=samples[j*4+k];
                }
            }
        }
        SliceSyntax pic;pic.idr=frame==0;pic.p_slice=frame!=0;pic.frame_num=frame;pic.poc=2*frame;write(out,SliceEncoder::picture(seq,pic,mbs));write(gold,recon);previous=recon;
    }
    write(out,{0,0,1,0x0b});
}
int sc_main(int argc,char** argv){try{std::string mode=argc>1?argv[1]:"cavlc";if(mode=="cavlc")cavlc_tests();else if(mode=="syntax")syntax_tests();else if(mode=="generate" && argc==4)generate(argv[2],std::stoul(argv[3]));else throw std::invalid_argument("arguments");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
