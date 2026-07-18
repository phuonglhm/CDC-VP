#pragma once

#include <memory>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

// TLM-2.0 integration wrapper for the signal-level SAURIA v4 NPU model.
//
// Software programs physical RAM addresses through the 64 KiB MMIO target.
// A worker thread stages matrices into the core's private SRAMs, runs the
// cycle-accurate INT8/INT8/INT32 core, writes the result back through the
// master socket, and raises a level-sensitive interrupt.
class npu_tlm_v4_model : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<npu_tlm_v4_model> target_socket;
    tlm_utils::simple_initiator_socket<npu_tlm_v4_model> master_socket;
    sc_core::sc_in<bool> reset_n;
    sc_core::sc_out<bool> irq_out;

    SC_HAS_PROCESS(npu_tlm_v4_model);

    explicit npu_tlm_v4_model(
        sc_core::sc_module_name name,
        sc_core::sc_time core_clock_period = sc_core::sc_time(2, sc_core::SC_NS),
        sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));
    ~npu_tlm_v4_model() override;

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    void drive_irq();
    void worker_thread();
};

} // namespace cdc::components
