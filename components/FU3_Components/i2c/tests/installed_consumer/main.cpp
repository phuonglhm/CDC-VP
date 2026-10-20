#include <fu3_i2c/i2c.h>
#include <tlm_utils/simple_initiator_socket.h>
struct Consumer:sc_core::sc_module {
    fu3_i2c::I2c controller{"controller"};
    tlm_utils::simple_initiator_socket<Consumer> cpu{"cpu"};
    sc_core::sc_signal<bool> reset{"reset"},scl{"scl",true},sda{"sda",true},scl_oe{"scl_oe"},sda_oe{"sda_oe"},irq{"irq"};
    explicit Consumer(sc_core::sc_module_name name):sc_module(name){cpu.bind(controller.socket);
        controller.reset_n(reset);controller.scl_i(scl);controller.sda_i(sda);
        controller.scl_oe_n(scl_oe);controller.sda_oe_n(sda_oe);controller.irq(irq);}
};
int sc_main(int,char**){Consumer c{"consumer"};sc_core::sc_start(1,sc_core::SC_NS);
    return c.scl_oe&&c.sda_oe&&!c.irq&&fu3_i2c::base_address==0x50080000?0:1;}
