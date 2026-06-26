#include "pwm.h"
#include <iostream>

PWM::PWM(sc_module_name name)
: sc_module(name),
  socket("socket"),
  pwm_out("pwm_out")
{
    socket.register_b_transport(
        this,
        &PWM::b_transport
    );

    alert_test = 0;
    regwen     = 1;

    // OpenTitan reset value of CFG is 0x38008000.
    // CNTR_EN = 0, DC_RESN = 7, CLK_DIV = 0x8000.
    cfg        = 0x38008000;

    pwm_en     = 0;
    invert     = 0;

    for(int i = 0; i < 6; i++)
    {
        pwm_param[i]   = 0;
        duty_cycle[i]  = 0;
        blink_param[i] = 0;
    }

    counter = 0;

    SC_THREAD(pwm_thread);
}

uint64_t PWM::calc_period() const
{
    uint32_t dc_resn =
        (cfg >> 27) & 0xF;

    uint32_t clk_div =
        cfg & 0x07FFFFFF;

    uint64_t period =
        (1ULL << (dc_resn + 1)) *
        static_cast<uint64_t>(clk_div + 1);

    if(period == 0)
    {
        period = 1;
    }

    return period;
}

void PWM::b_transport(
    tlm_generic_payload& trans,
    sc_time& delay)
{
    uint64_t addr =
        trans.get_address();

    unsigned char* ptr =
        trans.get_data_ptr();

    uint32_t* data =
        reinterpret_cast<uint32_t*>(ptr);

    if(trans.get_data_length() != 4)
    {
        trans.set_response_status(
            TLM_BURST_ERROR_RESPONSE
        );
        return;
    }

    if(addr % 4 != 0)
    {
        trans.set_response_status(
            TLM_ADDRESS_ERROR_RESPONSE
        );
        return;
    }

    if(trans.is_write())
    {
        switch(addr)
        {
        case REG_ALERT_TEST:
            alert_test = *data;
            break;

        case REG_REGWEN:
            regwen = *data & 0x1;
            break;

        case REG_CFG:
            if(regwen & 0x1)
            {
                cfg = *data;
            }
            break;

        case REG_PWM_EN:
            if(regwen & 0x1)
            {
                pwm_en = *data & 0x3F;
            }
            break;

        case REG_INVERT:
            if(regwen & 0x1)
            {
                invert = *data & 0x3F;
            }
            break;

        case REG_PWM_PARAM0:
            if(regwen & 0x1)
            {
                pwm_param[0] = *data;
            }
            break;

        case REG_PWM_PARAM1:
            if(regwen & 0x1)
            {
                pwm_param[1] = *data;
            }
            break;

        case REG_PWM_PARAM2:
            if(regwen & 0x1)
            {
                pwm_param[2] = *data;
            }
            break;

        case REG_PWM_PARAM3:
            if(regwen & 0x1)
            {
                pwm_param[3] = *data;
            }
            break;

        case REG_PWM_PARAM4:
            if(regwen & 0x1)
            {
                pwm_param[4] = *data;
            }
            break;

        case REG_PWM_PARAM5:
            if(regwen & 0x1)
            {
                pwm_param[5] = *data;
            }
            break;

        case REG_DUTY0:
            if(regwen & 0x1)
            {
                duty_cycle[0] = *data;
            }
            break;

        case REG_DUTY1:
            if(regwen & 0x1)
            {
                duty_cycle[1] = *data;
            }
            break;

        case REG_DUTY2:
            if(regwen & 0x1)
            {
                duty_cycle[2] = *data;
            }
            break;

        case REG_DUTY3:
            if(regwen & 0x1)
            {
                duty_cycle[3] = *data;
            }
            break;

        case REG_DUTY4:
            if(regwen & 0x1)
            {
                duty_cycle[4] = *data;
            }
            break;

        case REG_DUTY5:
            if(regwen & 0x1)
            {
                duty_cycle[5] = *data;
            }
            break;

        case REG_BLINK_PARAM0:
            if(regwen & 0x1)
            {
                blink_param[0] = *data;
            }
            break;

        case REG_BLINK_PARAM1:
            if(regwen & 0x1)
            {
                blink_param[1] = *data;
            }
            break;

        case REG_BLINK_PARAM2:
            if(regwen & 0x1)
            {
                blink_param[2] = *data;
            }
            break;

        case REG_BLINK_PARAM3:
            if(regwen & 0x1)
            {
                blink_param[3] = *data;
            }
            break;

        case REG_BLINK_PARAM4:
            if(regwen & 0x1)
            {
                blink_param[4] = *data;
            }
            break;

        case REG_BLINK_PARAM5:
            if(regwen & 0x1)
            {
                blink_param[5] = *data;
            }
            break;

        default:
            trans.set_response_status(
                TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }
    }
    else
    {
        switch(addr)
        {
        case REG_ALERT_TEST:
            *data = alert_test;
            break;

        case REG_REGWEN:
            *data = regwen;
            break;

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
            *data = pwm_param[0];
            break;

        case REG_PWM_PARAM1:
            *data = pwm_param[1];
            break;

        case REG_PWM_PARAM2:
            *data = pwm_param[2];
            break;

        case REG_PWM_PARAM3:
            *data = pwm_param[3];
            break;

        case REG_PWM_PARAM4:
            *data = pwm_param[4];
            break;

        case REG_PWM_PARAM5:
            *data = pwm_param[5];
            break;

        case REG_DUTY0:
            *data = duty_cycle[0];
            break;

        case REG_DUTY1:
            *data = duty_cycle[1];
            break;

        case REG_DUTY2:
            *data = duty_cycle[2];
            break;

        case REG_DUTY3:
            *data = duty_cycle[3];
            break;

        case REG_DUTY4:
            *data = duty_cycle[4];
            break;

        case REG_DUTY5:
            *data = duty_cycle[5];
            break;

        case REG_BLINK_PARAM0:
            *data = blink_param[0];
            break;

        case REG_BLINK_PARAM1:
            *data = blink_param[1];
            break;

        case REG_BLINK_PARAM2:
            *data = blink_param[2];
            break;

        case REG_BLINK_PARAM3:
            *data = blink_param[3];
            break;

        case REG_BLINK_PARAM4:
            *data = blink_param[4];
            break;

        case REG_BLINK_PARAM5:
            *data = blink_param[5];
            break;

        default:
            trans.set_response_status(
                TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }
    }

    delay += sc_time(10, SC_NS);

    trans.set_response_status(
        TLM_OK_RESPONSE
    );
}

void PWM::pwm_thread()
{
    while(true)
    {
        wait(10, SC_NS);

        bool counter_enable =
            ((cfg >> 31) & 0x1);

        if(!counter_enable)
        {
            pwm_out.write(false);
            continue;
        }

        if((pwm_en & 0x1) == 0)
        {
            pwm_out.write(false);
            continue;
        }

        uint64_t period =
            calc_period();

        counter =
            (counter + 1) % period;

        // Channel 0 only.
        // DUTY_CYCLE_0[15:0] = A.
        // DUTY_CYCLE_0[31:16] = B.
        // Normal mode uses A.
        uint16_t duty_a =
            duty_cycle[0] & 0xFFFF;

        uint64_t duty_count =
            (static_cast<uint64_t>(duty_a) * period) >> 16;

        // PWM_PARAM_0[15:0] = PHASE_DELAY.
        uint16_t phase_raw =
            pwm_param[0] & 0xFFFF;

        uint64_t phase_count =
            (static_cast<uint64_t>(phase_raw) * period) >> 16;

        uint64_t shifted =
            (counter + period - phase_count) % period;

        bool pwm =
            shifted < duty_count;

        if(invert & 0x1)
        {
            pwm = !pwm;
        }

        pwm_out.write(pwm);

        std::cout
            << sc_time_stamp()
            << " counter="
            << counter
            << " period="
            << period
            << " duty_raw=0x"
            << std::hex
            << duty_a
            << std::dec
            << " duty_count="
            << duty_count
            << " phase_count="
            << phase_count
            << " pwm="
            << pwm
            << std::endl;
    }
}
