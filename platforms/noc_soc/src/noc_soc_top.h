// SPDX-License-Identifier: Apache-2.0

#pragma once

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
    noc_soc_top(sc_core::sc_module_name name, std::string config_path,
                noc_soc_mode mode,
                noc_timing_mode timing = noc_timing_mode::detailed,
                std::string firmware = {},
                double sim_us = 0.0);
    ~noc_soc_top() override;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::noc_soc
