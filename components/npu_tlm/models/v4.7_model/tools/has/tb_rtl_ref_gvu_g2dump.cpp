// tb_rtl_ref_gvu_g2dump.cpp -- writes cycle-by-cycle stimulus and the outputs of the ported register file
// (has/rtl_ref_gvu_vrf.h, Vrf) as text, for replay on the RTL by tools/has/gvu_oracle/tb_oracle_g2.sv.
// One line per clock cycle, hex fields; wide values are printed most significant digit first.
//   vrf.txt         9 write ports (wen waddr wdata), 9 read addresses | 9 read data after the edge
// The LUT unit, the comparator, the stream registers, the Scratchpad and the load / store unit are dumped by
// tools/has/tb_rtl_ref_gvu_bdump.cpp.
// build: g++ -std=c++17 -O2 -Wall -I. tools/has/tb_rtl_ref_gvu_g2dump.cpp -o tb_rtl_ref_gvu_g2dump ; run: <binary> <out dir>
#include <cstdio>
#include <random>
#include <string>
#include "has/rtl_ref_gvu_vrf.h"

using namespace rtl_ref_gvu;
static std::mt19937_64 g_rng(0x47565534u);

static void hex_bytes(FILE *f, const uint8_t *d, size_t n) { for (size_t i = n; i-- > 0;) fprintf(f, "%02x", d[i]); }

static void dump_vrf(const std::string &dir)
{
    Vrf dut;
    FILE *f = fopen((dir + "/vrf.txt").c_str(), "w");
    const int rows[4] = {1, 2, 4, 8}, ports[4] = {2, 2, 3, 2};
    for (int cyc = 0; cyc < 20000; cyc++)
    {
        Vrf::Writes w;
        std::vector<Vrf::Write> *dst[4] = {&w.int8, &w.int16, &w.int32, &w.int64};
        for (int k = 0; k < 4; k++)
            for (int j = 0; j < ports[k]; j++)
            {
                Vrf::Write q; q.wen = g_rng() % 6 == 0; q.waddr = static_cast<unsigned>(g_rng() % (32 / rows[k]));
                q.wdata.resize(static_cast<size_t>(rows[k] * 32));
                for (auto &b : q.wdata) b = static_cast<uint8_t>(g_rng());
                dst[k]->push_back(q);
                fprintf(f, "%x %02x ", q.wen ? 1 : 0, q.waddr); hex_bytes(f, q.wdata.data(), q.wdata.size()); fprintf(f, " ");
            }
        dut.tick(w);
        unsigned ra[9]; int n = 0;
        for (int k = 0; k < 4; k++) for (int j = 0; j < ports[k]; j++) { ra[n] = static_cast<unsigned>(g_rng() % (32 / rows[k])); fprintf(f, "%02x ", ra[n]); n++; }
        n = 0;
        for (int k = 0; k < 4; k++) for (int j = 0; j < ports[k]; j++) { const Bytes d = dut.read(rows[k], ra[n++]); hex_bytes(f, d.data(), d.size()); fprintf(f, n == 9 ? "\n" : " "); }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: %s <out dir>\n", argv[0]); return 2; }
    dump_vrf(argv[1]);
    printf("RESULT: DUMP DONE\n");
    return 0;
}
