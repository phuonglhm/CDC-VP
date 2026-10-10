#pragma once
#include <h264/inter/inter_core.h>
#include <algorithm>
#include <iostream>
#include <string>

inline unsigned checks = 0;
inline void require(bool value, const char* label) {
    ++checks;
    if (!value) throw std::runtime_error(label);
}
template<class F> void rejects(F&& action, const char* label) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, label);
}
inline std::vector<uint8_t> pattern(unsigned width, unsigned height, unsigned seed) {
    std::vector<uint8_t> out(width * height);
    uint32_t state = seed;
    for (auto& v : out) { state = state * 1664525u + 1013904223u; v = uint8_t(state >> 24); }
    return out;
}
inline void fill_picture(h264::inter::ReferenceCache& cache, h264::inter::Reference ref,
                         unsigned width, unsigned height, unsigned seed) {
    using namespace h264::inter;
    cache.retag(ref.list, ref.tag, width, height);
    for (auto plane : {Plane::Y, Plane::U, Plane::V}) {
        const unsigned scale = plane == Plane::Y ? 1 : 2;
        cache.fill(ref, plane, 0, 0, width / scale, height / scale,
                   pattern(width / scale, height / scale, seed + unsigned(plane)));
    }
}
