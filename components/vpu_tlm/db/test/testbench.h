#ifndef TESTBENCH_H
#define TESTBENCH_H


#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "top.h"
#include "out_monitor.h"

using namespace sc_core;

class TestBench : public sc_module {
    public:
        TestBench(sc_module_name name)
            : sc_module(name),
              bs_socket("bs_socket"),
              mv_socket("mv_socket"),
              out_monitor("out_monitor"),
              top("top_test") {
            bs_socket.bind(top.db_bs.start_socket);
            mv_socket.bind(top.db_mv.start_socket);
            top.db_filter_sao.out_socket.bind(out_monitor.filter_socket);
        };
        SC_HAS_PROCESS(TestBench);
        bool dataflow_test(const CustomPacket &pkt_in);
          bool cbf_qp_param_test();
        bool pack_unpack_test();
        bool outmonitor_receive_test();

        // Additional simple unit tests
        bool filter_identity_test();
        bool filter_modifies_test();
        bool mv_selection_test();

        tlm_utils::simple_initiator_socket<TestBench> bs_socket;
        tlm_utils::simple_initiator_socket<TestBench> mv_socket;
        OutMonitor out_monitor;
        Top top;
};

#endif