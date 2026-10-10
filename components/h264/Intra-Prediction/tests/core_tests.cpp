#include <h264/intra/intra_core.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace h264::intra;
namespace {
unsigned checks=0;
void require(bool ok, const char* what) {
    ++checks; if (!ok) throw std::runtime_error(what);
}
template<class F> void rejects(F f, const char* what) {
    bool rejected=false;
    try { f(); } catch (const std::exception&) { rejected=true; }
    require(rejected,what);
}
References edges() {
    References r;
    for (unsigned i=0;i<8;++i) r.top[i]=uint8_t(10+10*i);
    for (unsigned i=0;i<4;++i) r.left[i]=uint8_t(90+10*i);
    r.upper_left=5;
    r.has_top=r.has_left=r.has_upper_left=r.has_upper_right=true;
    return r;
}
void algorithms() {
    const std::array<std::array<uint8_t,16>,9> golden{{
        {{10,20,30,40, 10,20,30,40, 10,20,30,40, 10,20,30,40}},
        {{90,90,90,90, 100,100,100,100, 110,110,110,110, 120,120,120,120}},
        {{65,65,65,65, 65,65,65,65, 65,65,65,65, 65,65,65,65}},
        {{20,30,40,50, 30,40,50,60, 40,50,60,70, 50,60,70,78}},
        {{28,11,20,30, 71,28,11,20, 100,71,28,11, 110,100,71,28}},
        {{8,15,25,35, 28,11,20,30, 71,8,15,25, 100,28,11,20}},
        {{48,28,11,20, 95,71,48,28, 105,100,95,71, 115,110,105,100}},
        {{15,25,35,45, 20,30,40,50, 25,35,45,55, 30,40,50,60}},
        {{95,100,105,110, 105,110,115,118, 115,118,120,120, 120,120,120,120}}
    }};
    auto r=edges();
    for (unsigned mode=0;mode<9;++mode)
        require(predict(Kind::Luma4x4,mode,r)==std::vector<uint8_t>(golden[mode].begin(),golden[mode].end()),"4x4 mode golden");
    for (unsigned mask=0;mask<8;++mask) {
        r=edges(); r.has_top=mask&1; r.has_left=mask&2; r.has_upper_left=mask&4;
        const auto legal=legal_modes(Kind::Luma4x4,r);
        require(std::find(legal.begin(),legal.end(),2)!=legal.end(),"DC always legal");
        require((std::find(legal.begin(),legal.end(),4)!=legal.end())==(mask==7),"diagonal availability");
        const auto dc=predict(Kind::Luma4x4,2,r);
        const uint8_t expected=mask&1 ? (mask&2 ? 65 : 25) : (mask&2 ? 105 : 128);
        require(std::all_of(dc.begin(),dc.end(),[&](auto p){return p==expected;}),"four DC availability cases");
    }
    r=edges(); r.has_upper_right=false;
    const auto replicated=predict(Kind::Luma4x4,3,r);
    require(replicated[15]==40 && replicated[0]==20 && replicated[7]==40,"upper-right replication");
    r.has_top=false;
    rejects([&]{predict(Kind::Luma4x4,0,r);},"unavailable mode rejected");
    for (const Kind kind : {Kind::Luma16x16,Kind::Chroma8x8}) {
        const unsigned n=kind==Kind::Luma16x16 ? 16 : 8;
        for (const bool descending : {false,true}) {
            r=References{}; r.has_top=r.has_left=r.has_upper_left=true;
            r.upper_left=descending ? 202 : 98;
            for (unsigned i=0;i<n;++i) r.top[i]=r.left[i]=uint8_t(descending ? 200-2*i : 100+2*i);
            const auto plane=predict(kind,3,r);
            for (unsigned y=0;y<n;++y) for (unsigned x=0;x<n;++x)
                require(plane[y*n+x]==(descending ? 198-2*x-2*y : 102+2*x+2*y),"plane gradient and negative rounding");
        }
        r=References{}; r.has_top=r.has_left=r.has_upper_left=true;
        r.top.fill(0); r.left.fill(0); r.upper_left=255;
        const auto plane=predict(kind,3,r);
        require(plane.front()>0 && plane.back()==0,"plane clips negative values");
        r.top.fill(255); r.left.fill(255); r.upper_left=0;
        const auto high=predict(kind,3,r);
        require(high.back()==255,"plane clips high values");
        for (unsigned mask=0;mask<4;++mask) {
            r.has_top=mask&1; r.has_left=mask&2; r.has_upper_left=false;
            r.top.fill(20); r.left.fill(100);
            const unsigned dc_mode=kind==Kind::Chroma8x8 ? 0 : 2;
            const auto dc=predict(kind,dc_mode,r);
            const uint8_t expected=mask==3 ? 60 : mask==1 ? 20 : mask==2 ? 100 : 128;
            for (unsigned y=0;y<n;++y) for (unsigned x=0;x<n;++x) {
                const unsigned quadrant_value=kind==Kind::Chroma8x8 && mask==3 && x/4!=y/4 ? (y<4 ? 20 : 100) : expected;
                require(dc[y*n+x]==quadrant_value,"large-block DC availability");
            }
            require(legal_modes(kind,r).size()==(mask==3 ? 3u : mask ? 2u : 1u),"large-block legal modes");
        }
        r=References{}; r.has_top=r.has_left=true;
        for (unsigned i=0;i<n;++i) { r.top[i]=uint8_t(3*i); r.left[i]=uint8_t(100+i); }
        const auto vertical=predict(kind,kind==Kind::Chroma8x8 ? 2 : 0,r);
        const auto horizontal=predict(kind,1,r);
        for (unsigned y=0;y<n;++y) for (unsigned x=0;x<n;++x)
            require(vertical[y*n+x]==3*x && horizontal[y*n+x]==100+y,"large-block directional modes");
    }
    r=References{}; r.has_top=r.has_left=true;
    for (unsigned i=0;i<8;++i) { r.top[i]=i<4 ? 10 : 50; r.left[i]=i<4 ? 90 : 130; }
    auto chroma=predict(Kind::Chroma8x8,0,r);
    require(chroma[0]==50 && chroma[4]==50 && chroma[32]==130 && chroma[36]==90,"chroma quadrant DC");
    std::vector<int16_t> residual(16,1);
    require(distortion(residual,4,Metric::Sad)==16,"SAD sums every sample");
    require(distortion(residual,4,Metric::Satd)==8,"SATD DC golden");
    residual.assign(16,0); residual[0]=-1;
    require(distortion(residual,4,Metric::Satd)==8,"SATD impulse golden");
    residual.assign(256,255);
    require(distortion(residual,16,Metric::Sad)==65280,"wide cost accumulator");
    require(distortion(residual,16,Metric::Satd)==32640,"SATD tiled full candidate");
}
void context() {
    Core core;
    rejects([&]{core.start_frame(16,17);},"reject noncoded dimensions");
    core.start_frame(48,32);
    Block origin;
    const auto first=core.evaluate(origin,std::vector<uint8_t>(16,255));
    require(first.mode==2 && first.cost==2032,"origin DC128");
    require(!core.committed_mode(origin),"mode not committed early");
    require(core.replay(first.token).predictor==first.predictor,"preserved replay");
    rejects([&]{core.evaluate(origin,std::vector<uint8_t>(16));},"single outstanding block");
    core.reconstruct(first.token,std::vector<uint8_t>(16,17));
    require(core.committed_mode(origin)==2,"mode committed with reconstruction");
    require(!core.committed_mode({Plane::Y,Kind::Luma16x16,0,0}),"4x4 mode cannot masquerade as16x16 context");
    Block right{Plane::Y,Kind::Luma4x4,4,0};
    auto refs=core.resolve(right);
    require(refs.has_left && refs.left[0]==17 && !refs.has_top,"decoded neighbor, not source255");
    core.import_reconstructed(right,std::vector<uint8_t>(16,128));
    Block below{Plane::Y,Kind::Luma4x4,4,4};
    core.import_reconstructed({Plane::Y,Kind::Luma4x4,0,4},std::vector<uint8_t>(16,128));
    core.import_reconstructed(origin,std::vector<uint8_t>(16,128));
    const auto tie=core.evaluate(below,std::vector<uint8_t>(16,128));
    require(tie.mode==0 && tie.cost==0,"equal cost preserves first numeric mode");
    require(tie.candidates.size()==9,"all legal modes evaluated");
    core.reconstruct(tie.token,std::vector<uint8_t>(16,128));
    core.import_reconstructed({Plane::Y,Kind::Luma4x4,8,0},std::vector<uint8_t>(16,222));
    require(!core.resolve(below).has_upper_right && core.resolve(below).top[4]==128,"scan3 guard despite imported pixels");
    core.import_reconstructed({Plane::Y,Kind::Luma16x16,16,0},std::vector<uint8_t>(256,33));
    core.import_reconstructed({Plane::Y,Kind::Luma16x16,32,0},std::vector<uint8_t>(256,44));
    const auto full=core.resolve({Plane::Y,Kind::Luma4x4,28,16});
    require(full.has_upper_right && full.top[3]==33 && full.top[4]==44,"upper-right uses full picture width");
    const auto edge=core.resolve({Plane::Y,Kind::Luma4x4,44,16});
    require(edge.has_top && !edge.has_upper_right && edge.top[7]==44,"right picture edge replication");
    const auto saved=core.evaluate({Plane::Y,Kind::Luma4x4,44,16},std::vector<uint8_t>(16,44));
    rejects([&]{core.import_reconstructed(origin,std::vector<uint8_t>(16));},"cannot mutate stalled reference context");
    core.start_frame(48,32);
    rejects([&]{core.replay(saved.token);},"frame invalidates old replay");
    require(!core.resolve(below).has_top && !core.committed_mode(below),"frame clears neighbor and mode RAM");
    std::array<uint32_t,9> penalty; penalty.fill(std::numeric_limits<uint32_t>::max());
    const auto wide=core.evaluate(origin,std::vector<uint8_t>(16,0),Metric::Sad,penalty);
    require(wide.cost==uint64_t(std::numeric_limits<uint32_t>::max())+2048,"mode cost exceeds32 bits safely");
    core.reset();
    rejects([&]{core.resolve(origin);},"hard reset invalidates frame");
    core.start_frame(16,16);
    rejects([&]{core.resolve({Plane::Y,Kind::Luma4x4,2,0});},"alignment checked");
    rejects([&]{core.resolve({Plane::U,Kind::Luma4x4,0,0});},"plane class checked");
    rejects([&]{core.resolve({Plane::Y,Kind::Luma4x4,16,0});},"picture extent checked");
    core.start_frame(32,16);
    core.import_reconstructed({Plane::U,Kind::Chroma8x8,0,0},std::vector<uint8_t>(64,42));
    core.import_reconstructed({Plane::V,Kind::Chroma8x8,0,0},std::vector<uint8_t>(64,99));
    const auto u=core.resolve({Plane::U,Kind::Chroma8x8,8,0});
    const auto v=core.resolve({Plane::V,Kind::Chroma8x8,8,0});
    require(u.has_left && u.left[7]==42 && v.has_left && v.left[7]==99,"U/V neighbor storage isolated");
    require(!core.resolve({Plane::Y,Kind::Luma16x16,16,0}).has_left,"chroma cannot seed luma reference");
}
}
int main() {
    try { algorithms(); context(); std::cout<<"PASS intra algorithms/context: "<<checks<<" checks\n"; return 0; }
    catch (const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
