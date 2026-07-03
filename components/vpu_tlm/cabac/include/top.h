#ifndef TOP_H
#define TOP_H

#include <systemc>
#include "mem_bridge.h"
#include "simple_memory.h"
#include "cabac.h"
using namespace sc_core;

class Top : public sc_module {
    public:
    MemBridge mem_bridge;
    SimpleMemory simple_mem;
    Cabac cabac;

    Top(sc_module_name name);
};

#endif