#include "test_support.h"
#include <limits>
using namespace h264::inter;

int main() {
    try {
        ReferenceCache cache; Reference ref{List::L0,{0,2}};
        unsigned cases=0;
        for (const auto dimensions : {std::pair<unsigned,unsigned>{16,16},{176,144},{1280,720},{1920,1088},{4096,4096}}) {
            const auto w=dimensions.first, h=dimensions.second;
            ++ref.tag.picture; cache.retag(ref.list,ref.tag,w,h);
            for (unsigned plane=0;plane<3;++plane) {
                const unsigned scale=plane?2:1;
                for (unsigned corner=0;corner<4;++corner) {
                    const unsigned x=(corner%2)?w/scale-1:0, y=(corner/2)?h/scale-1:0;
                    cache.fill(ref,Plane(plane),x,y,1,1,{uint8_t(70+corner*20+plane*7)});
                }
            }
            for (unsigned corner=0;corner<4;++corner) for (bool end_mb : {false,true}) {
                const int sx=corner%2?1:-1, sy=corner/2?1:-1;
                std::array<uint8_t,256> current{}; current.fill(uint8_t(70+corner*20));
                Request request; request.mb_x=end_mb?w-16:0; request.mb_y=end_mb?h-16:0;
                request.integer_candidates={{ref,{sx*65536,sy*65536},10}};
                request.fractional_candidates={{{-sx,-sy},0}};
                const auto decision=evaluate(cache,request,current);
                require(decision.mode.winner.cost==0 && decision.mode.winner.candidate.mv.x==sx*65535 &&
                        decision.mode.winner.candidate.mv.y==sy*65535,"max MV quarter refinement and edge extension");
                require(decision.predictor.size()==256 && decision.residual.size()==256,"max geometry predictor size");
                for (const auto& sample : sample_layout(decision.mode)) {
                    require(compensate_sample(cache,decision.mode,sample)==70+corner*20+unsigned(sample.plane)*7,"extreme signed MV all-plane clamped predictor");
                    const int phase=sample.plane==Plane::Y?4:8;
                    require(sample.phase_x==unsigned((sx*65535%phase+phase)%phase) && sample.phase_y==unsigned((sy*65535%phase+phase)%phase),"extreme MV phase metadata");
                }
                ++cases;
            }
            Request bad; bad.mb_x=w; bad.integer_candidates={{ref,{0,0},0}};
            rejects([&]{validate_request(cache,bad);},"MB beyond coded picture rejected");
        }
        require(cases==40,"5 geometry profiles x 4 extremes x 2 MB positions");
        for (const auto dims : {std::pair<unsigned,unsigned>{0,16},{16,0},{17,16},{16,4097},{4112,16}})
            rejects([&]{cache.retag(ref.list,ref.tag,dims.first,dims.second);},"invalid dimension rejected before mutation");
        require(cache.width(ref)==4096,"invalid retag preserves existing cache");
        Request request; request.integer_candidates.assign(4096,{ref,{65536,65536},0});
        request.fractional_candidates.assign(64,{{-1,-1},0});
        std::array<uint8_t,256> current{}; current.fill(130);
        const auto maximum=evaluate(cache,request,current);
        require(maximum.integer_results.size()==4096 && maximum.fractional_results.size()==65 && maximum.mode.winner.cost==0,"maximum supported candidate counts");
        request.integer_candidates.push_back(request.integer_candidates.back());
        rejects([&]{validate_request(cache,request);},"too many integer candidates rejected");
        request.integer_candidates.resize(1); request.fractional_candidates.push_back({{0,0},0});
        rejects([&]{validate_request(cache,request);},"too many fractional candidates rejected");
        request.fractional_candidates={{{4,0},0}};
        rejects([&]{validate_request(cache,request);},"invalid refinement range");
        request.fractional_candidates.clear(); request.integer_candidates[0].mv={65535,0};
        rejects([&]{validate_request(cache,request);},"fractional IME seed rejected");
        request.integer_candidates[0]={ref,{65536,65536},std::numeric_limits<uint64_t>::max()}; current.fill(0);
        rejects([&]{(void)evaluate(cache,request,current);},"search cost overflow rejected");
        rejects([&]{cache.fill(ref,Plane::Y,4094,4095,2,1,{2,131});},"mixed new/immutable conflicting refill rejected atomically");
        rejects([&]{(void)cache.sample(ref,Plane::Y,4094,4095);},"atomic failed refill publishes no new support");
        require(cache.sample(ref,Plane::Y,4095,4095)==130,"atomic failed refill preserves existing pixels");
        rejects([&]{(void)cache.sample(ref,Plane(9),0,0);},"invalid cache plane rejected");
        rejects([&]{(void)predict_luma(cache,1,0,{},maximum.mode.winner.candidate);},"public predictor rejects unaligned MB");
        rejects([&]{(void)predict_luma(cache,0,4096,{},maximum.mode.winner.candidate);},"public predictor rejects oversized MB");
        for (unsigned coordinate : {1u,4096u}) {
            auto mode=maximum.mode; mode.mb_x=coordinate;
            rejects([&]{validate_mode(cache,mode);},"committed mode invalid MB rejected");
            rejects([&]{(void)sample_layout(mode);},"MC layout invalid MB rejected");
        }
        Sample invalid; invalid.plane=Plane(9);
        rejects([&]{(void)compensate_sample(cache,maximum.mode,invalid);},"MC invalid plane rejected");
        invalid.plane=Plane::Y; invalid.x=16;
        rejects([&]{(void)compensate_sample(cache,maximum.mode,invalid);},"MC sample beyond partition width rejected");
        invalid.x=0; invalid.y=16;
        rejects([&]{(void)compensate_sample(cache,maximum.mode,invalid);},"MC sample beyond partition height rejected");
        auto shifted=maximum.mode; shifted.mb_x=16; invalid.y=0;
        rejects([&]{(void)compensate_sample(cache,shifted,invalid);},"MC sample before partition origin rejected");
        Request malformed; malformed.integer_candidates={{ref,{65536,65536},0}};
        malformed.picture=Picture(9);
        rejects([&]{validate_request(cache,malformed);},"unknown picture type rejected");
        malformed.picture=Picture::P; malformed.integer_candidates.clear();
        rejects([&]{validate_request(cache,malformed);},"empty search rejected");
        malformed.integer_candidates={{ref,{65536,65536},0}};
        auto wrong_tag=malformed; ++wrong_tag.integer_candidates[0].reference.tag.picture;
        rejects([&]{validate_request(cache,wrong_tag);},"missing reference tag rejected before source access");
        rejects([&]{(void)evaluate(cache,malformed,current,[&](const Candidate&,bool,unsigned){
            cache.retag(ref.list,ref.tag,4096,4096);
        });},"core search refuses a reference retagged between pipeline stages");
        std::cout<<"Limits: "<<checks<<" checks; 40 geometry/MV cases; QCIF/720p/1080p/4096, candidate bounds PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
