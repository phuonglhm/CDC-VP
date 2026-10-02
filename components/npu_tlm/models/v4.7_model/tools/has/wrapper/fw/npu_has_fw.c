/*
 * npu_has_fw.c -- replay engine for HasNpuTop programs through a system TLM wrapper (see npu_has_fw.h).
 * The sequence is the one of tools/has/wrapper/test_npu_tlm_core_rtl.cpp, written as firmware would run it.
 */
#include "npu_has_fw.h"

int npu_has_is_address_register(uint32_t a)
{
    switch (a) {
    case NPU_HAS_IN_ADDR: case NPU_HAS_W_ADDR: case NPU_HAS_OUT_ADDR: case NPU_HAS_BIAS_ADDR: case NPU_HAS_SKIP_ADDR:
    case NPU_HAS_A_ADDR: case NPU_HAS_B_ADDR: case NPU_HAS_V_ADDR:
    case NPU_HAS_SCALE_ADDR: case NPU_HAS_SHIFT_ADDR: case NPU_HAS_LUT_ADDR: case NPU_HAS_PARAM_ADDR: case NPU_HAS_MASK_ADDR:
        return 1;
    default:
        return 0;
    }
}

uint32_t npu_has_crc32(uint32_t crc, const uint8_t *data, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    crc = ~crc;
    while (n--) {
        crc ^= *data++;
        crc = t[crc & 0xFu] ^ (crc >> 4);
        crc = t[crc & 0xFu] ^ (crc >> 4);
    }
    return ~crc;
}

#define CHUNK 256u

static uint32_t ram_crc(uint32_t phys, uint32_t n)
{
    uint8_t buf[CHUNK];
    uint32_t crc = 0;
    while (n) {
        const uint32_t k = n < CHUNK ? n : CHUNK;
        npu_hal_ram_read(phys, buf, k);
        crc = npu_has_crc32(crc, buf, k);
        phys += k;
        n -= k;
    }
    return crc;
}

/* Nearest-neighbour upsample x2 of an int8 tensor [c][h][w] on RAM, one input row at a time. */
static int upsample2x(uint32_t in, uint32_t out, uint32_t c, uint32_t h, uint32_t w)
{
    uint8_t src[CHUNK / 2], dst[CHUNK];
    uint32_t ci, y, x;
    if (w == 0 || w > CHUNK / 2)
        return -1;
    for (ci = 0; ci < c; ci++) {
        for (y = 0; y < h; y++) {
            npu_hal_ram_read(in + (ci * h + y) * w, src, w);
            for (x = 0; x < w; x++)
                dst[2 * x] = dst[2 * x + 1] = src[x];
            npu_hal_ram_write(out + (ci * 2 * h + 2 * y) * 2 * w, dst, 2 * w);
            npu_hal_ram_write(out + (ci * 2 * h + 2 * y + 1) * 2 * w, dst, 2 * w);
        }
    }
    return 0;
}

int npu_has_replay(const uint32_t *s, uint32_t ram_base, uint32_t flags, npu_has_result *res)
{
    npu_has_result r = {0, 0, 0, 0, 0, 0, 0};
    const uint32_t *end;
    uint32_t step = 0;      /* instructions and host steps counted together */
    int step_ok = 1;        /* state of the step whose check record is still to come */
    char step_kind = 0;
    uint64_t last_cycles = 0;   /* PERF_EXEC_CYCLES of the instruction just completed */
    const int host_only = (flags & NPU_HAS_REPLAY_HOST_ONLY) != 0;

    if (s[0] != NPU_HAS_STREAM_MAGIC || s[1] != NPU_HAS_STREAM_VERSION || s[2] < NPU_HAS_STREAM_HDR_WORDS)
        return -1;
    end = s + s[2];
    if (!host_only) {
        /* The wrapper stages this window into the model before every extended instruction and copies it back after. */
        npu_hal_write32(NPU_HAS_WR_WINDOW_BASE, ram_base);
        npu_hal_write32(NPU_HAS_WR_WINDOW_SIZE, (s[3] + 4095u) & ~4095u);
    }
    s += NPU_HAS_STREAM_HDR_WORDS;

    while (s < end) {
        const uint32_t rec = *s++;
        if (rec == NPU_HAS_REC_END)
            break;
        if (rec == NPU_HAS_REC_WRITE) {
            const uint32_t addr = s[0];
            const uint32_t val = npu_has_is_address_register(addr) ? ram_base + s[1] : s[1];
            s += 2;
            if (host_only) {
                if (addr == NPU_HAS_PUSH_A) { step_kind = 'I'; step_ok = 1; }
                continue;
            }
            npu_hal_write32(NPU_HAS_VP_OFF(addr), val);
            if (addr == NPU_HAS_PUSH_A) {
                uint32_t st;
                for (;;) {
                    npu_hal_idle();
                    st = npu_hal_read32(NPU_HAS_WR_STATUS);
                    if (st & (NPU_HAS_WR_STATUS_DONE | NPU_HAS_WR_STATUS_ERROR))
                        break;
                }
                last_cycles = (uint64_t)npu_hal_read32(NPU_HAS_PERF_EXEC_LO) |
                              ((uint64_t)npu_hal_read32(NPU_HAS_PERF_EXEC_HI) << 32);
                r.exec_cycles += last_cycles;
                r.last_status = st;
                step_kind = 'I';
                step_ok = (st & NPU_HAS_WR_STATUS_ERROR) == 0;
                if (!step_ok)
                    r.last_error = npu_hal_read32(NPU_HAS_WR_LAST_ERROR);
                npu_hal_write32(NPU_HAS_WR_STATUS, NPU_HAS_WR_STATUS_DONE | NPU_HAS_WR_STATUS_ERROR);
            }
        } else if (rec == NPU_HAS_REC_HOST) {
            step_kind = 'H';
            step_ok = upsample2x(ram_base + s[0], ram_base + s[1], s[2], s[3], s[4]) == 0;
            s += 5;
        } else if (rec == NPU_HAS_REC_CHECK) {
            if (step_ok && !(flags & NPU_HAS_REPLAY_NO_CHECK))
                step_ok = ram_crc(ram_base + s[0], s[1]) == s[2];
            s += 3;
            if (step_kind == 'I') r.instructions++; else r.host_steps++;
            if (!step_ok) {
                if (r.failed == 0)
                    r.first_failed = step;
                r.failed++;
            }
            npu_hal_report(step_kind, step, step_ok, r.last_status, step_kind == 'I' ? last_cycles : 0);
            step++;
        } else {
            return -2;
        }
    }
    if (res)
        *res = r;
    return (int)r.failed;
}
