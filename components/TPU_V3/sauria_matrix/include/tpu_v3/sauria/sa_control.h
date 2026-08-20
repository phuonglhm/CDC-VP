// SPDX-License-Identifier: Apache-2.0
//
// Firmware-visible AXI4-Lite control target for one Sauria matrix engine.

#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/sauria/sauria_matrix_if.h"

namespace cdc::components::tpu_v3::sauria {

struct sa_control_config {
    std::uint64_t control_base = 0;
    std::uint64_t control_size = address_map::sa_control_size;

    void validate(const std::string& context) const;
};

/// The 32-bit AXI4-Lite register file frozen in `sa_registers.h`.
///
/// This object owns no data path and no Sauria implementation. It snapshots a
/// `job`, submits it through `sauria_matrix_if`, observes completion on clock
/// edges, and converts that state to firmware-visible status and a level IRQ.
class sa_control : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(sa_control);

    sa_control(sc_core::sc_module_name name, sa_control_config config,
               sauria_matrix_if& engine);

    sc_core::sc_in<bool> i_clk{"i_clk"};
    tlm_utils::simple_target_socket<sa_control> control{"control"};
    sc_core::sc_out<bool> irq{"irq"};

    void reset();

    /// Hold the engine's admission gate open or closed.
    ///
    /// Called by the NEO-CORE around its reset pulse. It is a plain method and
    /// a plain flag on purpose: `reset()` returns in zero time while the
    /// `i_rstn` signal it asserts only updates a delta later, so a gate that
    /// read the signal would let a `START` issued in between straight through.
    /// The engine's clocked modules are held in reset for that whole period,
    /// so a job admitted there would be configured by writes they cannot latch
    /// and would then run on a configuration nobody applied.
    void hold_in_reset(bool held) noexcept { held_in_reset_ = held; }
    bool held_in_reset() const noexcept { return held_in_reset_; }

    std::uint32_t status() const noexcept { return status_; }
    error_cause last_error() const noexcept { return error_cause_; }
    std::uint64_t job_count() const noexcept { return job_count_; }
    std::uint64_t error_count() const noexcept { return error_count_; }
    std::uint64_t abort_count() const noexcept { return abort_count_; }
    std::uint64_t overrun_count() const noexcept { return overrun_count_; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    bool check_control_rules(tlm::tlm_generic_payload& trans);
    std::uint32_t read_register(std::uint64_t offset) const;
    bool write_register(std::uint64_t offset, std::uint32_t value);
    void observe_completion();
    void drive_irq();
    void update_irq();
    void snapshot_engine();

    static error_cause cause_of(submit_status status) noexcept;

    sa_control_config config_;
    sauria_matrix_if& engine_;
    job programmed_;

    std::uint32_t status_ = 0;
    std::uint32_t irq_enable_ = 0;
    error_cause error_cause_ = error_cause::none;
    std::uint64_t job_count_ = 0;
    std::uint64_t error_count_ = 0;
    std::uint64_t abort_count_ = 0;
    std::uint64_t overrun_count_ = 0;
    std::uint64_t c_bytes_done_ = 0;
    bool held_in_reset_ = false;

    /// Wakes the sole writer of `irq`. Both the register paths and the clocked
    /// completion observer notify it; neither writes the signal itself.
    sc_core::sc_event irq_event_;
    std::uint64_t local_requests_ = 0;
    std::uint64_t local_bytes_ = 0;
    sauria_matrix_if::timing timing_;

    /// ABORT and active reset release admission immediately, but a native write
    /// already accepted by the fabric may return afterwards with bytes that are
    /// already in C. The abandoned job keeps ownership of C_BYTES_DONE until the
    /// next START; while this flag is set, register reads and clock-edge
    /// snapshots reconcile against the engine's still-live accounting.
    bool abandoned_accounting_open_ = false;
};

} // namespace cdc::components::tpu_v3::sauria
