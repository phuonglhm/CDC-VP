"""Register view for the FX1 ISP Python reference.

Deliberately independent of the C++ model: it replays a *profile* (the list of
CSR writes a driver would issue) onto a minimal register store built from the
generated schema (docs/csr/fx1_isp_csr_schema.json), including the LUT data
ports whose contents the algorithms read. Only software-programmed state is
modelled; hardware status is not.

Profile text format, one write per line (also read by the C++ testbench):
    <REGISTER_NAME> <value>      # value: decimal or 0x hex
Blank lines and '#' comments are ignored.
"""

import json
import os

from .stats import StatsState

GATED = {  # control register -> registers committed by its `updated` bit
    # None since the 07/10 map (DEC-24/26/27 superseded): CCM and CNF are
    # sampled at the accepted SOF (owner Q1); the Resizer publishes a set
    # derived from its key 35 clocks after the key is stable (MAS §6.20.4.3).
    # The reference replays configuration between frames, so the set a frame
    # uses is the one of the current registers.
}


def poison(i, mask):
    """Power-up content of an internal RAM word (MAS §6.5.2: undefined after
    reset). A fixed, deterministic but meaningless pattern, the same as the
    model's (src/control/control_unit.cpp, ram_poison)."""
    return ((i * 0x9E37) ^ 0xA5A5) & mask

# EE profile registers (MAS §6.18.4.1): each of the three resident profiles
# owns these scalars and the four tables; EE_CTRL.profile_wr_sel steers writes.
EE_SCALARS = ("EE_ALPHA", "EE_BETA", "EE_CLAMP", "EE_FEATURE_EN", "EE_RADIAL_CENTER", "EE_RADIAL_R2_TH0",
              "EE_RADIAL_R2_TH1", "EE_RADIAL_R2_TH2", "EE_RADIAL_GAIN_01", "EE_RADIAL_GAIN_23")

STATS_COMMANDS = ("AWB_RESULT_CTRL", "TUNE_AEC_CMD", "TUNE_AF_CMD", "TUNE_AEC_INDEX", "TUNE_AF_INDEX",
                  "TUNE_AEC_DROPS", "TUNE_AF_DROPS")

INPUT_DOMAIN = ("COMMON_FRAME_WIDTH", "COMMON_FRAME_HEIGHT", "COMMON_BAYER", "COMMON_INPUT_FORMAT")

SCHEMA = os.path.join(os.path.dirname(__file__), "..", "..", "docs", "csr", "fx1_isp_csr_schema.json")


class Registers:
    def __init__(self, schema_path=SCHEMA):
        doc = json.load(open(schema_path, encoding="utf-8"))
        self.by_name = {}
        self.value = {}
        for r in doc["registers"]:
            name = r["name"]
            fields = {}
            for f in r["fields"]:
                hi, lo = [int(x) for x in f["bits"].strip("[]").split(":")]
                mask = ((1 << (hi - lo + 1)) - 1) << lo
                for b in f["tied_zero_bits"]:
                    mask &= ~(1 << b)
                fields[f["name"]] = (lo, mask, f["access"])
            rw = sum(m for (_, m, acc) in fields.values() if acc == "RW")
            self.by_name[name] = {"offset": int(r["offset"], 16), "fields": fields, "rw": rw}
            self.value[name] = int(r["reset"], 16)
        self.reset = dict(self.value)
        # LUT contents after i_rst_n: undefined (MAS §6.5.2, CSR-16 superseded),
        # filled with the poison pattern so that a block enabled without its
        # table gives a visibly wrong picture.
        self.gamma_lut = [poison(i, 0xFFF) for i in range(256)]
        self.ee_luts = [[[poison(256 * p + 64 * t + i, 0xFFFF) for i in range(64)] for t in range(4)]
                        for p in range(3)]
        self.ee_scalar = [{n: self.value[n] for n in EE_SCALARS} for _ in range(3)]
        self.ee_frame = {"en": 0, "profile": 0, "scalar": dict(self.ee_scalar[0])}
        self.gtm_lut = [poison(i, 0xFFFF) for i in range(65)]
        # CSR/MAS v1.07 legacy-mode table validity (V107-4): per-entry write
        # flags and software confirmation. Gamma (§6.14.4): all 256 entries,
        # confirmed with EN = 0. EE residents (§9.9.1): 192 LUT entries each,
        # confirmed with EE disabled. GTM manual bank 0 (§6.16.4.3): both byte
        # lanes of all 65 points. Payload contents are not reset.
        self.gamma_written = [False] * 256
        self.gamma_valid = False
        self.ee_written = [[[False] * (64 if t < 2 else 32) for t in range(4)] for _ in range(3)]
        self.ee_valid = [False] * 3
        self.gtm_manual = [poison(i, 0xFFFF) for i in range(65)]
        self.gtm_manual_written = [False] * 65
        # Bank payloads of the profile the reference activates (single bank; V107-4/5).
        self.nr_curve = [0] * 17
        self.bpc_list = [0] * 1024
        self.lsc_sector = [[0] * 31, [0] * 31]
        self.gtm_cap = (0, self.value["GTM_KEY"], self.value["GTM_LWHITE"])  # GTM build controls of the frame
        # Gated sets (DEC-24/27): committed copies, updated only while the
        # block's `updated` bit is set. Profiles are replayed with the pipeline
        # idle, so a commit takes effect at the write (DEC-26).
        self.committed = {n: self.value[n] for grp in GATED.values() for n in grp}
        # LSC resident profiles (HAS §6.8.8): committed at a successful validate.
        self.lsc_mesh = [None, None, None]
        self._lsc_stage = None
        self._lsc_dest = 0
        self.lsc_active = 0
        self.lsc_sel = 0                         # accepted LSC_PROFILE_SEL, adopted at the SOF
        # GTM banks (HAS Table 6-49), 2DNR variance shadow (HAS §6.17.7.2).
        self.gtm_banks = [[poison(65 * b + i, 0xFFFF) for i in range(65)] for b in range(2)]
        self.gtm_read = 0
        self.gtm_valid = False
        self.gtm_pending = False
        self.nr_var = 0
        self.nr_have = False                     # an automatic estimate exists (MAS v1.07 §6.17.3.10)
        # Statistics publication state (HAS §9.5), incl. the AEC commit (#296).
        self.stats = StatsState()

    # -- access ------------------------------------------------------------
    def write(self, name, value):
        reg = self.by_name[name]
        old = self.value[name]
        self.value[name] = (self.value[name] & ~reg["rw"]) | (value & reg["rw"])
        w1c = sum(m for (_, m, acc) in reg["fields"].values() if acc == "W1C")
        self.value[name] &= ~(value & w1c)       # W1C bits the reference holds (TUNE_ERROR)
        if name in EE_SCALARS:
            p = self.f("EE_CTRL", "profile_wr_sel")
            if p < 3:
                self.ee_scalar[p][name] = self.value[name]
                self.ee_valid[p] = False                # a scalar write clears the confirmation
            else:
                self.value[name] = old              # code 3: write ignored
        if name in INPUT_DOMAIN and self.value[name] != old:
            # An input-domain change clears every EE resident's flags and validity (§9.9.1).
            self.ee_written = [[[False] * (64 if t < 2 else 32) for t in range(4)] for _ in range(3)]
            self.ee_valid = [False] * 3
        if name == "GAMMA_CTRL" and value & (1 << 8) and not old & 1:   # EN before the write (V107-A15)
            self.gamma_valid = all(self.gamma_written)  # LUT_LOAD_CONFIRM (W1P, pipeline idle here)
        if name == "EE_LUT_CTRL" and value & (1 << 8) and not self.f("EE_CTRL", "en"):
            p = self.f("EE_CTRL", "profile_wr_sel")     # PROFILE_LOAD_CONFIRM of the write resident
            if p < 3:
                self.ee_valid[p] = all(all(t) for t in self.ee_written[p])
        for ctrl, group in GATED.items():
            if (name == ctrl or name in group) and self.f(ctrl, "updated"):
                for n in group:
                    self.committed[n] = self.value[n]
        if name == "AEC_CTRL" and value & 0x2:
            self.stats.aec_commit = True         # W1S, transferred at the next SOF
        if name in STATS_COMMANDS:
            self.stats.command(self, name, value)   # one-clock commands, index ports, drop W1C
        if name == "LSC_LOAD_CTRL":
            self._lsc_command(value)
        elif name == "LSC_COEF_DATA" and self._lsc_stage is not None:
            self._lsc_stage.append(value & 0x1FFFFF)
        elif name == "LSC_MESH_NODES":
            self.lsc_mesh = [None, None, None]   # geometry change invalidates every profile
        elif name == "LSC_PROFILE_SEL":
            sel = value & 3
            if sel < 3 and self.lsc_mesh[sel] is not None and not (
                    self._lsc_stage is not None and sel == self._lsc_dest):
                self.lsc_sel = sel               # accepted; the SOF re-checks validity
        if name == "GTM_LUT_DATA":
            addr = self.f("GTM_LUT_ADDR", "addr")
            if not self.f("GTM_CTRL", "en") and addr < 65:   # discarded while enabled (MAS p146)
                self.gtm_manual[addr] = self.f("GTM_LUT_DATA", "data")   # manual bank 0 (v1.07)
                self.gtm_manual_written[addr] = True
            self._set_field("GTM_LUT_ADDR", "addr", (addr + 1) & 0x7F)
        if name == "TUNE_NR_LUT_DATA" and self.f("TUNE_NR_LUT_ADDR", "VALUE") < 17:
            self.nr_curve[self.f("TUNE_NR_LUT_ADDR", "VALUE")] = self.f("TUNE_NR_LUT_DATA", "VALUE")
        elif name == "TUNE_BPC_DATA":
            self.bpc_list[self.f("TUNE_BPC_ADDR", "VALUE")] = self.value["TUNE_BPC_DATA"]
        elif name == "TUNE_LSC_SECTOR_DATA" and self.f("TUNE_LSC_SECTOR_ADDR", "INDEX") < 31:
            self.lsc_sector[self.f("TUNE_LSC_SECTOR_ADDR", "AXIS")][self.f("TUNE_LSC_SECTOR_ADDR", "INDEX")] = \
                self.f("TUNE_LSC_SECTOR_DATA", "VALUE")
        if name == "GAMMA_LUT_DATA":
            addr = self.f("GAMMA_LUT_ADDR", "addr")
            self.gamma_lut[addr] = self.f("GAMMA_LUT_DATA", "data")
            self.gamma_written[addr] = True
            self.gamma_valid = False                    # every LUT write clears LUT_LOADED
            self._set_field("GAMMA_LUT_ADDR", "addr", (addr + 1) & 0xFF)
        elif name == "EE_LUT_WDATA":
            bank = self.f("EE_LUT_CTRL", "lut_sel")
            addr = self.f("EE_LUT_ADDR", "lut_addr")
            p = self.f("EE_CTRL", "profile_wr_sel")
            if p < 3:                                # code 3: ignored, the address still advances
                self.ee_luts[p][bank][addr & (63 if bank < 2 else 31)] = self.f("EE_LUT_WDATA", "lut_wdata")
                self.ee_written[p][bank][addr & (63 if bank < 2 else 31)] = True
                self.ee_valid[p] = False
            self._set_field("EE_LUT_ADDR", "lut_addr", (addr + 1) & 0x3F)

    def active_size(self):
        """Pipeline size after the Input Formatter phase crop (MAS §6.6)."""
        p = self.f("COMMON_BAYER", "pattern")
        w, h = self.f("COMMON_FRAME_WIDTH", "h_active"), self.f("COMMON_FRAME_HEIGHT", "v_active")
        return max(w - (p & 1), 0) // 2 * 2, max(h - ((p >> 1) & 1), 0) // 2 * 2

    def lsc_geometry_ok(self):
        """2..32 nodes per axis, not above the active size (MAS v1.07 §6.8.4, V107-A2)."""
        nx, ny = self.f("LSC_MESH_NODES", "mesh_nx"), self.f("LSC_MESH_NODES", "mesh_ny")
        w, h = self.active_size()
        return 2 <= nx <= 32 and 2 <= ny <= 32 and nx <= w and ny <= h

    def _lsc_command(self, value):
        """MAS Table 31: abort > validate > begin. The active profile is never a
        destination (owner Q5); a begin invalidates its destination, which stays
        invalid after an abort or a count mismatch."""
        nx, ny = self.f("LSC_MESH_NODES", "mesh_nx"), self.f("LSC_MESH_NODES", "mesh_ny")
        if value & (1 << 10):                    # abort
            self._lsc_stage = None
        elif value & (1 << 9):                   # validate
            if self._lsc_stage is not None and len(self._lsc_stage) == 4 * nx * ny and self.lsc_geometry_ok():
                self.lsc_mesh[self._lsc_dest] = list(self._lsc_stage)
            self._lsc_stage = None
        elif value & (1 << 8):                   # begin
            dest = value & 3
            if self._lsc_stage is None and dest < 3 and dest != self.lsc_active and self.lsc_geometry_ok():
                self._lsc_dest = dest
                self.lsc_mesh[dest] = None
                self._lsc_stage = []

    def ee_sof(self):
        """EE enable, profile (code 3 -> 0) and that profile's scalars are
        sampled at the accepted SOF (MAS §6.18.4.1); its tables stay live."""
        sel = self.f("EE_CTRL", "profile_sel")
        p = sel if sel < 3 else 0
        # An unconfirmed selected resident passes the frame through (v1.07 §9.9.1).
        self.ee_frame = {"en": self.f("EE_CTRL", "en") and self.ee_valid[p], "profile": p,
                         "scalar": dict(self.ee_scalar[p])}

    def ee_f(self, reg, field):
        """Field of an EE scalar register as the frame uses it."""
        lo, mask, _ = self.by_name[reg]["fields"][field]
        return (self.ee_frame["scalar"][reg] & mask) >> lo

    def lsc_sof(self):
        """The accepted selector becomes active at the SOF only if its profile
        is still valid (MAS Table 31, Figure 29); otherwise the active profile
        is kept (the model reports PROFILE_SEL_REJECT)."""
        sel = self.lsc_sel
        if sel != self.lsc_active and self.lsc_mesh[sel] is not None and not (
                self._lsc_stage is not None and sel == self._lsc_dest):
            self.lsc_active = sel

    def soft_reset(self, scope="common"):
        """Reference state a soft reset changes (MAS Table 183).
        common: the 2DNR variance shadow (ALG-2DNR-09), an LSC load in progress
                and the statistics; configuration, LUTs, LSC profiles, GTM banks,
                committed sets and a pending AEC commit (owner Q4) are kept.
        dma:    statistics, 2DNR and LSC are outside the DMA scope; the aborted
                frame publishes nothing and its AWB bank is freed."""
        if scope == "common":
            self.nr_var = 0
            self.nr_have = False
            self._lsc_stage = None
            self.stats.soft_reset()
        else:
            self.stats.abort()

    def f(self, reg, field):
        lo, mask, _ = self.by_name[reg]["fields"][field]
        return (self.value[reg] & mask) >> lo

    def profile_mode(self):
        return bool(self.f("TUNE_MODE", "ENABLE"))

    def ext(self, reg, field):
        """A TUNE extension control as the datapath uses it (MAS v1.07 §9.9.2):
        its reset value in legacy mode, the programmed value in profile mode.
        The reference has no banks: in profile mode a vector programs the
        bank it activates."""
        lo, mask, _ = self.by_name[reg]["fields"][field]
        v = self.value[reg] if self.f("TUNE_MODE", "ENABLE") else self.reset[reg]
        return (v & mask) >> lo

    def fc(self, reg, field):
        """Field as the frame uses it: the value sampled at the accepted SOF,
        which for configuration replayed between frames is the current value."""
        lo, mask, _ = self.by_name[reg]["fields"][field]
        return (self.committed.get(reg, self.value[reg]) & mask) >> lo

    def _set_field(self, reg, field, v):
        lo, mask, _ = self.by_name[reg]["fields"][field]
        self.value[reg] = (self.value[reg] & ~mask) | ((v << lo) & mask)

    # -- profiles ----------------------------------------------------------
    def apply_profile(self, path):
        for name, value in read_profile(path):
            self.write(name, value)


def read_profile(path):
    writes = []
    for lineno, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            raise ValueError("%s:%d: expected '<REGISTER> <value>'" % (path, lineno))
        writes.append((parts[0], int(parts[1], 0)))
    return writes
