#include "mini_soc_top.h"

#include <cstdint>
#include <iostream>

#include <bus_router.h>
#include <edu_timer_tlm.h>
#include <memory_tlm.h>
#include <riscv_vp_wrapper.h>
#include <uart.h>

namespace tutorial {
namespace {

constexpr std::uint64_t kRamBase = 0x8000'0000;
constexpr std::uint64_t kRamSize = 0x0001'0000;
constexpr std::uint64_t kUartBase = 0x1000'0000;
constexpr std::uint64_t kTimerBase = 0x1001'0000;
constexpr std::uint64_t kPeripheralSize = 0x1000;
constexpr unsigned kMachineTimerCause = 7;

} // namespace

struct mini_soc_top::impl : public sc_core::sc_module {
    cdc::cpu::riscv_vp_cpu cpu;
    cdc::components::bus_router bus;
    cdc::components::memory_tlm ram;
    UartTLM uart;
    edu_timer_tlm timer;

    sc_core::sc_buffer<unsigned char> uart_tx;
    sc_core::sc_signal<bool> uart_irq;
    sc_core::sc_signal<bool> timer_irq;
    sc_core::sc_signal<bool> timer_reset_n;
    sc_core::sc_trace_file* trace_file = nullptr;

    SC_HAS_PROCESS(impl);
    impl(sc_core::sc_module_name name, const std::string& config_path)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , bus("bus", 3, cpu.has_unified_bus() ? 1u : 2u)
        , ram("ram", kRamSize)
        , uart("uart")
        , timer("timer", sc_core::sc_time(20, sc_core::SC_US),
                sc_core::sc_time(10, sc_core::SC_NS))
        , uart_tx("uart_tx")
        , uart_irq("uart_irq")
        , timer_irq("timer_irq")
        , timer_reset_n("timer_reset_n")
    {
        if (cpu.has_unified_bus()) {
            cpu.data_bus().bind(bus.cpu_port(0));
        } else {
            cpu.instr_bus().bind(bus.cpu_port(0));
            cpu.data_bus().bind(bus.cpu_port(1));
        }

        bus.add_target(kRamBase, kRamSize).bind(ram.socket);
        bus.add_target(kUartBase, kPeripheralSize).bind(uart.bus);
        bus.add_target(kTimerBase, kPeripheralSize).bind(timer.socket);

        uart.tx(uart_tx);
        uart.irq(uart_irq);
        timer.reset_n(timer_reset_n);
        timer.irq(timer_irq);
        timer_reset_n.write(true);

        // This compact teaching platform uses the same direct MTIP bridge as
        // CDC-VP's riscv_cpu_eval.  A production peripheral IRQ would normally
        // enter a PLIC and reach the CPU as machine-external interrupt (MEIP).
        SC_METHOD(update_timer_irq);
        sensitive << timer_irq;
        dont_initialize();

        SC_METHOD(monitor_uart_tx);
        sensitive << uart_tx;
        dont_initialize();

        std::cout << "mini_soc_edu_timer config: " << config_path << '\n';
        std::cout << "cpu backend: " << cpu.backend_name() << '\n';
    }

    ~impl() override
    {
        if (trace_file != nullptr) {
            sc_core::sc_close_vcd_trace_file(trace_file);
        }
    }

    void open_trace(const std::string& path)
    {
        // sc_create_vcd_trace_file() appends its own ".vcd" suffix.
        std::string stem = path;
        const std::string suffix = ".vcd";
        if (stem.size() > suffix.size() &&
            stem.compare(stem.size() - suffix.size(), suffix.size(),
                         suffix) == 0) {
            stem.erase(stem.size() - suffix.size());
        }
        trace_file = sc_core::sc_create_vcd_trace_file(stem.c_str());
        trace_file->set_time_unit(1, sc_core::SC_NS);
        sc_core::sc_trace(trace_file, timer_irq, "timer_irq");
        sc_core::sc_trace(trace_file, timer_reset_n, "timer_reset_n");
        sc_core::sc_trace(trace_file, uart_irq, "uart_irq");
        sc_core::sc_trace(trace_file, uart_tx, "uart_tx");
        std::cout << "SIM: vcd trace -> " << stem << ".vcd\n";
    }

    void update_timer_irq()
    {
        cpu.set_irq(kMachineTimerCause, timer_irq.read());
    }

    void monitor_uart_tx()
    {
        std::cout << static_cast<char>(uart_tx.read());
        std::cout.flush();
    }
};

mini_soc_top::mini_soc_top(sc_core::sc_module_name name, std::string config_path)
    : sc_core::sc_module(name)
    , impl_(std::make_unique<impl>("impl", config_path))
{
}

mini_soc_top::~mini_soc_top() = default;

void mini_soc_top::load_firmware(const std::string& path)
{
    impl_->cpu.load_elf(path);
}

void mini_soc_top::enable_tracing(const std::string& path)
{
    impl_->open_trace(path);
}

std::string mini_soc_top::cpu_backend_name() const
{
    return impl_->cpu.backend_name();
}

std::uint64_t mini_soc_top::retired_instructions() const
{
    return impl_->cpu.get_instret();
}

} // namespace tutorial
