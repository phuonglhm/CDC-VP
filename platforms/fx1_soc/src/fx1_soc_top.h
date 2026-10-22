#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <systemc>

#include "ddr3_subsystem.h"

namespace cdc::platforms::fx1_soc {

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
    // This binary contains the camera (configured with FX1_BUILD_MIPI_CSI2).
    static bool camera_built() noexcept;
    // One line of exclusive-monitor counters ("off" without a monitor).
    std::string atomics_report() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::fx1_soc
