#include <fu3_hyperram/controller.h>
#include <tlm_utils/simple_initiator_socket.h>
struct Top:sc_core::sc_module {
    fu3_hyperram::Controller controller{"controller"};
    tlm_utils::simple_initiator_socket<Top> memory{"memory"},csr{"csr"};
    sc_core::sc_signal<bool> reset{"reset"},device_reset{"device_reset"},irq{"irq"};
    Top(sc_core::sc_module_name name):sc_module(name){memory.bind(controller.memory);csr.bind(controller.csr);
        controller.reset_n(reset);controller.device_reset_n(device_reset);controller.irq(irq);}
};
int sc_main(int,char**){Top top{"top"};sc_core::sc_start(1,sc_core::SC_NS);return top.irq?1:0;}
