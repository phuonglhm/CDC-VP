#pragma once
// FU3 INTMAP: peripheral IRQ levels -> Ibex interrupt inputs, per the Common
// Spec v2.1 Interrupt Mapping "Ibex CPU interrupt mapping - proposed FU3 source
// allocation" (decision Q03-b: v2.1 route table applied in the VP).
// Each source has a route enable that resets disabled. The enable register
// layout/offset is not defined yet (Q03-b, pending), so the enables are a host
// parameter (set_enable) for integration tests only; firmware cannot set them.
// Level-high inputs, no inversion; a group output is the OR of its enabled sources.
#include <cstdint>
#include <systemc>

namespace fu3_plic {

// Source nets (src[n] in the v2.1 table is irq_sources_i[n]; the INTMAP takes
// the same peripheral levels the PLIC does, plus Ibex-only WDT0 and MAILBOX_IBEX).
enum IntmapSource : unsigned {
    kSrcWdt0Warning,
    kSrcCrm,
    kSrcGpio0Wake,
    kSrcEdhocOscore,
    kSrcAes,
    kSrcRng,
    kSrcDma0,
    kSrcQspi,
    kSrcUart0, kSrcUart1, kSrcUart2, kSrcUart3, kSrcUart4, kSrcUart5, kSrcUart6, kSrcUart7,
    kSrcSpi0, kSrcSpi1, kSrcSpi2, kSrcSpi3,
    kSrcCan,
    kSrcGpioA, kSrcGpioB, kSrcGpioC,
    kSrcI2c0,
    kSrcAdc,
    kSrcPwm0, kSrcPwm1,
    kSrcTimer2, kSrcTimer3,
    kSrcWdt1,
    kSrcMailboxIbex,   // optional external-group input (decision Q03-g)
    kSrcSoftwareIbex,  // SYSCSR per-Ibex software-pending bit (Q03-e pending: tied low)
    kSrcTimer1,
    kIntmapSources
};

// Ibex inputs: 0..14 = irq_fast_i[k]
enum IntmapOutput : unsigned {
    kOutExternal = 15, kOutSoftware, kOutTimer, kOutNmi, kIntmapOutputs
};

// mcause of each input: fast k -> 16 + k, NMI -> 31
constexpr unsigned kIbexCause[kIntmapOutputs] = {
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 11, 3, 7, 31
};

// Route of each source (Common v2.1 table). irq_nm_i has no source (tied low).
constexpr unsigned kIntmapRoute[kIntmapSources] = {
    0,                                  // WDT0 warning -> fast[0]
    1,                                  // CRM -> fast[1]
    2,                                  // GPIO0_WAKE -> fast[2]
    3,                                  // EDHOC OSCORE -> fast[3]
    4,                                  // AES-256 -> fast[4]
    5,                                  // RNG -> fast[5]
    6,                                  // DMA0 -> fast[6]
    7,                                  // QSPI -> fast[7]
    8, 8, 8, 8, 8, 8, 8, 8,             // UART0-7 -> fast[8]
    9, 9, 9, 9,                         // SPI0-3 -> fast[9]
    10,                                 // CAN -> fast[10]
    11, 11, 11,                         // GPIOA/B/C -> fast[11]
    12,                                 // I2C -> fast[12]
    13,                                 // ADC -> fast[13]
    14, 14,                             // PWM0/1 -> fast[14]
    kOutExternal, kOutExternal,         // TIMER2/3 -> irq_external_i
    kOutExternal,                       // WDT1 -> irq_external_i
    kOutExternal,                       // MAILBOX_IBEX -> irq_external_i
    kOutSoftware,                       // SYSCSR software IRQ -> irq_software_i
    kOutTimer,                          // TIMER1 -> irq_timer_i
};

class Intmap : public sc_core::sc_module {
public:
    sc_core::sc_vector<sc_core::sc_in<bool>> sources{"sources", kIntmapSources};
    sc_core::sc_vector<sc_core::sc_out<bool>> irq{"irq", kIntmapOutputs};

    SC_HAS_PROCESS(Intmap);
    explicit Intmap(sc_core::sc_module_name name);

    // Route enables (reset: all disabled). Host parameter only, see above.
    void set_enable(unsigned source, bool on);
    void enable_all(bool on);
    bool enabled(unsigned source) const { return (enables_ >> source) & 1; }

private:
    std::uint64_t enables_ = 0;
    sc_core::sc_event changed_;
    void update();
};

}  // namespace fu3_plic
