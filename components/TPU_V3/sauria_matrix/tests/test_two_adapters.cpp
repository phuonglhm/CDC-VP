// SPDX-License-Identifier: Apache-2.0
//
// Two full adapter/composition instances: the Phase 5 elaboration and host-
// memory scalability point corresponding to the two NEO-COREs in one chip.

#include <iostream>

#include <systemc>

#include "tpu_v3/sauria/sauria_matrix_adapter.h"
#include "tpu_v3/sram/native_port.h"

namespace sa = cdc::components::tpu_v3::sauria;
namespace sram = cdc::components::tpu_v3::sram;

namespace {

using adapter_t = sa::sauria_matrix_adapter<
    sa::columns, sa::rows, sa::activation_t, sa::weight_t, sa::accumulator_t,
    /*SRAMA_CAP=*/1024, /*SRAMB_CAP=*/1024, /*SRAMC_CAP=*/2048>;

class idle_memory : public sc_core::sc_module,
                    public virtual sram::neo_local_sram_if {
public:
    explicit idle_memory(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

    void b_access(const sram::neo_local_request&,
                  sram::neo_local_response& response,
                  sc_core::sc_time&) override
    {
        response.status = sram::neo_status::aborted;
    }

    std::uint32_t dbg_access(const sram::neo_local_request&) override { return 0; }
};

} // namespace

int sc_main(int, char*[])
{
    idle_memory memory("memory");
    sa::adapter_config config;
    config.sram_base = 0x1000'0000;
    config.sram_window = 16 * 1024 * 1024;

    adapter_t core0_sa("core0_sa", config);
    adapter_t core1_sa("core1_sa", config);
    sc_core::sc_clock clock("clock", 10, sc_core::SC_NS);
    sc_core::sc_signal<bool> reset0("reset0"), reset1("reset1");

    core0_sa.i_clk(clock);
    core0_sa.i_rstn(reset0);
    core0_sa.local_port.bind(memory);
    core1_sa.i_clk(clock);
    core1_sa.i_rstn(reset1);
    core1_sa.local_port.bind(memory);

    reset0.write(false);
    reset1.write(false);
    sc_core::sc_start(100, sc_core::SC_NS);
    reset0.write(true);
    reset1.write(true);
    sc_core::sc_start(200, sc_core::SC_NS);

    const auto id0 = core0_sa.identity();
    const auto id1 = core1_sa.identity();
    if (id0.rows != 64 || id0.columns != 64 || id1.rows != 64
        || id1.columns != 64 || id0.source_revision != id1.source_revision) {
        std::cerr << "FAIL: two adapters do not report one pinned geometry/source\n";
        return 1;
    }
    std::cout << "two full Sauria adapters elaborated: " << core0_sa.name()
              << ", " << core1_sa.name() << " at " << sc_core::sc_time_stamp()
              << '\n';
    return 0;
}
