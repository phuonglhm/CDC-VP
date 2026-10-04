/*
 * npu_has_fw.h -- firmware-side definitions for running HasNpuTop programs through a system TLM wrapper with the
 * model root core_rtl/ (docs/SW_INTEGRATION_GUIDE.md, section 10): register addresses, opcodes, and a small replay
 * engine that executes a program prepared by make_fw_program.py.
 *
 * Plain C99, no library dependency. The integrator supplies the five npu_hal_* functions at the end of this file.
 *
 * Addresses. The instruction registers are given as MODEL addresses (0x400003xx / 0x400004xx), as in the documents
 * and in mmio.txt. In the wrapper they are reached through its compact alias: NPU base + NPU_HAS_VP_OFF(model address).
 * The wrapper's own registers (status, DRAM window, performance counters) are offsets from the NPU base.
 */
#ifndef NPU_HAS_FW_H
#define NPU_HAS_FW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- wrapper: compact alias of the instruction registers, relative to the NPU base ---- */
#define NPU_HAS_MODEL_RICH_BASE   0x40000000u
#define NPU_HAS_VP_RICH_ALIAS     0x00010000u
#define NPU_HAS_VP_OFF(model)     (NPU_HAS_VP_RICH_ALIAS + ((model) - NPU_HAS_MODEL_RICH_BASE))

/* ---- wrapper: software bank, relative to the NPU base (wrapper register map) ---- */
#define NPU_HAS_WR_BASE           0x00030000u
#define NPU_HAS_WR_STATUS         (NPU_HAS_WR_BASE + 0x0004u)   /* BUSY[0] DONE[1] ERROR[2] IDLE[3]; write 1 to clear */
#define NPU_HAS_WR_STATUS_DONE    0x2u
#define NPU_HAS_WR_STATUS_ERROR   0x4u
#define NPU_HAS_WR_LAST_ERROR     (NPU_HAS_WR_BASE + 0x101Cu)
#define NPU_HAS_WR_WINDOW_BASE    (NPU_HAS_WR_BASE + 0x1130u)   /* added by npu_tlm_rich_window.patch */
#define NPU_HAS_WR_WINDOW_SIZE    (NPU_HAS_WR_BASE + 0x1134u)
#define NPU_HAS_PERF_EXEC_LO      0x1200u                       /* cycles of the last completed instruction */
#define NPU_HAS_PERF_EXEC_HI      0x1240u

/* ---- instruction registers (model addresses) ---- */
#define NPU_HAS_PUSH_A            0x40000310u   /* write the opcode: pushes one instruction */
#define NPU_HAS_IN_ADDR           0x40000400u
#define NPU_HAS_W_ADDR            0x40000404u
#define NPU_HAS_OUT_ADDR          0x40000408u
#define NPU_HAS_BIAS_ADDR         0x4000040Cu
#define NPU_HAS_M                 0x40000410u
#define NPU_HAS_K                 0x40000414u
#define NPU_HAS_N                 0x40000418u
#define NPU_HAS_KH                0x4000041Cu
#define NPU_HAS_KW                0x40000420u
#define NPU_HAS_STRIDE            0x40000424u
#define NPU_HAS_PAD               0x40000428u
#define NPU_HAS_ACT_TYPE          0x4000042Cu
#define NPU_HAS_HAS_SKIP          0x40000430u
#define NPU_HAS_SKIP_ADDR         0x40000434u
#define NPU_HAS_A_ADDR            0x40000444u   /* ELEM_WISE A; FUSED_ATTN Q */
#define NPU_HAS_B_ADDR            0x40000448u   /* ELEM_WISE B; FUSED_ATTN K */
#define NPU_HAS_V_ADDR            0x4000044Cu   /* FUSED_ATTN V */
#define NPU_HAS_LEN               0x40000450u
#define NPU_HAS_MODE_PACK         0x40000454u   /* element-wise mode in bits 31:24 */
#define NPU_HAS_HEAD_DIM          0x40000458u   /* FUSED_ATTN head dimension D */
#define NPU_HAS_A_LEN             0x40000464u
#define NPU_HAS_B_LEN             0x40000468u

/* ---- extension registers (model addresses); one-shot: cleared by every push ---- */
#define NPU_HAS_EXT_BASE          0x4000046Cu
#define NPU_HAS_EXT(i)            (NPU_HAS_EXT_BASE + 4u * (i))
#define NPU_HAS_IN_C              NPU_HAS_EXT(0)
#define NPU_HAS_IN_H              NPU_HAS_EXT(1)
#define NPU_HAS_IN_W              NPU_HAS_EXT(2)
#define NPU_HAS_OUT_C             NPU_HAS_EXT(3)
#define NPU_HAS_OUT_H             NPU_HAS_EXT(4)
#define NPU_HAS_OUT_W             NPU_HAS_EXT(5)
#define NPU_HAS_TILE_COUT         NPU_HAS_EXT(6)
#define NPU_HAS_TILE_H            NPU_HAS_EXT(7)
#define NPU_HAS_TILE_W            NPU_HAS_EXT(8)
#define NPU_HAS_SCALE_ADDR        NPU_HAS_EXT(9)
#define NPU_HAS_SHIFT_ADDR        NPU_HAS_EXT(10)
#define NPU_HAS_LUT_ADDR          NPU_HAS_EXT(11)
#define NPU_HAS_ZP_OUT            NPU_HAS_EXT(12)
#define NPU_HAS_FLAGS             NPU_HAS_EXT(13)
#define NPU_HAS_ZP_A              NPU_HAS_EXT(14)
#define NPU_HAS_ZP_B              NPU_HAS_EXT(15)
#define NPU_HAS_ZP_O              NPU_HAS_EXT(16)
#define NPU_HAS_SA                NPU_HAS_EXT(17)
#define NPU_HAS_SHA               NPU_HAS_EXT(18)
#define NPU_HAS_SB                NPU_HAS_EXT(19)
#define NPU_HAS_SHB               NPU_HAS_EXT(20)
#define NPU_HAS_SO                NPU_HAS_EXT(21)
#define NPU_HAS_SHO               NPU_HAS_EXT(22)
#define NPU_HAS_POOL_K            NPU_HAS_EXT(23)
#define NPU_HAS_POOL_P            NPU_HAS_EXT(24)
#define NPU_HAS_POOL_MODE         NPU_HAS_EXT(25)
#define NPU_HAS_TILE_CIN          NPU_HAS_EXT(26)
#define NPU_HAS_Y_USED            NPU_HAS_EXT(27)
#define NPU_HAS_ROWS              NPU_HAS_EXT(28)
#define NPU_HAS_PARAM_ADDR        NPU_HAS_EXT(29)
#define NPU_HAS_MASK_ADDR         NPU_HAS_EXT(30)
#define NPU_HAS_EXT_END           NPU_HAS_EXT(31)   /* exclusive */

#define NPU_HAS_FLAG_PAD_TAIL      0x1u
#define NPU_HAS_FLAG_CHANNEL_MAJOR 0x2u

/* ---- opcodes (PUSH_A), element-wise modes, activation types ---- */
#define NPU_HAS_OP_SET_NSPLIT     0x05u
#define NPU_HAS_OP_GEMM_FUSED     0x12u
#define NPU_HAS_OP_FUSED_ATTN     0x13u
#define NPU_HAS_OP_LAYERNORM      0x14u
#define NPU_HAS_OP_ELEM_WISE      0x15u
#define NPU_HAS_ELEM_ADD          0u
#define NPU_HAS_ELEM_MAX_POOL     1u
#define NPU_HAS_ELEM_AVG_POOL     5u
#define NPU_HAS_ACT_NONE          0u
#define NPU_HAS_ACT_RELU          1u
#define NPU_HAS_ACT_SILU          2u
#define NPU_HAS_ACT_GELU          3u

/* 1 when the register holds a DRAM address: the program stores image offsets, firmware adds the RAM base of the image. */
int npu_has_is_address_register(uint32_t model_addr);

/* ---- program stream (written by make_fw_program.py): little-endian 32-bit words ----
 * header : magic, version, total words, image bytes, instructions, host steps
 * records: NPU_HAS_REC_WRITE  model address, value      (a write to PUSH_A ends an instruction)
 *          NPU_HAS_REC_HOST   input offset, output offset, c, h, w   (nearest-neighbour upsample x2 of [c][h][w] int8)
 *          NPU_HAS_REC_CHECK  output offset, bytes, CRC-32 of the expected output   (after every instruction / host step)
 *          NPU_HAS_REC_END
 */
#define NPU_HAS_STREAM_MAGIC      0x3150484Eu   /* "NHP1" */
#define NPU_HAS_STREAM_VERSION    1u
#define NPU_HAS_STREAM_HDR_WORDS  6u
#define NPU_HAS_REC_END           0u
#define NPU_HAS_REC_WRITE         1u
#define NPU_HAS_REC_HOST          2u
#define NPU_HAS_REC_CHECK         3u

#define NPU_HAS_REPLAY_HOST_ONLY  0x1u   /* skip the instructions: run host steps and checks only (RAM holds the golden image) */
#define NPU_HAS_REPLAY_NO_CHECK   0x2u   /* do not compute the output CRCs */

typedef struct {
    uint32_t instructions;     /* instructions executed */
    uint32_t host_steps;       /* host steps executed */
    uint32_t failed;           /* instructions or host steps with a wrapper error or a CRC mismatch */
    uint32_t first_failed;     /* index (instructions and host steps counted together) of the first failure */
    uint32_t last_status;      /* wrapper STATUS of the last instruction */
    uint32_t last_error;       /* wrapper LAST_ERROR when an error was reported */
    uint64_t exec_cycles;      /* sum of PERF_EXEC_CYCLES over the instructions */
} npu_has_result;

/* Runs the program. `stream` points to the stream words; `ram_base` is the physical address where the program's DRAM
 * image (dram_init.bin) was placed. Returns 0 when every step passed, a negative value when the stream is malformed,
 * otherwise the number of failed steps. */
int npu_has_replay(const uint32_t *stream, uint32_t ram_base, uint32_t flags, npu_has_result *res);

uint32_t npu_has_crc32(uint32_t crc, const uint8_t *data, uint32_t n);   /* CRC-32 (IEEE), start with crc = 0 */

/* ---- hardware abstraction, supplied by the integrator ---- */
void     npu_hal_write32(uint32_t npu_offset, uint32_t value);            /* 32-bit write to NPU base + offset */
uint32_t npu_hal_read32(uint32_t npu_offset);
void     npu_hal_ram_read(uint32_t phys, uint8_t *dst, uint32_t n);       /* copy from system RAM */
void     npu_hal_ram_write(uint32_t phys, const uint8_t *src, uint32_t n);
void     npu_hal_idle(void);                                              /* called while waiting for completion */
/* Progress report after every step; kind 'I' instruction or 'H' host step; ok = 1 when the step passed;
 * exec_cycles = cycles of that instruction (0 for a host step). */
void     npu_hal_report(char kind, uint32_t index, int ok, uint32_t status, uint64_t exec_cycles);

#ifdef __cplusplus
}
#endif

#endif /* NPU_HAS_FW_H */
