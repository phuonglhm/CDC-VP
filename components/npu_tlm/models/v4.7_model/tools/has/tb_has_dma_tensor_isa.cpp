// tb_has_dma_tensor_isa.cpp -- unit test of opcode 0x16 DMA_TENSOR in has/has_mmio_compat.h (pure C++).
//   D1  knob off (HAS_DMA_TENSOR unset / 0): push 0x16 -> UNSUPPORTED_OP, nothing queued (negative control)
//   D2  knob on: COPY (A_ADDR, OUT_ADDR, LEN, MODE_PACK 0) -> one LayerInstr {0x16, a_addr, out_addr, n}, RETIRED counts it
//   D3  refused with BAD_GEOM: LEN 0, source or destination past 2^32, overlapping regions (both directions, equal)
//   D4  touching regions (destination right after the source, or right before it) are accepted
//   D5  MODE_PACK != 0 -> UNSUPPORTED_MODE, also 0x01000000 (packed form of mode 1)
//   D6  the registers are the old sticky ones: a second push reuses them; extension registers are not needed
#include <cstdio>
#include "has/has_mmio_compat.h"

using namespace has;
namespace M = has::mmio;

static int fails = 0, n_cases = 0, n_refused = 0, n_accepted = 0, n_other = 0;
// One line per push-time case: what was pushed, what was expected, what happened.
static void case_line(bool ok, bool refuse, const char *expect, uint32_t src, uint32_t dst, uint32_t n, uint32_t mode)
{
    n_cases++;
    (ok ? (refuse ? n_refused : n_accepted) : n_other)++;
    std::printf("  case %2d: src 0x%08x dst 0x%08x len %u mode 0x%x -> expected %s: %s\n", n_cases, src, dst, n, mode, expect, ok ? "ok" : "NOT AS EXPECTED");
}
#define CHECK(cond, ...) do { if (!(cond)) { fails++; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void copy_regs(MmioCompat &m, uint32_t src, uint32_t dst, uint32_t n, uint32_t mode = 0)
{
    m.write(M::A_ADDR, src); m.write(M::OUT_ADDR, dst); m.write(M::LEN, n); m.write(M::MODE_PACK, mode);
}
static bool refused_with(MmioCompat &m, Err e)
{
    const bool r = m.queued() == 0 && m.last_error() == e && (m.status() & 4u);
    m.write(M::STATUS, 0);
    return r;
}

int main()
{
    {   // D1
        MmioCompat m;
        m.dma_tensor = false;
        copy_regs(m, 0x1000, 0x2000, 64);
        m.write(M::PUSH_A, 0x16);
        const bool r = refused_with(m, Err::UNSUPPORTED_OP);
        CHECK(r, "knob off: 0x16 must be refused with UNSUPPORTED_OP");
        case_line(r, true, "refused, UNSUPPORTED_OP (knob off)", 0x1000, 0x2000, 64, 0);
        std::printf("D1 knob off refused: %s\n", fails ? "FAIL" : "ok");
    }
    MmioCompat m;
    m.dma_tensor = true;
    {   // D2
        copy_regs(m, 0x1003, 0x40011, 151296);
        m.write(M::PUSH_A, 0x16);
        LayerInstr li;
        const bool q = m.queued() == 1 && m.last_error() == Err::NONE;
        CHECK(q, "COPY not queued");
        const bool f = m.pop(li) && li.opcode == 0x16 && li.a_addr == 0x1003 && li.out_addr == 0x40011 && li.n == 151296 && li.mode == 0;
        CHECK(f, "COPY fields");
        case_line(q && f, false, "accepted, one instruction with these fields", 0x1003, 0x40011, 151296, 0);
        m.retire(li);
        CHECK(m.read(M::RETIRED) == 1 && m.n_dma_tensor == 1, "RETIRED / n_dma_tensor");
    }
    {   // D3
        const uint32_t cases[][3] = {{0x1000, 0x2000, 0}, {0xFFFFFF00u, 0x1000, 0x101}, {0x1000, 0xFFFFFFF0u, 0x11},
                                     {0x1000, 0x1010, 0x20}, {0x1010, 0x1000, 0x20}, {0x1000, 0x1000, 1}, {0x1000, 0x10FF, 0x100}};
        for (const auto &c : cases)
        {
            copy_regs(m, c[0], c[1], c[2]);
            m.write(M::PUSH_A, 0x16);
            const bool r = refused_with(m, Err::BAD_GEOM);
            CHECK(r, "src 0x%x dst 0x%x n %u must be refused with BAD_GEOM", c[0], c[1], c[2]);
            const bool past = uint64_t(c[0]) + c[2] > (1ull << 32) || uint64_t(c[1]) + c[2] > (1ull << 32);
            case_line(r, true, c[2] == 0 ? "refused, BAD_GEOM (length 0)" : past ? "refused, BAD_GEOM (past 2^32)" : "refused, BAD_GEOM (overlap)",
                      c[0], c[1], c[2], 0);
        }
    }
    {   // D4
        const uint32_t cases[][3] = {{0x1000, 0x1100, 0x100}, {0x1100, 0x1000, 0x100}, {0xFFFFFF00u, 0x1000, 0x100}};
        for (const auto &c : cases)
        {
            copy_regs(m, c[0], c[1], c[2]);
            m.write(M::PUSH_A, 0x16);
            LayerInstr li;
            const bool a = m.queued() == 1 && m.pop(li) && li.n == c[2];
            CHECK(a, "src 0x%x dst 0x%x n %u must be accepted", c[0], c[1], c[2]);
            case_line(a, false, uint64_t(c[0]) + c[2] == (1ull << 32) ? "accepted (source ends exactly at 2^32)" : "accepted (regions touch, no overlap)",
                      c[0], c[1], c[2], 0);
            m.retire(li);
        }
    }
    {   // D5
        copy_regs(m, 0x1000, 0x2000, 64, 1);
        m.write(M::PUSH_A, 0x16);
        const bool r1 = refused_with(m, Err::UNSUPPORTED_MODE);
        CHECK(r1, "MODE_PACK 1 must be refused with UNSUPPORTED_MODE");
        case_line(r1, true, "refused, UNSUPPORTED_MODE", 0x1000, 0x2000, 64, 1);
        // MODE_PACK goes through the packing rule of instruction_decoder.h: 0x01000000 packs to mode 1 -> refused
        copy_regs(m, 0x1000, 0x2000, 64, 0x01000000u);
        m.write(M::PUSH_A, 0x16);
        const bool r2 = refused_with(m, Err::UNSUPPORTED_MODE);
        CHECK(r2, "MODE_PACK 0x01000000 (packed mode 1) must be refused with UNSUPPORTED_MODE");
        case_line(r2, true, "refused, UNSUPPORTED_MODE (packed mode 1)", 0x1000, 0x2000, 64, 0x01000000u);
    }
    {   // D6
        copy_regs(m, 0x4000, 0x8000, 300);
        m.write(M::PUSH_A, 0x16);
        m.write(M::PUSH_A, 0x16);
        LayerInstr a, b;
        const bool s2 = m.queued() == 2 && m.pop(a) && m.pop(b) && a.a_addr == b.a_addr && a.out_addr == b.out_addr && b.n == 300;
        CHECK(s2, "sticky registers reused by the second push");
        case_line(s2, false, "accepted twice from the same registers", 0x4000, 0x8000, 300, 0);
    }
    std::printf("push-time checks: cases %d, refused as expected %d, accepted as expected %d, not as expected %d\n", n_cases, n_refused,
                n_accepted, n_other);
    std::printf("tb_has_dma_tensor_isa: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
