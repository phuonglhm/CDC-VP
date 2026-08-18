// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

// TLM-2.0 integration wrapper for the signal-level SAURIA V4.4 NPU model.
//
// The 1 MiB aperture exposes native V4.4 configuration/SRAM windows, compact
// aliases for sparse rich/OBP/RCE regions, and a CDC 64x64 GEMM bank.
// The software worker stages matrices through the master socket, runs
// the cycle-level core, writes INT32 output, and raises a level-sensitive IRQ.
class npu_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<npu_tlm> target_socket;
    tlm_utils::simple_initiator_socket<npu_tlm> master_socket;
    sc_core::sc_in<bool> reset_n;
    sc_core::sc_out<bool> irq_out;

    SC_HAS_PROCESS(npu_tlm);

    explicit npu_tlm(
        sc_core::sc_module_name name,
        sc_core::sc_time core_clock_period = sc_core::sc_time(1.25, sc_core::SC_NS),
        sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));
    ~npu_tlm() override;

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    void drive_irq();
    void worker_thread();
};

} // namespace cdc::components
