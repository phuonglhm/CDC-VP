// has_dma.h -- 4-channel DMA of the HAS (section 6.11) for HasNpuTop.
//
// Same scheduling as control/sauria_dma.h (one read port shared by CH0..CH3, one independent write port, write
// processed before read in a cycle, a burst's data lands when its last beat completes) with what the v4.5 model
// hard-codes turned into parameters:
//   bytes_per_beat   32 = v4.5 model (256-bit bus)          16 = HAS AXI 128-bit
//   burst_beats      8 (HAS: "8-beat burst")
//   dram_latency     extra cycles before the FIRST burst of every transfer (read and write); ESTIMATE, 0 = v4.5
//   ch3_low_priority false = v4.5 round robin over CH0..CH3; true = HAS: CH3 (LUT / bias / scale) only when CH0..CH2 idle
// Compat parameters (32, 8, 0, false) reproduce SauriaDma cycle for cycle (tools/has/tb_has_dma checks it), so numbers
// measured with the v4.5 DMA stay comparable. The SRAM side goes through a port object, so the DMA can target the
// host half of the core's ping-pong SRAM instead of the separate v4.5 Sram:
//   struct Port { void write(int bank, uint32_t off, const uint8_t *src, uint32_t n);
//                 void read (int bank, uint32_t off, uint8_t *dst, uint32_t n); };
#if !defined(HAS_DMA_PURE_CPP)
#include <systemc.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#ifndef HAS_DMA_DESC_H
#define HAS_DMA_DESC_H

namespace has
{
    struct DmaParams
    {
        uint32_t bytes_per_beat{32};
        uint32_t burst_beats{8};
        uint32_t dram_latency{0};
        bool ch3_low_priority{false};
        static DmaParams v45() { return DmaParams(); }
        static DmaParams has_axi128(uint32_t latency = 0)
        {
            DmaParams p;
            p.bytes_per_beat = 16;
            p.dram_latency = latency;
            p.ch3_low_priority = true;
            return p;
        }
        uint32_t burst_bytes() const { return bytes_per_beat * burst_beats; }
    };

    // 4-D strided / slice / concat descriptor for DMA transfers (replaces GVU AGU_ND).
    // Dimension 0 is fastest (innermost), dimension 3 is slowest (outermost):
    //   addr_at(k) = base + (i0 * s[0] + i1 * s[1] + i2 * s[2] + i3 * s[3]) * elem_bytes
    // where each item k in [0, length()) is a contiguous block of `elem_bytes` bytes.
    struct DmaNdDesc
    {
        uint32_t base{0};
        std::array<unsigned, 4> n{{1, 1, 1, 1}};
        std::array<int64_t, 4> s{{1, 0, 0, 0}};
        uint32_t elem_bytes{1};

        uint64_t length() const
        {
            return uint64_t(std::max(1u, n[0])) * std::max(1u, n[1]) * std::max(1u, n[2]) * std::max(1u, n[3]);
        }
        uint32_t total_bytes() const { return static_cast<uint32_t>(length() * std::max(1u, elem_bytes)); }

        uint32_t addr_at(uint64_t k) const
        {
            const unsigned n0 = std::max(1u, n[0]), n1 = std::max(1u, n[1]), n2 = std::max(1u, n[2]), n3 = std::max(1u, n[3]);
            const uint64_t i0 = k % n0, q0 = k / n0;
            const uint64_t i1 = q0 % n1, q1 = q0 / n1;
            const uint64_t i2 = q1 % n2, i3 = (q1 / n2) % n3;
            const int64_t off = int64_t(base) +
                                (int64_t(i0) * s[0] + int64_t(i1) * s[1] + int64_t(i2) * s[2] + int64_t(i3) * s[3]) *
                                    int64_t(std::max(1u, elem_bytes));
            return static_cast<uint32_t>(off);
        }

        uint32_t byte_at(uint64_t byte_idx) const
        {
            const uint32_t eb = std::max(1u, elem_bytes);
            return addr_at(byte_idx / eb) + static_cast<uint32_t>(byte_idx % eb);
        }

        std::vector<uint32_t> sequence() const
        {
            const uint64_t L = length();
            std::vector<uint32_t> out(static_cast<size_t>(L));
            for (uint64_t k = 0; k < L; k++) out[static_cast<size_t>(k)] = addr_at(k);
            return out;
        }

        static DmaNdDesc linear(uint32_t base_addr, uint32_t count, uint32_t elem_b = 1)
        {
            DmaNdDesc d;
            d.base = base_addr;
            d.n = {{ std::max(1u, count), 1, 1, 1 }};
            d.s = {{ 1, 0, 0, 0 }};
            d.elem_bytes = std::max(1u, elem_b);
            return d;
        }

        // Row-addressed descriptor (e.g. 32-byte or 128-byte Scratchpad rows, matching AguNdDesc).
        static DmaNdDesc rows(unsigned base_row, std::array<unsigned, 4> counts, std::array<int, 4> row_strides, uint32_t row_bytes = 32)
        {
            DmaNdDesc d;
            d.base = base_row * row_bytes;
            for (int k = 0; k < 4; k++)
            {
                d.n[k] = std::max(1u, counts[k]);
                d.s[k] = row_strides[k];
            }
            d.elem_bytes = row_bytes;
            return d;
        }

        // Row-major [C][H][W] tensor at `base_addr` (dim 0 = W fastest, dim 1 = H, dim 2 = C).
        static DmaNdDesc tensor_chw(uint32_t base_addr, unsigned C, unsigned H, unsigned W, uint32_t elem_b = 1)
        {
            DmaNdDesc d;
            d.base = base_addr;
            d.n = {{ std::max(1u, W), std::max(1u, H), std::max(1u, C), 1 }};
            d.s = {{ 1, int64_t(W), int64_t(H) * int64_t(W), 0 }};
            d.elem_bytes = std::max(1u, elem_b);
            return d;
        }

        // Slice along internal dimension `dim` (0 = fastest .. 3 = slowest): elements [offset, end) with `step`.
        DmaNdDesc sliced(int dim, unsigned offset, unsigned end, unsigned step = 1) const
        {
            DmaNdDesc d = *this;
            if (dim < 0 || dim > 3) return d;
            const unsigned st = std::max(1u, step);
            d.base = static_cast<uint32_t>(int64_t(base) + int64_t(offset) * s[dim] * int64_t(std::max(1u, elem_bytes)));
            d.n[dim] = end > offset ? (end - offset + st - 1u) / st : 1u;
            d.s[dim] = s[dim] * int64_t(st);
            return d;
        }

        // Slice a [C][H][W] tensor along `chw_axis` (0 = C, 1 = H, 2 = W): starting at `offset` for `len` elements.
        DmaNdDesc sliced_chw(int chw_axis, unsigned offset, unsigned len, unsigned step = 1) const
        {
            const int dim = (chw_axis == 0) ? 2 : (chw_axis == 1 ? 1 : 0);
            return sliced(dim, offset, offset + len, step);
        }

        // Concat destination descriptor for placing a [part_C][part_H][part_W] tensor into [out_C][out_H][out_W]
        // at `offset` along `chw_axis` (0 = C, 1 = H, 2 = W).
        static DmaNdDesc concat_chw(uint32_t base_addr, unsigned out_C, unsigned out_H, unsigned out_W,
                                    int chw_axis, unsigned offset, unsigned part_len, uint32_t elem_b = 1)
        {
            return tensor_chw(base_addr, out_C, out_H, out_W, elem_b).sliced_chw(chw_axis, offset, part_len, 1);
        }

        // Permute / transpose dimensions: new dimension k walks old dimension perm[k].
        DmaNdDesc permuted(std::array<int, 4> perm) const
        {
            DmaNdDesc d = *this;
            for (int k = 0; k < 4; k++)
            {
                d.n[k] = n[perm[k] & 3];
                d.s[k] = s[perm[k] & 3];
            }
            return d;
        }

        // 3-D tile input window gather: slices [c0..c0+nc, y0..y0+hr, x0..x0+wr] from [in_c][in_h][in_w].
        static DmaNdDesc tile_window_3d(uint32_t base_addr, unsigned in_c, unsigned in_h, unsigned in_w,
                                        unsigned c0, unsigned nc, unsigned y0, unsigned hr, unsigned x0, unsigned wr)
        {
            (void)in_c;
            DmaNdDesc d;
            d.base = base_addr + ((c0 * in_h + y0) * in_w + x0);
            d.n = {{ std::max(1u, wr), std::max(1u, hr), std::max(1u, nc), 1 }};
            d.s = {{ 1, int64_t(in_w), int64_t(in_h) * int64_t(in_w), 0 }};
            d.elem_bytes = 1;
            return d;
        }

        // 3-D tile output write-back (concat/scatter): places tile [nc][ht][wt] into [out_c][out_h][out_w] at (c0, y0, x0).
        static DmaNdDesc tile_writeback_3d(uint32_t base_addr, unsigned out_c, unsigned out_h, unsigned out_w,
                                           unsigned c0, unsigned nc, unsigned y0, unsigned ht, unsigned x0, unsigned wt)
        {
            (void)out_c;
            DmaNdDesc d;
            d.base = base_addr + ((c0 * out_h + y0) * out_w + x0);
            d.n = {{ std::max(1u, wt), std::max(1u, ht), std::max(1u, nc), 1 }};
            d.s = {{ 1, int64_t(out_w), int64_t(out_h) * int64_t(out_w), 0 }};
            d.elem_bytes = 1;
            return d;
        }

        // Nearest-neighbor spatial upsample source descriptor for [C][H][W] -> [C][H*scale_h][W*scale_w].
        // Folds C and H into R = C*H contiguous input rows and uses zero-stride loops to replicate each
        // element `scale_w` times horizontally and each upsampled row `scale_h` times vertically.
        static DmaNdDesc upsample_chw(uint32_t base_addr, unsigned C, unsigned H, unsigned W,
                                      unsigned scale_h = 2, unsigned scale_w = 2, uint32_t elem_b = 1)
        {
            const unsigned c = std::max(1u, C), h = std::max(1u, H), w = std::max(1u, W);
            const unsigned sh = std::max(1u, scale_h), sw = std::max(1u, scale_w);
            const uint32_t eb = std::max(1u, elem_b);
            if (sh == 1u && sw == 1u)
                return linear(base_addr, c * h * w, eb);
            DmaNdDesc d;
            d.base = base_addr;
            d.elem_bytes = eb;
            if (sw == 1u)
            {
                // Entire row of W elements stays contiguous (run_of == W) and repeats `sh` times
                d.n = {{ w, sh, c * h, 1u }};
                d.s = {{ 1, 0, int64_t(w), 0 }};
            }
            else
            {
                d.n = {{ sw, w, sh, c * h }};
                d.s = {{ 0, 1, 0, int64_t(w) }};
            }
            return d;
        }

        // Resolves at most one inferred (-1) dimension in [out_c][out_h][out_w] and checks exact element conservation.
        static bool resolve_reshape_dims(uint64_t in_elems, int &out_c, int &out_h, int &out_w)
        {
            if (in_elems == 0) return false;
            int dims[3] = { out_c, out_h, out_w };
            int neg_idx = -1;
            uint64_t known_prod = 1;
            for (int i = 0; i < 3; i++)
            {
                if (dims[i] == -1)
                {
                    if (neg_idx >= 0) return false; // at most one -1 axis allowed
                    neg_idx = i;
                }
                else if (dims[i] <= 0)
                {
                    return false;
                }
                else
                {
                    const uint64_t d = static_cast<uint64_t>(dims[i]);
                    if (d > 0xFFFFFFFFFFFFFFFFull / known_prod) return false;
                    known_prod *= d;
                }
            }
            if (neg_idx >= 0)
            {
                if (known_prod == 0 || (in_elems % known_prod) != 0) return false;
                const uint64_t inferred = in_elems / known_prod;
                if (inferred == 0 || inferred > 0x7FFFFFFFull) return false;
                dims[neg_idx] = static_cast<int>(inferred);
                known_prod *= inferred;
            }
            if (known_prod != in_elems) return false;
            out_c = dims[0];
            out_h = dims[1];
            out_w = dims[2];
            return true;
        }

        // Reshaped [out_C][out_H][out_W] tensor view descriptor at `base_addr`.
        static DmaNdDesc reshape_chw(uint32_t base_addr, unsigned out_C, unsigned out_H, unsigned out_W, uint32_t elem_b = 1)
        {
            return tensor_chw(base_addr, out_C, out_H, out_W, elem_b);
        }
    };

    // Pure C++17 DMA 4-D strided / slice / concat / upsample / reshape transfer engine (usable with or without SystemC).
    struct DmaSliceConcat
    {
        static void move_nd(const uint8_t *src, size_t src_cap, const DmaNdDesc &src_desc,
                            uint8_t *dst, size_t dst_cap, const DmaNdDesc &dst_desc)
        {
            const uint64_t L_src = src_desc.length(), L_dst = dst_desc.length();
            if (src_desc.elem_bytes == dst_desc.elem_bytes)
            {
                const uint32_t eb = std::max(1u, src_desc.elem_bytes);
                const uint64_t L = std::min(L_src, L_dst);
                for (uint64_t k = 0; k < L; k++)
                {
                    const uint32_t sa = src_desc.addr_at(k), da = dst_desc.addr_at(k);
                    if (size_t(sa) + eb <= src_cap && size_t(da) + eb <= dst_cap)
                        std::memcpy(dst + da, src + sa, eb);
                }
                return;
            }
            const uint64_t B = std::min<uint64_t>(src_desc.total_bytes(), dst_desc.total_bytes());
            for (uint64_t b = 0; b < B; b++)
            {
                const uint32_t sa = src_desc.byte_at(b), da = dst_desc.byte_at(b);
                if (sa < src_cap && da < dst_cap) dst[da] = src[sa];
            }
        }

        // Slice a [C][H][W] tensor along `chw_axis` (0 = C, 1 = H, 2 = W) into contiguous `dst`.
        static void slice_chw(const uint8_t *src, size_t src_cap, unsigned C, unsigned H, unsigned W,
                              int chw_axis, unsigned offset, unsigned len, unsigned step,
                              uint8_t *dst, size_t dst_cap, uint32_t elem_b = 1)
        {
            const DmaNdDesc sd = DmaNdDesc::tensor_chw(0, C, H, W, elem_b).sliced_chw(chw_axis, offset, len, step);
            const DmaNdDesc dd = DmaNdDesc::linear(0, static_cast<uint32_t>(sd.length()), elem_b);
            move_nd(src, src_cap, sd, dst, dst_cap, dd);
        }

        // Concat a contiguous part tensor into [out_C][out_H][out_W] at `offset` along `chw_axis` (0 = C, 1 = H, 2 = W).
        static void concat_chw(const uint8_t *src, size_t src_cap,
                               uint8_t *dst, size_t dst_cap, unsigned out_C, unsigned out_H, unsigned out_W,
                               int chw_axis, unsigned offset, unsigned part_len, uint32_t elem_b = 1)
        {
            const DmaNdDesc dd = DmaNdDesc::concat_chw(0, out_C, out_H, out_W, chw_axis, offset, part_len, elem_b);
            const DmaNdDesc sd = DmaNdDesc::linear(0, static_cast<uint32_t>(dd.length()), elem_b);
            move_nd(src, src_cap, sd, dst, dst_cap, dd);
        }

        // Nearest-neighbor spatial upsample [C][H][W] -> [C][H*scale_h][W*scale_w] into contiguous `dst`.
        static void upsample_chw(const uint8_t *src, size_t src_cap, unsigned C, unsigned H, unsigned W,
                                 unsigned scale_h, unsigned scale_w,
                                 uint8_t *dst, size_t dst_cap, uint32_t elem_b = 1)
        {
            const DmaNdDesc sd = DmaNdDesc::upsample_chw(0, C, H, W, scale_h, scale_w, elem_b);
            const DmaNdDesc dd = DmaNdDesc::linear(0, static_cast<uint32_t>(sd.length()), elem_b);
            move_nd(src, src_cap, sd, dst, dst_cap, dd);
        }

        // Reshape a contiguous [in_C][in_H][in_W] tensor into [out_C][out_H][out_W] (allows one -1 axis; transpose_chw_hw
        // optionally transposes [in_C][in_H*in_W] -> [in_H*in_W][in_C] while reshaping). Returns false if shapes mismatch.
        static bool reshape_chw(const uint8_t *src, size_t src_cap,
                                unsigned in_C, unsigned in_H, unsigned in_W,
                                int out_C, int out_H, int out_W,
                                uint8_t *dst, size_t dst_cap, uint32_t elem_b = 1, bool transpose_chw_hw = false)
        {
            const uint64_t in_elems = uint64_t(in_C) * in_H * in_W;
            if (in_C == 0 || in_H == 0 || in_W == 0) return false;
            if (!DmaNdDesc::resolve_reshape_dims(in_elems, out_C, out_H, out_W)) return false;
            const uint32_t eb = std::max(1u, elem_b);
            if (!transpose_chw_hw)
            {
                const size_t total_b = static_cast<size_t>(in_elems) * eb;
                if (total_b <= src_cap && total_b <= dst_cap)
                {
                    if (dst != src) std::memmove(dst, src, total_b);
                    return true;
                }
                const DmaNdDesc sd = DmaNdDesc::linear(0, static_cast<uint32_t>(in_elems), eb);
                const DmaNdDesc dd = DmaNdDesc::linear(0, static_cast<uint32_t>(in_elems), eb);
                move_nd(src, src_cap, sd, dst, dst_cap, dd);
                return true;
            }
            const DmaNdDesc sd = DmaNdDesc::tensor_chw(0, in_C, 1u, in_H * in_W, eb).permuted({{ 2, 0, 1, 3 }});
            const DmaNdDesc dd = DmaNdDesc::linear(0, static_cast<uint32_t>(in_elems), eb);
            move_nd(src, src_cap, sd, dst, dst_cap, dd);
            return true;
        }

        // Materialize / reshape any strided or sliced 4-D `src_desc` into a contiguous [out_C][out_H][out_W] buffer.
        static bool reshape_nd(const uint8_t *src, size_t src_cap, const DmaNdDesc &src_desc,
                               int out_C, int out_H, int out_W, uint8_t *dst, size_t dst_cap)
        {
            const uint64_t in_elems = src_desc.length();
            if (!DmaNdDesc::resolve_reshape_dims(in_elems, out_C, out_H, out_W)) return false;
            const DmaNdDesc dd = DmaNdDesc::linear(0, static_cast<uint32_t>(in_elems), std::max(1u, src_desc.elem_bytes));
            move_nd(src, src_cap, src_desc, dst, dst_cap, dd);
            return true;
        }

        // Gather `rows` of 32 int8 elements from `A` and `B` (using DMA ND descriptors) into GVU Scratchpad vbank0 (bank 0) and vbank1 (bank 1).
        template <class Scratchpad>
        static void load_add_operands(Scratchpad &sp, unsigned b_buf, const int8_t *A, const int8_t *B,
                                      size_t n, size_t r0, size_t rows)
        {
            const size_t rem = n > r0 * 32 ? n - r0 * 32 : 0;
            const size_t valid_bytes = std::min(rows * 32, rem);
            std::vector<uint8_t> stage_a(rows * 32, 0), stage_b(((rows + 3) / 4) * 128, 0);
            const DmaNdDesc src_d = DmaNdDesc::linear(static_cast<uint32_t>(r0 * 32), static_cast<uint32_t>(valid_bytes), 1);
            const DmaNdDesc dst_d = DmaNdDesc::linear(0, static_cast<uint32_t>(valid_bytes), 1);
            move_nd(reinterpret_cast<const uint8_t *>(A), n, src_d, stage_a.data(), stage_a.size(), dst_d);
            move_nd(reinterpret_cast<const uint8_t *>(B), n, src_d, stage_b.data(), stage_b.size(), dst_d);
            for (size_t r = 0; r < rows; r++)
            {
                std::vector<uint8_t> ra(stage_a.begin() + r * 32, stage_a.begin() + (r + 1) * 32);
                sp.sram(0, 0).poke(static_cast<unsigned>(r), ra);
            }
            for (size_t k = 0; k < (rows + 3) / 4; k++)
            {
                std::vector<uint8_t> rb(stage_b.begin() + k * 128, stage_b.begin() + (k + 1) * 128);
                sp.sram(1, b_buf).poke(static_cast<unsigned>(k), rb);
            }
        }

        // Scatter `rows` of 32 int8 elements from GVU Scratchpad vbank0 (bank 0) into `out` using DMA ND descriptors.
        template <class Scratchpad>
        static void store_add_result(Scratchpad &sp, int8_t *out, size_t n, size_t r0, size_t rows,
                                     const std::function<void(size_t, int8_t)> &check_fn = nullptr)
        {
            std::vector<uint8_t> stage_o(rows * 32, 0);
            for (size_t r = 0; r < rows; r++)
            {
                const auto orow = sp.sram(0, 0).word(static_cast<unsigned>(r));
                std::memcpy(stage_o.data() + r * 32, orow.data(), std::min<size_t>(32, orow.size()));
            }
            const size_t rem = n > r0 * 32 ? n - r0 * 32 : 0;
            const size_t valid_bytes = std::min(rows * 32, rem);
            if (check_fn)
            {
                for (size_t b = 0; b < valid_bytes; b++)
                    check_fn(r0 * 32 + b, static_cast<int8_t>(stage_o[b]));
            }
            const DmaNdDesc src_d = DmaNdDesc::linear(0, static_cast<uint32_t>(valid_bytes), 1);
            const DmaNdDesc dst_d = DmaNdDesc::linear(static_cast<uint32_t>(r0 * 32), static_cast<uint32_t>(valid_bytes), 1);
            move_nd(stage_o.data(), stage_o.size(), src_d, reinterpret_cast<uint8_t *>(out), n, dst_d);
        }

        // Slice `n` channels `[c0..c0+n][H][W]` from `in[g][H][W]` and scatter into padded `[n][Hp][Wt*32]` in vbank0 buffer 0.
        template <class Scratchpad>
        static void load_pool_operands(Scratchpad &sp, const int8_t *in, int g, int H, int W,
                                       int c0, int n, int p, int Hp, int Wp, int Wt)
        {
            const size_t total_rows = static_cast<size_t>(n) * Hp * Wt;
            std::vector<uint8_t> stage_in(total_rows * 32, 0);
            for (int c = 0; c < n; c++)
                for (int y = 0; y < Hp; y++)
                    std::memset(stage_in.data() + static_cast<size_t>((c * Hp + y) * Wt) * 32, 0x80, static_cast<size_t>(Wp));
            const DmaNdDesc src_d = DmaNdDesc::tile_window_3d(
                0, static_cast<unsigned>(g), static_cast<unsigned>(H), static_cast<unsigned>(W),
                static_cast<unsigned>(c0), static_cast<unsigned>(n), 0u, static_cast<unsigned>(H), 0u, static_cast<unsigned>(W));
            const DmaNdDesc dst_d = DmaNdDesc::tile_writeback_3d(
                0, static_cast<unsigned>(n), static_cast<unsigned>(Hp), static_cast<unsigned>(Wt * 32),
                0u, static_cast<unsigned>(n), static_cast<unsigned>(p), static_cast<unsigned>(H), static_cast<unsigned>(p), static_cast<unsigned>(W));
            move_nd(reinterpret_cast<const uint8_t *>(in), static_cast<size_t>(g) * H * W, src_d,
                    stage_in.data(), stage_in.size(), dst_d);
            for (size_t r = 0; r < total_rows; r++)
            {
                std::vector<uint8_t> row(stage_in.begin() + r * 32, stage_in.begin() + (r + 1) * 32);
                sp.sram(0, 0).poke(static_cast<unsigned>(r), row);
            }
        }

        // Gather `n` channels `[n][Ho][Wo]` from vbank2 port 1 and concat into `res[g][Ho][Wo]` at channel offset `c0`.
        template <class Scratchpad>
        static void store_pool_result(Scratchpad &sp, std::vector<int8_t> &res, int g, int c0, int n,
                                      int Ho, int Wo, int Wt, int OL)
        {
            const size_t total_rows = static_cast<size_t>(n) * Ho * Wt;
            const size_t words128 = (total_rows + 3) / 4;
            std::vector<uint8_t> stage_out(words128 * 128, 0);
            for (size_t w = 0; w < words128; w++)
            {
                const auto wrd = sp.sram(2, 0).word(static_cast<unsigned>(w));
                std::memcpy(stage_out.data() + w * 128, wrd.data(), std::min<size_t>(128, wrd.size()));
            }
            for (int t = 0; t < Wt; t++)
            {
                const int x0 = t * OL;
                if (x0 >= Wo) break;
                const unsigned len_w = static_cast<unsigned>(std::min(OL, Wo - x0));
                DmaNdDesc src_d;
                src_d.base = static_cast<uint32_t>(t * 32);
                src_d.n = {{ len_w, static_cast<unsigned>(Ho), static_cast<unsigned>(n), 1u }};
                src_d.s = {{ 1, int64_t(Wt) * 32, int64_t(Ho) * int64_t(Wt) * 32, 0 }};
                src_d.elem_bytes = 1;
                const DmaNdDesc dst_d = DmaNdDesc::tile_writeback_3d(
                    0, static_cast<unsigned>(g), static_cast<unsigned>(Ho), static_cast<unsigned>(Wo),
                    static_cast<unsigned>(c0), static_cast<unsigned>(n), 0u, static_cast<unsigned>(Ho),
                    static_cast<unsigned>(x0), len_w);
                move_nd(stage_out.data(), stage_out.size(), src_d,
                        reinterpret_cast<uint8_t *>(res.data()), res.size(), dst_d);
            }
        }
    };

    struct DmaStats
    {
        uint64_t read_bytes{0}, write_bytes{0}, read_bursts{0}, write_bursts{0}, busy_cycles{0}, read_port_cycles{0}, write_port_cycles{0};
        uint64_t rd_nd_ops{0}, wr_nd_ops{0}, nd_bytes{0};
        uint64_t upsample_ops{0}, reshape_ops{0};
    };
} // namespace has
#endif // HAS_DMA_DESC_H

#if !defined(HAS_DMA_PURE_CPP) && !defined(HAS_DMA_H)
#define HAS_DMA_H
namespace has
{
    template <class Port>
    class HasDma : public sc_module
    {
    public:
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        DmaParams prm;
        DmaStats st;

        SC_HAS_PROCESS(HasDma);
        HasDma(sc_module_name nm, const DmaParams &p = DmaParams()) : sc_module(nm), prm(p)
        {
            SC_METHOD(process);
            sensitive << i_clk.pos();
        }

        void set_port(Port *p) { port_ = p; }
        void set_dram(std::vector<uint8_t> *d) { dram_ = d; }

        // `timed` (3-D descriptors): bytes that cross the AXI bus when a 3-D descriptor moves only part of `size`
        // (padding filled by the DMA, int8 packed on write-back). 0 = size. The data copy is always `size` bytes.
        void start_read(int ch, uint32_t dram_addr, int bank, uint32_t bank_offset, uint32_t size, uint32_t timed = 0, uint32_t extra = 0)
        {
            if (ch < 0 || ch > 3) return;
            Chan c;
            c.active = true; c.dram_addr = dram_addr; c.bank = bank; c.bank_offset = bank_offset;
            c.size = size; c.timed = clamp_timed(size, timed); c.extra = extra;
            rd_[ch] = c;
            st.read_bytes += rd_[ch].timed;
        }
        void start_write(uint32_t dram_addr, int bank, uint32_t bank_offset, uint32_t size, uint32_t timed = 0)
        {
            Chan c;
            c.active = true; c.dram_addr = dram_addr; c.bank = bank; c.bank_offset = bank_offset;
            c.size = size; c.timed = clamp_timed(size, timed);
            wr_ = c;
            st.write_bytes += wr_.timed;
        }

        // Multi-dimensional (4-D strided / slice / concat) DMA read: gathers from DRAM according to `dram_desc`
        // and writes into SRAM `bank` according to `sram_desc` (or linearly starting at `bank_offset`).
        void start_read_nd(int ch, const DmaNdDesc &dram_desc, int bank, const DmaNdDesc &sram_desc, uint32_t timed = 0)
        {
            if (ch < 0 || ch > 3) return;
            const uint32_t sz = std::min(dram_desc.total_bytes(), sram_desc.total_bytes());
            Chan c;
            c.active = true; c.dram_addr = dram_desc.base; c.bank = bank; c.bank_offset = sram_desc.base;
            c.size = sz; c.timed = clamp_timed(sz, timed);
            c.is_nd = true; c.dram_nd = dram_desc; c.sram_nd = sram_desc;
            rd_[ch] = c;
            st.read_bytes += rd_[ch].timed;
            st.rd_nd_ops++;
            st.nd_bytes += rd_[ch].timed;
        }
        void start_read_nd(int ch, const DmaNdDesc &dram_desc, int bank, uint32_t bank_offset = 0, uint32_t timed = 0)
        {
            start_read_nd(ch, dram_desc, bank, DmaNdDesc::linear(bank_offset, dram_desc.total_bytes(), 1), timed);
        }

        // Multi-dimensional (4-D strided / slice / concat) DMA write: reads from SRAM `bank` according to `sram_desc`
        // and scatters/concatenates into DRAM according to `dram_desc`.
        void start_write_nd(const DmaNdDesc &dram_desc, int bank, const DmaNdDesc &sram_desc, uint32_t timed = 0)
        {
            const uint32_t sz = std::min(dram_desc.total_bytes(), sram_desc.total_bytes());
            Chan c;
            c.active = true; c.dram_addr = dram_desc.base; c.bank = bank; c.bank_offset = sram_desc.base;
            c.size = sz; c.timed = clamp_timed(sz, timed);
            c.is_nd = true; c.dram_nd = dram_desc; c.sram_nd = sram_desc;
            wr_ = c;
            st.write_bytes += wr_.timed;
            st.wr_nd_ops++;
            st.nd_bytes += wr_.timed;
        }
        void start_write_nd(const DmaNdDesc &dram_desc, int bank, uint32_t bank_offset = 0, uint32_t timed = 0)
        {
            start_write_nd(dram_desc, bank, DmaNdDesc::linear(bank_offset, dram_desc.total_bytes(), 1), timed);
        }

        // Multi-dimensional tile write-back from SRAM-C (bank 4, int32 words -> int8 scatter/concat in DRAM):
        // Reads `swords * 128` bytes from SRAM-C into `stage_out` on the clock and scatters the valid int8 elements
        // `[real_c][ht][real_w]` into `out_addr` using the 3-D concat/write-back descriptor `out_nd`.
        void start_write_sramc_nd(uint32_t stage_out, int bank, uint32_t bank_offset, uint32_t size, uint32_t timed,
                                  const DmaNdDesc &out_nd, uint32_t nch, uint32_t ht, uint32_t wt, uint32_t real_c, uint32_t real_w)
        {
            Chan c;
            c.active = true; c.dram_addr = stage_out; c.bank = bank; c.bank_offset = bank_offset;
            c.size = size; c.timed = clamp_timed(size, timed);
            c.is_sramc_scatter = true; c.dram_nd = out_nd;
            c.sc_nch = nch; c.sc_ht = ht; c.sc_wt = wt; c.sc_real_c = real_c; c.sc_real_w = real_w;
            wr_ = c;
            st.write_bytes += wr_.timed;
            st.wr_nd_ops++;
            st.nd_bytes += wr_.timed;
        }
        void start_write_sramc_nd(uint32_t stage_out, int bank, uint32_t bank_offset, uint32_t size,
                                  const DmaNdDesc &out_nd, uint32_t nch, uint32_t ht, uint32_t wt, uint32_t timed = 0)
        {
            start_write_sramc_nd(stage_out, bank, bank_offset, size, timed, out_nd, nch, ht, wt, out_nd.n[2], out_nd.n[0]);
        }

        // Synchronous/immediate 4-D strided move between any two byte buffers (used for staging 3-D windows and GVU Scratchpad DMA).
        static void move_nd(const uint8_t *src, size_t src_cap, const DmaNdDesc &src_desc,
                            uint8_t *dst, size_t dst_cap, const DmaNdDesc &dst_desc)
        {
            const uint64_t L_src = src_desc.length(), L_dst = dst_desc.length();
            if (src_desc.elem_bytes == dst_desc.elem_bytes)
            {
                const uint32_t eb = std::max(1u, src_desc.elem_bytes);
                const uint64_t L = std::min(L_src, L_dst);
                for (uint64_t k = 0; k < L; k++)
                {
                    const uint32_t sa = src_desc.addr_at(k), da = dst_desc.addr_at(k);
                    if (size_t(sa) + eb <= src_cap && size_t(da) + eb <= dst_cap)
                        std::memcpy(dst + da, src + sa, eb);
                }
                return;
            }
            const uint64_t B = std::min<uint64_t>(src_desc.total_bytes(), dst_desc.total_bytes());
            for (uint64_t b = 0; b < B; b++)
            {
                const uint32_t sa = src_desc.byte_at(b), da = dst_desc.byte_at(b);
                if (sa < src_cap && da < dst_cap) dst[da] = src[sa];
            }
        }

        // Gather/slice within the bound DRAM image using 4-D descriptors (e.g. slicing a 3-D tile window into staging memory).
        void gather_dram_nd(const DmaNdDesc &src_desc, const DmaNdDesc &dst_desc, bool count_op = true)
        {
            if (!dram_) return;
            move_nd(dram_->data(), dram_->size(), src_desc, dram_->data(), dram_->size(), dst_desc);
            if (count_op)
            {
                st.rd_nd_ops++;
                st.nd_bytes += std::min(src_desc.total_bytes(), dst_desc.total_bytes());
            }
        }

        // Nearest-neighbor spatial upsample [C][H][W] -> [C][H*scale_h][W*scale_w] within bound DRAM.
        void upsample_dram(uint32_t src_addr, uint32_t dst_addr, unsigned C, unsigned H, unsigned W,
                           unsigned scale_h = 2, unsigned scale_w = 2, uint32_t elem_b = 1)
        {
            if (!dram_) return;
            const DmaNdDesc sd = DmaNdDesc::upsample_chw(src_addr, C, H, W, scale_h, scale_w, elem_b);
            const DmaNdDesc dd = DmaNdDesc::linear(dst_addr, static_cast<uint32_t>(sd.length()), elem_b);
            gather_dram_nd(sd, dd, true);
            st.upsample_ops++;
        }

        // Reshape [in_C][in_H][in_W] -> [out_C][out_H][out_W] within bound DRAM (returns false on shape mismatch).
        bool reshape_dram(uint32_t src_addr, uint32_t dst_addr,
                          unsigned in_C, unsigned in_H, unsigned in_W,
                          int out_C, int out_H, int out_W,
                          uint32_t elem_b = 1, bool transpose_chw_hw = false)
        {
            if (!dram_ || in_C == 0 || in_H == 0 || in_W == 0) return false;
            const uint64_t elems = uint64_t(in_C) * in_H * in_W;
            if (!DmaNdDesc::resolve_reshape_dims(elems, out_C, out_H, out_W)) return false;
            const uint32_t eb = std::max(1u, elem_b);
            const DmaNdDesc sd = transpose_chw_hw
                                     ? DmaNdDesc::tensor_chw(src_addr, in_C, 1u, in_H * in_W, eb).permuted({{ 2, 0, 1, 3 }})
                                     : DmaNdDesc::linear(src_addr, static_cast<uint32_t>(elems), eb);
            const DmaNdDesc dd = DmaNdDesc::linear(dst_addr, static_cast<uint32_t>(elems), eb);
            gather_dram_nd(sd, dd, true);
            st.reshape_ops++;
            return true;
        }

        bool is_read_active(int ch) const { return ch >= 0 && ch < 4 && rd_[ch].active; }
        bool is_write_active() const { return wr_.active; }
        bool is_any_read_active() const { return rd_[0].active || rd_[1].active || rd_[2].active || rd_[3].active; }

    private:
        struct Chan
        {
            bool active{false};
            uint32_t dram_addr{0};
            int bank{0};
            uint32_t bank_offset{0}, size{0}, done{0};
            bool first{true};   // first burst still to issue -> pays dram_latency
            uint32_t extra{0};  // edges added to the first burst (HAS_DFC_CORE_DMA: the 1-D commands of the DFC), 0 = none
            uint32_t timed{0}, tdone{0};   // bus bytes (<= size) and bus bytes done
            bool is_nd{false};
            bool is_sramc_scatter{false};
            DmaNdDesc dram_nd{}, sram_nd{};
            uint32_t sc_nch{0}, sc_ht{0}, sc_wt{0}, sc_real_c{0}, sc_real_w{0};
        };
        static uint32_t clamp_timed(uint32_t size, uint32_t timed) { return (timed == 0 || timed > size) ? size : (timed ? timed : 1); }
        Chan rd_[4], wr_;
        int rr_{0}, cur_rd_{-1};
        uint32_t rd_left_{0}, wr_left_{0};
        Port *port_{nullptr};
        std::vector<uint8_t> *dram_{nullptr};

        void scatter_sramc_tile(const Chan &c)
        {
            if (!dram_) return;
            const uint32_t npos = c.sc_ht * c.sc_wt;
            const uint32_t max_elems = c.size / 4u;
            const uint32_t out_h = c.dram_nd.s[1] > 0 && c.dram_nd.s[2] > 0 ? uint32_t(c.dram_nd.s[2] / c.dram_nd.s[1]) : c.sc_ht;
            const uint32_t out_w = c.dram_nd.s[1] > 0 ? uint32_t(c.dram_nd.s[1]) : c.sc_wt;
            for (uint32_t x = 0; x < c.sc_real_c; x++)
                for (uint32_t y = 0; y < c.sc_ht; y++)
                    for (uint32_t xx = 0; xx < c.sc_real_w; xx++)
                    {
                        const uint32_t p = y * c.sc_wt + xx;
                        if (uint64_t(x) * npos + p >= max_elems) continue;
                        int32_t val;
                        std::memcpy(&val, &(*dram_)[c.dram_addr + 4u * (x * npos + p)], 4);
                        const uint32_t dst = c.dram_nd.base + (x * out_h + y) * out_w + xx;
                        if (dst < dram_->size())
                            (*dram_)[dst] = static_cast<uint8_t>(static_cast<int8_t>(val));
                    }
        }

        void process()
        {
            if (!i_rstn.read())
            {
                for (auto &c : rd_) c = Chan();
                wr_ = Chan();
                rr_ = 0; cur_rd_ = -1; rd_left_ = wr_left_ = 0;
                return;
            }
            if (rd_left_ || wr_left_ || is_any_read_active() || is_write_active()) st.busy_cycles++;
            if (rd_left_) st.read_port_cycles++;
            if (wr_left_) st.write_port_cycles++;

            // 1. write port (before read: DRAM RAW)
            if (wr_left_ > 0 && --wr_left_ == 0 && wr_.active)
            {
                const uint32_t nt = std::min(wr_.timed - wr_.tdone, prm.burst_bytes());
                wr_.tdone += nt;
                const uint32_t n = wr_.tdone >= wr_.timed ? wr_.size - wr_.done : std::min(wr_.size - wr_.done, nt);
                if (dram_ && port_ && n)
                {
                    if (wr_.is_nd)
                    {
                        for (uint32_t i = 0; i < n; i++)
                        {
                            const uint32_t sram_a = wr_.sram_nd.byte_at(wr_.done + i);
                            const uint32_t dram_a = wr_.dram_nd.byte_at(wr_.done + i);
                            if (dram_a + 1 > dram_->size()) dram_->resize(size_t(dram_a) + 1, 0);
                            uint8_t b = 0;
                            port_->read(wr_.bank, sram_a, &b, 1);
                            (*dram_)[dram_a] = b;
                        }
                    }
                    else
                    {
                        const uint32_t dst = wr_.dram_addr + wr_.done;
                        if (dst + n > dram_->size()) dram_->resize(size_t(dst) + n, 0);
                        port_->read(wr_.bank, wr_.bank_offset + wr_.done, &(*dram_)[dst], n);
                    }
                }
                wr_.done += n;
                st.write_bursts++;
                if (wr_.tdone >= wr_.timed)
                {
                    if (wr_.is_sramc_scatter) scatter_sramc_tile(wr_);
                    wr_.active = false;
                }
            }
            if (wr_left_ == 0 && wr_.active)
            {
                wr_left_ = prm.burst_beats + (wr_.first ? prm.dram_latency : 0);
                wr_.first = false;
            }

            // 2. read port
            if (rd_left_ > 0 && --rd_left_ == 0)
            {
                const int ch = cur_rd_;
                if (ch >= 0 && rd_[ch].active)
                {
                    Chan &c = rd_[ch];
                    const uint32_t nt = std::min(c.timed - c.tdone, prm.burst_bytes());
                    c.tdone += nt;
                    const uint32_t n = c.tdone >= c.timed ? c.size - c.done : std::min(c.size - c.done, nt);
                    if (dram_ && port_ && n)
                    {
                        if (c.is_nd)
                        {
                            for (uint32_t i = 0; i < n; i++)
                            {
                                const uint32_t dram_a = c.dram_nd.byte_at(c.done + i);
                                const uint32_t sram_a = c.sram_nd.byte_at(c.done + i);
                                const uint8_t b = dram_a < dram_->size() ? (*dram_)[dram_a] : 0;
                                port_->write(c.bank, sram_a, &b, 1);
                            }
                        }
                        else
                        {
                            const uint32_t src = c.dram_addr + c.done;
                            if (src + n <= dram_->size()) port_->write(c.bank, c.bank_offset + c.done, &(*dram_)[src], n);
                            else if (src < dram_->size())
                                port_->write(c.bank, c.bank_offset + c.done, &(*dram_)[src], uint32_t(dram_->size() - src));
                        }
                    }
                    c.done += n;
                    st.read_bursts++;
                    if (c.tdone >= c.timed) c.active = false;
                }
                cur_rd_ = -1;
            }
            if (rd_left_ == 0)
            {
                int sel = -1;
                const int n_hi = prm.ch3_low_priority ? 3 : 4;
                for (int i = 0; i < 4 && sel < 0; i++)
                {
                    const int ch = (rr_ + i) % 4;
                    if (ch < n_hi && rd_[ch].active) sel = ch;
                }
                if (sel < 0 && prm.ch3_low_priority && rd_[3].active) sel = 3;
                if (sel >= 0)
                {
                    if (sel < n_hi) rr_ = (sel + 1) % 4;
                    cur_rd_ = sel;
                    rd_left_ = prm.burst_beats + (rd_[sel].first ? prm.dram_latency + rd_[sel].extra : 0);
                    rd_[sel].first = false;
                }
            }
        }
    };
} // namespace has
#endif // !HAS_DMA_PURE_CPP && !HAS_DMA_H
