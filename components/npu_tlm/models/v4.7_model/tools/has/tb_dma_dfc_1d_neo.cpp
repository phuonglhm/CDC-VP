// tb_dma_dfc_1d_neo.cpp -- unit test of has/dma_dfc_1d.h: cost of the one-dimensional DFC commands with the revision knob (HAS_NEO_REV, passed here as an argument)
// and the counters dram_noncontig / dram_need3d / restarts. Expected values are worked out by hand from the formulas in the header (not by calling the code under test):
//   write-back: tail 7 + per run (ceil(bytes / 16) + 1 + chain 15);  rev 1 replaces chain 15 by tail 7 + restart + first 19 for every run after the first (+11 + restart);
//   copy: first 19 + tail 7 + per command (ceil(bytes / 16) + 1), chain 15 between commands (first command none); rev 1 as above for a run after the first of a piece and side.
//   tools/has/tb_dma_dfc_1d_neo [report]      "report" = HAS_DMA_DFC1D_REPORT=1 (the counters also run with rev 0)
#include <cstdio>
#include <cstdlib>
#include <string>
#include "has/dma_dfc_1d.h"

using namespace has;

namespace
{
    int errors = 0;
    void expect(const char *what, uint64_t got, uint64_t want)
    {
        if (got != want) { std::printf("FAIL %s: got %llu, want %llu\n", what, (unsigned long long)got, (unsigned long long)want); errors++; }
    }

    struct Case
    {
        const char *name;
        bool copy;                  // cost(src, dst) instead of write_cost(src)
        DmaNdDesc src, dst;
        uint64_t cyc0, cyc1;        // rev 0, rev 1 with restart 0
        uint64_t cmds, bytes;
        uint64_t noncontig, need3d, restarts1;
    };

    DmaNdDesc nd(uint32_t base, unsigned n0, unsigned n1, unsigned n2, int64_t s1, int64_t s2)
    {
        DmaNdDesc d;
        d.base = base;
        d.n = {{n0, n1, n2, 1}};
        d.s = {{1, s1, s2, 0}};
        d.elem_bytes = 1;
        return d;
    }
}

int sc_main(int argc, char **argv)
{
    const bool report_arg = argc > 1 && std::string(argv[1]) == "report";
    // the expected values below are worked out with first 19 and tail 7 (the single-clock numbers); the model default is 6 / 3 core cycles, so the test
    // sets 19 / 7 unless the caller set them (chain 15 is the same in both)
    for (const char *k : {"HAS_DMA_DFC1D_FIRST", "HAS_DMA_DFC1D_TAIL"})
        if (!std::getenv(k))
#ifdef _WIN32
            _putenv_s(k, k[14] == 'F' ? "19" : "7");
#else
            setenv(k, k[14] == 'F' ? "19" : "7", 1);
#endif
    if (report_arg)
#ifdef _WIN32
        _putenv_s("HAS_DMA_DFC1D_REPORT", "1");
#else
        setenv("HAS_DMA_DFC1D_REPORT", "1", 1);
#endif
    const bool counted0 = report_arg;   // rev 0 counts only with the report switch

    const Case cases[] = {
        {"T1 contiguous 1024 B", false, nd(0x1000, 1024, 1, 1, 0, 0), {}, 87, 87, 1, 1024, 0, 0, 0},
        // tile 32 x 8 cut from a tensor 128 wide (rows 128 apart): 8 runs of 32 B, even pitch
        {"T2 pitch > row", false, DmaNdDesc::tile_writeback_3d(0, 1, 64, 128, 0, 1, 0, 8, 0, 32), {}, 151, 228, 8, 256, 1, 0, 7},
        // overlapping rows: 8 runs of 32 B, pitch 16 (< row)
        {"T3 pitch < row", false, nd(0, 32, 8, 1, 16, 0), {}, 151, 228, 8, 256, 1, 0, 7},
        // 3 channels of a 32 x 4 tile, planes 8192 apart: 12 runs, the plane step breaks the even pitch
        {"T4 3-D", false, DmaNdDesc::tile_writeback_3d(0, 3, 64, 128, 0, 3, 0, 4, 0, 32), {}, 223, 344, 12, 384, 1, 1, 11},
        // full rows (128 B) of 4 rows merge into one 512 B run; 2 channel planes: 2 runs, pitch 8192
        {"T5 rows merge, 2 planes", false, DmaNdDesc::tile_writeback_3d(0, 2, 64, 128, 0, 2, 0, 4, 0, 128), {}, 103, 114, 2, 1024, 1, 0, 1},
        // slice: 100 runs of 64 B (pitch 256) -> contiguous 6400 B: one piece; read side non-contiguous
        {"T6 slice", true, nd(0, 64, 100, 1, 256, 0), nd(0x100000, 6400, 1, 1, 0, 0), 2427, 3516, 101, 6400, 1, 0, 99},
        // 60000 B contiguous on both sides: three pieces of at most 25600 B
        {"T7 contiguous copy", true, nd(0, 60000, 1, 1, 0, 0), nd(0x100000, 60000, 1, 1, 0, 0), 7607, 7607, 6, 60000, 0, 0, 0},
    };

    for (const Case &c : cases)
        for (unsigned rev = 0; rev <= 1; rev++)
            for (int rs = 0; rs < 2; rs++)
            {
                if (rs && !rev) continue;
                const uint64_t restart = rs ? 5 : 0;
                const DmaDfc1dStats b = dma_dfc1d_stats();
                const uint64_t cyc = c.copy ? DmaDfc1d::cost_rev(c.src, c.dst, rev, restart) : DmaDfc1d::write_cost_rev(c.src, rev, restart);
                const DmaDfc1dStats &a = dma_dfc1d_stats();
                const std::string tag = std::string(c.name) + " rev " + std::to_string(rev) + (rs ? " restart 5" : "");
                const uint64_t n_re = rev ? c.restarts1 : 0;
                expect((tag + " cycles").c_str(), cyc, (rev ? c.cyc1 : c.cyc0) + (rs ? 5 * n_re : 0));
                expect((tag + " ops").c_str(), a.ops - b.ops, 1);
                expect((tag + " commands").c_str(), a.commands - b.commands, c.cmds);
                expect((tag + " bytes").c_str(), a.bytes - b.bytes, c.bytes);
                expect((tag + " cycles stat").c_str(), a.cycles - b.cycles, cyc);
                const bool counted = rev || counted0;
                expect((tag + " dram_noncontig").c_str(), a.dram_noncontig - b.dram_noncontig, counted ? c.noncontig : 0);
                expect((tag + " dram_need3d").c_str(), a.dram_need3d - b.dram_need3d, counted ? c.need3d : 0);
                expect((tag + " restarts").c_str(), a.restarts - b.restarts, n_re);
            }

    // T8: the entry points without a revision argument follow HAS_NEO_REV (not set here: revision 0 = the old formulas)
    if (neo_rev() == 0)
    {
        for (const Case &c : cases)
        {
            const uint64_t cyc = c.copy ? DmaDfc1d::cost(c.src, c.dst) : DmaDfc1d::write_cost(c.src);
            expect((std::string(c.name) + " default entry").c_str(), cyc, c.cyc0);
        }
    }
    else std::printf("T8 skipped: HAS_NEO_REV is set\n");

    std::printf(errors ? "tb_dma_dfc_1d_neo: %d FAIL\n" : "tb_dma_dfc_1d_neo: ALL PASS (%d errors)\n", errors);
    return errors ? 1 : 0;
}
