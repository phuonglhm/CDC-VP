/*
 * Host test of the firmware's job checks (drivers/fx1_npu_job.c,
 * fx1_npu_job_check): valid fixture images pass; each corruption below, made
 * on a valid image with its CRC recomputed (except the CRC case), is rejected
 * with the expected reason and index. Nothing of an invalid job may reach the
 * NPU, so every rule the firmware relies on has a case here.
 *
 *   test_npu_job_check <fixtures dir>   (fx1_npu_jobs: chain, mixed, dfc)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fx1/fx1_memory_map.h"
#include "fx1_npu_job.h"

static uint8_t base_img[FX1_NPU_JOB_SIZE], img[FX1_NPU_JOB_SIZE];
static uint32_t base_len;
static int failures, cases;

static int load(const char* dir, const char* name)
{
    char p[1024];
    snprintf(p, sizeof p, "%.900s/%.60s/job.bin", dir, name);
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    base_len = (uint32_t)fread(base_img, 1, sizeof base_img, f);
    fclose(f);
    return base_len > 0;
}

static fx1_npu_job_header* H(void) { return (fx1_npu_job_header*)img; }
static fx1_npu_job_tensor* T(unsigned i) { return (fx1_npu_job_tensor*)(img + H()->tensor_offset) + i; }
static fx1_npu_job_op* O(unsigned i) { return (fx1_npu_job_op*)(img + H()->op_offset) + i; }
static void reset(void) { memcpy(img, base_img, sizeof img); }
static void recrc(void)
{
    H()->job_crc = 0;
    H()->job_crc = fx1_npu_job_crc32(0, img, H()->total_bytes);
}

static void expect(const char* what, uint32_t reason, uint32_t index)
{
    ++cases;
    const uint32_t got = fx1_npu_job_check(img, base_len);
    const uint32_t want = reason << 16 | (index & 0xFFFFu);
    if (got != want) {
        ++failures;
        printf("  [FAIL] %s: got %s/%u, expected %s/%u\n", what, fx1_npu_job_reason_name(got >> 16), got & 0xFFFFu,
               fx1_npu_job_reason_name(reason), index);
    }
}

/* Index of the first tensor / op matching a predicate. */
static int tensor_with_role(uint8_t role)
{
    for (unsigned i = 0; i < H()->n_tensors; ++i)
        if (T(i)->role == role) return (int)i;
    return -1;
}
static int op_of_kind(uint32_t kind)
{
    for (unsigned i = 0; i < H()->n_ops; ++i)
        if (O(i)->kind == kind) return (int)i;
    return -1;
}

static void header_cases(void)
{
    reset();
    expect("valid image", FX1_NPU_JOB_E_NONE, 0);
    reset(); H()->magic ^= 1; expect("magic", FX1_NPU_JOB_E_MAGIC, 0);
    reset(); H()->version = 2; recrc(); expect("version", FX1_NPU_JOB_E_VERSION, 0);
    reset(); H()->header_bytes = 124; recrc(); expect("header_bytes", FX1_NPU_JOB_E_SIZE, 0);
    reset(); H()->total_bytes = FX1_NPU_JOB_SIZE + 4; expect("total_bytes past 64 KiB", FX1_NPU_JOB_E_SIZE, 0);
    reset(); img[200] ^= 0x40; expect("CRC (a table byte changed)", FX1_NPU_JOB_E_CRC, 0);
    reset(); H()->abi_major = 2; recrc(); expect("ABI major", FX1_NPU_JOB_E_ABI, 0);
    reset(); H()->reserved[3] = 1; recrc(); expect("reserved word", FX1_NPU_JOB_E_RESERVED, 3);
    reset(); H()->name[0] = '/'; recrc(); expect("name character", FX1_NPU_JOB_E_NAME, 0);
    reset(); memset(H()->name, 'a', 32); recrc(); expect("name not terminated", FX1_NPU_JOB_E_NAME, 0);
    reset(); H()->timeout_ms = 0; recrc(); expect("timeout 0", FX1_NPU_JOB_E_TIMEOUT, 0);
    reset(); H()->timeout_ms = FX1_NPU_JOB_TIMEOUT_MAX + 1; recrc(); expect("timeout too long", FX1_NPU_JOB_E_TIMEOUT, 0);
    reset(); H()->arena = 2; recrc(); expect("arena", FX1_NPU_JOB_E_ARENA, 0);
    reset(); H()->window_offset += 0x800; recrc(); expect("window offset not 4 KiB aligned", FX1_NPU_JOB_E_WINDOW, 0);
    reset(); H()->window_offset = FX1_NPU_ARENA_SMALL_SIZE; recrc(); expect("window past the arena", FX1_NPU_JOB_E_WINDOW, 0);
    reset(); H()->window_offset = 0xFFFFF000u; recrc(); expect("window offset wrapping", FX1_NPU_JOB_E_WINDOW, 0);
    reset(); H()->window_size = 0; recrc(); expect("window size 0", FX1_NPU_JOB_E_WINDOW, 0);
    reset(); H()->n_tensors = 0; recrc(); expect("no tensor", FX1_NPU_JOB_E_TABLE, 0);
    reset(); H()->n_ops = FX1_NPU_JOB_MAX_OPS + 1; recrc(); expect("too many ops", FX1_NPU_JOB_E_TABLE, 0);
    reset(); H()->op_offset += 4; recrc(); expect("op table offset", FX1_NPU_JOB_E_TABLE, 0);
}

static void tensor_cases(void)
{
    const int in = (reset(), tensor_with_role(FX1_NPU_JOB_INPUT));
    const int out = tensor_with_role(FX1_NPU_JOB_OUTPUT);
    reset(); T(in)->role = 0; recrc(); expect("tensor role 0", FX1_NPU_JOB_E_TENSOR_ROLE, in);
    reset(); T(in)->dtype = 3; recrc(); expect("tensor dtype", FX1_NPU_JOB_E_TENSOR_TYPE, in);
    reset(); T(in)->layout = 4; recrc(); expect("tensor layout", FX1_NPU_JOB_E_TENSOR_TYPE, in);
    reset(); T(in)->rank = 2; recrc(); expect("rank / layout", FX1_NPU_JOB_E_TENSOR_SHAPE, in);
    reset(); T(in)->dims[0] += 1; recrc(); expect("dims x size != bytes", FX1_NPU_JOB_E_TENSOR_SHAPE, in);
    reset(); T(in)->dims[3] = 2; recrc(); expect("unused dim not 1", FX1_NPU_JOB_E_TENSOR_SHAPE, in);
    reset(); T(in)->dims[0] = 0; recrc(); expect("dim 0", FX1_NPU_JOB_E_TENSOR_SHAPE, in);
    reset(); T(in)->offset += 2; recrc(); expect("offset not 4-byte aligned", FX1_NPU_JOB_E_TENSOR_ALIGN, in);
    reset(); T(out)->offset = H()->window_size - 4; recrc(); expect("tensor past the window end", FX1_NPU_JOB_E_TENSOR_BOUNDS, out);
    reset(); T(out)->offset = 0xFFFFFF00u; recrc(); expect("tensor offset wrapping", FX1_NPU_JOB_E_TENSOR_BOUNDS, out);
    /* overlap: the output moved onto the input */
    reset();
    T(out)->offset = T(in)->offset;
    recrc();
    expect("tensors overlap", FX1_NPU_JOB_E_TENSOR_OVERLAP, in > out ? in : out);
    const int scr = (reset(), tensor_with_role(FX1_NPU_JOB_SCRATCH));
    if (scr >= 0) {
        reset(); T(scr)->crc = 1; recrc(); expect("SCRATCH with a CRC", FX1_NPU_JOB_E_TENSOR_CRC, scr);
    }
    reset();
    for (unsigned i = 0; i < H()->n_tensors; ++i)
        if (T(i)->role == FX1_NPU_JOB_OUTPUT) T(i)->role = FX1_NPU_JOB_SCRATCH, T(i)->crc = 0;
    recrc();
    expect("no OUTPUT tensor", FX1_NPU_JOB_E_UNWRITTEN, 0xFFFFu);
}

static void op_cases(void)
{
    reset(); O(0)->kind = 0; recrc(); expect("op kind 0", FX1_NPU_JOB_E_OP_KIND, 0);
    reset(); O(0)->kind = 8; recrc(); expect("op kind 8", FX1_NPU_JOB_E_OP_KIND, 0);
    reset(); O(0)->reserved = 1; recrc(); expect("op reserved word", FX1_NPU_JOB_E_OP_PARAM, 0);
    reset(); O(0)->p[11] = 1; recrc(); expect("unused parameter set", FX1_NPU_JOB_E_OP_PARAM, 0);
    reset(); O(0)->t[7] = 0; recrc(); expect("unused slot set", FX1_NPU_JOB_E_OP_TENSOR, 0);
    reset(); O(0)->t[FX1_NPU_JOB_T_IN] = (uint8_t)H()->n_tensors; recrc(); expect("tensor index past the table", FX1_NPU_JOB_E_OP_TENSOR, 0);
    reset(); O(0)->t[FX1_NPU_JOB_T_IN] = FX1_NPU_JOB_NONE; recrc(); expect("required slot empty", FX1_NPU_JOB_E_OP_TENSOR, 0);
    /* the destination an INPUT tensor */
    reset(); O(0)->t[FX1_NPU_JOB_T_OUT] = O(0)->t[FX1_NPU_JOB_T_IN]; recrc(); expect("op writes an INPUT", FX1_NPU_JOB_E_OP_ROLE, 0);
    /* the same destination twice */
    reset(); O(1)->t[FX1_NPU_JOB_T_OUT] = O(0)->t[FX1_NPU_JOB_T_OUT]; recrc(); expect("destination written twice", FX1_NPU_JOB_E_OP_ORDER, 1);
}

static void chain_cases(void) /* chain: conv, conv + skip, MAX_POOL, AVG_POOL, ADD */
{
    /* op 1 reads op 0's output: swapped, op 0 reads before anything wrote it */
    reset();
    {
        fx1_npu_job_op a = *O(0);
        *O(0) = *O(1);
        *O(1) = a;
        recrc();
        expect("source read before it is written", FX1_NPU_JOB_E_OP_ORDER, 0);
    }
    const int conv = (reset(), op_of_kind(FX1_NPU_JOB_CONV));
    reset(); O(conv)->p[0] = 5; recrc(); expect("conv kh != weight dims", FX1_NPU_JOB_E_OP_SHAPE, conv);
    {   /* weights [cin][kh][kw][cout] given as [cout][kh][kw][cin]: same bytes, only the weight rule sees it */
        reset();
        fx1_npu_job_tensor* w = T(O(conv)->t[FX1_NPU_JOB_T_W]);
        const uint32_t cin = w->dims[0];
        w->dims[0] = w->dims[3];
        w->dims[3] = cin;
        recrc();
        expect("conv weight dims in the wrong order", FX1_NPU_JOB_E_OP_SHAPE, conv);
    }
    reset(); O(conv)->p[2] = 0; recrc(); expect("conv stride 0", FX1_NPU_JOB_E_OP_PARAM, conv);
    reset(); O(conv)->p[3] = 3; recrc(); expect("conv pad >= kernel", FX1_NPU_JOB_E_OP_PARAM, conv);
    reset(); O(conv)->p[4] = 2; recrc(); expect("conv activation", FX1_NPU_JOB_E_OP_PARAM, conv);
    reset(); O(conv)->p[5] = 200; recrc(); expect("conv zero point", FX1_NPU_JOB_E_OP_PARAM, conv);
    reset(); O(conv)->p[2] = 2; recrc(); expect("conv stride 2: output dims differ", FX1_NPU_JOB_E_OP_SHAPE, conv);
    reset(); O(conv)->t[FX1_NPU_JOB_T_BIAS] = O(conv)->t[FX1_NPU_JOB_T_IN]; recrc(); expect("conv bias of the wrong dtype", FX1_NPU_JOB_E_OP_SHAPE, conv);
    const int mp = (reset(), op_of_kind(FX1_NPU_JOB_MAX_POOL));
    reset(); O(mp)->p[0] = 0; recrc(); expect("pool k 0", FX1_NPU_JOB_E_OP_PARAM, mp);
    reset(); O(mp)->p[2] = 1; recrc(); expect("pool stride 1: output dims differ", FX1_NPU_JOB_E_OP_SHAPE, mp);
    const int ap = (reset(), op_of_kind(FX1_NPU_JOB_AVG_POOL));
    reset(); O(ap)->p[1] = 1; recrc(); expect("AVG_POOL with padding", FX1_NPU_JOB_E_OP_PARAM, ap);
    reset(); O(ap)->p[4] = 40; recrc(); expect("AVG_POOL shift", FX1_NPU_JOB_E_OP_PARAM, ap);
    const int add = (reset(), op_of_kind(FX1_NPU_JOB_ADD));
    reset(); O(add)->p[4] = -1; recrc(); expect("ADD shift", FX1_NPU_JOB_E_OP_PARAM, add);
    reset(); O(add)->p[3] = -5; recrc(); expect("ADD negative scale", FX1_NPU_JOB_E_OP_PARAM, add);
    reset(); O(add)->t[FX1_NPU_JOB_T_W] = O(conv)->t[FX1_NPU_JOB_T_W]; recrc(); expect("ADD operands of different sizes", FX1_NPU_JOB_E_OP_SHAPE, add);
    /* An OUTPUT nobody writes: drop the last op. */
    reset(); H()->n_ops -= 1; H()->total_bytes -= sizeof(fx1_npu_job_op); recrc();
    expect("OUTPUT never written", FX1_NPU_JOB_E_UNWRITTEN, (uint32_t)O(add)->t[FX1_NPU_JOB_T_OUT]);
}

static void mixed_cases(void) /* mixed: conv, GEMM bank, MAX_POOL, neo RESIDUAL */
{
    const int g = (reset(), op_of_kind(FX1_NPU_JOB_GEMM_BANK));
    reset(); O(g)->p[0] = 0; recrc(); expect("GEMM K 0", FX1_NPU_JOB_E_OP_PARAM, g);
    reset(); O(g)->p[0] = 993; recrc(); expect("GEMM K above 992", FX1_NPU_JOB_E_OP_PARAM, g);
    reset(); O(g)->p[0] = 32; recrc(); expect("GEMM K != tensor dims", FX1_NPU_JOB_E_OP_SHAPE, g);
    const int r = (reset(), op_of_kind(FX1_NPU_JOB_NEO_RESIDUAL));
    reset(); O(r)->p[3] = 3001; O(r)->p[4] = 3; recrc(); /* 3001 >> 3 is not exact */
    expect("neo RESIDUAL a per-operand shift it cannot express", FX1_NPU_JOB_E_OP_BUILD, r);
    reset(); O(r)->p[3] = 3000; O(r)->p[4] = 3; recrc(); /* 3000 >> 3 = 375 exactly: expressible */
    expect("neo RESIDUAL a reducible per-operand shift", FX1_NPU_JOB_E_NONE, 0);
}

static void dfc_cases(void) /* dfc: neo MAX_POOL, neo RESIDUAL */
{
    const int p = (reset(), op_of_kind(FX1_NPU_JOB_NEO_MAX_POOL));
    reset(); O(p)->p[1] = 1; recrc(); expect("neo MAX_POOL with padding", FX1_NPU_JOB_E_OP_PARAM, p);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_npu_job_check <fixtures dir>\n");
        return 2;
    }
    static const struct {
        const char* name;
        void (*fn)(void);
    } sets[] = {{"chain", chain_cases}, {"mixed", mixed_cases}, {"dfc", dfc_cases}};
    for (unsigned k = 0; k < 3; ++k) {
        if (!load(argv[1], sets[k].name)) {
            printf("  [FAIL] cannot read %s/%s/job.bin\n", argv[1], sets[k].name);
            return 1;
        }
        printf("[job-check] %s\n", sets[k].name);
        header_cases();
        tensor_cases();
        op_cases();
        sets[k].fn();
    }
    printf("[job-check] %d cases, %d failures\nRESULT %s\n", cases, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
