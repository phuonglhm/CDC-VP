#include <fu3_wdt/wdt.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>

struct Bench : sc_core::sc_module {
    fu3_wdt::Watchdog dut{"dut", sc_core::sc_time(5, sc_core::SC_NS)};
    fu3_wdt::Watchdog slow{"slow",sc_core::sc_time(1,sc_core::SC_US),10,sc_core::sc_time(2,sc_core::SC_NS)};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    tlm_utils::simple_initiator_socket<Bench> slow_cpu{"slow_cpu"};
    sc_core::sc_signal<bool> slow_irq,slow_reset,pause;
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
        dut.wdogpause(pause);
        slow_cpu.bind(slow.socket);slow.preset_n(preset);slow.wdogres_n(wdog_reset_n);
        slow.irq(slow_irq);slow.reset_request(slow_reset);
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
        expect(tx.get_response_status()==expected,"response matches WDT V1.2 access rule");
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
        expect(peek(FU3_WDT_LOCK)==0,"V1.2 resets unlocked despite contradictory software example");
        access(true,FU3_WDT_CNT_LD,3);access(true,FU3_WDT_CNT_CTRL,3);
        wait(19,sc_core::SC_NS);settle();expect(!irq&&!reset_request&&peek(FU3_WDT_CNT_VAL)==0,"no warning before N+1 edges");
        wait(1,sc_core::SC_NS);settle();expect(irq&&!reset_request,"first timeout warns");
        wait(20,sc_core::SC_NS);settle();expect(reset_request&&!irq&&peek(FU3_WDT_INT_RAW_STAT)==0,"second timeout clears raw/IRQ and requests reset");
        cold();
        access(true,FU3_WDT_LOCK,0);expect(peek(FU3_WDT_LOCK)==1,"wrong key locks");
        access(true,FU3_WDT_CNT_LD,123);expect(peek(FU3_WDT_CNT_LD)==FU3_WDT_RESET_VALUE,"locked configuration ignored");
        access(true,FU3_WDT_LOCK,FU3_WDT_UNLOCK_KEY);access(true,FU3_WDT_CNT_LD,0xaabbccdd);
        unsigned char lanes[4]={255,0,255,0},none[4]={};
        access(true,FU3_WDT_CNT_LD,0x12345678,4,lanes,4);
        expect(peek(FU3_WDT_CNT_LD)==0xaa34cc78,"Common Full strobes preserve disabled LOAD lanes");
        access(true,FU3_WDT_CNT_LD,0,4,none,4);expect(peek(FU3_WDT_CNT_LD)==0xaa34cc78,"zero strobes do not service watchdog");
        access(true,FU3_WDT_LOCK,FU3_WDT_UNLOCK_KEY,4,lanes,4);expect(peek(FU3_WDT_LOCK)==1,"partial key cannot unlock");
        access(true,FU3_WDT_LOCK,FU3_WDT_UNLOCK_KEY);access(true,FU3_WDT_CNT_LD,100);access(true,FU3_WDT_CNT_CTRL,7);
        const auto held=peek(FU3_WDT_CNT_VAL);wait(100,sc_core::SC_NS);settle();
        expect(peek(FU3_WDT_CNT_VAL)==held&&peek(FU3_WDT_DBG_STAT)==1&&pause,"DBGEN pause holds count and publishes wdogpause");
        access(true,FU3_WDT_LOCK,0);wdog_reset_n=false;settle();
        expect(peek(FU3_WDT_CNT_LD)==100&&peek(FU3_WDT_CNT_CTRL)==7&&peek(FU3_WDT_LOCK)==1,"watchdog reset preserves programming and lock");
        expect(peek(FU3_WDT_CNT_VAL)==FU3_WDT_RESET_VALUE&&!irq&&!reset_request,"VALUE holds its documented reset value while watchdog reset is asserted");
        wdog_reset_n=true;settle();expect(peek(FU3_WDT_CNT_VAL)==100,"reset release starts a fresh retained LOAD interval");preset=false;settle();
        expect(peek(FU3_WDT_CNT_LD)==FU3_WDT_RESET_VALUE&&peek(FU3_WDT_CNT_CTRL)==1&&peek(FU3_WDT_LOCK)==0,"programming reset owns configuration defaults");
        preset=true;settle();
        access(true,FU3_WDT_CNT_LD,1);access(true,FU3_WDT_CNT_CTRL,2);
        wait(10,sc_core::SC_NS);settle();expect(irq&&!reset_request,"RESEN=0 still warns on first timeout");
        wait(10,sc_core::SC_NS);settle();expect(!irq&&!reset_request,"RESEN=0 second timeout clears warning without reset");
        cold();access(true,FU3_WDT_CNT_CTRL,3);access(true,FU3_WDT_CNT_LD,0);
        expect(irq,"zero LOAD reports immediately");wait(5,sc_core::SC_NS);settle();expect(reset_request&&!irq,"zero LOAD reaches second stage");
        access(true,FU3_WDT_INT_CLR,1);expect(reset_request,"software cannot retract sticky recovery request");
        cold();access(false,0x1020,0,4,nullptr,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(false,FU3_WDT_CNT_LD+1,0,4,nullptr,0,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(false,FU3_WDT_CNT_LD,0,2,nullptr,0,tlm::TLM_BURST_ERROR_RESPONSE);
        unsigned char invalid=1;access(true,FU3_WDT_CNT_LD,0,4,&invalid,1,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        unsigned char bytes[4]={100,0,0,0};tlm::tlm_generic_payload t;
        t.set_address(FU3_WDT_CNT_LD);t.set_command(tlm::TLM_WRITE_COMMAND);t.set_data_ptr(bytes);
        t.set_data_length(4);t.set_streaming_width(4);auto delay=sc_core::SC_ZERO_TIME;
        const auto before=sc_core::sc_time_stamp();slow_cpu->b_transport(t,delay);
        expect(t.is_response_ok()&&sc_core::sc_time_stamp()-before==sc_core::sc_time(20,sc_core::SC_NS),"APB latency uses its own period with a slow watchdog clock");
        finished=true;sc_core::sc_stop();
    }
    void watchdog() { wait(2,sc_core::SC_SEC); expect(false,"test watchdog"); sc_core::sc_stop(); }
};
int sc_main(int,char**) {
    Bench bench{"bench"}; sc_core::sc_start();
    std::cout << "WDT V1.2 behavior: " << bench.checks << " checks, " << bench.failures << " failures\n";
    return bench.failures || !bench.finished ? 1 : 0;
}
