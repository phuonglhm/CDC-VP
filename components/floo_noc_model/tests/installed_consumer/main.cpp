// SPDX-License-Identifier: Apache-2.0

#include <floo_noc_model/noc_interconnect.h>

#include <iostream>

int sc_main(int, char**)
{
    using cdc::components::noc_interconnect;
    if (noc_interconnect::tlm_status_for(0) != tlm::TLM_OK_RESPONSE
        || noc_interconnect::tlm_status_for(2)
            != tlm::TLM_GENERIC_ERROR_RESPONSE) {
        return 1;
    }
    std::cout << "installed noc_interconnect consumer PASS\n";
    return 0;
}
