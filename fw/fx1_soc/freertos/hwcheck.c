#include "hwcheck.h"

#include "fx1_fw.h"
#include "dma/fx1_dma_regs.h"
#include "fx1_isp/fx1_isp_csr.h"
#ifdef FX1_FW_HAS_CAMERA
#include "fx1_ccs_sensor.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_i2c.h"
#endif

#define UNMAPPED       0x60000000u
#define DDR_WINDOW     (FX1_DDR_BASE + FX1_DDR_SIZE - 0x1000u) /* last 4 KiB of DDR */
#define UART_IFLS      0x034u
#define UART_IFLS_RST  0x12u
#define UART_LCR_H     0x02Cu
#define UART_CR        0x030u
#define UART_CR_RESET  0x0300u /* TXE | RXE */
#define UART_IBRD      0x024u

hwcheck_result hwcheck_report;

static hwcheck_ip* cur;
static volatile uint32_t faults, fault_cause, fault_addr;

/* Faults are part of the contract under test: count them and skip the access. */
static uint32_t trap(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    if (mcause == 1u || mcause == 5u || mcause == 7u) {
        ++faults;
        fault_cause = mcause;
        fault_addr = mtval;
        const uint16_t insn = *(volatile uint16_t*)mepc;
        return mepc + ((insn & 3u) == 3u ? 4u : 2u);
    }
    fx1_log_hex("hwcheck: unexpected trap, mcause", mcause);
    fx1_fail(0x0E1);
}

static void begin(const char* name)
{
    cur = &hwcheck_report.ip[hwcheck_report.ips++];
    cur->name = name;
}

static void expect(int ok, const char* what, uint32_t addr, uint32_t got, uint32_t expected)
{
    ++cur->checks;
    ++hwcheck_report.total_checks;
#ifdef HWCHECK_INJECT_FAILURE /* negative test of the boot gate: ISP identity is "wrong" */
    if (cur->name[0] == 'I' && cur->name[1] == 'S' && cur->checks == 1) ok = 0;
#endif
    if (ok) return;
    if (!cur->failures) {
        cur->what = what;
        cur->addr = addr;
        cur->got = got;
        cur->expected = expected;
    }
    ++cur->failures;
    ++hwcheck_report.total_failures;
}

static void expect_eq(const char* what, uint32_t addr, uint32_t got, uint32_t expected)
{
    expect(got == expected, what, addr, got, expected);
}

/* Write `pattern & mask`, read back, restore the original value. */
static void rw(const char* what, uint32_t addr, uint32_t pattern, uint32_t mask)
{
    const uint32_t saved = fx1_read32(addr);
    fx1_write32(addr, pattern & mask);
    const uint32_t got = fx1_read32(addr) & mask;
    fx1_write32(addr, saved);
    expect_eq(what, addr, got, pattern & mask);
    expect_eq("restored", addr, fx1_read32(addr) & mask, saved & mask);
}

/* A byte load must raise a load access fault (32-bit-only registers). */
static void byte_load_faults(const char* what, uint32_t addr)
{
    const uint32_t before = faults;
    volatile uint8_t sink = *(volatile uint8_t*)addr;
    (void)sink;
    expect(faults == before + 1u && fault_cause == 5u && fault_addr == addr, what, addr, fault_cause, 5u);
}

/* Not offset 0: the BootROM sits at address 0, and GCC turns an access through
 * a constant null pointer into ebreak (undefined behaviour in C). */
#define ROM_PROBE (FX1_BOOTROM_BASE + 0x100u)

static void check_bootrom(void)
{
    begin("BootROM");
    const uint32_t word = fx1_read32(ROM_PROBE);
    const uint32_t before = faults;
    fx1_write32(ROM_PROBE, ~word);
    expect(faults == before + 1u && fault_cause == 7u, "store to ROM faults", ROM_PROBE, fault_cause, 7u);
    expect_eq("ROM unchanged", ROM_PROBE, fx1_read32(ROM_PROBE), word);
}

static void check_ddr(void)
{
    begin("DDR");
    static const uint32_t patterns[] = {0x00000000u, 0xFFFFFFFFu, 0xAAAAAAAAu, 0x55555555u};
    volatile uint32_t* w = (volatile uint32_t*)DDR_WINDOW;
    const uint32_t words = 0x1000u / 4u;
    for (uint32_t p = 0; p < sizeof(patterns) / sizeof(patterns[0]); ++p) {
        uint32_t bad = 0, first = 0;
        for (uint32_t i = 0; i < words; ++i) w[i] = patterns[p];
        for (uint32_t i = 0; i < words; ++i)
            if (w[i] != patterns[p] && !bad++) first = i;
        expect(!bad, "pattern", DDR_WINDOW + 4u * first, w[first], patterns[p]);
    }
    for (uint32_t i = 0; i < words; ++i) w[i] = DDR_WINDOW + 4u * i;
    uint32_t bad = 0;
    for (uint32_t i = 0; i < words; ++i) bad += w[i] != DDR_WINDOW + 4u * i;
    expect(!bad, "address in address", DDR_WINDOW, bad, 0);
    for (uint32_t b = 0; b < 32; ++b) { /* walking one */
        w[0] = 1u << b;
        if (w[0] != (1u << b)) {
            expect(0, "walking one", DDR_WINDOW, w[0], 1u << b);
            break;
        }
    }
    w[1] = 0x11223344u;
    *((volatile uint8_t*)&w[1] + 1) = 0xAB;
    *((volatile uint16_t*)&w[1] + 1) = 0xCDEF;
    expect_eq("byte/halfword merge", DDR_WINDOW + 4u, w[1], 0xCDEFAB44u);
}

static void check_unmapped(void)
{
    begin("Bus");
    const uint32_t before = faults;
    volatile uint32_t sink = fx1_read32(UNMAPPED);
    (void)sink;
    expect(faults == before + 1u && fault_cause == 5u, "unmapped load faults", UNMAPPED, fault_cause, 5u);
    fx1_write32(UNMAPPED, 0);
    expect(faults == before + 2u && fault_cause == 7u, "unmapped store faults", UNMAPPED, fault_cause, 7u);
}

static void check_clint(void)
{
    begin("CLINT");
    const uint32_t t0 = fx1_read32(FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF);
    for (volatile int i = 0; i < 50; ++i) {
    }
    const uint32_t t1 = fx1_read32(FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF);
    expect(t1 != t0, "mtime advances", FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF, t1, t0 + 1u);
    const uint32_t cmp1 = FX1_CLINT_BASE + FX1_CLINT_MTIMECMP_OFF + 8u; /* hart 1's own, MTIE is off */
    rw("mtimecmp[1] low", cmp1, 0x12345678u, 0xFFFFFFFFu);
    rw("mtimecmp[1] high", cmp1 + 4u, 0x0000ABCDu, 0xFFFFFFFFu);
}

static void check_plic(void)
{
    begin("PLIC");
    const uint32_t prio7 = FX1_PLIC_BASE + FX1_PLIC_PRIORITY_OFF + 4u * 7u;
    rw("priority[7]", prio7, 5u, 7u);
    const uint32_t enable1 = FX1_PLIC_BASE + FX1_PLIC_ENABLE_OFF + 0x80u * FX1_PLIC_CONTEXT_HART_M(1);
    rw("enable ctx1 bit 7", enable1, 1u << 7, 1u << 7);
    const uint32_t threshold1 = FX1_PLIC_BASE + FX1_PLIC_CONTEXT_OFF + 0x1000u * FX1_PLIC_CONTEXT_HART_M(1);
    rw("threshold ctx1", threshold1, 6u, 7u);
    expect_eq("claim ctx1 with nothing pending", threshold1 + 4u, fx1_read32(threshold1 + 4u), 0);
}

static void check_uart(void)
{
    begin("UART");
    expect_eq("IFLS reset value", FX1_UART_BASE + UART_IFLS, fx1_read32(FX1_UART_BASE + UART_IFLS) & 0x3Fu,
              UART_IFLS_RST);
    rw("IFLS", FX1_UART_BASE + UART_IFLS, 0x2Du, 0x3Fu);
    expect_eq("CR reset value", FX1_UART_BASE + UART_CR, fx1_read32(FX1_UART_BASE + UART_CR), UART_CR_RESET);
    rw("LCR_H", FX1_UART_BASE + UART_LCR_H, 0x70u, 0xFFu);
    rw("IBRD", FX1_UART_BASE + UART_IBRD, 0x1Bu, 0xFFFFu);
    byte_load_faults("byte access faults", FX1_UART_BASE + UART_IFLS);
}

static void check_dma(void)
{
    begin("SYS_DMA");
    expect_eq("capability (identity)", FX1_SYS_DMA_CSR_BASE + FX1_DMA_CORE_CAPABILITY_STATUS0,
              fx1_read32(FX1_SYS_DMA_CSR_BASE + FX1_DMA_CORE_CAPABILITY_STATUS0), FX1_DMA_CAPABILITY0);
    expect_eq("no channel interrupt", FX1_SYS_DMA_CSR_BASE + FX1_DMA_CORE_STATUS,
              fx1_read32(FX1_SYS_DMA_CSR_BASE + FX1_DMA_CORE_STATUS), 0);
    const uint32_t ch7 = FX1_SYS_DMA_CSR_BASE + FX1_DMA_CH(7, 0);
    rw("ch7 CMD_READ_ADDR", ch7 + FX1_DMA_CH_CMD_READ_ADDR, 0x9ABCDEF0u, 0xFFFFFFFFu);
    rw("ch7 CMD_WRITE_ADDR", ch7 + FX1_DMA_CH_CMD_WRITE_ADDR, 0x13579BDFu, 0xFFFFFFFFu);
    byte_load_faults("byte access faults", FX1_SYS_DMA_CSR_BASE + FX1_DMA_CORE_STATUS);
}

static void check_isp(void)
{
    begin("ISP");
    expect_eq("COMMON_VER_ID", FX1_ISP_CSR_BASE + FX1_ISP_COMMON_VER_ID_OFFSET,
              fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_VER_ID_OFFSET), FX1_ISP_COMMON_VER_ID_RESET);
    expect_eq("DMA_ERR clear", FX1_ISP_CSR_BASE + FX1_ISP_DMA_ERR_OFFSET,
              fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_ERR_OFFSET), 0);
    rw("ODMA_Y_ADDR[3] low", FX1_ISP_CSR_BASE + FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * 3u, 0x8ABCD000u,
       0xFFFFFFF0u);
    rw("IDMA_BUF_ADDR[3] low", FX1_ISP_CSR_BASE + FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * 3u, 0x81234560u,
       0xFFFFFFF0u);
}

#ifdef FX1_FW_HAS_CAMERA
#define I2C_CTRL   0x10u
#define I2C_STATUS 0x14u
#define I2C_STATUS_HOSTIDLE (1u << 3)

static void check_camera(void)
{
    begin("CSI-2 RX");
    const uint32_t base = FX1_MIPI_CSR_BASE;
    const uint32_t before = faults;
    const uint32_t ver = fx1_read32(base + FX1_CSI2_IP_VERSION);
    const int present = faults == before;
    if (!present) {
        cur->skipped = "absent (fx1_soc without --camera)";
    } else {
        expect_eq("IP_VERSION", base + FX1_CSI2_IP_VERSION, ver, FX1_CSI2_IP_VERSION_RESET);
        expect_eq("CORE_CONFIG reset value", base + FX1_CSI2_CORE_CONFIG, fx1_read32(base + FX1_CSI2_CORE_CONFIG),
                  FX1_CSI2_CORE_CONFIG_RESET);
        expect_eq("DMA_CONTROL reset value", base + FX1_CSI2_DMA_CONTROL, fx1_read32(base + FX1_CSI2_DMA_CONTROL),
                  FX1_CSI2_DMA_CONTROL_RESET);
        rw("EXPECTED_RESOLUTION", base + FX1_CSI2_EXPECTED_RESOLUTION, 0x02D00500u, 0xFFFFFFFFu);
        rw("DMA_BUFFER0_ADDR", base + FX1_CSI2_DMA_BUFFER0_ADDR, 0x8ABCDE00u, 0xFFFFFFFFu);
        rw("DMA_BUFFER1_ADDR", base + FX1_CSI2_DMA_BUFFER1_ADDR, 0x81234500u, 0xFFFFFFFFu);
    }
    begin(present ? "I2C0 + sensor" : "I2C0");
    const uint32_t i2c = FX1_I2C0_BASE;
    const uint32_t st = fx1_read32(i2c + I2C_STATUS);
    expect((st & I2C_STATUS_HOSTIDLE) != 0, "STATUS host idle", i2c + I2C_STATUS, st, I2C_STATUS_HOSTIDLE);
    rw("CTRL enablehost", i2c + I2C_CTRL, 1u, 1u);
    if (present) { /* the sensor exists only with --camera */
        const uint32_t ctrl = fx1_read32(i2c + I2C_CTRL);
        fx1_i2c_t bus;
        fx1_ccs s;
        uint16_t id = 0;
        const int ok = fx1_i2c_open(&bus, i2c, 0, 0, 0) == FX1_I2C_OK && fx1_ccs_probe(&s, &bus, FX1_CCS_ADDR, 2000) == FX1_CCS_OK &&
                       fx1_ccs_read16(&s, 0x0000u, &id) == FX1_CCS_OK;
        expect(ok && id == FX1_CCS_MODEL_ID, "sensor model_id over I2C0", FX1_CCS_ADDR, id, FX1_CCS_MODEL_ID);
        fx1_write32(i2c + I2C_CTRL, ctrl); /* as found */
    }
}
#endif

void hwcheck_run(void)
{
    fx1_set_trap_handler(trap);
    check_bootrom();
    check_ddr();
    check_unmapped();
    check_clint();
    check_plic();
    check_uart();
    check_dma();
    check_isp();
#ifdef FX1_FW_HAS_CAMERA
    check_camera();
#endif
    fx1_set_trap_handler(0);
    fx1_release(); /* the report before the flag */
    hwcheck_report.done = 1;
}

void hwcheck_print(void)
{
    const hwcheck_result* r = &hwcheck_report;
    fx1_log("hwcheck (hart 1, at boot):");
    for (uint32_t i = 0; i < r->ips; ++i) {
        const hwcheck_ip* ip = &r->ip[i];
        if (ip->skipped) {
            fx1_puts("  --   ");
            fx1_puts(ip->name);
            fx1_puts(": ");
            fx1_puts(ip->skipped);
            fx1_puts(", skipped\n");
            continue;
        }
        fx1_puts(ip->failures ? "  FAIL " : "  ok   ");
        fx1_puts(ip->name);
        fx1_puts(", checks ");
        fx1_putdec(ip->checks);
        if (ip->failures) {
            fx1_puts(", failures ");
            fx1_putdec(ip->failures);
            fx1_puts(", first: ");
            fx1_puts(ip->what);
            fx1_puts(" @");
            fx1_puthex(ip->addr);
            fx1_puts(" got ");
            fx1_puthex(ip->got);
            fx1_puts(" expected ");
            fx1_puthex(ip->expected);
        }
        fx1_puts("\n");
    }
    fx1_puts("  total: ");
    fx1_putdec(r->total_checks);
    fx1_puts(" checks, ");
    fx1_putdec(r->total_failures);
    fx1_puts(r->total_failures ? " failures -> FAIL\n" : " failures -> PASS\n");
}
