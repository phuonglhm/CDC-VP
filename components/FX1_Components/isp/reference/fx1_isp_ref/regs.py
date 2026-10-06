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
    "CCM_CTRL": ["CCM_" + n for n in ("CRR", "CRG", "CRB", "CGR", "CGG", "CGB", "CBR", "CBG", "CBB",
                                      "OFS_R", "OFS_G", "OFS_B")],
    "CNF_CTRL": ["CNF_CHROMA_TH", "CNF_LUMA_TH"],
    "RESIZER_CTRL": ["RESIZER_CTRL"],   # only the scale field is used from the copy
}

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
        # LUT contents after i_rst_n (CSR-16): Gamma 0, EE 0x8000, GTM 0.
        self.gamma_lut = [0] * 256
        self.ee_tables = [[0x8000] * 64 for _ in range(4)]
        self.gtm_lut = [0] * 65
        # Gated sets (DEC-24/27): committed copies, updated only while the
        # block's `updated` bit is set. Profiles are replayed with the pipeline
        # idle, so a commit takes effect at the write (DEC-26).
        self.committed = {n: self.value[n] for grp in GATED.values() for n in grp}
        # LSC resident profiles (HAS §6.8.8): committed at a successful validate.
        self.lsc_mesh = [None, None, None]
        self._lsc_stage = None
        self._lsc_dest = 0
        self.lsc_active = 0
        # GTM banks (HAS Table 6-49), 2DNR variance shadow (HAS §6.17.7.2).
        self.gtm_banks = [[0] * 65, [0] * 65]
        self.gtm_read = 0
        self.gtm_valid = False
        self.gtm_pending = False
        self.nr_var = 0
        # Statistics publication state (HAS §9.5), incl. the AEC commit (#296).
        self.stats = StatsState()

    # -- access ------------------------------------------------------------
    def write(self, name, value):
        reg = self.by_name[name]
        self.value[name] = (self.value[name] & ~reg["rw"]) | (value & reg["rw"])
        for ctrl, group in GATED.items():
            if (name == ctrl or name in group) and self.f(ctrl, "updated"):
                for n in group:
                    self.committed[n] = self.value[n]
        if name == "AEC_CTRL" and value & 0x2:
            self.stats.aec_commit = True         # W1S, transferred at the next SOF
        if name == "LSC_LOAD_CTRL":
            self._lsc_command(value)
        elif name == "LSC_COEF_DATA" and self._lsc_stage is not None:
            self._lsc_stage.append(value & 0x1FFFFF)
        elif name == "LSC_MESH_NODES":
            self.lsc_mesh = [None, None, None]   # geometry change invalidates every profile
        elif name == "LSC_PROFILE_SEL":
            sel = value & 3
            if sel < 3 and self.lsc_mesh[sel] is not None:
                self.lsc_active = sel            # replayed idle: adopted at the next SOF
        if name == "GTM_LUT_DATA":
            addr = self.f("GTM_LUT_ADDR", "addr")
            if (not self.f("GTM_CTRL", "en") or self.f("GTM_CTRL", "manual")) and addr < 65:
                self.gtm_banks[self.gtm_read][addr] = self.f("GTM_LUT_DATA", "data")
                self.gtm_valid = True
            self._set_field("GTM_LUT_ADDR", "addr", (addr + 1) & 0x7F)
        if name == "GAMMA_LUT_DATA":
            addr = self.f("GAMMA_LUT_ADDR", "addr")
            self.gamma_lut[addr] = self.f("GAMMA_LUT_DATA", "data")
            self._set_field("GAMMA_LUT_ADDR", "addr", (addr + 1) & 0xFF)
        elif name == "EE_LUT_WDATA":
            bank = self.f("EE_LUT_CTRL", "lut_sel")
            addr = self.f("EE_LUT_ADDR", "lut_addr")
            self.ee_tables[bank][addr & (63 if bank < 2 else 31)] = self.f("EE_LUT_WDATA", "lut_wdata")
            self._set_field("EE_LUT_ADDR", "lut_addr", (addr + 1) & 0x3F)

    def _lsc_command(self, value):
        nx, ny = self.f("LSC_MESH_NODES", "mesh_nx"), self.f("LSC_MESH_NODES", "mesh_ny")
        if value & (1 << 10):                    # abort
            self._lsc_stage = None
        elif value & (1 << 9):                   # validate
            if self._lsc_stage is not None and len(self._lsc_stage) == 4 * nx * ny:
                self.lsc_mesh[self._lsc_dest] = list(self._lsc_stage)
            self._lsc_stage = None
        elif value & (1 << 8):                   # begin
            self._lsc_dest = value & 3
            self._lsc_stage = []

    def soft_reset(self):
        """Reference state a soft reset changes (DEC-14): the 2DNR variance
        shadow (ALG-2DNR-09) and the statistics; configuration, LUTs, LSC
        profiles, GTM banks and committed sets are kept."""
        self.nr_var = 0
        self._lsc_stage = None
        self.stats.soft_reset()

    def f(self, reg, field):
        lo, mask, _ = self.by_name[reg]["fields"][field]
        return (self.value[reg] & mask) >> lo

    def fc(self, reg, field):
        """Field of the committed copy of a gated register (DEC-24)."""
        lo, mask, _ = self.by_name[reg]["fields"][field]
        return (self.committed[reg] & mask) >> lo

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
