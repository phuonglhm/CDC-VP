// Plan T07: clock, reset and handoff of the assembled platform through the
// CRM and SYSCSR models, driven by a test master on the (still unbound) DBG
// initiator port while both CPUs sleep in WFI loops:
//   - power-on reset timing, CRM startup (system, then Ibex), straps;
//   - every CRM clock gate / software reset bit against every bus target
//     (independent table from the CRM register names), and the functional
//     answer of a gated target (decision Q02-i);
//   - CVA6 release through CRM FCLK_SW[0] closes staging first, a local CVA6
//     reset does not reopen it (decision Q02-b);
//   - software, watchdog, debug and global resets: scope, new boot epoch,
//     recorded cause (decision Q05-b: platform record, not a CSR), DBG kept by
//     the debug reset only (Q05-e);
//   - the CPU clock follows the CRM (decision Q01-g), stops with the PLL; the
//     bus fabric follows the CRM bus clock (user decision after T07).

#include <cstdint>
#include <iostream>
#include <map>
#include <string>

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <fu3_bus/access.h>
#include <fu3_crm/registers.h>
#include <fu3_syscsr/registers.h>
#include <fu3_vp_abi.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

constexpr std::uint64_t kCrm = FU3_CRM_BASE, kSyscsr = FU3_SYSCSR_BASE;
constexpr std::uint32_t kWfiLoop[] = {0x10500073, 0xffdff06f};   // wfi; j .-4

// CRM register -> bus targets (CSR_CRM_fu3_v2 field names; UART3-UART7 on bits
// 25-29 per FU3_VP_ABI_1, Q05-d). IBEX_DBG follows the DBG reset instead of
// the system reset.
enum reg_index { gate0, gate1, rst0, rst1 };
struct control {
    reg_index reg;
    std::uint32_t bit;
    const char* target;
};
const control kControls[] = {
    {gate0, FU3_CRM_GATE0_ROM, "ROM"}, {gate0, FU3_CRM_GATE0_ISRAM, "CVA6_ISRAM"},
    {gate0, FU3_CRM_GATE0_ISRAM, "IBEX_ISRAM"}, {gate0, FU3_CRM_GATE0_DSRAM, "DSRAM"},
    {gate0, FU3_CRM_GATE0_AES_256, "AES"}, {gate0, FU3_CRM_GATE0_QSPI, "AP_QSPIC"},
    {gate0, FU3_CRM_GATE0_QSPI, "QSPI_XIP"}, {gate0, FU3_CRM_GATE0_HYPERBUS, "HYPERBUS"},
    {gate0, FU3_CRM_GATE0_DMA, "AP_DMA"}, {gate0, FU3_CRM_GATE0_DBG, "IBEX_DBG"},
    {rst0, FU3_CRM_RST0_ROM, "ROM"}, {rst0, FU3_CRM_RST0_ISRAM, "CVA6_ISRAM"},
    {rst0, FU3_CRM_RST0_ISRAM, "IBEX_ISRAM"}, {rst0, FU3_CRM_RST0_DSRAM, "DSRAM"},
    {rst0, FU3_CRM_RST0_AES_256, "AES"}, {rst0, FU3_CRM_RST0_QSPI, "AP_QSPIC"},
    {rst0, FU3_CRM_RST0_QSPI, "QSPI_XIP"}, {rst0, FU3_CRM_RST0_DMA, "AP_DMA"},
    {rst0, FU3_CRM_RST0_HYPERBUS, "HYPERBUS"},
};
struct peri {
    std::uint32_t bit;
    const char* target;
};
const peri kPeri[] = {   // CLK_GATE_1 and SW_RST_N_1 share the bit order
    {FU3_CRM_PERI_WDT0, "WDT0"}, {FU3_CRM_PERI_WDT1, "WDT1"}, {FU3_CRM_PERI_ADC, "ADC"},
    {FU3_CRM_PERI_RNG, "RNG"}, {FU3_CRM_PERI_SYSCSR, "SYSCSR"}, {FU3_CRM_PERI_PLIC, "AP_PLIC"},
    {FU3_CRM_PERI_GPIOC, "GPIOC"}, {FU3_CRM_PERI_GPIOB, "GPIOB"}, {FU3_CRM_PERI_GPIOA, "GPIOA"},
    {FU3_CRM_PERI_I2C, "I2C"}, {FU3_CRM_PERI_UART0, "UART0"}, {FU3_CRM_PERI_UART1, "UART1"},
    {FU3_CRM_PERI_UART2, "UART2"}, {FU3_CRM_PERI_SPI0, "SPI0"}, {FU3_CRM_PERI_SPI1, "SPI1"},
    {FU3_CRM_PERI_SPI2, "SPI2"}, {FU3_CRM_PERI_SPI3, "SPI3"}, {FU3_CRM_PERI_TIMER0, "TIMER0"},
    {FU3_CRM_PERI_TIMER1, "TIMER1"}, {FU3_CRM_PERI_TIMER2, "TIMER2"}, {FU3_CRM_PERI_TIMER3, "TIMER3"},
    {FU3_CRM_PERI_MAILBOX_IBEX, "MAILBOX_IBEX"}, {FU3_CRM_PERI_MAILBOX_CVA6, "MAILBOX_CVA6"},
    {FU3_CRM_PERI_AP_HBCTRL, "AP_HBCTRL"}, {FU3_CRM_PERI_UART3, "UART3"}, {FU3_CRM_PERI_UART4, "UART4"},
    {FU3_CRM_PERI_UART5, "UART5"}, {FU3_CRM_PERI_UART6, "UART6"}, {FU3_CRM_PERI_UART7, "UART7"},
};

bool controlled_by(const std::string& target, reg_index reg, std::uint32_t bit)
{
    for (const auto& c : kControls)
        if (c.reg == reg && c.bit == bit && target == c.target)
            return true;
    if (reg == gate1 || reg == rst1)
        for (const auto& p : kPeri)
            if (p.bit == bit && target == p.target)
                return true;
    return false;
}

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    tlm_utils::simple_initiator_socket<bench> master{"master"};
    unsigned checks = 0, failures = 0;
    bool finished = false;   // run() reached its end (not a starved or stopped simulation)

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n, const fu3_soc_options& options) : sc_module(n), top("fu3_soc", options)
    {
        master.bind(top.fabric().initiator("DBG"));
        // Both CPUs sleep: Ibex at its reset entry in ROM, CVA6 at its reset PC.
        for (unsigned i = 0; i < 8; ++i) {
            top.memory("ROM")->poke(0x80 + i, (kWfiLoop[i / 4] >> (8 * (i % 4))) & 0xff);
            top.memory("CVA6_ISRAM")->poke(i, (kWfiLoop[i / 4] >> (8 * (i % 4))) & 0xff);
        }
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what)
    {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL @" << sc_core::sc_time_stamp() << ": " << what << '\n';
        }
    }
    tlm::tlm_response_status access(bool write, std::uint64_t address, std::uint32_t& value)
    {
        unsigned char data[4];
        for (unsigned i = 0; i < 4; ++i)
            data[i] = (value >> (8 * i)) & 0xff;
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        fu3_bus::ScopedAccess scoped(tx, 4);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        master->b_transport(tx, delay);
        wait(delay);
        value = data[0] | data[1] << 8 | data[2] << 16 | std::uint32_t(data[3]) << 24;
        return tx.get_response_status();
    }
    void write(std::uint64_t address, std::uint32_t value)
    {
        check(access(true, address, value) == tlm::TLM_OK_RESPONSE, "write OK at " + std::to_string(address));
    }
    std::uint32_t read(std::uint64_t address)
    {
        std::uint32_t value = 0;
        check(access(false, address, value) == tlm::TLM_OK_RESPONSE, "read OK at " + std::to_string(address));
        return value;
    }
    void wait_ready()
    {
        for (unsigned i = 0; i < 1000 && !top.system_ready(); ++i)
            wait(50, sc_core::SC_NS);
        check(top.system_ready(), "system domain out of reset");
        wait(1500, sc_core::SC_NS);   // Ibex released by the CRM startup too
    }
    unsigned count(const std::string& event) const
    {
        unsigned n = 0;
        for (const auto& e : top.events())
            n += e.second == event;
        return n;
    }
    // Position of the first `event` in the platform event log (log size if absent).
    std::size_t first(const std::string& event) const
    {
        std::size_t i = 0;
        while (i < top.events().size() && top.events()[i].second != event)
            ++i;
        return i;
    }
    sc_core::sc_time last(const std::string& event) const
    {
        sc_core::sc_time t = sc_core::SC_ZERO_TIME;
        for (const auto& e : top.events())
            if (e.second == event)
                t = e.first;
        return t;
    }

    void power_on()
    {
        wait(50, sc_core::SC_NS);
        check(!top.system_ready() && !top.gate("DSRAM")->available(), "system held during PRSTN");
        check(top.cva6()->in_reset() && top.ibex()->in_reset(), "both CPUs held during PRSTN");
        wait_ready();
        const auto por = last("PRSTN released");
        check(por == sc_core::sc_time(100, sc_core::SC_NS), "PRSTN released after 100 ns");
        // CRM: PRSTN filter 100 ns (spec 3.2), readiness 255 OSC cycles = 12.75 us
        // (3.1.1), startup: system at once, Ibex 24 MRV clocks (1.2 us) later (4.1/4.2).
        const auto startup = por + sc_core::sc_time(100 + 12750, sc_core::SC_NS);
        check(top.events().size() > 2 && last("Ibex released") == startup + sc_core::sc_time(1200, sc_core::SC_NS),
              "Ibex released 100 ns + 12.75 us + 1.2 us after PRSTN");
        check(!top.ibex()->in_reset() && top.cva6()->in_reset(), "Ibex runs, CVA6 held");
        check(top.fabric().staging(), "staging open after power-on");
        check(top.reset_history().size() == 1 && top.reset_history()[0].causes == reset_por, "POR recorded");
        check(top.cpu_clock_period() == sc_core::sc_time(50, sc_core::SC_NS) &&
                  top.ibex()->clock_period() == top.cpu_clock_period() &&
                  top.cva6()->clock_period() == top.cpu_clock_period(),
              "both CPUs on the 20 MHz OSC after reset");
        // SYSCSR v1 STATUS is live; FU3_VP_ABI_1 BOOT_STATUS holds the straps
        // captured at the SYSCSR reset release (boot = 1, mode_0 = 0, mode_1 = 1).
        check((read(kSyscsr + FU3_SYSCSR_STATUS_OFFSET) & 0x1f) == 0x05, "SYSCSR STATUS: live straps");
        // board.debug_en = true from construction (die 6): live in BOOT_STATUS[8].
        check(read(kSyscsr + FU3_VP_BOOT_STATUS_OFFSET) ==
                  (FU3_VP_BOOT_STRAPS_VALID | FU3_VP_BOOT_BOOT | FU3_VP_BOOT_MODE_1 | FU3_VP_BOOT_DEBUG_EN),
              "ABI BOOT_STATUS: straps captured at reset release, valid; board DEBUG_EN high from power-on");
        top.set_debug_en(false);
        wait(10, sc_core::SC_NS);
        check(read(kSyscsr + FU3_VP_BOOT_STATUS_OFFSET) == (FU3_VP_BOOT_STRAPS_VALID | FU3_VP_BOOT_BOOT | FU3_VP_BOOT_MODE_1),
              "ABI BOOT_STATUS: DEBUG_EN low once the board releases it (live)");
        check(read(kSyscsr + FU3_VP_ABI_VERSION_OFFSET) == FU3_VP_ABI_VERSION_VALUE, "ABI version 1.0");
        check(read(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET) == FU3_VP_RESET_POR, "ABI RESET_CAUSE: POR after power-on");
    }

    void gates()
    {
        const std::uint64_t regs[4] = {kCrm + FU3_CRM_CLK_GATE_0_OFFSET, kCrm + FU3_CRM_CLK_GATE_1_OFFSET,
                                       kCrm + FU3_CRM_SW_RST_N_0_OFFSET, kCrm + FU3_CRM_SW_RST_N_1_OFFSET};
        const std::uint32_t fields[4] = {0x1ff, 0x3fff'ffff, 0xff, 0x3fff'ffff};   // gate1/rst1: bits 25-29 UART3-7 (ABI_1)
        for (unsigned r = 0; r < 4; ++r) {
            check(read(regs[r]) == fields[r], "CRM gate/reset register at its reset value");
            for (unsigned b = 0; b < 32; ++b) {
                const std::uint32_t bit = 1u << b;
                if (!(fields[r] & bit))
                    continue;
                write(regs[r], fields[r] & ~bit);
                wait(sc_core::SC_ZERO_TIME);
                for (const auto& i : top.instances()) {
                    const auto* g = top.gate(i.name);
                    if (!g)
                        continue;
                    const bool off = controlled_by(i.name, static_cast<reg_index>(r), bit);
                    check(g->available() == !off, i.name + (off ? " gated" : " not gated") + " by CRM register " +
                                                      std::to_string(r) + " bit " + std::to_string(b));
                }
                write(regs[r], fields[r]);
            }
        }

        // A gated target answers TLM_GENERIC_ERROR_RESPONSE without reaching the block;
        // the debug path still reaches it.
        write(kCrm + FU3_CRM_CLK_GATE_0_OFFSET, 0x1ff & ~FU3_CRM_GATE0_DSRAM);
        std::uint32_t v = 0;
        const auto refused = top.gate("DSRAM")->refused();
        check(access(false, 0x0100'0000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE && top.gate("DSRAM")->refused() == refused + 1,
              "gated DSRAM refuses a functional read");
        top.memory("DSRAM")->poke(0, 0x5a);
        unsigned char byte[4] = {};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_address(0x0100'0000);
        tx.set_data_ptr(byte);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        check(master->transport_dbg(tx) == 4 && byte[0] == 0x5a, "debug path passes the gate");
        write(kCrm + FU3_CRM_CLK_GATE_0_OFFSET, 0x1ff);
        check(access(false, 0x0100'0000, v) == tlm::TLM_OK_RESPONSE, "DSRAM answers again once ungated");

        // SW_RST_N_1.syscsr resets the SYSCSR registers; gated SYSCSR refuses.
        write(kSyscsr + FU3_SYSCSR_OSC_PAD_CTRL_OFFSET, 0x1);
        write(kCrm + FU3_CRM_SW_RST_N_1_OFFSET, 0x1ff'ffff & ~FU3_CRM_PERI_SYSCSR);
        check(access(false, kSyscsr + FU3_SYSCSR_OSC_PAD_CTRL_OFFSET, v) == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "SYSCSR held in reset refuses");
        write(kCrm + FU3_CRM_SW_RST_N_1_OFFSET, 0x1ff'ffff);
        check(read(kSyscsr + FU3_SYSCSR_OSC_PAD_CTRL_OFFSET) == 0x13, "SYSCSR registers reset by SW_RST_N_1.syscsr");
    }

    // Latency of a functional DSRAM read from the test master: bus stages only
    // (the memory itself answers at once), so it scales with the bus clock.
    sc_core::sc_time dsram_read_latency()
    {
        std::uint32_t v = 0;
        const auto t0 = sc_core::sc_time_stamp();
        check(access(false, 0x0100'0000, v) == tlm::TLM_OK_RESPONSE, "timed DSRAM read");
        return sc_core::sc_time_stamp() - t0;
    }

    void clocks()
    {
        const sc_core::sc_time ns50(50, sc_core::SC_NS), ns5(5, sc_core::SC_NS);
        check(top.bus_clock_period() == ns50, "bus fabric on the 20 MHz OSC after reset");
        const auto osc_latency = dsram_read_latency();
        write(kCrm + FU3_CRM_PLL_OFFSET, 0x150);       // 20 MHz x 20 / 1 = 400 MHz
        write(kCrm + FU3_CRM_PLL_CTRL_OFFSET, 0x5948);  // powered up
        write(kCrm + FU3_CRM_CLK_0_OFFSET, 1);
        check(top.cpu_clock_period() == sc_core::sc_time(50, sc_core::SC_NS), "CLK_0 alone keeps the OSC");
        write(kCrm + FU3_CRM_CLK_1_OFFSET, 1);
        wait(sc_core::SC_ZERO_TIME);
        check(top.cpu_clock_period() == sc_core::sc_time(2.5, sc_core::SC_NS) &&
                  top.cva6()->clock_period() == top.cpu_clock_period(),
              "both CPUs at 400 MHz once the PLL is selected");
        check(top.bus_clock_period() == sc_core::sc_time(2.5, sc_core::SC_NS), "CLK.div = 0: bus on the branch clock");
        // Divider change per CRM spec 3.7.4: both selectors back on the OSC,
        // divider, branch selector, final selector.
        write(kCrm + FU3_CRM_CLK_1_OFFSET, 0);
        write(kCrm + FU3_CRM_CLK_0_OFFSET, 0);
        write(kCrm + FU3_CRM_CLK_OFFSET, 1);           // bus divider: 400 / 2 = 200 MHz
        write(kCrm + FU3_CRM_CLK_0_OFFSET, 1);
        write(kCrm + FU3_CRM_CLK_1_OFFSET, 1);
        wait(sc_core::SC_ZERO_TIME);
        check(top.bus_clock_period() == ns5, "bus fabric at 200 MHz with CLK.div = 1");
        const auto bus_latency = dsram_read_latency();
        check(osc_latency == 10 * bus_latency, "a bus read takes 10x less time at 200 MHz than at 20 MHz");
        write(kCrm + FU3_CRM_PLL_CTRL_OFFSET, 0x5949);  // PLL powered down while selected
        wait(sc_core::SC_ZERO_TIME);
        check(top.cpu_clock_period() == sc_core::SC_ZERO_TIME && top.ibex()->clock_period() == sc_core::SC_ZERO_TIME,
              "CPU clock stops with the PLL");
        check(top.bus_clock_period() == ns5, "the fabric keeps its last period while the clock is stopped");
        write(kCrm + FU3_CRM_PLL_CTRL_OFFSET, 0x5948);
        wait(sc_core::SC_ZERO_TIME);
        check(top.cpu_clock_period() == sc_core::sc_time(2.5, sc_core::SC_NS), "CPU clock back at 400 MHz");
    }

    void handoff()
    {
        // Ibex-side staging write, then CRM FCLK_SW[0] releases CVA6 (the test
        // master stands in for the Ibex boot code here).
        check(top.cva6()->in_reset() && top.fabric().staging(), "CVA6 held, staging open before the handoff");
        const auto before = sc_core::sc_time_stamp();
        write(kCrm + FU3_CRM_FCLK_SW_OFFSET, 1);
        wait(100, sc_core::SC_NS);
        check(!top.fabric().staging() && !top.cva6()->in_reset(), "FCLK_SW releases CVA6 with staging closed");
        check(count("boot staging closed") == 1 && last("boot staging closed") >= before &&
                  first("boot staging closed") < first("CVA6 released"),
              "staging closes before CVA6 is released");
        write(kCrm + FU3_CRM_FCLK_SW_OFFSET, 0);
        wait(100, sc_core::SC_NS);
        check(top.cva6()->in_reset(), "FCLK_SW = 0 holds CVA6 again");
        write(kCrm + FU3_CRM_FCLK_SW_OFFSET, 1);
        wait(100, sc_core::SC_NS);
        check(!top.cva6()->in_reset() && !top.fabric().staging() && count("boot staging closed") == 1 &&
                  count("CVA6 released") == 2,
              "a local CVA6 reset does not reopen staging");
    }

    // One system reset from `trigger`; checks the new boot epoch. `pulse`: the
    // system reset lasts the CRM hold time (1.4 us) and its scope is sampled.
    // The global reset has no width in the VP: SYSCSR drops the request as the
    // reset it causes starts, and the CRM restarts in the same delta cycles.
    void system_reset(const std::string& what, unsigned cause, bool dbg_kept, void (bench::*trigger)(bool),
                      bool pulse = true)
    {
        const auto resets = top.reset_history().size();
        const auto ibex_releases = count("Ibex released");
        (this->*trigger)(true);
        wait(200, sc_core::SC_NS);
        check(top.ibex()->in_reset() && top.cva6()->in_reset(), what + ": CPUs held");
        if (pulse) {
            check(!top.system_ready() && !top.gate("DSRAM")->available(), what + ": system domain in reset");
            check(top.gate("IBEX_DBG")->available() == dbg_kept, what + (dbg_kept ? ": DBG kept" : ": DBG reset too"));
        }
        (this->*trigger)(false);
        wait_ready();
        check(top.reset_history().size() == resets + 1 && top.reset_history().back().causes == cause,
              what + ": reset cause recorded");
        // FW-visible cause (FU3_VP_ABI_1): the bitmap was cleared before, so it
        // holds exactly this reset; W1C clears it (POR-only state survives the reset).
        check(read(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET) == cause, what + ": ABI RESET_CAUSE shows it");
        write(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET, cause);
        check(read(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET) == 0, what + ": RESET_CAUSE W1C");
        check(top.fabric().staging() && top.cva6()->in_reset() && count("Ibex released") == ibex_releases + 1,
              what + ": new boot epoch (staging open, Ibex rebooted, CVA6 held)");
        check(read(kCrm + FU3_CRM_FCLK_SW_OFFSET) == 0, what + ": FCLK_SW cleared");
    }
    void software_request(bool on)
    {
        if (on)
            write(kSyscsr + FU3_SYSCSR_SYS_CTRL_OFFSET, FU3_SYSCSR_SW_RESET_REQ);
    }
    void watchdog_request(bool on) { top.drive_watchdog_request(on); }
    void debug_request(bool on) { top.drive_ndmreset(on); }
    void global_request(bool on)
    {
        if (on)
            write(kSyscsr + FU3_SYSCSR_SYS_CTRL_OFFSET, FU3_SYSCSR_SW_GLOBAL_RST_REQ);
    }

    void resets()
    {
        top.set_straps(false, true, false);   // pins change while running
        wait(10, sc_core::SC_NS);
        check((read(kSyscsr + FU3_SYSCSR_STATUS_OFFSET) & 0x7) == 0x2, "SYSCSR STATUS follows the pins (live)");
        check((read(kSyscsr + FU3_VP_BOOT_STATUS_OFFSET) & 0x7) == 0x5, "ABI BOOT_STATUS keeps the captured straps");
        write(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET, 0xff);   // clear the POR history (W1C)
        check(read(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET) == 0, "RESET_CAUSE: POR cleared by W1C");
        write(kCrm + FU3_CRM_FCLK_SW_OFFSET, 1);   // CVA6 running before the reset
        wait(100, sc_core::SC_NS);

        system_reset("software reset", reset_software, false, &bench::software_request);
        check(read(kSyscsr + FU3_SYSCSR_SYS_CTRL_OFFSET) == 0, "software reset clears its own request");
        check(read(kSyscsr + FU3_VP_BOOT_STATUS_OFFSET) == (FU3_VP_BOOT_STRAPS_VALID | FU3_VP_BOOT_MODE_0),
              "ABI BOOT_STATUS: new straps captured at the reset release");
        check(read(kCrm + FU3_CRM_CLK_1_OFFSET) == 1 && top.cpu_clock_period() == sc_core::sc_time(2.5, sc_core::SC_NS),
              "CRM clock setup kept across a system reset");

        system_reset("watchdog reset", reset_watchdog, false, &bench::watchdog_request);
        system_reset("debug reset", reset_debug, true, &bench::debug_request);
        system_reset("global reset", reset_global, false, &bench::global_request, false);
        check(read(kCrm + FU3_CRM_CLK_1_OFFSET) == 0 && top.cpu_clock_period() == sc_core::sc_time(50, sc_core::SC_NS) &&
                  top.bus_clock_period() == sc_core::sc_time(50, sc_core::SC_NS),
              "global reset restores the CRM: CPUs and bus back on the OSC");
    }

    void run()
    {
        power_on();
        gates();
        clocks();
        handoff();
        resets();
        check(sc_core::sc_report_handler::get_count("fu3_crm/clock_switch") == 0,
              "no clock switch outside the supported sequence");
        std::cout << "fu3_soc clock/reset: " << checks << " checks, " << failures << " failures -> "
                  << (failures ? "FAIL" : "PASS") << '\n';
        finished = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions("fu3_soc/not_integrated", sc_core::SC_DO_NOTHING);
    sc_core::sc_report_handler::set_actions("fu3_soc/gated", sc_core::SC_DO_NOTHING);
    fu3_soc_options options;
    options.board.boot = true;
    options.board.mode_1 = true;
    options.board.debug_en = true;   // DEBUG_EN high from power-on (review ABI1-R01)
    options.jtag = false;   // the bench master takes the DBG port
    bench b("bench", options);
    sc_core::sc_start(50, sc_core::SC_MS);
    if (!b.finished)
        std::cerr << "FAIL: the bench did not run to its end (simulation starved or timed out at "
                  << sc_core::sc_time_stamp() << ")\n";
    return b.failures || !b.checks || !b.finished ? 1 : 0;
}
