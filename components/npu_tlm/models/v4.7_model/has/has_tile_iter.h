// has_tile_iter.h -- tile iterator of the Data Flow Controller (docs/ARCHITECTURE.md, section 7).
//
// One GEMM_FUSED instruction = one layer. The DFC walks the layer's output tensor on a regular grid of
// cout_t x h_t x w_t tiles (edge tiles clipped with min), output channels outermost, then oy, then ox -- the
// order and grid the offline planner (tools/fe/fe_tile_plan.py) already produces. Everything else of a tile
// is derived here: the padded input window (iy/ix may be < 0 or > H/W: zero padding), X_used (= channels of
// the tile) and Y_used (proposal: gcd(w_tile, 32)). tools/has/tb_has_tile_iter checks this against every tile
// of an exported prog.bin.
//
// Pure C++ (no SystemC) so the same code can sit in the DFC FSM and in host-side tools.
#ifndef HAS_TILE_ITER_H
#define HAS_TILE_ITER_H

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>

namespace has
{
    // Per-layer geometry = what the instruction carries (old MMIO kh/kw/stride/pad + extension registers).
    struct LayerGeom
    {
        int cout{0}, oh{0}, ow{0};        // output tensor
        int kh{1}, kw{1}, sy{1}, sx{1};   // kernel, stride (dilation 1)
        int pad_t{0}, pad_l{0};           // top / left padding
        int cout_t{0}, h_t{0}, w_t{0};    // tile size (from the planner)
        int sa_y{32};                     // systolic array rows (Y_DIM): bound of Y_used
        int order{0};                     // 0: channel tile outermost (planner order); 1: spatial outermost (per-layer tile order);
                                          // 2: channel tile outermost, then column tile, row tile innermost (halo reuse)
        int y_used{0};                    // Y_USED register: explicit Y_used when it divides the tile width; 0 = gcd rule
        bool pad_tail{false};             // FLAGS bit 0 PAD_TAIL: edge tiles are COMPUTED at full cout_t channels and
                                          // a width rounded up to Y (Y_USED or 32); c1/ox1 are then the computed extents and the
                                          // DFC writes back only c < cout, ox < ow. Weights of the last channel block are padded
                                          // with zeros to cout_t by the compiler.
    };

    struct TileGeom
    {
        int c0, c1, oy0, oy1, ox0, ox1;   // output region [c0,c1) x [oy0,oy1) x [ox0,ox1)
        int iy0, iy1, ix0, ix1;           // input window, half-open, may reach into the padding
        int x_used, y_used;
    };

    class TileIter
    {
    public:
        explicit TileIter(const LayerGeom &g) : g_(g)
        {
            if (g.cout <= 0 || g.oh <= 0 || g.ow <= 0 || g.cout_t <= 0 || g.h_t <= 0 || g.w_t <= 0 ||
                g.kh <= 0 || g.kw <= 0 || g.sy <= 0 || g.sx <= 0 || g.sa_y <= 0)
                throw std::invalid_argument("TileIter: non-positive layer geometry");
            nc_ = (g.cout + g.cout_t - 1) / g.cout_t;
            ny_ = (g.oh + g.h_t - 1) / g.h_t;
            nx_ = (g.ow + g.w_t - 1) / g.w_t;
        }

        long count() const { return long(nc_) * ny_ * nx_; }
        int n_cout_tiles() const { return nc_; }
        long n_spatial_tiles() const { return long(ny_) * nx_; }
        int n_row_tiles() const { return ny_; }

        // Tile number i in issue order (channel tile outermost, then row tile, then column tile).
        TileGeom at(long i) const
        {
            if (i < 0 || i >= count()) throw std::out_of_range("TileIter::at");
            int xi, yi, ci;
            if (g_.order == 1) { ci = int(i % nc_); xi = int((i / nc_) % nx_); yi = int(i / (long(nc_) * nx_)); }
            else if (g_.order == 2) { yi = int(i % ny_); xi = int((i / ny_) % nx_); ci = int(i / (long(nx_) * ny_)); }
            else { xi = int(i % nx_); yi = int((i / nx_) % ny_); ci = int(i / (long(nx_) * ny_)); }
            TileGeom t;
            t.c0 = ci * g_.cout_t;  t.c1 = std::min(t.c0 + g_.cout_t, g_.cout);
            t.oy0 = yi * g_.h_t;    t.oy1 = std::min(t.oy0 + g_.h_t, g_.oh);
            t.ox0 = xi * g_.w_t;    t.ox1 = std::min(t.ox0 + g_.w_t, g_.ow);
            t.iy0 = t.oy0 * g_.sy - g_.pad_t;  t.iy1 = (t.oy1 - 1) * g_.sy - g_.pad_t + g_.kh;
            t.ix0 = t.ox0 * g_.sx - g_.pad_l;  t.ix1 = (t.ox1 - 1) * g_.sx - g_.pad_l + g_.kw;
            t.x_used = t.c1 - t.c0;
            const int wt = t.ox1 - t.ox0;
            t.y_used = (g_.y_used > 0 && g_.y_used <= g_.sa_y && wt % g_.y_used == 0) ? g_.y_used : std::gcd(wt, g_.sa_y);
            if (g_.pad_tail)
            {
                t.c1 = t.c0 + g_.cout_t;
                t.x_used = g_.cout_t;
                const int y = (g_.y_used > 0 && g_.y_used <= g_.sa_y) ? g_.y_used : g_.sa_y;
                if (wt % y != 0)
                {
                    t.ox1 = t.ox0 + (wt + y - 1) / y * y;
                    t.ix1 = (t.ox1 - 1) * g_.sx - g_.pad_l + g_.kw;
                }
                t.y_used = y;
            }
            return t;
        }

        // Stateful walk, the way the DFC FSM consumes it.
        bool next(TileGeom &t)
        {
            if (pos_ >= count()) return false;
            t = at(pos_++);
            return true;
        }
        void reset() { pos_ = 0; }

    private:
        LayerGeom g_;
        int nc_{0}, ny_{0}, nx_{0};
        long pos_{0};
    };
} // namespace has

#endif
