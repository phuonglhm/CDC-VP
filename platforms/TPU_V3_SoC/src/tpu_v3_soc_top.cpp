// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3_soc_top.h"

#include <sstream>
#include <utility>

namespace cdc::platforms::tpu_v3_soc {

namespace tpu = cdc::components::tpu_v3;

tpu_v3_soc_top::tpu_v3_soc_top(sc_core::sc_module_name name,
                               tpu::tpu_soc_config config)
    : sc_core::sc_module(name)
    , config_(std::move(config))
{
    // Throws std::invalid_argument naming the offending field. Letting it
    // escape during elaboration is intended: a half-built platform is not
    // something to continue from.
    config_.validate();
}

std::string tpu_v3_soc_top::report() const
{
    std::ostringstream out;

    out << tpu::describe(config_);

    out << "\nInstantiated in this build\n"
        << "  configuration model  : yes\n"
        << "  address map          : yes (validated, non-overlapping)\n"
        << "  SystemC elaboration  : yes\n"
        << "  TPU cores            : no  (Phase 5)\n"
        << "  MXUs                 : no  (Phase 4)\n"
        << "  SVM                  : no  (Phase 3)\n"
        << "  RV32GCV hart         : no  (Phase 2)\n"
        << "  NoC / global memory  : no  (Phase 7)\n"
        << '\n'
        << "This is the Phase 1 skeleton. It reports the architecture it is\n"
        << "configured for; it does not simulate it yet.\n";

    return out.str();
}

void tpu_v3_soc_top::start_of_simulation()
{
    started_ = true;
}

void tpu_v3_soc_top::end_of_simulation()
{
    finished_ = true;
}

} // namespace cdc::platforms::tpu_v3_soc
