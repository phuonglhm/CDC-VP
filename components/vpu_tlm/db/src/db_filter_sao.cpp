#include "db_filter_sao.h"

Filter_SAO::Filter_SAO(sc_core::sc_module_name name)  : sc_module(name), out_socket("sao_socket"), bs_socket("bs_socket"), mv_socket("mv_socket") {
    bs_socket.register_b_transport(this, &Filter_SAO::b_transport);
    mv_socket.register_b_transport(this, &Filter_SAO::b_transport); //placeholder
};

void Filter_SAO::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    out_socket->b_transport(trans, delay);
}