/*
 * Host tool of fx1_fw_npu_oracle_xcheck: writes the firmware NPU test cases
 * (tests/npu_common/npu_cases.c) as a program directory of the drop's numpy
 * reference (tools/has/ref_program.py):
 *   <dir>/mmio.txt       the register writes of the firmware's builders, window base 0
 *   <dir>/dram_init.bin  the initial window image (npu_cases_fill)
 *   <dir>/c_oracle.bin   the image after the firmware's C oracle (npu_oracle.c)
 * The DFC cases D1 / D2 are written as the lane-A instructions they decode to
 * (MAX_POOL, ADD), so the oracle of the DFC test is cross-checked as well.
 *   npu_case_gen <dir>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npu_cases.h"

static uint8_t img[NPU_CASE_WINDOW];

static void write_instr(FILE* f, unsigned idx, const char* label, const fx1_npu_instr_t* in)
{
    fprintf(f, "# [%u] %s\n", idx, label);
    for (uint32_t k = 0; k < in->n; ++k) fprintf(f, "W %08x %x\n", in->addr[k], in->val[k]);
    fprintf(f, "W %08x %x\n", FX1_NPU_M_PUSH_A, in->push);
}

static int write_file(const char* dir, const char* name, const void* data, size_t n)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE* f = fopen(path, "wb");
    if (!f || fwrite(data, 1, n, f) != n) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fclose(f);
    return 1;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: npu_case_gen <dir>\n");
        return 2;
    }
    npu_cases_t t;
    if (!npu_cases_build(&t, 0)) {
        fprintf(stderr, "case build failed\n");
        return 1;
    }
    npu_cases_fill(img);
    if (!write_file(argv[1], "dram_init.bin", img, sizeof img)) return 1;
    char path[1024];
    snprintf(path, sizeof path, "%s/mmio.txt", argv[1]);
    FILE* f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "# npu_case_gen: firmware NPU test cases (fw/fx1_soc/tests/npu_common), window base 0\n");
    unsigned idx = 0;
    for (unsigned r = 0; r < NPU_RICH_COUNT; ++r) write_instr(f, idx++, t.label[r], &t.instr[r]);
    fx1_npu_instr_t d1, d2;
    if (!fx1_npu_build_pool(&d1, &t.d1) || !fx1_npu_build_add(&d2, &t.d2)) return 1;
    write_instr(f, idx++, "D1 as lane-A MAX_POOL", &d1);
    write_instr(f, idx++, "D2 as lane-A ADD", &d2);
    fclose(f);
    npu_cases_apply_rich(&t, img, 0);
    npu_cases_apply_dfc(&t, img, 0);
    if (!write_file(argv[1], "c_oracle.bin", img, sizeof img)) return 1;
    printf("npu_case_gen: %u instructions, image %u bytes, in %s\n", idx, (unsigned)sizeof img, argv[1]);
    return 0;
}
