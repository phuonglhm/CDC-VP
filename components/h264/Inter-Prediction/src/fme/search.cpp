#include <h264/inter/inter_core.h>
#include <algorithm>
#include <cstdlib>

namespace h264::inter {
void validate_request(const ReferenceCache& cache, const Request& r) {
    validate_partition(r.partition); validate_mv(r.predicted_mv);
    if (r.picture != Picture::P && r.picture != Picture::B) throw std::invalid_argument("invalid picture type");
    if (r.integer_candidates.empty() || r.integer_candidates.size() > 4096 || r.fractional_candidates.size() > 64)
        throw std::invalid_argument("invalid candidate count");
    bool list0 = false, list1 = false;
    for (const auto& c : r.integer_candidates) {
        validate_mv(c.mv);
        if (c.mv.x % 4 || c.mv.y % 4) throw std::invalid_argument("IME MV must be integer aligned");
        if (!cache.matches(c.reference)) throw NotResident("retag reference before submitting search");
        if (r.mb_x % 16 || r.mb_y % 16 || r.mb_x > 4080 || r.mb_y > 4080 ||
            r.mb_x + 16 > cache.width(c.reference) || r.mb_y + 16 > cache.height(c.reference))
            throw std::invalid_argument("invalid MB coordinates");
        list0 |= c.reference.list == List::L0; list1 |= c.reference.list == List::L1;
    }
    if (r.picture == Picture::P && list1) throw std::invalid_argument("P search uses List 0 only");
    if (r.picture == Picture::B && !(list0 && list1))
        throw std::invalid_argument("B search requires explicit candidates on both lists");
    for (const auto& f : r.fractional_candidates)
        if (f.offset.x < -3 || f.offset.x > 3 || f.offset.y < -3 || f.offset.y > 3)
            throw std::invalid_argument("FME offset outside +/-3 quarter samples");
}
Decision evaluate(const ReferenceCache& cache, const Request& r,
                  const std::array<uint8_t, 256>& current, const CandidateHook& hook) {
    validate_request(cache, r);
    std::vector<uint64_t> incarnations;
    for (const auto& c : r.integer_candidates) incarnations.push_back(cache.incarnation(c.reference));
    auto check_references = [&] {
        for (unsigned i = 0; i < r.integer_candidates.size(); ++i)
            if (cache.incarnation(r.integer_candidates[i].reference) != incarnations[i])
                throw std::logic_error("reference retagged during search");
    };
    Decision out;
    for (unsigned i = 0; i < r.integer_candidates.size(); ++i) {
        const auto& c = r.integer_candidates[i];
        if (hook) hook(c, true, i);
        check_references();
        const auto prediction = predict_luma(cache, r.mb_x, r.mb_y, {}, c);
        std::array<uint8_t, 256> pixels{};
        std::copy(prediction.begin(), prediction.end(), pixels.begin());
        uint64_t sad = 0;
        for (const auto& sum : sad_tree(current, pixels))
            if (sum.partition.x == r.partition.x && sum.partition.y == r.partition.y &&
                sum.partition.width == r.partition.width && sum.partition.height == r.partition.height) {
                sad = sum.sad; break;
            }
        out.integer_results.push_back({c, sad, checked_cost(sad, c.rate)});
    }
    auto best = out.integer_results.front();
    for (const auto& result : out.integer_results) if (result.cost < best.cost) best = result;
    const auto integer_best = best;
    // Retain IME winner as the first FME candidate, with its own rate term.
    out.fractional_results.push_back(integer_best);
    for (unsigned i = 0; i < r.fractional_candidates.size(); ++i) {
        const auto& f = r.fractional_candidates[i];
        Candidate c = integer_best.candidate;
        c.mv.x += f.offset.x; c.mv.y += f.offset.y; c.rate = f.rate;
        validate_mv(c.mv);
        if (hook) hook(c, false, i);
        check_references();
        const auto prediction = predict_luma(cache, r.mb_x, r.mb_y, r.partition, c);
        uint64_t sad = 0;
        for (unsigned y = 0; y < r.partition.height; ++y) for (unsigned x = 0; x < r.partition.width; ++x)
            sad += std::abs(int(current[(r.partition.y + y) * 16 + r.partition.x + x]) -
                            prediction[y * r.partition.width + x]);
        const CandidateResult result{c, sad, checked_cost(sad, c.rate)};
        out.fractional_results.push_back(result);
        if (result.cost < best.cost) best = result;
    }
    check_references();
    out.mode = {r.mb_x, r.mb_y, r.partition, best, cache.incarnation(best.candidate.reference), r.predicted_mv};
    out.predictor = predict_luma(cache, r.mb_x, r.mb_y, r.partition, best.candidate);
    for (unsigned y = 0; y < r.partition.height; ++y) for (unsigned x = 0; x < r.partition.width; ++x)
        out.residual.push_back(int16_t(int(current[(r.partition.y + y) * 16 + r.partition.x + x]) -
                                      out.predictor[y * r.partition.width + x]));
    return out;
}
}
