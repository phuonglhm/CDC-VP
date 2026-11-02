// tb_has_mmio_compat.cpp -- unit test of has/has_mmio_compat.h (docs/SW_INTEGRATION_GUIDE.md).
//   C1  old-style ELEM_WISE ADD sequence copied from tools/test_onnx_model.cpp -> one ADD instruction, unit scales;
//       the unit-scale parameters give exactly sat8(A + B) for all 65 536 int8 pairs (HAS and compat knobs)
//   C2  old ELEM_WISE MUL / SUB / broadcast ADD -> rejected with an error code, nothing queued
//   C3  old-style GEMM_FUSED (m, k, n, float scales) -> 1x1 layer, fallback tiling inside the HAS SRAMs,
//       float scale converted to (S, s) with relative error < 2^-30
//   C4  new-style conv with extension registers (YOLOv8m stem) -> tile iterator gives the planner's 204 tiles
//   C5  SiLU without LUT_ADDR -> NEED_LUT; with it -> queued
//   C6  has_skip -> GEMM + in-place unit ADD; RETIRED counts the host instruction once
//   C7  Lane B push, legacy 64-bit push -> errors; SET_NSPLIT -> ignored (counted)
//   C8  queue overflow -> Q_OVF, STATUS bits; STATUS write clears the sticky error
//   C9  extension registers are one-shot: a later old-style push does not reuse them
//   C10 float ELEM ADD scales -> integer path close to round((A*sa + B*sb)/so)
//   C11 padding per side and padding value
//   C12 dilation: effective kernel, tile windows, one-shot, 0 = 1, 1x1 keeps 1, limit of the dilation pattern
//   C13 row sweep of the IFMAP feeder: xlim 256 accepted, 257 refused with SWEEP; Y_USED brings it back
#include <cmath>
#include <cstdio>
#include <cstring>
#include "has/has_mmio_compat.h"
#include "has/gvu_quant.h"

using namespace has;
namespace M = has::mmio;

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static uint32_t ext(int i) { return M::EXT_BASE + 4u * uint32_t(i); }

// ELEM_WISE ADD as HasElemwise computes it (drawing ELEM_WISE: dequant A, B -> add int32 -> requant).
static int8_t add_ref(const AddQ &q, int a, int b, const Knobs &k)
{
    const int32_t da = dequant(a, q.zpA, decode_scale(q.SA, k.scale_fmt), q.sA, k);
    const int32_t db = dequant(b, q.zpB, decode_scale(q.SB, k.scale_fmt), q.sB, k);
    return requant(add_sat32(da, db), decode_scale(q.SO, k.scale_fmt), q.sO, q.zpO, k);
}

int main()
{
    // ---------------------------------------------------------------- C1
    {
        std::printf("[C1] old-style ELEM_WISE ADD (test_onnx_model.cpp instruction 2)\n");
        MmioCompat m;
        m.write(0x40000444, 0x3CA9200);
        m.write(0x40000448, 0x3CA9600);
        m.write(0x40000408, 0x3CACC00);
        m.write(0x40000450, 1024);
        m.write(0x40000454, 0);
        m.write(0x40000464, 1024);
        m.write(0x40000468, 1024);
        m.write(0x40000310, 0x15);
        LayerInstr li;
        CHECK(m.queued() == 1 && m.last_error() == Err::NONE, "queued %zu err %d", m.queued(), int(m.last_error()));
        CHECK(m.pop(li) && li.opcode == 0x15 && li.mode == 0 && li.a_addr == 0x3CA9200 && li.b_addr == 0x3CA9600 &&
              li.out_addr == 0x3CACC00 && li.n == 1024, "decoded fields");
        long bad = 0;
        for (int kk = 0; kk < 2; kk++)
        {
            const Knobs k = kk ? Knobs::compat() : Knobs();
            for (int a = -128; a < 128; a++)
                for (int b = -128; b < 128; b++)
                {
                    const int want = std::max(-128, std::min(127, a + b));
                    if (add_ref(li.add, a, b, k) != want) bad++;
                }
        }
        CHECK(bad == 0, "%ld of 131072 pairs differ from sat8(A+B)", bad);
        std::printf("  unit-scale ADD vs sat8(A+B): %ld mismatches over 2 x 65536 pairs (HAS + compat knobs)\n", bad);
    }
    // ---------------------------------------------------------------- C2
    {
        std::printf("[C2] ELEM_WISE modes outside the HAS ISA\n");
        MmioCompat m;
        const struct { uint32_t mode, a_len, b_len; Err want; } cs[] = {
            {3, 1024, 1024, Err::UNSUPPORTED_MODE},   // test_onnx_model.cpp instruction 1 (Sub)
            {2, 4096, 1024, Err::UNSUPPORTED_MODE},   // instruction 3 (Mul, broadcast)
            {0, 409600, 1024, Err::BROADCAST},        // instruction 25 (Add with broadcast B)
        };
        for (const auto &c : cs)
        {
            m.write(M::STATUS, 0);
            m.write(0x40000450, c.mode == 0 ? 409600 : c.a_len);
            m.write(0x40000454, c.mode);
            m.write(0x40000464, c.a_len);
            m.write(0x40000468, c.b_len);
            m.write(0x40000310, 0x15);
            CHECK(m.last_error() == c.want && m.queued() == 0, "mode %u: err %d queued %zu", c.mode, int(m.last_error()), m.queued());
        }
        CHECK((m.read(M::STATUS) & 4) != 0, "sticky error bit");
    }
    // ---------------------------------------------------------------- C3
    {
        std::printf("[C3] old-style GEMM_FUSED (test_onnx_model.cpp instruction 24)\n");
        MmioCompat m;
        m.write(0x40000400, 0x468B700);
        m.write(0x40000404, 0x45C3300);
        m.write(0x40000408, 0x46EF700);
        m.write(0x40000410, 4096);
        m.write(0x40000414, 768);
        m.write(0x40000418, 768);
        m.write(0x4000042C, 0);
        m.write(0x40000438, 0x3E000000);   // 0.125
        m.write(0x4000043C, 0x3F800000);
        m.write(0x40000440, 0x3F800000);
        m.write(0x40000310, 0x12);
        LayerInstr li;
        CHECK(m.pop(li) && li.opcode == 0x12, "queued GEMM");
        const LayerGeom &g = li.geom;
        CHECK(li.in_c == 768 && li.in_h == 1 && li.in_w == 4096 && g.cout == 768 && g.oh == 1 && g.ow == 4096 && g.kh == 1,
              "1x1 geometry");
        const SramCaps cap;
        CHECK(li.default_tiling && g.cout_t * 768 <= cap.b_bytes && 768 * g.w_t <= cap.a_bytes && g.cout_t * g.w_t <= cap.c_elems,
              "fallback tiling %d x %d fits", g.cout_t, g.w_t);
        const double got = double(li.bcast_S) * std::ldexp(1.0, -li.bcast_s);
        CHECK(li.from_float_scale && std::fabs(got / 0.125 - 1.0) < std::ldexp(1.0, -30), "scale 0.125 -> S %u s %d", li.bcast_S, li.bcast_s);
        std::printf("  tiling cout_t %d, w_t %d (%ld tiles), scale S=%u s=%d\n", g.cout_t, g.w_t, TileIter(g).count(), li.bcast_S, li.bcast_s);
        // K = 4096 (ViT MLP down-projection): one weight buffer holds only 20 output channels, one IFmap buffer 19 positions
        MmioCompat m2;
        m2.write(0x40000410, 197); m2.write(0x40000414, 4096); m2.write(0x40000418, 768); m2.write(0x40000310, 0x12);
        CHECK(m2.pop(li) && li.geom.cout_t == 82944 / 4096 && li.geom.w_t == 80896 / 4096, "deep K tiling: cout_t %d w_t %d",
              li.geom.cout_t, li.geom.w_t);
        std::printf("  K=4096 fallback tiling: cout_t %d, w_t %d -> %ld tiles\n", li.geom.cout_t, li.geom.w_t, TileIter(li.geom).count());
        MmioCompat m3;
        m3.write(0x40000410, 197); m3.write(0x40000414, 90000); m3.write(0x40000418, 64); m3.write(0x40000310, 0x12);
        CHECK(m3.queued() == 0 && m3.last_error() == Err::NO_TILING, "K > one weight buffer -> NO_TILING");
    }
    // ---------------------------------------------------------------- C4
    {
        std::printf("[C4] new-style conv, YOLOv8m stem (3x640x640 -> 48x320x320, k3 s2 p1, tile 8x19x160)\n");
        MmioCompat m;
        m.write(0x40000400, 0x1000); m.write(0x40000404, 0x2000); m.write(0x40000408, 0x3000); m.write(0x4000040C, 0x4000);
        m.write(0x4000041C, 3); m.write(0x40000420, 3); m.write(0x40000424, 2); m.write(0x40000428, 1);
        m.write(0x4000042C, 2);
        const uint32_t v[] = {3, 640, 640, 48, 320, 320, 8, 19, 160, 0x5000, 0x6000, 0x7000, 0, 0};
        for (int i = 0; i < 14; i++) m.write(ext(i), v[i]);
        m.write(0x40000310, 0x12);
        LayerInstr li;
        CHECK(m.pop(li) && li.per_channel && !li.from_float_scale && !li.default_tiling && li.lut_addr == 0x7000, "per-channel conv");
        TileIter it(li.geom);
        CHECK(it.count() == 204, "iterator tiles %ld (planner: 204)", it.count());
        const TileGeom t0 = it.at(0), tl = it.at(it.count() - 1);
        CHECK(t0.iy0 == -1 && t0.iy1 == 38 && t0.ix0 == -1 && t0.ix1 == 320 && t0.x_used == 8 && t0.y_used == 32,
              "tile 0 window = prog.bin tile 0 ([-1,38) x [-1,320), X 8, Y 32)");
        std::printf("  %ld tiles, tile 0 iy[%d,%d) ix[%d,%d), last c[%d,%d) oy[%d,%d) ox[%d,%d)\n", it.count(), t0.iy0, t0.iy1,
                    t0.ix0, t0.ix1, tl.c0, tl.c1, tl.oy0, tl.oy1, tl.ox0, tl.ox1);
    }
    // ---------------------------------------------------------------- C5
    {
        std::printf("[C5] SiLU needs LUT_ADDR\n");
        MmioCompat m;
        m.write(0x4000042C, 2);
        m.write(0x40000310, 0x12);
        CHECK(m.last_error() == Err::NEED_LUT && m.queued() == 0, "SiLU without LUT rejected");
        m.write(ext(M::X_LUT_ADDR), 0x100);
        m.write(0x40000310, 0x12);
        CHECK(m.queued() == 1, "SiLU with LUT queued");
        m.write(0x4000042C, 1);
        m.write(0x40000310, 0x12);
        CHECK(m.queued() == 2, "ReLU needs no LUT (built-in table)");
    }
    // ---------------------------------------------------------------- C6
    {
        std::printf("[C6] has_skip -> GEMM + unit ADD, retired once\n");
        MmioCompat m;
        m.write(0x40000408, 0x9000); m.write(0x40000430, 1); m.write(0x40000434, 0xA000);
        m.write(0x40000410, 64); m.write(0x40000414, 32); m.write(0x40000418, 16);
        m.write(0x40000310, 0x12);
        LayerInstr a, b;
        CHECK(m.queued() == 2 && m.n_skip_split == 1, "two internal instructions");
        CHECK(m.pop(a) && a.opcode == 0x12 && !a.last_of_host, "GEMM half");
        m.retire(a);
        CHECK(m.read(M::RETIRED) == 0 && m.busy(), "not retired after GEMM half");
        CHECK(m.pop(b) && b.opcode == 0x15 && b.mode == 0 && b.a_addr == 0x9000 && b.out_addr == 0x9000 && b.b_addr == 0xA000 &&
              b.n == 16u * 64u && b.last_of_host, "in-place ADD half");
        m.retire(b);
        CHECK(m.read(M::RETIRED) == 1 && !m.busy() && m.irq(), "retired once, idle, irq");
    }
    // ---------------------------------------------------------------- C7
    {
        std::printf("[C7] dual-lane / legacy pushes\n");
        MmioCompat m;
        m.write(0x40000314, 0x12);
        CHECK(m.last_error() == Err::LANE_B && m.queued() == 0, "Lane B rejected");
        m.write(0x40000300, 0x12);
        m.write(0x40000304, 0x12000000);
        CHECK(m.last_error() == Err::LEGACY64 && m.queued() == 0, "64-bit legacy rejected");
        m.write(M::STATUS, 0);
        m.write(0x40000310, 0x05);
        CHECK(m.last_error() == Err::NONE && m.n_nsplit_ignored == 1 && m.queued() == 0, "SET_NSPLIT ignored");
        m.write(0x40000310, 0x13);
        CHECK(m.last_error() == Err::BAD_GEOM && m.queued() == 0, "FUSED_ATTN without PARAM_ADDR rejected");
        m.write(M::STATUS, 0);
        m.write(0x40000444, 0x100); m.write(0x40000448, 0x200); m.write(0x4000044C, 0x300); m.write(0x40000450, 197);
        m.write(0x40000458, 64); m.write(0x40000408, 0x400); m.write(ext(M::X_PARAM_ADDR), 0x500); m.write(ext(M::X_MASK_ADDR), 0x600);
        m.write(0x40000310, 0x13);
        LayerInstr a;
        CHECK(m.last_error() == Err::NONE && m.pop(a) && a.opcode == 0x13 && a.a_addr == 0x100 && a.b_addr == 0x200 &&
              a.v_addr == 0x300 && a.seq_len == 197 && a.head_dim == 64 && a.rows == 197 && a.param_addr == 0x500 &&
              a.mask_addr == 0x600 && a.out_addr == 0x400, "FUSED_ATTN decoded");
        m.retire(a);
        m.write(0x40000400, 0x700); m.write(0x40000450, 768); m.write(ext(M::X_ROWS), 197); m.write(ext(M::X_PARAM_ADDR), 0x800);
        m.write(0x40000310, 0x14);
        CHECK(m.last_error() == Err::NONE && m.pop(a) && a.opcode == 0x14 && a.in_addr == 0x700 && a.seq_len == 768 &&
              a.rows == 197 && a.param_addr == 0x800, "LAYERNORM decoded");
        m.retire(a);
        // 0x7F is no opcode of the model in any configuration (0x16 is DMA_TENSOR, accepted by default: HAS_DMA_TENSOR=1)
        m.write(0x40000310, 0x7F);
        CHECK(m.last_error() == Err::UNSUPPORTED_OP, "unknown opcode rejected");
        {
            MmioCompat md;
            md.dma_tensor = false;   // as HAS_DMA_TENSOR=0
            md.write(0x40000310, 0x16);
            CHECK(md.last_error() == Err::UNSUPPORTED_OP && md.queued() == 0, "DMA_TENSOR refused when off (HAS_DMA_TENSOR=0)");
        }
        CHECK(!m.owns(0x40000504) && m.owns(0x40000318) && m.owns(ext(M::X_POOL_MODE)) && !m.owns(M::EXT_END), "address ownership");
    }
    // ---------------------------------------------------------------- C8
    {
        std::printf("[C8] queue overflow and STATUS\n");
        MmioCompat m(4);
        for (int i = 0; i < 5; i++) m.write(0x40000310, 0x12);
        const uint32_t st = m.read(M::STATUS);
        CHECK(m.queued() == 4 && m.last_error() == Err::Q_OVF, "4 queued, 5th overflows");
        CHECK((st & 1) && (st & 2) && (st & 4) && ((st >> 8) & 0xFF) == 4 && (st >> 16) == uint32_t(Err::Q_OVF), "STATUS 0x%08x", st);
        m.write(M::STATUS, 1);
        CHECK((m.read(M::STATUS) & 4) == 0 && m.last_error() == Err::NONE, "STATUS write clears sticky error");
    }
    // ---------------------------------------------------------------- C9
    {
        std::printf("[C9] extension registers are one-shot\n");
        MmioCompat m;
        const uint32_t v[] = {16, 40, 40, 32, 40, 40, 32, 8, 40};
        for (int i = 0; i < 9; i++) m.write(ext(i), v[i]);
        m.write(0x4000041C, 3); m.write(0x40000420, 3); m.write(0x40000428, 1);
        m.write(0x40000310, 0x12);
        m.write(0x4000041C, 1); m.write(0x40000420, 1); m.write(0x40000428, 0);
        m.write(0x40000410, 100); m.write(0x40000414, 64); m.write(0x40000418, 32);
        m.write(0x40000310, 0x12);
        LayerInstr a, b;
        CHECK(m.pop(a) && !a.default_tiling && a.geom.oh == 40 && a.geom.h_t == 8, "first push uses extension geometry");
        CHECK(m.pop(b) && b.default_tiling && b.geom.oh == 1 && b.geom.ow == 100 && b.in_c == 64, "second push is old-style m/k/n");
    }
    // ---------------------------------------------------------------- C10
    {
        std::printf("[C10] float ELEM ADD scales -> integer path\n");
        MmioCompat m;
        m.write(0x40000450, 4096); m.write(0x40000454, 0);
        m.write(0x40000458, fbits(0.05f)); m.write(0x4000045C, fbits(0.02f)); m.write(0x40000460, fbits(0.04f));
        m.write(0x40000310, 0x15);
        LayerInstr li;
        CHECK(m.pop(li) && li.from_float_scale, "queued, float path");
        long off1 = 0, worst = 0;
        const Knobs k;
        for (int a = -128; a < 128; a++)
            for (int b = -128; b < 128; b++)
            {
                const double ideal = (a * 0.05 + b * 0.02) / 0.04;
                const int want = int(std::max(-128.0, std::min(127.0, std::floor(ideal + 0.5))));
                const long d = std::labs(long(add_ref(li.add, a, b, k)) - want);
                off1 += d > 0;
                worst = std::max(worst, d);
            }
        CHECK(worst <= 1, "worst |diff| %ld vs round((A*sa+B*sb)/so)", worst);
        std::printf("  vs round((A*0.05 + B*0.02)/0.04): %ld of 65536 pairs off by 1, worst %ld (float path is approximate by design)\n",
                    off1, worst);
    }
    // ---------------------------------------------------------------- C11
    {
        std::printf("[C11] padding per side and padding value (PAD_T / PAD_L / PAD_B / PAD_R, PAD_VALUE)\n");
        // 8x20x20 -> 8x10x10, k3 s2: TFLite SAME needs (top, left, bottom, right) = (0, 0, 1, 1)
        auto conv = [](MmioCompat &m, uint32_t pad)
        {
            m.write(0x40000400, 0x1000); m.write(0x40000404, 0x2000); m.write(0x40000408, 0x3000); m.write(0x4000040C, 0x4000);
            m.write(0x4000041C, 3); m.write(0x40000420, 3); m.write(0x40000424, 2); m.write(0x40000428, pad);
            m.write(0x4000042C, 0);
            const uint32_t v[] = {8, 20, 20, 8, 10, 10, 8, 5, 10, 0x5000, 0x6000, 0, 0, 0};
            for (int i = 0; i < 14; i++) m.write(ext(i), v[i]);
        };
        LayerInstr li;
        MmioCompat m;
        conv(m, 0);
        m.write(0x40000310, 0x12);
        CHECK(m.queued() == 0 && m.last_error() == Err::BAD_GEOM, "PAD 0 alone: the last output row / column has no input -> BAD_GEOM");
        m.write(M::STATUS, 1);
        conv(m, 0);
        m.write(ext(M::X_PAD_B), 1); m.write(ext(M::X_PAD_R), 1);
        m.write(ext(M::X_PAD_VALUE), 0xFFFFFF80u);   // -128
        m.write(0x40000310, 0x12);
        CHECK(m.pop(li) && li.geom.pad_t == 0 && li.geom.pad_l == 0 && li.geom.pad_b == 1 && li.geom.pad_r == 1 && li.pad_val == -128,
              "pads (0, 0, 1, 1), value -128: t %d l %d b %d r %d value %d", li.geom.pad_t, li.geom.pad_l, li.geom.pad_b, li.geom.pad_r,
              li.pad_val);
        {
            TileIter it(li.geom);
            const TileGeom t0 = it.at(0), tl = it.at(it.count() - 1);
            CHECK(it.count() == 2 && t0.iy0 == 0 && t0.ix0 == 0 && tl.iy1 == 21 && tl.ix1 == 21,
                  "windows: first starts at (0, 0), last ends at row 21 / column 21 (one past the image); got %ld tiles, [%d,%d) x [%d,%d)",
                  it.count(), tl.iy0, tl.iy1, tl.ix0, tl.ix1);
        }
        // one-shot: the next push without the new registers is the symmetric PAD again, value 0
        conv(m, 1);
        m.write(0x40000310, 0x12);
        CHECK(m.pop(li) && li.geom.pad_t == 1 && li.geom.pad_l == 1 && li.geom.pad_b == 1 && li.geom.pad_r == 1 && li.pad_val == 0,
              "PAD 1 without the new registers: all four sides 1, value 0");
        // the four registers replace PAD: with only top / left written, bottom / right are 0 even though PAD is 1
        MmioCompat m2;
        m2.write(0x4000041C, 3); m2.write(0x40000420, 3); m2.write(0x40000424, 1); m2.write(0x40000428, 1);
        const uint32_t v2[] = {8, 20, 20, 8, 20, 20, 8, 5, 20, 0x5000, 0x6000, 0, 0, 0};
        for (int i = 0; i < 14; i++) m2.write(ext(i), v2[i]);
        m2.write(ext(M::X_PAD_T), 1); m2.write(ext(M::X_PAD_L), 1);   // bottom / right not written = 0
        m2.write(0x40000310, 0x12);
        CHECK(m2.queued() == 0 && m2.last_error() == Err::BAD_GEOM, "PAD_T / PAD_L written, bottom / right 0: output 20x20 does not fit -> BAD_GEOM");
        CHECK(m.owns(ext(M::X_PAD_R)) && ext(M::X_PAD_R) == 0x400004F8, "PAD_R at 0x400004F8");
    }
    // ---------------------------------------------------------------- C12
    {
        std::printf("[C12] dilation (DILATION)\n");
        // 4x24x24, k3 s1, dilation 9, PAD 9 -> 8x24x24: effective kernel 19
        auto conv = [](MmioCompat &m, uint32_t kw)
        {
            m.write(0x40000400, 0x1000); m.write(0x40000404, 0x2000); m.write(0x40000408, 0x3000); m.write(0x4000040C, 0x4000);
            m.write(0x4000041C, 3); m.write(0x40000420, kw); m.write(0x40000424, 1); m.write(0x40000428, 9);
            m.write(0x4000042C, 0);
            const uint32_t v[] = {4, 24, 24, 8, 24, 24, 8, 8, 24, 0x5000, 0x6000, 0, 0, 0};
            for (int i = 0; i < 14; i++) m.write(ext(i), v[i]);
        };
        LayerInstr li;
        MmioCompat m;
        conv(m, 3);
        m.write(0x40000310, 0x12);
        CHECK(m.pop(li) && li.geom.dil == 1 && li.geom.ekh() == 3, "DILATION not written: dilation 1, got %d", li.geom.dil);
        conv(m, 3);
        m.write(ext(M::X_DILATION), 9);
        m.write(0x40000310, 0x12);
        CHECK(m.pop(li) && li.geom.dil == 9 && li.geom.ekh() == 19 && li.geom.ekw() == 19, "dilation 9: effective kernel 19, got dil %d", li.geom.dil);
        {
            TileIter it(li.geom);
            const TileGeom t0 = it.at(0), tl = it.at(it.count() - 1);
            CHECK(it.count() == 3 && t0.iy0 == -9 && t0.iy1 == 17 && t0.ix0 == -9 && t0.ix1 == 33 && tl.iy1 == 33,
                  "windows with dilation 9: first [%d,%d) x [%d,%d), last ends at row %d; %ld tiles", t0.iy0, t0.iy1, t0.ix0, t0.ix1,
                  tl.iy1, it.count());
        }
        // one-shot: the next push without DILATION has dilation 1 again
        conv(m, 3);
        m.write(ext(M::X_PAD_T), 1); m.write(ext(M::X_PAD_L), 1); m.write(ext(M::X_PAD_B), 1); m.write(ext(M::X_PAD_R), 1);
        m.write(0x40000310, 0x12);
        CHECK(m.pop(li) && li.geom.dil == 1, "DILATION is one-shot: next push has dilation 1, got %d", li.geom.dil);
        // 0 means 1; a 1x1 kernel keeps dilation 1 (one tap)
        conv(m, 3);
        m.write(ext(M::X_PAD_T), 1); m.write(ext(M::X_PAD_L), 1); m.write(ext(M::X_PAD_B), 1); m.write(ext(M::X_PAD_R), 1);
        m.write(ext(M::X_DILATION), 0);
        m.write(0x40000310, 0x12);
        CHECK(m.pop(li) && li.geom.dil == 1, "DILATION 0 = 1, got %d", li.geom.dil);
        MmioCompat m1;
        m1.write(0x4000041C, 1); m1.write(0x40000420, 1); m1.write(0x40000424, 1); m1.write(0x40000428, 0);
        const uint32_t v1[] = {4, 24, 24, 8, 24, 24, 8, 8, 24, 0x5000, 0x6000, 0, 0, 0};
        for (int i = 0; i < 14; i++) m1.write(ext(i), v1[i]);
        m1.write(ext(M::X_DILATION), 5);
        m1.write(0x40000310, 0x12);
        CHECK(m1.pop(li) && li.geom.dil == 1, "1x1 kernel with DILATION 5 -> dilation 1, got %d", li.geom.dil);
        // limit of the dilation pattern: (KW - 1) x DILATION <= 63; 8 taps 9 apart = 63 fits, 9 taps 8 apart = 64 does not
        auto row = [](MmioCompat &m, uint32_t kw, uint32_t d)
        {
            m.write(0x4000041C, 1); m.write(0x40000420, kw); m.write(0x40000424, 1); m.write(0x40000428, 0);
            const uint32_t v[] = {2, 4, 80, 8, 4, 80 - (kw - 1) * d, 8, 4, 8, 0x5000, 0x6000, 0, 0, 0};
            for (int i = 0; i < 14; i++) m.write(ext(i), v[i]);
            m.write(ext(M::X_DILATION), d);
        };
        MmioCompat m3;
        row(m3, 8, 9);
        m3.write(0x40000310, 0x12);
        CHECK(m3.queued() == 1 && m3.last_error() == Err::NONE, "1x8 kernel, dilation 9 (span 63): queued");
        MmioCompat m4;
        row(m4, 9, 8);
        m4.write(0x40000310, 0x12);
        CHECK(m4.queued() == 0 && m4.last_error() == Err::BAD_GEOM, "1x9 kernel, dilation 8 (span 64): BAD_GEOM");
        MmioCompat m5;
        conv(m5, 3);
        m5.write(ext(M::X_DILATION), 0xFFFFFFFFu);
        m5.write(0x40000310, 0x12);
        CHECK(m5.queued() == 0 && m5.last_error() == Err::BAD_GEOM, "DILATION -1: BAD_GEOM");
        CHECK(m.owns(ext(M::X_DILATION)) && ext(M::X_DILATION) == 0x400004FC && m.owns(ext(M::X_TMAJ_C0)) && ext(M::X_TMAJ_C0) == 0x40000500 &&
              !m.owns(M::EXT_END) && M::EXT_END == 0x40000504,
              "DILATION at 0x400004FC, TMAJ_C0 at 0x40000500, extension registers end at 0x40000504");
        // TMAJ_C0 (0x40000500): written = channels from it on token-major; one-shot; out of [0, OUT_C) or with HAS_SKIP refused
        MmioCompat m6;
        conv(m6, 3);
        m6.write(0x40000310, 0x12);
        CHECK(m6.pop(li) && li.tmaj_c0 == -1, "TMAJ_C0 not written: -1, got %d", li.tmaj_c0);
        conv(m6, 3);
        m6.write(ext(M::X_TMAJ_C0), 0);
        m6.write(0x40000310, 0x12);
        CHECK(m6.pop(li) && li.tmaj_c0 == 0, "TMAJ_C0 0 written: 0, got %d", li.tmaj_c0);
        conv(m6, 3);
        m6.write(0x40000310, 0x12);
        CHECK(m6.pop(li) && li.tmaj_c0 == -1, "TMAJ_C0 is one-shot: next push -1, got %d", li.tmaj_c0);
        conv(m6, 3);
        m6.write(ext(M::X_TMAJ_C0), uint32_t(li.geom.cout));
        m6.write(0x40000310, 0x12);
        CHECK(m6.queued() == 0 && m6.last_error() == Err::BAD_GEOM, "TMAJ_C0 = OUT_C: BAD_GEOM");
        MmioCompat m7;
        conv(m7, 3);
        m7.write(0x40000430, 1);
        m7.write(ext(M::X_TMAJ_C0), 0);
        m7.write(0x40000310, 0x12);
        CHECK(m7.queued() == 0 && m7.last_error() == Err::UNSUPPORTED_MODE, "TMAJ_C0 with HAS_SKIP: UNSUPPORTED_MODE");
    }

    // ---------------------------------------------------------------- C13
    {
        std::printf("[C13] row sweep of the IFMAP feeder (xlim <= 256, error SWEEP)\n");
        // 1 x KW kernel, 32 positions per context, output 1 x 32: xlim = 1 + 31 s + bw + 1 - (bw mod 2) + 32
        auto row = [](MmioCompat &m, uint32_t kw, uint32_t s, uint32_t d, uint32_t y_used)
        {
            const uint32_t bw = (kw - 1) * d + 1, in_w = 31 * s + bw;
            m.write(0x4000041C, 1); m.write(0x40000420, kw); m.write(0x40000424, s); m.write(0x40000428, 0);
            const uint32_t v[] = {2, 1, in_w, 8, 1, 32, 8, 1, 32, 0x5000, 0x6000, 0, 0, 0};
            for (int i = 0; i < 14; i++) m.write(ext(i), v[i]);
            if (d > 1) m.write(ext(M::X_DILATION), d);
            if (y_used) m.write(ext(M::X_Y_USED), y_used);
            m.write(0x40000310, 0x12);
        };
        MmioCompat a, b, c, e;
        row(a, 3, 6, 18, 0);   // xlim 256
        CHECK(a.queued() == 1 && a.last_error() == Err::NONE, "xlim 256 (1x3, stride 6, dilation 18): queued");
        row(b, 3, 7, 3, 0);    // xlim 257
        CHECK(b.queued() == 0 && b.last_error() == Err::SWEEP, "xlim 257 (1x3, stride 7, dilation 3): SWEEP, got %d", int(b.last_error()));
        row(c, 3, 8, 1, 0);    // 3x3-like row, stride 8, 32 positions: xlim 284
        CHECK(c.queued() == 0 && c.last_error() == Err::SWEEP, "stride 8 with 32 positions per context: SWEEP");
        row(e, 3, 8, 1, 16);   // the same with Y_USED 16: xlim 156
        CHECK(e.queued() == 1 && e.last_error() == Err::NONE, "stride 8 with Y_USED 16: queued");
    }

    std::printf("[tb_has_mmio_compat] RESULT: %s (%d failed checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
