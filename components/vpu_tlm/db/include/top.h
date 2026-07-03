#ifndef TOP_H
#define TOP_H

#include <systemc>
#include "db_bs.h"
#include "db_mv.h"
#include "db_filter_sao.h"

using namespace sc_core;

class Top : public sc_module {
  public:
    Top(sc_module_name name);

    BorderStrength db_bs;
    MotionVector db_mv;
    Filter_SAO db_filter_sao;
};

#endif // TOP_H
