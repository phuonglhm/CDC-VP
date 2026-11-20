// tb_vert_scan_bit.cpp -- position of the core's transpose_mode bit per RTL revision (driver/libneo_cfg.h neo::transpose_bit).
// Expected (neo_lite config_regs.sv field order): revision 2 = core word 28 bit 16 (byte 0x810 of the core window),
// revision 0 / 1 = core word 28 bit 25; the set / get helpers touch only that bit. No SystemC.
// Build: g++ -std=c++17 -O1 -I <tree dir> tools/has/tb_vert_scan_bit.cpp -o tb_vert_scan_bit
#include <cstdio>
#include <vector>
#include "driver/libneo_cfg.h"

static int bad = 0;
static void chk(bool ok, const char *what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) bad++; }

int main()
{
    const neo::CoreBit r0 = neo::transpose_bit(0), r1 = neo::transpose_bit(1), r2 = neo::transpose_bit(2), r3 = neo::transpose_bit(7);
    chk(r0.word == 28 && r0.bit == 25, "revision 0: word 28 bit 25");
    chk(r1.word == 28 && r1.bit == 25, "revision 1: word 28 bit 25");
    chk(r2.word == 28 && r2.bit == 16, "revision 2: word 28 bit 16");
    chk(r3.word == r2.word && r3.bit == r2.bit, "a larger revision takes the newest layout");
    chk(neo::addr::core_word_addr(r2.word) == 0x10810u, "revision 2 word 28 = byte 0x810 of the core window (0x10810)");
    std::vector<uint32_t> core(neo::N_CORE, 0u);
    chk(!neo::get_transpose_bit(core, 2), "bit clear in an empty image");
    neo::set_transpose_bit(core, 2, true);
    chk(neo::get_transpose_bit(core, 2) && core[28] == (1u << 16), "set: only bit 16 of word 28");
    chk(!neo::get_transpose_bit(core, 1), "revision 1 position is another bit (25), still clear");
    neo::set_transpose_bit(core, 2, false);
    chk(!neo::get_transpose_bit(core, 2) && core[28] == 0u, "clear");
    neo::set_transpose_bit(core, 1, true);
    chk(neo::get_transpose_bit(core, 1) && core[28] == (1u << 25), "revision 1: only bit 25 of word 28");
    std::printf("transpose bit: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
