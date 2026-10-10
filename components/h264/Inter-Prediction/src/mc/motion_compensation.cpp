#include <h264/inter/inter_core.h>

namespace h264::inter {
void validate_mode(const ReferenceCache& cache, const Mode& mode) {
    validate_partition(mode.partition); validate_mv(mode.winner.candidate.mv);
    const auto ref = mode.winner.candidate.reference;
    if (cache.incarnation(ref) != mode.reference_incarnation)
        throw std::logic_error("committed reference incarnation is stale");
    if (mode.mb_x % 16 || mode.mb_y % 16 || mode.mb_x > 4080 || mode.mb_y > 4080 ||
        mode.mb_x + 16 > cache.width(ref) || mode.mb_y + 16 > cache.height(ref))
        throw std::invalid_argument("committed MB invalid");
}
std::vector<Sample> sample_layout(const Mode& mode) {
    validate_partition(mode.partition); validate_mv(mode.winner.candidate.mv);
    if (mode.mb_x % 16 || mode.mb_y % 16 || mode.mb_x > 4080 || mode.mb_y > 4080)
        throw std::invalid_argument("invalid MC layout coordinates");
    std::vector<Sample> out;
    for (unsigned plane = 0; plane < 3; ++plane) {
        const unsigned scale = plane == 0 ? 1 : 2;
        const auto p = mode.partition;
        const unsigned left = p.x / scale, top = p.y / scale;
        const unsigned right = (p.x + p.width) / scale, bottom = (p.y + p.height) / scale;
        for (unsigned block = 0; block < (plane == 0 ? 16u : 4u); ++block) {
            // AVC luma block numbering groups four 4x4 blocks into each 8x8.
            const unsigned bx = plane == 0 ? ((block / 4) % 2) * 8 + (block % 2) * 4 : (block % 2) * 4;
            const unsigned by = plane == 0 ? (block / 8) * 8 + ((block / 2) % 2) * 4 : (block / 2) * 4;
            for (unsigned y = by; y < by + 4; ++y) for (unsigned x = bx; x < bx + 4; ++x)
                if (x >= left && x < right && y >= top && y < bottom) {
                    const auto mv = mode.winner.candidate.mv;
                    const int phase = plane == 0 ? 4 : 8;
                    out.push_back({static_cast<Plane>(plane), block, mode.mb_x / scale + x,
                                   mode.mb_y / scale + y,
                                   unsigned((mv.x % phase + phase) % phase),
                                   unsigned((mv.y % phase + phase) % phase), unsigned(out.size()), 0, false});
                }
        }
    }
    out.back().last = true;
    return out;
}
uint8_t compensate_sample(const ReferenceCache& cache, const Mode& mode, const Sample& sample) {
    validate_mode(cache, mode);
    const auto ref = mode.winner.candidate.reference;
    const auto mv = mode.winner.candidate.mv;
    const auto p = mode.partition;
    if (sample.plane != Plane::Y && sample.plane != Plane::U && sample.plane != Plane::V)
        throw std::invalid_argument("invalid MC plane");
    const unsigned scale = sample.plane == Plane::Y ? 1 : 2;
    if (sample.x < (mode.mb_x + p.x) / scale || sample.x >= (mode.mb_x + p.x + p.width) / scale ||
        sample.y < (mode.mb_y + p.y) / scale || sample.y >= (mode.mb_y + p.y + p.height) / scale)
        throw std::invalid_argument("sample outside committed partition");
    SampleReader read = [&](int x, int y) { return cache.sample(ref, sample.plane, x, y); };
    return sample.plane == Plane::Y ? luma_qpel(read, int(sample.x) * 4 + mv.x, int(sample.y) * 4 + mv.y) :
                                     chroma_eighth(read, int(sample.x) * 8 + mv.x, int(sample.y) * 8 + mv.y);
}
}
