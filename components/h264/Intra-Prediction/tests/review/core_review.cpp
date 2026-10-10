#include <h264/intra/intra_core.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include "../prediction_oracle.h"
using namespace h264::intra;
namespace {
unsigned checks=0;
void require(bool ok,const char* why) {
    ++checks; if (!ok) throw std::runtime_error(why);
}
template<class F> void rejects(F action,const char* why) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    require(rejected,why);
}
// Independent separable matrix transform; does not call production distortion.
uint64_t cost(const std::vector<int16_t>& r,unsigned n,Metric metric) {
    uint64_t total=0;
    if (metric==Metric::Sad) {
        for (int v:r) total+=std::abs(v);
        return total;
    }
    const int h[4][4]={{1,1,1,1},{1,1,-1,-1},{1,-1,-1,1},{1,-1,1,-1}};
    for (unsigned y=0;y<n;y+=4) for (unsigned x=0;x<n;x+=4) {
        uint64_t sum=0;
        for (unsigned a=0;a<4;++a) for (unsigned b=0;b<4;++b) {
            int v=0;
            for (unsigned i=0;i<4;++i) for (unsigned j=0;j<4;++j)
                v+=h[a][i]*r[(y+i)*n+x+j]*h[b][j];
            sum+=std::abs(v);
        }
        total+=(sum+1)/2;
    }
    return total;
}
void availability() {
    // All boolean combinations, including upper-right absent/present.
    for (unsigned mask=0;mask<16;++mask) {
        References r; r.has_top=mask&1; r.has_left=mask&2;
        r.has_upper_left=mask&4; r.has_upper_right=mask&8;
        const bool t=r.has_top,l=r.has_left,b=t&&l&&r.has_upper_left;
        std::vector<unsigned> expected;
        for (unsigned m=0;m<9;++m)
            if (m==2 || ((m==0||m==3||m==7)&&t) || ((m==1||m==8)&&l)
                || ((m==4||m==5||m==6)&&b)) expected.push_back(m);
        require(legal_modes(Kind::Luma4x4,r)==expected,"complete luma4 legal set");
        expected.clear();
        if(t) expected.push_back(0); if(l) expected.push_back(1);
        expected.push_back(2); if(b) expected.push_back(3);
        require(legal_modes(Kind::Luma16x16,r)==expected,"complete luma16 legal set");
        expected={0}; if(l) expected.push_back(1); if(t) expected.push_back(2);
        if(b) expected.push_back(3);
        require(legal_modes(Kind::Chroma8x8,r)==expected,"complete chroma legal set");
    }
    Core c; c.start_frame(48,48);
    for (unsigned y=0;y<48;y+=4) for(unsigned x=0;x<48;x+=4)
        c.import_reconstructed({Plane::Y,Kind::Luma4x4,x,y},std::vector<uint8_t>(16,uint8_t(x+y)));
    // Scan positions listed in H.264 order, independently of resolver expression.
    const unsigned xs[]={0,4,0,4,8,12,8,12,0,4,0,4,8,12,8,12};
    const unsigned ys[]={0,0,4,4,0,0,4,4,8,8,12,12,8,8,12,12};
    for (unsigned scan=0;scan<16;++scan) {
        const auto r=c.resolve({Plane::Y,Kind::Luma4x4,16+xs[scan],16+ys[scan]});
        const bool expected=scan!=3 && scan!=7 && scan!=11 && scan!=13 && scan!=15;
        require(r.has_upper_right==expected,"all 16 scan positions with future imports");
        if(!expected) for(unsigned i=4;i<8;++i)
            require(r.top[i]==r.top[3],"all unavailable upper-right samples replicate D");
    }
    for(unsigned y=0;y<48;y+=4) {
        const auto right=c.resolve({Plane::Y,Kind::Luma4x4,44,y});
        require(!right.has_upper_right,"entire right picture edge");
    }
}
void numerical() {
    std::mt19937 rng(20261010);
    for (unsigned n:{4u,8u,16u}) for(unsigned iteration=0;iteration<128;++iteration) {
        std::vector<int16_t> r(n*n);
        for(auto& v:r) v=int(rng()%511)-255;
        for(Metric metric:{Metric::Sad,Metric::Satd})
            require(distortion(r,n,metric)==cost(r,n,metric),"random residual matrix cost oracle");
    }
    // Whole 48x32 picture in MB raster/H.264 block order; no reference import.
    for(Metric metric:{Metric::Sad,Metric::Satd}) for(bool luma16:{false,true}) {
        Core c; c.start_frame(48,32);
        const unsigned xs[]={0,4,0,4,8,12,8,12,0,4,0,4,8,12,8,12};
        const unsigned ys[]={0,0,4,4,0,0,4,4,8,8,12,12,8,8,12,12};
        for(unsigned my=0;my<2;++my) for(unsigned mx=0;mx<3;++mx)
            for(Plane p:{Plane::Y,Plane::U,Plane::V}) {
                const unsigned count=p==Plane::Y&&!luma16 ? 16 : 1;
                for(unsigned i=0;i<count;++i) {
                    Block block{p,p==Plane::Y ? (luma16?Kind::Luma16x16:Kind::Luma4x4):Kind::Chroma8x8,
                        p==Plane::Y?mx*16+(luma16?0:xs[i]):mx*8,
                        p==Plane::Y?my*16+(luma16?0:ys[i]):my*8};
                    const unsigned n=block.size(); std::vector<uint8_t> source(n*n),decoded(n*n);
                    for(auto& v:source) v=uint8_t(rng());
                    for(auto& v:decoded) v=uint8_t(rng());
                    std::array<uint32_t,9> penalties{};
                    for(auto& v:penalties) v=rng()%4096;
                    const auto d=c.evaluate(block,source,metric,penalties);
                    require(d.candidates.size()==legal_modes(block.kind,d.references).size(),"all candidates recorded");
                    uint64_t best=UINT64_MAX; unsigned winner=99;
                    for(const auto& candidate:d.candidates) {
                        const auto pred=intra_test::prediction(block.kind,candidate.mode,d.references);
                        std::vector<int16_t> residual(source.size());
                        for(size_t j=0;j<source.size();++j) residual[j]=int(source[j])-pred[j];
                        const auto distortion=cost(residual,n,metric);
                        require(candidate.distortion==distortion && candidate.cost==distortion+penalties[candidate.mode],"candidate cost and penalty");
                        if(candidate.cost<best) { best=candidate.cost; winner=candidate.mode; }
                    }
                    require(d.mode==winner && d.cost==best,"global minimum winner");
                    for(size_t j=0;j<source.size();++j)
                        require(d.residual[j]==int(source[j])-d.predictor[j],"signed residual ownership");
                    require(c.replay(d.token).predictor==d.predictor,"full picture stable replay");
                    c.reconstruct(d.token,decoded);
                    require(c.committed_mode(block)==d.mode,"full picture mode commit");
                }
            }
    }
}
void predictor_golden() {
    std::mt19937 rng(20261010);
    for(unsigned iteration=0;iteration<512;++iteration) {
        References r;
        for(auto& v:r.top) v=uint8_t(rng());
        for(auto& v:r.left) v=uint8_t(rng());
        r.upper_left=uint8_t(rng());
        if(iteration<4) {
            for(unsigned i=0;i<16;++i) {
                r.top[i]=iteration==0 ? 0:iteration==1 ? 255:(i%2 ? 255:0);
                r.left[i]=iteration<2 ? r.top[i]:(i%2 ? 0:255);
            }
            r.upper_left=iteration%2 ? 255:0;
        }
        for(unsigned mask=0;mask<16;++mask) {
            r.has_top=mask&1; r.has_left=mask&2;
            r.has_upper_left=mask&4; r.has_upper_right=mask&8;
            for(Kind kind:{Kind::Luma4x4,Kind::Luma16x16,Kind::Chroma8x8})
                for(unsigned mode:legal_modes(kind,r))
                    require(predict(kind,mode,r)==intra_test::prediction(kind,mode,r),"independent random predictor golden");
        }
    }
}
void invalid_inputs() {
    Core c; c.start_frame(32,32);
    const auto d=c.evaluate(Block{},std::vector<uint8_t>(16,255));
    const auto generation=c.generation();
    for(const auto dims:{std::pair<unsigned,unsigned>{0,16},{16,0},{8,16},{16,17},{4112,16},{16,4112}}) {
        rejects([&]{c.start_frame(dims.first,dims.second);},"invalid frame dimensions rejected");
        require(c.generation()==generation && c.replay(d.token).predictor==d.predictor,"bad dimensions preserve pending frame");
    }
    for(const Block block:{Block{static_cast<Plane>(99),Kind::Luma4x4,0,0},
        Block{Plane::Y,static_cast<Kind>(99),0,0},Block{Plane::U,Kind::Luma16x16,0,0},
        Block{Plane::Y,Kind::Chroma8x8,0,0},Block{Plane::Y,Kind::Luma4x4,2,0},
        Block{Plane::Y,Kind::Luma4x4,0,2},Block{Plane::Y,Kind::Luma4x4,32,0},
        Block{Plane::Y,Kind::Luma4x4,0,32},Block{Plane::Y,Kind::Luma4x4,UINT32_MAX-3,0}})
        rejects([&]{c.resolve(block);},"invalid plane/kind/alignment/boundary rejected");
    for(unsigned length:{0u,15u,17u}) {
        rejects([&]{c.reconstruct(d.token,std::vector<uint8_t>(length));},"bad feedback sample count rejected");
        require(c.busy() && !c.committed_mode(Block{}),"bad feedback never commits or discards pending winner");
    }
    Token wrong=d.token; ++wrong.generation;
    rejects([&]{c.reconstruct(wrong,std::vector<uint8_t>(16));},"wrong generation feedback rejected");
    c.reconstruct(d.token,std::vector<uint8_t>(16,17));
    for(unsigned length:{0u,15u,17u}) {
        rejects([&]{c.import_reconstructed(Block{},std::vector<uint8_t>(length));},"bad import sample count rejected");
        rejects([&]{c.evaluate(Block{},std::vector<uint8_t>(length));},"bad source sample count rejected");
        require(c.committed_mode(Block{})==d.mode,"bad buffers preserve committed mode");
    }
    rejects([&]{c.evaluate(Block{},std::vector<uint8_t>(16),static_cast<Metric>(99));},"invalid core metric rejected");
    require(!c.busy() && c.committed_mode(Block{})==d.mode,"bad metric never creates partial winner");
    for(unsigned n:{0u,1u,3u,5u})
        rejects([&]{distortion(std::vector<int16_t>(n*n),n,Metric::Sad);},"invalid distortion dimensions rejected");
    rejects([&]{distortion(std::vector<int16_t>(15),4,Metric::Satd);},"invalid residual sample count rejected");
    rejects([&]{distortion(std::vector<int16_t>(16),4,static_cast<Metric>(99));},"invalid distortion metric rejected");
    rejects([&]{legal_modes(static_cast<Kind>(99),References{});},"invalid legal-mode block kind rejected");
    rejects([&]{predict(static_cast<Kind>(99),0,References{});},"invalid predictor block kind rejected");
    for(const Kind kind:{Kind::Luma4x4,Kind::Luma16x16,Kind::Chroma8x8}) {
        References none;
        const unsigned dc=kind==Kind::Chroma8x8?0:2;
        for(unsigned mode=0;mode<(kind==Kind::Luma4x4?10u:5u);++mode)
            if(mode!=dc) rejects([&]{predict(kind,mode,none);},"every unavailable mode rejected");
    }
    for(unsigned n:{4u,8u,16u}) {
        std::vector<int16_t> residual(n*n);
        for(size_t i=0;i<residual.size();++i) residual[i]=i%2?INT16_MIN:INT16_MAX;
        require(distortion(residual,n,Metric::Sad)==cost(residual,n,Metric::Sad),"signed16 SAD extremes");
        require(distortion(residual,n,Metric::Satd)==cost(residual,n,Metric::Satd),"signed16 SATD extremes");
    }
    c.start_frame(4096,16);
    require(!c.resolve({Plane::Y,Kind::Luma4x4,4092,12}).has_top,"maximum coded width valid");
    c.start_frame(16,4096);
    require(!c.resolve({Plane::Y,Kind::Luma4x4,12,4092}).has_left,"maximum coded height valid");
    c.start_frame(32,32);
    const Block large{Plane::Y,Kind::Luma16x16,0,0};
    const auto big=c.evaluate(large,std::vector<uint8_t>(256,128));
    c.reconstruct(big.token,std::vector<uint8_t>(256,128));
    require(c.committed_mode(large)==big.mode,"large committed mode available before partial overwrite");
    c.import_reconstructed({Plane::Y,Kind::Luma4x4,4,4},std::vector<uint8_t>(16,17));
    require(!c.committed_mode(large),"partial context overwrite invalidates incompatible large mode");
}
}
int main() {
    try { availability(); predictor_golden(); numerical(); invalid_inputs(); std::cout<<"PASS review core: "<<checks<<" checks, seed=20261010\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
