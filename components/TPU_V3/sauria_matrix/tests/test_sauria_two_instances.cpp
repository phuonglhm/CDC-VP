// SPDX-License-Identifier: Apache-2.0
//
// The two-instance hygiene gate that decision record D17 requires.
//
// The pinned Sauria source writes CSV traces through function-local
// `static std::ofstream` objects. A function-local static is shared by every
// instantiation of its enclosing template, so the two matrix engines of a
// two-core NEO-CORE chip would share one file handle and one `header_written`
// flag — which `INTERFACE_CONTRACT.md` forbids, in terms that name this exact
// case.
//
// `sa_array.h`'s writers are the reachable ones: both `dump_mac_*` functions are
// called from the compute process on ordinary functional conditions, and
// `dump_mac_cell` runs in the per-processing-element inner loop with a `flush()`
// per call. The hygiene patch compiles them out.
//
// This program is the observable half of that claim. It elaborates **two**
// arrays at the pinned geometry and runs them. The gate script around it then
// checks that no `trace_sysc/` directory exists in the working directory —
// which it would, on the unpatched source, the moment either dump ran.
//
// Two instances rather than one, deliberately: a single instance would leave
// "shared between instances" untested, and that is the half of the contract
// clause that a `static` actually violates.

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <iostream>

#include <systemc>

#include "tpu_v3/sauria/sauria_geometry.h"
#include "systolic_array/sa_array.h"

namespace sauria_tpu = cdc::components::tpu_v3::sauria;

/// The pinned instantiation, named once.
///
/// Every argument comes from the extracted profile rather than from a literal,
/// so a source whose geometry moved cannot be compiled here under the old name:
/// `sauria_geometry.h` asserts the profile against plan §16, and this alias
/// asserts the instantiation against the profile.
using pinned_array =
    sauria::SystolicArray<sauria_tpu::columns, sauria_tpu::rows,
                          sauria_tpu::activation_t, sauria_tpu::weight_t,
                          sauria_tpu::accumulator_t>;

int sc_main(int, char*[])
{
    sauria::PeConfig configuration;

    // Neither name contains the substring the source's remaining (feeder and
    // PSM) trace writers gate on, and that is a rule rather than a habit —
    // see `sauria_geometry.h::reserved_trace_instance_name`.
    pinned_array first("sauria_array_0", configuration);
    pinned_array second("sauria_array_1", configuration);

    for (const char* name : {first.name(), second.name()}) {
        if (std::string(name).find(sauria_tpu::reserved_trace_instance_name)
            != std::string::npos) {
            std::cerr << "FAIL: instance '" << name << "' uses the reserved "
                      << "name that enables the source's trace writers\n";
            return 1;
        }
    }

    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> reset{"reset"};
    reset.write(true);

    first.i_clk(clock);
    first.i_rstn(reset);
    second.i_clk(clock);
    second.i_rstn(reset);

    // Every remaining port needs a binding for elaboration to complete.
    //
    // They are *not* all held idle, and that distinction is the whole gate. The
    // source's dumps fire on a column-switch rising edge —
    // `raw_cswitch_any && !raw_cswitch_q` for `dump_mac_matrix`, and
    // `local_cswitch_rise` per processing element for `dump_mac_cell`. An
    // earlier version of this test bound every control signal to its reset
    // value, so neither condition ever occurred: it passed against the
    // *unpatched* source as happily as the patched one and proved nothing.
    // `cswitch` is driven below to make the dump path actually reachable.
    sc_core::sc_signal<float> threshold{"threshold"};
    sc_core::sc_signal<uint32_t> nsplit{"nsplit"};
    sc_core::sc_signal<sauria::act_vector_t<sauria_tpu::rows, sauria_tpu::activation_t>> act_a{"act_a"}, act_b{"act_b"};
    sc_core::sc_signal<sauria::wei_vector_t<sauria_tpu::columns, sauria_tpu::weight_t>> wei_a{"wei_a"}, wei_b{"wei_b"};
    sc_core::sc_signal<sauria::psum_vector_t<sauria_tpu::rows, sauria_tpu::accumulator_t>> c_in_a{"c_in_a"}, c_in_b{"c_in_b"};
    sc_core::sc_signal<sauria::psum_vector_t<sauria_tpu::rows, sauria_tpu::accumulator_t>> c_out_a0{"c_out_a0"}, c_out_b0{"c_out_b0"};
    sc_core::sc_signal<sauria::psum_vector_t<sauria_tpu::rows, sauria_tpu::accumulator_t>> c_out_a1{"c_out_a1"}, c_out_b1{"c_out_b1"};
    sc_core::sc_signal<bool> pipeline_a{"pipeline_a"}, pipeline_b{"pipeline_b"};
    sc_core::sc_signal<bool> cscan_a{"cscan_a"}, cscan_b{"cscan_b"};
    sc_core::sc_signal<sc_dt::sc_bv<sauria_tpu::columns>> cswitch_a{"cswitch_a"}, cswitch_b{"cswitch_b"};
    sc_core::sc_signal<bool> clear_a{"clear_a"}, clear_b{"clear_b"};
    sc_core::sc_signal<uint32_t> context_a{"context_a"}, context_b{"context_b"};

    for (auto* array : {&first, &second}) {
        array->i_threshold(threshold);
        array->i_nsplit(nsplit);
        array->i_act_arr_a(act_a);
        array->i_act_arr_b(act_b);
        array->i_wei_arr_a(wei_a);
        array->i_wei_arr_b(wei_b);
        array->i_c_arr_a(c_in_a);
        array->i_c_arr_b(c_in_b);
        array->i_pipeline_en_a(pipeline_a);
        array->i_pipeline_en_b(pipeline_b);
        array->i_cscan_en_a(cscan_a);
        array->i_cscan_en_b(cscan_b);
        array->i_cswitch_arr_a(cswitch_a);
        array->i_cswitch_arr_b(cswitch_b);
        array->i_sa_clear_a(clear_a);
        array->i_sa_clear_b(clear_b);
        array->i_context_id_a(context_a);
        array->i_context_id_b(context_b);
    }
    // Outputs cannot share a signal: two drivers on one `sc_signal` is an
    // elaboration error, and it would also hide exactly the cross-instance
    // interference this gate is looking for.
    first.o_c_arr_a(c_out_a0);
    first.o_c_arr_b(c_out_b0);
    second.o_c_arr_a(c_out_a1);
    second.o_c_arr_b(c_out_b1);

    // Drive a column-switch rising edge on both lanes, with the pipeline
    // enabled, so the source would write its traces if they were still compiled
    // in. Verified by construction: against the unpatched source this stimulus
    // produces `trace_sysc/sa_macq_dump.csv`.
    pipeline_a.write(true);
    pipeline_b.write(true);
    nsplit.write(sauria_tpu::rows / 2);
    cswitch_a.write(0);
    cswitch_b.write(0);
    sc_core::sc_start(100, sc_core::SC_NS);

    sc_dt::sc_bv<sauria_tpu::columns> all_switched;
    all_switched = ~sc_dt::sc_bv<sauria_tpu::columns>(0);
    cswitch_a.write(all_switched);
    cswitch_b.write(all_switched);
    sc_core::sc_start(200, sc_core::SC_NS);

    cswitch_a.write(0);
    cswitch_b.write(0);
    sc_core::sc_start(200, sc_core::SC_NS);

    std::cout << "elaborated and ran two " << sauria_tpu::columns << 'x'
              << sauria_tpu::rows << " arrays: " << first.name() << ", "
              << second.name() << '\n'
              << "simulated time: " << sc_core::sc_time_stamp() << '\n';
    return 0;
}
