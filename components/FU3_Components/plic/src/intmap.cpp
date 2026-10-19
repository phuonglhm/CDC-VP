#include "fu3_plic/intmap.h"

namespace fu3_plic {

Intmap::Intmap(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    SC_METHOD(update);
    for (auto& s : sources) sensitive << s;
}

void Intmap::update() {
    auto in = [this](unsigned s) { return sources[s].read(); };

    // INTMAP logic của HAS v2.0 section 10.2
    irq[14].write(in(kSrcMailboxIbex));
    irq[13].write(in(kSrcCrm));
    irq[12].write(in(kSrcAes));
    irq[11].write(in(kSrcEdhocOscore));
    irq[10].write(in(kSrcRng));
    irq[9].write(in(kSrcDma0));
    irq[8].write(in(kSrcQspi));
    irq[7].write(in(kSrcUart0));
    irq[6].write(in(kSrcSpi0));
    irq[5].write(in(kSrcGpio0Wake));
    irq[4].write(in(kSrcUart1) || in(kSrcUart2));
    irq[3].write(in(kSrcSpi1) || in(kSrcSpi2) || in(kSrcSpi3));
    irq[2].write(in(kSrcI2c0));
    irq[1].write(in(kSrcGpioA) || in(kSrcGpioB) || in(kSrcGpioC));
    irq[0].write(in(kSrcTimer2) || in(kSrcTimer3));
    irq[kOutExternal].write(in(kSrcAdc) || in(kSrcHbctrl));
    irq[kOutSoftware].write(in(kSrcSoftwareIbex));
    irq[kOutTimer].write(in(kSrcTimer1));
    irq[kOutNmi].write(in(kSrcWdt0));
}

}  // namespace fu3_plic
