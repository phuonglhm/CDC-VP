#include "top.h"

Top::Top(sc_module_name name) : 
    sc_module(name),
    rec_intra("rec_intra"),
    rec_mc ("rec_mc"),
    rec_tq ("rec_tq"),
    inv_tq ("inv_tq"),
    res_buffer("res_buffer"),
    rec_mv(1024),
    rec_mem("rec_mem", 256, 256, true, 128)

{
    rec_intra.buffer_socket.bind(res_buffer.intra_socket);
    rec_mc.buffer_socket.bind(res_buffer.mc_socket);
    res_buffer.buffer_socket.bind(rec_tq.buffer_socket);
    rec_tq.inv_tq_socket.bind(inv_tq.tq_socket);

    rec_intra.bindMemory(rec_mem);
    rec_mc.bindMemory(rec_mem);
    res_buffer.bindMemory(rec_mem);
    rec_mc.bindMvMemory(rec_mv);

}