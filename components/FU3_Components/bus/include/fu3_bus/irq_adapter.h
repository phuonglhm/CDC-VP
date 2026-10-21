#pragma once
#include <systemc>

namespace fu3_bus {
// Functional pulse-to-level handshake. No physical CDC latency is claimed.
// The platform acknowledges after servicing the source, before PLIC completion.
class IrqAdapter : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> source{"source"}, reset_n{"reset_n"};
    sc_core::sc_out<bool> irq{"irq"};
    SC_HAS_PROCESS(IrqAdapter);
    explicit IrqAdapter(sc_core::sc_module_name name):sc_module(name) {
        SC_METHOD(update);sensitive<<source<<reset_n<<changed_;
    }
    void acknowledge(){clear_=true;changed_.notify(sc_core::SC_ZERO_TIME);}
private:
    bool pending_=false,clear_=false;
    sc_core::sc_event changed_;
    void update(){
        if(!reset_n.read())pending_=false;
        else {if(clear_)pending_=false;if(source.read())pending_=true;}
        clear_=false;irq.write(pending_);
    }
};
}
