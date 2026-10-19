// INTMAP: Bật từng nguồn một, chỉ đúng chân Ibex theo HAS v2.0 section 10.2 lên 
// Thêm OR nhiều nguồn và cause
#include <fu3_plic/intmap.h>

#include <iostream>

using namespace fu3_plic;

// Bảng kỳ vọng chép độc lập từ HAS 
constexpr unsigned kExpected[kIntmapSources] = {
    kOutNmi,       // WDT0
    14,            // MAILBOX_IBEX
    13,            // CRM
    12,            // AES
    11,            // EDHOC OSCORE
    10,            // RNG
    9,             // DMA0
    8,             // QSPI
    7,             // UART0
    6,             // SPI0
    5,             // GPIO0_WAKE
    4,             // UART1
    4,             // UART2
    3,             // SPI1
    3,             // SPI2
    3,             // SPI3
    2,             // I2C0
    1,             // GPIOA
    1,             // GPIOB
    1,             // GPIOC
    0,             // TIMER2
    0,             // TIMER3
    kOutExternal,  // ADC
    kOutExternal,  // AP_HBCTRL
    kOutSoftware,  // SYSCSR software IRQ (Ibex)
    kOutTimer,     // TIMER1
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

        expect_only(3, "SPI3 keeps irq_fast_i[3] after SPI1 falls");
        src[kSrcSpi3].write(false); 
        settle();

        // Tất cả nguồn lên (mọi chân lên)
        for (auto& s : src)
            s.write(true);
        settle();
        
        for (unsigned o = 0; o < kIntmapOutputs; ++o) 
            check(out[o].read(), "all high, out " + std::to_string(o));
        
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
