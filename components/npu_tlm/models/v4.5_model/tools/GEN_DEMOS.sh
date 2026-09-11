#!/usr/bin/env bash
# ======================================================================
# Reference: how to RUN, GENERATE, and RE-CAPTURE the demo cases.
# Run from the model root (unified/).  3 sections by Python dependency.
# ======================================================================

# ----------------------------------------------------------------------
# A. RUN existing demos  —  NO Python  (this is all the server needs)
# ----------------------------------------------------------------------
#   make check                       # build + run ALL 11 demos + cfgtest/goldtest/stimtest
#   make demo CASE=demo_gemm_32x32   # run one case
#   make list                        # list case names
# The 11 cases ship in npu_demo_clean/cases/ — no need to regenerate them.

# ----------------------------------------------------------------------
# B. GENERATE a new case in C  —  NO Python  (works on the server)
# ----------------------------------------------------------------------
#   g++ -std=c++17 -I. -Idriver tools/gen_case.cpp -o /tmp/sauria_gencase
#   /tmp/sauria_gencase int8_32x32 "1 1 1 1 64 32 1 32 32 32 0" npu_demo_clean/cases/my_case 7
#   make demo CASE=my_case
#   make selftest                    # generate + run a few sample cases in one shot
# Constraint: Cw == Yused (no W-splitting); Cw%Yused==0; Xused<=X; Yused<=Y.

# ----------------------------------------------------------------------
# C. RE-CAPTURE the exact 11 demos via SAURIA  —  NEEDS Python
#    (only where the patched SAURIA Python exists; not required — the 11
#     cases already ship. Use to reproduce bit-identical golden data.)
#    Run:  SAURIA_PY=/path/to/sauria/Python bash tools/GEN_DEMOS.sh capture
# ----------------------------------------------------------------------
capture_all() {
  : "${SAURIA_PY:?set SAURIA_PY to the patched SAURIA Python dir}"
  local S=npu_demo_clean/scripts/capture_case.sh
  local FP="-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float"
  local I16="-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t"
  # idx <version> -> "-DSAURIA_ACT_IDX_W=.. -DSAURIA_WEI_IDX_W=.. -DSAURIA_OUT_IDX_W=.. [dtype]"
  idx() { case "$1" in
    int8_8x16)   echo "-DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=16 -DSAURIA_OUT_IDX_W=14" ;;
    int8_32x32)  echo "-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16" ;;
    int8_64x64)  echo "-DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17" ;;
    FP16_8x16)   echo "-DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=15 -DSAURIA_OUT_IDX_W=15 $FP" ;;
    FP16_32x32)  echo "-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16 $FP" ;;
    FP16_64x64)  echo "-DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17 $FP" ;;
    int16_8x16)  echo "-DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=16 -DSAURIA_OUT_IDX_W=14 $I16" ;;
    int16_32x32) echo "-DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16 $I16" ;;
  esac; }
  cap() { # name shape ex ey ver title desc region
    bash "$S" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$(idx "$5")" "$8"
  }
  # name                   shape                          EX EY version     title            desc      region
  cap demo_mvm_8x16        "1 1 1 1 64 8 1 16 16 8 1"     16 8  int8_8x16   "MVM 8x16"       "1x1 MVM" 65536
  cap demo_gemm_32x32      "1 1 1 1 64 32 1 32 32 32 1"   32 32 int8_32x32  "GeMM 32x32"     "1x1 GeMM" 65536
  cap demo_gemm_64x64      "1 1 1 1 256 64 1 64 64 64 1"  64 64 int8_64x64  "GeMM 64x64"     "1x1 GeMM" 65536
  cap demo_strided_32x32   "3 3 1 2 16 8 4 32 32 8 1"     32 32 int8_32x32  "Strided"        "3x3 s=2" 65536
  cap demo_multitile_32x32 "3 3 1 1 16 8 4 64 32 8 1"     32 32 int8_32x32  "Multi-tile"     "Cout=64" 65536
  cap conv5x5_demo         "5 5 1 1 16 8 4 32 32 8 1"     32 32 int8_32x32  "Conv 5x5"       "5x5"     65536
  cap demo_fp16_mvm_8x16   "1 1 1 1 64 8 1 16 16 8 0"     16 8  FP16_8x16   "FP16 MVM 8x16"  "half"    16384
  cap demo_fp16_gemm_32x32 "1 1 1 1 64 32 1 32 32 32 0"   32 32 FP16_32x32  "FP16 GeMM 32x32" "half"   65536
  cap demo_fp16_gemm_64x64 "1 1 1 1 256 64 1 64 64 64 0"  64 64 FP16_64x64  "FP16 GeMM 64x64" "half"   131072
  cap demo_int16_mvm_8x16  "1 1 1 1 64 8 1 16 16 8 0"     16 8  int16_8x16  "INT16 MVM 8x16"  "int16"  32768
  cap demo_int16_gemm_32x32 "1 1 1 1 64 32 1 32 32 32 0"  32 32 int16_32x32 "INT16 GeMM 32x32" "int16" 131072
}

[ "$1" = "capture" ] && capture_all
echo "See sections A/B/C above. Run 'bash tools/GEN_DEMOS.sh capture' (needs SAURIA_PY) to re-capture all 11."
