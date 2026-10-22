/*
 * The UART CLI task: prompt, line editing (backspace, CR/LF), command table.
 * System, memory/register and test commands live here; the device commands
 * are in cmd_isp.c, cmd_dma.c and cmd_fpu.c.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "fx1_fw.h"
#include "dma/fx1_dma_regs.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "hwcheck.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

#define PROMPT   "root@fx1:~# "
#define LINE_MAX 96
#define ARGS_MAX 8

volatile uint32_t app_failures;

/* ---- output -------------------------------------------------------------- */
void out(const char* s) { fx1_puts(s); }
void out_hex(uint32_t v) { fx1_puthex(v); /* prints the 0x prefix itself */ }
void out_dec(uint32_t v) { fx1_putdec(v); }
void out_line(const char* s)
{
    fx1_puts(s);
    fx1_puts("\n");
}
int report(const char* name, int ok)
{
    out("  ");
    out(name);
    out_line(ok ? ": PASS" : ": FAIL");
    if (!ok) ++app_failures;
    return ok;
}

int parse_u32(const char* s, uint32_t* v)
{
    uint32_t base = 10, x = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (!*s) return 0;
    for (; *s; ++s) {
        uint32_t d;
        if (*s >= '0' && *s <= '9') d = (uint32_t)(*s - '0');
        else if (base == 16 && *s >= 'a' && *s <= 'f') d = (uint32_t)(*s - 'a' + 10);
        else if (base == 16 && *s >= 'A' && *s <= 'F') d = (uint32_t)(*s - 'A' + 10);
        else return 0;
        if (x > (UINT32_MAX - d) / base) return 0; /* x * base + d would exceed 32 bits */
        x = x * base + d;
    }
    *v = x;
    return 1;
}

/* ---- address checks for md/mw --------------------------------------------- */
typedef struct {
    uint32_t base, size;
    int writable;
    const char* name;
} region;
static const region regions[] = {
    {FX1_BOOTROM_BASE, FX1_BOOTROM_SIZE, 0, "BootROM"},
    {FX1_CLINT_BASE, 0x10000u, 1, "CLINT"},
    {FX1_PLIC_BASE, FX1_PLIC_SIZE, 1, "PLIC"},
    {FX1_UART_BASE, 0x1000u, 1, "UART"},
    {FX1_SYS_DMA_CSR_BASE, 0x2000u, 1, "SYS_DMA"},
    {FX1_ISP_CSR_BASE, 0x10000u, 1, "ISP"},
    {FX1_I2C0_BASE, 0x1000u, 1, "I2C0"},
#ifdef FX1_FW_HAS_CAMERA
    {FX1_MIPI_CSR_BASE, 0x1000u, 1, "MIPI_CSR"}, /* faults on the bus without --camera */
#endif
    {FX1_DDR_BASE, FX1_DDR_SIZE, 1, "DDR"},
};
/* The region holding all of [addr, addr + bytes), or 0. Written so that no
 * term can wrap: offset < size, then bytes <= size - offset. */
static const region* find_region(uint32_t addr, uint32_t bytes)
{
    for (unsigned i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        const uint32_t offset = addr - regions[i].base;
        if (addr >= regions[i].base && offset < regions[i].size && bytes <= regions[i].size - offset)
            return &regions[i];
    }
    return 0;
}

static void bus_error(const char* cmd, uint32_t addr)
{
    out(cmd);
    out(": bus error at ");
    out_hex(addr);
    out_line("");
}

/* ---- commands --------------------------------------------------------------- */
typedef int (*cmd_fn)(int argc, char** argv);
typedef struct {
    const char* name;
    cmd_fn fn;
    const char* usage;
} command;

static int cmd_help(int argc, char** argv);

static int cmd_version(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    out_line(APP_VERSION);
    out("platform: FX1 SoC VP, 2 harts, ISP COMMON_VER_ID ");
    out_hex(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_VER_ID_OFFSET));
    out(", SYS_DMA capability ");
    out_hex(fx1_read32(FX1_SYS_DMA_CSR_BASE + FX1_DMA_CORE_CAPABILITY_STATUS0));
    out_line("");
    return 0;
}

static int cmd_uptime(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    out("uptime ");
    out_dec((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    out(" ms, mtime ");
    out_dec((uint32_t)fx1_mtime());
    out_line(" ticks (20 MHz)");
    return 0;
}

static int cmd_tasks(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    static TaskStatus_t st[8];
    static const char* const state[] = {"run", "ready", "block", "susp", "del", "inv"};
    const UBaseType_t n = uxTaskGetSystemState(st, 8, 0);
    out_line("  name         state prio stack-free(words)");
    for (UBaseType_t i = 0; i < n; ++i) {
        out("  ");
        out(st[i].pcTaskName);
        for (size_t k = strlen(st[i].pcTaskName); k < 13; ++k) out(" ");
        out(state[st[i].eCurrentState < 5 ? st[i].eCurrentState : 5]);
        out("  ");
        out_dec((uint32_t)st[i].uxCurrentPriority);
        out("    ");
        out_dec((uint32_t)st[i].usStackHighWaterMark);
        out_line("");
    }
    return 0;
}

static int cmd_heap(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    out("heap free ");
    out_dec((uint32_t)xPortGetFreeHeapSize());
    out(" bytes, minimum ever ");
    out_dec((uint32_t)xPortGetMinimumEverFreeHeapSize());
    out(" of ");
    out_dec((uint32_t)configTOTAL_HEAP_SIZE);
    out_line("");
    return 0;
}

static int cmd_hwcheck(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    hwcheck_print();
    return 0;
}

static int cmd_md(int argc, char** argv)
{
    uint32_t addr, words = 4;
    if (argc < 2 || !parse_u32(argv[1], &addr) || (argc > 2 && !parse_u32(argv[2], &words)) || words == 0 ||
        words > 64 || (addr & 3u))
        return 1;
    if (!find_region(addr, words * 4u)) {
        out_line("md: address range not mapped");
        return 0;
    }
    for (uint32_t i = 0; i < words; ++i) {
        uint32_t value;
        if (fx1_probe_read32(addr + 4u * i, &value)) {
            if (i) out_line("");
            bus_error("md", addr + 4u * i);
            return 0;
        }
        if (i % 4u == 0) {
            if (i) out_line("");
            out_hex(addr + 4u * i);
            out(":");
        }
        out(" ");
        out_hex(value);
    }
    out_line("");
    return 0;
}

static int cmd_mw(int argc, char** argv)
{
    uint32_t addr, value;
    if (argc != 3 || !parse_u32(argv[1], &addr) || !parse_u32(argv[2], &value) || (addr & 3u)) return 1;
    const region* r = find_region(addr, 4);
    if (!r || !r->writable) {
        out_line("mw: address not mapped or read-only");
        return 0;
    }
    uint32_t back;
    if (fx1_probe_write32(addr, value) || fx1_probe_read32(addr, &back)) {
        bus_error("mw", addr);
        return 0;
    }
    out_hex(addr);
    out(" <- ");
    out_hex(value);
    out(", reads back ");
    out_hex(back);
    out_line("");
    return 0;
}

typedef struct {
    const char* name;
    uint32_t addr;
} reg;
static void dump(const reg* r, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) {
        out("  ");
        out(r[i].name);
        for (size_t k = strlen(r[i].name); k < 22; ++k) out(" ");
        out_hex(r[i].addr);
        out("  ");
        uint32_t value;
        if (fx1_probe_read32(r[i].addr, &value)) out_line("bus error");
        else {
            out_hex(value);
            out_line("");
        }
    }
}

static int cmd_regdump(int argc, char** argv)
{
#define ISPR(n) {#n, FX1_ISP_CSR_BASE + FX1_ISP_##n##_OFFSET}
#define DMAR(n) {#n, FX1_SYS_DMA_CSR_BASE + FX1_DMA_##n}
    static const reg isp[] = {ISPR(COMMON_VER_ID), ISPR(COMMON_CTRL), ISPR(COMMON_STATUS), ISPR(DMA_CTRL),
                              ISPR(DMA_STAT), ISPR(DMA_ERR), ISPR(IDMA_FRAME_COUNT), ISPR(ODMA_FRAME_COUNT),
                              ISPR(RESIZER_OUT_W), ISPR(RESIZER_OUT_H), ISPR(AEC_FRAME_ID)};
    static const reg dma[] = {DMAR(CORE_CAPABILITY_STATUS0), DMAR(CORE_STATUS), DMAR(CORE_IDLE_STATUS),
                              {"CH0_ACTIVE_STATUS", FX1_SYS_DMA_CSR_BASE + FX1_DMA_CH(0, FX1_DMA_CH_ACTIVE_STATUS)},
                              {"CH0_TRANSFER_COUNT", FX1_SYS_DMA_CSR_BASE + FX1_DMA_CH(0, FX1_DMA_CH_TRANSFER_COUNT)}};
    static const reg plic[] = {{"PENDING", FX1_PLIC_BASE + FX1_PLIC_PENDING_OFF},
                               {"ENABLE_CTX0", FX1_PLIC_BASE + FX1_PLIC_ENABLE_OFF},
                               {"THRESHOLD_CTX0", FX1_PLIC_BASE + FX1_PLIC_CONTEXT_OFF}};
    static const reg clint[] = {{"MSIP0", FX1_CLINT_BASE + FX1_CLINT_MSIP_OFF},
                                {"MSIP1", FX1_CLINT_BASE + FX1_CLINT_MSIP_OFF + 4u},
                                {"MTIMECMP0_LO", FX1_CLINT_BASE + FX1_CLINT_MTIMECMP_OFF},
                                {"MTIME_LO", FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF},
                                {"MTIME_HI", FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF + 4u}};
    static const reg uart[] = {{"FR", FX1_UART_BASE + 0x018u}, {"IFLS", FX1_UART_BASE + 0x034u},
                               {"IMSC", FX1_UART_BASE + 0x038u}, {"RIS", FX1_UART_BASE + 0x03Cu}};
#define DUMP(t) dump(t, sizeof(t) / sizeof(t[0]))
    if (argc != 2) return 1;
    if (!strcmp(argv[1], "isp")) DUMP(isp);
    else if (!strcmp(argv[1], "dma")) DUMP(dma);
    else if (!strcmp(argv[1], "plic")) DUMP(plic);
    else if (!strcmp(argv[1], "clint")) DUMP(clint);
    else if (!strcmp(argv[1], "uart")) DUMP(uart);
    else return 1;
    return 0;
}

static int cmd_selftest(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    static char* fpu[] = {"fpu", "test"};
    static char* dma[] = {"dma", "test"};
    static char* isp[] = {"isp", "selftest"};
    const uint32_t before = app_failures;
    out_line("selftest: hwcheck report, FPU context, SYS_DMA, ISP");
    report("hwcheck (hart 1 at boot)", hwcheck_report.done && hwcheck_report.total_failures == 0);
    cmd_fpu(2, fpu);
    cmd_dma(2, dma);
    cmd_isp(2, isp);
    out("selftest: ");
    out_line(app_failures == before ? "PASS" : "FAIL");
    return 0;
}

static int cmd_poweroff(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    out("poweroff: ");
    out_dec(app_failures);
    out_line(" failed check(s)");
    if (app_failures) fx1_fail(0x400u + app_failures); /* the simulator exits with FAIL */
    fx1_pass();
}

static const command commands[] = {
    {"help", cmd_help, "help                 this list"},
    {"version", cmd_version, "version              firmware and IP identity"},
    {"uptime", cmd_uptime, "uptime               time since the scheduler started"},
    {"tasks", cmd_tasks, "tasks                FreeRTOS tasks"},
    {"heap", cmd_heap, "heap                 FreeRTOS heap"},
    {"hwcheck", cmd_hwcheck, "hwcheck              hart 1 boot IP check report"},
    {"md", cmd_md, "md ADDR [WORDS]      read 32-bit words (mapped regions only)"},
    {"mw", cmd_mw, "mw ADDR VALUE        write one 32-bit word"},
    {"regdump", cmd_regdump, "regdump isp|dma|plic|clint|uart"},
    {"isp", cmd_isp, "isp selftest|run [auto]|stats  fixture; RAW job with optional adaptive WB/tone; statistics"},
    {"dma", cmd_dma, "dma test [BYTES]     SYS_DMA M2M copy with IRQ, verified"},
    {"fpu", cmd_fpu, "fpu test             two tasks interleaving FP work (context switch check)"},
#ifdef FX1_FW_HAS_CAMERA
    {"camera", cmd_camera,
     "camera probe|mode [W H BITS LANES]|source [sensor|tpg]|start|capture N [crc]|status|stop [US]|recover"},
#endif
    {"selftest", cmd_selftest, "selftest             hwcheck + fpu + dma + isp"},
    {"poweroff", cmd_poweroff, "poweroff             end the simulation: PASS if no check failed"},
};

static int cmd_help(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        out("  ");
        out_line(commands[i].usage);
    }
    return 0;
}

static void execute(char* line)
{
    char* argv[ARGS_MAX];
    int argc = 0;
    for (char* p = line; *p && argc < ARGS_MAX;) {
        while (*p == ' ') *p++ = 0;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') ++p;
    }
    if (!argc) return;
    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (!strcmp(argv[0], commands[i].name)) {
            if (commands[i].fn(argc, argv)) {
                out("usage: ");
                out_line(commands[i].usage);
            }
            return;
        }
    }
    out(argv[0]);
    out_line(": unknown command (try help)");
}

void cli_task(void* arg)
{
    (void)arg;
    char line[LINE_MAX];
    unsigned len = 0;
    int last_cr = 0;
    out(PROMPT);
    for (;;) {
        const int c = bsp_getc(portMAX_DELAY);
        if (c < 0) continue;
        if (c == '\n' && last_cr) { /* CR LF: one end of line */
            last_cr = 0;
            continue;
        }
        last_cr = c == '\r';
        if (c == '\r' || c == '\n') {
            out_line("");
            line[len] = 0;
            execute(line);
            len = 0;
            out(PROMPT);
        } else if (c == 0x08 || c == 0x7F) {
            if (len) {
                --len;
                out("\b \b");
            }
        } else if (c >= 0x20 && c < 0x7F && len < LINE_MAX - 1) {
            line[len++] = (char)c;
            fx1_putc((char)c); /* echo */
        }
    }
}
