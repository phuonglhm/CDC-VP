/*
 * Host test (plan N4): every DDR region the NPU uses (job image, result
 * block, ownership lock, the small arena with the selftest range, the large
 * arena) is disjoint from every other region a firmware or host user owns:
 * firmware image and stacks, the ISP demo job (incl. the full-size
 * 2688 x 1520 sample of the launcher), the ISP stream areas, slots and pool,
 * the camera paths (camera -> ISP, the 4K TPG record and slots) and the
 * atomic tests' area. Prints the map as evidence.
 *
 * Sizes come from the headers that define them. Two firmware-only headers
 * cannot be included on the host; their constants are restated here with
 * their source: tests/camera_isp/cam_isp.h (CAM_ISP_SIDE / STAGING / Y,
 * 0x10000 apart, 4 each) and tests/camera_isp/tpg4k.c (TPG_SIDE 16 MiB apart,
 * TPG_Y 20 MiB apart, 4 frames at most).
 */
#include <stdio.h>

#include "fx1/fx1_isp_stream.h"
#include "fx1/fx1_memory_map.h"
#include "isp_demo_job.h"

typedef struct {
    const char* name;
    unsigned long long base, size;
    int npu; /* 1: an NPU region */
} region;

#define MiB 0x100000ull
#define CAM_ISP_SIDE(i) (FX1_FW_RAW_BUF_BASE + (i) * 0x10000u) /* cam_isp.h */
#define CAM_ISP_Y(i) (FX1_FW_YUV_BUF_BASE + (i) * 0x10000u)    /* cam_isp.h */
#define ISP_FULL_RAW (2688ull * 1520ull * 2ull)  /* the launcher's d65 sample, 16-bit container */
#define ISP_FULL_Y (2688ull * 1520ull)
#define ISP_FULL_UV (2688ull * 760ull)

static const region regions[] = {
    {"firmware image", FX1_FW_IMAGE_BASE, FX1_FW_IMAGE_SIZE, 0},
    {"stacks (2 harts)", FX1_FW_STACK_BASE, (unsigned long long)FX1_FW_STACK_SIZE * FX1_NUM_HARTS, 0},
    {"ISP demo job + result", FX1_FW_SHARED_BASE + ISP_DEMO_JOB_OFFSET, ISP_DEMO_PROFILE_OFFSET, 0},
    {"ISP demo profile (32768 writes)", FX1_FW_SHARED_BASE + ISP_DEMO_PROFILE_OFFSET, 8ull * 32768ull, 0},
    {"ISP stream desc..records", FX1_ISP_STREAM_DESC_ADDR, FX1_ISP_STREAM_PROFILE_ADDR - FX1_ISP_STREAM_DESC_ADDR, 0},
    {"ISP stream profile", FX1_ISP_STREAM_PROFILE_ADDR, 8ull * FX1_ISP_STREAM_PROFILE_MAX, 0},
    {"ISP stream chains", FX1_ISP_STREAM_CHAIN_ADDR, (unsigned long long)FX1_ISP_STREAM_CHAIN_PITCH * FX1_ISP_STREAM_SLOTS, 0},
    {"camera 4K TPG record", FX1_FW_SHARED_BASE + 0x800000ull, 0x4000ull, 0},
    {"ISP demo RAW (2688x1520)", FX1_FW_RAW_BUF_BASE, ISP_FULL_RAW, 0},
    {"ISP stream input slots", FX1_ISP_STREAM_IN_ADDR(0), (unsigned long long)FX1_ISP_STREAM_SLOTS * FX1_ISP_STREAM_SLOT_BYTES, 0},
    {"camera->ISP side / staging", CAM_ISP_SIDE(0), 0x80000ull, 0},
    {"camera 4K TPG sides", FX1_FW_RAW_BUF_BASE, 4ull * 0x1000000ull, 0},
    {"ISP demo Y + UV (2688x1520)", FX1_FW_YUV_BUF_BASE, 0x400000ull + ISP_FULL_UV, 0},
    {"ISP stream output slots", FX1_ISP_STREAM_OUT_ADDR(0), (unsigned long long)FX1_ISP_STREAM_SLOTS * FX1_ISP_STREAM_SLOT_BYTES, 0},
    {"camera->ISP Y / UV", CAM_ISP_Y(0), 0x80000ull, 0},
    {"camera 4K TPG Y / UV slots", FX1_FW_YUV_BUF_BASE, 4ull * 0x1400000ull, 0},
    {"atomic tests' words", FX1_FW_TEST_AREA_BASE, 0x10000ull, 0},
    {"ISP stream source pool", FX1_ISP_STREAM_POOL_BASE, FX1_ISP_STREAM_POOL_END - FX1_ISP_STREAM_POOL_BASE, 0},
    {"NPU job image", FX1_NPU_JOB_BASE, FX1_NPU_JOB_SIZE, 1},
    {"NPU result block", FX1_NPU_RESULT_BASE, FX1_NPU_RESULT_SIZE, 1},
    {"NPU ownership lock", FX1_NPU_LOCK_BASE, FX1_NPU_LOCK_SIZE, 1},
    {"NPU small arena", FX1_NPU_ARENA_SMALL_BASE, FX1_NPU_ARENA_SMALL_SIZE, 1},
    {"NPU large arena", FX1_NPU_ARENA_LARGE_BASE, FX1_NPU_ARENA_LARGE_SIZE, 1},
};
#define N (sizeof regions / sizeof regions[0])

static int overlap(const region* a, const region* b)
{
    return a->base < b->base + b->size && b->base < a->base + a->size;
}

int main(void)
{
    int failures = 0;
    printf("[npu-map] DDR %#llx..%#llx\n", (unsigned long long)FX1_DDR_BASE,
           (unsigned long long)FX1_DDR_BASE + FX1_DDR_SIZE);
    for (unsigned i = 0; i < N; ++i)
        printf("  %-34s %#010llx .. %#010llx  %8.2f MiB%s\n", regions[i].name, regions[i].base,
               regions[i].base + regions[i].size, (double)regions[i].size / MiB, regions[i].npu ? "  NPU" : "");
    for (unsigned i = 0; i < N; ++i) {
        const region* a = &regions[i];
        if (a->base < FX1_DDR_BASE || a->base + a->size > (unsigned long long)FX1_DDR_BASE + FX1_DDR_SIZE) {
            printf("  [FAIL] %s outside DDR\n", a->name);
            ++failures;
        }
        for (unsigned j = 0; j < N; ++j) {
            const region* b = &regions[j];
            /* every NPU region against every other region (NPU or not) */
            if (j <= i || !(a->npu || b->npu)) continue;
            if (overlap(a, b)) {
                printf("  [FAIL] %s overlaps %s\n", a->name, b->name);
                ++failures;
            }
        }
    }
    /* The selftest range (small arena +3 MiB..+4 MiB) lies in the small arena. */
    const unsigned long long st = FX1_NPU_ARENA_SMALL_BASE + 0x300000ull;
    if (st + 0x100000ull > (unsigned long long)FX1_NPU_ARENA_SMALL_BASE + FX1_NPU_ARENA_SMALL_SIZE) {
        printf("  [FAIL] the selftest range leaves the small arena\n");
        ++failures;
    }
    printf("[npu-map] %u regions, every NPU region apart from every other: %s\n", (unsigned)N, failures ? "NO" : "yes");
    printf("RESULT %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
