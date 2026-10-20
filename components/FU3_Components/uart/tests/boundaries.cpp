#include "support/fixture.h"
#include <vector>

struct Bench : sc_core::sc_module, Checks {
    fu3_uart::Uart dut{"dut"}; // Default profile deliberately leaves auto-CTS off.
    Pins pins{"pins"};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    sc_core::sc_event launch;
    bool cancel=false;
    unsigned completed=0;
    std::vector<unsigned> order;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name name):sc_module(name) {
        pins.bind(dut);cpu.bind(dut.socket);
        SC_THREAD(run);SC_THREAD(worker0);SC_THREAD(worker1);SC_THREAD(watchdog);
    }
    void settle(){for(unsigned i=0;i<5;++i)wait(sc_core::SC_ZERO_TIME);}
    unsigned access(bool write,unsigned address,unsigned value=0,tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE) {
        unsigned char byte=static_cast<unsigned char>(value);tlm::tlm_generic_payload tx;
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);tx.set_address(address);
        tx.set_data_ptr(&byte);tx.set_data_length(1);tx.set_streaming_width(1);auto delay=sc_core::SC_ZERO_TIME;
        cpu->b_transport(tx,delay);settle();expect(tx.get_response_status()==expected,"boundary MMIO result");
        expect(delay==sc_core::SC_ZERO_TIME,"boundary delay consumed");return byte;
    }
    void w(unsigned a,unsigned v){access(true,a,v);}unsigned r(unsigned a){return access(false,a);}
    void cold(){pins.rx=true;pins.reset=false;wait(30,sc_core::SC_NS);pins.reset=true;wait(30,sc_core::SC_NS);}
    void configure(unsigned format=3,unsigned divisor=1) {
        w(3,128|format);w(0,divisor);w(1,0);w(2,0x21);w(3,format);
    }
    void worker(unsigned value) {
        while(true){wait(launch);access(true,0,value,cancel?tlm::TLM_GENERIC_ERROR_RESPONSE:tlm::TLM_OK_RESPONSE);
            order.push_back(value);++completed;}
    }
    void worker0(){worker(0x35);}void worker1(){worker(0x6a);}
    void await_workers(){while(completed<2)wait(10,sc_core::SC_NS);}
    // Frames have no extra idle gap. Errors belong to each frame, not a global latch.
    void send(unsigned data,bool parity_error=false) {
        const sc_core::sc_time bit(160,sc_core::SC_NS);
        pins.rx=false;wait(bit);unsigned ones=0;
        for(unsigned i=0;i<8;++i){pins.rx=(data>>i)&1;ones+=(data>>i)&1;wait(bit);}
        pins.rx=bool(ones&1)^parity_error;wait(bit);pins.rx=true;wait(bit);
    }
    void run() {
        cold();configure(3,0);w(4,16);launch.notify(sc_core::SC_ZERO_TIME);await_workers();
        expect(!(r(5)&32)&&pins.tx,"zero divisor queues without transmission");
        w(3,131);w(0,1);w(3,3);wait(4,sc_core::SC_US);
        expect(r(0)==order[0]&&r(0)==order[1],"concurrent callers each commit exactly once in admitted order");
        expect(!(r(5)&1),"no duplicated writes from concurrent callers");

        cold();configure();completed=0;order.clear();cancel=true;
        // Last MMIO ends on an edge; both requests launch before the next edge.
        launch.notify(sc_core::SC_ZERO_TIME);wait(1,sc_core::SC_NS);pins.reset=false;
        await_workers();wait(20,sc_core::SC_NS);pins.reset=true;wait(20,sc_core::SC_NS);
        expect(r(5)==0x60&&r(3)==0,"reset cancels queued requests without stale writes");
        w(7,0x72);pins.reset=false;wait(1,sc_core::SC_NS);pins.reset=true;wait(20,sc_core::SC_NS);
        expect(r(7)==0x72,"reset pulse missing clock edge has no synchronous effect");

        cold();configure(27);w(1,15);pins.dsr=false;wait(20,sc_core::SC_NS);
        send(0x63,true);send(0x96,true);
        expect((r(2)&15)==6,"line status beats RDA, THRE and modem");
        expect(r(5)&4,"first FIFO entry has parity error");
        expect((r(2)&15)==4,"RDA beats THRE and modem after LSR acknowledge");
        expect(r(0)==0x63&&(r(2)&15)==6,"next erroneous head rearms line status");
        r(5);expect(r(0)==0x96,"back-to-back frames preserve order");
        expect((r(2)&15)==2&&(r(2)&15)==2,"THRE is not acknowledged while modem interrupt also pending");
        r(6);expect((r(2)&15)==2&&(r(2)&15)==1,"THRE acknowledges when it becomes sole source");
        expect(!(r(5)&128),"FIFO error summary clears once erroneous entries leave");

        // Default CTS is inactive; without platform auto-flow, normal TX still works.
        cold();configure();w(0,0x45);expect(!pins.tx,"default configuration does not silently gate TX by CTS");
        wait(2,sc_core::SC_US);expect(r(5)&64,"default TX completes");
        // Malformed requests yield and preserve SCR.
        w(7,0x39);
        unsigned char value=0;tlm::tlm_generic_payload tx;tx.set_address(7);tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_data_length(1);tx.set_streaming_width(1);auto delay=sc_core::sc_time(27,sc_core::SC_NS);
        auto before=sc_core::sc_time_stamp();cpu->b_transport(tx,delay);
        expect(tx.get_response_status()==tlm::TLM_GENERIC_ERROR_RESPONSE&&delay==sc_core::SC_ZERO_TIME&&
            sc_core::sc_time_stamp()-before==sc_core::sc_time(37,sc_core::SC_NS),"null payload consumes delay once and yields");
        tx.set_data_ptr(&value);tx.set_command(tlm::TLM_IGNORE_COMMAND);cpu->b_transport(tx,delay);
        expect(tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE,"ignore command rejected");
        tx.set_command(tlm::TLM_READ_COMMAND);tx.set_streaming_width(0);cpu->b_transport(tx,delay);
        expect(tx.get_response_status()==tlm::TLM_BURST_ERROR_RESPONSE,"wrapping payload rejected");
        tx.set_streaming_width(1);tx.set_byte_enable_ptr(&value);tx.set_byte_enable_length(0);cpu->b_transport(tx,delay);
        expect(tx.get_response_status()==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE&&r(7)==0x39,"malformed byte enable rejected without mutation");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"boundary simulation watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**) {
    Bench bench{"bench"};sc_core::sc_start();
    std::cout<<"UART boundaries: "<<bench.checks<<" checks, "<<bench.failures<<" failures\n";
    return bench.failures||!bench.finished?1:0;
}
