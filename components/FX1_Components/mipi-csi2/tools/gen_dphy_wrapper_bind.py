#!/usr/bin/env python3
"""Generate phy/src/dphy_rx_wrapper_bind.inc from src/top/mipirxphy.h.

Every top-level port of mipirxphy is classified below; an unclassified port
or a classified name missing from the header is an error, so a change of the
generated PHY header cannot silently leave a port with a guessed tie-off.
Evidence for each tie-off value: docs/PHY_ADAPTER_CONTRACT.md.

Usage: gen_dphy_wrapper_bind.py <component-root>
"""
import re
import sys
from pathlib import Path

# name -> (kind, value)
#   ctl:<member>      public control signal of the wrapper (one writer outside)
#   ppi:<member>      public observation channel
#   tie:<0|1|Z>       constant on a private signal, no writer (sc_in ports)
#   drv:<0|1>         constant via vl::tie on a private resolved net (inout supplies)
#   pad:<member>      public vl::sres lane pad
#   float             private resolved net, never driven (analog inout)
#   sink              private resolved net, output not used
CLASS = {}


def add(kind, *names):
    for n in names:
        if n in CLASS:
            raise SystemExit(f"duplicate classification {n}")
        CLASS[n] = kind


# Lane pads.
add("pad:clkp", "MIPIRX_CLKP")
add("pad:clkn", "MIPIRX_CLKN")
for i in range(4):
    add(f"pad:dp[{i}]", f"MIPIRX_DAT{i}P")
    add(f"pad:dn[{i}]", f"MIPIRX_DAT{i}N")
    # Bumps of the PLL IO ring: evaluator with an empty body (top_cells.h).
    add("tie:Z", f"MIPIRX_DATAP{i}", f"MIPIRX_DATAN{i}")
# Supplies.
add("drv:1", "MIPIRX_VDD")
add("drv:0", "MIPIRX_VSS", "MIPIRX_AGND")
add("tie:1", "MIPIRX_VP", "MIPIRX_VPH", "MIPIRX_VPL")
# Analog test / reference pins: not part of any functional loop.
add("tie:Z", "MIPIRX_IO_ATB", "MIPIRX_PLL_ATB_SENSE", "MIPIRX_PLL_IO_ATB", "MIPIRX_PLL_VBG", "MIPIRX_REXT")
add("float", "MIPIRX_ATB", "MIPIRX_RX_ATB_LINE", "MIPIRX_RX_HSDDR_CLK_M", "MIPIRX_RX_HSDDR_CLK_P",
    "MIPIRX_RX_OSC_CLK_M", "MIPIRX_RX_OSC_CLK_P")
add("sink", "MIPIRX_VREG_MPLL")
# Clocks, resets, configuration driven from outside.
add("ctl:cfg_clk", "mipi_pclk")
add("ctl:refclk", "mipi_clk")
add("ctl:rstz", "dphy_reset_n")
add("ctl:shutdownz", "sysc2mipirxphy_shutdownz")
add("ctl:testclr", "mipirx2phy_testclr")
add("ctl:testclk", "mipirx2phy_testclk")
add("ctl:testen", "mipirx2phy_testen")
add("ctl:testdin", "mipirx2phy_testdin")
add("ctl:enableclk", "sysc2mipirxphy_enableclk")
add("ctl:enable", "mipirxphy_enable")
add("ctl:cfgclkfreqrange", "sysc2mipirxphy_cfgclkfreqrange")
add("ctl:hsfreqrange", "sysc2mipirxphy_hsfreqrange")
add("ctl:basedir", "sysc2mipirxphy_basedir")
for i in range(4):
    add(f"ctl:forcerxmode[{i}]", f"sysc2mipirxphy_forcerxmode_{i}")
# Only reaches the PLL scan reset; PLL unused in RX mode.
add("tie:1", "mipi_reset_n")
add("tie:0", "mipirxphy_bisten", "sysc2mipirxphy_cont_en", "sysc2mipirxphy_forcetxstopmode_0",
    "sysc2mipirxphy_turndisable_0", "sysc2mipirxphy_turnrequest_0")
# PLL shadow-register inputs (PLL powered down in RX mode).
add("tie:0", "sysc2mipirxphy_clkouten_left", "sysc2mipirxphy_clkouten_right", "sysc2mipirxphy_clksel",
    "sysc2mipirxphy_force_lock", "sysc2mipirxphy_gmp_cntrl", "sysc2mipirxphy_int_cntrl",
    "sysc2mipirxphy_pll_m", "sysc2mipirxphy_pll_n", "sysc2mipirxphy_pll_shadow_control",
    "sysc2mipirxphy_pll_testlock", "sysc2mipirxphy_prop_cntrl", "sysc2mipirxphy_shadow_clear",
    "sysc2mipirxphy_updatepll", "sysc2mipirxphy_vco_cntrl")
# PPI observation.
add("ppi:rxbyteclkhs", "rxbyteclkhs")
add("ppi:rxclkactivehs", "rxclkactivehs")
add("ppi:stopstateclk", "stopstateclk")
add("ppi:rxulpsclknot", "rxulpsclknot")
add("ppi:rxskewcalhs", "rxskewcalhs")
add("ppi:direction", "mipirxphy2sysc_direction")
add("ppi:testdout", "phy2mipirx_testdout")
for i in range(4):
    add(f"ppi:stopstatedata[{i}]", f"stopstatedata_{i}")
    add(f"ppi:rxdatahs[{i}]", f"rxdatahs_{i}")
    add(f"ppi:rxvalidhs[{i}]", f"rxvalidhs_{i}")
    add(f"ppi:rxactivehs[{i}]", f"mipirxphy2sysc_rxactivehs_{i}")
    add(f"ppi:rxsynchs[{i}]", f"mipirxphy2sysc_rxsynchs_{i}")
    add(f"ppi:errsoths[{i}]", f"mipirxphy_errsoths_{i}")
    add(f"ppi:errsotsynchs[{i}]", f"mipirxphy_errsotsynchs_{i}")
    add(f"ppi:errcontrol[{i}]", f"mipirxphy2sysc_errcontrol_{i}")
    add(f"ppi:erresc[{i}]", f"mipirxphy_erresc_{i}")
    add(f"ppi:errsyncesc[{i}]", f"mipirxphy2sysc_errsyncesc_{i}")
    add(f"ppi:rxulpsesc[{i}]", f"rxulpsesc_{i}")
    # Escape-mode receive: declared, not consumed (MAS PPI table).
    add("sink", f"rxclkesc_{i}", f"rxdataesc_{i}", f"mipirxphy2sysc_rxvalidesc_{i}", f"mipirxphy2sysc_rxlpdtesc_{i}",
        f"mipirxphy2sysc_rxtriggeresc_{i}")
add("sink", "mipirxphy2sysc_errcontentionlp0_0", "mipirxphy2sysc_errcontentionlp1_0", "pll_refclk",
    "mipirxphy2sysc_lock_phy", "mipirxphy2sysc_testlock", "mipirxphy_bistdone", "mipirxphy_bistok", "scanout")


def classify_rest(name, direction):
    """Scan, observation and continuity ports, classified by naming rule."""
    if name in ("scan_mode",):
        return "tie:0"
    if name == "scan_set_rst_n":
        return "tie:1"  # only selected by scan_mode muxes
    if direction == "in" and (name.startswith("scan") or name == "scanclk"):
        return "tie:0"
    if direction == "out" and (name.startswith("scan") or name.endswith("_obs")
                               or name.startswith("MIPIRXPHY_CONT_DAT")):
        return "sink"
    return None


def main():
    root = Path(sys.argv[1])
    header = (root / "src/top/mipirxphy.h").read_text().split("SC_HAS_PROCESS")[0]
    ports = re.findall(r"sc_core::sc_(inout|in|out)<(sc_dt::sc_logic|sc_dt::sc_lv<(\d+)>)>\s+(\w+)\{", header)
    names = {p[3] for p in ports}
    missing = sorted(set(CLASS) - names)
    if missing:
        raise SystemExit(f"classified ports absent from header: {missing}")
    decl, bind = [], []
    for direction, _, width, name in ports:
        kind = CLASS.get(name) or classify_rest(name, direction)
        if kind is None:
            raise SystemExit(f"unclassified port {direction} {name}")
        tag, _, arg = kind.partition(":")
        w = int(width) if width else 1
        if tag in ("ctl", "ppi", "pad"):
            bind.append(f"    phy->{name}({arg});")
            continue
        if tag == "tie":
            if w == 1:
                init = {"0": "sc_dt::SC_LOGIC_0", "1": "sc_dt::SC_LOGIC_1", "Z": "sc_dt::SC_LOGIC_Z"}[arg]
                decl.append(f"    sc_core::sc_signal<sc_dt::sc_logic> {name}{{\"{name}\", {init}}};")
            else:
                assert arg in "01"
                decl.append(f"    sc_core::sc_signal<sc_dt::sc_lv<{w}>> {name}{{\"{name}\", sc_dt::sc_lv<{w}>({arg})}};")
        elif tag == "drv":
            decl.append(f"    sc_core::sc_signal_resolved {name}{{\"{name}\"}};")
            decl.append(f"    vl::tie {name}_drv{{\"{name}_drv\", sc_dt::SC_LOGIC_{arg}}};")
            bind.append(f"    t.{name}_drv.o(t.{name});")
        elif tag in ("float", "sink"):
            if w == 1:
                decl.append(f"    sc_core::sc_signal_resolved {name}{{\"{name}\"}};")
            else:
                decl.append(f"    sc_core::sc_signal_rv<{w}> {name}{{\"{name}\"}};")
        bind.append(f"    phy->{name}(t.{name});")
    out = ["// GENERATED by tools/gen_dphy_wrapper_bind.py from src/top/mipirxphy.h. Do not edit.",
           f"// {len(ports)} ports.", "#ifdef FX1_DPHY_WRAPPER_DECL"] + decl + ["#endif", "#ifdef FX1_DPHY_WRAPPER_BIND"] + bind + ["#endif", ""]
    dst = root / "phy/src/dphy_rx_wrapper_bind.inc"
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("\n".join(out))
    print(f"{dst}: {len(ports)} ports, {len(decl)} private declarations")


if __name__ == "__main__":
    main()
