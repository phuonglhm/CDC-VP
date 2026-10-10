#include "test_support.h"
#include "interpolation_oracle.h"
#include <limits>
#include <cstdlib>
using namespace h264::inter;

int main() {
    try {
        for (unsigned seed : {20261010u, 19840123u}) for (unsigned trial = 0; trial < 128; ++trial) {
            auto a = pattern(16,16,seed+trial), b = pattern(16,16,seed+trial+721);
            if (!trial) { std::fill(a.begin(),a.end(),0); std::fill(b.begin(),b.end(),255); }
            if (trial == 1) b = a;
            std::array<uint8_t,256> aa{},bb{};
            std::copy(a.begin(),a.end(),aa.begin()); std::copy(b.begin(),b.end(),bb.begin());
            const auto tree = sad_tree(aa,bb);
            require(tree.size() == 41, "all 41 SAD partition positions");
            for (const auto& item : tree) {
                uint64_t expected = 0; const auto p = item.partition;
                for (unsigned y = p.y; y < p.y+p.height; ++y) for (unsigned x = p.x; x < p.x+p.width; ++x)
                    expected += std::abs(int(a[y*16+x])-b[y*16+x]);
                require(item.sad == expected, "SAD direct pixel oracle");
            }
        }
        require(checked_cost(65280,1ull<<40) == (1ull<<40)+65280, "wide cost");
        rejects([] { (void)checked_cost(1,std::numeric_limits<uint64_t>::max()); }, "cost overflow rejected");
        Reference ref{List::L0,{1,0}}, other{List::L1,{2,1}};
        ReferenceCache cache;
        cache.retag(ref.list,ref.tag,32,32); cache.retag(other.list,other.tag,32,32);
        require(cache.matches(ref) && cache.matches(other), "independent tags");
        rejects([&] { (void)cache.sample(ref,Plane::Y,0,0); }, "tag alone not resident");
        cache.fill(ref,Plane::Y,0,0,8,16,std::vector<uint8_t>(128,20));
        rejects([&] { (void)predict_luma(cache,0,0,{}, {ref,{0,0},0}); }, "row-start second column needed");
        cache.fill(ref,Plane::Y,8,0,8,16,std::vector<uint8_t>(128,20));
        require(predict_luma(cache,0,0,{}, {ref,{0,0},0}).size() == 256, "second column permits integer MB");
        rejects([&] { (void)cache.sample(other,Plane::Y,0,0); }, "L0 residency not L1 readiness");
        rejects([&] { (void)cache.sample(ref,Plane::U,0,0); }, "Y residency not chroma readiness");
        auto bad = std::vector<uint8_t>(256,20); bad.back()=21;
        rejects([&] { cache.fill(ref,Plane::Y,0,0,16,16,bad); }, "immutable refill rejected atomically");
        require(cache.sample(ref,Plane::Y,15,15) == 20, "failed refill leaves old samples");
        require(cache.sample(ref,Plane::Y,-999,-999) == 20, "edge extension");
        const auto old = cache.incarnation(ref);
        cache.retag(ref.list,ref.tag,32,32);
        require(cache.incarnation(ref) != old, "same-tag retag changes incarnation");
        rejects([&] { (void)cache.sample(ref,Plane::Y,0,0); }, "retag invalidates residency");
        rejects([&] { cache.retag(List(8),{},32,32); }, "invalid list");
        rejects([&] { cache.retag(List::L0,{0,3},32,32); }, "invalid slot");
        rejects([&] { cache.retag(List::L0,{},17,32); }, "invalid geometry");
        rejects([&] { cache.fill(ref,Plane::Y,31,0,2,1,{1,2}); }, "invalid rectangle");
        rejects([&] { validate_partition({4,0,8,8}); }, "partition alignment");
        rejects([&] { validate_partition({0,0,16,4}); }, "invalid partition shape");
        rejects([&] { validate_mv({65537,0}); }, "MV range");

        fill_picture(cache,ref,32,32,20261010); fill_picture(cache,other,32,32,7382);
        std::array<uint8_t,256> source{};
        Request request;
        request.integer_candidates = {{ref,{-4,0},uint64_t(1)<<35},{ref,{0,0},3},{ref,{4,4},7}};
        request.fractional_candidates = {{{1,0},2},{{-1,2},4},{{3,-3},1}};
        request.predicted_mv={-8,12};
        constexpr unsigned widths[] = {4,4,8,8,8,16,16}, heights[] = {4,8,4,8,16,8,16};
        for (unsigned trial = 0; trial < 12; ++trial) {
            const auto bytes = pattern(16,16,trial+773); std::copy(bytes.begin(),bytes.end(),source.begin());
            for (unsigned kind = 0; kind < 7; ++kind) for (unsigned py = 0; py < 16; py += heights[kind])
                for (unsigned px = 0; px < 16; px += widths[kind]) {
                    request.partition = {px,py,widths[kind],heights[kind]};
                    const auto d = evaluate(cache,request,source);
                    auto oracle = [&](const Candidate& c) {
                        SampleReader read=[&](int x,int y) { return cache.sample(c.reference,Plane::Y,x,y); };
                        uint64_t sum=0;
                        for (unsigned y=py;y<py+heights[kind];++y) for (unsigned x=px;x<px+widths[kind];++x)
                            sum+=std::abs(int(source[y*16+x])-oracle_luma(read,int(x)*4+c.mv.x,int(y)*4+c.mv.y));
                        return sum;
                    };
                    auto best = d.integer_results.front();
                    for (const auto& c : d.integer_results) {
                        require(c.sad == oracle(c.candidate), "IME oracle metadata/cost");
                        require(c.cost == c.sad+c.candidate.rate, "IME wide rate");
                        if (c.cost < best.cost) best=c;
                    }
                    for (const auto& c : d.fractional_results) {
                        require(c.sad == oracle(c.candidate), "FME independent predictor oracle");
                        if (c.cost < best.cost) best=c;
                    }
                    require(d.mode.winner.cost == best.cost && d.mode.winner.candidate.mv.x == best.candidate.mv.x &&
                            d.mode.winner.candidate.mv.y == best.candidate.mv.y, "winner alignment");
                    const auto syntax=motion_syntax(d.mode);
                    require(syntax.mvd.x == best.candidate.mv.x+8 && syntax.mvd.y == best.candidate.mv.y-12, "EEI supplied MVP subtraction");
                    const auto layout=sample_layout(d.mode);
                    require(layout.size() == widths[kind]*heights[kind]*3/2 && layout.back().last, "MC layout count/last");
                    unsigned seen=0;
                    for (const auto& sample : layout) {
                        SampleReader read=[&](int x,int y) { return cache.sample(best.candidate.reference,sample.plane,x,y); };
                        const int scale=sample.plane==Plane::Y ? 4 : 8;
                        const auto expected=sample.plane==Plane::Y ? oracle_luma(read,int(sample.x)*scale+best.candidate.mv.x,int(sample.y)*scale+best.candidate.mv.y) :
                            oracle_chroma(read,int(sample.x)*scale+best.candidate.mv.x,int(sample.y)*scale+best.candidate.mv.y);
                        require(compensate_sample(cache,d.mode,sample)==expected, "MC all-plane oracle");
                        require(sample.sequence == seen++ && sample.last == (seen==layout.size()), "MC metadata sequence");
                    }
                    unsigned n=0;
                    for (unsigned y=py;y<py+heights[kind];++y) for (unsigned x=px;x<px+widths[kind];++x) {
                        require(d.residual[n] == int(source[y*16+x])-d.predictor[n], "partition residual"); ++n;
                    }
                }
        }
        // Intentional ties across distinct MVs/lists: stable caller order.
        cache.retag(ref.list,ref.tag,32,32); cache.retag(other.list,other.tag,32,32);
        for (auto ref2 : {ref,other}) for (auto p : {Plane::Y,Plane::U,Plane::V}) {
            const unsigned n=p==Plane::Y?32:16; cache.fill(ref2,p,0,0,n,n,std::vector<uint8_t>(n*n,50));
        }
        source.fill(50); request.partition={}; request.picture=Picture::B;
        request.integer_candidates={{other,{4,0},9},{ref,{0,0},9}};
        request.fractional_candidates={{{1,1},9}};
        const auto tie=evaluate(cache,request,source);
        require(tie.mode.winner.candidate.reference.list==List::L1 && tie.mode.winner.candidate.mv.x==4, "stable list/MV tie");
        std::swap(request.integer_candidates[0],request.integer_candidates[1]);
        require(evaluate(cache,request,source).mode.winner.candidate.reference.list==List::L0, "caller order tie policy");
        cache.retag(other.list,other.tag,32,32);
        rejects([&] { validate_mode(cache,tie.mode); }, "stale committed mode rejected");
        request.picture=Picture::P;
        rejects([&] { validate_request(cache,request); }, "P list1 invalid");
        request.integer_candidates.resize(1); request.picture=Picture::B;
        rejects([&] { validate_request(cache,request); }, "B requires both explicit lists");
        cache.invalidate(); require(!cache.matches(ref) && !cache.matches(other), "reset clears both lists");
        std::cout << "Core: " << checks << " checks; SAD 41 positions, search, MC/EEI, residency PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
