#ifndef VPU_TLM_FETCH_TOP_H
#define VPU_TLM_FETCH_TOP_H

#include <systemc>
#include "fetch_wrapper_tlm.h"

class FetchTop : public sc_core::sc_module {
public:
    // Wrapper around the functional fetch-side frame loader/memory path.
    FetchWrapper fetch;

    explicit FetchTop(sc_core::sc_module_name name);
};

#endif
