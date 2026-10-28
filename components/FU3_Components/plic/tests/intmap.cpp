// INTMAP: each source alone raises exactly its Ibex input per the Common v2.1
// Ibex interrupt mapping, only while its route enable is set (reset: disabled,
// decision Q03-b); OR of several sources; causes.
#include <fu3_plic/intmap.h>

#include <iostream>

using namespace fu3_plic;

// Expected input per source, transcribed independently from the Common v2.1
// sheet "Interrupt Mapping" (Ibex proposal), not from kIntmapRoute.
constexpr int kExpected[kIntmapSources] = {
    0,             // WDT0 warning: irq_fast_i[0]
    1,             // CRM: fast[1]
    2,             // GPIO0_WAKE: fast[2]
    3,             // EDHOC OSCORE: fast[3]
    4,             // AES-256: fast[4]
    5,             // RNG: fast[5]
    6,             // DMA0: fast[6]
    7,             // QSPI: fast[7]
    8, 8, 8, 8, 8, 8, 8, 8,   // UART0-7: fast[8]
    9, 9, 9, 9,    // SPI0-3: fast[9]
    10,            // CAN: fast[10]
    11, 11, 11,    // GPIOA/B/C: fast[11]
    12,            // I2C: fast[12]
    13,            // ADC: fast[13]
    14, 14,        // PWM0/PWM1: fast[14]
    kOutExternal, kOutExternal,   // TIMER2/3: irq_external_i
    kOutExternal,  // WDT1: irq_external_i
    kOutExternal,  // MAILBOX_IBEX (optional): irq_external_i
    kOutSoftware,  // SYSCSR software: irq_software_i
    kOutTimer,     // TIMER1: irq_timer_i
};

struct Bench : sc_core::sc_module {
    Intmap dut{"dut"};

    sc_core::sc_vector<sc_core::sc_signal<bool>> src{"src", kIntmapSources};
    sc_core::sc_vector<sc_core::sc_signal<bool>> out{"out", kIntmapOutputs};
    unsigned checks = 0, errors = 0;

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n) : sc_module(n) {
        for (unsigned s = 0; s < kIntmapSources; ++s) 
            dut.sources[s](src[s]);

        for (unsigned o = 0; o < kIntmapOutputs; ++o) 
            dut.irq[o](out[o]);
        SC_THREAD(run);
    }

    void check(bool ok, const std::string& what) {
        ++checks;
        if (!ok) { 
            ++errors; 
            std::cerr << "FAIL: " << what << '\n'; 
        }
    }

    void settle() { sc_core::wait(sc_core::SC_ZERO_TIME); sc_core::wait(sc_core::SC_ZERO_TIME); }

    void expect_only(int high, const std::string& what) {
        for (unsigned o = 0; o < kIntmapOutputs; ++o)
            check(out[o].read() == (int(o) == high), what + ", out " + std::to_string(o));
    }

    void run() {
        settle();
        expect_only(-1, "all sources low");

        // Route enables reset disabled: no source reaches Ibex.
        for (auto& s : src)
            s.write(true);
        settle();
        expect_only(-1, "all sources high, all routes disabled");
        for (auto& s : src)
            s.write(false);
        dut.enable_all(true);
        settle();

        for (unsigned s = 0; s < kIntmapSources; ++s) {
            src[s].write(true); 
            settle();
            
            expect_only(kExpected[s], "source " + std::to_string(s));
            src[s].write(false); 
            settle();
            
            expect_only(-1, "source " + std::to_string(s) + " released");
        }

        // OR: một nguồn rơi, nguồn kia vẫn giữ chân lên
        src[kSrcSpi1].write(true); 
        src[kSrcSpi3].write(true); 
        settle();

        src[kSrcSpi1].write(false); 
        settle();

        expect_only(9, "SPI3 keeps irq_fast_i[9] after SPI1 falls");
        // Disabling one route masks that source only.
        dut.set_enable(kSrcSpi3, false);
        settle();
        expect_only(-1, "SPI3 route disabled");
        src[kSrcSpi1].write(true);
        settle();
        expect_only(9, "SPI1 still routed");
        src[kSrcSpi1].write(false);
        src[kSrcSpi3].write(false);
        dut.set_enable(kSrcSpi3, true);
        settle();

        // Tất cả nguồn lên (mọi chân lên)
        for (auto& s : src)
            s.write(true);
        settle();
        
        for (unsigned o = 0; o < kIntmapOutputs; ++o)
            check(out[o].read() == (o != kOutNmi), "all high, out " + std::to_string(o) + " (NMI tied low)");
        
            for (auto& s : src) 
            s.write(false);
        settle();

        // Cause theo CPU Cause
        check(kIbexCause[kOutNmi] == 31 && kIbexCause[14] == 30 && kIbexCause[0] == 16, "Fast-IRQ/NMI causes");
        check(kIbexCause[kOutExternal] == 11 && kIbexCause[kOutSoftware] == 3 && kIbexCause[kOutTimer] == 7, "Standard causes");
        sc_core::sc_stop();
    }
};

int sc_main(int, char**) {
    Bench bench("Bench");
    sc_core::sc_start();
    std::cout << "INTMAP: " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
