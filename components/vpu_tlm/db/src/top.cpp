#include <systemc>
#include <iostream>
#include "../include/top.h"
#include "../include/db_filter_sao.h"

DbTop::DbTop(sc_core::sc_module_name name)
    : sc_core::sc_module(name), db_bs("db_bs"), db_mv("db_mv"), db_filter_sao("db_filter_sao") {
    db_bs.filter_socket.bind(db_filter_sao.bs_socket);
    db_mv.filter_socket.bind(db_filter_sao.mv_socket);
}
