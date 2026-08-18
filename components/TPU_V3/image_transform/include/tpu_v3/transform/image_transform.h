// SPDX-License-Identifier: Apache-2.0
// Source-traced Phase 6 ImageTransform engine.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/sram/native_port.h"
#include "tpu_v3/transform/im2col.h"

namespace cdc::components::tpu_v3::transform {

struct image_transform_config {
    std::uint64_t control_base = 0;
    std::uint64_t control_size = address_map::transform_control_size;
    std::uint64_t sram_base = 0;
    std::uint64_t sram_window = address_map::core_sram_window;

    void validate(const std::string& context) const;
};

/// One asynchronous ImageTransform job slot.
///
/// The functional path is buffered: input CHW bytes are fetched once through
/// the native port, then matrix rows are emitted through the same port. This is
/// a transaction-level staging policy, not a line-buffer/cycle claim about RTL.
/// It preserves the source's element order while keeping bank arbitration,
/// bounds, errors and traffic counters visible.
class image_transform : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(image_transform);

    image_transform(sc_core::sc_module_name name,
                    image_transform_config config);

    /// 32-bit AXI4-Lite target behind the NEO control fabric.
    tlm_utils::simple_target_socket<image_transform> control{"control"};
    /// The only tensor data path. No external master and no SRAM backing ptr.
    sc_core::sc_port<sram::neo_local_sram_if> local{"local"};
    /// Level interrupt for DONE or ERROR when enabled. ABORT does not raise it.
    sc_core::sc_out<bool> irq{"irq"};

    void reset();

    const image_transform_config& config() const noexcept { return config_; }
    bool busy() const noexcept
    {
        return (status_ & status_bit::busy) != 0;
    }
    std::uint32_t status() const noexcept { return status_; }
    error_cause last_error() const noexcept { return error_cause_; }
    std::uint64_t bytes_done() const noexcept { return bytes_done_; }
    std::uint64_t local_requests() const noexcept { return local_requests_; }
    std::uint64_t local_bytes() const noexcept { return local_bytes_; }
    std::uint64_t job_count() const noexcept { return job_count_; }
    std::uint64_t error_count() const noexcept { return error_count_; }
    std::uint64_t abort_count() const noexcept { return abort_count_; }
    std::uint64_t overrun_count() const noexcept { return overrun_count_; }

    std::string report() const;

private:
    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    bool check_control_rules(tlm::tlm_generic_payload& trans);
    std::uint32_t read_register(std::uint64_t offset) const;
    bool write_register(std::uint64_t offset, std::uint32_t value);

    void worker();
    error_cause validate_descriptor(const descriptor& work,
                                    matrix_shape& shape) const noexcept;
    error_cause run_im2col(const descriptor& work,
                           std::uint64_t generation,
                           std::uint64_t traffic_epoch);
    bool local_access(sram::neo_command command, std::uint64_t address,
                      std::uint32_t size, unsigned char* data,
                      std::uint64_t generation, std::uint64_t traffic_epoch,
                      bool destination, error_cause& cause);
    void finish(std::uint32_t status_bit, error_cause cause,
                std::uint64_t generation);
    void update_irq();
    void drive_irq();
    bool superseded(std::uint64_t generation) const noexcept
    {
        return generation != generation_;
    }

    image_transform_config config_;
    descriptor programmed_{};
    descriptor active_{};

    std::uint32_t status_ = 0;
    std::uint32_t irq_enable_ = 0;
    error_cause error_cause_ = error_cause::none;
    std::uint64_t bytes_done_ = 0;
    std::uint64_t local_requests_ = 0;
    std::uint64_t local_bytes_ = 0;
    std::uint64_t job_count_ = 0;
    std::uint64_t error_count_ = 0;
    std::uint64_t abort_count_ = 0;
    std::uint64_t overrun_count_ = 0;

    std::uint64_t generation_ = 0;
    std::uint64_t active_generation_ = 0;
    std::uint64_t bytes_owner_generation_ = 0;
    std::uint64_t traffic_epoch_ = 0;
    bool start_pending_ = false;
    sc_core::sc_event start_event_;
    sc_core::sc_event irq_event_;

    std::vector<unsigned char> input_staging_;
    std::vector<unsigned char> output_row_;
};

} // namespace cdc::components::tpu_v3::transform
