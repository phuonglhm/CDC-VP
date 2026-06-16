#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "pwm.h"

using namespace sc_core;
using namespace tlm;

class TB : public sc_module
{
public:

    tlm_utils::simple_initiator_socket<TB>
        socket;

    SC_HAS_PROCESS(TB);

    TB(sc_module_name name)
    : sc_module(name),
      socket("socket")
    {
        SC_THREAD(run);
    }

    void write_reg(
        uint32_t addr,
        uint32_t value)
    {
        tlm_generic_payload trans;

        sc_time delay =
            SC_ZERO_TIME;

        trans.set_command(
            TLM_WRITE_COMMAND);

        trans.set_address(addr);

        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(
                &value));

        trans.set_data_length(4);

        socket->b_transport(
            trans,
            delay);
    }

    void run()
    {
        wait(100, SC_NS);

    std::cout << "\n========== TC1: PWM Disable ==========\n";

    write_reg(0x08, 0x80000000); // counter enable
    write_reg(0x0C, 0);          // pwm disable
    write_reg(0x10, 0);
    write_reg(0x14, 0);
    write_reg(0x2C, 40);
    write_reg(0x30, 100);

    wait(1000, SC_NS);


    std::cout << "\n========== TC2: Duty 40% ==========\n";

    write_reg(0x0C, 1);          // pwm enable
    write_reg(0x10, 0);
    write_reg(0x14, 0);
    write_reg(0x2C, 40);
    write_reg(0x30, 100);

    wait(1000, SC_NS);


    std::cout << "\n========== TC3: Duty 75% ==========\n";

    write_reg(0x2C, 75);

    wait(1000, SC_NS);


    std::cout << "\n========== TC4: Phase Delay 20 ==========\n";

    write_reg(0x14, 20);

    wait(1000, SC_NS);


    std::cout << "\n========== TC5: Invert ==========\n";

    write_reg(0x10, 1);

    wait(1000, SC_NS);


    std::cout << "\n========== TC6: Duty 0% ==========\n";

    write_reg(0x10, 0);
    write_reg(0x2C, 0);

    wait(1000, SC_NS);


    std::cout << "\n========== TC7: Duty 100% ==========\n";

    write_reg(0x2C, 100);

    wait(1000, SC_NS);

    sc_stop();
    }
};

int sc_main(
    int argc,
    char* argv[])
{
    PWM pwm("PWM");

    TB tb("TB");

    sc_signal<bool> pwm_sig;

    pwm.pwm_out(pwm_sig);

    tb.socket.bind(
        pwm.socket);

    sc_trace_file* tf;

    tf =
      sc_create_vcd_trace_file(
          "wave");

    sc_trace(
        tf,
        pwm_sig,
        "pwm_out");

    sc_start();

    sc_close_vcd_trace_file(
        tf);

    return 0;
}
