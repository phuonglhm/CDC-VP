// tb_has_sram_check.cpp -- unit test of has/sram_limits.h (SRAM buffer limits of RTL revision 2, HAS_NEO_REV >= 2) and of the SramCaps default.
//   S1  limits per revision: 0 / 1 = HAS numbers, 2 = A min(98 304, model array 80 896), B 65 536, C 10 240 psums (320 rows)
//   S2  boundaries of one tile against revision 2: A, B, C each exactly at the limit (fits) and one step above (refused, only that buffer)
//   S3  revisions 0 / 1 never refuse (nothing is checked below revision 2)
//   S4  core_dot positions per pass and FUSED_ATTN fit (rev 2: C rows 320; rev < 2: the arrays of the model, as before)
//   S5  SramCaps of the environment's revision (run once with HAS_NEO_REV unset or 0 and once with HAS_NEO_REV=2)
//   S7  hw_check (has/hw_capability.h): B-SRAM only at revision 2, for a refused GEMM_FUSED tile or a FUSED_ATTN product that does not fit
//   S6  program (optional): every GEMM_FUSED of an mmio.txt through MmioCompat; --expect A B C ANY = layers over revision 2 per buffer
//       and in total (rev 0 / 1: always 0)
// Build (no SystemC needed):  g++ -std=c++17 -O1 -I. tools/has/tb_has_sram_check.cpp -o tools/has/tb_has_sram_check
// Run:  tb_has_sram_check [mmio.txt [--expect A B C ANY]]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "has/hw_capability.h"
#include "has/sram_limits.h"

using namespace has;

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// a 1x1 (k = 1) or kxk stride-1 layer, one uniform grid
static LayerInstr layer(int cin, int cin_t, int cout, int oh, int ow, int k, int cout_t, int h_t, int w_t, bool pad_tail = false)
{
    LayerInstr li;
    li.opcode = 0x12;
    li.in_c = cin;
    li.cin_t = cin_t;
    LayerGeom &g = li.geom;
    g.cout = cout; g.oh = oh; g.ow = ow;
    g.kh = g.kw = k;
    g.cout_t = cout_t; g.h_t = h_t; g.w_t = w_t;
    g.pad_tail = pad_tail;
    return li;
}

int main(int argc, char **argv)
{
    // ---------------------------------------------------------------- S1
    {
        std::printf("[S1] limits per revision\n");
        for (unsigned r : {0u, 1u})
        {
            const SramLimits l = sram_limits_rev(r);
            CHECK(l.a_bytes == 80896 && l.b_bytes == 82944 && l.c_psums == 24576 && l.c_rows == 768, "rev %u: %d %d %d %d", r, l.a_bytes, l.b_bytes, l.c_psums, l.c_rows);
        }
        const SramLimits l2 = sram_limits_rev(2);
        CHECK(l2.a_bytes == 80896 && l2.b_bytes == 65536 && l2.c_psums == 10240 && l2.c_rows == 320, "rev 2: %d %d %d %d", l2.a_bytes, l2.b_bytes, l2.c_psums, l2.c_rows);
        CHECK(sram_limits_rev(2, 98304).a_bytes == 98304, "A of the revision when the model array is as large as the RTL buffer");
        CHECK(sram_limits_rev(3).c_psums == 10240, "a revision above 2 takes revision 2");
    }
    // ---------------------------------------------------------------- S2
    {
        std::printf("[S2] boundaries at revision 2\n");
        struct Case { const char *name; LayerInstr li; unsigned mask; } cs[] = {
            {"C exact 32 x 320",            layer(64, 0, 64, 1, 640, 1, 32, 1, 320), 0u},
            {"C one step over (ow 321)",    layer(64, 0, 64, 1, 321, 1, 32, 1, 321), 4u},
            {"B exact cin 2048 x 32",       layer(2048, 0, 64, 1, 32, 1, 32, 1, 32), 0u},
            {"B one step over cin 2049",    layer(2049, 0, 64, 1, 32, 1, 32, 1, 32), 2u},
            {"A exact 2528 x 32 (cout_t 8)", layer(2528, 0, 8, 1, 64, 1, 8, 1, 32), 0u},
            {"A one position over (33)",    layer(2528, 0, 8, 1, 33, 1, 8, 1, 33), 1u},
            {"S1: B ok with cin_t 144",     layer(288, 144, 288, 20, 20, 3, 32, 10, 20), 0u},
            {"B over 3x3 cin 288 whole",    layer(288, 0, 288, 20, 20, 3, 32, 5, 20), 2u},
            {"C over PAD_TAIL 32 x 20 x 32",    layer(48, 0, 48, 160, 160, 3, 32, 20, 32, true), 4u},
            {"C ok PAD_TAIL 32 x 10 x 32",      layer(48, 0, 48, 160, 160, 3, 32, 10, 32, true), 0u},
        };
        for (const Case &c : cs)
        {
            const SramVerdict v = layer_sram_verdict(c.li, 2);
            CHECK(v.mask == c.mask, "%s: mask %u (A %lld B %lld C %lld) want %u", c.name, v.mask, (long long)v.use.a, (long long)v.use.b, (long long)v.use.c, c.mask);
        }
        std::printf("  %zu boundary cases\n", sizeof(cs) / sizeof(cs[0]));
    }
    // ---------------------------------------------------------------- S3
    {
        std::printf("[S3] revisions 0 / 1 never refuse\n");
        const LayerInstr big = layer(2049, 0, 64, 1, 321, 1, 32, 1, 321);
        for (unsigned r : {0u, 1u}) CHECK(layer_sram_verdict(big, r).mask == 0, "rev %u refused", r);
    }
    // ---------------------------------------------------------------- S4
    {
        std::printf("[S4] core_dot positions per pass, FUSED_ATTN fit\n");
        CHECK(core_dot_wmax(197, 2) == 320 && core_dot_wmax(64, 2) == 320, "rev 2 wmax 197: %u 64: %u", core_dot_wmax(197, 2), core_dot_wmax(64, 2));
        CHECK(core_dot_wmax(197, 0) == 384 && core_dot_wmax(64, 0) == 1248, "rev 0 wmax 197: %u 64: %u", core_dot_wmax(197, 0), core_dot_wmax(64, 0));
        CHECK(core_dot_wmax(0, 2) == 0 && core_dot_wmax(100000, 2) == 0, "no fit at all");
        LayerInstr fa;
        fa.opcode = 0x13; fa.rows = 197; fa.seq_len = 197; fa.head_dim = 64;
        CHECK(attn_sram_ok(fa, 2) && attn_sram_ok(fa, 0), "ViT-B head fits");
        fa.seq_len = 2048;
        CHECK(attn_sram_ok(fa, 2), "L 2048: 2048 x 32 = 65 536 fits");
        fa.seq_len = 2049;
        CHECK(!attn_sram_ok(fa, 2) && attn_sram_ok(fa, 0), "L 2049 over B at rev 2 only");
    }
    // ---------------------------------------------------------------- S5
    {
        std::printf("[S5] SramCaps of this run (neo_rev %u)\n", neo_rev());
        const SramCaps c;
        if (neo_rev() >= NEO_REV_2) CHECK(c.a_bytes == 80896 && c.b_bytes == 65536 && c.c_elems == 10240, "rev 2 caps %d %d %d", c.a_bytes, c.b_bytes, c.c_elems);
        else CHECK(c.a_bytes == 80896 && c.b_bytes == 82944 && c.c_elems == 24576, "rev 0 / 1 caps %d %d %d", c.a_bytes, c.b_bytes, c.c_elems);
        int ct = 0, wt = 0;
        CHECK(default_tiling(768, 768, 4096, c, ct, wt) && ct * 768 <= c.b_bytes && ct * wt <= c.c_elems, "fallback tiling %d x %d inside the caps", ct, wt);
    }
    // ---------------------------------------------------------------- S7
    {
        std::printf("[S7] hw_check B-SRAM (neo_rev %u)\n", neo_rev());
        const bool r2 = neo_rev() >= NEO_REV_2;
        const unsigned bit = 1u << HW_B_SRAM;
        const LayerInstr over = layer(2049, 0, 64, 1, 32, 1, 32, 1, 32), fits = layer(2048, 0, 64, 1, 32, 1, 32, 1, 32);
        CHECK(bool(hw_check(over).codes & bit) == r2, "refused tile: B-SRAM %d at revision %u", int(bool(hw_check(over).codes & bit)), neo_rev());
        CHECK(!(hw_check(fits).codes & bit), "a tile that fits must not carry B-SRAM");
        LayerInstr fa;
        fa.opcode = 0x13; fa.rows = 197; fa.seq_len = 2049; fa.head_dim = 64;
        CHECK(bool(hw_check(fa).codes & bit) == r2, "FUSED_ATTN L 2049: B-SRAM at revision %u", neo_rev());
        fa.seq_len = 197;
        CHECK(!(hw_check(fa).codes & bit), "FUSED_ATTN ViT head");
        CHECK(std::string(hw_code_name(HW_B_SRAM)) == "B-SRAM" && hw_code_severity(HW_B_SRAM) == 'T' && hw_code_severity(HW_P_POOLDENSE) == 'I', "code name / severity");
    }
    // ---------------------------------------------------------------- S6
    if (argc > 1)
    {
        std::printf("[S6] program %s\n", argv[1]);
        long expect[4] = {-1, -1, -1, -1};
        for (int i = 2; i < argc; i++)
            if (!std::strcmp(argv[i], "--expect") && i + 4 < argc) { for (int k = 0; k < 4; k++) expect[k] = std::atol(argv[i + 1 + k]); i += 4; }
        std::FILE *f = std::fopen(argv[1], "r");
        CHECK(f != nullptr, "cannot open %s", argv[1]);
        long n_gemm = 0, over[3] = {0, 0, 0}, any = 0, any_old = 0, n_attn = 0, attn_bad = 0;
        if (f)
        {
            MmioCompat m(1u << 20);
            char line[512];
            while (std::fgets(line, sizeof line, f))
            {
                if (line[0] != 'W') continue;
                unsigned a = 0, v = 0;
                if (std::sscanf(line + 1, "%x %x", &a, &v) != 2) continue;
                m.write(a, v);
                if (a != 0x40000310u) continue;
                LayerInstr li;
                while (m.pop(li))
                {
                    if (li.opcode == 0x13) { n_attn++; if (!attn_sram_ok(li, 2)) attn_bad++; }
                    if (li.opcode != 0x12) continue;
                    n_gemm++;
                    const SramVerdict v2 = layer_sram_verdict(li, 2);
                    for (int b = 0; b < 3; b++) if (v2.mask & (1u << b)) over[b]++;
                    if (v2.over()) any++;
                    if (layer_sram_verdict(li, 1).over()) any_old++;
                }
            }
            std::fclose(f);
        }
        std::printf("  GEMM_FUSED %ld, FUSED_ATTN %ld: revision 2 over A %ld B %ld C %ld, layers over %ld (FA not fitting %ld); revision 1 over %ld\n", n_gemm, n_attn,
                    over[0], over[1], over[2], any, attn_bad, any_old);
        CHECK(any_old == 0 && attn_bad == 0, "rev 1 refused something or an attention product does not fit");
        if (expect[0] >= 0)
            CHECK(over[0] == expect[0] && over[1] == expect[1] && over[2] == expect[2] && any == expect[3], "expected A %ld B %ld C %ld ANY %ld", expect[0], expect[1], expect[2], expect[3]);
    }
    std::printf("RESULT: %s (fails=%d)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
