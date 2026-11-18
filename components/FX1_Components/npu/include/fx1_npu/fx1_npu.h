// SPDX-License-Identifier: Apache-2.0
//
// FX1 NPU: TLM-2.0 integration of the SAURIA v4.7 model for platforms/fx1_soc.
//
// A clone of components/npu_tlm (left unchanged for platforms/VP_FX1_Full_SoC)
// with the FX1 integration contract (report FX1_NPU_N0 section 8):
//   - two engines of the model drop, used directly: the native top level
//     (native configuration, SRAM windows, OBP/RCE tables, the 32x32 GEMM bank)
//     and has::HasNpuTop, which runs EVERY lane-A rich push and the neo_lite
//     register-file doorbell (no routing by "extension registers written");
//   - a 16-entry accept queue: a push snapshots the registers, rebases every
//     address register against RICH_WINDOW_BASE/SIZE and bounds-checks the
//     instruction as the model decodes it; entries run one after another, each
//     staged from DDR when it starts and copied back before the next starts;
//   - a push into a full queue waits for a slot at most PUSH_WAIT_NS of
//     simulated time, then fails with a TLM error and is never enqueued later;
//   - VP-only ABORT / QUIESCENT and an identification bank (fx1_npu_regmap.h);
//   - a private DRAM image whose offset 0 is the window base and whose size
//     follows the window, not the absolute address.
// The model drop is read in place and never modified.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace fx1::npu {

struct NpuOptions {
    // System RAM that RICH windows and GEMM-bank buffers must lie in.
    std::uint64_t ram_base = 0x8000'0000ULL;
    std::uint64_t ram_size = 0x1000'0000ULL;
    // Largest RICH window (bytes); the private image never exceeds it plus the
    // model's staging area (512 KiB).
    std::uint32_t max_window = 32u << 20;
    unsigned queue_depth = 16;                  // MAS instruction queue depth
    sc_core::sc_time clock_period{1.25, sc_core::SC_NS};
    sc_core::sc_time access_latency{10, sc_core::SC_NS};
    std::uint32_t push_wait_ns = 100'000;      // reset value of VP_PUSH_WAIT_NS
    std::uint32_t dma_chunk_bytes = 4096;      // one bus transaction, aligned to its size
    // Host wall-time watchdog: while an engine works, simulated time must advance
    // within this many seconds, else the process exits with host_watchdog_exit.
    // 0 disables it.
    unsigned host_watchdog_s = 600;
    int host_watchdog_exit = 7;
    std::uint32_t model_hash = 0;              // VP_MODEL_HASH (first 32 bits of the drop manifest SHA-256)
};

struct NpuStats {
    std::uint64_t pushes_accepted = 0, pushes_refused = 0, push_timeouts = 0;
    std::uint64_t retired = 0, aborts = 0, abort_dropped = 0;
    std::uint64_t dma_read_bytes = 0, dma_write_bytes = 0, dma_transactions = 0;
    std::uint64_t native_jobs = 0, model_refusals = 0;
    std::uint64_t model_dispatches = 0;  // rich entries replayed into HasNpuTop
    std::uint64_t native_aborts = 0;     // native runs (raw START) stopped by ABORT
    std::size_t image_size = 0, image_capacity = 0, image_peak = 0;
};

class Fx1Npu : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Fx1Npu> target_socket;
    tlm_utils::simple_initiator_socket<Fx1Npu> master_socket;
    sc_core::sc_in<bool> reset_n;
    sc_core::sc_out<bool> irq_out;

    SC_HAS_PROCESS(Fx1Npu);
    Fx1Npu(sc_core::sc_module_name name, const NpuOptions& options = NpuOptions());
    ~Fx1Npu() override;

    NpuStats stats() const;
    // One line: model profile, knobs that differ from the defaults, queue, window.
    std::string banner() const;
    // One line of counters (end of run).
    std::string report() const;
    // The effective model configuration (profile, HAS_* knobs read from the
    // environment at construction), "key=value" separated by spaces.
    std::string model_config() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    void drive_irq();
};

} // namespace fx1::npu
