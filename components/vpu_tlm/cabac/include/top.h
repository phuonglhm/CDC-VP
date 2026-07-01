#ifndef TOP_H
#define TOP_H

#include <systemc>
#include "rec_mem_target.h"
#include "simple_memory.h"
#include "cabac.h"
using namespace sc_core;

class Top : public sc_module {
    public:
    // Memory bridge and external memory target for CABAC tables
    MemBridge mem_bridge;
    SimpleMemory simple_mem;
    Cabac cabac;

    Top(sc_module_name name);
};

#endif