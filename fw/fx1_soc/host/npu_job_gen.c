/*
 * npu_job_gen: the fixtures of the FX1 NPU job format (drivers/fx1_npu_job.h),
 * with their goldens from the firmware's C oracle (tests/npu_common/npu_oracle.c,
 * cross-checked with the drop's numpy reference by fx1_regression_npu_job_xcheck).
 *
 *   npu_job_gen <out>                   every fixture in <out>/<name>/
 *   npu_job_gen --xcheck <name> <dir>   <name> as a ref_program.py program
 *                                       directory (mmio.txt, dram_init.bin,
 *                                       c_oracle.bin; GEMM-bank ops left out)
 *   npu_job_gen --list
 *
 * A fixture directory:
 *   job.bin        the job image, for fx1_soc --load <job_addr>:job.bin
 *   window.bin     the INPUT / CONST tensors as the window holds them, from the
 *                  window base, for --load <window_addr>:window.bin
 *   golden_<i>.bin the expected bytes of OUTPUT tensor i
 *   fixture.json   addresses, sizes, outputs (CRC-32), the expected outcome
 * Every valid fixture passes fx1_npu_job_check (the firmware's own code); the
 * negative ones are made from a valid fixture and fail it as declared.
 *
 * Ownership of <out> (review N5 R1): it must be new, empty, or one this tool
 * made (marker file .npu_job_gen). Anything else is refused (exit 2) before a
 * file is written. In its own directory the tool replaces only what it manages:
 * <out>/.npu_job_gen and, in <out>/<fixture>/, job.bin, window.bin,
 * fixture.json and golden_<n>.bin (a stale golden of an earlier run goes).
 * Every other file, and every other directory (your own fixtures), is kept.
 * Links (review N5 R3): the tool never writes through a link. A symbolic link,
 * or anything but a regular file, at the marker or at a managed name of a
 * fixture directory is refused (exit 2) before a file is written; each file is
 * created anew (unlink, then O_CREAT | O_EXCL | O_NOFOLLOW), so a hard link's
 * other names keep their content. <out> itself may be a link to a directory:
 * the rules then apply to that directory.
 */
#define _POSIX_C_SOURCE 200809L /* lstat, opendir: a host tool (Linux) */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fx1/fx1_memory_map.h"
#include "fx1_npu_job.h"
#include "npu_oracle.h"

#define MAXT FX1_NPU_JOB_MAX_TENSORS
#define MAXO FX1_NPU_JOB_MAX_OPS

typedef struct {
    const char* name;
    const char* desc;
    fx1_npu_job_header h;
    fx1_npu_job_tensor t[MAXT];
    fx1_npu_job_op op[MAXO];
    const char* tname[MAXT];
    uint8_t* init; /* the window as loaded and as the NPU starts (OUTPUT / SCRATCH: canary) */
    uint32_t next; /* allocator */
    uint32_t expect_status, expect_detail;
    int corrupt_load; /* negative fixture: one byte of window.bin differs from the CRC */
} fixture;

static uint32_t rng_state;
static uint32_t rng(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}
static int32_t rnd(int32_t lo, int32_t hi) { return lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }

static void die(const char* m)
{
    fprintf(stderr, "npu_job_gen: %s\n", m);
    exit(1);
}

static void begin(fixture* f, const char* name, const char* desc, uint32_t arena, uint32_t woff, uint32_t wsize,
                  uint32_t timeout_ms, uint32_t seed)
{
    memset(f, 0, sizeof *f);
    f->name = name;
    f->desc = desc;
    f->h.magic = FX1_NPU_JOB_MAGIC;
    f->h.version = FX1_NPU_JOB_VERSION;
    f->h.header_bytes = sizeof f->h;
    f->h.abi_major = FX1_NPU_VP_ABI_MAJOR(FX1_NPU_VP_ABI_VALUE);
    f->h.arena = arena;
    f->h.window_offset = woff;
    f->h.window_size = wsize;
    f->h.timeout_ms = timeout_ms;
    snprintf(f->h.name, sizeof f->h.name, "%s", name);
    f->init = calloc(1, wsize);
    if (!f->init) die("out of memory");
    f->next = 0x100; /* no tensor at offset 0 */
    rng_state = seed;
}

static uint32_t tensor(fixture* f, const char* name, uint8_t role, uint8_t dtype, uint8_t layout, uint32_t d0,
                       uint32_t d1, uint32_t d2, uint32_t d3)
{
    static const uint8_t rank_of[] = {1, 3, 2, 4};
    if (f->h.n_tensors >= MAXT) die("too many tensors");
    const uint32_t i = f->h.n_tensors++;
    fx1_npu_job_tensor* x = &f->t[i];
    x->role = role;
    x->dtype = dtype;
    x->layout = layout;
    x->rank = rank_of[layout];
    x->dims[0] = d0;
    x->dims[1] = d1;
    x->dims[2] = d2;
    x->dims[3] = d3;
    x->bytes = d0 * d1 * d2 * d3 * (dtype == FX1_NPU_JOB_INT32 ? 4u : 1u);
    x->offset = (f->next + 63u) & ~63u;
    f->next = x->offset + x->bytes;
    if (f->next > f->h.window_size) die("window too small");
    f->tname[i] = name;
    return i;
}
static uint32_t t8(fixture* f, const char* n, uint8_t role, uint32_t c, uint32_t h, uint32_t w)
{
    return tensor(f, n, role, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CHW, c, h, w, 1);
}
static uint32_t vec32(fixture* f, const char* n, uint32_t len)
{
    return tensor(f, n, FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT32, FX1_NPU_JOB_FLAT, len, 1, 1, 1);
}

static void rand8(fixture* f, uint32_t i, int32_t lo, int32_t hi)
{
    for (uint32_t k = 0; k < f->t[i].bytes; ++k) f->init[f->t[i].offset + k] = (uint8_t)(int8_t)rnd(lo, hi);
}
static void put32(fixture* f, uint32_t i, uint32_t k, int32_t v)
{
    const uint32_t u = (uint32_t)v, o = f->t[i].offset + 4 * k;
    f->init[o] = (uint8_t)u;
    f->init[o + 1] = (uint8_t)(u >> 8);
    f->init[o + 2] = (uint8_t)(u >> 16);
    f->init[o + 3] = (uint8_t)(u >> 24);
}
static void rand32(fixture* f, uint32_t i, int32_t lo, int32_t hi)
{
    for (uint32_t k = 0; k < f->t[i].bytes / 4; ++k) put32(f, i, k, rnd(lo, hi));
}
static void const32(fixture* f, uint32_t i, int32_t v)
{
    for (uint32_t k = 0; k < f->t[i].bytes / 4; ++k) put32(f, i, k, v);
}

static fx1_npu_job_op* op(fixture* f, uint32_t kind)
{
    if (f->h.n_ops >= MAXO) die("too many ops");
    fx1_npu_job_op* o = &f->op[f->h.n_ops++];
    o->kind = kind;
    memset(o->t, FX1_NPU_JOB_NONE, sizeof o->t);
    return o;
}
static void conv(fixture* f, uint32_t in, uint32_t w, uint32_t b, uint32_t s, uint32_t sh, int skip, uint32_t out,
                 int32_t kh, int32_t kw, int32_t stride, int32_t pad, int32_t act, int32_t zp)
{
    fx1_npu_job_op* o = op(f, FX1_NPU_JOB_CONV);
    o->t[FX1_NPU_JOB_T_IN] = (uint8_t)in;
    o->t[FX1_NPU_JOB_T_W] = (uint8_t)w;
    o->t[FX1_NPU_JOB_T_BIAS] = (uint8_t)b;
    o->t[FX1_NPU_JOB_T_SCALE] = (uint8_t)s;
    o->t[FX1_NPU_JOB_T_SHIFT] = (uint8_t)sh;
    if (skip >= 0) o->t[FX1_NPU_JOB_T_SKIP] = (uint8_t)skip;
    o->t[FX1_NPU_JOB_T_OUT] = (uint8_t)out;
    const int32_t p[] = {kh, kw, stride, pad, act, zp};
    memcpy(o->p, p, sizeof p);
}
static void pool(fixture* f, uint32_t kind, uint32_t in, uint32_t out, int32_t k, int32_t pad, int32_t stride,
                 int32_t so, int32_t sho)
{
    fx1_npu_job_op* o = op(f, kind);
    o->t[FX1_NPU_JOB_T_IN] = (uint8_t)in;
    o->t[FX1_NPU_JOB_T_OUT] = (uint8_t)out;
    o->p[0] = k;
    o->p[1] = pad;
    o->p[2] = stride;
    if (kind == FX1_NPU_JOB_AVG_POOL) {
        o->p[3] = so;
        o->p[4] = sho;
    }
}
static void add(fixture* f, uint32_t kind, uint32_t a, uint32_t b, uint32_t out, const int32_t q[9])
{
    fx1_npu_job_op* o = op(f, kind);
    o->t[FX1_NPU_JOB_T_IN] = (uint8_t)a;
    o->t[FX1_NPU_JOB_T_W] = (uint8_t)b;
    o->t[FX1_NPU_JOB_T_OUT] = (uint8_t)out;
    memcpy(o->p, q, 9 * sizeof q[0]);
}
static void gemm(fixture* f, uint32_t a, uint32_t w, uint32_t c, int32_t k)
{
    fx1_npu_job_op* o = op(f, FX1_NPU_JOB_GEMM_BANK);
    o->t[FX1_NPU_JOB_T_IN] = (uint8_t)a;
    o->t[FX1_NPU_JOB_T_W] = (uint8_t)w;
    o->t[FX1_NPU_JOB_T_OUT] = (uint8_t)c;
    o->p[0] = k;
}

/* ---- oracle ------------------------------------------------------------------------ */
#define P8(i) ((int8_t*)(img + f->t[i].offset))
#define C8(i) ((const int8_t*)(img + f->t[i].offset))
#define C32(i) ((const int32_t*)(const void*)(img + f->t[i].offset))

static void apply(const fixture* f, uint8_t* img, int with_gemm)
{
    for (uint32_t i = 0; i < f->h.n_ops; ++i) {
        const fx1_npu_job_op* o = &f->op[i];
        const fx1_npu_job_tensor* in = &f->t[o->t[FX1_NPU_JOB_T_IN]];
        const uint8_t ti = o->t[FX1_NPU_JOB_T_IN], tw = o->t[FX1_NPU_JOB_T_W], tout = o->t[FX1_NPU_JOB_T_OUT];
        switch (o->kind) {
        case FX1_NPU_JOB_GEMM_BANK:
            if (with_gemm) npu_oracle_gemm_bank(C8(ti), C8(tw), (uint32_t)o->p[0], (int32_t*)(void*)P8(tout));
            break;
        case FX1_NPU_JOB_CONV: {
            const fx1_npu_conv_t c = {.cin = in->dims[0], .ih = in->dims[1], .iw = in->dims[2],
                                      .cout = f->t[tout].dims[0], .kh = (uint32_t)o->p[0], .kw = (uint32_t)o->p[1],
                                      .stride = (uint32_t)o->p[2], .pad = (uint32_t)o->p[3], .act = (uint32_t)o->p[4],
                                      .zp_out = o->p[5], .skip = o->t[FX1_NPU_JOB_T_SKIP] != FX1_NPU_JOB_NONE};
            const uint8_t sk = o->t[FX1_NPU_JOB_T_SKIP];
            npu_oracle_conv(&c, C8(ti), C8(tw), C32(o->t[FX1_NPU_JOB_T_BIAS]), C32(o->t[FX1_NPU_JOB_T_SCALE]),
                            C32(o->t[FX1_NPU_JOB_T_SHIFT]), sk == FX1_NPU_JOB_NONE ? 0 : C8(sk), P8(tout));
            break;
        }
        case FX1_NPU_JOB_MAX_POOL:
        case FX1_NPU_JOB_AVG_POOL:
        case FX1_NPU_JOB_NEO_MAX_POOL: {
            const fx1_npu_pool_t p = {.c = in->dims[0], .h = in->dims[1], .w = in->dims[2], .k = (uint32_t)o->p[0],
                                      .pad = (uint32_t)o->p[1], .stride = (uint32_t)o->p[2],
                                      .avg = o->kind == FX1_NPU_JOB_AVG_POOL, .so = o->p[3], .sho = o->p[4]};
            npu_oracle_pool(&p, C8(ti), P8(tout));
            break;
        }
        case FX1_NPU_JOB_ADD:
        case FX1_NPU_JOB_NEO_RESIDUAL: {
            const fx1_npu_add_t a = {.n = f->t[tout].bytes, .zp_a = o->p[0], .zp_b = o->p[1], .zp_o = o->p[2],
                                     .sa = o->p[3], .sha = o->p[4], .sb = o->p[5], .shb = o->p[6], .so = o->p[7],
                                     .sho = o->p[8]};
            npu_oracle_add(&a, C8(ti), C8(tw), P8(tout));
            break;
        }
        }
    }
}

/* ---- image --------------------------------------------------------------------------- */
static uint8_t image[FX1_NPU_JOB_SIZE];

static uint32_t make_image(fixture* f)
{
    fx1_npu_job_header* h = &f->h;
    h->tensor_offset = sizeof *h;
    h->op_offset = h->tensor_offset + h->n_tensors * sizeof(fx1_npu_job_tensor);
    h->total_bytes = h->op_offset + h->n_ops * sizeof(fx1_npu_job_op);
    h->job_crc = 0;
    memset(image, 0, sizeof image);
    memcpy(image, h, sizeof *h);
    memcpy(image + h->tensor_offset, f->t, h->n_tensors * sizeof(fx1_npu_job_tensor));
    memcpy(image + h->op_offset, f->op, h->n_ops * sizeof(fx1_npu_job_op));
    h->job_crc = fx1_npu_job_crc32(0, image, h->total_bytes);
    memcpy(image, h, sizeof *h);
    return h->total_bytes;
}

/* CRCs, canaries and the reference window: ref = init after the oracle. */
static uint8_t* prepare(fixture* f, int with_gemm)
{
    for (uint32_t i = 0; i < f->h.n_tensors; ++i) {
        fx1_npu_job_tensor* x = &f->t[i];
        if (x->role == FX1_NPU_JOB_OUTPUT || x->role == FX1_NPU_JOB_SCRATCH)
            memset(f->init + x->offset, FX1_NPU_JOB_CANARY, x->bytes);
        else
            x->crc = fx1_npu_job_crc32(0, f->init + x->offset, x->bytes);
    }
    uint8_t* ref = malloc(f->h.window_size);
    if (!ref) die("out of memory");
    memcpy(ref, f->init, f->h.window_size);
    apply(f, ref, with_gemm);
    for (uint32_t i = 0; i < f->h.n_tensors; ++i)
        if (f->t[i].role == FX1_NPU_JOB_OUTPUT) f->t[i].crc = fx1_npu_job_crc32(0, ref + f->t[i].offset, f->t[i].bytes);
    return ref;
}

static uint32_t load_bytes(const fixture* f)
{
    uint32_t end = 0;
    for (uint32_t i = 0; i < f->h.n_tensors; ++i)
        if (f->t[i].role == FX1_NPU_JOB_INPUT || f->t[i].role == FX1_NPU_JOB_CONST)
            if (f->t[i].offset + f->t[i].bytes > end) end = f->t[i].offset + f->t[i].bytes;
    return (end + 3u) & ~3u;
}

/* A new file at p, never through a link (review N5 R3): a regular file there
 * is unlinked first (a hard link's other names keep their content), anything
 * else (a symbolic link, dangling or not) stops the tool. O_EXCL | O_NOFOLLOW
 * also refuse a link made after that check. */
static FILE* create_new(const char* p)
{
    struct stat st;
    if (lstat(p, &st) == 0) {
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "npu_job_gen: %s: a symbolic link or not a regular file; not written through\n", p);
            exit(2);
        }
        if (unlink(p) != 0) die(p);
    } else if (errno != ENOENT) {
        die(p);
    }
    const int fd = open(p, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
    if (fd < 0) die(p);
    FILE* fp = fdopen(fd, "wb");
    if (!fp) die(p);
    return fp;
}

static void write_file(const char* dir, const char* name, const void* d, size_t n)
{
    if (strlen(dir) + strlen(name) + 2 > 1024) die("path too long");
    char p[1100];
    snprintf(p, sizeof p, "%.600s/%.400s", dir, name);
    FILE* fp = create_new(p);
    if (fwrite(d, 1, n, fp) != n || fclose(fp)) die(p);
}

static const char* const role_names[] = {"?", "INPUT", "CONST", "OUTPUT", "SCRATCH"};
static const char* const kind_names[] = {"?", "GEMM_BANK", "CONV", "MAX_POOL", "AVG_POOL", "ADD", "NEO_MAX_POOL", "NEO_RESIDUAL"};

#define MARKER ".npu_job_gen"

static void refuse(const char* path, const char* why)
{
    fprintf(stderr, "npu_job_gen: %s: %s; nothing written\n", path, why);
    exit(2);
}

/* mkdir -p, without a shell. */
static void make_dirs(const char* path)
{
    char p[512];
    if (strlen(path) >= sizeof p) die("output path too long");
    strcpy(p, path);
    for (char* c = p + 1; *c; ++c) {
        if (*c != '/') continue;
        *c = 0;
        if (mkdir(p, 0777) != 0 && errno != EEXIST) die(p);
        *c = '/';
    }
    if (mkdir(p, 0777) != 0 && errno != EEXIST) die(p);
}

/* A file name this tool writes in a fixture directory. */
static int managed(const char* n)
{
    if (!strcmp(n, "job.bin") || !strcmp(n, "window.bin") || !strcmp(n, "fixture.json")) return 1;
    const size_t len = strlen(n);
    if (len <= strlen("golden_.bin") || strncmp(n, "golden_", 7) || strcmp(n + len - 4, ".bin")) return 0;
    for (size_t i = 7; i < len - 4; ++i)
        if (n[i] < '0' || n[i] > '9') return 0;
    return 1;
}

/* The tool's files of an earlier run in a fixture directory (a stale golden
 * must not survive); everything else stays. */
static void clear_managed(const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) return;
    for (struct dirent* e; (e = readdir(d));) {
        if (!managed(e->d_name)) continue;
        char p[1100];
        snprintf(p, sizeof p, "%.600s/%.400s", dir, e->d_name);
        struct stat st;
        if (lstat(p, &st) == 0 && S_ISREG(st.st_mode) && unlink(p) != 0) die(p);
    }
    closedir(d);
}

static void emit(fixture* f, const char* root)
{
    if (strlen(root) > 400) die("output path too long");
    char dir[512];
    snprintf(dir, sizeof dir, "%.400s/%.40s", root, f->name);
    make_dirs(dir);
    clear_managed(dir);
    uint8_t* ref = prepare(f, 1);
    if (f->expect_status == FX1_NPU_JOB_BAD_JOB && f->expect_detail >> 16 == FX1_NPU_JOB_E_TENSOR_BOUNDS) {
        f->t[f->expect_detail & 0xFFFFu].offset = f->h.window_size; /* past the window, the CRC still right */
    }
    const uint32_t len = make_image(f);
    const uint32_t chk = fx1_npu_job_check(image, len);
    if (f->expect_status == FX1_NPU_JOB_BAD_JOB) {
        if (chk != f->expect_detail) die("negative fixture: fx1_npu_job_check does not reject it as declared");
    } else if (chk) {
        fprintf(stderr, "%s: %s, index %u\n", f->name, fx1_npu_job_reason_name(chk >> 16), chk & 0xFFFFu);
        die("fixture rejected by fx1_npu_job_check");
    }
    write_file(dir, "job.bin", image, len);
    const uint32_t lb = load_bytes(f);
    if (f->corrupt_load) f->init[f->t[0].offset] ^= 0x01u; /* t0 is an INPUT */
    write_file(dir, "window.bin", f->init, lb);
    const uint32_t wbase = fx1_npu_job_window_base(&f->h);
    char p[600];
    snprintf(p, sizeof p, "%.500s/fixture.json", dir);
    FILE* j = create_new(p);
    fprintf(j, "{\n  \"name\": \"%s\",\n  \"description\": \"%s\",\n  \"format_version\": %u,\n", f->name, f->desc,
            FX1_NPU_JOB_VERSION);
    fprintf(j, "  \"job_file\": \"job.bin\",\n  \"job_addr\": %u,\n  \"job_bytes\": %u,\n  \"job_crc\": %u,\n",
            FX1_NPU_JOB_BASE, len, f->h.job_crc);
    fprintf(j, "  \"window_file\": \"window.bin\",\n  \"window_addr\": %u,\n  \"window_size\": %u,\n  \"load_bytes\": %u,\n",
            wbase, f->h.window_size, lb);
    fprintf(j, "  \"arena\": \"%s\",\n  \"result_addr\": %u,\n  \"result_bytes\": %u,\n  \"timeout_ms\": %u,\n",
            f->h.arena ? "large" : "small", FX1_NPU_RESULT_BASE, (unsigned)sizeof(fx1_npu_job_result), f->h.timeout_ms);
    fprintf(j, "  \"expect\": {\"status\": \"%s\", \"detail\": %u},\n", fx1_npu_job_status_name(f->expect_status),
            f->expect_detail);
    fprintf(j, "  \"ops\": [");
    for (uint32_t i = 0; i < f->h.n_ops; ++i) fprintf(j, "%s\"%s\"", i ? ", " : "", kind_names[f->op[i].kind]);
    fprintf(j, "],\n  \"tensors\": [\n");
    for (uint32_t i = 0; i < f->h.n_tensors; ++i) {
        const fx1_npu_job_tensor* x = &f->t[i];
        char gname[64] = "";
        if (x->role == FX1_NPU_JOB_OUTPUT && f->expect_status == FX1_NPU_JOB_OK) { /* negative: no golden */
            snprintf(gname, sizeof gname, "golden_%u.bin", i);
            write_file(dir, gname, ref + x->offset, x->bytes);
        }
        fprintf(j,
                "    {\"index\": %u, \"name\": \"%s\", \"role\": \"%s\", \"dtype\": \"%s\", \"dims\": [%u, %u, %u, %u], "
                "\"offset\": %u, \"addr\": %u, \"bytes\": %u, \"crc32\": %u, \"golden\": \"%s\"}%s\n",
                i, f->tname[i], role_names[x->role], x->dtype == FX1_NPU_JOB_INT32 ? "int32" : "int8", x->dims[0],
                x->dims[1], x->dims[2], x->dims[3], x->offset, wbase + x->offset, x->bytes, x->crc, gname,
                i + 1 < f->h.n_tensors ? "," : "");
    }
    fprintf(j, "  ]\n}\n");
    fclose(j);
    printf("npu_job_gen: %-12s %2u tensors %2u ops, window %u KiB at 0x%08x, job %u B, load %u B -> %s\n", f->name,
           f->h.n_tensors, f->h.n_ops, f->h.window_size / 1024, wbase, len, lb, dir);
    free(ref);
    free(f->init);
}

/* ---- fixtures ---------------------------------------------------------------------- */
static void requant(fixture* f, uint32_t b, uint32_t s, uint32_t sh, int32_t blo, int32_t bhi, int32_t slo, int32_t shi)
{
    rand32(f, b, blo, bhi);
    rand32(f, s, slo, shi);
    const32(f, sh, 16);
}

/* conv 3x3 ReLU -> conv 1x1 + residual -> MAX_POOL, AVG_POOL -> ADD (the N2 chain, larger). */
static void fx_chain(fixture* f)
{
    begin(f, "chain", "conv3x3 ReLU, conv1x1 + skip, MAX_POOL 3 p1 s2, AVG_POOL 2 s2, ADD", FX1_NPU_JOB_ARENA_SMALL, 0,
          0x10000, 200, 0x5EED0101u);
    const uint32_t in = t8(f, "in", FX1_NPU_JOB_INPUT, 8, 16, 16);
    const uint32_t w1 = tensor(f, "w1", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CONV_W, 8, 3, 3, 16);
    const uint32_t b1 = vec32(f, "b1", 16), s1 = vec32(f, "s1", 16), h1 = vec32(f, "sh1", 16);
    const uint32_t w2 = tensor(f, "w2", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CONV_W, 16, 1, 1, 16);
    const uint32_t b2 = vec32(f, "b2", 16), s2 = vec32(f, "s2", 16), h2 = vec32(f, "sh2", 16);
    const uint32_t skip = t8(f, "skip", FX1_NPU_JOB_CONST, 16, 16, 16);
    const uint32_t c1 = t8(f, "conv1", FX1_NPU_JOB_OUTPUT, 16, 16, 16);
    const uint32_t c2 = t8(f, "conv2", FX1_NPU_JOB_SCRATCH, 16, 16, 16);
    const uint32_t mp = t8(f, "maxpool", FX1_NPU_JOB_OUTPUT, 16, 8, 8);
    const uint32_t ap = t8(f, "avgpool", FX1_NPU_JOB_OUTPUT, 16, 8, 8);
    const uint32_t sum = tensor(f, "add", FX1_NPU_JOB_OUTPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 16 * 8 * 8, 1, 1, 1);
    rand8(f, in, -128, 127);
    rand8(f, w1, -8, 8);
    requant(f, b1, s1, h1, -2000, 2000, 900, 1500);
    rand8(f, w2, -16, 16);
    requant(f, b2, s2, h2, -500, 500, 1500, 3000);
    rand8(f, skip, -64, 64);
    conv(f, in, w1, b1, s1, h1, -1, c1, 3, 3, 1, 1, FX1_NPU_ACT_RELU, 3);
    conv(f, c1, w2, b2, s2, h2, (int)skip, c2, 1, 1, 1, 0, FX1_NPU_ACT_NONE, -2);
    pool(f, FX1_NPU_JOB_MAX_POOL, c2, mp, 3, 1, 2, 0, 0);
    pool(f, FX1_NPU_JOB_AVG_POOL, c1, ap, 2, 0, 2, 1, 2);
    const int32_t q[9] = {1, -2, 3, 12000, 4, 9000, 3, 3, 12};
    add(f, FX1_NPU_JOB_ADD, mp, ap, sum, q);
}

static void fx_gemm(fixture* f)
{
    begin(f, "gemm", "GEMM bank C[32][32] int32 = A[32][128] x W[128][32]", FX1_NPU_JOB_ARENA_SMALL, 0x10000, 0x10000, 200,
          0x5EED0102u);
    const uint32_t a = tensor(f, "a", FX1_NPU_JOB_INPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_MATRIX, 32, 128, 1, 1);
    const uint32_t w = tensor(f, "w", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_MATRIX, 128, 32, 1, 1);
    const uint32_t c = tensor(f, "c", FX1_NPU_JOB_OUTPUT, FX1_NPU_JOB_INT32, FX1_NPU_JOB_MATRIX, 32, 32, 1, 1);
    rand8(f, a, -128, 127);
    rand8(f, w, -128, 127);
    gemm(f, a, w, c, 128);
}

static void fx_dfc(fixture* f)
{
    begin(f, "dfc", "neo_lite register file: MAX_POOL 3 s1 [16][12][12], RESIDUAL n=1024", FX1_NPU_JOB_ARENA_SMALL,
          0x20000, 0x10000, 200, 0x5EED0103u);
    const uint32_t in = t8(f, "in", FX1_NPU_JOB_INPUT, 16, 12, 12);
    const uint32_t a = tensor(f, "a", FX1_NPU_JOB_INPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 1024, 1, 1, 1);
    const uint32_t b = tensor(f, "b", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 1024, 1, 1, 1);
    const uint32_t mp = t8(f, "maxpool", FX1_NPU_JOB_OUTPUT, 16, 10, 10);
    const uint32_t r = tensor(f, "residual", FX1_NPU_JOB_OUTPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 1024, 1, 1, 1);
    rand8(f, in, -128, 127);
    rand8(f, a, -128, 127);
    rand8(f, b, -128, 127);
    pool(f, FX1_NPU_JOB_NEO_MAX_POOL, in, mp, 3, 0, 1, 0, 0);
    const int32_t q[9] = {3, -5, 2, 3000, 0, 5000, 0, 1, 14}; /* expressible as RESIDUAL (N2 D2) */
    add(f, FX1_NPU_JOB_NEO_RESIDUAL, a, b, r, q);
}

/* Batches around a GEMM-bank op: rich, GEMM, rich + neo. */
static void fx_mixed(fixture* f)
{
    begin(f, "mixed", "conv3x3, GEMM bank K=64, MAX_POOL on the conv, neo RESIDUAL: three batches",
          FX1_NPU_JOB_ARENA_SMALL, 0x30000, 0x10000, 200, 0x5EED0104u);
    const uint32_t in = t8(f, "in", FX1_NPU_JOB_INPUT, 4, 8, 8);
    const uint32_t w = tensor(f, "w", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CONV_W, 4, 3, 3, 8);
    const uint32_t b = vec32(f, "b", 8), s = vec32(f, "s", 8), h = vec32(f, "sh", 8);
    const uint32_t ga = tensor(f, "gemm_a", FX1_NPU_JOB_INPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_MATRIX, 32, 64, 1, 1);
    const uint32_t gw = tensor(f, "gemm_w", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_MATRIX, 64, 32, 1, 1);
    const uint32_t ra = tensor(f, "res_a", FX1_NPU_JOB_INPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 256, 1, 1, 1);
    const uint32_t rb = tensor(f, "res_b", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 256, 1, 1, 1);
    const uint32_t c = t8(f, "conv", FX1_NPU_JOB_SCRATCH, 8, 8, 8);
    const uint32_t gc = tensor(f, "gemm_c", FX1_NPU_JOB_OUTPUT, FX1_NPU_JOB_INT32, FX1_NPU_JOB_MATRIX, 32, 32, 1, 1);
    const uint32_t mp = t8(f, "maxpool", FX1_NPU_JOB_OUTPUT, 8, 4, 4);
    const uint32_t r = tensor(f, "residual", FX1_NPU_JOB_OUTPUT, FX1_NPU_JOB_INT8, FX1_NPU_JOB_FLAT, 256, 1, 1, 1);
    rand8(f, in, -128, 127);
    rand8(f, w, -8, 8);
    requant(f, b, s, h, -2000, 2000, 1500, 2500);
    rand8(f, ga, -128, 127);
    rand8(f, gw, -128, 127);
    rand8(f, ra, -128, 127);
    rand8(f, rb, -128, 127);
    conv(f, in, w, b, s, h, -1, c, 3, 3, 1, 1, FX1_NPU_ACT_RELU, 3);
    gemm(f, ga, gw, gc, 64);
    pool(f, FX1_NPU_JOB_MAX_POOL, c, mp, 3, 1, 2, 0, 0);
    const int32_t q[9] = {3, -5, 2, 3000, 0, 5000, 0, 1, 14};
    add(f, FX1_NPU_JOB_NEO_RESIDUAL, ra, rb, r, q);
}

/* The large arena, a window 1 MiB above its base. */
static void fx_large(fixture* f)
{
    begin(f, "large", "large arena: conv3x3 ReLU [16][32][32] -> [32][32][32], MAX_POOL 2 s2", FX1_NPU_JOB_ARENA_LARGE,
          0x100000, 0x100000, 1000, 0x5EED0105u);
    const uint32_t in = t8(f, "in", FX1_NPU_JOB_INPUT, 16, 32, 32);
    const uint32_t w = tensor(f, "w", FX1_NPU_JOB_CONST, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CONV_W, 16, 3, 3, 32);
    const uint32_t b = vec32(f, "b", 32), s = vec32(f, "s", 32), h = vec32(f, "sh", 32);
    const uint32_t c = t8(f, "conv", FX1_NPU_JOB_OUTPUT, 32, 32, 32);
    const uint32_t mp = t8(f, "maxpool", FX1_NPU_JOB_OUTPUT, 32, 16, 16);
    rand8(f, in, -128, 127);
    rand8(f, w, -8, 8);
    requant(f, b, s, h, -3000, 3000, 500, 900);
    conv(f, in, w, b, s, h, -1, c, 3, 3, 1, 1, FX1_NPU_ACT_RELU, -4);
    pool(f, FX1_NPU_JOB_MAX_POOL, c, mp, 2, 0, 2, 0, 0);
}

/* Long enough to be stopped by `npu reset` or a short `npu wait`: a chain of
 * MAX_POOL 5 p2 s1 on [16][64][64]. */
static void fx_long(fixture* f)
{
    begin(f, "long", "8 x MAX_POOL 5 p2 s1 on [16][64][64] (reset / timeout tests)", FX1_NPU_JOB_ARENA_SMALL, 0x100000,
          0x100000, 2000, 0x5EED0106u);
    uint32_t prev = t8(f, "in", FX1_NPU_JOB_INPUT, 16, 64, 64);
    rand8(f, prev, -128, 127);
    for (int k = 0; k < 8; ++k) {
        static const char* const names[] = {"p1", "p2", "p3", "p4", "p5", "p6", "p7", "out"};
        const uint32_t o = t8(f, names[k], k == 7 ? FX1_NPU_JOB_OUTPUT : FX1_NPU_JOB_SCRATCH, 16, 64, 64);
        pool(f, FX1_NPU_JOB_MAX_POOL, prev, o, 5, 2, 1, 0, 0);
        prev = o;
    }
}

/* Long NPU execution for little hart work (plan N4 coexistence: ISP, SYS_DMA
 * and Ethernet traffic while the NPU's DMA runs): MAX_POOL with a 31 x 31
 * window, 8 chained, in the large arena. */
static void fx_busy(fixture* f)
{
    begin(f, "busy", "8 x MAX_POOL 31 p15 s1 on [16][64][64], large arena (coexistence: long NPU work)",
          FX1_NPU_JOB_ARENA_LARGE, 0x400000, 0x100000, 10000, 0x5EED0107u);
    uint32_t prev = t8(f, "in", FX1_NPU_JOB_INPUT, 16, 64, 64);
    rand8(f, prev, -128, 127);
    static const char* const names[] = {"q1", "q2", "q3", "q4", "q5", "q6", "q7", "out"};
    for (int k = 0; k < 8; ++k) {
        const uint32_t o = t8(f, names[k], k == 7 ? FX1_NPU_JOB_OUTPUT : FX1_NPU_JOB_SCRATCH, 16, 64, 64);
        pool(f, FX1_NPU_JOB_MAX_POOL, prev, o, 31, 15, 1, 0, 0);
        prev = o;
    }
}

/* Negative: a tensor past the window (the image CRC is right). */
static void fx_bad_bounds(fixture* f)
{
    fx_gemm(f);
    f->name = "bad_bounds";
    snprintf(f->h.name, sizeof f->h.name, "%s", f->name);
    f->desc = "negative: tensor 2 placed past the window: rejected before the NPU is touched";
    f->expect_status = FX1_NPU_JOB_BAD_JOB;
    f->expect_detail = (uint32_t)FX1_NPU_JOB_E_TENSOR_BOUNDS << 16 | 2u;
}

/* Negative: window.bin with one input byte changed after the CRC was taken. */
static void fx_bad_input(fixture* f)
{
    fx_gemm(f);
    f->name = "bad_input";
    snprintf(f->h.name, sizeof f->h.name, "%s", f->name);
    f->desc = "negative: one byte of the loaded input differs from its CRC: INPUT_CRC, the NPU not started";
    f->expect_status = FX1_NPU_JOB_INPUT_CRC;
    f->expect_detail = 0;
    f->corrupt_load = 1;
}

typedef void (*fx_fn)(fixture*);
static const struct {
    const char* name;
    fx_fn fn;
} fixtures[] = {{"chain", fx_chain}, {"gemm", fx_gemm},       {"dfc", fx_dfc},           {"mixed", fx_mixed},
                {"large", fx_large}, {"long", fx_long},       {"busy", fx_busy},       {"bad_bounds", fx_bad_bounds}, {"bad_input", fx_bad_input}};
#define NFIX (sizeof fixtures / sizeof fixtures[0])

/* ---- ref_program.py program directory ---------------------------------------------- */
static int xcheck(const char* name, const char* dir)
{
    static fixture f;
    unsigned i = 0;
    while (i < NFIX && strcmp(fixtures[i].name, name)) ++i;
    if (i == NFIX) die("unknown fixture");
    fixtures[i].fn(&f);
    if (f.expect_status != FX1_NPU_JOB_OK) die("negative fixtures have no reference program");
    uint8_t* ref = prepare(&f, 0); /* the GEMM bank is not a ref_program instruction */
    make_image(&f);
    if (strlen(dir) > 400) die("output path too long");
    char p[512];
    snprintf(p, sizeof p, "%.400s/mmio.txt", dir);
    FILE* m = create_new(p);
    fprintf(m, "# npu_job_gen --xcheck %s: window base 0 (GEMM-bank ops left out)\n", name);
    unsigned n = 0;
    for (uint32_t k = 0; k < f.h.n_ops; ++k) {
        const fx1_npu_job_op* o = &f.op[k];
        if (o->kind == FX1_NPU_JOB_GEMM_BANK) continue;
        /* neo ops as the lane-A instruction they decode to (as fx1_fw_npu_oracle_xcheck does). */
        fx1_npu_job_op lane = *o;
        if (o->kind == FX1_NPU_JOB_NEO_MAX_POOL) lane.kind = FX1_NPU_JOB_MAX_POOL;
        if (o->kind == FX1_NPU_JOB_NEO_RESIDUAL) lane.kind = FX1_NPU_JOB_ADD;
        f.op[k] = lane;
        make_image(&f);
        fx1_npu_gemm_t g;
        fx1_npu_instr_t in;
        fx1_npu_neo_t ne;
        if (!fx1_npu_job_build(image, k, 0, &g, &in, &ne)) die("build");
        f.op[k] = *o;
        fprintf(m, "# [%u] op %u %s\n", n++, k, kind_names[o->kind]);
        for (uint32_t w = 0; w < in.n; ++w) fprintf(m, "W %08x %x\n", in.addr[w], in.val[w]);
        fprintf(m, "W %08x %x\n", FX1_NPU_M_PUSH_A, in.push);
    }
    fclose(m);
    write_file(dir, "dram_init.bin", f.init, f.h.window_size);
    write_file(dir, "c_oracle.bin", ref, f.h.window_size);
    printf("npu_job_gen --xcheck %s: %u instructions, window %u bytes, in %s\n", name, n, f.h.window_size, dir);
    return 0;
}

/* Every check before the first write: <out> new, empty or marked; the marker
 * a regular file; every fixture path absent or a directory (not a link), and in
 * it every managed name a regular file (not a link). */
static void claim_root(const char* root)
{
    if (strlen(root) > 400) die("output path too long");
    struct stat st;
    if (stat(root, &st) != 0) {
        if (errno != ENOENT) refuse(root, strerror(errno));
    } else {
        if (!S_ISDIR(st.st_mode)) refuse(root, "exists and is not a directory");
        DIR* d = opendir(root);
        if (!d) refuse(root, strerror(errno));
        int entries = 0, marked = 0;
        for (struct dirent* e; (e = readdir(d));) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            ++entries;
            if (!strcmp(e->d_name, MARKER)) marked = 1;
        }
        closedir(d);
        if (entries && !marked)
            refuse(root, "not empty and not made by npu_job_gen (no " MARKER "); choose another directory");
        char m[512];
        snprintf(m, sizeof m, "%.400s/" MARKER, root);
        if (marked && (lstat(m, &st) != 0 || !S_ISREG(st.st_mode)))
            refuse(m, "a symbolic link or not a regular file; npu_job_gen does not write through links");
        for (unsigned i = 0; i < NFIX; ++i) {
            char p[512];
            snprintf(p, sizeof p, "%.400s/%.40s", root, fixtures[i].name);
            if (lstat(p, &st) != 0) continue;
            if (!S_ISDIR(st.st_mode)) refuse(p, "exists and is not a directory (a symbolic link is not followed)");
            DIR* fx = opendir(p);
            if (!fx) refuse(p, strerror(errno));
            for (struct dirent* e; (e = readdir(fx));) {
                if (!managed(e->d_name)) continue;
                char q[1100];
                snprintf(q, sizeof q, "%.500s/%.400s", p, e->d_name);
                if (lstat(q, &st) != 0 || !S_ISREG(st.st_mode)) {
                    closedir(fx);
                    refuse(q, "a symbolic link or not a regular file; npu_job_gen does not write through links");
                }
            }
            closedir(fx);
        }
    }
    make_dirs(root);
    char m[512];
    snprintf(m, sizeof m, "%.400s/" MARKER, root);
    FILE* fp = create_new(m);
    if (fputs("npu_job_gen output directory. Replaced by each run: this file and, in every fixture\n"
              "directory it names, job.bin, window.bin, fixture.json, golden_<n>.bin. Nothing else.\n",
              fp) < 0 || fclose(fp))
        die(m);
}

int main(int argc, char** argv)
{
    if (argc == 2 && !strcmp(argv[1], "--list")) {
        for (unsigned i = 0; i < NFIX; ++i) printf("%s\n", fixtures[i].name);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "--xcheck")) return xcheck(argv[2], argv[3]);
    if (argc != 2) {
        fprintf(stderr, "usage: npu_job_gen <out> | --xcheck <name> <dir> | --list\n");
        return 2;
    }
    static fixture f;
    claim_root(argv[1]);
    for (unsigned i = 0; i < NFIX; ++i) {
        fixtures[i].fn(&f);
        emit(&f, argv[1]);
    }
    return 0;
}
