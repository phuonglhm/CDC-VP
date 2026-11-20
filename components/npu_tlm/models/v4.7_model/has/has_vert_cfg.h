// has_vert_cfg.h -- configuration of the PSM index counters for the vertical result scan (core transpose_mode).
//
// The write counters of the PSM (psm_idxcnt: CXLIM, CXSTEP, CKLIM, CKSTEP, TIL_CYLIM, TIL_CYSTEP, TIL_CKLIM, TIL_CKSTEP) do not know the
// transpose mode: a packet is one "x" run of elements, the k counter steps over packets, the tiling counters over contexts. The horizontal
// scan hands out one packet per CHANNEL (lanes = positions); the vertical scan one packet per POSITION (lanes = channels). With the roles of
// the counters exchanged the core writes the compact token-major image of the tile, element e = p * K + c (position p, channel c; K = channels
// of the tile = TILE_COUT), also for tiles with X_used or Y_used < 32 and for tiles of several channel tiles (k_til > X_used).
// Measured on the RTL as delivered (neo_lite revision 2, Verilator) and on the model: the vertical-scan study of the model;
// the rule below is the one of the compiler: five fields change (CXLIM, CKSTEP, CKLIM, TIL_CYLIM, TIL_CYSTEP), two more for several channel
// tiles (TIL_CKLIM, TIL_CKSTEP); CXSTEP is unchanged.
#ifndef HAS_VERT_CFG_H
#define HAS_VERT_CFG_H

#include <cstdint>
#include "driver/libsauria_cfg.h"

namespace has
{
    // f = the fields of sauria_compute_core_fields for the horizontal scan; on return the index counters follow the vertical scan.
    inline void vertical_psm_fields(uint64_t *f, const sauria::SauriaLayerDesc &d, unsigned sramc_n = 32)
    {
        using namespace sauria;
        const uint64_t X = uint64_t(d.X_used), Y = uint64_t(d.Y_used), K = uint64_t(d.k_til), NP = uint64_t(d.w_til) * uint64_t(d.h_til);
        f[F_CFG_CXLIM] = X + sramc_n;     // a packet = X_used consecutive elements (the channels of one position)
        f[F_CFG_CKSTEP] = K;              // step between positions
        f[F_CFG_CKLIM] = Y * K;           // Y_used positions per context
        f[F_CFG_TIL_CYLIM] = NP * K;      // contexts over the positions of the tile ...
        f[F_CFG_TIL_CYSTEP] = Y * K;      // ... advancing Y_used positions
        f[F_CFG_TIL_CKLIM] = K;           // channel tiles ...
        f[F_CFG_TIL_CKSTEP] = X;          // ... advancing X_used channels
    }
}

#endif // HAS_VERT_CFG_H
