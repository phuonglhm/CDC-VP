/*
 * LDCI (Local Dynamic Contrast Improvement) Block
 * Placeholder/Passthrough module
 * Matches RTL: Not implemented - passthrough
 */

#ifndef ISP_LDCI_H
#define ISP_LDCI_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"

class isp_ldci : public sc_module {
public:
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};

    sc_in<uint8_t> i_data_y{"i_data_y"};
    sc_in<uint8_t> i_data_u{"i_data_u"};
    sc_in<uint8_t> i_data_v{"i_data_v"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};

    sc_out<uint8_t> o_data_y{"o_data_y"};
    sc_out<uint8_t> o_data_u{"o_data_u"};
    sc_out<uint8_t> o_data_v{"o_data_v"};

    isp_ldci(const sc_module_name& name)
        : sc_module(name)
    {
        SC_METHOD(passthrough);
        sensitive << i_href << i_vsync << i_data_y << i_data_u << i_data_v;
    }

private:
    void passthrough() {
        o_href.write(i_href.read());
        o_vsync.write(i_vsync.read());
        o_data_y.write(i_data_y.read());
        o_data_u.write(i_data_u.read());
        o_data_v.write(i_data_v.read());
    }
};

#endif // ISP_LDCI_H
