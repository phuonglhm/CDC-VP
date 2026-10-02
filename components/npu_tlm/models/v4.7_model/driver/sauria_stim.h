// sauria_stim — emit the GoldenStimuli register-command stream in pure C (port of
// config_helper.generate_controller_cmds + the file layout in file_helper). This is
// the last piece needed to write a brand-new, self-generated test case that tb_demo
// can replay — no Python.
//
// GoldenStimuli.txt row = 7 uppercase-hex columns (space-delimited, no leading zeros):
//   data_in  address  wren  rden  waitflag  data_out  checkflag
//
// Command sequence (SAURIA controller @ CTRL=0x4000_0000, core @ CORE=0x5000_0000):
//   wr(CTRL+0x8,3)                        enable interrupts
//   wr(CTRL+0x10 + i*4, args[i]) for all  write controller regs (args[0..21]+[22..])
//   wr(CTRL+0x0,3)                        start
//   {waitflag=1}                          wait for done
//   wr(CTRL+0xC,3)                        lower interrupts
//   rd(CORE+0x14) rd(CORE+0x18)           read cycle / stall counters (no golden)
//   {checkflag=1}  (+1 blank)             finish
//   ... zero-pad to N_VECTORS = N_REGS + 100 (N_REGS = args.size() - 22)

#ifndef SAURIA_STIM_H
#define SAURIA_STIM_H

#include <cstdint>
#include <vector>
#include <string>
#include <cstdio>

namespace sauria
{
    struct StimRow { uint32_t data_in, address, wren, rden, waitflag, data_out, checkflag; };

    inline std::vector<StimRow> sauria_emit_stim(const std::vector<uint32_t> &args)
    {
        const uint32_t CTRL = 0x40000000u, CORE = 0x50000000u;
        int N_REGS = (int)args.size() - 22;      // packed core-config register count
        int N_VECTORS = N_REGS + 100;            // matches file_helper
        std::vector<StimRow> r;
        auto wr = [&](uint32_t a, uint32_t d) { r.push_back({d, a, 1, 0, 0, 0, 0}); };

        wr(CTRL + 0x8, 3);
        for (size_t i = 0; i < args.size(); i++)
            wr(CTRL + 0x10 + (uint32_t)(i << 2), args[i]);
        wr(CTRL + 0x0, 3);
        r.push_back({0, 0, 0, 0, 1, 0, 0});      // waitflag
        wr(CTRL + 0xC, 3);
        r.push_back({0, CORE + 0x14, 0, 1, 0, 0, 0}); // rd cycle counter
        r.push_back({0, CORE + 0x18, 0, 1, 0, 0, 0}); // rd stall counter
        r.push_back({0, 0, 0, 0, 0, 0, 1});      // checkflag -> finish
        r.push_back({0, 0, 0, 0, 0, 0, 0});      // idx += 2 (one blank after checkflag)
        while ((int)r.size() < N_VECTORS)
            r.push_back({0, 0, 0, 0, 0, 0, 0});
        return r;
    }

    // Serialize to the GoldenStimuli.txt text (fmt '%01X', space-delimited).
    inline std::string sauria_stim_to_text(const std::vector<StimRow> &rows)
    {
        std::string out;
        char buf[96];
        for (const auto &r : rows)
        {
            std::snprintf(buf, sizeof(buf), "%X %X %X %X %X %X %X\n",
                          r.data_in, r.address, r.wren, r.rden, r.waitflag, r.data_out, r.checkflag);
            out += buf;
        }
        return out;
    }

} // namespace sauria

#endif // SAURIA_STIM_H
