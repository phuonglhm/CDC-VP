#ifndef VPU_TLM_CABAC_TOP_H
#define VPU_TLM_CABAC_TOP_H

#include <systemc>
#include "mem_bridge.h"
#include "vpu_cabac_simple_memory.h"
#include "cabac.h"

class CabacTop : public sc_core::sc_module {
public:
    CabacMemBridge mem_bridge;
    CabacSimpleMemory simple_mem;
    Cabac cabac;

    explicit CabacTop(sc_core::sc_module_name name);
};

#endif
