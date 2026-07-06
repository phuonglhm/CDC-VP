#ifndef TOP_H
#define TOP_H

#include <systemc>
#include "fetch_wrapper_tlm.h"
using namespace sc_core;

class Top : public sc_module {
    public:
    // Memory bridge and external memory target for CABAC tables
    FetchWrapper fetch;

    Top(sc_module_name name);
};

#endif