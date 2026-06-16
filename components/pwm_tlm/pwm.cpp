#include "pwm.h"
#include <iostream>

PWM::PWM(sc_module_name name)
: sc_module(name),
  socket("socket")
{
    socket.register_b_transport(
        this,
        &PWM::b_transport);

    cfg         = 0;
    pwm_en      = 0;
    invert      = 0;
    phase_delay = 0;
    duty_cycle  = 0;
    period      = 256;

    counter     = 0;

    SC_THREAD(pwm_thread);
}

void PWM::b_transport(
    tlm_generic_payload& trans,
    sc_time& delay)
{
    uint64_t addr =
        trans.get_address();

    uint32_t* data =
        reinterpret_cast<uint32_t*>(
            trans.get_data_ptr());

    if(trans.is_write())
    {
        switch(addr)
        {
        case REG_CFG:
            cfg = *data;
            break;

        case REG_PWM_EN:
            pwm_en = *data;
            break;

        case REG_INVERT:
            invert = *data;
            break;

        case REG_PWM_PARAM0:
            phase_delay = *data;
            break;

        case REG_DUTY0:
            duty_cycle = *data;
            break;

        case REG_PERIOD:
            period = *data;
            break;

        default:
            trans.set_response_status(
                TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    }
    else
    {
        switch(addr)
        {
        case REG_CFG:
            *data = cfg;
            break;

        case REG_PWM_EN:
            *data = pwm_en;
            break;

        case REG_INVERT:
            *data = invert;
            break;

        case REG_PWM_PARAM0:
            *data = phase_delay;
            break;

        case REG_DUTY0:
            *data = duty_cycle;
            break;

        case REG_PERIOD:
            *data = period;
            break;

        default:
            trans.set_response_status(
                TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    }

    delay += sc_time(10, SC_NS);

    trans.set_response_status(
        TLM_OK_RESPONSE);
}

void PWM::pwm_thread()
{
    while(true)
    {
        wait(10, SC_NS);

        bool counter_enable =
            ((cfg >> 31) & 1);

        if(!counter_enable)
            continue;

        counter =
            (counter + 1) % period;

        bool pwm = false;

        if(pwm_en & 0x1)
        {
            uint16_t shifted =
                (counter +
                 period -
                 phase_delay)
                 % period;

            if(shifted < duty_cycle)
            {
                pwm = true;
            }
        }

        if(invert & 0x1)
        {
            pwm = !pwm;
        }

        pwm_out.write(pwm);

        // std::cout
        //     << sc_time_stamp()
        //     << " counter="
        //     << counter
        //     << " pwm="
        //     << pwm
        //     << std::endl;
    }
}
