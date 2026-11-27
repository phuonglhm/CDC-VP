#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <systemc>

#include "ddr3_subsystem.h"

namespace cdc::components::fx1_isp {
class demosaic_observer;  // fx1_isp/fx1_isp_debug.h
}

namespace cdc::platforms::fx1_soc {

// True when this binary contains the NPU (configured with FX1_SOC_ENABLE_NPU).
bool fx1_soc_top_npu_default() noexcept;

struct fx1_soc_options {
    // ELF image. The platform writes its PT_LOAD segments before the harts run
    // (BootROM/SRAM at once; DDR after the DDR3 preinit, never in firmware
    // boot) and every hart resets at its entry point (one-image boot, README).
    std::string firmware;
    // DDR3 boot mode (DEC-09): preinit (default) or firmware (boot from ROM/SRAM).
    DdrBoot ddr_boot = DdrBoot::preinit;
    // DDR3 build option DDR_SYS_ECC_ENABLE (baseline on).
    bool ddr3_ecc = true;
    // --ddr3-fault requests (host-only fault injection, timed from CPU release).
    std::vector<Ddr3Fault> ddr3_faults;
    // Mirror of the UART TX stream; empty for none.
    std::string uart_log;
    bool uart_stdout = true;
    // UART RX from the host (components/uart_host_tlm): a TCP console on
    // 127.0.0.1:<uart_socket> (-1: off; with uart_wait the run waits for the
    // client at t=0), and/or bytes replayed from uart_rx_file starting at
    // uart_rx_delay_us of simulated time (deterministic, for CI).
    int uart_socket = -1;
    // Throttle: simulated time never runs ahead of wall-clock time.
    bool realtime = false;
    bool uart_wait = false;
    std::string uart_rx_file;
    unsigned uart_rx_delay_us = 0;
    // Pacing of injected RX bytes (socket and replay): one byte per this many
    // microseconds. 10 (default) is about 1 Mbaud; 87 is the 115200 8N1
    // character time.
    unsigned uart_rx_byte_us = 10;
    // Print every routed bus transaction.
    bool bus_trace = false;
    // Plan C7 exclusive monitor: real LR/SC reservations shared with SYS_DMA
    // and ISP ODMA. false = upstream VP++ lock model (negative control only).
    bool exclusive_monitor = true;
    // --camera: CSI-2 RX controller + D-PHY + CCS sensor model (I2C0). Needs a
    // build with the camera (camera_built()). camera_raw: frames of 16-bit
    // little-endian samples for the sensor; empty for its built-in pattern.
    bool camera = false;
    std::string camera_raw;
    // VP-only camera fault injection (plan DIRECT_CSI_ISP D2b; --camera only, not
    // a SoC model): --camera-fault FRAME:crc[:LINE] | FRAME:no-fe, the sensor's
    // CcsSensorFault list (may be repeated); --camera-frame-id-preset N: the
    // CSI-2 RX's 14-bit FRAME_ID counter is set to N (test hook) once the
    // power-on reset is released, the first capture gets N + 1 (-1: off);
    // --csi-dma-fault write-error:N: the N-th CSI RAW DMA write is answered with
    // a bus error (csi_dma_fault.h; 0: off).
    struct camera_fault {
        unsigned frame = 0;  // 1-based, frames the sensor starts
        bool no_fe = false;  // false: crc on `line`
        unsigned line = 0;
    };
    std::vector<camera_fault> camera_faults;
    int camera_frame_id_preset = -1;
    std::uint64_t csi_dma_fault_nth = 0;
    // Ethernet MAC (always present): what the PHY's line side meets,
    // "loopback" (default), "partner" (echo station) or "none" (link down).
    std::string eth_peer = "loopback";
    // Negative control only: the MAC's DMA bypasses its write guard, so its
    // writes no longer cancel LR reservations (tests prove the guard matters).
    bool eth_unguarded_dma = false;
    // Fault injection (Ethernet review F08/ER02): each DES0 write-back of the
    // MAC is held this long after its DONE; 0: off.
    unsigned eth_wb_delay_us = 0;
    // VP-only host debug (--dump-isp-demosaic): observer of the ISP's
    // post-Demosaic image, owned by the caller; null: off. Not an ISP output.
    cdc::components::fx1_isp::demosaic_observer* isp_demosaic = nullptr;
    // VP-only ISP stream host bridge (--isp-stream-spool DIR, isp_stream_bridge.h):
    // empty for none. The sim-control doorbell register always exists; it reports
    // a bridge attached only with this option. 0: no spool budget.
    std::string isp_stream_spool;
    std::uint64_t isp_stream_max_bytes = 0;
    unsigned isp_stream_hold_us = 0;  // --isp-stream-hold-us (fault injection)
    bool isp_stream_foreign_ack = false;  // --isp-stream-fault foreign-ack (fault injection)
    // Fault injection (--isp-dma-fault KIND:N, isp_dma_fault.h): the N-th ISP IDMA read
    // (kind 1 idma-error, 3 idma-stall) or ODMA write (kind 2 odma-error) is answered by
    // the VP. 0: off, and the ISP's DMA sockets are bound to the fabric directly.
    unsigned isp_dma_fault_kind = 0;
    // --traffic-report FILE (traffic_counter.h): VP-only counters on MIPI_DMA,
    // ISP_IDMA and ISP_ODMA, written to FILE after the run; empty: off (the
    // sockets are bound to the fabric directly).
    std::string traffic_report;
    // --traffic-window MASTER:LO:HI (with --traffic-report, may be repeated):
    // inclusive byte windows of that master's counter; a transaction not wholly
    // inside one of them is counted `outside` (traffic_counter.h).
    struct traffic_window {
        std::string master;  // MIPI_DMA, ISP_IDMA or ISP_ODMA
        std::uint64_t lo = 0, hi = 0;
    };
    std::vector<traffic_window> traffic_windows;
    // VP-only bandwidth measurement (plan CSI_ISP_BW1, bw_recorder.h):
    // --bw-report FILE: the JSONL event log (marker, CSI payload, AXIS, DMA,
    // ISP outputs), with the window marker; --bw-marker: the marker register
    // alone (no recorder: the on/off control of the same stimulus);
    // --bw-dump-nv12 DIR (with --bw-report): each DONE output's NV12, by backdoor.
    // bw_header: JSON fields the caller adds to the report header.
    std::string bw_report;
    bool bw_marker = false;
    std::string bw_dump_nv12;
    std::vector<std::pair<std::string, std::string>> bw_header;
    std::uint64_t isp_dma_fault_nth = 0;
    std::uint64_t isp_dma_fault_hold_us = 1000000;  // idma-stall:N:US
    // SAURIA v4.7 NPU (components/FX1_Components/npu) at FX1_NPU_VP_BASE, IRQ
    // source 4, DMA as NPU_DMA through a write guard. Present by default in a
    // build that has it (npu_built()); --no-npu removes it (DECERR, source 4 low).
    bool npu = fx1_soc_top_npu_default();
    // Host wall-time watchdog of the NPU model, seconds; 0 = off.
    unsigned npu_host_watchdog_s = 600;
    // Negative control (plan N4): NPU_DMA bypasses its write guard, so its
    // writes no longer wait for open AMO / SC brackets nor cancel reservations.
    bool npu_unguarded_dma = false;
};

// FX1 SoC for FW/SW bring-up: two VP++ RV32IMAFC harts, FX1 bus, BootROM,
// boot SRAM, the DDR3 controller subsystem (ddr3_subsystem.h), CLINT, PLIC,
// UART, SYS_DMA, ISP, I2C0, the CSI-2 RX with a camera sensor (--camera) and
// the VP-only sim-control finisher. Addresses come from fx1/fx1_memory_map.h.
class fx1_soc_top : public sc_core::sc_module {
public:
    enum class result { running, pass, fail };

    fx1_soc_top(sc_core::sc_module_name name, const fx1_soc_options& options);
    ~fx1_soc_top() override;

    result outcome() const;
    std::uint32_t fail_code() const;
    unsigned harts() const;
    std::uint64_t instret(unsigned hart) const;
    std::uint64_t pc(unsigned hart) const;
    // Copy `length` bytes of DDR at global `address` into `path` (debug
    // artifacts such as ISP NV12 output; stored bytes, no ECC correction).
    // Throws if out of range, not initialized or unwritable.
    void dump_ddr(std::uint64_t address, std::uint64_t length, const std::string& path) const;
    // Stage the file at `path` for DDR at global `address`, before the
    // simulation starts (host-provided data such as a RAW frame or a job
    // descriptor). It is written after the DDR3 preinit, before the CPUs run.
    // Throws if out of range or unreadable, and in firmware boot.
    void load_ddr(std::uint64_t address, const std::string& path);
    // Run the boot phase that precedes the CPUs: in preinit mode the DDR3
    // initialization, startup scrub and preload (simulated time passes); in
    // firmware mode nothing. Throws if the DDR3 preinit failed.
    void run_until_cpu_release();
    sc_core::sc_time cpu_release_time() const;
    // One line: DDR3 mode/profile/ECC/aperture (start of run), DDR3 counters (end).
    std::string ddr3_banner() const;
    std::string ddr3_report() const;
    // One line describing the camera (empty without --camera).
    std::string camera_banner() const;
    // --bw-report: write the footer (reason: pass / fail / timeout) and flush.
    // Returns false when the report or an NV12 dump failed. No-op without it.
    bool bw_finish(const std::string& reason);
    // This binary contains the camera (configured with FX1_BUILD_MIPI_CSI2).
    static bool camera_built() noexcept;
    // One line of exclusive-monitor counters ("off" without a monitor).
    std::string atomics_report() const;
    // One line describing the Ethernet MAC wiring (start of run) and its counters (end).
    std::string eth_banner() const;
    std::string eth_report() const;
    // --isp-stream-spool: after the run, write DIR/status.json; true when the bridge
    // captured every published frame without a protocol or host I/O error.
    bool isp_stream_enabled() const noexcept;
    void isp_stream_finish();
    bool isp_stream_ok() const;
    std::string isp_stream_report() const;
    // --isp-dma-fault: what it saw and whether it fired; empty when off.
    std::string isp_dma_fault_report() const;
    // --camera-fault / --camera-frame-id-preset / --csi-dma-fault: what was
    // injected (one line); empty when none of them is given.
    std::string camera_fault_report() const;
    // --traffic-report: one line per counted master (key=value); empty when off.
    std::string traffic_report() const;
    std::vector<std::string> isp_stream_errors() const;
    // NPU: built into this binary; present in this run; one-line banner (model,
    // knobs, queue) and end-of-run counters ("absent" without it).
    static bool npu_built() noexcept;
    // The NPU's VP_ABI register value (major << 16 | minor); 0 when not built.
    static std::uint32_t npu_abi() noexcept;
    bool npu_present() const noexcept;
    std::string npu_banner() const;
    std::string npu_report() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::fx1_soc
