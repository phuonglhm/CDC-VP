#include "fx1_soc_top.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <bus/bus_system.h>
#include <cdc/cpu/elf_loader.h>
#include <dma/dma.h>
#include <fx1/clint.h>
#include <fx1/cpu_port_adapter.h>
#include <fx1/exclusive_monitor.h>
#include <fx1/fx1_memory_map.h>
#include <fx1/plic.h>
#include <fx1/sim_control.h>
#include <fx1/sparse_ram.h>
#include <fx1/word_access_guard.h>
#include <fx1_isp/fx1_isp_tlm.h>
#include <i2c.h>
#include <riscv_vp_plusplus_wrapper.h>
#include <uart.h>
#include <uart_host_bridge.h>
#ifdef FX1_SOC_HAS_CAMERA
#include <fx1_csi2/csi2_rx_controller.h>
#include <fx1_csi2_bfm/dphy_tx_bfm.h>
#include <fx1_csi2_phy/dphy_rx_wrapper.h>
#include <fx1_csi2_sensor/ccs_sensor.h>
#endif

namespace cdc::platforms::fx1_soc {
namespace {

// RISC-V machine interrupt causes (mip bit numbers).
constexpr unsigned kCauseMachineSoftware = 3;
constexpr unsigned kCauseMachineTimer = 7;
constexpr unsigned kCauseMachineExternal = 11;

// SYS_DMA, ISP and CSI-2 RX reset pulse at power-up (active low), in ns. Not
// a namespace-scope sc_time: main() sets the time resolution (1 fs) first.
constexpr double kResetPulseNs = 20;

// Exclusive-monitor master IDs of the non-CPU writers (harts use 0..N-1).
constexpr unsigned kMasterSysDma = 0x100;
constexpr unsigned kMasterIspOdma = 0x101;
constexpr unsigned kMasterMipiDma = 0x102;

// I2C0: functional host at fast-mode 400 kHz; the camera sensor answers at
// 7-bit address 0x10 (CcsSensorConfig default).
constexpr double kI2cSclHz = 400e3;
constexpr unsigned kSensorI2cAddr = 0x10;

// Bus target names (bus::BusConfig::fx1()) and the platform-only finisher.
constexpr const char* kSimCtrlTarget = "SIM_CTRL";
const char* const kHartPort[FX1_NUM_HARTS] = {"CPU1", "CPU2"};

// Level-sensitive interrupt line -> cpu_base::set_irq(cause, level).
struct irq_forward : sc_core::sc_module {
    sc_core::sc_in<bool> in{"in"};
    cdc::cpu::cpu_base& cpu;
    unsigned cause;

    SC_HAS_PROCESS(irq_forward);
    irq_forward(sc_core::sc_module_name name, cdc::cpu::cpu_base& target, unsigned irq_cause)
        : sc_module(name), cpu(target), cause(irq_cause)
    {
        SC_METHOD(forward);
        sensitive << in;
        dont_initialize();
    }
    void forward() { cpu.set_irq(cause, in.read()); }
};

// Holds a hart's bus accesses until the DDR3 subsystem releases the CPUs (the
// DDR3 preinit runs first; plan §5.3 order scrub -> preload -> CPU). Debug
// accesses pass at once. A pass-through after the release.
struct release_gate : sc_core::sc_module {
    tlm_utils::simple_target_socket<release_gate> target{"target"};
    tlm_utils::simple_initiator_socket<release_gate> out{"out"};
    const Ddr3Subsystem& ddr;

    release_gate(sc_core::sc_module_name name, const Ddr3Subsystem& subsystem)
        : sc_module(name), ddr(subsystem)
    {
        target.register_b_transport(this, &release_gate::b_transport);
        target.register_transport_dbg(this, &release_gate::transport_dbg);
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        if (!ddr.released()) sc_core::wait(ddr.release_event());
        out->b_transport(tx, delay);
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& tx) { return out->transport_dbg(tx); }
};

bool in_window(std::uint64_t address, std::uint64_t length, std::uint64_t base, std::uint64_t size)
{
    return address >= base && length <= size && address - base <= size - length;
}

bus::BusConfig make_bus_config(bool camera)
{
    auto cfg = bus::BusConfig::fx1();
    for (auto& target : cfg.targets)
        if (target.name == "SYS_DMA_CSR" || target.name == "ISP_CSR" || target.name == "SRAM" ||
            target.name == "DDR3_CSR" || target.name == "DDR_SYSCTRL" || target.name == "I2C0" ||
            (camera && target.name == "MIPI_CSR"))
            target.enabled = true;  // instantiated below (G3, G4, DDR3 subtask 5, CSI-2 M5)
    cfg.targets.push_back({kSimCtrlTarget, FX1_SIM_CTRL_BASE, FX1_APB_SLOT_SIZE,
                           bus::TargetPath::Peribus0Apb, true});
    return cfg;
}

i2c_config make_i2c_config()
{
    i2c_config c;
    c.functional_host = true;  // FDATA commands run on the bus with SCL timing
    c.scl_hz = kI2cSclHz;
    return c;
}

#ifdef FX1_SOC_HAS_CAMERA
// --camera: CSI-2 RX controller with its detailed D-PHY RX, and the board's
// camera, a CCS register model on I2C0 driving the D-PHY transmit BFM
// (components/FX1_Components/mipi-csi2). The DMA writes through a write guard
// like the other non-CPU writers (plan C7).
struct camera : sc_core::sc_module {
    fx1::WriteGuard dma_guard;
    fx1::csi2::DphyRxWrapper phy;
    fx1::csi2::Csi2RxController ctl;
    fx1::csi2::bfm::DphyTxBfm tx;
    fx1::csi2::CcsSensor sensor;

    static fx1::csi2::CcsSensorConfig sensor_config(const std::string& raw_file)
    {
        fx1::csi2::CcsSensorConfig c;
        c.i2c_addr = kSensorI2cAddr;
        c.raw_file = raw_file;
        c.continuous_clock = false;  // P16: clock lane in LP-11 between frames (host cost of the PHY)
        return c;
    }
    camera(sc_core::sc_module_name name, fx1::ExclusiveMonitor& monitor, const std::string& raw_file)
        : sc_module(name)
        , dma_guard("mipi_dma_guard", monitor, kMasterMipiDma)
        , phy("dphy_rx")
        , ctl("csi2_rx", phy)
        , tx("sensor_tx", phy.clkp, phy.clkn, phy.dp, phy.dn)
        , sensor("sensor", tx, sensor_config(raw_file))
    {
        ctl.dma_socket.bind(dma_guard.target);
    }
};
#endif

} // namespace

struct fx1_soc_top::impl : sc_core::sc_module {
    fx1_soc_options options;
    bus::BusSystem fabric;
    fx1::SparseRam bootrom;
    fx1::SparseRam sram;
    // Replaces the former SparseRam DDR: every DDR access goes through the
    // controller (bridge -> AXI port 0), no backdoor on the normal path.
    Ddr3Subsystem ddr3;
    fx1::Clint clint;
    fx1::Plic plic;
    // uart2_tlm always copies 4 bytes; the guard turns any other access into a
    // guest access fault before it can overrun the payload buffer.
    fx1::WordAccessGuard uart_guard;
    UartTLM uart;
    cdc::components::uart_host_bridge uart_host; // host -> UART RX (socket / replay)
    fx1::SimControl sim;
    // Plan C7: one monitor for the address space. Declared before the CPUs and
    // the guards that refer to it, so it outlives them.
    fx1::ExclusiveMonitor monitor;
    fx1::WriteGuard sys_dma_guard;
    fx1::WriteGuard isp_odma_guard;
    fx1::dma::Dma sys_dma;
    cdc::components::fx1_isp::fx1_isp_tlm isp;
    // I2C0 (always present; no device on the bus without --camera). The model
    // copies up to 4 bytes: the guard faults any non-word access, as for UART.
    fx1::WordAccessGuard i2c_guard;
    i2c i2c0;
#ifdef FX1_SOC_HAS_CAMERA
    std::unique_ptr<camera> cam;  // --camera only
#endif

    sc_core::sc_buffer<unsigned char> uart_tx{"uart_tx"};
    // PLIC source lines, index = source ID - 1.
    sc_core::sc_vector<sc_core::sc_signal<bool>> irq_line{"irq_line", FX1_PLIC_NUM_SOURCES - 1};
    sc_core::sc_vector<sc_core::sc_signal<bool>> msip{"msip", FX1_NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> mtip{"mtip", FX1_NUM_HARTS};
    sc_core::sc_vector<sc_core::sc_signal<bool>> meip{"meip", FX1_NUM_HARTS};
    // SYS_DMA side band. No peripheral handshake partner exists yet, so the
    // request inputs stay 0 (M2M only); each clear output drives its own signal.
    sc_core::sc_signal<bool> dma_reset_n{"dma_reset_n"};
    sc_core::sc_signal<std::uint32_t> dma_rx_request{"dma_rx_request"}, dma_tx_request{"dma_tx_request"};
    sc_core::sc_signal<std::uint32_t> dma_rx_clear{"dma_rx_clear"}, dma_tx_clear{"dma_tx_clear"};
    sc_core::sc_signal<bool> isp_rst_n{"isp_rst_n"};
    sc_core::sc_signal<bool> csi_rst_n{"csi_rst_n"};

    std::vector<std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu>> cpus;
    std::vector<std::unique_ptr<fx1::CpuPortAdapter>> cpu_ports;
    std::vector<std::unique_ptr<release_gate>> gates;
    std::vector<std::unique_ptr<irq_forward>> forwards;
    std::ofstream uart_log;

    SC_HAS_PROCESS(impl);
    impl(sc_core::sc_module_name name, const fx1_soc_options& opts)
        : sc_module(name)
        , options(opts)
        , fabric("fabric", make_bus_config(opts.camera), opts.bus_trace)
        , bootrom("bootrom", FX1_BOOTROM_SIZE, sc_core::sc_time(10, sc_core::SC_NS), true)
        , sram("sram", FX1_SRAM_SIZE, sc_core::sc_time(10, sc_core::SC_NS))
        , ddr3("ddr3_subsystem", Ddr3Subsystem::Options{opts.ddr_boot, opts.ddr3_ecc, opts.ddr3_faults})
        , clint("clint", FX1_NUM_HARTS, FX1_MTIME_HZ)
        , plic("plic", FX1_PLIC_NUM_SOURCES, FX1_NUM_HARTS, FX1_PLIC_PRIORITY_MAX)
        , uart_guard("uart_guard")
        , uart("uart")
        , uart_host("uart_host", sc_core::sc_time(static_cast<double>(opts.uart_rx_byte_us), sc_core::SC_US))
        , sim("sim_control", true, FX1_SIM_CTRL_IRQ_LINES)
        , monitor(FX1_NUM_HARTS, FX1_RESERVATION_GRANULE)
        , sys_dma_guard("sys_dma_guard", monitor, kMasterSysDma)
        , isp_odma_guard("isp_odma_guard", monitor, kMasterIspOdma)
        , sys_dma("sys_dma")
        , isp("isp")
        , i2c_guard("i2c_guard")
        , i2c0("i2c0", make_i2c_config())
    {
        if (options.firmware.empty())
            throw std::invalid_argument("fx1_soc: a firmware ELF is required");
        if (!options.uart_log.empty()) {
            uart_log.open(options.uart_log);
            if (!uart_log) throw std::runtime_error("fx1_soc: cannot open " + options.uart_log);
        }

        fabric.target("BootROM").bind(bootrom.socket);
        fabric.target("SRAM").bind(sram.socket);
        fabric.target("MEMCTL_DDR").bind(ddr3.mem_target());
        fabric.target("DDR3_CSR").bind(ddr3.csr_target());
        fabric.target("DDR_SYSCTRL").bind(ddr3.sysctrl_target());
        fabric.target("CLINT").bind(clint.socket);
        fabric.target("PLIC").bind(plic.socket);
        fabric.target("UART").bind(uart_guard.target);
        uart_guard.out.bind(uart.bus);
        fabric.target(kSimCtrlTarget).bind(sim.socket);
        fabric.target("SYS_DMA_CSR").bind(sys_dma.target_socket);
        // Non-CPU writers reach the fabric through a write guard (plan C7);
        // with the monitor off the guard is still bound but never refuses.
        sys_dma.master_socket.bind(sys_dma_guard.target);
        sys_dma_guard.out.bind(fabric.initiator("SYS_DMA"));
        sys_dma.reset_n(dma_reset_n);
        sys_dma.rx_request(dma_rx_request);
        sys_dma.tx_request(dma_tx_request);
        sys_dma.rx_clear(dma_rx_clear);
        sys_dma.tx_clear(dma_tx_clear);
        // ISP: CSR on PERIBUS_1 (local offsets), two DMA masters, level IRQ.
        fabric.target("ISP_CSR").bind(isp.csr_socket);
        isp.idma_socket.bind(fabric.initiator("ISP_IDMA"));
        isp.odma_socket.bind(isp_odma_guard.target); // IDMA only reads
        isp_odma_guard.out.bind(fabric.initiator("ISP_ODMA"));
        isp.rst_n(isp_rst_n);
        fabric.target("I2C0").bind(i2c_guard.target);
        i2c_guard.out.bind(i2c0.socket);
        if (options.camera) {
#ifdef FX1_SOC_HAS_CAMERA
            // CSR on PERIBUS_1 (local offsets), DMA as MIPI_DMA, csi2_irq on
            // PLIC source 7, sensor control on I2C0. Without --camera MIPI_CSR
            // stays disabled (DECERR) and source 7 undriven.
            cam = std::make_unique<camera>("camera", monitor, options.camera_raw);
            fabric.target("MIPI_CSR").bind(cam->ctl.csr_socket);
            cam->dma_guard.out.bind(fabric.initiator("MIPI_DMA"));
            cam->ctl.resetn(csi_rst_n);
            i2c0.attach(kSensorI2cAddr, &cam->sensor);
#else
            throw std::invalid_argument("fx1_soc: --camera: this build has no camera "
                                        "(configure with -DFX1_BUILD_MIPI_CSI2=ON)");
#endif
        }
        uart.tx(uart_tx);
        uart_host.rx_out(uart.rx);
        uart_host.tx_in(uart_tx);
        if (!options.uart_rx_file.empty())
            uart_host.replay_file(options.uart_rx_file,
                                  sc_core::sc_time(static_cast<double>(options.uart_rx_delay_us), sc_core::SC_US));
        if (options.uart_socket >= 0)
            uart_host.listen_on(static_cast<std::uint16_t>(options.uart_socket), options.uart_wait);
        // Host console input starts with the CPUs (the RX delay counts from the release).
        if (!ddr3.released()) uart_host.hold_until(ddr3.release_event());

        // Interrupt sources (fx1_memory_map.h): 1 UART, 2 SYS_DMA, 3 ISP;
        // 4..6 are the VP-only test lines; 7 CSI-2 RX (--camera); 8 DDR3; 9 I2C0.
        for (unsigned id = 1; id < FX1_PLIC_NUM_SOURCES; ++id) plic.irq_in[id - 1](irq_line[id - 1]);
        uart.irq(irq_line[FX1_IRQ_UART - 1]);
        sys_dma.irq(irq_line[FX1_IRQ_SYS_DMA - 1]);
        isp.irq(irq_line[FX1_IRQ_ISP - 1]);
        for (unsigned i = 0; i < FX1_SIM_CTRL_IRQ_LINES; ++i)
            sim.test_irq[i](irq_line[FX1_SIM_CTRL_IRQ_FIRST_SOURCE - 1 + i]);
        ddr3.irq()(irq_line[FX1_IRQ_DDR - 1]);
        i2c0.irq(irq_line[FX1_IRQ_I2C0 - 1]);
#ifdef FX1_SOC_HAS_CAMERA
        if (cam) cam->ctl.irq(irq_line[FX1_IRQ_MIPI - 1]);
#endif

        // The platform loads the firmware image itself (not the hart's own
        // loader): BootROM and SRAM segments now, DDR segments staged for the
        // DDR3 preinit (refused in firmware boot). Every hart resets at the
        // ELF entry.
        image_sink sink{this};
        const std::uint64_t entry = cdc::cpu::load_elf(sink, options.firmware);

        // The monitor compares DDR storage, not bus names: region aliases share
        // reservations and brackets, and a controller reset, reinitialization or
        // remap drops every DDR reservation (review S5-R2/R3).
        monitor.set_canonicalizer([this](std::uint64_t lo, std::uint64_t hi,
                                         std::vector<fx1::ExclusiveMonitor::Range>& out) {
            ddr3.canonical_ranges(lo, hi, out);
        });
        ddr3.controller().set_storage_listener([this] {
            monitor.invalidate(Ddr3Subsystem::kCanonicalBase,
                               Ddr3Subsystem::kCanonicalBase + Ddr3Subsystem::kCanonicalSize);
            monitor.invalidate(FX1_DDR_BASE, std::uint64_t{FX1_DDR_BASE} + FX1_DDR_SIZE);
        });

        // One shared lock for LR/SC and AMO: both harts share the address space.
        auto bus_lock = cdc::cpu::make_shared_bus_lock();
        cdc::cpu::riscv_vp_plusplus_options cpu_options;
        cpu_options.disable_extensions = "DV";  // FX1 CVA6 configuration: RV32IMAFC
        cpu_options.mtime_source = [this] { return clint.mtime(); };
        if (options.exclusive_monitor) cpu_options.exclusive_monitor = &monitor;
        for (unsigned hart = 0; hart < FX1_NUM_HARTS; ++hart) {
            cdc::cpu::cpu_config cfg;
            cfg.xlen = 32;
            cfg.hart_id = hart;
            cfg.reset_pc = entry;
            auto cpu = std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>(
                sc_core::sc_gen_unique_name("cpu"), cfg, cpu_options);
            cpu->attach_bus_lock(bus_lock);
            // Unified I/D socket -> payload completion + guest-fault mapping ->
            // CPU release gate -> bus port.
            auto port = std::make_unique<fx1::CpuPortAdapter>(sc_core::sc_gen_unique_name("cpu_port"));
            auto gate = std::make_unique<release_gate>(sc_core::sc_gen_unique_name("cpu_gate"), ddr3);
            cpu->data_bus().bind(port->target);
            port->out.bind(gate->target);
            gate->out.bind(fabric.initiator(kHartPort[hart]));
            cpu_ports.push_back(std::move(port));
            gates.push_back(std::move(gate));

            clint.msip_irq[hart](msip[hart]);
            clint.mtip_irq[hart](mtip[hart]);
            plic.eip[FX1_PLIC_CONTEXT_HART_M(hart)](meip[hart]);
            add_forward(*cpu, msip[hart], kCauseMachineSoftware);
            add_forward(*cpu, mtip[hart], kCauseMachineTimer);
            add_forward(*cpu, meip[hart], kCauseMachineExternal);
            cpus.push_back(std::move(cpu));
        }

        SC_METHOD(on_uart_tx);
        sensitive << uart_tx;
        dont_initialize();
        SC_THREAD(power_on_reset);
        if (options.realtime) SC_THREAD(pace_to_wall_clock);
    }

    // Hold SYS_DMA, the ISP and the CSI-2 RX in reset for a short pulse after
    // power-up, then release them.
    void power_on_reset()
    {
        dma_reset_n.write(false);
        isp_rst_n.write(false);
        csi_rst_n.write(false);
        wait(sc_core::sc_time(kResetPulseNs, sc_core::SC_NS));
        dma_reset_n.write(true);
        isp_rst_n.write(true);
        csi_rst_n.write(true);
    }

    // Receives the PT_LOAD segments of the firmware ELF (cdc::cpu::load_elf
    // calls `sink->transport_dbg`).
    struct image_sink {
        impl* self;
        image_sink* operator->() { return this; }
        unsigned transport_dbg(tlm::tlm_generic_payload& tx)
        {
            self->place_segment(tx.get_address(), tx.get_data_ptr(), tx.get_data_length());
            return tx.get_data_length();
        }
    };

    void place_segment(std::uint64_t address, const std::uint8_t* data, std::size_t length)
    {
        const std::string what = "ELF segment of " + options.firmware;
        if (in_window(address, length, FX1_BOOTROM_BASE, FX1_BOOTROM_SIZE))
            bootrom.storage().write(address - FX1_BOOTROM_BASE, data, length);
        else if (in_window(address, length, FX1_SRAM_BASE, FX1_SRAM_SIZE))
            sram.storage().write(address - FX1_SRAM_BASE, data, length);
        else if (in_window(address, length, FX1_DDR_BASE, FX1_DDR_SIZE))
            ddr3.stage(address, data, length, what);
        else {
            std::ostringstream message;
            message << "fx1_soc: " << what << " at 0x" << std::hex << address << " (+0x" << length
                    << ") is not inside BootROM, SRAM or DDR";
            throw std::runtime_error(message.str());
        }
    }

    // --realtime: every millisecond of simulated time, sleep the host until
    // wall-clock time has caught up. A simulation slower than real time is
    // never slowed further. Pacing starts with the CPUs (after a DDR3 preinit).
    void pace_to_wall_clock()
    {
        if (!ddr3.released()) wait(ddr3.release_event());
        const auto start = std::chrono::steady_clock::now();
        const sc_core::sc_time origin = sc_core::sc_time_stamp();
        for (;;) {
            wait(sc_core::sc_time(1, sc_core::SC_MS));
            const auto due = start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                         std::chrono::duration<double>((sc_core::sc_time_stamp() - origin).to_seconds()));
            const auto now = std::chrono::steady_clock::now();
            if (due > now) std::this_thread::sleep_for(due - now);
        }
    }

    void add_forward(cdc::cpu::cpu_base& cpu, sc_core::sc_signal<bool>& line, unsigned cause)
    {
        auto forward = std::make_unique<irq_forward>(sc_core::sc_gen_unique_name("irq_fwd"), cpu, cause);
        forward->in(line);
        forwards.push_back(std::move(forward));
    }

    std::uint64_t camera_writes_held() const
    {
#ifdef FX1_SOC_HAS_CAMERA
        if (cam) return cam->dma_guard.writes_held();
#endif
        return 0;
    }

    void on_uart_tx()
    {
        const char c = static_cast<char>(uart_tx.read());
        if (options.uart_stdout) std::cout << c << std::flush;
        if (uart_log.is_open()) uart_log << c << std::flush;
    }
};

fx1_soc_top::fx1_soc_top(sc_core::sc_module_name name, const fx1_soc_options& options)
    : sc_module(name)
    , impl_(std::make_unique<impl>("impl", options))
{
}

fx1_soc_top::~fx1_soc_top() = default;

fx1_soc_top::result fx1_soc_top::outcome() const
{
    switch (impl_->sim.result()) {
    case fx1::SimControl::Result::pass: return result::pass;
    case fx1::SimControl::Result::fail: return result::fail;
    default: return result::running;
    }
}

std::uint32_t fx1_soc_top::fail_code() const { return impl_->sim.fail_code(); }
unsigned fx1_soc_top::harts() const { return static_cast<unsigned>(impl_->cpus.size()); }
std::uint64_t fx1_soc_top::instret(unsigned hart) const { return impl_->cpus.at(hart)->get_instret(); }
std::uint64_t fx1_soc_top::pc(unsigned hart) const { return impl_->cpus.at(hart)->get_pc(); }

void fx1_soc_top::dump_ddr(std::uint64_t address, std::uint64_t length, const std::string& path) const
{
    if (!in_window(address, length, FX1_DDR_BASE, FX1_DDR_SIZE))
        throw std::out_of_range("fx1_soc: dump range is outside DDR");
    std::vector<std::uint8_t> bytes(length);
    impl_->ddr3.dump(address, bytes.data(), bytes.size());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("fx1_soc: cannot write " + path);
}

void fx1_soc_top::load_ddr(std::uint64_t address, const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("fx1_soc: cannot read " + path);
    const auto size = static_cast<std::uint64_t>(in.tellg());
    if (!in_window(address, size, FX1_DDR_BASE, FX1_DDR_SIZE))
        throw std::out_of_range("fx1_soc: --load " + path + " does not fit in DDR at that address");
    std::vector<std::uint8_t> bytes(size);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    if (!in) throw std::runtime_error("fx1_soc: short read of " + path);
    impl_->ddr3.stage(address, bytes.data(), bytes.size(), "--load " + path);
}

void fx1_soc_top::run_until_cpu_release()
{
    if (impl_->ddr3.released()) return;
    // The preinit thread pauses the simulation when it releases the CPUs, or
    // stops it on failure.
    sc_core::sc_start();
    if (!impl_->ddr3.released()) {
        const auto& error = impl_->ddr3.startup_error();
        throw std::runtime_error(error.empty() ? "fx1_soc: the simulation ended before the CPUs were released"
                                               : "fx1_soc: " + error);
    }
}

sc_core::sc_time fx1_soc_top::cpu_release_time() const { return impl_->ddr3.release_time(); }
std::string fx1_soc_top::ddr3_banner() const { return impl_->ddr3.banner(); }
std::string fx1_soc_top::ddr3_report() const { return impl_->ddr3.report(); }

std::string fx1_soc_top::camera_banner() const
{
#ifdef FX1_SOC_HAS_CAMERA
    if (impl_->cam) {
        std::ostringstream out;
        out << "CSI-2 RX at 0x" << std::hex << FX1_MIPI_CSR_BASE << std::dec << " (IRQ " << FX1_IRQ_MIPI
            << ", DMA via MIPI_DMA), CCS sensor at I2C0 address 0x" << std::hex << kSensorI2cAddr << std::dec
            << ", frames from "
            << (impl_->options.camera_raw.empty() ? std::string("the built-in pattern") : impl_->options.camera_raw);
        return out.str();
    }
#endif
    return {};
}

bool fx1_soc_top::camera_built() noexcept
{
#ifdef FX1_SOC_HAS_CAMERA
    return true;
#else
    return false;
#endif
}

std::string fx1_soc_top::atomics_report() const
{
    if (!impl_->options.exclusive_monitor) return "off (upstream VP++ bus-lock model)";
    const auto& s = impl_->monitor.stats();
    std::ostringstream out;
    out << "granule " << impl_->monitor.granule() << " B, lr " << s.reservations << ", sc ok "
        << s.sc_succeeded << " fail " << s.sc_failed << ", cancelled by hart " << s.cancelled_by_cpu
        << " by device " << s.cancelled_by_device << " by DDR3 reset/remap " << s.cancelled_by_platform
        << ", device writes held "
        << impl_->sys_dma_guard.writes_held() + impl_->isp_odma_guard.writes_held() +
               impl_->camera_writes_held()
        << ", atomics drained " << s.brackets_waited;
    return out.str();
}

} // namespace cdc::platforms::fx1_soc
