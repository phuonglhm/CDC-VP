#!/usr/bin/env python3
"""Re-apply the extra SAURIA hw versions this project depends on, onto a fresh
SAURIA Python checkout. These versions are NOT part of upstream SAURIA and are
lost whenever sauria is re-cloned, so run this once after cloning:

    SAURIA_PY=~/Desktop/sauria/Python python3 tools/hw_versions_ext.py
    python3 tools/hw_versions_ext.py --sauria /path/to/sauria/Python

Idempotent: a version already present is left untouched. The four blocks below are
byte-for-byte the ones verified against the model (values cross-checked with
hw_versions.get_params, 2026-07-02). See also the config_helper 64-bit-field patch
reminder printed at the end.
"""
import os, sys, argparse

# Each extra version, inserted verbatim before `elif version=="int8_8x16":`.
BLOCKS = {
    "FP16_32x32": '''    elif version=="FP16_32x32":

        HOPTS["MEMA_DEPTH"] =          2048
        HOPTS["MEMB_DEPTH"] =          2048
        HOPTS["MEMC_DEPTH"] =          1024
        HOPTS["DATA_AXI_DATA_WIDTH"] = 128
        HOPTS["DATA_AXI_ADDR_WIDTH"] = 32
        HOPTS["X"] =                   32
        HOPTS["Y"] =                   32
        HOPTS["DILP_W"] =              64
        HOPTS["PARAMS_W"] =            8
        HOPTS["TH_W"] =                2
        HOPTS["IFM_FIFO_POSITIONS"] =  5
        HOPTS["WEI_FIFO_POSITIONS"] =  4
        HOPTS["FIFO_FILL_CYCLES"] =    1
        HOPTS["IA_W"] =                16
        HOPTS["IB_W"] =                16
        HOPTS["OC_W"] =                16
        HOPTS["OP_TYPE"] =             1
        HOPTS["IA_MANT"] =             10
        HOPTS["IB_MANT"] =             10
        HOPTS["IC_MANT"] =             10
        HOPTS["rounding"] =            "RNE"
        HOPTS["approx_comp"] =         False
        HOPTS["mul_type"] =            3
        HOPTS["M"] =                   14
        HOPTS["add_type"] =            4
        HOPTS["A"] =                   16

''',
    "FP16_64x64": '''    elif version=="FP16_64x64":

        HOPTS["MEMA_DEPTH"] =          2048
        HOPTS["MEMB_DEPTH"] =          2048
        HOPTS["MEMC_DEPTH"] =          1024
        HOPTS["DATA_AXI_DATA_WIDTH"] = 128
        HOPTS["DATA_AXI_ADDR_WIDTH"] = 32
        HOPTS["X"] =                   64
        HOPTS["Y"] =                   64
        HOPTS["DILP_W"] =              64
        HOPTS["PARAMS_W"] =            8
        HOPTS["TH_W"] =                2
        HOPTS["IFM_FIFO_POSITIONS"] =  5
        HOPTS["WEI_FIFO_POSITIONS"] =  4
        HOPTS["FIFO_FILL_CYCLES"] =    1
        HOPTS["IA_W"] =                16
        HOPTS["IB_W"] =                16
        HOPTS["OC_W"] =                16
        HOPTS["OP_TYPE"] =             1
        HOPTS["IA_MANT"] =             10
        HOPTS["IB_MANT"] =             10
        HOPTS["IC_MANT"] =             10
        HOPTS["rounding"] =            "RNE"
        HOPTS["approx_comp"] =         False
        HOPTS["mul_type"] =            3
        HOPTS["M"] =                   14
        HOPTS["add_type"] =            4
        HOPTS["A"] =                   16

''',
    "int16_8x16": '''    elif version=="int16_8x16":

        HOPTS["MEMA_DEPTH"] =          2048
        HOPTS["MEMB_DEPTH"] =          2048
        HOPTS["MEMC_DEPTH"] =          1024
        HOPTS["DATA_AXI_DATA_WIDTH"] = 128
        HOPTS["DATA_AXI_ADDR_WIDTH"] = 32
        HOPTS["X"] =                   16
        HOPTS["Y"] =                   8
        HOPTS["DILP_W"] =              64
        HOPTS["PARAMS_W"] =            8
        HOPTS["TH_W"] =                2
        HOPTS["IFM_FIFO_POSITIONS"] =  5
        HOPTS["WEI_FIFO_POSITIONS"] =  4
        HOPTS["FIFO_FILL_CYCLES"] =    1
        HOPTS["IA_W"] =                16
        HOPTS["IB_W"] =                16
        HOPTS["OC_W"] =                64
        HOPTS["OP_TYPE"] =             0
        HOPTS["IA_MANT"] =             0
        HOPTS["IB_MANT"] =             0
        HOPTS["IC_MANT"] =             0
        HOPTS["rounding"] =            "RNE"
        HOPTS["approx_comp"] =         False
        HOPTS["mul_type"] =            0
        HOPTS["M"] =                   0
        HOPTS["add_type"] =            0
        HOPTS["A"] =                   0

''',
    "int16_32x32": '''    elif version=="int16_32x32":

        HOPTS["MEMA_DEPTH"] =          2048
        HOPTS["MEMB_DEPTH"] =          2048
        HOPTS["MEMC_DEPTH"] =          1024
        HOPTS["DATA_AXI_DATA_WIDTH"] = 128
        HOPTS["DATA_AXI_ADDR_WIDTH"] = 32
        HOPTS["X"] =                   32
        HOPTS["Y"] =                   32
        HOPTS["DILP_W"] =              64
        HOPTS["PARAMS_W"] =            8
        HOPTS["TH_W"] =                2
        HOPTS["IFM_FIFO_POSITIONS"] =  5
        HOPTS["WEI_FIFO_POSITIONS"] =  4
        HOPTS["FIFO_FILL_CYCLES"] =    1
        HOPTS["IA_W"] =                16
        HOPTS["IB_W"] =                16
        HOPTS["OC_W"] =                64
        HOPTS["OP_TYPE"] =             0
        HOPTS["IA_MANT"] =             0
        HOPTS["IB_MANT"] =             0
        HOPTS["IC_MANT"] =             0
        HOPTS["rounding"] =            "RNE"
        HOPTS["approx_comp"] =         False
        HOPTS["mul_type"] =            0
        HOPTS["M"] =                   0
        HOPTS["add_type"] =            0
        HOPTS["A"] =                   0

''',
}

ANCHOR = '    elif version=="int8_8x16":'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sauria", default=os.environ.get("SAURIA_PY",
                    os.path.expanduser("~/Desktop/sauria/Python")))
    ap.add_argument("--check", action="store_true",
                    help="report missing versions without editing")
    args = ap.parse_args()

    hv = os.path.join(args.sauria, "src", "hw_versions.py")
    if not os.path.exists(hv):
        sys.exit(f"[hw_versions_ext] not found: {hv} (set SAURIA_PY or --sauria)")

    s = open(hv).read()
    if ANCHOR not in s:
        sys.exit(f"[hw_versions_ext] anchor not found in {hv}; SAURIA layout changed")

    missing = [v for v in BLOCKS if f'version=="{v}"' not in s]
    present = [v for v in BLOCKS if v not in missing]
    for v in present:
        print(f"[hw_versions_ext] present  : {v}")
    if args.check:
        for v in missing:
            print(f"[hw_versions_ext] MISSING  : {v}")
        sys.exit(1 if missing else 0)

    for v in missing:
        s = s.replace(ANCHOR, BLOCKS[v] + ANCHOR, 1)
        print(f"[hw_versions_ext] inserted : {v}")
    if missing:
        open(hv, "w").write(s)

    print("[hw_versions_ext] done.")
    print("[hw_versions_ext] REMINDER: also ensure config_helper.py packs 64-bit")
    print("    fields (o_rows_active/o_cols_active at 64x64) across >2 registers, and")
    print("    that any np.int -> int fix is applied. See memory config-helper-64bit-field-fix.")


if __name__ == "__main__":
    main()
