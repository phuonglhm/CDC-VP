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
}

namespace OPS {
    constexpr uint32_t ENABLE = 0x1;
    constexpr uint32_t EX_EN = 0x2;
    constexpr uint32_t EX_CLK = 0x4;
    constexpr uint32_t INTR_EN = 0x8;
}

SC_MODULE(Timer) {
    public:
        tlm_utils::simple_target_socket<Timer> socket;
        sc_in<bool> prstn;
        sc_in<bool> extin;
        sc_out<bool> timerint;

        void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
        SC_HAS_PROCESS(Timer);
        Timer(sc_module_name name, sc_time period = sc_time(20, SC_NS)) : socket("socket"), ctrl_reg(0), value_reg(0), reload_reg(0), intr_status(false), tick_period(period) {
            cout << "Timer initiated." << endl;
            socket.register_b_transport(this, &Timer::b_transport);
            SC_THREAD(timer_thread);
        }

    private:
        uint32_t ctrl_reg;
        uint32_t value_reg;
        uint32_t reload_reg;
        bool intr_status;

        sc_event intr_clear;
        sc_event update_reg;
        sc_time tick_period;

        bool intr_clear_inc = false;

        void reset();
        void timer_thread();
};
