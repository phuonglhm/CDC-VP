#include <fu3_rng/rng.h>
#include <tlm_utils/simple_initiator_socket.h>
struct Top:sc_core::sc_module {
    fu3_rng::Rng rng{"rng",{sc_core::sc_time(10,sc_core::SC_NS),[](std::uint64_t i){return bool(i&1);}}};
    tlm_utils::simple_initiator_socket<Top> cpu{"cpu"};
    sc_core::sc_signal<bool> reset{"reset"},irq{"irq"};
    Top(sc_core::sc_module_name n):sc_module(n){cpu.bind(rng.socket);rng.reset_n(reset);rng.irq(irq);}
};
int sc_main(int,char**){Top t{"top"};sc_core::sc_start(20,sc_core::SC_NS);return t.irq?1:0;}
