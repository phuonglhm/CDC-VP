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
#include <icsl_eth/eth_mac_tlm.h>
#include <icsl_eth/eth_phy_tlm.h>
#include "eth_line_peer.h"
#include "eth_wb_delay.h"
#include "csi_dma_fault.h"
#include "csi_isp_direct.h"
#include "isp_dma_fault.h"
#include "traffic_counter.h"
#include "bw_recorder.h"
#include "isp_stream_bridge.h"
#include <riscv_vp_plusplus_wrapper.h>
#include <uart.h>
#include <uart_host_bridge.h>
#ifdef FX1_SOC_HAS_NPU
#include <fx1_npu/fx1_npu.h>
#include <fx1_npu/fx1_npu_regmap.h>
#endif
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
constexpr unsigned kMasterEthDma = 0x103;
constexpr unsigned kMasterNpuDma = 0x104;
// Bus target of the NPU VP aperture (added by the platform; BusConfig::fx1()
// keeps its reserved NPU_CSR entry disabled).
constexpr const char* kNpuTarget = "NPU_VP";

// Ethernet MAC instance parameters: the defaults of MAS Table 5-1 (32-bit AXI,
// the only width the FX1 fabric takes) and the DDR base added to every DMA
// offset, once, inside the MAC.
icsl_eth::EthMacParams eth_params()
{
    icsl_eth::EthMacParams p;
    p.dram_base = FX1_DDR_BASE;
    return p;
}

EthLinePeer::Mode eth_peer_mode(const std::string& text)
{
    EthLinePeer::Mode mode;
    if (!EthLinePeer::parse(text, mode))
        throw std::invalid_argument("fx1_soc: --eth-peer " + text + ": expected loopback, partner or none");
    return mode;
}

// I2C0: functional host at fast-mode 400 kHz; the camera sensor answers at
// 7-bit address 0x10 (CcsSensorConfig default).
constexpr double kI2cSclHz = 400e3;
constexpr unsigned kSensorI2cAddr = 0x10;

// Bus target names (bus::BusConfig::fx1()) and the platform-only finisher.
constexpr const char* kSimCtrlTarget = "SIM_CTRL";
// VP-only ISP_INGRESS (direct CSI->ISP, contract §6): always mapped with the ISP.
constexpr const char* kIspIngressTarget = "ISP_INGRESS";
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

bus::BusConfig make_bus_config(bool camera, bool npu)
{
    auto cfg = bus::BusConfig::fx1();
    for (auto& target : cfg.targets)
        if (target.name == "SYS_DMA_CSR" || target.name == "ISP_CSR" || target.name == "SRAM" ||
            target.name == "DDR3_CSR" || target.name == "DDR_SYSCTRL" || target.name == "I2C0" ||
            target.name == "ETH_CSR" || (camera && target.name == "MIPI_CSR"))
            target.enabled = true;  // instantiated below (G3, G4, DDR3 subtask 5, CSI-2 M5, Ethernet M2)
    cfg.targets.push_back({kSimCtrlTarget, FX1_SIM_CTRL_BASE, FX1_APB_SLOT_SIZE,
                           bus::TargetPath::Peribus0Apb, true});
    cfg.targets.push_back({kIspIngressTarget, FX1_ISP_INGRESS_BASE, FX1_APB_SLOT_SIZE,
                           bus::TargetPath::Peribus1Apb, true});
    // NPU_VP is a leaf of its own on SYSBUS_1 (review N1, N1-B): a push that
    // waits for a queue slot holds only the NPU port, not the SYSBUS_0 link to
    // CLINT, PLIC and UART.
    if (npu)
        cfg.targets.push_back({kNpuTarget, FX1_NPU_VP_BASE, FX1_NPU_VP_SIZE, bus::TargetPath::SysBus1Axi, true});
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
// --csi-dma-fault (D2b): a fault shim before the guard. The DMA socket is
// bound by the platform: its first hop is the MIPI_DMA traffic counter when
// there is one (D2b review F1: issued traffic, injected errors included).
struct camera : sc_core::sc_module {
    fx1::WriteGuard dma_guard;
    fx1::csi2::DphyRxWrapper phy;
    fx1::csi2::Csi2RxController ctl;
    fx1::csi2::bfm::DphyTxBfm tx;
    fx1::csi2::CcsSensor sensor;
    std::unique_ptr<CsiDmaFault> dma_fault;  // --csi-dma-fault only

    static fx1::csi2::CcsSensorConfig sensor_config(const fx1_soc_options& o)
    {
        fx1::csi2::CcsSensorConfig c;
        c.i2c_addr = kSensorI2cAddr;
        c.raw_file = o.camera_raw;
        c.continuous_clock = false;  // P16: clock lane in LP-11 between frames (host cost of the PHY)
        for (const auto& f : o.camera_faults)
            c.faults.push_back({f.frame,
                                f.no_fe ? fx1::csi2::CcsSensorFault::Kind::no_fe : fx1::csi2::CcsSensorFault::Kind::crc,
                                f.line});
        return c;
    }
    camera(sc_core::sc_module_name name, fx1::ExclusiveMonitor& monitor, const fx1_soc_options& o)
        : sc_module(name)
        , dma_guard("mipi_dma_guard", monitor, kMasterMipiDma)
        , phy("dphy_rx")
        , ctl("csi2_rx", phy)
        , tx("sensor_tx", phy.clkp, phy.clkn, phy.dp, phy.dn)
        , sensor("sensor", tx, sensor_config(o))
    {
        if (o.csi_dma_fault_nth) {
            dma_fault = std::make_unique<CsiDmaFault>("csi_dma_fault", o.csi_dma_fault_nth);
            dma_fault->out.bind(dma_guard.target);
        }
    }
    // Where the DMA's writes go after the traffic counter: the fault shim or the guard.
    tlm::tlm_target_socket<32>& dma_next()
    {
        if (dma_fault) return dma_fault->target;
        return dma_guard.target;
    }
};
#endif

#ifdef FX1_SOC_HAS_NPU
// NPU (plan FX1_NPU_V47 N1): the SAURIA v4.7 clone; DMA through a write guard
// like the other non-CPU writers (plan C7); RAM = the CPU DDR aperture.
struct npu_block {
    std::unique_ptr<fx1::WriteGuard> dma_guard; // null with --npu-unguarded-dma (negative control)
    fx1::npu::Fx1Npu npu;
    static fx1::npu::NpuOptions options(unsigned watchdog_s)
    {
        fx1::npu::NpuOptions o;
        o.ram_base = FX1_DDR_BASE;
        o.ram_size = FX1_DDR_SIZE;
        o.max_window = FX1_NPU_ARENA_LARGE_SIZE;
        o.host_watchdog_s = watchdog_s;
        return o;
    }
    npu_block(fx1::ExclusiveMonitor& monitor, unsigned watchdog_s, bool guarded) : npu("npu", options(watchdog_s))
    {
        if (guarded) {
            dma_guard = std::make_unique<fx1::WriteGuard>("npu_dma_guard", monitor, kMasterNpuDma);
            npu.master_socket.bind(dma_guard->target);
        }
    }
};
#else
struct npu_block {};
#endif

} // namespace

bool fx1_soc_top_npu_default() noexcept { return fx1_soc_top::npu_built(); }

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
    // Ethernet MAC (components/FX1_Components/ethernet, plan M2): CSR on
    // PERIBUS_1 at local offsets behind a word-access guard (FX1 MMIO policy:
    // one aligned 32-bit access, else an access fault; the MAC's own decode is
    // MAS Table 6-47), DMA as ETH_DMA through a write guard (physical
    // addresses: the MAC adds FX1_DDR_BASE), level IRQ on source 6, the PHY at
    // MDIO address 1 and the line peer of --eth-peer.
    fx1::WordAccessGuard eth_csr_guard;
    std::unique_ptr<fx1::WriteGuard> eth_dma_guard;  // null with --eth-unguarded-dma only
    EthWbDelay eth_wb_delay;                         // --eth-wb-delay-us (a pass-through at 0)
    icsl_eth::EthMacTlm eth;
    icsl_eth::EthPhyTlm eth_phy;
    EthLinePeer eth_peer;
    sc_core::sc_signal<bool> eth_rst_n{"eth_rst_n"};
    sc_core::sc_signal<bool> eth_rmii_rst_n{"eth_rmii_rst_n"};
#ifdef FX1_SOC_HAS_CAMERA
    std::unique_ptr<camera> cam;  // --camera only
    std::unique_ptr<CsiIspDirect> csi_isp;  // --camera: the CSI stream and observer to the ISP ingress
#endif
    // --traffic-report or --bw-report: counters on the camera / ISP DMA masters.
    std::unique_ptr<TrafficCounter> traffic_mipi_dma, traffic_isp_idma, traffic_isp_odma;
    std::unique_ptr<BwRecorder> bw;  // --bw-report only
#ifdef FX1_SOC_HAS_CAMERA
    std::unique_ptr<CsiRxToBw> bw_rx;  // --bw-report with --camera
#endif
    std::unique_ptr<IspStreamBridge> isp_stream;  // --isp-stream-spool only
    std::unique_ptr<IspDmaFault> isp_dma_fault;   // --isp-dma-fault only
    std::unique_ptr<npu_block> npu;               // null with --no-npu or without FX1_SOC_HAS_NPU
    sc_core::sc_signal<bool> npu_rst_n{"npu_rst_n"};

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
        , fabric("fabric", make_bus_config(opts.camera, opts.npu), opts.bus_trace)
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
        , eth_csr_guard("eth_csr_guard")
        , eth_wb_delay("eth_wb_delay", opts.eth_wb_delay_us)
        , eth("eth", eth_params())
        , eth_phy("eth_phy", 1)
        , eth_peer("eth_peer", eth_peer_mode(opts.eth_peer))
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
        // VP-only ISP_INGRESS at 0x1105_0000 (direct CSI->ISP): always mapped;
        // CAPABILITY.BOUND only with --camera (the CSI stream bound below).
        fabric.target(kIspIngressTarget).bind(isp.ingress_csr_socket);
        // VP-only bandwidth recorder (--bw-report): it only listens (bw_recorder.h).
        if (!opts.bw_report.empty()) {
            bw = std::make_unique<BwRecorder>(opts.bw_report, opts.bw_dump_nv12,
                                              [this](std::uint64_t a, std::uint8_t* d, std::size_t n) {
                                                  if (a < FX1_DDR_BASE || a + n > FX1_DDR_BASE + FX1_DDR_SIZE)
                                                      throw std::out_of_range("outside DDR");
                                                  ddr3.dump(a, d, n);
                                              });
        }
        // Window marker (SIM_CTRL 0xC): with the recorder, or alone (--bw-marker) as
        // the control run of the same stimulus without any observer.
        if (bw) sim.enable_bw_marker([this](const fx1::SimControl::BwMark& m) { bw->mark(m); });
        else if (opts.bw_marker) sim.enable_bw_marker();
        if (!opts.traffic_report.empty() || bw) {
            traffic_isp_idma = std::make_unique<TrafficCounter>("traffic_isp_idma");
            traffic_isp_odma = std::make_unique<TrafficCounter>("traffic_isp_odma");
            if (opts.camera) traffic_mipi_dma = std::make_unique<TrafficCounter>("traffic_mipi_dma");
            for (const auto& w : opts.traffic_windows) {
                TrafficCounter* c = w.master == "MIPI_DMA" ? traffic_mipi_dma.get()
                                    : w.master == "ISP_IDMA" ? traffic_isp_idma.get()
                                                             : traffic_isp_odma.get();
                if (c) c->add_window(w.lo, w.hi);
            }
            if (bw) {
                if (traffic_mipi_dma) traffic_mipi_dma->set_event_sink(bw.get(), 0);
                traffic_isp_idma->set_event_sink(bw.get(), 1);
                traffic_isp_odma->set_event_sink(bw.get(), 2);
            }
        }
        // --isp-dma-fault puts its module in front of one master; without it the
        // sockets are bound as they always were.
        if (opts.isp_dma_fault_kind) {
            const IspDmaFault::Kind kind = opts.isp_dma_fault_kind == 1   ? IspDmaFault::Kind::idma_error
                                           : opts.isp_dma_fault_kind == 2 ? IspDmaFault::Kind::odma_error
                                                                          : IspDmaFault::Kind::idma_stall;
            isp_dma_fault = std::make_unique<IspDmaFault>("isp_dma_fault", kind, opts.isp_dma_fault_nth,
                                                          opts.isp_dma_fault_hold_us);
        }
        // The traffic counters are the first hop of each ISP DMA master, as for
        // MIPI_DMA (review BW1 F2): they see every transaction the master issues
        // at its issue time, including one the fault shim answers with an error
        // or holds, and one the write guard delays.
        //   IDMA: ISP -> [counter] -> [fault shim] -> fabric
        //   ODMA: ISP -> [counter] -> [fault shim] -> write guard -> fabric
        tlm::tlm_initiator_socket<>* idma_next = &isp.idma_socket;
        if (traffic_isp_idma) {
            idma_next->bind(traffic_isp_idma->target);
            idma_next = &traffic_isp_idma->out;
        }
        if (isp_dma_fault && isp_dma_fault->idma()) {
            idma_next->bind(isp_dma_fault->target);
            idma_next = &isp_dma_fault->out;
        }
        idma_next->bind(fabric.initiator("ISP_IDMA"));
        tlm::tlm_initiator_socket<>* odma_next = &isp.odma_socket;
        if (traffic_isp_odma) {
            odma_next->bind(traffic_isp_odma->target);
            odma_next = &traffic_isp_odma->out;
        }
        if (isp_dma_fault && !isp_dma_fault->idma()) {
            odma_next->bind(isp_dma_fault->target);
            odma_next = &isp_dma_fault->out;
        }
        odma_next->bind(isp_odma_guard.target);
        isp_odma_guard.out.bind(fabric.initiator("ISP_ODMA"));
        isp.rst_n(isp_rst_n);
        if (opts.isp_demosaic) isp.debug_set_demosaic_observer(opts.isp_demosaic);
        if (bw) isp.debug_set_output_observer(bw.get());
        fabric.target("I2C0").bind(i2c_guard.target);
        i2c_guard.out.bind(i2c0.socket);
        fabric.target("ETH_CSR").bind(eth_csr_guard.target);
        eth_csr_guard.out.bind(eth.csr);
        eth.axi.bind(eth_wb_delay.target);
        if (options.eth_unguarded_dma) {
            eth_wb_delay.out.bind(fabric.initiator("ETH_DMA"));
        } else {
            eth_dma_guard = std::make_unique<fx1::WriteGuard>("eth_dma_guard", monitor, kMasterEthDma);
            eth_wb_delay.out.bind(eth_dma_guard->target);
            eth_dma_guard->out.bind(fabric.initiator("ETH_DMA"));
        }
        eth.phy_tx.bind(eth_phy.mac_tx);
        eth_phy.mac_rx.bind(eth.phy_rx);
        eth.mdio.bind(eth_phy.mdio);
        eth_phy.line_tx.bind(eth_peer.line_rx);
        eth_peer.line_tx.bind(eth_phy.line_rx);
        eth_phy.set_link(eth_peer.mode() != EthLinePeer::Mode::none);
        eth.rst_n(eth_rst_n);
        eth.rmii_rst_n(eth_rmii_rst_n);
        // VP-only doorbell of the ISP stream (sim-control offset 8): present in every
        // run, ATTACHED only with the host bridge (which also enables it).
        sim.enable_doorbell(false);
        if (!options.isp_stream_spool.empty())
            isp_stream = std::make_unique<IspStreamBridge>(
                "isp_stream_bridge", ddr3, sim,
                IspStreamBridge::Options{options.isp_stream_spool, options.isp_stream_max_bytes,
                                         options.isp_stream_hold_us,
                                         options.isp_stream_foreign_ack
                                             ? IspStreamBridge::Options::Fault::foreign_ack
                                             : IspStreamBridge::Options::Fault::none});
        if (options.camera) {
#ifdef FX1_SOC_HAS_CAMERA
            // CSR on PERIBUS_1 (local offsets), DMA as MIPI_DMA, csi2_irq on
            // PLIC source 7, sensor control on I2C0. Without --camera MIPI_CSR
            // stays disabled (DECERR) and source 7 undriven.
            cam = std::make_unique<camera>("camera", monitor, options);
            fabric.target("MIPI_CSR").bind(cam->ctl.csr_socket);
            // MIPI_DMA: CSI DMA socket -> [traffic counter] -> [fault shim] -> write guard -> fabric.
            if (traffic_mipi_dma) {
                cam->ctl.dma_socket.bind(traffic_mipi_dma->target);
                traffic_mipi_dma->out.bind(cam->dma_next());
            } else {
                cam->ctl.dma_socket.bind(cam->dma_next());
            }
            cam->dma_guard.out.bind(fabric.initiator("MIPI_DMA"));
            // Direct CSI->ISP (D2): the CSI stream master and capture observer feed
            // the ISP's stream ingress; SOURCE = memory (reset) discards it (P21).
            csi_isp = std::make_unique<CsiIspDirect>(isp.ingress());
            cam->ctl.set_stream_sink(csi_isp.get());
            cam->ctl.set_capture_observer(csi_isp.get());
            if (bw) {
                csi_isp->set_bw(bw.get());
                bw_rx = std::make_unique<CsiRxToBw>(*bw);
                cam->ctl.set_rx_payload_observer(bw_rx.get());
            }
            isp.ingress().set_bound(true);
            cam->ctl.resetn(csi_rst_n);
            i2c0.attach(kSensorI2cAddr, &cam->sensor);
#else
            throw std::invalid_argument("fx1_soc: --camera: this build has no camera "
                                        "(configure with -DFX1_BUILD_MIPI_CSI2=ON)");
#endif
        }
        if (options.npu) {
#ifdef FX1_SOC_HAS_NPU
            // 1 MiB aperture on SYSBUS_1 (FX1_NPU_VP_BASE), NPU_DMA through its
            // write guard, level IRQ on source 4, 20 ns power-on reset.
            npu = std::make_unique<npu_block>(monitor, options.npu_host_watchdog_s, !options.npu_unguarded_dma);
            fabric.target(kNpuTarget).bind(npu->npu.target_socket);
            if (npu->dma_guard) npu->dma_guard->out.bind(fabric.initiator("NPU_DMA"));
            else npu->npu.master_socket.bind(fabric.initiator("NPU_DMA")); /* --npu-unguarded-dma */
            npu->npu.reset_n(npu_rst_n);
#else
            throw std::invalid_argument("fx1_soc: this build has no NPU (configure with -DFX1_SOC_ENABLE_NPU=ON)");
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
        // 4 NPU (low with --no-npu); 5 H.264 (reserved, low); 6 Ethernet;
        // 7 CSI-2 RX (--camera); 8 DDR3; 9 I2C0; 10..11 the VP-only test lines.
        for (unsigned id = 1; id < FX1_PLIC_NUM_SOURCES; ++id) plic.irq_in[id - 1](irq_line[id - 1]);
        uart.irq(irq_line[FX1_IRQ_UART - 1]);
        sys_dma.irq(irq_line[FX1_IRQ_SYS_DMA - 1]);
        isp.irq(irq_line[FX1_IRQ_ISP - 1]);
        for (unsigned i = 0; i < FX1_SIM_CTRL_IRQ_LINES; ++i)
            sim.test_irq[i](irq_line[FX1_SIM_CTRL_IRQ_FIRST_SOURCE - 1 + i]);
        ddr3.irq()(irq_line[FX1_IRQ_DDR - 1]);
        i2c0.irq(irq_line[FX1_IRQ_I2C0 - 1]);
        eth.irq(irq_line[FX1_IRQ_ETH - 1]);
#ifdef FX1_SOC_HAS_CAMERA
        if (cam) cam->ctl.irq(irq_line[FX1_IRQ_MIPI - 1]);
#endif
#ifdef FX1_SOC_HAS_NPU
        if (npu) npu->npu.irq_out(irq_line[FX1_IRQ_NPU - 1]);
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

    // Hold SYS_DMA, the ISP, the CSI-2 RX and both Ethernet domains (system
    // and RMII, MAS 11.3) in reset for a short pulse after power-up, then
    // release them. ETH_CTRL then reads 0: every MAC block stays held until
    // firmware writes it (MAS 9.1).
    void power_on_reset()
    {
        dma_reset_n.write(false);
        npu_rst_n.write(false);
        isp_rst_n.write(false);
        csi_rst_n.write(false);
        eth_rst_n.write(false);
        eth_rmii_rst_n.write(false);
        wait(sc_core::sc_time(kResetPulseNs, sc_core::SC_NS));
        dma_reset_n.write(true);
        npu_rst_n.write(true);
        isp_rst_n.write(true);
        csi_rst_n.write(true);
        eth_rst_n.write(true);
        eth_rmii_rst_n.write(true);
#ifdef FX1_SOC_HAS_CAMERA
        // --camera-frame-id-preset (D2b test hook): after the reset release
        // (resetn clears the counter), long before firmware can start a capture.
        if (cam && options.camera_frame_id_preset >= 0) {
            wait(sc_core::sc_time(kResetPulseNs, sc_core::SC_NS));
            cam->ctl.debug_set_frame_id_counter(static_cast<std::uint16_t>(options.camera_frame_id_preset));
        }
#endif
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

    std::uint64_t npu_writes_held() const
    {
#ifdef FX1_SOC_HAS_NPU
        if (npu && npu->dma_guard) return npu->dma_guard->writes_held();
#endif
        return 0;
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
    if (impl_->bw) {
        auto& i = *impl_;
        std::vector<std::pair<std::string, std::string>> f = options.bw_header;
        f.emplace_back("isp_core_period_ticks", std::to_string(i.isp.params().core_period.value()));
        f.emplace_back("isp_axi_data_bytes", std::to_string(i.isp.params().axi_data_bytes));
#ifdef FX1_SOC_HAS_CAMERA
        if (i.cam) {
            f.emplace_back("csi_sys_clk_period_ticks",
                           std::to_string(fx1::csi2::Csi2ControllerConfig{}.sys_clk_period.value()));
            f.emplace_back("axis_data_bytes", "8");
        }
#endif
        f.emplace_back("camera", options.camera ? "1" : "0");
        f.emplace_back("quantum_ticks", std::to_string(tlm::tlm_global_quantum::instance().get().value()));
        f.emplace_back("dma_masters", "[\"MIPI_DMA\",\"ISP_IDMA\",\"ISP_ODMA\"]");
        f.emplace_back("not_observed", "[\"CPU\",\"CPU_DMI\",\"SYS_DMA\",\"ETH_DMA\",\"NPU_DMA\"]");
        f.emplace_back("nv12_dump", options.bw_dump_nv12.empty() ? "0" : "1");
        i.bw->header(f);
    }
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

std::string fx1_soc_top::eth_banner() const
{
    std::ostringstream out;
    out << "MAC at 0x" << std::hex << FX1_ETH_CSR_BASE << std::dec << " (IRQ " << FX1_IRQ_ETH
        << ", DMA via ETH_DMA" << (impl_->eth_dma_guard ? "" : " UNGUARDED (negative control)")
        << "), PHY at MDIO 1, line " << EthLinePeer::name_of(impl_->eth_peer.mode());
    if (impl_->options.eth_wb_delay_us)
        out << ", DES0 write-backs held " << impl_->options.eth_wb_delay_us << " us (fault injection)";
    return out.str();
}

std::string fx1_soc_top::eth_report() const
{
    std::ostringstream out;
    out << "frames sent " << impl_->eth.frames_sent() << " received " << impl_->eth.frames_received()
        << ", line peer in " << impl_->eth_peer.frames_in() << " out " << impl_->eth_peer.frames_out()
        << ", PHY dropped " << impl_->eth_phy.dropped() << ", CSR accesses refused " << impl_->eth_csr_guard.rejected();
    if (impl_->options.eth_wb_delay_us) out << ", DES0 write-backs held " << impl_->eth_wb_delay.held();
    return out.str();
}

bool fx1_soc_top::isp_stream_enabled() const noexcept { return impl_->isp_stream != nullptr; }
void fx1_soc_top::isp_stream_finish()
{
    if (impl_->isp_stream) impl_->isp_stream->finish();
}
bool fx1_soc_top::isp_stream_ok() const { return !impl_->isp_stream || impl_->isp_stream->ok(); }
std::string fx1_soc_top::isp_stream_report() const
{
    return impl_->isp_stream ? impl_->isp_stream->report() : std::string();
}
std::string fx1_soc_top::traffic_report() const
{
    std::ostringstream o;
    if (impl_->traffic_mipi_dma) o << impl_->traffic_mipi_dma->report("MIPI_DMA") << '\n';
    if (impl_->traffic_isp_idma) o << impl_->traffic_isp_idma->report("ISP_IDMA") << '\n';
    if (impl_->traffic_isp_odma) o << impl_->traffic_isp_odma->report("ISP_ODMA") << '\n';
    return o.str();
}

std::string fx1_soc_top::isp_dma_fault_report() const
{
    return impl_->isp_dma_fault ? impl_->isp_dma_fault->report() : std::string();
}
std::string fx1_soc_top::camera_fault_report() const
{
    std::ostringstream o;
#ifdef FX1_SOC_HAS_CAMERA
    const auto& opt = impl_->options;
    if (!impl_->cam) return {};
    const char* sep = "";
    if (!opt.camera_faults.empty()) {
        o << "sensor faults " << impl_->cam->sensor.faults_fired() << " of " << opt.camera_faults.size() << " fired (";
        for (std::size_t i = 0; i < opt.camera_faults.size(); ++i) {
            const auto& f = opt.camera_faults[i];
            o << (i ? " " : "") << f.frame << ':' << (f.no_fe ? "no-fe" : "crc");
            if (!f.no_fe) o << ':' << f.line;
        }
        o << ')';
        sep = "; ";
    }
    if (opt.camera_frame_id_preset >= 0) {
        o << sep << "FRAME_ID counter preset " << opt.camera_frame_id_preset;
        sep = "; ";
    }
    if (impl_->cam->dma_fault) o << sep << "csi dma " << impl_->cam->dma_fault->report();
#endif
    return o.str();
}
std::vector<std::string> fx1_soc_top::isp_stream_errors() const
{
    return impl_->isp_stream ? impl_->isp_stream->errors() : std::vector<std::string>();
}

bool fx1_soc_top::bw_finish(const std::string& reason)
{
    if (!impl_->bw) return true;
    impl_->bw->finish(reason);
    return impl_->bw->ok();
}

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

bool fx1_soc_top::npu_built() noexcept
{
#ifdef FX1_SOC_HAS_NPU
    return true;
#else
    return false;
#endif
}

std::uint32_t fx1_soc_top::npu_abi() noexcept
{
#ifdef FX1_SOC_HAS_NPU
    return fx1::npu::reg::VP_ABI_VALUE;
#else
    return 0;
#endif
}

bool fx1_soc_top::npu_present() const noexcept { return impl_->npu != nullptr; }

std::string fx1_soc_top::npu_banner() const
{
#ifdef FX1_SOC_HAS_NPU
    if (impl_->npu) {
        std::ostringstream out;
        out << "at 0x" << std::hex << FX1_NPU_VP_BASE << std::dec << " (1 MiB, SYSBUS_1, IRQ " << FX1_IRQ_NPU
            << ", DMA via NPU_DMA" << (impl_->npu->dma_guard ? "" : " UNGUARDED (negative control)") << "), "
            << impl_->npu->npu.banner() << ", host watchdog "
            << impl_->options.npu_host_watchdog_s << " s";
        return out.str();
    }
#endif
    return npu_built() ? "absent (--no-npu): aperture DECERR, source 4 low" : "not built";
}

std::string fx1_soc_top::npu_report() const
{
#ifdef FX1_SOC_HAS_NPU
    if (impl_->npu) return impl_->npu->npu.report();
#endif
    return "absent";
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
               impl_->camera_writes_held() + impl_->npu_writes_held() +
               (impl_->eth_dma_guard ? impl_->eth_dma_guard->writes_held() : 0)
        << ", atomics drained " << s.brackets_waited;
    return out.str();
}

} // namespace cdc::platforms::fx1_soc
