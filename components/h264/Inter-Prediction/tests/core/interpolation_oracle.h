#pragma once
#include <h264/inter/inter_core.h>
#include <algorithm>
#include <cmath>

// Table-driven oracle: construct the six anchor samples using direct 2D
// convolution, then select among all sixteen phases. Production uses separate
// phase branches and an unrounded intermediate row pipeline.
inline int oracle_floor(int n, int d) { return int(std::floor(double(n) / d)); }
inline int oracle_clip(int v) { return std::clamp(v, 0, 255); }
inline uint8_t oracle_luma(const h264::inter::SampleReader& read, int qx, int qy) {
    const int x = oracle_floor(qx, 4), y = oracle_floor(qy, 4);
    constexpr int c[6] = {1, -5, 20, 20, -5, 1};
    int hb = 0, hs = 0, vh = 0, vm = 0, dd = 0;
    for (int i = 0; i < 6; ++i) {
        hb += c[i] * read(x - 2 + i, y);
        hs += c[i] * read(x - 2 + i, y + 1);
        vh += c[i] * read(x, y - 2 + i);
        vm += c[i] * read(x + 1, y - 2 + i);
        for (int k = 0; k < 6; ++k) dd += c[i] * c[k] * read(x - 2 + i, y - 2 + k);
    }
    const int G = read(x, y), H = read(x + 1, y), M = read(x, y + 1);
    const int b = oracle_clip(oracle_floor(hb + 16, 32));
    const int s = oracle_clip(oracle_floor(hs + 16, 32));
    const int h = oracle_clip(oracle_floor(vh + 16, 32));
    const int m = oracle_clip(oracle_floor(vm + 16, 32));
    const int j = oracle_clip(oracle_floor(dd + 512, 1024));
    const auto avg = [](int a, int z) { return (a + z + 1) / 2; };
    const int grid[4][4] = {
        {G, avg(G,b), b, avg(H,b)},
        {avg(G,h), avg(b,h), avg(b,j), avg(b,m)},
        {h, avg(h,j), j, avg(j,m)},
        {avg(M,h), avg(h,s), avg(j,s), avg(m,s)}
    };
    return uint8_t(grid[qy - y * 4][qx - x * 4]);
}
inline uint8_t oracle_chroma(const h264::inter::SampleReader& read, int qx, int qy) {
    const int x = oracle_floor(qx, 8), y = oracle_floor(qy, 8);
    const int fx = qx - x * 8, fy = qy - y * 8;
    const double ax = double(fx) / 8, ay = double(fy) / 8;
    const double value = (1-ax)*(1-ay)*read(x,y) + ax*(1-ay)*read(x+1,y) +
                        (1-ax)*ay*read(x,y+1) + ax*ay*read(x+1,y+1);
    return uint8_t(std::floor(value + 0.5)); // Binary phases are exactly representable.
}
