//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_core_log
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_core_log.h"

mipi_4_rx_dphy_ns_DPHYLPTX_core_log::mipi_4_rx_dphy_ns_DPHYLPTX_core_log(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    sctg_I270.reset(new mipi_4_rx_dphy_ns_sctg_nand2x("sctg_I270"));
    sctg_I270->a0(din);
    sctg_I270->a1(onb);
    sctg_I270->z(net1486);
    sctg_I270->gd(gd);
    sctg_I270->vph(vph);
    sctg_I271.reset(new mipi_4_rx_dphy_ns_sctg_nand2x("sctg_I271"));
    sctg_I271->a0(enulp);
    sctg_I271->a1(din);
    sctg_I271->z(enpullupz);
    sctg_I271->gd(gd);
    sctg_I271->vph(vph);
    sctg_I272.reset(new mipi_4_rx_dphy_ns_sctg_nand2x("sctg_I272"));
    sctg_I272->a0(dinz);
    sctg_I272->a1(onb);
    sctg_I272->z(net1496);
    sctg_I272->gd(gd);
    sctg_I272->vph(vph);
    sctg_I267.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I267"));
    sctg_I267->a(net1486);
    sctg_I267->z(net1481);
    sctg_I267->gd(gd);
    sctg_I267->vph(vph);
    sctg_I273.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_I273"));
    sctg_I273->a(net1496);
    sctg_I273->z(net1503);
    sctg_I273->gd(gd);
    sctg_I273->vph(vph);
    sctg_I268.reset(new mipi_4_rx_dphy_ns_sctg_inv3x("sctg_I268"));
    sctg_I268->a(net1481);
    sctg_I268->z(net1485);
    sctg_I268->gd(gd);
    sctg_I268->vph(vph);
    sctg_I275.reset(new mipi_4_rx_dphy_ns_sctg_inv3x("sctg_I275"));
    sctg_I275->a(net1503);
    sctg_I275->z(net1502);
    sctg_I275->gd(gd);
    sctg_I275->vph(vph);
    sctg_I269.reset(new mipi_4_rx_dphy_ns_sctg_inv9x("sctg_I269"));
    sctg_I269->a(net1485);
    sctg_I269->z(enn);
    sctg_I269->gd(gd);
    sctg_I269->vph(vph);
    sctg_I274.reset(new mipi_4_rx_dphy_ns_sctg_inv9x("sctg_I274"));
    sctg_I274->a(net1502);
    sctg_I274->z(enpz);
    sctg_I274->gd(gd);
    sctg_I274->vph(vph);
    sctg_I276.reset(new mipi_4_rx_dphy_ns_sctg_inv9x("sctg_I276"));
    sctg_I276->a(enn);
    sctg_I276->z(ennz);
    sctg_I276->gd(gd);
    sctg_I276->vph(vph);
    sctg_I277.reset(new mipi_4_rx_dphy_ns_sctg_inv9x("sctg_I277"));
    sctg_I277->a(enpz);
    sctg_I277->z(enp);
    sctg_I277->gd(gd);
    sctg_I277->vph(vph);
}
