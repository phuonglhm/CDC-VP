// sauria_run — top-level driver API (pure C, no SystemC, no Python). Ties the
// config encoder (libsauria_cfg) and the DRAM packer (libsauria_mem) together so a
// software framework can prepare a core run entirely in C:
//
//   SauriaRunInputs in = sauria_prepare(target, desc, A, B, Cpre);
//     -> in.controller_args : the AXI-Lite register program (args[0..21]+[22..])
//     -> in.initial_dram    : the DRAM image (activations + weights + preloads)
//     -> in.A_off/B_off/C_off : region byte offsets (also written into args[18..20])
//
// The caller then drives the accelerator: write controller_args, DMA initial_dram,
// assert start, wait done, then read the output C region:
//
//   std::vector<double> out = sauria_read_output(dram_after_run, in.C_off, n, target);
//
// In this virtual platform "driving the accelerator" is the SystemC core (tb); on
// silicon it is the real register/DMA sequence. This header does everything up to
// and after that step, with no Python in the loop.

#ifndef SAURIA_RUN_H
#define SAURIA_RUN_H

#include <cstdint>
#include <vector>
#include "libsauria_cfg.h"
#include "libsauria_mem.h"
#include "sauria_targets.h"

namespace sauria
{
    struct SauriaRunInputs
    {
        std::vector<uint32_t> controller_args; // register program
        std::vector<uint8_t> initial_dram;     // DRAM image to load
        uint32_t A_off, B_off, C_off;          // region byte offsets
    };

    // Prepare a complete core run from raw tensors. A=[C_in,A_h,A_w],
    // B=[C_out,C_in,B_h,B_w], Cpre=[C_out,C_h,C_w] (row-major doubles; ints or
    // half-representable floats depending on the target). The DRAM base addresses
    // programmed into the config (args[18..20]) are the region offsets themselves.
    inline SauriaRunInputs sauria_prepare(
        const SauriaTarget &t, const SauriaLayerDesc &desc,
        const double *A, int A_c, int A_h, int A_w,
        const double *B, int C_out, int C_in, int B_h, int B_w,
        const double *Cpre, int C_c, int C_h, int C_w)
    {
        SauriaRunInputs r;
        SauriaDramLayout L = sauria_assemble_dram(
            A, A_c, A_h, A_w, B, C_out, C_in, B_h, B_w, Cpre, C_c, C_h, C_w,
            desc.c_til, desc.k_til, t);
        r.initial_dram = std::move(L.dram);
        r.A_off = L.A_off; r.B_off = L.B_off; r.C_off = L.C_off;
        uint32_t bases[3] = {L.A_off, L.B_off, L.C_off};
        r.controller_args = sauria_encode_controller_args(desc, t, bases);
        return r;
    }

    // Read the output tensor from the DRAM C region after the core has run.
    inline std::vector<double> sauria_read_output(const std::vector<uint8_t> &dram,
                                                  uint32_t C_off, int n,
                                                  const SauriaTarget &t)
    {
        return sauria_unpack_output(dram, C_off, n, t);
    }

} // namespace sauria

#endif // SAURIA_RUN_H
