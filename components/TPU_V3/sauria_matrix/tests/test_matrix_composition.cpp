// SPDX-License-Identifier: Apache-2.0
//
// The matrix-only composition elaborates, runs, and stays clean.
//
// ## Why elaboration alone is worth a test
//
// SystemC refuses to start with an unbound port, so reaching `sc_start` at all
// proves every one of the composition's 429 port bindings resolved. That is not
// a small claim: the bindings were generated from `npu_top.h` and filtered to
// the kept instances, and the failure this guards against — a module left
// dangling when its driver was excluded — would otherwise surface much later,
// as a signal stuck at its reset value producing plausible wrong arithmetic.
//
// It is also the check that keeps the audit's closure finding honest. §4 of
// `TPU_V3_PHASE5_AUDIT.md` says the kept set needs nothing from `Obp`, `Rce`,
// `ReductionEngine`, `SauriaDma` or `InstructionDecoder`. If that were wrong,
// this file would not elaborate, because the signals those modules drove would
// have no driver and no binding.
//
// What this test deliberately does **not** claim: that the engine computes
// anything. Nothing is configured and no job is submitted, so the array runs
// with its reset configuration. Correct arithmetic is the differential's job,
// once the config load and the controllers exist.

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <iostream>
#include <string>

#include <systemc>

#include "tpu_v3/sauria/matrix_composition.h"

namespace sauria_tpu = cdc::components::tpu_v3::sauria;

/// The pinned instantiation. Every geometry and type argument comes from the
/// extracted profile, never a literal — `sauria_geometry.h` explains why a
/// default here would silently build a different engine.
using engine_t = sauria_tpu::matrix_composition<
    sauria_tpu::columns, sauria_tpu::rows, sauria_tpu::activation_t,
    sauria_tpu::weight_t, sauria_tpu::accumulator_t,
    /*SRAMA_CAP=*/1024, /*SRAMB_CAP=*/1024, /*SRAMC_CAP=*/2048>;

int sc_main(int, char*[])
{
    engine_t engine("engine");

    if (std::string(engine.name())
            .find(sauria_tpu::reserved_trace_instance_name)
        != std::string::npos) {
        std::cerr << "FAIL: the composition uses the reserved instance name "
                  << "that enables the source's remaining trace writers\n";
        return 1;
    }

    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> rstn{"rstn"}, soft_reset{"soft_reset"};
    sc_core::sc_signal<bool> start{"start"};
    sc_core::sc_signal<bool> done{"done"}, deadlock{"deadlock"};
    sc_core::sc_signal<bool> host_wren{"host_wren"}, host_rden{"host_rden"};
    sc_core::sc_signal<std::uint32_t> host_addr{"host_addr"};
    sc_core::sc_signal<std::uint32_t> mvm_k{"mvm_k"};
    sc_core::sc_signal<std::uint32_t> total_contexts{"total_contexts"};
    sc_core::sc_signal<::sauria::host_data_t> host_wdata{"host_wdata"};
    sc_core::sc_signal<::sauria::host_mask_t> host_wmask{"host_wmask"};
    sc_core::sc_signal<float> threshold{"threshold"};

    engine.i_clk(clock);
    engine.i_rstn(rstn);
    engine.i_soft_reset(soft_reset);
    engine.i_start(start);
    engine.o_done(done);
    engine.o_deadlock(deadlock);
    engine.i_host_addr(host_addr);
    engine.i_host_wren(host_wren);
    engine.i_host_rden(host_rden);
    engine.i_host_wdata(host_wdata);
    engine.i_host_wmask(host_wmask);
    engine.i_threshold(threshold);
    engine.i_mvm_k(mvm_k);
    engine.i_total_contexts(total_contexts);

    // The staging store is reachable and is the composition's own storage, not
    // a separate memory the adapter has to find.
    if (engine.store.activation_capacity() != 1024
        || engine.store.weight_capacity() != 1024
        || engine.store.result_capacity() != 2048) {
        std::cerr << "FAIL: the staging store was not sized from the "
                  << "composition's template arguments\n";
        return 1;
    }

    rstn.write(false);
    sc_core::sc_start(50, sc_core::SC_NS);
    rstn.write(true);
    sc_core::sc_start(200, sc_core::SC_NS);

    std::cout << "composition elaborated and ran: " << engine.name() << '\n'
              << "geometry       : " << sauria_tpu::columns << 'x'
              << sauria_tpu::rows << '\n'
              << "simulated time : " << sc_core::sc_time_stamp() << '\n'
              << "matrix composition: elaboration PASS\n";
    return 0;
}
