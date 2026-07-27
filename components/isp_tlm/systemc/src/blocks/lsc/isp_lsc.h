/*
 * LSC (Lens Shading Correction) Block
 * Placeholder/Passthrough module
 * Matches RTL: Placeholder only
 */

#ifndef ISP_LSC_H
#define ISP_LSC_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"

template<unsigned int BITS = 10>
class isp_lsc : public sc_module {
public:
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_raw{"i_raw"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_raw{"o_raw"};

    isp_lsc(const sc_module_name& name)
        : sc_module(name)
    {
        SC_METHOD(passthrough);
        sensitive << i_href << i_vsync << i_raw;
    }

private:
    void passthrough() {
        o_href.write(i_href.read());
        o_vsync.write(i_vsync.read());
        o_raw.write(i_raw.read());
    }
};

using isp_lsc_10b = isp_lsc<10>;

#endif // ISP_LSC_H
