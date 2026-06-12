#ifndef PWM_H
#define PWM_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>

using namespace sc_core;
using namespace tlm;

class PWM : public sc_module
{
public:

    tlm_utils::simple_target_socket<PWM> socket;

    sc_out<bool> pwm_out;

    SC_HAS_PROCESS(PWM);

    PWM(sc_module_name name);

    void b_transport(tlm_generic_payload& trans,
                     sc_time& delay);

private:

    enum
    {
        REG_CFG         = 0x08,
        REG_PWM_EN      = 0x0C,
        REG_INVERT      = 0x10,
        REG_PWM_PARAM0  = 0x14,
        REG_DUTY0       = 0x2C,
        REG_PERIOD      = 0x30
    };

    uint32_t cfg;
    uint32_t pwm_en;
    uint32_t invert;
    uint32_t phase_delay;
    uint32_t duty_cycle;
    uint32_t period;

    uint16_t counter;

    void pwm_thread();
};

#endif
