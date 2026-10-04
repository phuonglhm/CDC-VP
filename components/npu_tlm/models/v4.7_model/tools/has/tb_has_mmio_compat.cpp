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
        m.write(0x40000310, 0x16);
        CHECK(m.last_error() == Err::UNSUPPORTED_OP, "unknown opcode rejected");
        CHECK(!m.owns(0x40000500) && m.owns(0x40000318) && m.owns(ext(M::X_POOL_MODE)) && !m.owns(M::EXT_END), "address ownership");
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

    std::printf("[tb_has_mmio_compat] RESULT: %s (%d failed checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
