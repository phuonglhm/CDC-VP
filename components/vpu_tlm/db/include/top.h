#ifndef VPU_TLM_DB_TOP_H
#define VPU_TLM_DB_TOP_H

#include <systemc>
#include "db_bs.h"
#include "db_mv.h"
#include "db_filter_sao.h"

class DbTop : public sc_core::sc_module {
public:
    explicit DbTop(sc_core::sc_module_name name);

    BorderStrength db_bs;
    DbMotionVector db_mv;
    Filter_SAO db_filter_sao;
};

#endif
