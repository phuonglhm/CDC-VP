#ifndef TOP_H
#define TOP_H

#include <systemc>
#include <iostream>
#include "rec_intra.h"
#include "rec_mc.h"
#include "rec_mc.h"
#include "rec_mv.h"
#include "rec_tq.h"
#include "rec_inv_tq.h"
#include "res_buffer.h"
#include "rec_mem_target.h"
using namespace sc_core;

class Top : public sc_module {
    public:
    RecIntra rec_intra;
    RecMc rec_mc;
    RecTQ rec_tq;
    InvTQ inv_tq;
    ResBuffer res_buffer;
    SimpleRecMvMem rec_mv;
    RecMemory rec_mem;

    Top(sc_module_name name);
};

#endif