/*
 * ISP RAW-frame demo: the job's ISP bring-up, shared by the bare-metal demo
 * (main.c) and the FreeRTOS CLI (`isp run`). See isp_demo_job.h.
 *
 * Profile mode (the default, CSR v1.07): the job's profile is applied in
 * legacy mode, then fx1_isp_profile_from_legacy builds it into bank 0
 * (INIT, payloads and the profile's TUNE extension scalars written into the
 * bank -- ISP-UPD-009 --, SEAL, APPLY). A failure is reported (status 2,
 * the TUNE_ERROR bits), never replaced by a legacy run.
 */
#ifndef ISP_DEMO_SETUP_H
#define ISP_DEMO_SETUP_H

#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "isp_demo_job.h"

/* 0 if the descriptor is not a job this firmware runs. */
static inline int isp_demo_job_ok(const volatile isp_demo_job* job)
{
    if (job->magic != ISP_DEMO_JOB_MAGIC || job->frames < 1u) return 0;
    if (job->version == ISP_DEMO_VERSION_V1) return 1;
    if (job->version != ISP_DEMO_VERSION || job->mode > ISP_DEMO_MODE_LEGACY) return 0;
    return job->outputs == 0u || ((job->outputs & 1u) && job->outputs <= 7u);
}

static inline uint32_t isp_demo_mode(const volatile isp_demo_job* job)
{
    return job->version == ISP_DEMO_VERSION_V1 ? ISP_DEMO_MODE_LEGACY : job->mode;
}

static inline uint32_t isp_demo_outputs(const volatile isp_demo_job* job)
{
    return job->version == ISP_DEMO_VERSION_V1 || job->outputs == 0u ? 1u : job->outputs;
}

static inline uint32_t isp_demo_profile_id(const volatile isp_demo_job* job)
{
    return job->version == ISP_DEMO_VERSION_V1 || job->profile_id == 0u ? 1u : job->profile_id;
}

/* Geometry, profile, outputs and (profile mode) the bank; before
 * fx1_isp_start. Returns FX1_ISP_OK, or the failing driver result with the
 * TUNE_ERROR bits in *tune_error. `mode`: the mode to run (the job's, or
 * legacy where the caller needs it). */
static inline int isp_demo_configure(fx1_isp_dev* dev, const volatile isp_demo_job* job,
                                     const fx1_isp_reg_write* profile, uint32_t mode, uint32_t* tune_error)
{
    *tune_error = 0;
    int rc = fx1_isp_profile_mode(dev, 0); /* a previous profile-mode run left it on */
    if (rc == FX1_ISP_OK) rc = fx1_isp_set_geometry(dev, job->width, job->height, job->bayer);
    if (rc == FX1_ISP_OK) rc = fx1_isp_apply(dev, profile, job->profile_count); /* EIO: a table not confirmed */
    if (rc != FX1_ISP_OK) return rc;
    const uint32_t outputs = isp_demo_outputs(job);
    const fx1_isp_reg_write enable = {FX1_ISP_OUTPUT_ENABLE_OFFSET, outputs};
    fx1_isp_apply(dev, &enable, 1);
    for (unsigned ch = 1; ch < 3u; ++ch) {
        if (!(outputs & (1u << ch))) continue;
        fx1_isp_channel_buffers cb;
        for (unsigned k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) { /* one frame at a time: one slot is enough */
            cb.y[k] = ch == 1u ? job->ch1_y_addr : job->ch2_addr;
            cb.uv[k] = job->ch1_uv_addr;
        }
        cb.y_stride = ch == 1u ? job->ch1_stride : job->ch2_stride;
        cb.uv_stride = ch == 1u ? job->ch1_stride : 0u;
        rc = fx1_isp_set_channel_buffers(dev, ch, &cb);
        if (rc != FX1_ISP_OK) return rc;
    }
    if (mode == ISP_DEMO_MODE_PROFILE)
        rc = fx1_isp_profile_from_legacy(dev, 0, profile, job->profile_count, isp_demo_profile_id(job), tune_error);
    return rc;
}

/* ISP-UPD-001: one LSC mesh in the three residents. The job profile loads
 * the mesh into residents 1 and 2 and selects one of them: resident 0 is
 * active at reset and is never a load destination (MAS Table 31, owner Q5).
 * Once the first SOF has made the selected resident active, the firmware
 * loads the same validated coefficients -- the profile's own LSC_COEF_DATA
 * writes -- into every resident that is still invalid and not active
 * (fx1_isp_lsc_load_writes: the mesh size in place is kept). */
typedef struct {
    uint32_t first, count; /* the selected resident's coefficient writes in the profile */
    unsigned selected;
} isp_demo_lsc_mesh;

/* 1 if the profile validates a mesh for the resident it selects. The scan
 * follows the loader (control_unit::lsc_load_ctrl, MAS Table 31): the
 * destination is the one latched by LOAD_BEGIN (a VALIDATE or ABORT needs no
 * profile bits, review Stop B R2); abort > validate > begin within a write; a
 * BEGIN while a load is open, or of profile 3, is refused and the open load
 * goes on; BEGIN invalidates its destination; VALIDATE commits only an exact
 * 4 * nx * ny coefficient count, otherwise the destination stays invalid; an
 * LSC_MESH_NODES write invalidates every resident. The coefficient writes of
 * the selected resident must also form one contiguous block (other writes
 * around them do not matter) so that the firmware can replay them. */
static inline int isp_demo_lsc_mesh_of(const fx1_isp_reg_write* p, uint32_t n, isp_demo_lsc_mesh* m)
{
    uint32_t first[3] = {0}, count[3] = {0}, start = 0, got = 0, valid = 0, nodes = 0;
    int dest = -1, plain = 0, sel = -1;
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t off = p[i].offset, v = p[i].value;
        if (off == FX1_ISP_LSC_MESH_NODES_OFFSET) {
            nodes = v;
            valid = 0;
        } else if (off == FX1_ISP_LSC_LOAD_CTRL_OFFSET) {
            if (v & FX1_ISP_LSC_LOAD_CTRL_LOAD_ABORT_MASK) {
                dest = -1; /* the destination stays invalid */
            } else if (v & FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK) {
                if (dest >= 0) {
                    const uint32_t nx = nodes & FX1_ISP_LSC_MESH_NODES_MESH_NX_MASK;
                    const uint32_t ny = (nodes & FX1_ISP_LSC_MESH_NODES_MESH_NY_MASK) >> FX1_ISP_LSC_MESH_NODES_MESH_NY_SHIFT;
                    if (got && got == 4u * nx * ny) {
                        valid |= 1u << dest;
                        first[dest] = start;
                        count[dest] = plain ? got : 0u; /* 0: valid, but not replayable as one block */
                    }
                }
                dest = -1;
            } else if ((v & FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK) && dest < 0) {
                const uint32_t d = v & FX1_ISP_LSC_LOAD_CTRL_LOAD_PROFILE_MASK;
                if (d < 3u) {
                    dest = (int)d;
                    got = 0;
                    plain = 1;
                    valid &= ~(1u << d); /* begin invalidates its destination */
                }
            }
        } else if (off == FX1_ISP_LSC_COEF_DATA_OFFSET && dest >= 0) {
            if (!got) start = i;
            if (i == start + got) ++got; else { ++got; plain = 0; } /* counted, but a split block */
        }
        if (off == FX1_ISP_LSC_PROFILE_SEL_OFFSET) sel = (int)(v & FX1_ISP_LSC_PROFILE_SEL_PROFILE_SEL_MASK);
    }
    if (sel < 0 || sel > 2 || !(valid & (1u << sel)) || !count[sel]) return 0;
    m->first = first[sel];
    m->count = count[sel];
    m->selected = (unsigned)sel;
    return 1;
}

/* After the first SOF: fills the invalid, non-active residents with the
 * selected mesh. Returns the LSC_PROFILE_STATUS valid mask (bits 0..2) after
 * the fill, *filled the residents loaded; *rc the first driver error. A
 * profile without a selected mesh is left alone. */
static inline uint32_t isp_demo_lsc_fill(fx1_isp_dev* dev, const fx1_isp_reg_write* p, uint32_t n,
                                         uint32_t* filled, int* rc)
{
    isp_demo_lsc_mesh m;
    *filled = 0;
    *rc = FX1_ISP_OK;
    uint32_t st = dev->hal.read32(dev->hal.ctx, FX1_ISP_LSC_PROFILE_STATUS_OFFSET);
    if (!isp_demo_lsc_mesh_of(p, n, &m)) return st & 7u;
    const unsigned active = (st & FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_MASK) >>
                            FX1_ISP_LSC_PROFILE_STATUS_ACTIVE_PROFILE_SHIFT;
    for (unsigned d = 0; d < 3u; ++d) {
        if ((st & (1u << d)) || d == active) continue;
        uint32_t err = 0;
        const int r = fx1_isp_lsc_load_writes(dev, d, &p[m.first], m.count, &err);
        if (r == FX1_ISP_OK) *filled |= 1u << d;
        else if (*rc == FX1_ISP_OK) *rc = r;
    }
    st = dev->hal.read32(dev->hal.ctx, FX1_ISP_LSC_PROFILE_STATUS_OFFSET);
    return st & 7u;
}

/* ISP-UPD-004: the result word of the AEC global UE/OE percentages of the
 * newest AEC result fx1_isp_read_aec_global returned (0 if none). */
static inline uint32_t isp_demo_aec_word(fx1_isp_dev* dev)
{
    fx1_isp_aec_percent pc;
    if (fx1_isp_read_aec_percent(dev, &pc) != FX1_ISP_OK) return 0;
    return ISP_DEMO_AEC_PERCENT_VALID | (pc.oe_bp << 16) | pc.ue_bp;
}

/* ISP-UPD-005: the result word of the outputs' colorimetry for the job's
 * mode (bank 0 in profile mode, as isp_demo_configure builds it); 0 if the
 * driver cannot tell. Call after isp_demo_configure, before fx1_isp_start. */
static inline uint32_t isp_demo_colorimetry_word(fx1_isp_dev* dev)
{
    unsigned std = 0;
    int full = 0;
    if (fx1_isp_output_colorimetry(dev, 0, &std, &full) != FX1_ISP_OK) return 0;
    return ISP_DEMO_COLORIMETRY_VALID | ((uint32_t)(std & 3u) << 1) | (full ? 1u : 0u);
}

/* Output geometry of channel ch (after start). */
static inline void isp_demo_channel_size(fx1_isp_dev* dev, unsigned ch, uint32_t* w, uint32_t* h)
{
    *w = dev->hal.read32(dev->hal.ctx, ch == 1u ? FX1_ISP_CH1_RESIZER_OUT_W_OFFSET : FX1_ISP_CH2_RESIZER_OUT_W_OFFSET);
    *h = dev->hal.read32(dev->hal.ctx, ch == 1u ? FX1_ISP_CH1_RESIZER_OUT_H_OFFSET : FX1_ISP_CH2_RESIZER_OUT_H_OFFSET);
}

#endif /* ISP_DEMO_SETUP_H */
