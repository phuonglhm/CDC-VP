//author: Viet Hoang
//verified: linhtk55-fpt

#pragma once
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <iostream>
#include <cstdint>

using namespace std;
using namespace sc_core;

namespace ADDR {
    constexpr uint32_t CTRL = 0x00;
    constexpr uint32_t VALUE = 0x04;
    constexpr uint32_t RELOAD = 0x08;
    constexpr uint32_t INTSTATUS = 0x0C;

    constexpr uint32_t PID0 = 0xFE0;
    constexpr uint32_t PID1 = 0xFE4;
    constexpr uint32_t PID2 = 0xFE8;
    constexpr uint32_t PID3 = 0xFEC;
    constexpr uint32_t PID4 = 0xFD0;
    constexpr uint32_t PID5 = 0xFD4;
    constexpr uint32_t PID6 = 0xFD8;
    constexpr uint32_t PID7 = 0xFDC;

    constexpr uint32_t CID0 = 0xFF0;
    constexpr uint32_t CID1 = 0xFF4;
    constexpr uint32_t CID2 = 0xFF8;
    constexpr uint32_t CID3 = 0xFFC;
}

namespace OPS {
    constexpr uint32_t ENABLE = 0x1;
    constexpr uint32_t EX_EN = 0x2;
    constexpr uint32_t EX_CLK = 0x4;
    constexpr uint32_t INTR_EN = 0x8;
}

namespace cdc::components {
    SC_MODULE(Timer) {
        public:
            tlm_utils::simple_target_socket<Timer> socket;
            sc_in<bool> reset_n;
            sc_in<bool> extin;
            sc_out<bool> irq_out;

            void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
            SC_HAS_PROCESS(Timer);
            Timer(sc_module_name name,
                uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3,
                uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7,
                uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3,
                sc_time period = sc_time(20, SC_NS)
            ) : socket("socket"), ctrl_reg(0), value_reg(0), reload_reg(0), intr_status(false), tick_period(period),
            regPID0(p0), regPID1(p1), regPID2(p2), regPID3(p3), regPID4(p4), regPID5(p5), regPID6(p6), regPID7(p7),
            regCID0(c0), regCID1(c1), regCID2(c2), regCID3(c3) {
                cout << "Timer initiated." << endl;
                socket.register_b_transport(this, &Timer::b_transport);
                SC_THREAD(timer_thread);
            }

        private:
            uint32_t ctrl_reg;
            uint32_t value_reg;
            uint32_t reload_reg;

            const uint32_t regPID0;
            const uint32_t regPID1;
            const uint32_t regPID2;
            const uint32_t regPID3;
            const uint32_t regPID4;
            const uint32_t regPID5;
            const uint32_t regPID6;
            const uint32_t regPID7;

            const uint32_t regCID0;
            const uint32_t regCID1;
            const uint32_t regCID2;
            const uint32_t regCID3;

            bool intr_status;

            sc_event intr_clear;
            sc_event update_reg;
            sc_time tick_period;

            bool intr_clear_inc = false;

            void reset();
            void timer_thread();
    };
}