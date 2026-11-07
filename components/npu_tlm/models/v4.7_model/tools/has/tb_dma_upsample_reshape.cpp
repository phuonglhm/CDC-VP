// tb_dma_upsample_reshape.cpp -- Standalone C++17 verification suite for DMA-handled upsample & reshape across:
//   1. DmaNdDesc & DmaSliceConcat (has/has_dma.h, pure C++17 mode) + DmaDfc1d cost/run check
//   2. MmioCompat ISA decode for ELEM_WISE (0x15 modes 8/9) and DMA_TENSOR (0x16 modes 2/3 & 8/9)
//   3. (only with -DHAS_TB_NEO_CSR, needs has/rtl_ref_dfc_neo.h of the colleague's tree, not part of this model) NeoTopCsrSubsystem: DmaNeoModel
//      extension CSRs (0x30040..0x30050) + DfcNeoModel opcodes 7 & 8
//   4. Optional `--emit-program <dir> [--with-transpose]` generator of a register program (mmio.txt + DRAM images) for HasNpuTop
//      (tb_has_npu_top, HAS_DMA_RESHAPE=1) and for tools/has/ref_program.py; the two corner-turn reshapes only with --with-transpose
//      (the model refuses them unless HAS_DMA_RESHAPE_T=1)
//
// Build & run:
//   g++ -std=c++17 -O2 -Wall -I. tools/has/tb_dma_upsample_reshape.cpp -o /tmp/tb_dma_upsample_reshape
//   /tmp/tb_dma_upsample_reshape

#define HAS_DMA_PURE_CPP
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "has/has_dma.h"
#include "has/dma_dfc_1d.h"
#include "has/has_mmio_compat.h"
#ifdef HAS_TB_NEO_CSR
#include "has/rtl_ref_dfc_neo.h"
#endif

namespace
{
    int g_pass = 0;
    int g_fail = 0;

    constexpr uint32_t ext_addr(has::mmio::Ext x)
    {
        return has::mmio::EXT_BASE + 4u * static_cast<uint32_t>(x);
    }

    void check(bool cond, const char *name, const char *detail = "")
    {
        if (cond)
        {
            g_pass++;
        }
        else
        {
            g_fail++;
            std::fprintf(stderr, "[FAIL] %s %s\n", name, detail);
        }
    }

    // Golden CPU nearest-neighbor upsample [C][H][W] -> [C][H*sh][W*sw]
    std::vector<uint8_t> golden_upsample(const std::vector<uint8_t> &src, unsigned C, unsigned H, unsigned W,
                                         unsigned sh, unsigned sw, uint32_t eb = 1)
    {
        const unsigned oh = H * sh, ow = W * sw;
        std::vector<uint8_t> dst(size_t(C) * oh * ow * eb, 0);
        for (unsigned c = 0; c < C; c++)
            for (unsigned y = 0; y < oh; y++)
                for (unsigned x = 0; x < ow; x++)
                {
                    const size_t si = (size_t(c) * H + (y / sh)) * W + (x / sw);
                    const size_t di = (size_t(c) * oh + y) * ow + x;
                    std::memcpy(dst.data() + di * eb, src.data() + si * eb, eb);
                }
        return dst;
    }

    // Golden CPU [C][H*W] -> [H*W][C] corner-turn transpose
    std::vector<uint8_t> golden_transpose_chw_hw(const std::vector<uint8_t> &src, unsigned C, unsigned H, unsigned W, uint32_t eb = 1)
    {
        const unsigned hw = H * W;
        std::vector<uint8_t> dst(size_t(C) * hw * eb, 0);
        for (unsigned p = 0; p < hw; p++)
            for (unsigned c = 0; c < C; c++)
                std::memcpy(dst.data() + size_t(p * C + c) * eb, src.data() + size_t(c * hw + p) * eb, eb);
        return dst;
    }

    void test_dma_nd_desc_and_engine()
    {
        using namespace has;

        // U1: YOLOv8-style 2x2 nearest-neighbor upsample [16][4][5] -> [16][8][10]
        {
            const unsigned C = 16, H = 4, W = 5, sh = 2, sw = 2;
            std::vector<uint8_t> src(C * H * W);
            for (size_t i = 0; i < src.size(); i++) src[i] = uint8_t((i * 37 + 11) & 0xFF);
            std::vector<uint8_t> dst(C * H * sh * W * sw, 0xAA);
            DmaSliceConcat::upsample_chw(src.data(), src.size(), C, H, W, sh, sw, dst.data(), dst.size(), 1);
            const auto gold = golden_upsample(src, C, H, W, sh, sw, 1);
            check(dst == gold, "U1: DmaSliceConcat::upsample_chw 2x2 [16][4][5]->[16][8][10]");

            const DmaNdDesc sd = DmaNdDesc::upsample_chw(0x1000, C, H, W, sh, sw, 1);
            const DmaNdDesc dd = DmaNdDesc::linear(0x4000, uint32_t(dst.size()), 1);
            check(sd.length() == dst.size(), "U1b: upsample_chw 4D descriptor exact element length");
            check(DmaDfc1d::cost(sd, dd) > 0, "U1c: DmaDfc1d::cost positive on 2x2 upsample descriptor");
        }

        // U2: Asymmetric 3x2 upsample [3][4][5] -> [3][12][10] + multi-byte elem_b = 2
        {
            const unsigned C = 3, H = 4, W = 5, sh = 3, sw = 2;
            std::vector<uint8_t> src(C * H * W * 2);
            for (size_t i = 0; i < src.size(); i++) src[i] = uint8_t((i * 19 + 5) & 0xFF);
            std::vector<uint8_t> dst(C * H * sh * W * sw * 2, 0);
            DmaSliceConcat::upsample_chw(src.data(), src.size(), C, H, W, sh, sw, dst.data(), dst.size(), 2);
            const auto gold = golden_upsample(src, C, H, W, sh, sw, 2);
            check(dst == gold, "U2: DmaSliceConcat::upsample_chw asymmetric 3x2 (elem_b=2)");
        }

        // U3: Vertical-only upsample (scale_h=4, scale_w=1) keeps contiguous W-byte row runs
        {
            const unsigned C = 2, H = 3, W = 7, sh = 4, sw = 1;
            std::vector<uint8_t> src(C * H * W);
            for (size_t i = 0; i < src.size(); i++) src[i] = uint8_t(i + 1);
            std::vector<uint8_t> dst(C * H * sh * W, 0);
            DmaSliceConcat::upsample_chw(src.data(), src.size(), C, H, W, sh, sw, dst.data(), dst.size(), 1);
            const auto gold = golden_upsample(src, C, H, W, sh, sw, 1);
            const DmaNdDesc sd = DmaNdDesc::upsample_chw(0, C, H, W, sh, sw, 1);
            check(dst == gold, "U3: DmaSliceConcat::upsample_chw 4x1 vertical replication");
            check(DmaDfc1d::run_of(sd) == W, "U3b: scale_w=1 preserves contiguous W-byte burst run");
        }

        // U4: Identity 1x1 upsample collapses to linear 1-D descriptor
        {
            const DmaNdDesc sd = DmaNdDesc::upsample_chw(0x200, 4, 3, 5, 1, 1, 1);
            check(sd.n[0] == 60 && sd.n[1] == 1 && sd.s[0] == 1, "U4: 1x1 upsample_chw collapses to linear descriptor");
        }

        // U5: Horizontal-only upsample (1x5), 1x1 spatial broadcast ([8][1][1]->[8][7][5]), and elem_b=4 (FP32/INT32)
        {
            std::vector<uint8_t> src_h(3 * 4 * 6);
            for (size_t i = 0; i < src_h.size(); i++) src_h[i] = uint8_t((i * 41 + 9) & 0xFF);
            std::vector<uint8_t> dst_h(3 * 4 * 30, 0);
            DmaSliceConcat::upsample_chw(src_h.data(), src_h.size(), 3, 4, 6, 1, 5, dst_h.data(), dst_h.size(), 1);
            check(dst_h == golden_upsample(src_h, 3, 4, 6, 1, 5, 1), "U5a: horizontal-only 1x5 upsample_chw [3][4][6]->[3][4][30]");

            std::vector<uint8_t> src_1x1(8);
            for (size_t i = 0; i < src_1x1.size(); i++) src_1x1[i] = uint8_t(0x10 + i * 17);
            std::vector<uint8_t> dst_1x1(8 * 7 * 5, 0);
            DmaSliceConcat::upsample_chw(src_1x1.data(), src_1x1.size(), 8, 1, 1, 7, 5, dst_1x1.data(), dst_1x1.size(), 1);
            check(dst_1x1 == golden_upsample(src_1x1, 8, 1, 1, 7, 5, 1), "U5b: 1x1 spatial upsample_chw [8][1][1]->[8][7][5]");

            std::vector<uint8_t> src_eb4(2 * 5 * 7 * 4);
            for (size_t i = 0; i < src_eb4.size(); i++) src_eb4[i] = uint8_t((i * 53 + 3) & 0xFF);
            std::vector<uint8_t> dst_eb4(2 * 10 * 21 * 4, 0);
            DmaSliceConcat::upsample_chw(src_eb4.data(), src_eb4.size(), 2, 5, 7, 2, 3, dst_eb4.data(), dst_eb4.size(), 4);
            check(dst_eb4 == golden_upsample(src_eb4, 2, 5, 7, 2, 3, 4), "U5c: 32-bit (elem_b=4) upsample_chw [2][5][7]->[2][10][21]");
        }

        // U6: Chunked double-buffered DMA streaming simulation (mirrors HasNpuTop::nd_sub + run_dma_nd_op bank-4 staging)
        {
            auto nd_sub = [](const DmaNdDesc &d, uint64_t k0, uint64_t cnt, int lv) {
                DmaNdDesc r = d;
                uint64_t below = 1, q = k0;
                int64_t off = 0;
                for (int j = 0; j < lv; j++) below *= std::max(1u, d.n[j]);
                for (int j = 0; j < 4; j++) { const uint64_t nj = std::max(1u, d.n[j]); off += int64_t(q % nj) * d.s[j]; q /= nj; }
                r.base = uint32_t(int64_t(d.base) + off * int64_t(std::max(1u, d.elem_bytes)));
                r.n[lv] = unsigned(cnt / below);
                for (int j = lv + 1; j < 4; j++) r.n[j] = 1;
                return r;
            };
            auto run_chunked = [&](const std::vector<uint8_t> &src, const DmaNdDesc &sd, uint64_t chunk_b) {
                const uint64_t L = sd.length();
                const DmaNdDesc dd = DmaNdDesc::linear(0, uint32_t(L), 1);
                std::vector<uint8_t> dst(L, 0xCC), fifo_ping_pong(2 * size_t(chunk_b), 0);
                int lv = 0;
                uint64_t per = 1;
                while (lv < 3 && per * std::max(1u, sd.n[lv]) <= chunk_b) { per *= std::max(1u, sd.n[lv]); lv++; }
                const uint64_t nlv = std::max(1u, sd.n[lv]), m = std::max<uint64_t>(1, chunk_b / per);
                uint32_t piece = 0;
                for (uint64_t k = 0; k < L; piece++)
                {
                    const uint64_t cnt = std::min(m, nlv - (k / per) % nlv) * per;
                    const DmaNdDesc a = nd_sub(sd, k, cnt, lv), b = nd_sub(dd, k, cnt, 0);
                    const DmaNdDesc stage = DmaNdDesc::linear((piece % 2u) * uint32_t(chunk_b), uint32_t(cnt), 1);
                    DmaSliceConcat::move_nd(src.data(), src.size(), a, fifo_ping_pong.data(), fifo_ping_pong.size(), stage);
                    DmaSliceConcat::move_nd(fifo_ping_pong.data(), fifo_ping_pong.size(), stage, dst.data(), dst.size(), b);
                    k += cnt;
                }
                return dst;
            };

            // YOLOv8m neck scale: [64][20][20] (25.6 KB) -> [64][40][40] (102.4 KB > 64 KB SRAM-A) with 1,024 B chunks
            std::vector<uint8_t> yolo_in(64 * 20 * 20);
            for (size_t i = 0; i < yolo_in.size(); i++) yolo_in[i] = uint8_t((i * 131 + 17) & 0xFF);
            const auto yolo_got = run_chunked(yolo_in, DmaNdDesc::upsample_chw(0, 64, 20, 20, 2, 2, 1), 1024);
            check(yolo_got == golden_upsample(yolo_in, 64, 20, 20, 2, 2, 1),
                  "U6a: chunked (1024 B) YOLOv8 neck upsample [64][20][20]->[64][40][40] (102.4 KB > SRAM-A)");

            // Prime dimensions [7][13][19] -> [7][39][38] across weird chunk sizes (16, 64, 257 B)
            std::vector<uint8_t> prime_in(7 * 13 * 19);
            for (size_t i = 0; i < prime_in.size(); i++) prime_in[i] = uint8_t((i * 73 + 29) & 0xFF);
            const auto prime_gold = golden_upsample(prime_in, 7, 13, 19, 3, 2, 1);
            bool all_chunks_ok = true;
            for (uint64_t ch : { 16ull, 64ull, 257ull })
                if (run_chunked(prime_in, DmaNdDesc::upsample_chw(0, 7, 13, 19, 3, 2, 1), ch) != prime_gold) all_chunks_ok = false;
            check(all_chunks_ok, "U6b: chunked (16, 64, 257 B) prime upsample [7][13][19]->[7][39][38]");

            // Prime corner-turn reshape [11][13][17] -> [221][1][11] across 257 B chunks
            std::vector<uint8_t> ct_in(11 * 13 * 17);
            for (size_t i = 0; i < ct_in.size(); i++) ct_in[i] = uint8_t((i * 97 + 5) & 0xFF);
            const DmaNdDesc ct_sd = DmaNdDesc::tensor_chw(0, 11, 1, 13 * 17, 1).permuted({{ 2, 0, 1, 3 }});
            check(run_chunked(ct_in, ct_sd, 257) == golden_transpose_chw_hw(ct_in, 11, 13, 17, 1),
                  "U6c: chunked (257 B) prime corner-turn reshape [11][13][17]->[221][1][11]");
        }

        // U7: 200-case deterministic pseudo-random upsample fuzz suite
        {
            uint32_t rng = 0xC0FFEE11u;
            auto next_u = [&]() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
            bool fuzz_ok = true;
            for (int t = 0; t < 200 && fuzz_ok; t++)
            {
                const unsigned C = 1 + (next_u() % 8), H = 1 + (next_u() % 9), W = 1 + (next_u() % 11);
                const unsigned sh = 1 + (next_u() % 5), sw = 1 + (next_u() % 5);
                const uint32_t eb = (1u << (next_u() % 3)); // 1, 2, or 4
                std::vector<uint8_t> s(size_t(C) * H * W * eb);
                for (size_t i = 0; i < s.size(); i++) s[i] = uint8_t(next_u() & 0xFF);
                std::vector<uint8_t> d(size_t(C) * H * sh * W * sw * eb, 0x5A);
                DmaSliceConcat::upsample_chw(s.data(), s.size(), C, H, W, sh, sw, d.data(), d.size(), eb);
                if (d != golden_upsample(s, C, H, W, sh, sw, eb)) fuzz_ok = false;
            }
            check(fuzz_ok, "U7: 200 random upsample_chw cases (C=1..8, H=1..9, W=1..11, sh/sw=1..5, elem_b=1/2/4)");
        }

        // R1: Reshape dimension inference (-1) & element conservation checks
        {
            int oc = -1, oh = 8, ow = 8;
            check(DmaNdDesc::resolve_reshape_dims(192, oc, oh, ow) && oc == 3, "R1a: resolve_reshape_dims infers OUT_C=-1 -> 3");
            oc = 3; oh = -1; ow = 8;
            check(DmaNdDesc::resolve_reshape_dims(192, oc, oh, ow) && oh == 8, "R1b: resolve_reshape_dims infers OUT_H=-1 -> 8");
            oc = 6; oh = 4; ow = -1;
            check(DmaNdDesc::resolve_reshape_dims(192, oc, oh, ow) && ow == 8, "R1c: resolve_reshape_dims infers OUT_W=-1 -> 8");
            oc = -1; oh = -1; ow = 8;
            check(!DmaNdDesc::resolve_reshape_dims(192, oc, oh, ow), "R1d: rejects two -1 dimensions");
            oc = 5; oh = -1; ow = 8;
            check(!DmaNdDesc::resolve_reshape_dims(192, oc, oh, ow), "R1e: rejects non-divisible -1 dimension");
            oc = 4; oh = 4; ow = 4;
            check(!DmaNdDesc::resolve_reshape_dims(192, oc, oh, ow), "R1f: rejects mismatched element count");
        }

        // R2: Contiguous reshape + corner-turn transpose reshape + sliced-ND reshape
        {
            std::vector<uint8_t> src(4 * 3 * 5);
            for (size_t i = 0; i < src.size(); i++) src[i] = uint8_t(0x80u ^ (i * 7));
            std::vector<uint8_t> dst(src.size(), 0);
            check(DmaSliceConcat::reshape_chw(src.data(), src.size(), 4, 3, 5, -1, 2, 6, dst.data(), dst.size(), 1, false),
                  "R2a: reshape_chw [4][3][5] -> [5][2][6] returns true");
            check(dst == src, "R2b: C-order reshape_chw preserves exact stream order");

            // In-place C-order reshape
            check(DmaSliceConcat::reshape_chw(dst.data(), dst.size(), 5, 2, 6, 10, -1, 2, dst.data(), dst.size(), 1, false) && dst == src,
                  "R2c: in-place C-order reshape_chw succeeds");

            // Corner-turn transpose during reshape: [4][3][5] -> [15][1][4]
            std::vector<uint8_t> dst_tr(src.size(), 0);
            check(DmaSliceConcat::reshape_chw(src.data(), src.size(), 4, 3, 5, 15, 1, 4, dst_tr.data(), dst_tr.size(), 1, true),
                  "R2d: reshape_chw with transpose_chw_hw=true returns true");
            check(dst_tr == golden_transpose_chw_hw(src, 4, 3, 5, 1),
                  "R2e: reshape_chw with transpose_chw_hw matches [C][H*W]->[H*W][C] golden");

            // Materialize & reshape a sliced 4-D view [2][3][5] -> [6][5][1]
            const DmaNdDesc sliced = DmaNdDesc::tensor_chw(0, 4, 3, 5, 1).sliced_chw(0, 1, 2, 1);
            std::vector<uint8_t> dst_nd(2 * 3 * 5, 0);
            check(DmaSliceConcat::reshape_nd(src.data(), src.size(), sliced, 6, -1, 1, dst_nd.data(), dst_nd.size()),
                  "R2f: reshape_nd materializes sliced tensor into [6][5][1]");
            check(std::memcmp(dst_nd.data(), src.data() + 15, 30) == 0,
                  "R2g: reshape_nd matches channel slice [1..3][3][5]");
        }

        // R3: Overflow / invalid dim guards, multi-byte corner-turn (elem_b=4), and round-trip corner-turn identity
        {
            int oc = 1, oh = 1, ow = 1;
            check(!DmaNdDesc::resolve_reshape_dims(0, oc, oh, ow), "R3a: rejects in_elems == 0");
            oc = -2; oh = 4; ow = 4;
            check(!DmaNdDesc::resolve_reshape_dims(32, oc, oh, ow), "R3b: rejects dim < -1 (-2)");
            oc = 0; oh = -1; ow = 4;
            check(!DmaNdDesc::resolve_reshape_dims(32, oc, oh, ow), "R3c: rejects zero output dimension");
            oc = 1000000000; oh = 1000000000; ow = 1000000000;
            const uint64_t wrapped = uint64_t(1000000000ull) * uint64_t(1000000000ull) * uint64_t(1000000000ull);
            check(!DmaNdDesc::resolve_reshape_dims(wrapped, oc, oh, ow), "R3d: rejects 64-bit product wrap-around overflow");

            std::vector<uint8_t> s4(5 * 3 * 4 * 4), mid4(5 * 3 * 4 * 4, 0), back4(5 * 3 * 4 * 4, 0);
            for (size_t i = 0; i < s4.size(); i++) s4[i] = uint8_t((i * 29 + 13) & 0xFF);
            check(DmaSliceConcat::reshape_chw(s4.data(), s4.size(), 5, 3, 4, 12, 1, 5, mid4.data(), mid4.size(), 4, true) &&
                  mid4 == golden_transpose_chw_hw(s4, 5, 3, 4, 4),
                  "R3e: 32-bit (elem_b=4) corner-turn reshape_chw [5][3][4]->[12][1][5]");
            check(DmaSliceConcat::reshape_chw(mid4.data(), mid4.size(), 12, 1, 5, 5, 3, 4, back4.data(), back4.size(), 4, true) &&
                  back4 == s4,
                  "R3f: round-trip double corner-turn [5][3][4]->[12][1][5]->[5][3][4] recovers exact tensor");
        }

        // R4: Chained YOLOv8 FPN/head pipeline (slice -> upsample -> concat -> corner-turn reshape) + 200-case random reshape fuzz
        {
            std::vector<uint8_t> neck_in(6 * 4 * 4), skip_in(2 * 8 * 8);
            for (size_t i = 0; i < neck_in.size(); i++) neck_in[i] = uint8_t((i * 17 + 1) & 0xFF);
            for (size_t i = 0; i < skip_in.size(); i++) skip_in[i] = uint8_t((i * 31 + 0x80) & 0xFF);

            std::vector<uint8_t> sliced(4 * 4 * 4, 0), up(4 * 8 * 8, 0), cat(6 * 8 * 8, 0), head_tmaj(6 * 8 * 8, 0);
            DmaSliceConcat::slice_chw(neck_in.data(), neck_in.size(), 6, 4, 4, 0, 2, 4, 1, sliced.data(), sliced.size(), 1);
            DmaSliceConcat::upsample_chw(sliced.data(), sliced.size(), 4, 4, 4, 2, 2, up.data(), up.size(), 1);
            DmaSliceConcat::concat_chw(up.data(), up.size(), cat.data(), cat.size(), 6, 8, 8, 0, 0, 4, 1);
            DmaSliceConcat::concat_chw(skip_in.data(), skip_in.size(), cat.data(), cat.size(), 6, 8, 8, 0, 4, 2, 1);
            check(DmaSliceConcat::reshape_chw(cat.data(), cat.size(), 6, 8, 8, 64, 1, -1, head_tmaj.data(), head_tmaj.size(), 1, true),
                  "R4a: chained slice->upsample->concat->reshape pipeline executes cleanly");

            std::vector<uint8_t> expected_cat(6 * 8 * 8);
            const auto exp_up = golden_upsample(std::vector<uint8_t>(neck_in.begin() + 2 * 16, neck_in.end()), 4, 4, 4, 2, 2, 1);
            std::memcpy(expected_cat.data(), exp_up.data(), exp_up.size());
            std::memcpy(expected_cat.data() + exp_up.size(), skip_in.data(), skip_in.size());
            check(head_tmaj == golden_transpose_chw_hw(expected_cat, 6, 8, 8, 1),
                  "R4b: chained YOLOv8 neck+head pipeline matches golden bit-for-bit");

            uint32_t rng = 0xBADC0DE5u;
            auto next_u = [&]() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
            bool rfuzz_ok = true;
            for (int t = 0; t < 200 && rfuzz_ok; t++)
            {
                const unsigned ic = 1 + (next_u() % 10), ih = 1 + (next_u() % 8), iw = 1 + (next_u() % 9);
                const uint32_t eb = (1u << (next_u() % 3));
                const bool tr = (next_u() & 1u) != 0;
                const uint32_t total = ic * ih * iw;
                int oc = int(ih), oh = int(iw), ow = int(ic);
                const int wild = int(next_u() % 4);
                int qc = (wild == 0) ? -1 : oc, qh = (wild == 1) ? -1 : oh, qw = (wild == 2) ? -1 : ow;
                std::vector<uint8_t> s(size_t(total) * eb), d(size_t(total) * eb, 0x33);
                for (size_t i = 0; i < s.size(); i++) s[i] = uint8_t(next_u() & 0xFF);
                if (!DmaSliceConcat::reshape_chw(s.data(), s.size(), ic, ih, iw, qc, qh, qw, d.data(), d.size(), eb, tr))
                    rfuzz_ok = false;
                else if (d != (tr ? golden_transpose_chw_hw(s, ic, ih, iw, eb) : s))
                    rfuzz_ok = false;
            }
            check(rfuzz_ok, "R4c: 200 random reshape_chw + corner-turn + -1 axis fuzz cases");
        }
    }

    void test_mmio_compat_decode()
    {
        using namespace has;
        using namespace has::mmio;

        MmioCompat m(16);
        m.dma_tensor = true;
        m.dma_nd = true;
        m.dma_reshape = true;     // HAS_DMA_RESHAPE=1
        m.dma_reshape_t = true;   // HAS_DMA_RESHAPE_T=1 (the corner-turn flag of the descriptor tests)

        // U1: ELEM_WISE (0x15) mode 8 = DMA_UPSAMPLE via OUT_H / OUT_W
        m.write(A_ADDR, 0x1000);
        m.write(OUT_ADDR, 0x2000);
        m.write(MODE_PACK, 8);
        m.write(ext_addr(X_IN_C), 8);
        m.write(ext_addr(X_IN_H), 4);
        m.write(ext_addr(X_IN_W), 5);
        m.write(ext_addr(X_OUT_H), 8);
        m.write(ext_addr(X_OUT_W), 15);
        m.write(PUSH_A, 0x15);
        LayerInstr li;
        check(m.pop(li) && li.opcode == 0x15 && li.mode == 8 && li.pool_k == 2 && li.pool_s == 3 && li.n == 8 * 8 * 15,
              "U1: 0x15 mode 8 DMA_UPSAMPLE decodes sh=2, sw=3, n=960");
        m.retire(li);

        // U2: DMA_TENSOR (0x16) mode 2 = DMA_UPSAMPLE via POOL_K / POOL_P
        m.write(A_ADDR, 0x3000);
        m.write(OUT_ADDR, 0x5000);
        m.write(MODE_PACK, 2);
        m.write(ext_addr(X_IN_C), 4);
        m.write(ext_addr(X_IN_H), 6);
        m.write(ext_addr(X_IN_W), 6);
        m.write(ext_addr(X_POOL_K), 2);
        m.write(PUSH_A, 0x16);
        check(m.pop(li) && li.opcode == 0x16 && li.mode == 2 && li.pool_k == 2 && li.pool_s == 2 && li.n == 4 * 12 * 12,
              "U2: 0x16 mode 2 DMA_UPSAMPLE decodes isotropic 2x2 from POOL_K=2");
        m.retire(li);

        // U3: ELEM_WISE (0x15) mode 9 = DMA_RESHAPE with inferred OUT_H = -1
        m.write(A_ADDR, 0x6000);
        m.write(OUT_ADDR, 0x7000);
        m.write(MODE_PACK, 9);
        m.write(ext_addr(X_IN_C), 4);
        m.write(ext_addr(X_IN_H), 6);
        m.write(ext_addr(X_IN_W), 8);
        m.write(ext_addr(X_OUT_C), 3);
        m.write(ext_addr(X_OUT_H), uint32_t(-1));
        m.write(ext_addr(X_OUT_W), 8);
        m.write(PUSH_A, 0x15);
        check(m.pop(li) && li.opcode == 0x15 && li.mode == 9 && li.geom.cout == 3 && li.geom.oh == 8 && li.geom.ow == 8 && li.n == 192,
              "U3: 0x15 mode 9 DMA_RESHAPE resolves [4][6][8] -> [3][8][8]");
        m.retire(li);

        // U4: DMA_TENSOR (0x16) mode 3 = DMA_RESHAPE with FLAGS=1 ([C][HW]->[HW][C] transpose) & sticky LEN ignored when IN_C set
        m.write(LEN, 99999); // sticky LEN from an earlier op must NOT break IN_C-based reshape
        m.write(A_ADDR, 0x8000);
        m.write(OUT_ADDR, 0x9000);
        m.write(MODE_PACK, 3);
        m.write(ext_addr(X_IN_C), 4);
        m.write(ext_addr(X_IN_H), 3);
        m.write(ext_addr(X_IN_W), 5);
        m.write(ext_addr(X_OUT_C), 15);
        m.write(ext_addr(X_OUT_H), 1);
        m.write(ext_addr(X_OUT_W), 4);
        m.write(ext_addr(X_FLAGS), 1);
        m.write(PUSH_A, 0x16);
        check(m.pop(li) && li.opcode == 0x16 && li.mode == 3 && li.flags == 1 && li.n == 60,
              "U4: 0x16 mode 3 DMA_RESHAPE decodes FLAGS=1 transpose & ignores sticky LEN");
        m.retire(li);

        // U5: In-place C-order DMA_RESHAPE (A_ADDR == OUT_ADDR, FLAGS=0) allowed, while in-place transpose refused
        m.write(A_ADDR, 0xA000);
        m.write(OUT_ADDR, 0xA000);
        m.write(LEN, 128);
        m.write(MODE_PACK, 3);
        m.write(ext_addr(X_OUT_C), uint32_t(-1));
        m.write(ext_addr(X_OUT_H), 4);
        m.write(ext_addr(X_OUT_W), 8);
        m.write(PUSH_A, 0x16);
        check(m.pop(li) && li.geom.cout == 4 && li.n == 128, "U5a: in-place C-order DMA_RESHAPE via LEN=128 succeeds");
        m.retire(li);

        m.write(STATUS, 0);
        m.write(A_ADDR, 0xA000);
        m.write(OUT_ADDR, 0xA000);
        m.write(MODE_PACK, 3);
        m.write(ext_addr(X_IN_C), 4);
        m.write(ext_addr(X_IN_H), 4);
        m.write(ext_addr(X_IN_W), 8);
        m.write(ext_addr(X_OUT_C), 32);
        m.write(ext_addr(X_OUT_H), 1);
        m.write(ext_addr(X_OUT_W), 4);
        m.write(ext_addr(X_FLAGS), 1);
        m.write(PUSH_A, 0x16);
        check(m.queued() == 0 && m.last_error() == Err::BAD_GEOM, "U5b: in-place transpose DMA_RESHAPE refused (BAD_GEOM)");
        m.write(STATUS, 0);

        // U6: Mode 1 on 0x16 stays refused (regression guard for tb_has_dma_tensor_isa D5), and dma_nd=false gates 0x15 modes 8/9
        m.write(MODE_PACK, 1);
        m.write(PUSH_A, 0x16);
        check(m.queued() == 0 && m.last_error() == Err::UNSUPPORTED_MODE, "U6a: 0x16 mode 1 remains UNSUPPORTED_MODE");
        m.write(STATUS, 0);

        m.dma_nd = false;
        m.write(MODE_PACK, 8);
        m.write(ext_addr(X_IN_C), 2);
        m.write(ext_addr(X_IN_H), 2);
        m.write(ext_addr(X_IN_W), 2);
        m.write(ext_addr(X_POOL_K), 2);
        m.write(PUSH_A, 0x15);
        check(m.queued() == 0 && m.last_error() == Err::UNSUPPORTED_MODE, "U6b: 0x15 mode 8 gated by dma_nd");
        m.write(STATUS, 0);
        m.dma_nd = true;

        check(m.n_dma_upsample == 2 && m.n_dma_reshape == 3, "M7: MmioCompat n_dma_upsample=2 and n_dma_reshape=3 counters accurate");

        // M8: STRIDE fallback upsample, asymmetric POOL_K/POOL_P on 0x16 mode 8 alias, matching OUT_C==IN_C, and default H/W=1 reshapes
        m.write(A_ADDR, 0x10000);
        m.write(OUT_ADDR, 0x12000);
        m.write(STRIDE, 3);
        m.write(MODE_PACK, 8);
        m.write(ext_addr(X_IN_C), 2);
        m.write(ext_addr(X_IN_H), 3);
        m.write(ext_addr(X_IN_W), 4);
        m.write(ext_addr(X_OUT_C), 2);
        m.write(PUSH_A, 0x15);
        check(m.pop(li) && li.pool_k == 3 && li.pool_s == 3 && li.n == 2 * 9 * 12,
              "M8a: STRIDE=3 fallback upsample + explicit OUT_C==IN_C accepted");
        m.retire(li);

        m.write(A_ADDR, 0x14000);
        m.write(OUT_ADDR, 0x16000);
        m.write(MODE_PACK, 8);
        m.write(ext_addr(X_IN_C), 3);
        m.write(ext_addr(X_IN_H), 2);
        m.write(ext_addr(X_IN_W), 5);
        m.write(ext_addr(X_POOL_K), 2);
        m.write(ext_addr(X_POOL_P), 4);
        m.write(PUSH_A, 0x16);
        check(m.pop(li) && li.opcode == 0x16 && li.pool_k == 2 && li.pool_s == 4 && li.n == 3 * 4 * 20,
              "M8b: 0x16 mode 8 alias decodes asymmetric POOL_K=2, POOL_P=4");
        m.retire(li);

        m.write(A_ADDR, 0x18000);
        m.write(OUT_ADDR, 0x19000);
        m.write(LEN, 256);
        m.write(MODE_PACK, 9);
        m.write(ext_addr(X_OUT_C), 16);
        m.write(ext_addr(X_OUT_W), uint32_t(-1));
        m.write(PUSH_A, 0x16);
        check(m.pop(li) && li.geom.cout == 16 && li.geom.oh == 1 && li.geom.ow == 16 && li.n == 256,
              "M8c: 0x16 mode 9 alias defaults omitted OUT_H=1 and infers OUT_W=16");
        m.retire(li);

        m.write(A_ADDR, 0x1A000);
        m.write(OUT_ADDR, 0x1B000);
        m.write(MODE_PACK, 9);
        m.write(ext_addr(X_IN_C), 8);
        m.write(ext_addr(X_IN_H), 4);
        m.write(ext_addr(X_IN_W), 8);
        m.write(ext_addr(X_OUT_C), 256);
        m.write(PUSH_A, 0x15);
        check(m.pop(li) && li.geom.cout == 256 && li.geom.oh == 1 && li.geom.ow == 1 && li.n == 256,
              "M8d: 0x15 mode 9 defaults omitted OUT_H=1, OUT_W=1 ([8][4][8]->[256][1][1])");
        m.retire(li);

        // M9: Exhaustive 15-case negative / guardrail matrix
        auto expect_err = [&](Err want, const char *label, auto setup_fn) {
            m.write(STATUS, 0);
            setup_fn();
            check(m.queued() == 0 && m.last_error() == want, label);
            m.write(STATUS, 0);
        };

        expect_err(Err::BAD_GEOM, "M9.01: upsample missing X_IN_W refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(MODE_PACK, 2);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_POOL_K), 2);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.02: upsample OUT_C != IN_C refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(MODE_PACK, 2);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
            m.write(ext_addr(X_OUT_C), 8); m.write(ext_addr(X_POOL_K), 2);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.03: upsample OUT_H without OUT_W refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(MODE_PACK, 8);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
            m.write(ext_addr(X_OUT_H), 8);
            m.write(PUSH_A, 0x15);
        });
        expect_err(Err::BAD_GEOM, "M9.04: upsample non-divisible OUT_H % IN_H != 0 refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(MODE_PACK, 8);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
            m.write(ext_addr(X_OUT_H), 10); m.write(ext_addr(X_OUT_W), 8);
            m.write(PUSH_A, 0x15);
        });
        expect_err(Err::BAD_GEOM, "M9.05: upsample conflicting OUT_H/IN_H != POOL_K refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(MODE_PACK, 2);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
            m.write(ext_addr(X_OUT_H), 8); m.write(ext_addr(X_OUT_W), 8); m.write(ext_addr(X_POOL_K), 3);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.06: upsample STRIDE=0 fallback refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(STRIDE, 0); m.write(MODE_PACK, 2);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
            m.write(PUSH_A, 0x16);
            m.write(STRIDE, 1);
        });
        expect_err(Err::BAD_GEOM, "M9.07: upsample overlapping src and dst refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x20020); m.write(MODE_PACK, 2);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
            m.write(ext_addr(X_POOL_K), 2);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.08: upsample 32-bit address wrap refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0xFFFFFF00u); m.write(MODE_PACK, 2);
            m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 8);
            m.write(ext_addr(X_POOL_K), 2);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.09: reshape missing OUT_C refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(LEN, 64); m.write(MODE_PACK, 3);
            m.write(ext_addr(X_OUT_H), 8); m.write(ext_addr(X_OUT_W), 8);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.10: reshape two -1 axes refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(LEN, 64); m.write(MODE_PACK, 3);
            m.write(ext_addr(X_OUT_C), uint32_t(-1)); m.write(ext_addr(X_OUT_H), uint32_t(-1)); m.write(ext_addr(X_OUT_W), 8);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.11: reshape non-divisible -1 refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(LEN, 100); m.write(MODE_PACK, 9);
            m.write(ext_addr(X_OUT_C), uint32_t(-1)); m.write(ext_addr(X_OUT_H), 3); m.write(ext_addr(X_OUT_W), 3);
            m.write(PUSH_A, 0x15);
        });
        expect_err(Err::UNSUPPORTED_MODE, "M9.12: reshape unknown FLAGS bit (FLAGS=2) refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(LEN, 64); m.write(MODE_PACK, 3);
            m.write(ext_addr(X_OUT_C), 8); m.write(ext_addr(X_OUT_H), 8); m.write(ext_addr(X_FLAGS), 2);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.13: reshape FLAGS=1 transpose without X_IN_C refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(LEN, 64); m.write(MODE_PACK, 3);
            m.write(ext_addr(X_OUT_C), 8); m.write(ext_addr(X_OUT_H), 8); m.write(ext_addr(X_FLAGS), 1);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::BAD_GEOM, "M9.14: reshape partial buffer overlap refused", [&]() {
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x20010); m.write(LEN, 64); m.write(MODE_PACK, 3);
            m.write(ext_addr(X_OUT_C), 8); m.write(ext_addr(X_OUT_H), 8);
            m.write(PUSH_A, 0x16);
        });
        expect_err(Err::UNSUPPORTED_OP, "M9.15: dma_tensor=false gates 0x16 upsample/reshape", [&]() {
            m.dma_tensor = false;
            m.write(A_ADDR, 0x20000); m.write(OUT_ADDR, 0x24000); m.write(LEN, 64); m.write(MODE_PACK, 3);
            m.write(ext_addr(X_OUT_C), 8); m.write(ext_addr(X_OUT_H), 8);
            m.write(PUSH_A, 0x16);
            m.dma_tensor = true;
        });

        // M10: One-shot extension hygiene -- after a failed reshape with X_FLAGS=1 & X_IN_C, next push has clean ext_[]
        m.write(A_ADDR, 0x30000); m.write(OUT_ADDR, 0x30000); m.write(MODE_PACK, 3);
        m.write(ext_addr(X_IN_C), 4); m.write(ext_addr(X_IN_H), 4); m.write(ext_addr(X_IN_W), 4);
        m.write(ext_addr(X_OUT_C), 16); m.write(ext_addr(X_OUT_H), 1); m.write(ext_addr(X_OUT_W), 4);
        m.write(ext_addr(X_FLAGS), 1);
        m.write(PUSH_A, 0x16);
        m.write(STATUS, 0);
        m.write(LEN, 64);
        m.write(ext_addr(X_OUT_C), 8); m.write(ext_addr(X_OUT_H), 8);
        m.write(PUSH_A, 0x16);
        check(m.pop(li) && li.flags == 0 && li.c == 64 && li.h == 1 && li.w == 1,
              "M10: one-shot extension registers cleared cleanly even after refused push");
        m.retire(li);

        // M11: knob off (the defaults): every upsample / reshape push is refused with UNSUPPORTED_MODE, nothing queued; the COPY of mode 0 is not touched
        {
            MmioCompat m0(16);
            m0.dma_tensor = true; m0.dma_nd = true; m0.dma_reshape = false; m0.dma_reshape_t = false;
            const struct { uint32_t op, mode; } cs[] = { {0x16, 2}, {0x16, 3}, {0x16, 8}, {0x16, 9}, {0x15, 8}, {0x15, 9} };
            bool all_refused = true;
            for (const auto &c : cs)
            {
                m0.write(STATUS, 0);
                m0.write(A_ADDR, 0x1000); m0.write(OUT_ADDR, 0x4000); m0.write(MODE_PACK, c.mode);
                m0.write(ext_addr(X_IN_C), 2); m0.write(ext_addr(X_IN_H), 4); m0.write(ext_addr(X_IN_W), 4);
                m0.write(ext_addr(X_OUT_C), 2); m0.write(ext_addr(X_OUT_H), 8); m0.write(ext_addr(X_OUT_W), 8);
                m0.write(PUSH_A, c.op);
                if (!(m0.queued() == 0 && m0.last_error() == Err::UNSUPPORTED_MODE)) all_refused = false;
            }
            check(all_refused, "M11: HAS_DMA_RESHAPE=0: 0x16 modes 2 / 3 / 8 / 9 and 0x15 modes 8 / 9 refused with UNSUPPORTED_MODE, nothing queued");
            m0.write(STATUS, 0);
            m0.write(A_ADDR, 0x1000); m0.write(OUT_ADDR, 0x4000); m0.write(LEN, 64); m0.write(MODE_PACK, 0);
            m0.write(PUSH_A, 0x16);
            check(m0.queued() == 1 && m0.n_dma_tensor == 1, "M11b: HAS_DMA_RESHAPE=0: DMA_TENSOR COPY (mode 0) still accepted");
        }
        // M12: knob on, transpose flag off (HAS_DMA_RESHAPE_T=0, the default): a reshape with X_FLAGS = 1 is refused, X_FLAGS = 0 accepted
        {
            MmioCompat m1(16);
            m1.dma_tensor = true; m1.dma_nd = true; m1.dma_reshape = true; m1.dma_reshape_t = false;
            m1.write(A_ADDR, 0x1000); m1.write(OUT_ADDR, 0x4000); m1.write(MODE_PACK, 3);
            m1.write(ext_addr(X_IN_C), 4); m1.write(ext_addr(X_IN_H), 3); m1.write(ext_addr(X_IN_W), 5);
            m1.write(ext_addr(X_OUT_C), 15); m1.write(ext_addr(X_OUT_H), 1); m1.write(ext_addr(X_OUT_W), 4);
            m1.write(ext_addr(X_FLAGS), 1);
            m1.write(PUSH_A, 0x16);
            check(m1.queued() == 0 && m1.last_error() == Err::UNSUPPORTED_MODE, "M12: HAS_DMA_RESHAPE=1 HAS_DMA_RESHAPE_T=0: reshape with X_FLAGS bit 0 refused (UNSUPPORTED_MODE)");
            m1.write(STATUS, 0);
            m1.write(A_ADDR, 0x1000); m1.write(OUT_ADDR, 0x4000); m1.write(MODE_PACK, 3);
            m1.write(ext_addr(X_IN_C), 4); m1.write(ext_addr(X_IN_H), 3); m1.write(ext_addr(X_IN_W), 5);
            m1.write(ext_addr(X_OUT_C), 15); m1.write(ext_addr(X_OUT_H), 1); m1.write(ext_addr(X_OUT_W), 4);
            m1.write(PUSH_A, 0x16);
            check(m1.queued() == 1, "M12b: the same reshape without the flag accepted");
        }
    }

#ifdef HAS_TB_NEO_CSR
    void test_neo_rtl_csr_and_dfc()
    {
        using namespace neo_rtl;

        NeoTopCsrSubsystem sys;
        std::vector<uint8_t> dram(0x20000, 0);
        sys.set_dram(&dram);
        sys.set_cfg_base(0x00000000u);
        sys.set_dram_base(0x80000000u);

        // N1: Unmapped 0x30008 still returns 0x3BADADD2 + SLVERR
        bool slverr = false;
        const uint32_t bad = sys.read(DMA_OFFSET + 0x08u, &slverr);
        check(bad == ERR_DMA_BAD_ADDR && slverr, "N1: DMA_OFFSET + 0x08 still returns 0x3BADADD2 + SLVERR");

        // N2: Standalone DMA CSR UPSAMPLE (DRAM 0x8000_1000 -> DRAM 0x8000_2000, [3][4][5] -> [3][8][10])
        const unsigned C = 3, H = 4, W = 5, sh = 2, sw = 2;
        std::vector<uint8_t> src_u(C * H * W);
        for (size_t i = 0; i < src_u.size(); i++)
        {
            src_u[i] = uint8_t((i * 23 + 7) & 0xFF);
            dram[0x1000 + i] = src_u[i];
        }
        sys.write(DMA_OFFSET + DMA_REG_INTR_EN, 0x3u);
        sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, 0x80001000u);
        sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, 0x80002000u);
        sys.write(DMA_OFFSET + DMA_REG_EXT_IN_HW, (H << 16) | W);
        sys.write(DMA_OFFSET + DMA_REG_EXT_IN_C, C);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_UPSAMPLE | (sh << 8) | (sw << 16) | (1u << 24));
        sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u); // reader_start | writer_start

        const auto gold_u = golden_upsample(src_u, C, H, W, sh, sw, 1);
        std::vector<uint8_t> got_u(dram.begin() + 0x2000, dram.begin() + 0x2000 + gold_u.size());
        check(got_u == gold_u, "N2a: Standalone DMA CSR upsample [3][4][5]->[3][8][10] exact");
        check(sys.o_reader_dmaintr() && sys.o_writer_dmaintr() && sys.dma.st.upsample_ops == 1,
              "N2b: Standalone DMA CSR upsample asserts reader/writer IRQs and increments upsample_ops");
        sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u); // COW clear

        // N3: Standalone DMA CSR RESHAPE across crossbar (DRAM 0x8000_1000 -> GVU VBANK0 -> DRAM 0x8000_3000 with corner-turn)
        sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, 0x80001000u);
        sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, GVU_DMA_OFFSET + VBANK0_OFFSET);
        sys.write(DMA_OFFSET + DMA_REG_EXT_IN_HW, (H << 16) | W);
        sys.write(DMA_OFFSET + DMA_REG_EXT_IN_C, C);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_C, 12u);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_HW, (1u << 16) | 0xFFFFu); // OUT_H=1, OUT_W=-1 -> inferred 5
        sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_RESHAPE);
        sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
        sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u);

        // Now corner-turn reshape from GVU VBANK0 -> DRAM 0x8000_3000 ([3][4][5] -> [20][1][3])
        sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, GVU_DMA_OFFSET + VBANK0_OFFSET);
        sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, 0x80003000u);
        sys.write(DMA_OFFSET + DMA_REG_EXT_IN_HW, (H << 16) | W);
        sys.write(DMA_OFFSET + DMA_REG_EXT_IN_C, C);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_C, 20u);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_HW, (1u << 16) | 3u);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_RESHAPE | (1u << 4)); // bit 4 = transpose_chw_hw
        sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
        sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u);
        sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_1D_COPY);

        const auto gold_tr = golden_transpose_chw_hw(src_u, C, H, W, 1);
        std::vector<uint8_t> got_tr(dram.begin() + 0x3000, dram.begin() + 0x3000 + gold_tr.size());
        check(got_tr == gold_tr && sys.dma.st.reshape_ops == 2,
              "N3: Standalone DMA CSR reshape DRAM->GVU_VBANK0->DRAM with -1 inference & corner-turn exact");

        // N4: Strict 64B RTL CSR mode makes 0x30040..0x30050 return 0x3BADADD2 + SLVERR (reads and writes)
        sys.dma.set_strict_rtl_csr(true);
        slverr = false;
        check(sys.read(DMA_OFFSET + DMA_REG_EXT_OP, &slverr) == ERR_DMA_BAD_ADDR && slverr,
              "N4a: strict_rtl_csr(true) returns 0x3BADADD2 + SLVERR on extension offset 0x40 read");
        slverr = false;
        sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_C, 0x12345678u, 0xFu, &slverr);
        check(slverr, "N4b: strict_rtl_csr(true) asserts SLVERR on extension offset 0x50 write");
        sys.dma.set_strict_rtl_csr(false);

        // N5: df_controller NEO_OP_DMA_UPSAMPLE (opcode 7, asymmetric 3x2: [3][4][5] -> [3][12][10] at 0x8000_4000)
        auto dfc_ctrl_addr = [](unsigned idx) { return CONTROLLER_OFFSET + DFC_CTRL_REGS_OFFSET + idx * 4u; };
        sys.write(CONTROLLER_OFFSET + DFC_REG_IEN, 1u);
        sys.write(dfc_ctrl_addr(DFC_TIL_XY_LIM), (H << 16) | W);
        sys.write(dfc_ctrl_addr(DFC_TIL_CK_LIM), C | (3u << 16) | (2u << 24)); // sh=3, sw=2
        sys.write(dfc_ctrl_addr(DFC_START_SRAMA_ADDR), 0x80001000u);
        sys.write(dfc_ctrl_addr(DFC_START_SRAMC_ADDR), 0x80004000u);
        sys.write(dfc_ctrl_addr(DFC_FLAGS), 0u);
        sys.write(dfc_ctrl_addr(DFC_OPCODE), NEO_OP_DMA_UPSAMPLE);
        sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u); // start pulse

        const auto gold_u32 = golden_upsample(src_u, C, H, W, 3, 2, 1);
        std::vector<uint8_t> got_u32(dram.begin() + 0x4000, dram.begin() + 0x4000 + gold_u32.size());
        check(got_u32 == gold_u32 && sys.o_intr(),
              "N5: df_controller NEO_OP_DMA_UPSAMPLE (opcode 7) 3x2 exact + asserts o_intr()");
        sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u); // clear done
        sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);    // clear intr

        // N6: df_controller NEO_OP_DMA_RESHAPE (opcode 8, [3][12][10] -> [-1][15][8] at 0x8000_5000)
        sys.write(dfc_ctrl_addr(DFC_TIL_XY_LIM), (12u << 16) | 10u);
        sys.write(dfc_ctrl_addr(DFC_TIL_CK_LIM), 3u);
        sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_X_STEP), 8u);
        sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_Y_STEP), 15u);
        sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_K_STEP), uint32_t(-1)); // OUT_C = -1 -> 3
        sys.write(dfc_ctrl_addr(DFC_START_SRAMA_ADDR), 0x80004000u);
        sys.write(dfc_ctrl_addr(DFC_START_SRAMC_ADDR), 0x80005000u);
        sys.write(dfc_ctrl_addr(DFC_FLAGS), 0u);
        sys.write(dfc_ctrl_addr(DFC_OPCODE), NEO_OP_DMA_RESHAPE);
        sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u);

        std::vector<uint8_t> got_r(dram.begin() + 0x5000, dram.begin() + 0x5000 + gold_u32.size());
        check(got_r == gold_u32 && sys.o_intr(),
              "N6: df_controller NEO_OP_DMA_RESHAPE (opcode 8) [-1][15][8] exact + asserts o_intr()");
        sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u);
        sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);

        // N7: Crossbar SAURIA SRAM-A -> UPSAMPLE (elem_b=2, partial wstrb, scale from EXT_OUT_HW) -> SAURIA SRAM-C -> DRAM
        {
            std::vector<uint8_t> s16(2 * 3 * 4 * 2);
            for (size_t i = 0; i < s16.size(); i++)
            {
                s16[i] = uint8_t((i * 59 + 11) & 0xFF);
                dram[0x6000 + i] = s16[i];
            }
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_1D_COPY);
            sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, 0x80006000u);
            sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, SAURIA_DMA_OFFSET + SRAMA_OFFSET);
            sys.write(DMA_OFFSET + DMA_REG_BTT, uint32_t(s16.size()));
            sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
            sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u);

            sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, SAURIA_DMA_OFFSET + SRAMA_OFFSET);
            sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, SAURIA_DMA_OFFSET + SRAMC_OFFSET);
            sys.write(DMA_OFFSET + DMA_REG_EXT_IN_HW, 4u, 0x3u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_IN_HW, (3u << 16), 0xCu);
            sys.write(DMA_OFFSET + DMA_REG_EXT_IN_C, 2u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_HW, (3u << 16) | 16u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_UPSAMPLE, 0x1u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, 0u, 0x6u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, (2u << 24), 0x8u);
            check(sys.read(DMA_OFFSET + DMA_REG_EXT_IN_HW) == ((3u << 16) | 4u) &&
                  sys.read(DMA_OFFSET + DMA_REG_EXT_OP) == ((2u << 24) | DMA_EXT_UPSAMPLE),
                  "N7a: partial wstrb writes on DMA_REG_EXT_IN_HW and DMA_REG_EXT_OP read back accurately");
            sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
            sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u);

            const auto gold16 = golden_upsample(s16, 2, 3, 4, 1, 4, 2);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_1D_COPY);
            sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, SAURIA_DMA_OFFSET + SRAMC_OFFSET);
            sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, 0x80006800u);
            sys.write(DMA_OFFSET + DMA_REG_BTT, uint32_t(gold16.size()));
            sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
            sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u);

            std::vector<uint8_t> got16(dram.begin() + 0x6800, dram.begin() + 0x6800 + gold16.size());
            check(got16 == gold16, "N7b: Crossbar SAURIA SRAM-A -> UPSAMPLE (1x4, elem_b=2) -> SRAM-C exact");
        }

        // N8: Standalone DMA CSR zero-copy in-place reshape + invalid geometry refusal guards
        {
            const uint64_t rd_before = sys.dma.st.bytes_read, wr_before = sys.dma.st.bytes_written;
            const uint64_t rop_before = sys.dma.st.reshape_ops, uop_before = sys.dma.st.upsample_ops;
            sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, 0x80001000u);
            sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, 0x80001000u);
            sys.write(DMA_OFFSET + DMA_REG_BTT, 60u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_IN_C, 0u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_C, 5u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_HW, (0xFFFFu << 16) | 4u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_RESHAPE);
            sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
            check(sys.dma.st.reshape_ops == rop_before + 1 &&
                  sys.dma.st.bytes_read == rd_before && sys.dma.st.bytes_written == wr_before &&
                  sys.o_reader_dmaintr() && sys.o_writer_dmaintr(),
                  "N8a: Standalone DMA CSR in-place reshape performs zero bus traffic & asserts IRQs");
            sys.write(DMA_OFFSET + DMA_REG_INTR_STATUS, 0x3u);

            sys.write(DMA_OFFSET + DMA_REG_READ_ADDR, 0x80001000u);
            sys.write(DMA_OFFSET + DMA_REG_WRITE_ADDR, 0x80007000u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_IN_HW, (4u << 16) | 5u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_IN_C, 3u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_HW, (7u << 16) | 10u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_UPSAMPLE);
            sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
            check(sys.dma.st.upsample_ops == uop_before, "N8b: DMA CSR rejects non-divisible EXT_OUT_HW upsample");

            sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_C, 7u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OUT_HW, (7u << 16) | 7u);
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_RESHAPE);
            sys.write(DMA_OFFSET + DMA_REG_CTRL, 0x3u);
            check(sys.dma.st.reshape_ops == rop_before + 1, "N8c: DMA CSR rejects mismatched reshape element count");
            sys.write(DMA_OFFSET + DMA_REG_EXT_OP, DMA_EXT_1D_COPY);
        }

        // N9: df_controller opcodes 7 & 8: PSUM_STEP scale derivation + elem_b=2, flat ETT reshape, and corner-turn transpose
        {
            std::vector<uint8_t> s2(3 * 4 * 5 * 2);
            for (size_t i = 0; i < s2.size(); i++) { s2[i] = uint8_t((i * 43 + 19) & 0xFF); dram[0x8000 + i] = s2[i]; }
            sys.write(dfc_ctrl_addr(DFC_TIL_XY_LIM), (4u << 16) | 5u);
            sys.write(dfc_ctrl_addr(DFC_TIL_CK_LIM), 3u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_Y_STEP), 8u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_X_STEP), 15u);
            sys.write(dfc_ctrl_addr(DFC_START_SRAMA_ADDR), 0x80008000u);
            sys.write(dfc_ctrl_addr(DFC_START_SRAMC_ADDR), 0x80008400u);
            sys.write(dfc_ctrl_addr(DFC_OPCODE), NEO_OP_DMA_UPSAMPLE | (2u << 24));
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u);
            const auto gold_dfc_u = golden_upsample(s2, 3, 4, 5, 2, 3, 2);
            std::vector<uint8_t> got_dfc_u(dram.begin() + 0x8400, dram.begin() + 0x8400 + gold_dfc_u.size());
            check(got_dfc_u == gold_dfc_u, "N9a: DFC opcode 7 derives sh=2, sw=3 from PSUM_TIL_Y/X_STEP with elem_b=2");
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);

            const uint64_t uop_now = sys.dma.st.upsample_ops;
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_Y_STEP), 9u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u);
            check(sys.dma.st.upsample_ops == uop_now, "N9b: DFC opcode 7 rejects non-divisible PSUM_TIL_Y_STEP");
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);

            sys.write(dfc_ctrl_addr(DFC_TIL_XY_LIM), 0u);
            sys.write(dfc_ctrl_addr(DFC_TIL_CK_LIM), 0u);
            sys.write(dfc_ctrl_addr(DFC_IFMAP_DMA_ETT), 120u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_K_STEP), 10u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_Y_STEP), 3u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_X_STEP), uint32_t(-1));
            sys.write(dfc_ctrl_addr(DFC_START_SRAMA_ADDR), 0x80008000u);
            sys.write(dfc_ctrl_addr(DFC_START_SRAMC_ADDR), 0x80009000u);
            sys.write(dfc_ctrl_addr(DFC_OPCODE), NEO_OP_DMA_RESHAPE);
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u);
            std::vector<uint8_t> got_flat(dram.begin() + 0x9000, dram.begin() + 0x9000 + 120);
            check(got_flat == s2, "N9c: DFC opcode 8 reshapes flat DFC_IFMAP_DMA_ETT=120 into [10][3][-1]");
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);

            sys.write(dfc_ctrl_addr(DFC_TIL_XY_LIM), (4u << 16) | 5u);
            sys.write(dfc_ctrl_addr(DFC_TIL_CK_LIM), 6u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_K_STEP), 20u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_Y_STEP), 1u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_X_STEP), 6u);
            sys.write(dfc_ctrl_addr(DFC_START_SRAMA_ADDR), 0x80008000u);
            sys.write(dfc_ctrl_addr(DFC_START_SRAMC_ADDR), 0x80009400u);
            sys.write(dfc_ctrl_addr(DFC_OPCODE), NEO_OP_DMA_RESHAPE | (1u << 16));
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u);
            const auto gold_dfc_tr = golden_transpose_chw_hw(s2, 6, 4, 5, 1);
            std::vector<uint8_t> got_dfc_tr(dram.begin() + 0x9400, dram.begin() + 0x9400 + 120);
            check(got_dfc_tr == gold_dfc_tr, "N9d: DFC opcode 8 corner-turn transpose via DFC_OPCODE[31:16] [6][4][5]->[20][1][6] exact");
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);

            // Also verify DFC_FLAGS[15:0] (control_regs[38]) encoding with elem_b=2 + transpose_chw_hw=1 ([3][4][5]x2B -> [20][1][3]x2B)
            sys.write(dfc_ctrl_addr(DFC_TIL_XY_LIM), (4u << 16) | 5u);
            sys.write(dfc_ctrl_addr(DFC_TIL_CK_LIM), 3u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_K_STEP), 20u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_Y_STEP), 1u);
            sys.write(dfc_ctrl_addr(DFC_PSUM_TIL_X_STEP), uint32_t(-1));
            sys.write(dfc_ctrl_addr(DFC_START_SRAMA_ADDR), 0x80008000u);
            sys.write(dfc_ctrl_addr(DFC_START_SRAMC_ADDR), 0x80009800u);
            sys.write(dfc_ctrl_addr(DFC_FLAGS), (2u << 8) | 1u);
            sys.write(dfc_ctrl_addr(DFC_OPCODE), NEO_OP_DMA_RESHAPE);
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 1u);
            const auto gold_dfc_tr16 = golden_transpose_chw_hw(s2, 3, 4, 5, 2);
            std::vector<uint8_t> got_dfc_tr16(dram.begin() + 0x9800, dram.begin() + 0x9800 + 120);
            check(got_dfc_tr16 == gold_dfc_tr16, "N9e: DFC opcode 8 corner-turn transpose via DFC_FLAGS[15:0] (elem_b=2, -1 axis) exact");
            sys.write(dfc_ctrl_addr(DFC_FLAGS), 0u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_CONTROL, 2u);
            sys.write(CONTROLLER_OFFSET + DFC_REG_INTR, 1u);
        }
    }

#endif // HAS_TB_NEO_CSR

    int emit_ref_program_dir(const std::string &dir, bool with_transpose)
    {
        using namespace has;
        using namespace has::mmio;

        std::vector<uint8_t> init_img(0x10000, 0);
        // Source tensor T0: [4][3][5] = 60 B at 0x0100
        std::vector<uint8_t> t0(60);
        for (size_t i = 0; i < t0.size(); i++)
        {
            t0[i] = uint8_t((i * 13 + 3) & 0xFF);
            init_img[0x0100 + i] = t0[i];
        }
        // Multi-chunk source tensor T1: [8][12][16] = 1,536 B at 0x2000 (> 1,024 B chunk)
        std::vector<uint8_t> t1(8 * 12 * 16);
        for (size_t i = 0; i < t1.size(); i++)
        {
            t1[i] = uint8_t((i * 37 + 9) & 0xFF);
            init_img[0x2000 + i] = t1[i];
        }
        std::vector<uint8_t> gold_img = init_img;

        // [0] 0x16 mode 2 DMA_UPSAMPLE [4][3][5] -> [4][6][10] (240 B) at 0x0400 via POOL_K=2
        const auto u0 = golden_upsample(t0, 4, 3, 5, 2, 2, 1);
        std::memcpy(gold_img.data() + 0x0400, u0.data(), u0.size());

        // [1] 0x15 mode 8 DMA_UPSAMPLE [4][3][5] -> [4][9][10] (360 B, sh=3, sw=2 via OUT_H/OUT_W) at 0x0600
        const auto u1 = golden_upsample(t0, 4, 3, 5, 3, 2, 1);
        std::memcpy(gold_img.data() + 0x0600, u1.data(), u1.size());

        // [2] 0x15 mode 8 DMA_UPSAMPLE vertical-only 4x1 [4][3][5] -> [4][12][5] (240 B) at 0x0900 via POOL_K=4, POOL_P=1
        const auto u2 = golden_upsample(t0, 4, 3, 5, 4, 1, 1);
        std::memcpy(gold_img.data() + 0x0900, u2.data(), u2.size());

        // [3] 0x16 mode 8 alias DMA_UPSAMPLE horizontal-only 1x3 [4][3][5] -> [4][3][15] (180 B) at 0x0B00 via POOL_K=1, POOL_P=3
        const auto u3 = golden_upsample(t0, 4, 3, 5, 1, 3, 1);
        std::memcpy(gold_img.data() + 0x0B00, u3.data(), u3.size());

        // [4] 0x15 mode 8 DMA_UPSAMPLE isotropic 3x3 via STRIDE=3 [4][3][5] -> [4][9][15] (540 B) at 0x0D00
        const auto u4 = golden_upsample(t0, 4, 3, 5, 3, 3, 1);
        std::memcpy(gold_img.data() + 0x0D00, u4.data(), u4.size());

        // [5] 0x16 mode 2 multi-chunk DMA_UPSAMPLE [8][12][16] (1,536 B at 0x2000) -> [8][24][32] (6,144 B at 0x3000)
        const auto u5 = golden_upsample(t1, 8, 12, 16, 2, 2, 1);
        std::memcpy(gold_img.data() + 0x3000, u5.data(), u5.size());

        // [6] 0x16 mode 3 DMA_RESHAPE [4][6][10] (240 B at 0x0400) -> [-1][8][10] ([3][8][10] at 0x5000)
        DmaSliceConcat::reshape_chw(gold_img.data() + 0x0400, 240, 4, 6, 10, -1, 8, 10, gold_img.data() + 0x5000, 240, 1, false);

        // [7] 0x15 mode 9 DMA_RESHAPE [4][9][10] (360 B at 0x0600) -> [6][-1][12] ([6][5][12] at 0x5200)
        DmaSliceConcat::reshape_chw(gold_img.data() + 0x0600, 360, 4, 9, 10, 6, -1, 12, gold_img.data() + 0x5200, 360, 1, false);

        // [8] 0x16 mode 9 alias DMA_RESHAPE from flat LEN=180 (0x0B00) -> [9][4][-1] ([9][4][5] at 0x5500)
        DmaSliceConcat::reshape_chw(gold_img.data() + 0x0B00, 180, 180, 1, 1, 9, 4, -1, gold_img.data() + 0x5500, 180, 1, false);

        // [9] 0x15 mode 9 in-place zero-copy DMA_RESHAPE at 0x5500 ([9][4][5] -> [15][12][1])
        DmaSliceConcat::reshape_chw(gold_img.data() + 0x5500, 180, 9, 4, 5, 15, 12, 1, gold_img.data() + 0x5500, 180, 1, false);

        // [10] 0x15 mode 9 DMA_RESHAPE + FLAGS=1 corner-turn [4][3][5] (60 B at 0x0100) -> [15][1][4] at 0x5800
        if (with_transpose) DmaSliceConcat::reshape_chw(gold_img.data() + 0x0100, 60, 4, 3, 5, 15, 1, 4, gold_img.data() + 0x5800, 60, 1, true);

        // [11] 0x16 mode 3 multi-chunk DMA_RESHAPE + FLAGS=1 corner-turn [8][12][16] (1,536 B at 0x2000) -> [192][1][8] at 0x6000
        if (with_transpose) DmaSliceConcat::reshape_chw(gold_img.data() + 0x2000, 1536, 8, 12, 16, 192, 1, -1, gold_img.data() + 0x6000, 1536, 1, true);

        // Write dram_init.bin and dram_golden.bin
        {
            std::ofstream fi(dir + "/dram_init.bin", std::ios::binary);
            std::ofstream fg(dir + "/dram_golden.bin", std::ios::binary);
            if (!fi || !fg) return 2;
            fi.write(reinterpret_cast<const char *>(init_img.data()), std::streamsize(init_img.size()));
            fg.write(reinterpret_cast<const char *>(gold_img.data()), std::streamsize(gold_img.size()));
        }

        // Write mmio.txt
        std::ofstream fm(dir + "/mmio.txt");
        if (!fm) return 2;
        auto w = [&](uint32_t addr, uint32_t val) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "W 0x%08X 0x%08X\n", addr, val);
            fm << buf;
        };
        fm << "# [0] DMA_TENSOR UPSAMPLE 2x2 via POOL_K=2\n";
        w(A_ADDR, 0x0100); w(OUT_ADDR, 0x0400); w(MODE_PACK, 2);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 3); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_POOL_K), 2);
        w(PUSH_A, 0x16);

        fm << "# [1] ELEM_WISE DMA_UPSAMPLE 3x2 via OUT_H/OUT_W\n";
        w(A_ADDR, 0x0100); w(OUT_ADDR, 0x0600); w(MODE_PACK, 8);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 3); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_OUT_H), 9); w(ext_addr(X_OUT_W), 10);
        w(PUSH_A, 0x15);

        fm << "# [2] ELEM_WISE DMA_UPSAMPLE vertical 4x1 via POOL_K=4, POOL_P=1\n";
        w(A_ADDR, 0x0100); w(OUT_ADDR, 0x0900); w(MODE_PACK, 8);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 3); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_POOL_K), 4); w(ext_addr(X_POOL_P), 1);
        w(PUSH_A, 0x15);

        fm << "# [3] DMA_TENSOR (mode 8 alias) UPSAMPLE horizontal 1x3 via POOL_K=1, POOL_P=3\n";
        w(A_ADDR, 0x0100); w(OUT_ADDR, 0x0B00); w(MODE_PACK, 8);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 3); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_POOL_K), 1); w(ext_addr(X_POOL_P), 3);
        w(PUSH_A, 0x16);

        fm << "# [4] ELEM_WISE DMA_UPSAMPLE isotropic 3x3 via STRIDE=3\n";
        w(A_ADDR, 0x0100); w(OUT_ADDR, 0x0D00); w(STRIDE, 3); w(MODE_PACK, 8);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 3); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_OUT_C), 4);
        w(PUSH_A, 0x15);
        w(STRIDE, 1);

        fm << "# [5] DMA_TENSOR multi-chunk UPSAMPLE 2x2 [8][12][16] -> [8][24][32] (6,144 B)\n";
        w(A_ADDR, 0x2000); w(OUT_ADDR, 0x3000); w(MODE_PACK, 2);
        w(ext_addr(X_IN_C), 8); w(ext_addr(X_IN_H), 12); w(ext_addr(X_IN_W), 16);
        w(ext_addr(X_POOL_K), 2);
        w(PUSH_A, 0x16);

        fm << "# [6] DMA_TENSOR RESHAPE [-1][8][10]\n";
        w(A_ADDR, 0x0400); w(OUT_ADDR, 0x5000); w(MODE_PACK, 3);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 6); w(ext_addr(X_IN_W), 10);
        w(ext_addr(X_OUT_C), uint32_t(-1)); w(ext_addr(X_OUT_H), 8); w(ext_addr(X_OUT_W), 10);
        w(PUSH_A, 0x16);

        fm << "# [7] ELEM_WISE DMA_RESHAPE [4][9][10] -> [6][-1][12]\n";
        w(A_ADDR, 0x0600); w(OUT_ADDR, 0x5200); w(MODE_PACK, 9);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 9); w(ext_addr(X_IN_W), 10);
        w(ext_addr(X_OUT_C), 6); w(ext_addr(X_OUT_H), uint32_t(-1)); w(ext_addr(X_OUT_W), 12);
        w(PUSH_A, 0x15);

        fm << "# [8] DMA_TENSOR (mode 9 alias) RESHAPE from flat LEN=180 -> [9][4][-1]\n";
        w(A_ADDR, 0x0B00); w(OUT_ADDR, 0x5500); w(LEN, 180); w(MODE_PACK, 9);
        w(ext_addr(X_OUT_C), 9); w(ext_addr(X_OUT_H), 4); w(ext_addr(X_OUT_W), uint32_t(-1));
        w(PUSH_A, 0x16);

        fm << "# [9] ELEM_WISE in-place zero-copy DMA_RESHAPE [9][4][5] -> [15][12][1]\n";
        w(A_ADDR, 0x5500); w(OUT_ADDR, 0x5500); w(MODE_PACK, 9);
        w(ext_addr(X_IN_C), 9); w(ext_addr(X_IN_H), 4); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_OUT_C), 15); w(ext_addr(X_OUT_H), 12); w(ext_addr(X_OUT_W), 1);
        w(PUSH_A, 0x15);

        if (with_transpose) {
        fm << "# [10] ELEM_WISE DMA_RESHAPE + corner-turn [4][3][5] -> [15][1][4]\n";
        w(A_ADDR, 0x0100); w(OUT_ADDR, 0x5800); w(MODE_PACK, 9);
        w(ext_addr(X_IN_C), 4); w(ext_addr(X_IN_H), 3); w(ext_addr(X_IN_W), 5);
        w(ext_addr(X_OUT_C), 15); w(ext_addr(X_OUT_H), 1); w(ext_addr(X_OUT_W), 4);
        w(ext_addr(X_FLAGS), 1);
        w(PUSH_A, 0x15);

        fm << "# [11] DMA_TENSOR multi-chunk RESHAPE + corner-turn [8][12][16] -> [192][1][-1]\n";
        w(A_ADDR, 0x2000); w(OUT_ADDR, 0x6000); w(MODE_PACK, 3);
        w(ext_addr(X_IN_C), 8); w(ext_addr(X_IN_H), 12); w(ext_addr(X_IN_W), 16);
        w(ext_addr(X_OUT_C), 192); w(ext_addr(X_OUT_H), 1); w(ext_addr(X_OUT_W), uint32_t(-1));
        w(ext_addr(X_FLAGS), 1);
        w(PUSH_A, 0x16);
        }
        return 0;
    }
} // namespace

int main(int argc, char **argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--emit-program")
        return emit_ref_program_dir(argv[2], argc >= 4 && std::string(argv[3]) == "--with-transpose");

    test_dma_nd_desc_and_engine();
    test_mmio_compat_decode();
#ifdef HAS_TB_NEO_CSR
    test_neo_rtl_csr_and_dfc();
#endif

    std::printf("[tb_dma_upsample_reshape] %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
