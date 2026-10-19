//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_pll_io
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_PLL_IO_H
#define VL_MIPI_4_RX_PLL_IO_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "top_cells.h"

class mipi_4_rx_pll_io : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb{"atb"};
    sc_core::sc_in<sc_dt::sc_logic> rext{"rext"};
    sc_core::sc_in<sc_dt::sc_logic> datan0{"datan0"};
    sc_core::sc_in<sc_dt::sc_logic> datan1{"datan1"};
    sc_core::sc_in<sc_dt::sc_logic> datan2{"datan2"};
    sc_core::sc_in<sc_dt::sc_logic> datan3{"datan3"};
    sc_core::sc_in<sc_dt::sc_logic> clkn{"clkn"};
    sc_core::sc_in<sc_dt::sc_logic> datap0{"datap0"};
    sc_core::sc_in<sc_dt::sc_logic> datap1{"datap1"};
    sc_core::sc_in<sc_dt::sc_logic> datap2{"datap2"};
    sc_core::sc_in<sc_dt::sc_logic> datap3{"datap3"};
    sc_core::sc_in<sc_dt::sc_logic> clkp{"clkp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_pll_io);
    explicit mipi_4_rx_pll_io(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    std::unique_ptr<mipi_4_rx_pll_io_DPHY3RAFE4L_PLL_bump> IOPADS;
};

#endif
