#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <cstdint>

#include "pwm.h"

using namespace sc_core;
using namespace tlm;

class TB : public sc_module
{
public:
    tlm_utils::simple_initiator_socket<TB> socket;

    SC_HAS_PROCESS(TB);

    TB(sc_module_name name)
    : sc_module(name),
      socket("socket")
    {
        SC_THREAD(run);
    }

    void write_reg(uint32_t addr, uint32_t value)
    {
        tlm_generic_payload trans;

        sc_time delay =
            SC_ZERO_TIME;

        trans.set_command(
            TLM_WRITE_COMMAND
        );

        trans.set_address(addr);

        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );

        trans.set_data_length(4);

        socket->b_transport(
            trans,
            delay
        );

        if(trans.get_response_status() != TLM_OK_RESPONSE)
        {
            std::cout
                << "WRITE ERROR addr=0x"
                << std::hex
                << addr
                << " value=0x"
                << value
                << std::dec
                << std::endl;
        }

        wait(delay);
    }

    uint32_t read_reg(uint32_t addr)
    {
        tlm_generic_payload trans;

        sc_time delay =
            SC_ZERO_TIME;

        uint32_t value = 0;

        trans.set_command(
            TLM_READ_COMMAND
        );

        trans.set_address(addr);

        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );

        trans.set_data_length(4);

        socket->b_transport(
            trans,
            delay
        );

        if(trans.get_response_status() != TLM_OK_RESPONSE)
        {
            std::cout
                << "READ ERROR addr=0x"
                << std::hex
                << addr
                << std::dec
                << std::endl;
        }

        wait(delay);

        return value;
    }

    void run()
    {
        const uint32_t REG_CFG        = 0x08;
        const uint32_t REG_PWM_EN     = 0x0C;
        const uint32_t REG_INVERT     = 0x10;
        const uint32_t REG_PWM_PARAM0 = 0x14;
        const uint32_t REG_DUTY0      = 0x2C;

        const uint32_t CFG_CNTR_EN    = 1u << 31;

        // DC_RESN = 7 gives period = 2^(7 + 1) * (CLK_DIV + 1)
        // CLK_DIV = 0 gives period = 256 ticks.
        const uint32_t DC_RESN_7      = 7u << 27;
        const uint32_t CLK_DIV_0      = 0u;

        wait(100, SC_NS);

        std::cout
            << "\n========== Read reset registers ==========\n";

        std::cout
            << "CFG reset = 0x"
            << std::hex
            << read_reg(REG_CFG)
            << std::dec
            << std::endl;

        std::cout
            << "\n========== TC1: Counter enable, PWM channel disable ==========\n";

        write_reg(REG_CFG, CFG_CNTR_EN | DC_RESN_7 | CLK_DIV_0);
        write_reg(REG_PWM_EN, 0x0);
        write_reg(REG_INVERT, 0x0);
        write_reg(REG_PWM_PARAM0, 0x00000000);
        write_reg(REG_DUTY0, 0x00008000); // 50%

        wait(1000, SC_NS);

        std::cout
            << "\n========== TC2: Duty 40% ==========\n";

        write_reg(REG_PWM_EN, 0x1);
        write_reg(REG_INVERT, 0x0);
        write_reg(REG_PWM_PARAM0, 0x00000000);

        // 40% of 0x10000 approximately = 0x6666
        write_reg(REG_DUTY0, 0x00006666);

        wait(3000, SC_NS);

        std::cout
            << "\n========== TC3: Duty 75% ==========\n";

        // 75% = 0xC000
        write_reg(REG_DUTY0, 0x0000C000);

        wait(3000, SC_NS);

        std::cout
            << "\n========== TC4: Phase delay 20% ==========\n";

        // 20% of 0x10000 approximately = 0x3333
        write_reg(REG_PWM_PARAM0, 0x00003333);

        wait(3000, SC_NS);

        std::cout
            << "\n========== TC5: Invert ==========\n";

        write_reg(REG_INVERT, 0x1);

        wait(3000, SC_NS);

        std::cout
            << "\n========== TC6: Duty 0% ==========\n";

        write_reg(REG_INVERT, 0x0);
        write_reg(REG_PWM_PARAM0, 0x00000000);
        write_reg(REG_DUTY0, 0x00000000);

        wait(3000, SC_NS);

        std::cout
            << "\n========== TC7: Duty nearly 100% ==========\n";

        // OpenTitan 16-bit duty max is 0xFFFF.
        // It is almost 100%.
        write_reg(REG_DUTY0, 0x0000FFFF);

        wait(3000, SC_NS);

        std::cout
            << "\n========== TC8: Disable counter ==========\n";

        write_reg(REG_CFG, 0x0);

        wait(1000, SC_NS);

        sc_stop();
    }
};

int sc_main(int argc, char* argv[])
{
    PWM pwm("PWM");

    TB tb("TB");

    sc_signal<bool> pwm_sig;

    pwm.pwm_out(pwm_sig);

    tb.socket.bind(
        pwm.socket
    );

    sc_trace_file* tf;

    tf =
        sc_create_vcd_trace_file(
            "wave"
        );

    sc_trace(
        tf,
        pwm_sig,
        "pwm_out"
    );

    sc_start();

    sc_close_vcd_trace_file(
        tf
    );

    return 0;
}
