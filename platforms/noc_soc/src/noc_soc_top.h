// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::noc_soc {

enum class noc_soc_mode {
    survey,
    firmware,
};

enum class noc_timing_mode {
    detailed,
    fast,
};

// A small SoC whose interconnect is the cycle-accurate FlooNoC model instead
// of `bus_router`.
//
// It is kept separate from `mini_tlm` on purpose. The two are otherwise the
// same shape, so running both and comparing is the point: `mini_tlm` shows what
// the platform does with a zero-cost bus, this one shows what it costs on a
// real network-on-chip.
class noc_soc_top : public sc_core::sc_module {
public:
    /// The two traffic-ownership modes are explicit and mutually exclusive.
    /// Survey mode requires an empty `firmware` path and owns the synthetic RAM,
    /// peripheral and DMA traffic. Firmware mode requires an ELF and gives RAM
    /// contents plus DMA0 exclusively to the CPU/firmware path.
    /// `sim_us` bounds the run. Firmware ends in a spin loop, so without a
    /// limit the simulation never returns.
    /// `measure_baseline` installs the Step 12.7 passive measurement observer
    /// and prints its report at the end of the run. It is off by default: the
    /// observer classifies every completion, and a regression should not carry
    /// instrumentation it does not read.
    noc_soc_top(sc_core::sc_module_name name, std::string config_path,
                noc_soc_mode mode,
                noc_timing_mode timing = noc_timing_mode::detailed,
                std::string firmware = {},
                double sim_us = 0.0,
                bool measure_baseline = false,
                std::string metrics_path = {});
    ~noc_soc_top() override;

    /// Configure UART0's host-side pin bridge before sc_start(). File replay
    /// is deterministic and intended for CI; the loopback TCP backend is for
    /// interactive use. Both paths enter the real UART RX FIFO and reach
    /// firmware through PLIC source 1.
    void set_uart0_socket(std::uint16_t port, bool wait_for_client);
    void set_uart0_rx_file(const std::string& path,
                           std::uint64_t start_delay_us = 0);

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::noc_soc
