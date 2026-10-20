#include <fu3_wdt/wdt.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>

struct Bench : sc_core::sc_module {
    fu3_wdt::Watchdog dut{"dut", sc_core::sc_time(5, sc_core::SC_NS)};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    sc_core::sc_signal<bool> preset{"preset"}, wdog_reset_n{"wdog_reset_n"};
    sc_core::sc_signal<bool> unlock{"unlock"}, irq{"irq"}, reset_request{"reset_request"};
    unsigned checks=0, failures=0;
    bool finished=false;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name name) : sc_module(name) {
        cpu.bind(dut.socket);
        dut.preset_n(preset);
        dut.wdogres_n(wdog_reset_n);
        dut.i_wdt_other_unlock(unlock);
        dut.irq(irq);
        dut.reset_request(reset_request);
        SC_THREAD(run);
        SC_THREAD(watchdog);
    }
    void expect(bool ok, const char* message) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL @ " << sc_core::sc_time_stamp() << ": " << message << '\n'; }
    }
    void settle() { for (unsigned i=0;i<5;++i) wait(sc_core::SC_ZERO_TIME); }
    void cold() {
        preset=false; wdog_reset_n=false; unlock=true; settle();
        wdog_reset_n=true; preset=true; settle();
    }
    unsigned access(bool write, unsigned address, unsigned value=0,
                    unsigned length=4, unsigned char* enables=nullptr, unsigned enable_length=0,
                    tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE) {
        unsigned char bytes[4]{};
        for (unsigned i=0;i<4;++i) bytes[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;
        tx.set_address(address);
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(bytes); tx.set_data_length(length); tx.set_streaming_width(length);
        tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(enable_length);
        auto delay=sc_core::SC_ZERO_TIME;
        cpu->b_transport(tx,delay);
        settle();
        expect(tx.get_response_status()==expected,"response matches WDT V1.1 access rule");
        expect(delay==sc_core::SC_ZERO_TIME && !tx.is_dmi_allowed(),"annotation consumed and DMI disabled");
        unsigned result=0;
        for (unsigned i=0;i<4;++i) result |= unsigned(bytes[i])<<(8*i);
        return result;
    }
    unsigned peek(unsigned address) {
        unsigned char bytes[4]{};
        tlm::tlm_generic_payload tx;
        tx.set_address(address); tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(bytes); tx.set_data_length(4); tx.set_streaming_width(4);
        const auto before=sc_core::sc_time_stamp();
        expect(cpu->transport_dbg(tx)==4 && tx.is_response_ok(),"debug read succeeds");
        expect(sc_core::sc_time_stamp()==before,"debug read is untimed");
        unsigned value=0;
        for (unsigned i=0;i<4;++i) value |= unsigned(bytes[i])<<(8*i);
        return value;
    }
    void run() {
        cold();
        expect(peek(FU3_WDT_CNT_LD)==FU3_WDT_RESET_VALUE &&
               peek(FU3_WDT_CNT_VAL)==FU3_WDT_RESET_VALUE &&
               peek(FU3_WDT_CNT_CTRL)==FU3_WDT_RESEN &&
               peek(FU3_WDT_INT_RAW_STAT)==0 && peek(FU3_WDT_INT_MASK_STAT)==0 &&
               peek(FU3_WDT_INT_CLR)==0 && peek(FU3_WDT_DBG_STAT)==0,
               "watchdog-side reset defaults");

        // Independent edge oracle: LOAD=3 expires after four 5 ns edges.
        access(true,FU3_WDT_CNT_LD,3);
        access(true,FU3_WDT_CNT_CTRL,FU3_WDT_RESEN|FU3_WDT_INTEN);
        wait(19,sc_core::SC_NS); settle();
        expect(!irq && !reset_request && peek(FU3_WDT_CNT_VAL)==0,
               "no premature warning before the fourth eligible edge");
        wait(1,sc_core::SC_NS); settle();
        expect(irq && !reset_request && peek(FU3_WDT_CNT_VAL)==3,
               "first timeout reloads exactly on the fourth eligible edge");
        wait(19,sc_core::SC_NS); settle();
        expect(!reset_request,"no premature second-stage reset");
        wait(1,sc_core::SC_NS); settle();
        expect(reset_request && irq,"second timeout requests reset after four more edges");
        cold();
        access(false,FU3_WDT_RESERVED_BASE,0,4,nullptr,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(false,0x1020,0,4,nullptr,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(false,FU3_WDT_CNT_LD+1,0,4,nullptr,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(false,FU3_WDT_CNT_LD,0,2,nullptr,0,tlm::TLM_BURST_ERROR_RESPONSE);
        unsigned char sparse[4]={0xff,0,0xff,0xff};
        access(true,FU3_WDT_CNT_LD,0x12345678,4,sparse,4,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);

        // SYSCSR drives write protection; reads remain available while locked.
        unlock=false;
        access(true,FU3_WDT_CNT_LD,7);
        expect(peek(FU3_WDT_CNT_LD)==FU3_WDT_RESET_VALUE,"locked write is ignored without an error");
        unlock=true; access(true,FU3_WDT_CNT_LD,40); access(true,FU3_WDT_CNT_CTRL,3); unlock=false;
        wait(220,sc_core::SC_NS); settle();
        expect(irq && peek(FU3_WDT_INT_RAW_STAT)==FU3_WDT_TIMEOUT_SEEN &&
               peek(FU3_WDT_INT_MASK_STAT)==FU3_WDT_TIMEOUT_SEEN,
               "first expiry sets raw and enabled interrupt status");
        unlock=true; access(true,FU3_WDT_CNT_LD,40); unlock=false;
        expect(!irq && peek(FU3_WDT_INT_RAW_STAT)==0,"LOAD services watchdog and clears interrupt");

        // DBGEN halts the counter; clearing it resumes from the held value.
        unlock=true; access(true,FU3_WDT_CNT_LD,40); access(true,FU3_WDT_CNT_CTRL,7); unlock=false;
        const auto held=peek(FU3_WDT_CNT_VAL);
        expect(peek(FU3_WDT_DBG_STAT)==1 && !irq,"DBGEN reports and holds debug state");
        wait(100,sc_core::SC_NS); settle();
        expect(peek(FU3_WDT_CNT_VAL)==held && !irq,"counter remains frozen while DBGEN is set");
        unlock=true; access(true,FU3_WDT_CNT_CTRL,3); unlock=false;
        wait(20,sc_core::SC_NS); settle();
        expect(peek(FU3_WDT_CNT_VAL)<held,"counter resumes after DBGEN clears");

        // APB reset clears the programming-side halt request but preserves WDT CSRs.
        unlock=true; access(true,FU3_WDT_CNT_LD,80); access(true,FU3_WDT_CNT_CTRL,7); unlock=false;
        const auto before_program_reset=peek(FU3_WDT_CNT_LD);
        preset=false; settle();
        expect(peek(FU3_WDT_CNT_LD)==before_program_reset && peek(FU3_WDT_DBG_STAT)==0,
               "preset_n resets APB-side halt state without resetting watchdog CSRs");
        preset=true; settle();

        // Watchdog reset restores LOAD/CONTROL as well as the counter and status.
        wdog_reset_n=false; settle();
        expect(!irq && !reset_request && peek(FU3_WDT_CNT_LD)==FU3_WDT_RESET_VALUE &&
               peek(FU3_WDT_CNT_VAL)==FU3_WDT_RESET_VALUE && peek(FU3_WDT_CNT_CTRL)==FU3_WDT_RESEN,
               "wdogres_n resets watchdog-domain configuration and state");
        wdog_reset_n=true; settle();

        // RESEN=0 permits repeated second chances without asserting reset.
        unlock=true; access(true,FU3_WDT_CNT_LD,1); access(true,FU3_WDT_CNT_CTRL,FU3_WDT_INTEN); unlock=false;
        wait(45,sc_core::SC_NS); settle();
        expect(irq && !reset_request && peek(FU3_WDT_INT_RAW_STAT)==FU3_WDT_TIMEOUT_SEEN,
               "RESEN=0 keeps warning asserted and reloads after subsequent timeout");

        // A zero load reports immediately; a reset request is sticky until watchdog reset.
        wdog_reset_n=false; settle(); wdog_reset_n=true; settle();
        unlock=true; access(true,FU3_WDT_CNT_CTRL,3); access(true,FU3_WDT_CNT_LD,0); unlock=false;
        expect(irq && peek(FU3_WDT_INT_RAW_STAT)==FU3_WDT_TIMEOUT_SEEN,
               "zero load raises timeout status immediately");
        wait(5,sc_core::SC_NS); settle();
        expect(reset_request && irq,"second-stage reset request asserts on watchdog edge");
        unlock=true; access(true,FU3_WDT_INT_CLR,0xa5a55a5a); unlock=false;
        expect(reset_request,"software clear cannot retract an asserted reset request");
        wdog_reset_n=false; settle();
        expect(!reset_request && !irq,"watchdog reset clears the sticky reset request");
        finished=true;
        sc_core::sc_stop();
    }
    void watchdog() { wait(2,sc_core::SC_SEC); expect(false,"test watchdog"); sc_core::sc_stop(); }
};
int sc_main(int,char**) {
    Bench bench{"bench"}; sc_core::sc_start();
    std::cout << "WDT V1.1 behavior: " << bench.checks << " checks, " << bench.failures << " failures\n";
    return bench.failures || !bench.finished ? 1 : 0;
}
