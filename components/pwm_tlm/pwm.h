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
        REG_ALERT_TEST   = 0x00,
        REG_REGWEN       = 0x04,
        REG_CFG          = 0x08,
        REG_PWM_EN       = 0x0C,
        REG_INVERT       = 0x10,

        REG_PWM_PARAM0   = 0x14,
        REG_PWM_PARAM1   = 0x18,
        REG_PWM_PARAM2   = 0x1C,
        REG_PWM_PARAM3   = 0x20,
        REG_PWM_PARAM4   = 0x24,
        REG_PWM_PARAM5   = 0x28,

        REG_DUTY0        = 0x2C,
        REG_DUTY1        = 0x30,
        REG_DUTY2        = 0x34,
        REG_DUTY3        = 0x38,
        REG_DUTY4        = 0x3C,
        REG_DUTY5        = 0x40,

        REG_BLINK_PARAM0 = 0x44,
        REG_BLINK_PARAM1 = 0x48,
        REG_BLINK_PARAM2 = 0x4C,
        REG_BLINK_PARAM3 = 0x50,
        REG_BLINK_PARAM4 = 0x54,
        REG_BLINK_PARAM5 = 0x58
    };

    uint32_t alert_test;
    uint32_t regwen;
    uint32_t cfg;
    uint32_t pwm_en;
    uint32_t invert;

    uint32_t pwm_param[6];
    uint32_t duty_cycle[6];
    uint32_t blink_param[6];

    uint64_t counter;

    void pwm_thread();

    uint64_t calc_period() const;
};

#endif
