// Plans T08-T18: the peripheral models on the assembled platform, driven by a
// test master on the IBEX initiator port (free: no CPUs): each block answers at
// its bus address and its interrupt reaches the PLIC (CVA6) and, through the
// INTMAP, the Ibex input of the Common v2.1 Interrupt Mapping. Without CPUs:
// the interrupt levels are checked at the CPU-side nets. Last, the CRM clock
// gates (HAS 6.1, review R01): a gated IP stops (not only its MMIO), keeps its
// state and resumes when the clock returns.

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <fu3_bus/access.h>
#include <fu3_crm/registers.h>
#include <fu3_plic/registers.h>
#include <fu3/aes/registers.h>
#include <fu3/gpio/registers.h>
#include <fu3/qspi/registers.h>
#include <fu3_hyperram/registers.h>
#include <fu3_i2c/registers.h>
#include <fu3_mailbox/registers.h>
#include <fu3_rng/registers.h>
#include <fu3_spi/registers.h>
#include <fu3_syscsr/registers.h>
#include <fu3_vp_abi.h>
#include <fu3_timer/registers.h>
#include <fu3_uart/registers.h>
#include <fu3_wdt/registers.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

constexpr std::uint64_t kCrm = FU3_CRM_BASE, kPlic = FU3_PLIC_BASE, kSyscsr = FU3_VP_ABI_SYSCSR_BASE;

struct bench : sc_core::sc_module {
    fu3_soc_top top;
    fu3_peripherals& p;
    tlm_utils::simple_initiator_socket<bench> master{"master"};
    tlm_utils::simple_initiator_socket<bench> cva6_master{"cva6_master"};   // CVA6 port (free without CPUs)
    unsigned checks = 0, failures = 0;
    bool finished = false;   // run() reached its end (not a starved or stopped simulation)

    static fu3_soc_options options()
    {
        fu3_soc_options o;
        o.cpus = false;
        o.board.test_mode_sel = true;   // die 13: reaches SYSCSR STATUS[4] (test mode itself off)
        return o;
    }

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n), top("fu3_soc", options()), p(top.peripherals())
    {
        master.bind(top.fabric().initiator("IBEX"));   // free without CPUs; IBEX may access every peripheral
        cva6_master.bind(top.fabric().initiator("CVA6"));
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
    tlm::tlm_response_status access(bool write, std::uint64_t address, std::uint32_t& value,
                                    const unsigned char* enables = nullptr)
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
        if (enables) {
            tx.set_byte_enable_ptr(const_cast<unsigned char*>(enables));
            tx.set_byte_enable_length(4);
        }
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        fu3_bus::ScopedAccess scoped(tx, 4);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        master->b_transport(tx, delay);
        wait(delay);
        value = data[0] | data[1] << 8 | data[2] << 16 | std::uint32_t(data[3]) << 24;
        return tx.get_response_status();
    }
    // One byte (8-bit registers such as the UART's at byte offsets 0-7).
    tlm::tlm_response_status access8(bool write, std::uint64_t address, std::uint8_t& value)
    {
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(&value);
        tx.set_data_length(1);
        tx.set_streaming_width(1);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        fu3_bus::ScopedAccess scoped(tx, 1);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        master->b_transport(tx, delay);
        wait(delay);
        return tx.get_response_status();
    }
    void write8(std::uint64_t address, std::uint8_t value)
    {
        check(access8(true, address, value) == tlm::TLM_OK_RESPONSE, "byte write OK at " + hex(address));
    }
    std::uint8_t read8(std::uint64_t address)
    {
        std::uint8_t value = 0;
        check(access8(false, address, value) == tlm::TLM_OK_RESPONSE, "byte read OK at " + hex(address));
        return value;
    }
    void write(std::uint64_t address, std::uint32_t value)
    {
        check(access(true, address, value) == tlm::TLM_OK_RESPONSE, "write OK at " + hex(address));
    }
    std::uint32_t read(std::uint64_t address)
    {
        std::uint32_t value = 0;
        check(access(false, address, value) == tlm::TLM_OK_RESPONSE, "read OK at " + hex(address));
        return value;
    }
    static std::string hex(std::uint64_t v)
    {
        char b[24];
        std::snprintf(b, sizeof b, "0x%08llx", static_cast<unsigned long long>(v));
        return b;
    }
    // Waits up to `limit` for `cond`, in steps of 50 ns.
    bool wait_for(const std::function<bool()>& cond, sc_core::sc_time limit)
    {
        const auto end = sc_core::sc_time_stamp() + limit;
        while (!cond() && sc_core::sc_time_stamp() < end)
            wait(50, sc_core::SC_NS);
        return cond();
    }
    bool ibex(unsigned input) const { return p.ibex_irq[input].read(); }
    bool plic_pending(unsigned id) { return (read(kPlic + FU3_PLIC_PENDING_OFFSET) >> id) & 1; }

    void wait_ready()
    {
        check(wait_for([this] { return top.system_ready(); }, sc_core::sc_time(50, sc_core::SC_US)),
              "system domain out of reset");
    }

    // T08: PLIC registers, gateway/claim/complete to CVA6 irq_i[0]; INTMAP routes.
    void plic_intmap()
    {
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(31), ~0u);
        check(read(kPlic + FU3_PLIC_PRIORITY_OFFSET(31)) == 31, "PLIC: ID 31 priority, 5 bits");
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(31), 0);

        // CRM.int is PLIC source ID 1 and Ibex irq_fast_i[1].
        write(kCrm + FU3_CRM_CRM_OFFSET, 1);
        wait(10, sc_core::SC_NS);
        check(plic_pending(FU3_PLIC_ID_CRM), "PLIC: CRM pending (ID 1)");
        check(!p.cva6_external.read(), "PLIC: not enabled -> no CVA6 interrupt");
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_CRM), 3);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, 1u << FU3_PLIC_ID_CRM);
        wait(10, sc_core::SC_NS);
        check(p.cva6_external.read(), "PLIC: enabled CRM interrupt -> CVA6 irq_i[0]");
        check(read(kPlic + FU3_PLIC_CLAIM_OFFSET) == FU3_PLIC_ID_CRM, "PLIC: claim returns ID 1");
        wait(10, sc_core::SC_NS);
        check(!p.cva6_external.read(), "PLIC: claimed -> irq_i[0] low");

        // INTMAP: routes reset disabled (decision Q03-b), enabled by firmware
        // through the FU3_VP_ABI_1 CSR (SYSCSR 0x110/0x114).
        check(!ibex(1) && read(kSyscsr + FU3_VP_INTMAP_EN0_OFFSET) == 0 && read(kSyscsr + FU3_VP_INTMAP_EN1_OFFSET) == 0,
              "INTMAP: every route disabled at reset (ABI enables read 0)");
        write(kSyscsr + FU3_VP_INTMAP_EN0_OFFSET, 1u << FU3_VP_INTMAP_SRC_CRM);
        wait(10, sc_core::SC_NS);
        check(ibex(1), "INTMAP: CRM -> Ibex irq_fast_i[1]");
        write(kCrm + FU3_CRM_CRM_OFFSET, 0);
        write(kPlic + FU3_PLIC_CLAIM_OFFSET, FU3_PLIC_ID_CRM);   // complete
        wait(10, sc_core::SC_NS);
        check(!ibex(1) && !p.cva6_external.read(), "CRM.int low: both CPU inputs low");
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, 0);
        enable_routes();   // the remaining checks look at every route
    }
    // FU3_VP_ABI_1 registers (SYSCSR 0x100-0x123): access rules and SWIRQ.
    void vp_abi()
    {
        const std::uint64_t a = kSyscsr;
        check(read(a + FU3_VP_ABI_VERSION_OFFSET) == FU3_VP_ABI_VERSION_VALUE, "ABI: VP_ABI_VERSION 1.0");
        std::uint32_t v = 0x12345678;
        check(access(true, a + FU3_VP_ABI_VERSION_OFFSET, v) == tlm::TLM_OK_RESPONSE &&
                  read(a + FU3_VP_ABI_VERSION_OFFSET) == FU3_VP_ABI_VERSION_VALUE,
              "ABI: VP_ABI_VERSION read-only");
        write(a + FU3_VP_RESERVED_10C_OFFSET, ~0u);
        check(read(a + FU3_VP_RESERVED_10C_OFFSET) == 0, "ABI: 0x10C reserved, RAZ/WI");
        // Byte lanes and zero strobe on INTMAP_EN0.
        const auto before = read(a + FU3_VP_INTMAP_EN0_OFFSET);
        const unsigned char none[4] = {0, 0, 0, 0}, lane1[4] = {0, 0xff, 0, 0};
        v = 0;
        access(true, a + FU3_VP_INTMAP_EN0_OFFSET, v, none);
        check(read(a + FU3_VP_INTMAP_EN0_OFFSET) == before, "ABI: zero-strobe write has no effect");
        access(true, a + FU3_VP_INTMAP_EN0_OFFSET, v, lane1);
        check(read(a + FU3_VP_INTMAP_EN0_OFFSET) == (before & ~0xff00u) && !p.intmap().enabled(FU3_VP_INTMAP_SRC_UART(0)),
              "ABI: byte lane 1 clears sources 8-15 (UART routes) only");
        write8(a + FU3_VP_INTMAP_EN0_OFFSET + 1, 0xff);
        check(read(a + FU3_VP_INTMAP_EN0_OFFSET) == before, "ABI: byte write sets them again");
        // DEBUG_EN is live in BOOT_STATUS; the captured straps do not move.
        const auto captured = read(a + FU3_VP_BOOT_STATUS_OFFSET) & 0x1f;
        p.set_board_pad(6, true);
        wait(top.bus_clock_period() * 2);
        check((read(a + FU3_VP_BOOT_STATUS_OFFSET) & FU3_VP_BOOT_DEBUG_EN) &&
                  (read(a + FU3_VP_BOOT_STATUS_OFFSET) & 0x1f) == captured,
              "ABI BOOT_STATUS: DEBUG_EN (die 6) live, captured straps unchanged");
        p.set_board_pad(6, false);
        wait(top.bus_clock_period() * 2);
        check(!(read(a + FU3_VP_BOOT_STATUS_OFFSET) & FU3_VP_BOOT_DEBUG_EN), "ABI BOOT_STATUS: DEBUG_EN low again");
        // SWIRQ: level until cleared; CVA6 ipi_i directly, Ibex through INTMAP source 32.
        check(read(a + FU3_VP_SWIRQ_PENDING_OFFSET) == 0 && !p.cva6_software.read() && !ibex(fu3_plic::kOutSoftware),
              "ABI SWIRQ: none pending");
        write(a + FU3_VP_SWIRQ_SET_OFFSET, FU3_VP_SWIRQ_CVA6 | FU3_VP_SWIRQ_IBEX | 0xf0);
        wait(top.bus_clock_period() * 2);
        check(read(a + FU3_VP_SWIRQ_PENDING_OFFSET) == (FU3_VP_SWIRQ_CVA6 | FU3_VP_SWIRQ_IBEX) &&
                  read(a + FU3_VP_SWIRQ_SET_OFFSET) == 0 && read(a + FU3_VP_SWIRQ_CLEAR_OFFSET) == 0,
              "ABI SWIRQ: W1S sets both (reserved bits ignored); SET/CLEAR read 0");
        check(p.cva6_software.read() && ibex(fu3_plic::kOutSoftware), "ABI SWIRQ: CVA6 ipi_i and Ibex irq_software_i high");
        write(a + FU3_VP_INTMAP_EN1_OFFSET, 0x2);   // Ibex software route off, TIMER1 kept
        wait(top.bus_clock_period() * 2);
        check(!ibex(fu3_plic::kOutSoftware) && p.cva6_software.read(), "ABI SWIRQ: Ibex input gated by its INTMAP route");
        write(a + FU3_VP_INTMAP_EN1_OFFSET, 0x3);
        unsigned char data[4] = {};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(a + FU3_VP_SWIRQ_CLEAR_OFFSET);
        data[0] = FU3_VP_SWIRQ_CVA6;
        tx.set_data_ptr(data);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        master->transport_dbg(tx);
        check(read(a + FU3_VP_SWIRQ_PENDING_OFFSET) == (FU3_VP_SWIRQ_CVA6 | FU3_VP_SWIRQ_IBEX),
              "ABI: a debug-path write does not clear SWIRQ (no side effect)");
        write(a + FU3_VP_SWIRQ_CLEAR_OFFSET, FU3_VP_SWIRQ_CVA6);
        wait(top.bus_clock_period() * 2);
        check(read(a + FU3_VP_SWIRQ_PENDING_OFFSET) == FU3_VP_SWIRQ_IBEX && !p.cva6_software.read() &&
                  ibex(fu3_plic::kOutSoftware),
              "ABI SWIRQ: W1C clears CVA6 only");
        write(a + FU3_VP_SWIRQ_CLEAR_OFFSET, FU3_VP_SWIRQ_IBEX);
        wait(top.bus_clock_period() * 2);
        check(read(a + FU3_VP_SWIRQ_PENDING_OFFSET) == 0 && !ibex(fu3_plic::kOutSoftware), "ABI SWIRQ: all cleared");
        // RESET_CAUSE is W1C only (writing 0 keeps it).
        const auto cause = read(a + FU3_VP_RESET_CAUSE_OFFSET);
        write(a + FU3_VP_RESET_CAUSE_OFFSET, 0);
        check(cause && read(a + FU3_VP_RESET_CAUSE_OFFSET) == cause, "ABI RESET_CAUSE: writing 0 changes nothing");
    }

    // All INTMAP routes on, as firmware does after every SYSCSR reset.
    void enable_routes()
    {
        write(kSyscsr + FU3_VP_INTMAP_EN0_OFFSET, ~0u);
        write(kSyscsr + FU3_VP_INTMAP_EN1_OFFSET, ~0u);
        check(read(kSyscsr + FU3_VP_INTMAP_EN0_OFFSET) == ~0u &&
                  read(kSyscsr + FU3_VP_INTMAP_EN1_OFFSET) == FU3_VP_INTMAP_EN1_MASK,
              "ABI INTMAP_EN0/1: all 34 routes enabled (EN1 bits 2-31 reserved)");
    }

    // T09: TIMER0-3 through the pending latch (Q03-d), WDT0/1 interrupts and reset.
    void timers()
    {
        const std::uint64_t base[4] = {FU3_TIMER0_BASE, FU3_TIMER1_BASE, FU3_TIMER2_BASE, FU3_TIMER3_BASE};
        const std::function<bool()> line[4] = {
            [this] { return p.cva6_timer.read(); },
            [this] { return p.plic_source[FU3_PLIC_ID_TIMER1 - 1].read() && ibex(fu3_plic::kOutTimer); },
            [this] { return p.plic_source[FU3_PLIC_ID_TIMER2 - 1].read() && ibex(fu3_plic::kOutExternal); },
            [this] { return p.plic_source[FU3_PLIC_ID_TIMER3 - 1].read() && ibex(fu3_plic::kOutExternal); },
        };
        const char* where[4] = {"CVA6 time_irq_i", "PLIC ID6 + Ibex irq_timer_i", "PLIC ID7 + Ibex irq_external_i",
                                "PLIC ID8 + Ibex irq_external_i"};
        for (unsigned i = 0; i < 4; ++i) {
            const std::string t = "TIMER" + std::to_string(i) + ": ";
            check(p.timer(i).period() == top.bus_clock_period(), t + "ticks on the CRM bus clock");
            write(base[i] + FU3_TIMER_COMPARE_OFFSET, 8);
            write(base[i] + FU3_TIMER_CONTROL_OFFSET, FU3_TIMER_ENABLE);
            check(wait_for(line[i], top.bus_clock_period() * 30), t + "compare match reaches " + where[i]);
            // The match lasts one tick; the latch holds it until CMP is written.
            wait(top.bus_clock_period() * 3);
            check(line[i](), t + "interrupt held by the pending latch");
            read(base[i] + FU3_TIMER_COMPARE_OFFSET);
            const unsigned char none[4] = {0, 0, 0, 0};
            std::uint32_t v = 8;
            access(true, base[i] + FU3_TIMER_COMPARE_OFFSET, v, none);
            wait(sc_core::SC_ZERO_TIME);
            check(line[i](), t + "read and zero-strobe write do not acknowledge");
            write(base[i] + FU3_TIMER_CONTROL_OFFSET, 0);
            write(base[i] + FU3_TIMER_COMPARE_OFFSET, 8);
            wait(sc_core::SC_ZERO_TIME);
            check(!line[i](), t + "CMP write acknowledges");
        }
    }

    void watchdogs()
    {
        const std::uint64_t base[2] = {FU3_WDT0_BASE, FU3_WDT1_BASE};
        const std::function<bool()> line[2] = {
            [this] { return p.wdt0_warning.read() && ibex(0); },
            [this] { return p.plic_source[FU3_PLIC_ID_WDT1 - 1].read() && ibex(fu3_plic::kOutExternal); },
        };
        for (unsigned i = 0; i < 2; ++i) {
            const std::string w = "WDT" + std::to_string(i) + ": ";
            check(p.wdt(i).tick() == top.bus_clock_period(), w + "ticks on the CRM bus clock");
            write(base[i] + FU3_WDT_CNT_LD, 16);
            write(base[i] + FU3_WDT_CNT_CTRL, FU3_WDT_INTEN);   // no reset generation
            check(wait_for(line[i], top.bus_clock_period() * 40),
                  w + (i ? "timeout -> PLIC ID18 + Ibex irq_external_i" : "timeout -> Ibex irq_fast_i[0]"));
            write(base[i] + FU3_WDT_INT_CLR, 1);
            write(base[i] + FU3_WDT_CNT_CTRL, 0);
            wait(top.bus_clock_period() * 2);
            check(!line[i](), w + "interrupt cleared");
        }
        // WDT1 with reset generation: second timeout -> CRM system reset.
        const auto resets = top.reset_history().size();
        write(FU3_WDT1_BASE + FU3_WDT_CNT_LD, 16);
        write(FU3_WDT1_BASE + FU3_WDT_CNT_CTRL, FU3_WDT_INTEN | FU3_WDT_RESEN);
        check(wait_for([&] { return top.reset_history().size() > resets; }, top.bus_clock_period() * 100),
              "WDT1 second timeout resets the system through the CRM");
        check(top.reset_history().back().causes == reset_watchdog, "reset cause: watchdog");
        wait_ready();
        check(read(FU3_WDT1_BASE + FU3_WDT_CNT_CTRL) == FU3_WDT_RESEN, "WDT1 back at its reset state");
        check(read(kSyscsr + FU3_VP_INTMAP_EN0_OFFSET) == 0 && !p.intmap().enabled(fu3_plic::kSrcUart0),
              "ABI: the SYSCSR reset (watchdog) cleared the INTMAP route enables");
        check(read(kSyscsr + FU3_VP_RESET_CAUSE_OFFSET) == (FU3_VP_RESET_POR | FU3_VP_RESET_WATCHDOG),
              "ABI RESET_CAUSE: POR and watchdog accumulated");
        enable_routes();
    }

    // T10: UART0-7: registers, TX to the board-side line, RX from it, RX
    // interrupt to the PLIC and Ibex irq_fast_i[8].
    // Pin multiplexing as firmware sets it (Common v2.1 IOPAD MAP; SYSCSR
    // csr_io_cfg is not connected yet, decision Q06-b: host selects).
    void select_uart(unsigned i, bool on)
    {
        const auto& pads = fu3_peripherals::kUartPad[i];
        p.set_pad_select(pads.txd, on ? pads.select : 0);
        p.set_pad_select(pads.rxd, on ? pads.select : 0);
        if (i == 5)
            p.attach_uart_peer(5, on);   // UART5 shares the I2C pads
    }
    void select_spi(unsigned k, bool on)
    {
        const auto& pads = fu3_peripherals::kSpiPad[k];
        for (unsigned die : {pads.sck, pads.miso, pads.mosi, pads.ssn})
            p.set_pad_select(die, on ? pads.select : 0);
        if (k == 1)
            p.attach_uart_peer(3, !on);   // SSN_1 is on RXD_3's pad (die 63)
    }
    void select_i2c(bool on)
    {
        p.set_pad_select(fu3_peripherals::kI2cSdaPad, on ? fu3_peripherals::kI2cSelect : 0);
        p.set_pad_select(fu3_peripherals::kI2cSclPad, on ? fu3_peripherals::kI2cSelect : 0);
    }

    void uarts()
    {
        const std::uint64_t base[8] = {0x440d0000, 0x440e0000, 0x44090000, 0x440a0000,
                                       0x50050000, 0x500d0000, 0x500e0000, 0x500f0000};
        const unsigned id[8] = {FU3_PLIC_ID_UART0, FU3_PLIC_ID_UART1, FU3_PLIC_ID_UART2, FU3_PLIC_ID_UART3,
                                FU3_PLIC_ID_UART4, FU3_PLIC_ID_UART5, FU3_PLIC_ID_UART6, FU3_PLIC_ID_UART7};
        for (unsigned i = 0; i < 8; ++i) {
            const std::string u = "UART" + std::to_string(i) + ": ";
            const auto b = base[i];
            const auto& pads = fu3_peripherals::kUartPad[i];
            if (i == 2) {
                // Not selected on its pads (b00 there is GPIOC_2/3): the line stays quiet.
                write8(b + FU3_UART_LCR, FU3_UART_DLAB);
                write8(b + FU3_UART_DLL, 1);
                write8(b + FU3_UART_DLM, 0);
                write8(b + FU3_UART_LCR, 0x03);
                write8(b + FU3_UART_THR, 'q');
                wait(p.uart(i).bit_time() * 20.0);
                check(p.uart_peer(i).received().empty() && !p.pad_level(pads.txd),
                      u + "TXD not on die " + std::to_string(pads.txd) + " while csr_io_cfg selects GPIO");
            }
            select_uart(i, true);
            check(std::string(p.pinmux().function(pads.txd)) == "TXD_" + std::to_string(i) &&
                      std::string(p.pinmux().function(pads.rxd)) == "RXD_" + std::to_string(i),
                  u + "IOPAD MAP: TXD/RXD on die " + std::to_string(pads.txd) + "/" + std::to_string(pads.rxd));
            write8(b + FU3_UART_SCR, 0x5a + i);
            check(read8(b + FU3_UART_SCR) == 0x5a + i, u + "scratch register at its v2.1 address");
            write8(b + FU3_UART_LCR, FU3_UART_DLAB);
            write8(b + FU3_UART_DLL, 1);   // divisor 1: bit = 16 bus clocks
            write8(b + FU3_UART_DLM, 0);
            write8(b + FU3_UART_LCR, 0x03);   // 8N1
            write8(b + FU3_UART_FCR, 0x07);   // FIFOs on, cleared
            const char tx = static_cast<char>('A' + i);
            write8(b + FU3_UART_THR, static_cast<std::uint8_t>(tx));
            check(wait_for([&] { return p.uart_peer(i).received().size() == 1; }, sc_core::sc_time(20, sc_core::SC_US)) &&
                      p.uart_peer(i).received()[0] == tx,
                  u + "THR byte arrives on the board-side TXD line");
            write8(b + FU3_UART_IER, FU3_UART_IER_RX);
            p.uart_peer(i).send(std::string(1, static_cast<char>('a' + i)));
            check(wait_for([&] { return p.plic_source[id[i] - 1].read() && ibex(8); }, sc_core::sc_time(20, sc_core::SC_US)),
                  u + "RX data interrupt -> PLIC ID " + std::to_string(id[i]) + " + Ibex irq_fast_i[8]");
            check((read8(b + FU3_UART_LSR) & FU3_UART_DR) && read8(b + FU3_UART_RBR) == 'a' + i, u + "RBR holds the byte");
            wait(top.bus_clock_period() * 4);
            check(!p.plic_source[id[i] - 1].read(), u + "interrupt clears once RBR is read");
            // The INTMAP route enable of this UART (and no other) gates its input.
            p.uart_peer(i).send("x");
            check(wait_for([&] { return ibex(8); }, sc_core::sc_time(20, sc_core::SC_US)), u + "second RX interrupt");
            p.intmap().set_enable(fu3_plic::kSrcUart0 + i, false);
            check(wait_for([&] { return !ibex(8); }, top.bus_clock_period()), u + "own INTMAP route enable gates irq_fast_i[8]");
            p.intmap().set_enable(fu3_plic::kSrcUart0 + i, true);
            check(wait_for([&] { return ibex(8); }, top.bus_clock_period()), u + "route enabled again");
            read8(b + FU3_UART_RBR);
            check((read8(b + FU3_UART_MSR) & 0xf0) == 0x10, u + "modem tie-offs: CTS asserted, DSR/RI/DCD inactive");
            write8(b + FU3_UART_IER, 0);
            if (i >= 2)
                select_uart(i, false);   // later sections use these pads for SPI/I2C/GPIO
        }
    }

    // T11: SPI0-3 master transfer through a board loopback (MOSI -> MISO),
    // end-of-transfer interrupt; I2C master START + address to an empty bus:
    // NACK and the transfer interrupt.
    void serial()
    {
        const std::uint64_t base[4] = {FU3_SPI0_BASE, FU3_SPI1_BASE, FU3_SPI2_BASE, FU3_SPI3_BASE};
        const unsigned id[4] = {FU3_PLIC_ID_SPI0, FU3_PLIC_ID_SPI1, FU3_PLIC_ID_SPI2, FU3_PLIC_ID_SPI3};
        for (unsigned i = 0; i < 4; ++i) {
            const std::string t = "SPI" + std::to_string(i) + ": ";
            select_spi(i, true);
            p.set_spi_loopback(i, true);
            write(base[i] + FU3_SPI_CTRL, 0xe2);   // 8-bit frames, master, disabled
            write(base[i] + FU3_SPI_ISR, 0x7c);
            write(base[i] + FU3_SPI_DIV, 2);
            write(base[i] + FU3_SPI_IER, FU3_SPI_EOT);
            write(base[i] + FU3_SPI_TXDR, 0xa5 + i);
            write(base[i] + FU3_SPI_CTRL, 0xe3);
            check(wait_for([&] { return p.plic_source[id[i] - 1].read() && ibex(9); }, sc_core::sc_time(50, sc_core::SC_US)),
                  t + "end of transfer -> PLIC ID " + std::to_string(id[i]) + " + Ibex irq_fast_i[9]");
            check(read(base[i] + FU3_SPI_RXDR) == 0xa5 + i, t + "looped-back byte received");
            write(base[i] + FU3_SPI_ISR, 0x7c);
            write(base[i] + FU3_SPI_IER, 0);
            write(base[i] + FU3_SPI_CTRL, 0xe2);
            wait(top.bus_clock_period() * 4);
            check(!p.plic_source[id[i] - 1].read(), t + "interrupt cleared");
            p.set_spi_loopback(i, false);
            select_spi(i, false);
        }

        namespace r = fu3_i2c::reg;
        const std::uint64_t i2c = fu3_i2c::base_address;
        select_i2c(true);
        write(i2c + r::CTRL, 0);
        write(i2c + r::PRESCALER, 19);
        for (unsigned o = r::TLOW; o <= r::TSU_STO; o += 4)
            write(i2c + o, 29);
        write(i2c + r::TBUF, 29);
        write(i2c + r::TSP, 1);
        write(i2c + r::CTRL, r::EN | r::IEN | r::TIMING_EN);
        write(i2c + r::CMD, r::IACK);
        check(p.i2c_scl() && p.i2c_sda(), "I2C: idle bus pulled up");
        write(i2c + r::TX, 0xa4);
        write(i2c + r::CMD, r::STA | r::STO | r::WR | r::IACK);
        check(wait_for([&] { return !p.i2c_sda(); }, sc_core::sc_time(500, sc_core::SC_US)) && p.i2c_scl(),
              "I2C: START pulls SDA low on the bus while SCL is high");
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_I2C0 - 1].read() && ibex(12); }, sc_core::sc_time(500, sc_core::SC_US)),
              "I2C: transfer done -> PLIC ID15 + Ibex irq_fast_i[12]");
        check(read(i2c + r::STATUS) & r::RXACK, "I2C: no device on the bus -> NACK");
        select_i2c(false);
        write(i2c + r::CMD, r::IACK);
        wait(top.bus_clock_period() * 4);
        check(!p.plic_source[FU3_PLIC_ID_I2C0 - 1].read() && p.i2c_scl() && p.i2c_sda(), "I2C: IACK clears, STOP frees the bus");
    }

    // T12: GPIO banks through IO_PAD: output to a pad, pad input with a
    // level interrupt to the PLIC and Ibex irq_fast_i[11], GPIO0_WAKE.
    void gpios()
    {
        namespace g = fu3::gpio::reg;
        struct bank { const char* name; std::uint64_t base; unsigned id, out_pad, out_bit, in_pad, in_bit, select; };
        // Common v2.1 IOPAD MAP die pads: GPIOA_1/2 on dies 14/15 (b01, JTAG
        // at b00), GPIOB_6/7 on 46/47 (b00), GPIOC_4/5 on 58/61 (b00).
        const bank banks[3] = {{"GPIOA", 0x50030000, FU3_PLIC_ID_GPIOA, 14, 1, 15, 2, 1},
                               {"GPIOB", 0x50040000, FU3_PLIC_ID_GPIOB, 46, 6, 47, 7, 0},
                               {"GPIOC", 0x440f0000, FU3_PLIC_ID_GPIOC, 58, 4, 61, 5, 0}};
        for (const auto& b : banks) {
            const std::string t = std::string(b.name) + ": ";
            if (b.select) {
                // csr_io_cfg b00 keeps the JTAG function: the GPIO output does not reach the pad.
                write(b.base + g::PADDIR, 1u << b.out_bit);
                write(b.base + g::PADOUT, 1u << b.out_bit);
                wait(top.bus_clock_period() * 4);
                check(!p.pad_level(b.out_pad), t + "not on die " + std::to_string(b.out_pad) + " while b00 selects JTAG");
                write(b.base + g::PADOUT, 0);
            }
            p.set_pad_select(b.out_pad, b.select);
            p.set_pad_select(b.in_pad, b.select);
            check(read(b.base + g::PADCFG0) == g::PADCFG_RESET, t + "PADCFG0 reset value at its v2.1 address");
            write(b.base + g::PADDIR, 1u << b.out_bit);
            write(b.base + g::PADOUT, 1u << b.out_bit);
            wait(top.bus_clock_period() * 4);
            check(p.pad_level(b.out_pad), t + "output bit drives pad " + std::to_string(b.out_pad));
            write(b.base + g::PADOUT, 0);
            wait(top.bus_clock_period() * 4);
            check(!p.pad_level(b.out_pad), t + "output low");
            write(b.base + g::INTEN, 1u << b.in_bit);   // INTTYPE 00: level high
            p.set_board_pad(b.in_pad, true);
            check(wait_for([&] { return p.plic_source[b.id - 1].read() && ibex(11); }, sc_core::sc_time(5, sc_core::SC_US)),
                  t + "pad " + std::to_string(b.in_pad) + " high -> PLIC ID " + std::to_string(b.id) + " + Ibex irq_fast_i[11]");
            check((read(b.base + g::PADIN) >> b.in_bit) & 1, t + "PADIN sees the pad");
            write(b.base + g::INTEN, 0);
            p.set_board_pad(b.in_pad, false);
            wait(top.bus_clock_period() * 6);
            read(b.base + g::INTSTATUS);   // read-clear
            wait(top.bus_clock_period() * 4);
            check(!p.plic_source[b.id - 1].read(), t + "INTSTATUS read clears the interrupt");
            write(b.base + g::POWEREVENT, 1u << b.in_bit);
            p.set_board_pad(b.in_pad, true);
            check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_GPIO0_WAKE - 1].read() && ibex(2); },
                           sc_core::sc_time(5, sc_core::SC_US)),
                  t + "power event -> GPIO0_WAKE (PLIC ID19 + Ibex irq_fast_i[2])");
            p.set_board_pad(b.in_pad, false);
            write(b.base + g::POWEREVENT, 0);
            write(b.base + g::PADDIR, 0);
            p.set_pad_select(b.out_pad, 0);
            p.set_pad_select(b.in_pad, 0);
            p.release_board_pad(b.in_pad);
        }
        const auto status = read(0x44010000 + FU3_SYSCSR_STATUS_OFFSET);
        check(((status >> 4) & 1) && !((status >> 3) & 1),
              "pads: TEST_MODE_SEL die 13 (board) -> SYSCSR STATUS[4], TESTMODE die 5 low -> STATUS[3]");
        // Straps reach SYSCSR through their fixed die pads 2-4 (latched at reset release).
        // Independent transcription of Common v2.1 IOPAD MAP (function, die,
        // csr_io_cfg) against the generated table: all bonded GPIO and SPI pins.
        struct placed { const char* fn; unsigned die, select; };
        const placed pins[] = {
            {"GPIOA_0", 8, 1}, {"GPIOA_1", 14, 1}, {"GPIOA_2", 15, 1}, {"GPIOA_3", 16, 1}, {"GPIOA_4", 17, 1},
            {"GPIOA_5", 22, 0}, {"GPIOA_6", 23, 0}, {"GPIOA_7", 24, 0},
            {"GPIOB_0", 27, 0}, {"GPIOB_1", 28, 0}, {"GPIOB_2", 29, 0}, {"GPIOB_3", 30, 0}, {"GPIOB_4", 31, 0},
            {"GPIOB_5", 32, 0}, {"GPIOB_6", 46, 0}, {"GPIOB_7", 47, 0},
            {"GPIOC_0", 48, 1}, {"GPIOC_1", 49, 1}, {"GPIOC_2", 56, 0}, {"GPIOC_3", 57, 0}, {"GPIOC_4", 58, 0},
            {"GPIOC_5", 61, 0}, {"GPIOC_6", 62, 0}, {"GPIOC_7", 63, 0},
            {"SCK_0", 56, 1}, {"MISO_0", 57, 1}, {"MOSI_0", 58, 1}, {"SSN_0", 61, 1},
            {"SCK_1", 59, 1}, {"MISO_1", 60, 1}, {"MOSI_1", 62, 1}, {"SSN_1", 63, 1},
            {"SCK_2", 14, 2}, {"MISO_2", 15, 2}, {"MOSI_2", 16, 2}, {"SSN_2", 17, 2},
            {"SCK_3", 30, 3}, {"MISO_3", 31, 3}, {"MOSI_3", 32, 3}, {"SSN_3", 46, 3},
            {"SDA_0", 22, 2}, {"SCL_0", 23, 2}, {"TDI", 8, 0}, {"TMS", 14, 0}, {"TDO", 15, 0}, {"TRSTN", 16, 0},
            {"TCLK", 17, 0}, {"QSD_3", 64, 0}, {"HB_RESETN", 94, 0},
        };
        unsigned placed_ok = 0;
        for (const auto& pin : pins) {
            p.set_pad_select(pin.die, pin.select);
            const char* fn = p.pinmux().function(pin.die);
            placed_ok += fn && std::string(fn) == pin.fn;
            if (!fn || std::string(fn) != pin.fn)
                std::cerr << "  IOPAD MAP mismatch: die " << pin.die << " select " << pin.select << " -> "
                          << (fn ? fn : "none") << ", expected " << pin.fn << '\n';
            p.set_pad_select(pin.die, 0);
        }
        check(placed_ok == sizeof pins / sizeof pins[0], "pads: generated IOPAD MAP table matches an independent transcription (" +
                                                             std::to_string(placed_ok) + " pins)");
        check(p.pinmux().contentions() == 0, "pads: no board/chip contention so far");
        check(p.strap_boot.read() == p.pad_level(2) && p.strap_mode_1.read() == p.pad_level(3) &&
                  p.strap_mode_0.read() == p.pad_level(4),
              "pads: strap dies 2-4 reach SYSCSR");
        p.set_board_pad(6, true);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        check(p.debug_en.read(), "pads: DEBUG_EN die 6 is a board input (no FW-visible path, Q05-a)");
        p.set_board_pad(6, false);
        check(p.pinmux().function(7) == nullptr && std::string(p.pinmux().function(8)) == "TDI",
              "pads: die 7 mux (duplicate of die 8, Q06-d) not connected; die 8 carries TDI at b00");
    }

    // T13: QSPI indirect read from the board flash with the transfer-complete
    // interrupt (PLIC ID4, Ibex irq_fast_i[7]), then memory-mapped XIP reads.
    void qspi()
    {
        namespace q = fu3::qspi::reg;
        const std::uint64_t base = q::AP_QSPIC_BASE;
        p.flash().load({0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88});
        check(p.flash().size() == fu3_peripherals::kFlashBytes, "QSPI: 16 MB board flash");
        write(base + q::DCR, 23u << 16);   // FSIZE: 2^(23+1) bytes
        write(base + q::CR, q::EN | q::TCIE);
        write(base + q::FCR, 63);
        write(base + q::DLR, 3);
        const std::uint32_t read_cmd = (2u << 12) | (1u << 10) | (1u << 8) | 0x03;   // 24-bit address, 1 line, READ
        write(base + q::CCR, (1u << 26) | (1u << 24) | read_cmd);                 // indirect read
        write(base + q::AR, 0);
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_QSPI - 1].read() && ibex(7); }, sc_core::sc_time(200, sc_core::SC_US)),
              "QSPI: transfer complete -> PLIC ID4 + Ibex irq_fast_i[7]");
        check(read(base + q::DR) == 0x44332211u, "QSPI: indirect read returns the flash bytes");
        write(base + q::FCR, 63);
        write(base + q::CR, q::EN);
        wait(top.bus_clock_period() * 4);
        check(!p.plic_source[FU3_PLIC_ID_QSPI - 1].read(), "QSPI: TCF cleared");
        write(base + q::CCR, (3u << 26) | (1u << 24) | read_cmd);   // memory-mapped (XIP)
        check(read(0x30000004) == 0x88776655u && read(0x30000000) == 0x44332211u, "QSPI_XIP: memory-mapped reads at 0x3000_0000");
        std::uint32_t v = 1;
        check(access(true, 0x30000000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "QSPI_XIP: read/execute only");
        // FU3_VP_ABI_1: Ibex may fetch from XIP; CVA6 may read it but not fetch
        // (CVA6-Config #54: executable region is CVA6_ISRAM only).
        check(fetch(master, 0x30000000) == tlm::TLM_OK_RESPONSE, "QSPI_XIP: Ibex instruction fetch allowed");
        check(fetch(cva6_master, 0x30000000) == tlm::TLM_GENERIC_ERROR_RESPONSE, "QSPI_XIP: CVA6 instruction fetch denied");
        check(fetch(cva6_master, 0x30000000, false) == tlm::TLM_OK_RESPONSE, "QSPI_XIP: CVA6 data read allowed");
    }
    // One word through `socket` with the bus Access extension (instruction fetch or data).
    tlm::tlm_response_status fetch(tlm_utils::simple_initiator_socket<bench>& socket, std::uint64_t address,
                                   bool instruction = true)
    {
        unsigned char data[4] = {};
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(data);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        fu3_bus::ScopedAccess scoped(tx, 4, fu3_bus::Burst::Increment, instruction);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        wait(delay);
        return tx.get_response_status();
    }

    // T14: HyperRAM: CSR ID, memory refused before firmware initialisation,
    // data write/read through HYPERBUS after it (provisional VP ABI v1, Q07-a).
    // CPU 400 MHz / bus 200 MHz, switched as CRM spec 3.7.4 requires.
    void pll_400_200()
    {
        write(kCrm + FU3_CRM_PLL_OFFSET, 0x150);
        write(kCrm + FU3_CRM_PLL_CTRL_OFFSET, 0x5948);
        check(wait_for([this] { return (read(kCrm + FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK) != 0; },
                       sc_core::sc_time(40, sc_core::SC_US)),
              "PLL locks");
        write(kCrm + FU3_CRM_CLK_OFFSET, 1);
        write(kCrm + FU3_CRM_CLK_0_OFFSET, 1);
        write(kCrm + FU3_CRM_CLK_1_OFFSET, 1);
        wait(sc_core::SC_ZERO_TIME);
        check(top.bus_clock_period() == sc_core::sc_time(5, sc_core::SC_NS), "bus clock 200 MHz");
    }

    void hyperram()
    {
        namespace h = fu3_hyperram::reg;
        const std::uint64_t csr = fu3_hyperram::csr_base, mem = fu3_hyperram::memory_base;
        check(read(csr + h::ID) == 0x48525650, "HBCTRL: ID at 0x500b_0000");
        std::uint32_t v = 0;
        check(access(false, mem, v) != tlm::TLM_OK_RESPONSE, "HYPERBUS: memory refused before initialisation");
        // A HyperRAM access does not fit tCSM at the 20 MHz OSC: firmware
        // raises the clock first (the remaining checks run at 200 MHz).
        pll_400_200();
        check(top.bus_clock_period() == sc_core::sc_time(5, sc_core::SC_NS) &&
                  p.qspi_clock_period() == top.bus_clock_period() && p.aes_clock_period() == top.bus_clock_period() &&
                  p.dma().peripheral_cycle() == top.bus_clock_period(),
              "QSPI, AES and DMA0 clocks follow the 200 MHz bus clock");
        auto init = [&] {
            write(csr + h::ERROR_CLEAR, ~0u);
            write(csr + h::CTRL, 0);
            write(csr + h::LATENCY, 0x107);
            write(csr + h::TCSM, 200);   // tCSM <= 1 us at 200 MHz
            write(csr + h::SETUP, 4);
            write(csr + h::HOLD, 4);
            write(csr + h::RECOVERY, 8);
            write(csr + h::APPLY, 1);
            check(wait_for([&] { return !(read(csr + h::STATUS) & h::PENDING); }, sc_core::sc_time(100, sc_core::SC_US)),
                  "HBCTRL: timing applied");
            write(csr + h::CTRL, 1);
            check(read(csr + h::STATUS) & h::READY, "HBCTRL: READY after firmware initialisation");
        };
        init();
        write(mem + 0x100, 0xcafe1234);
        write(mem + 0x1fffffc, 0x0badf00d);
        check(read(mem + 0x100) == 0xcafe1234 && read(mem + 0x1fffffc) == 0x0badf00d,
              "HYPERBUS: 32 MB data window at 0x6000_0000 holds data");
        // HB_RESETN (die 94) follows the HYPERBUS domain reset: a software reset
        // of the controller (SW_RST_N_0[7]) also resets the device, so its
        // memory does not survive (the model zero-fills a reset device).
        sw_reset(0, 7, true);
        wait(top.bus_clock_period() * 4);
        sw_reset(0, 7, false);
        check(!(read(csr + h::STATUS) & h::READY), "HBCTRL: not ready after the HYPERBUS reset");
        init();
        check(read(mem + 0x100) == 0, "HB_RESETN: the HYPERBUS reset also reset the device (memory lost)");
        write(mem + 0x100, 0xcafe1234);
    }

    // T15: AES-256 known answer (FIPS-197 C.3), ADC conversion from the
    // hardmacro stimulus, RNG VALID interrupt and reproducible sequence.
    void security_analog()
    {
        namespace a = fu3::aes::reg;
        const std::uint64_t aes = 0x40000000;
        const std::uint32_t key[8] = {0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f,
                                      0x10111213, 0x14151617, 0x18191a1b, 0x1c1d1e1f};
        const std::uint32_t plain[4] = {0x00112233, 0x44556677, 0x8899aabb, 0xccddeeff};
        const std::uint32_t cipher[4] = {0x8ea2b7ca, 0x516745bf, 0xeafc4990, 0x4b496089};
        write(aes + a::CONFIG, a::ENCRYPT | a::KEY256);
        for (unsigned i = 0; i < 8; ++i)
            write(aes + a::key(i), key[i]);
        write(aes + a::CTRL, a::INIT);
        check(wait_for([&] { return (read(aes + a::STATUS) & a::READY) != 0; }, sc_core::sc_time(20, sc_core::SC_US)),
              "AES: key schedule ready");
        for (unsigned i = 0; i < 4; ++i)
            write(aes + a::block(i), plain[i]);
        write(aes + a::CTRL, a::NEXT);
        check(wait_for([&] { return (read(aes + a::STATUS) & a::VALID) != 0; }, sc_core::sc_time(20, sc_core::SC_US)),
              "AES: result valid");
        bool same = true;
        for (unsigned i = 0; i < 4; ++i)
            same = same && read(aes + a::result(i)) == cipher[i];
        check(same, "AES: FIPS-197 C.3 AES-256 ciphertext");

        namespace d = fu3::adc::reg;
        const std::uint64_t adc = d::BASE;
        p.adc_input().set_input(3, 0x5a5);
        write(adc + d::CFG, 0x10 | 3);
        write(adc + d::TIMING, 0x20040c00);
        write(adc + d::SAMPLE_CFG, 2);
        write(adc + d::CTRL, 1);
        check(wait_for([&] { return (read(adc + d::STATUS) & d::DONE) != 0; }, sc_core::sc_time(200, sc_core::SC_US)),
              "ADC: conversion sequence done");
        check(read(adc + d::sample(0)) == 0x5a5 && read(adc + d::sample(1)) == 0x5a5 && read(adc + d::DATA) == 0x5a5,
              "ADC: samples and average from the channel-3 stimulus");
        write(adc + d::CTRL, 0);

        namespace r = fu3_rng::reg;
        const std::uint64_t rng = fu3_rng::base_address;
        write(rng + r::CONTROL, r::ENABLE);
        write(rng + r::COMMAND, r::NEXT | r::RESTART);
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_RNG - 1].read() && ibex(5); }, sc_core::sc_time(50, sc_core::SC_US)),
              "RNG: VALID -> PLIC ID21 + Ibex irq_fast_i[5]");
        const auto first = read(rng + r::DATA);
        write(rng + r::COMMAND, r::NEXT);
        check(wait_for([&] { return (read(rng + r::STATUS) & r::VALID) != 0; }, sc_core::sc_time(50, sc_core::SC_US)),
              "RNG: next word");
        const auto second = read(rng + r::DATA);
        write(rng + r::COMMAND, r::NEXT | r::RESTART);
        check(wait_for([&] { return (read(rng + r::STATUS) & r::VALID) != 0; }, sc_core::sc_time(50, sc_core::SC_US)),
              "RNG: restarted");
        check(first != second && read(rng + r::DATA) == first, "RNG: seeded source, a restart repeats the sequence");
        p.set_rng_seed(0x1234);
        write(rng + r::COMMAND, r::NEXT | r::RESTART);
        check(wait_for([&] { return (read(rng + r::STATUS) & r::VALID) != 0; }, sc_core::sc_time(50, sc_core::SC_US)) &&
                  read(rng + r::DATA) != first,
              "RNG: another seed gives another sequence");
        write(rng + r::CONTROL, 0);
        write(rng + r::COMMAND, r::RESTART);
        wait(top.bus_clock_period() * 4);
        check(!p.plic_source[FU3_PLIC_ID_RNG - 1].read(), "RNG: RESTART clears VALID and the interrupt");
    }

    // T16: a word from the CVA6 port arrives at the Ibex port, with the receive
    // threshold interrupt to Ibex irq_external_i; and back.
    void mailbox()
    {
        const std::uint64_t cva6 = FU3_MAILBOX_CVA6_BASE, ibex_port = FU3_MAILBOX_IBEX_BASE;
        write(ibex_port + FU3_MAILBOX_IRQ_ENABLE_OFFSET, FU3_MAILBOX_IRQ_RD_THRESHOLD);
        check((read(ibex_port + FU3_MAILBOX_STATUS_OFFSET) & FU3_MAILBOX_STATUS_EMPTY) != 0, "MAILBOX: Ibex side empty");
        write(cva6 + FU3_MAILBOX_FIFO_WR_OFFSET, 0x1234abcd);
        check(wait_for([&] { return p.mailbox_ibex_irq.read() && ibex(fu3_plic::kOutExternal); }, sc_core::sc_time(5, sc_core::SC_US)),
              "MAILBOX: CVA6 -> Ibex message raises Ibex irq_external_i");
        check(read(ibex_port + FU3_MAILBOX_FIFO_RD_OFFSET) == 0x1234abcd, "MAILBOX: Ibex reads the CVA6 word");
        write(ibex_port + FU3_MAILBOX_IRQ_STATUS_OFFSET, FU3_MAILBOX_IRQ_RD_THRESHOLD);   // W1C
        wait(top.bus_clock_period() * 2);
        check(!p.mailbox_ibex_irq.read(), "MAILBOX: W1C clears the Ibex interrupt");
        write(ibex_port + FU3_MAILBOX_FIFO_WR_OFFSET, 0x5a5a0001);
        check(read(cva6 + FU3_MAILBOX_FIFO_RD_OFFSET) == 0x5a5a0001, "MAILBOX: Ibex -> CVA6 word");
        write(ibex_port + FU3_MAILBOX_IRQ_ENABLE_OFFSET, 0);
    }

    // T17: DMA0 memory-to-memory copy in DSRAM with the completion interrupt
    // (PLIC ID5, Ibex irq_fast_i[6]); every endpoint on the channel Common v2.1
    // DMA Mapping allocates it (review T22-R01): SPI0 RX/TX 0/1, UART1 2/3,
    // UART2 4/5, SPI1 6/7, through their request/clear handshakes (VP
    // selector: peripheral ID). Memory copies and error cases use free channels.
    void dma()
    {
        const std::uint64_t dma = 0x44020000, src = 0x01000200, dst = 0x01000300;
        for (unsigned i = 0; i < 4; ++i)
            write(src + 4 * i, 0x10203040u + i);
        auto channel = [&](unsigned ch, std::uint64_t from, std::uint64_t to, unsigned bytes, std::uint32_t rd_cfg,
                           std::uint32_t wr_cfg, std::uint32_t peripheral) {
            const std::uint64_t c = dma + 0x100 * ch;
            write(c + 0x00, std::uint32_t(from));
            write(c + 0x04, std::uint32_t(to));
            write(c + 0x08, bytes);
            write(c + 0x0c, 3);   // SET_INT | LAST
            write(c + 0x10, rd_cfg);
            write(c + 0x14, wr_cfg);
            write(c + 0x20, peripheral);
            write(c + 0xa4, ~0u);
            write(c + 0x44, 1);   // START
        };
        const std::uint32_t inc16 = 0x80000010, fixed1 = 0x00000001, inc1 = 0x80000001;
        channel(0, src, dst, 16, inc16, inc16, 0);
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_DMA0 - 1].read() && ibex(6); }, sc_core::sc_time(50, sc_core::SC_US)),
              "DMA0: completion -> PLIC ID5 + Ibex irq_fast_i[6]");
        bool copied = true;
        for (unsigned i = 0; i < 4; ++i)
            copied = copied && read(dst + 4 * i) == 0x10203040u + i;
        check(copied, "DMA0: DSRAM block copied through the DMA0 master port");
        write(dma + 0xa4, ~0u);
        wait(top.bus_clock_period() * 4);
        check(!p.plic_source[FU3_PLIC_ID_DMA0 - 1].read(), "DMA0: interrupt cleared");

        auto wait_done = [&](unsigned ch) {
            return wait_for([&] { return (read(dma + 0x100 * ch + 0x48) & 3) == 0; }, sc_core::sc_time(200, sc_core::SC_US));
        };
        // Raw status: no read/write SLVERR/DECERR on the channel.
        auto no_error = [&](unsigned ch) { return !(read(dma + 0x100 * ch + 0xa0) & 0x1e); };
        // UART1 transmit by DMA (channel 3, Common v2.1 DMA Mapping): FIFO mode with DMA mode 1 (FCR[3]).
        const std::uint64_t uart1 = 0x440e0000;
        write8(uart1 + FU3_UART_LCR, FU3_UART_DLAB);
        write8(uart1 + FU3_UART_DLL, 1);
        write8(uart1 + FU3_UART_DLM, 0);
        write8(uart1 + FU3_UART_LCR, 0x03);
        write8(uart1 + FU3_UART_FCR, 0x0f);
        const auto before = p.uart_peer(1).received().size();
        write(src, 0x34333231);   // "1234"
        channel(3, src, uart1 + FU3_UART_THR, 4, inc1, fixed1, fu3_peripherals::kDmaIdUart1 << 16);
        check(wait_for([&] { return p.uart_peer(1).received().size() == before + 4; }, sc_core::sc_time(200, sc_core::SC_US)) &&
                  p.uart_peer(1).received().substr(before) == "1234",
              "DMA0 channel 3: memory -> UART1 THR by the TX request handshake");
        check(wait_done(3) && no_error(3), "DMA0 channel 3 (UART1 TX): done, no error");
        write(dma + 0x300 + 0xa4, ~0u);

        auto dsram_bytes = [&](std::uint64_t address, unsigned n) {
            std::string out;
            for (unsigned i = 0; i < n; ++i)
                out += static_cast<char>(top.memory("DSRAM")->peek(address - 0x01000000 + i));
            return out;
        };
        // UART2 transmit (peripheral ID 3).
        select_uart(2, true);
        const std::uint64_t uart2 = 0x44090000;
        write8(uart2 + FU3_UART_LCR, FU3_UART_DLAB);
        write8(uart2 + FU3_UART_DLL, 1);
        write8(uart2 + FU3_UART_DLM, 0);
        write8(uart2 + FU3_UART_LCR, 0x03);
        write8(uart2 + FU3_UART_FCR, 0x0f);   // FIFOs, DMA mode 1, RX trigger 1
        const auto before2 = p.uart_peer(2).received().size();
        write(src, 0x38373635);   // "5678"
        channel(5, src, uart2 + FU3_UART_THR, 4, inc1, fixed1, fu3_peripherals::kDmaIdUart2 << 16);
        check(wait_for([&] { return p.uart_peer(2).received().size() == before2 + 4; }, sc_core::sc_time(200, sc_core::SC_US)) &&
                  p.uart_peer(2).received().substr(before2) == "5678",
              "DMA0 channel 5: memory -> UART2 THR by the TX request handshake");
        check(wait_done(5) && no_error(5), "DMA0 channel 5 (UART2 TX): done, no error");
        write(dma + 0x500 + 0xa4, ~0u);
        // UART1 / UART2 receive (RX request at trigger level 1) on channels 2 / 4.
        const std::uint64_t rx1 = 0x01000340, rx2 = 0x01000350;
        channel(2, uart1 + FU3_UART_RBR, rx1, 4, fixed1, inc1, fu3_peripherals::kDmaIdUart1);
        channel(4, uart2 + FU3_UART_RBR, rx2, 4, fixed1, inc1, fu3_peripherals::kDmaIdUart2);
        p.uart_peer(1).send("wxyz");
        p.uart_peer(2).send("klmn");
        check(wait_done(2) && dsram_bytes(rx1, 4) == "wxyz" && no_error(2),
              "DMA0 channel 2: UART1 RBR -> memory by the RX request handshake, no error");
        check(wait_done(4) && dsram_bytes(rx2, 4) == "klmn" && no_error(4),
              "DMA0 channel 4: UART2 RBR -> memory by the RX request handshake, no error");
        check(!(read8(uart1 + FU3_UART_LSR) & FU3_UART_DR) && !(read8(uart2 + FU3_UART_LSR) & FU3_UART_DR),
              "DMA0: UART1/2 RX FIFOs drained by the DMA");
        write(dma + 0x200 + 0xa4, ~0u);
        write(dma + 0x400 + 0xa4, ~0u);

        // Plan T22: prolonged backpressure. 256 bytes into the 64-byte UART1 TX
        // FIFO at a slow baud: the DMA waits on the TX request for most of the
        // transfer; nothing is lost or reordered, no error, and the bus keeps
        // serving the other masters meanwhile.
        {
            write8(uart1 + FU3_UART_LCR, FU3_UART_DLAB);
            write8(uart1 + FU3_UART_DLL, 4);
            write8(uart1 + FU3_UART_DLM, 0);
            write8(uart1 + FU3_UART_LCR, 0x03);
            write8(uart1 + FU3_UART_FCR, 0x0f);
            const std::uint64_t big = 0x01004000;
            std::string expected;
            for (unsigned n = 0; n < 256; ++n)
                expected += static_cast<char>('!' + n % 90);
            for (unsigned i = 0; i < 64; ++i)
                write(big + 4 * i, std::uint32_t(std::uint8_t(expected[4 * i])) |
                                       std::uint32_t(std::uint8_t(expected[4 * i + 1])) << 8 |
                                       std::uint32_t(std::uint8_t(expected[4 * i + 2])) << 16 |
                                       std::uint32_t(std::uint8_t(expected[4 * i + 3])) << 24);
            const auto start = p.uart_peer(1).received().size();
            channel(3, big, uart1 + FU3_UART_THR, 256, inc1, fixed1, fu3_peripherals::kDmaIdUart1 << 16);
            // Well past the first FIFO load: the channel is still busy, waiting.
            check(wait_for([&] { return p.uart_peer(1).received().size() >= start + 96; }, sc_core::sc_time(20, sc_core::SC_MS)),
                  "DMA0 backpressure: transfer under way");
            const bool busy = (read(dma + 0x300 + 0x48) & 3) != 0;
            const auto t = sc_core::sc_time_stamp();
            const auto probe = read(big);
            const auto latency = sc_core::sc_time_stamp() - t;
            check(busy && probe == read(big) && latency < sc_core::sc_time(2, sc_core::SC_US),
                  "DMA0 backpressure: channel waiting on the FIFO, the bus still answers another master (" +
                      latency.to_string() + ")");
            check(wait_done(3) || wait_for([&] { return (read(dma + 0x300 + 0x48) & 3) == 0; }, sc_core::sc_time(50, sc_core::SC_MS)),
                  "DMA0 backpressure: channel completes");
            check(wait_for([&] { return p.uart_peer(1).received().size() >= start + 256; }, sc_core::sc_time(20, sc_core::SC_MS)) &&
                      p.uart_peer(1).received().substr(start) == expected,
                  "DMA0 backpressure: 256 bytes at the UART1 peer, in order, none lost");
            check(!(read(dma + 0x300 + 0xa0) & 0x1e), "DMA0 backpressure: no error status");
            write(dma + 0x300 + 0xa4, ~0u);
            write8(uart1 + FU3_UART_LCR, FU3_UART_DLAB);
            write8(uart1 + FU3_UART_DLL, 1);
            write8(uart1 + FU3_UART_LCR, 0x03);
        }
        select_uart(2, false);

        // SPI0 / SPI1: transmit and receive together through a board loopback
        // (TX and RX requests at threshold 1, 32-bit data registers).
        const std::uint32_t inc4 = 0x80000004, fixed4 = 0x00000004;
        const std::uint64_t spi_base[2] = {FU3_SPI0_BASE, FU3_SPI1_BASE};
        const unsigned spi_id[2] = {fu3_peripherals::kDmaIdSpi0, fu3_peripherals::kDmaIdSpi1};
        for (unsigned k = 0; k < 2; ++k) {
            const std::string t = k ? "SPI1" : "SPI0";
            const std::uint64_t b = spi_base[k], tx = 0x01000400 + 0x40 * k, rx = tx + 0x20;
            for (unsigned i = 0; i < 4; ++i) {
                write(tx + 4 * i, 0x5a + 0x11 * i + k);
                write(rx + 4 * i, 0);
            }
            select_spi(k, true);
            p.set_spi_loopback(k, true);
            write(b + FU3_SPI_CTRL, 0xe2);
            write(b + FU3_SPI_ISR, 0x7c);
            write(b + FU3_SPI_DIV, 2);
            write(b + FU3_SPI_DMACR, 3 | (1u << 8) | (1u << 16));
            const unsigned tx_ch = k ? 7 : 1, rx_ch = k ? 6 : 0;   // Common v2.1 DMA Mapping
            channel(tx_ch, tx, b + FU3_SPI_TXDR, 16, inc4, fixed4, spi_id[k] << 16);
            channel(rx_ch, b + FU3_SPI_RXDR, rx, 16, fixed4, inc4, spi_id[k]);
            write(b + FU3_SPI_CTRL, 0xe3);
            bool same = wait_done(tx_ch) && wait_done(rx_ch);
            for (unsigned i = 0; i < 4; ++i)
                same = same && (read(rx + 4 * i) & 0xff) == 0x5a + 0x11 * i + k;
            check(same && no_error(tx_ch) && no_error(rx_ch),
                  "DMA0 channels " + std::to_string(tx_ch) + "/" + std::to_string(rx_ch) + ": memory -> " + t + " TXDR and " +
                      t + " RXDR -> memory (loopback), no error");
            write(b + FU3_SPI_CTRL, 0xe2);
            write(b + FU3_SPI_DMACR, 0);
            write(dma + 0x100 * tx_ch + 0xa4, ~0u);
            write(dma + 0x100 * rx_ch + 0xa4, ~0u);
            p.set_spi_loopback(k, false);
            select_spi(k, false);
        }

        // Bus error: a read from unmapped space ends the channel with read_decerr.
        channel(7, 0x08000000, dst, 16, inc16, inc16, 0);
        check(wait_done(7) && (read(dma + 0x700 + 0xa0) & (1u << 3)), "DMA0: unmapped source -> read_decerr, channel stops");
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_DMA0 - 1].read(); }, sc_core::sc_time(5, sc_core::SC_US)),
              "DMA0: the error raises the DMA0 interrupt");
        write(dma + 0x700 + 0xa4, ~0u);

        // FU3_VP_ABI_1 access profile of DMA0 (§5): data memories, XIP read,
        // and the FIFO data registers of its endpoints in their direction only.
        struct denied_case { const char* what; std::uint64_t from, to; std::uint32_t rd, wr; };
        const denied_case denied[] = {
            {"ROM read", 0x00000000, dst, inc16, inc16},
            {"CVA6_ISRAM write", src, 0x00010000, inc16, inc16},
            {"IBEX_ISRAM debug partition read", 0x00060000, dst, inc16, inc16},
            {"CRM read", 0x50010000, dst, 0x80000004, 0x80000004},
            {"UART1 LCR write (not its data register)", src, uart1 + FU3_UART_LCR, inc1, fixed1},
            {"SPI0 RXDR write (wrong direction)", src, FU3_SPI0_BASE + FU3_SPI_RXDR, 0x80000004, 0x00000004},
        };
        for (const auto& d : denied) {
            const auto guard = read(dst);
            channel(7, d.from, d.to, 4, d.rd, d.wr, 0);
            check(wait_done(7) && (read(dma + 0x700 + 0xa0) & 0x6) && read(dst) == guard,
                  std::string("DMA0 profile: ") + d.what + " denied (slave error, nothing written)");
            write(dma + 0x700 + 0xa4, ~0u);
        }
        channel(7, 0x30000000, dst, 8, inc16, inc16, 0);
        check(wait_done(7) && !(read(dma + 0x700 + 0xa0) & 0x1e) && read(dst) == read(0x30000000) &&
                  read(dst + 4) == read(0x30000004),
              "DMA0 profile: QSPI XIP read allowed");
        write(dma + 0x700 + 0xa4, ~0u);

        // Reset in the middle of a copy (SW_RST_N_0[5]): no further writes,
        // registers back to reset, the channel works again after release.
        {
            const std::uint64_t from = 0x01003000, to = 0x01003800;
            for (unsigned i = 0; i < 255; ++i) {
                write(from + 4 * i, 0xabc00000u + i);
                write(to + 4 * i, 0);
            }
            auto landed = [&] {
                unsigned n = 0;
                for (unsigned i = 0; i < 255; ++i)
                    n += top.memory("DSRAM")->peek(to - 0x01000000 + 4 * i) == ((0xabc00000u + i) & 0xff);
                return n;
            };
            channel(0, from, to, 1020, inc16, inc16, 0);
            sw_reset(0, 5, true);
            wait(1, sc_core::SC_US);
            const auto at_reset = landed();
            wait(20, sc_core::SC_US);
            const auto after = landed();
            sw_reset(0, 5, false);
            check(at_reset > 0 && at_reset < 255 && after == at_reset, "DMA0: reset stops a copy in progress (" + std::to_string(at_reset) + "/255)");
            check(read(dma + 0x08) == 0 && (read(dma + 0x48) & 3) == 0 && !p.plic_source[FU3_PLIC_ID_DMA0 - 1].read(),
                  "DMA0: registers at reset values after the reset");
            channel(0, src, dst, 16, inc16, inc16, 0);
            check(wait_done(0) && read(dst) == read(src), "DMA0: channel usable again after the reset");
            write(dma + 0xa4, ~0u);
        }
    }

    // CRM clock gate / software reset of one target (CLK_GATE_x / SW_RST_N_x).
    void clock_gate(unsigned reg, unsigned bit, bool on)
    {
        const std::uint64_t a = kCrm + (reg ? FU3_CRM_CLK_GATE_1_OFFSET : FU3_CRM_CLK_GATE_0_OFFSET);
        const auto v = read(a);
        write(a, on ? v | (1u << bit) : v & ~(1u << bit));
    }
    void sw_reset(unsigned reg, unsigned bit, bool asserted)
    {
        const std::uint64_t a = kCrm + (reg ? FU3_CRM_SW_RST_N_1_OFFSET : FU3_CRM_SW_RST_N_0_OFFSET);
        const auto v = read(a);
        write(a, asserted ? v & ~(1u << bit) : v | (1u << bit));
    }

    // Review R01 / HAS 6.1: the CRM gate stops the IP itself, not only its MMIO.
    void clock_gating()
    {
        const auto bus = top.bus_clock_period();
        // Wiring: each gate bit reaches the clock enable of its model.
        struct gated {
            const char* name;
            unsigned reg, bit;
            std::function<bool()> enabled;
        };
        const gated models[] = {
            {"AP_PLIC", 1, 6, [this] { return p.plic().clock_enabled(); }},
            {"WDT0", 1, 1, [this] { return p.wdt(0).clock_enabled(); }},
            {"WDT1", 1, 2, [this] { return p.wdt(1).clock_enabled(); }},
            {"RNG", 1, 4, [this] { return p.rng().clock_enabled(); }},
            {"I2C", 1, 10, [this] { return p.i2c().clock_enabled(); }},
            {"UART0", 1, 11, [this] { return p.uart(0).clock_enabled(); }},
            {"UART1", 1, 12, [this] { return p.uart(1).clock_enabled(); }},
            {"UART2", 1, 13, [this] { return p.uart(2).clock_enabled(); }},
            {"SPI0", 1, 14, [this] { return p.spi(0).clock_enabled(); }},
            {"SPI1", 1, 15, [this] { return p.spi(1).clock_enabled(); }},
            {"SPI2", 1, 16, [this] { return p.spi(2).clock_enabled(); }},
            {"SPI3", 1, 17, [this] { return p.spi(3).clock_enabled(); }},
            {"HYPERBUS", 0, 5, [this] { return p.hyperram().clock_enabled(); }},
            {"AP_DMA", 0, 7, [this] { return p.dma().clock_enabled(); }},
        };
        for (const auto& m : models) {
            clock_gate(m.reg, m.bit, false);
            const bool off = wait_for([&] { return !m.enabled(); }, bus * 4);
            clock_gate(m.reg, m.bit, true);
            check(off && wait_for([&] { return m.enabled(); }, bus * 4),
                  std::string("gate: CRM bit reaches the clock enable of ") + m.name);
        }

        // UART0: no RX sampling or interrupt while gated, state kept, resumes.
        const std::uint64_t uart0 = 0x440d0000;
        write8(uart0 + FU3_UART_LCR, FU3_UART_DLAB);
        write8(uart0 + FU3_UART_DLL, 1);
        write8(uart0 + FU3_UART_DLM, 0);
        write8(uart0 + FU3_UART_LCR, 0x03);
        write8(uart0 + FU3_UART_FCR, 0x07);
        write8(uart0 + FU3_UART_SCR, 0x77);
        write8(uart0 + FU3_UART_IER, FU3_UART_IER_RX);
        const auto frame = p.uart(0).bit_time() * 10.0;
        clock_gate(1, 11, false);
        p.uart_peer(0).send("G");
        wait(frame * 4);
        check(!p.plic_source[FU3_PLIC_ID_UART0 - 1].read(), "gate UART0: no RX sampling, no interrupt");
        clock_gate(1, 11, true);
        wait(frame);
        check(!(read8(uart0 + FU3_UART_LSR) & FU3_UART_DR) && read8(uart0 + FU3_UART_SCR) == 0x77 &&
                  read8(uart0 + FU3_UART_LCR) == 0x03,
              "gate UART0: the byte sent while gated is not received; registers kept");
        p.uart_peer(0).send("H");
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_UART0 - 1].read(); }, frame * 4) &&
                  read8(uart0 + FU3_UART_RBR) == 'H',
              "gate UART0: receives again once the clock is back");
        // Transmitter paused mid-frame.
        write8(uart0 + FU3_UART_IER, 0);
        const auto sent = p.uart_peer(0).received().size();
        write8(uart0 + FU3_UART_THR, 'Z');
        wait(p.uart(0).bit_time() * 3.0);
        clock_gate(1, 11, false);
        wait(frame * 4);
        const bool held = p.uart_peer(0).received().size() == sent;
        clock_gate(1, 11, true);
        check(held && !(read8(uart0 + FU3_UART_LSR) & FU3_UART_TEMT), "gate UART0: transmission paused mid-frame");
        check(wait_for([&] { return (read8(uart0 + FU3_UART_LSR) & FU3_UART_TEMT) != 0; }, frame * 2),
              "gate UART0: transmission completes after the clock returns");

        // WDT0: the counter holds while gated (no expiry), then continues.
        write(FU3_WDT0_BASE + FU3_WDT_CNT_LD, 200);
        write(FU3_WDT0_BASE + FU3_WDT_CNT_CTRL, FU3_WDT_INTEN);
        wait(bus * 50);
        clock_gate(1, 1, false);
        wait(bus * 1000);
        const bool quiet = !p.wdt0_warning.read();
        clock_gate(1, 1, true);
        const auto value = read(FU3_WDT0_BASE + FU3_WDT_CNT_VAL);
        check(quiet && value > 100 && value < 160, "gate WDT0: no timeout while gated, counter held (" +
                                                       std::to_string(value) + ")");
        check(!wait_for([&] { return p.wdt0_warning.read(); }, bus * 80) &&
                  wait_for([&] { return p.wdt0_warning.read(); }, bus * 200),
              "gate WDT0: expires after the remaining count once the clock returns");
        write(FU3_WDT0_BASE + FU3_WDT_INT_CLR, 1);
        write(FU3_WDT0_BASE + FU3_WDT_CNT_CTRL, 0);

        // RNG: no word produced while gated; the sequence is unchanged.
        {
            namespace r = fu3_rng::reg;
            const std::uint64_t rng = fu3_rng::base_address;
            write(rng + r::CONTROL, r::ENABLE);
            write(rng + r::COMMAND, r::NEXT | r::RESTART);
            check(wait_for([&] { return (read(rng + r::STATUS) & r::VALID) != 0; }, sc_core::sc_time(50, sc_core::SC_US)),
                  "gate RNG: reference word");
            const auto first = read(rng + r::DATA);
            write(rng + r::COMMAND, r::NEXT | r::RESTART);
            clock_gate(1, 4, false);
            wait(50, sc_core::SC_US);
            const bool none = !p.plic_source[FU3_PLIC_ID_RNG - 1].read();
            clock_gate(1, 4, true);
            check(none, "gate RNG: no word while gated");
            check(wait_for([&] { return (read(rng + r::STATUS) & r::VALID) != 0; }, sc_core::sc_time(50, sc_core::SC_US)) &&
                      read(rng + r::DATA) == first,
                  "gate RNG: the word comes once the clock returns, same sequence");
            write(rng + r::CONTROL, 0);
            write(rng + r::COMMAND, r::RESTART);
        }

        // SPI0: a transfer paused mid-way completes with the right data.
        select_spi(0, true);
        p.set_spi_loopback(0, true);
        write(FU3_SPI0_BASE + FU3_SPI_CTRL, 0xe2);
        write(FU3_SPI0_BASE + FU3_SPI_ISR, 0x7c);
        write(FU3_SPI0_BASE + FU3_SPI_DIV, 60);
        write(FU3_SPI0_BASE + FU3_SPI_IER, FU3_SPI_EOT);
        write(FU3_SPI0_BASE + FU3_SPI_TXDR, 0x3c);
        write(FU3_SPI0_BASE + FU3_SPI_CTRL, 0xe3);
        wait(bus * 100);
        clock_gate(1, 14, false);
        wait(50, sc_core::SC_US);
        const bool spi_paused = !p.plic_source[FU3_PLIC_ID_SPI0 - 1].read();
        clock_gate(1, 14, true);
        check(spi_paused, "gate SPI0: no end of transfer while gated");
        check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_SPI0 - 1].read(); }, sc_core::sc_time(50, sc_core::SC_US)) &&
                  read(FU3_SPI0_BASE + FU3_SPI_RXDR) == 0x3c,
              "gate SPI0: the transfer completes after the clock returns, data intact");
        write(FU3_SPI0_BASE + FU3_SPI_ISR, 0x7c);
        write(FU3_SPI0_BASE + FU3_SPI_IER, 0);
        write(FU3_SPI0_BASE + FU3_SPI_CTRL, 0xe2);
        p.set_spi_loopback(0, false);
        select_spi(0, false);

        // I2C: a transfer paused after START completes after the clock returns.
        {
            namespace r = fu3_i2c::reg;
            const std::uint64_t i2c = fu3_i2c::base_address;
            select_i2c(true);
            write(i2c + r::CMD, r::IACK);
            write(i2c + r::TX, 0xa4);
            write(i2c + r::CMD, r::STA | r::STO | r::WR | r::IACK);
            check(wait_for([&] { return !p.i2c_sda(); }, sc_core::sc_time(500, sc_core::SC_US)), "gate I2C: START on the bus");
            clock_gate(1, 10, false);
            wait(500, sc_core::SC_US);
            const bool i2c_paused = !p.plic_source[FU3_PLIC_ID_I2C0 - 1].read() && !p.i2c_sda();
            clock_gate(1, 10, true);
            check(i2c_paused, "gate I2C: no progress while gated (SDA held, no interrupt)");
            check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_I2C0 - 1].read(); }, sc_core::sc_time(500, sc_core::SC_US)),
                  "gate I2C: the transfer completes after the clock returns");
            write(i2c + r::CMD, r::IACK);
            select_i2c(false);
        }

        // PLIC: the gateways capture nothing while gated; a level still high
        // is captured once the clock returns, a pulse ended meanwhile is lost.
        // Sources latched by the earlier sections are claimed first.
        for (unsigned id = 1; id <= FU3_PLIC_SOURCES; ++id)
            write(kPlic + FU3_PLIC_PRIORITY_OFFSET(id), 1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_ENABLE_MASK);
        for (unsigned n = 0; n < FU3_PLIC_SOURCES; ++n) {
            const auto id = read(kPlic + FU3_PLIC_CLAIM_OFFSET);
            if (!id)
                break;
            write(kPlic + FU3_PLIC_CLAIM_OFFSET, id);
        }
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, 0);
        for (unsigned id = 1; id <= FU3_PLIC_SOURCES; ++id)
            write(kPlic + FU3_PLIC_PRIORITY_OFFSET(id), 0);
        check(!(p.plic().pending() & (FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1) | FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER2))),
              "gate AP_PLIC: TIMER1/2 not pending before the test");
        clock_gate(1, 6, false);
        for (const std::uint64_t t : {FU3_TIMER1_BASE, FU3_TIMER2_BASE}) {
            write(t + FU3_TIMER_COMPARE_OFFSET, read(t + FU3_TIMER_COUNT_OFFSET) + 8);   // counts kept from earlier
            write(t + FU3_TIMER_CONTROL_OFFSET, FU3_TIMER_ENABLE);
        }
        wait(bus * 30);
        write(FU3_TIMER1_BASE + FU3_TIMER_CONTROL_OFFSET, 0);
        write(FU3_TIMER2_BASE + FU3_TIMER_CONTROL_OFFSET, 0);
        write(FU3_TIMER2_BASE + FU3_TIMER_COMPARE_OFFSET, 8);   // TIMER2 acknowledged while the PLIC is gated
        wait(bus * 2);
        const bool levels = p.plic_source[FU3_PLIC_ID_TIMER1 - 1].read() && !p.plic_source[FU3_PLIC_ID_TIMER2 - 1].read();
        const auto gated_pending = p.plic().pending();
        clock_gate(1, 6, true);
        wait(bus * 2);
        const auto pending = p.plic().pending();
        check(levels && !(gated_pending & (FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1) | FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER2))),
              "gate AP_PLIC: no source captured while gated");
        check((pending & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1)) && !(pending & FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER2)),
              "gate AP_PLIC: level still high captured after the gate, ended pulse lost");
        write(FU3_TIMER1_BASE + FU3_TIMER_COMPARE_OFFSET, 8);
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER1), 1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1));
        check(read(kPlic + FU3_PLIC_CLAIM_OFFSET) == FU3_PLIC_ID_TIMER1, "gate AP_PLIC: captured source claimable");
        write(kPlic + FU3_PLIC_CLAIM_OFFSET, FU3_PLIC_ID_TIMER1);
        write(kPlic + FU3_PLIC_ENABLE_OFFSET, 0);
        write(kPlic + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER1), 0);

        // HyperRAM: a memory access in flight when the core clock stops
        // completes only after it returns, with the right data.
        {
            const std::uint64_t mem = fu3_hyperram::memory_base + 0x200;
            write(mem, 0x600dcafe);
            const sc_core::sc_time stop = bus * 4, span(2, sc_core::SC_US);
            sc_core::sc_spawn([this, stop, span] {
                wait(stop);
                p.hyperram().set_clock_enabled(false);
                wait(span);
                p.hyperram().set_clock_enabled(true);
            });
            const auto start = sc_core::sc_time_stamp();
            const auto data = read(mem);
            check(data == 0x600dcafe && sc_core::sc_time_stamp() - start >= span,
                  "gate HYPERBUS: an access in flight waits for the core clock, data intact");
        }

        // DMA0: a copy paused by the gate makes no progress, then completes.
        {
            const std::uint64_t dma = 0x44020000, src = 0x01001000, dst = 0x01002000;
            constexpr unsigned words = 255;   // LENGTH is 10 bits (1020 bytes)
            for (unsigned i = 0; i < words; ++i) {
                write(src + 4 * i, 0xc0de0000u + i);
                write(dst + 4 * i, 0);
            }
            auto copied = [&] {
                unsigned n = 0;
                for (unsigned i = 0; i < words; ++i) {
                    std::uint32_t v = 0;
                    for (unsigned b = 0; b < 4; ++b)
                        v |= std::uint32_t(top.memory("DSRAM")->peek(dst - 0x01000000 + 4 * i + b)) << (8 * b);
                    n += v == 0xc0de0000u + i;
                }
                return n;
            };
            const std::uint64_t c = dma + 0x100 * 1;
            write(c + 0x00, std::uint32_t(src));
            write(c + 0x04, std::uint32_t(dst));
            write(c + 0x08, 4 * words);
            write(c + 0x0c, 3);
            write(c + 0x10, 0x80000010);
            write(c + 0x14, 0x80000010);
            write(c + 0x20, 0);
            write(c + 0xa4, ~0u);
            write(c + 0x44, 1);
            clock_gate(0, 7, false);
            wait(1, sc_core::SC_US);   // a burst already on the bus completes
            const auto at_gate = copied();
            wait(20, sc_core::SC_US);
            const auto during = copied();
            const bool no_irq = !p.plic_source[FU3_PLIC_ID_DMA0 - 1].read();
            clock_gate(0, 7, true);
            check(at_gate < words && during == at_gate && no_irq,
                  "gate AP_DMA: copy paused (" + std::to_string(at_gate) + "/" + std::to_string(words) + " words)");
            check(wait_for([&] { return p.plic_source[FU3_PLIC_ID_DMA0 - 1].read(); }, sc_core::sc_time(100, sc_core::SC_US)) &&
                      copied() == words,
                  "gate AP_DMA: copy completes after the clock returns");
            write(c + 0xa4, ~0u);
        }

        // Reset while gated (CRM Spec V1.1 3.1.2): assertion reaches the IP at
        // once, release waits for the destination clock. SPI0's reset is
        // asynchronous; UART0's is synchronous (model contract): the CRM keeps
        // it in reset until the clock returns, so it is reset on that edge.
        write(FU3_SPI0_BASE + FU3_SPI_CTRL, 0xe2);
        clock_gate(1, 14, false);
        sw_reset(1, 14, true);
        sw_reset(1, 14, false);
        clock_gate(1, 14, true);
        check(read(FU3_SPI0_BASE + FU3_SPI_CTRL) == 0xe0, "gate SPI0: a reset pulse while gated resets the IP");
        write8(uart0 + FU3_UART_SCR, 0x55);
        clock_gate(1, 11, false);
        sw_reset(1, 11, true);
        wait(bus * 4);
        clock_gate(1, 11, true);   // still in reset when the clock returns
        wait(bus * 4);
        sw_reset(1, 11, false);
        check(read8(uart0 + FU3_UART_SCR) == 0, "gate UART0: reset held across the gate applies on the first edge");
        write8(uart0 + FU3_UART_SCR, 0x55);
        clock_gate(1, 11, false);
        sw_reset(1, 11, true);
        wait(bus * 4);
        sw_reset(1, 11, false);
        wait(bus * 4);
        check(!p.uart(0).clock_enabled() && top.domain_reset_n("UART0") == false,
              "gate UART0: SW_RST released while gated, the destination reset is still held (no clock)");
        clock_gate(1, 11, true);
        check(read8(uart0 + FU3_UART_SCR) == 0,
              "gate UART0: a reset pulse inside the gate still resets the IP when the clock returns");
    }

    void run()
    {
        wait_ready();
        plic_intmap();
        timers();
        watchdogs();
        vp_abi();
        uarts();
        serial();
        gpios();
        qspi();
        hyperram();
        security_analog();
        mailbox();
        dma();
        clock_gating();
        check(p.pinmux().contentions() == 0, "pads: no board/chip contention in the whole run");
        // A board driver against an enabled chip output: the chip level wins
        // and the contention is counted (and reported once).
        write(0x50040000 + fu3::gpio::reg::PADDIR, 1u << 6);   // GPIOB_6 on die 46 (b00)
        write(0x50040000 + fu3::gpio::reg::PADOUT, 1u << 6);
        p.set_board_pad(46, false);
        wait(top.bus_clock_period() * 4);
        check(p.pad_level(46) && p.pinmux().contentions() > 0, "pads: chip output wins a board contention, counted");
        p.release_board_pad(46);
        write(0x50040000 + fu3::gpio::reg::PADDIR, 0);
        std::cout << "fu3_soc peripherals: " << checks << " checks, " << failures << " failures -> "
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
    bench b("bench");
    sc_core::sc_start(100, sc_core::SC_MS);
    if (!b.finished)
        std::cerr << "FAIL: the bench did not run to its end (simulation starved or timed out at "
                  << sc_core::sc_time_stamp() << ")\n";
    return b.failures || !b.checks || !b.finished ? 1 : 0;
}
