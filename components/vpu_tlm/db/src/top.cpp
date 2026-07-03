#include <systemc>
#include <iostream>
#include "top.h"
#include "db_sao.h"

using namespace sc_core;

Top::Top(sc_module_name name)
    : sc_module(name), db_bs("db_bs"), db_mv("db_mv"), db_filter_sao("db_filter_sao") {
    db_bs.filter_socket.bind(db_filter_sao.bs_socket);
    db_mv.filter_socket.bind(db_filter_sao.mv_socket);
}