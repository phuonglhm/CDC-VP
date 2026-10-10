#include <h264/inter/inter_core.h>
#include <cstdlib>

namespace h264::inter {
std::vector<SadPartition> sad_tree(const std::array<uint8_t, 256>& current,
                                 const std::array<uint8_t, 256>& reference) {
    std::array<uint64_t, 16> primitive{};
    for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 16; ++x)
        primitive[(y / 4) * 4 + x / 4] += std::abs(int(current[y * 16 + x]) - reference[y * 16 + x]);
    constexpr unsigned widths[] = {4, 4, 8, 8, 8, 16, 16};
    constexpr unsigned heights[] = {4, 8, 4, 8, 16, 8, 16};
    std::vector<SadPartition> out;
    out.reserve(41);
    for (unsigned kind = 0; kind < 7; ++kind)
        for (unsigned y = 0; y < 16; y += heights[kind])
            for (unsigned x = 0; x < 16; x += widths[kind]) {
                uint64_t sad = 0;
                for (unsigned yy = y; yy < y + heights[kind]; yy += 4)
                    for (unsigned xx = x; xx < x + widths[kind]; xx += 4)
                        sad += primitive[(yy / 4) * 4 + xx / 4];
                out.push_back({{x, y, widths[kind], heights[kind]}, sad});
            }
    return out;
}
}
