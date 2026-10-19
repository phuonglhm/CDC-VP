#pragma once
// FU3 INTMAP (HAS v2.0 section 10.2): OR tổ hợp IRQ ngoại vi -> chân ngắt Ibex
// Không thanh ghi, không trạng thái
// Nguồn level-high, không đảo
#include <systemc>

namespace fu3_plic {

// Net IRQ ngoại vi (Source IPs ORed)
enum IntmapSource : unsigned {
    kSrcWdt0, 
    kSrcMailboxIbex, 
    kSrcCrm, 
    kSrcAes, 
    kSrcEdhocOscore, 
    kSrcRng, 
    kSrcDma0,

    kSrcQspi, 
    kSrcUart0, 
    kSrcSpi0, 
    kSrcGpio0Wake, 
    kSrcUart1, 
    kSrcUart2,

    kSrcSpi1, 
    kSrcSpi2, 
    kSrcSpi3, 
    kSrcI2c0, 
    kSrcGpioA, 
    kSrcGpioB, 
    kSrcGpioC,
    kSrcTimer2, 
    kSrcTimer3, 
    kSrcAdc, 
    kSrcHbctrl, 
    kSrcSoftwareIbex, 
    kSrcTimer1,
    kIntmapSources
};

// Chân ngắt Ibex: 0..14 = irq_fast_i[k]
enum IntmapOutput : unsigned {
    kOutExternal = 15, kOutSoftware, kOutTimer, kOutNmi, kIntmapOutputs
};

// mcause của từng chân (CPU Cause): fast k -> 16+k, NMI -> 31
constexpr unsigned kIbexCause[kIntmapOutputs] = {
    16, 
    17, 
    18, 
    19, 
    20, 
    21, 
    22, 
    23, 
    24, 
    25, 
    26, 
    27, 
    28, 
    29, 
    30, 
    11, 
    3, 
    7, 
    31
};

class Intmap : public sc_core::sc_module {
public:
    sc_core::sc_vector<sc_core::sc_in<bool>> sources{"sources", kIntmapSources};
    sc_core::sc_vector<sc_core::sc_out<bool>> irq{"irq", kIntmapOutputs};

    SC_HAS_PROCESS(Intmap);
    explicit Intmap(sc_core::sc_module_name name);

private:
    void update();
};

}  // namespace fu3_plic
