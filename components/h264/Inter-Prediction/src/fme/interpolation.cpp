#include <h264/inter/inter_core.h>
#include <algorithm>

namespace h264::inter {
namespace {
int floor_div(int a, int b) { const int q = a / b; return q - (a % b < 0); }
uint8_t clip(int a) { return static_cast<uint8_t>(std::clamp(a, 0, 255)); }
int average(int a, int b) { return (a + b + 1) / 2; }
constexpr int taps[6] = {1, -5, 20, 20, -5, 1};
int half(const SampleReader& read, int x, int y, bool horizontal) {
    int sum = 0;
    for (int k = 0; k < 6; ++k)
        sum += taps[k] * read(x + (horizontal ? k - 2 : 0), y + (horizontal ? 0 : k - 2));
    return clip(floor_div(sum + 16, 32));
}
int diagonal(const SampleReader& read, int x, int y) {
    int sum = 0;
    // Unrounded, unclipped horizontal intermediates are retained for stage 2.
    for (int j = 0; j < 6; ++j) {
        int row = 0;
        for (int i = 0; i < 6; ++i) row += taps[i] * read(x + i - 2, y + j - 2);
        sum += taps[j] * row;
    }
    return clip(floor_div(sum + 512, 1024));
}
}
uint8_t luma_qpel(const SampleReader& read, int xq, int yq) {
    const int x = floor_div(xq, 4), y = floor_div(yq, 4);
    const int fx = xq - x * 4, fy = yq - y * 4;
    if (!fx && !fy) return read(x, y);
    if (!fy) {
        const int b = half(read, x, y, true);
        return fx == 2 ? b : average(b, read(x + (fx == 3), y));
    }
    if (!fx) {
        const int h = half(read, x, y, false);
        return fy == 2 ? h : average(h, read(x, y + (fy == 3)));
    }
    if (fx == 2 && fy == 2) return diagonal(read, x, y);
    if (fx == 2) return average(diagonal(read, x, y), half(read, x, y + (fy == 3), true));
    if (fy == 2) return average(diagonal(read, x, y), half(read, x + (fx == 3), y, false));
    return average(half(read, x, y + (fy == 3), true), half(read, x + (fx == 3), y, false));
}
uint8_t chroma_eighth(const SampleReader& read, int xq, int yq) {
    const int x = floor_div(xq, 8), y = floor_div(yq, 8);
    const int fx = xq - x * 8, fy = yq - y * 8;
    int sum = (8 - fx) * (8 - fy) * read(x, y);
    // Zero-weight samples do not create artificial support dependencies.
    if (fx) sum += fx * (8 - fy) * read(x + 1, y);
    if (fy) sum += (8 - fx) * fy * read(x, y + 1);
    if (fx && fy) sum += fx * fy * read(x + 1, y + 1);
    return static_cast<uint8_t>((sum + 32) / 64);
}
std::vector<uint8_t> predict_luma(const ReferenceCache& cache, unsigned mb_x, unsigned mb_y,
                                Partition p, const Candidate& c) {
    validate_partition(p); validate_mv(c.mv);
    if (mb_x % 16 || mb_y % 16 || mb_x > 4080 || mb_y > 4080 ||
        mb_x + 16 > cache.width(c.reference) || mb_y + 16 > cache.height(c.reference))
        throw std::invalid_argument("MB outside reference geometry");
    SampleReader reader = [&](int x, int y) { return cache.sample(c.reference, Plane::Y, x, y); };
    std::vector<uint8_t> out;
    out.reserve(p.width * p.height);
    for (unsigned y = 0; y < p.height; ++y) for (unsigned x = 0; x < p.width; ++x)
        out.push_back(luma_qpel(reader, int(mb_x + p.x + x) * 4 + c.mv.x,
                               int(mb_y + p.y + y) * 4 + c.mv.y));
    return out;
}
}
