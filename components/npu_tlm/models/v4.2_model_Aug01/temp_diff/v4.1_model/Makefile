# ======================================================================
# SAURIA NPU — unified SystemC model (v1 core, runtime PROFILE)
# Convenience + flexible-build Makefile.        Run:  make help
# ======================================================================
# Quick start
#   make help                     # this help
#   make smoke                    # profile-routing smoke test
#   make mvm                      # run MVM 8x16 demo        (PASS)
#   make gemm32                   # run GeMM 32x32 demo      (PASS)
#   make gemm64                   # run GeMM 64x64 demo      (PASS)
#   make demo CASE=<name>         # run any captured demo case
#   make list                     # list demo cases
#   make shape SHAPE="..." EVAL_X=32 EVAL_Y=32 VER=int8_32x32
#   make eval                     # build+run tb_evaluate on stimuli/
#   make all                      # build smoke + evaluate + demo
#   make clean
#
# Flexible knobs (override on the command line):
#   GEO=8x16|32x32|64x64          preset EVAL_X/EVAL_Y + IDX widths (+RB for 64x64)
#   EVAL_X= EVAL_Y=               SA geometry (default 16x8)
#   IDX_FLAGS="-D..."             packed-config index widths (if not using GEO)
#   RB=                           SRAM region bytes A/B/C (default 16384)
#   DEBUG=1                       module trace prints (SAURIA_DEBUG)
#   OPT=-O0 -g / -O3              optimization
#   EXTRA="-DSAURIA_..."          extra defines (e.g. profile addr)
#   SYSTEMC_HOME=/path            source install (else system-wide -lsystemc)
# ======================================================================

# ---- SystemC location ----
# Resolved in this order:
#   1. Company EDA server (hostname eda-server-01): Accellera build under /opt/arm.
#   2. SYSTEMC_HOME given (source install with include/ + a lib dir). Override the lib
#      dir with SC_LIBDIR= if it is neither lib-linux64 nor lib64.
#   3. System-wide package: headers on the default path, link -lsystemc (local dev VM).
HOST := $(shell hostname 2>/dev/null)
ifeq ($(HOST),eda-server-01)
  SYSTEMC_HOME ?= /opt/arm/fastmodels/SystemC/Accellera/SystemC
  SC_LIBDIR    ?= $(SYSTEMC_HOME)/dynlib/Linux64_GCC-10.3
endif

SYSTEMC_HOME ?=
SC_LIBDIR    ?=
SC_INC :=
SC_LIB :=
ifneq ($(SYSTEMC_HOME),)
  ifneq ($(wildcard $(SYSTEMC_HOME)/include/systemc.h),)
    SC_INC := -I$(SYSTEMC_HOME)/include
    ifneq ($(SC_LIBDIR),)
      SC_LIB := -L$(SC_LIBDIR) -Wl,-rpath,$(SC_LIBDIR)
    else ifneq ($(wildcard $(SYSTEMC_HOME)/lib-linux64),)
      SC_LIB := -L$(SYSTEMC_HOME)/lib-linux64 -Wl,-rpath,$(SYSTEMC_HOME)/lib-linux64
    else ifneq ($(wildcard $(SYSTEMC_HOME)/lib64),)
      SC_LIB := -L$(SYSTEMC_HOME)/lib64 -Wl,-rpath,$(SYSTEMC_HOME)/lib64
    endif
  endif
endif
SC_LIBS := -lsystemc -lm -pthread

# ---- Toolchain & flexible knobs ----
CXX       ?= g++
STD       ?= -std=c++17
OPT       ?= -O3
WARN      ?= -Wall
EVAL_X    ?= 16
EVAL_Y    ?= 8
RB        ?= 16384
IDX_FLAGS ?=
EXTRA     ?=

# ---- Geometry presets:  make <target> GEO=8x16|32x32|64x64 ----
IDX_8x16  := -DSAURIA_ACT_IDX_W=15 -DSAURIA_WEI_IDX_W=16 -DSAURIA_OUT_IDX_W=14
IDX_32x32 := -DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17 -DSAURIA_OUT_IDX_W=16
IDX_64x64 := -DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17
ifeq ($(GEO),8x16)
  EVAL_X    := 16
  EVAL_Y    := 8
  IDX_FLAGS := $(IDX_8x16)
endif
ifeq ($(GEO),32x32)
  EVAL_X    := 32
  EVAL_Y    := 32
  IDX_FLAGS := $(IDX_32x32)
endif
ifeq ($(GEO),64x64)
  EVAL_X    := 64
  EVAL_Y    := 64
  IDX_FLAGS := $(IDX_64x64)
  RB        := 65536
endif

ifeq ($(DEBUG),1)
  DBG := -DSAURIA_DEBUG=1
endif

GEOFLAGS := -DEVAL_X=$(EVAL_X) -DEVAL_Y=$(EVAL_Y)
REGFLAGS := -DA_REGION_BYTES=$(RB) -DB_REGION_BYTES=$(RB) -DC_REGION_BYTES=$(RB)
CXXFLAGS := $(STD) $(OPT) $(WARN) -I. $(SC_INC) $(GEOFLAGS) $(REGFLAGS) $(IDX_FLAGS) $(DBG) $(EXTRA)
LINK     := $(SC_LIB) $(SC_LIBS)

DEMO_SCRIPTS := npu_demo_clean/scripts
GEN_TOOL     := npu_demo_clean/tools/create_tb_demo_from_tb_evaluate.py

.DEFAULT_GOAL := help
.PHONY: help all clean smoke eval demo list mvm gemm32 gemm64 shape check cfgtest enctest memtest goldtest stimtest selftest tb_utilization_optimization run_utilization_optimization

# ---- Direct binary builds (honour GEO / EVAL_* / RB / DEBUG) ----
tb_unified_smoke: tb_unified_smoke.cpp
	@echo "[BUILD] $@  EVAL=$(EVAL_X)x$(EVAL_Y) DEBUG=$(DEBUG)"
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

tb_evaluate: tb_evaluate.cpp
	@echo "[BUILD] $@  EVAL=$(EVAL_X)x$(EVAL_Y) RB=$(RB) DEBUG=$(DEBUG)"
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

# tb_demo.cpp is AUTO-GENERATED from tb_evaluate.cpp (path->env transform).
tb_demo.cpp: tb_evaluate.cpp $(GEN_TOOL)
	python3 $(GEN_TOOL)

tb_demo: tb_demo.cpp
	@echo "[BUILD] $@  EVAL=$(EVAL_X)x$(EVAL_Y) RB=$(RB)"
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

tb_utilization_optimization: tb_utilization_optimization.cpp
	@echo "[BUILD] $@  EVAL=32x32"
	$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 $< $(LINK) -o $@

all: tb_unified_smoke tb_evaluate tb_demo tb_utilization_optimization
	@echo "[OK] built: smoke + evaluate + demo + utilization"

# ---- Convenience run targets ----
smoke: tb_unified_smoke
	@echo "== smoke =="; ./tb_unified_smoke

eval: tb_evaluate
	@echo "== tb_evaluate (reads stimuli/) =="; ./tb_evaluate

run_utilization_optimization: tb_utilization_optimization
	@echo "== run tb_utilization_optimization =="; ./tb_utilization_optimization

# Case-driven demo: geometry + IDX widths + region bytes come from case.env,
# so no flags are needed here — just the case name.
demo:
	@test -n "$(CASE)" || { echo "Usage: make demo CASE=<name>   (see: make list)"; exit 1; }
	@bash $(DEMO_SCRIPTS)/run_tb_demo_case.sh $(CASE) build

list:
	@echo "Demo cases (npu_demo_clean/cases):"
	@ls -1 npu_demo_clean/cases 2>/dev/null | sed 's/^/  /' || echo "  (none)"

# Smoke-CI: run EVERY captured demo case + the cfg-layout round-trip, print a
# pass/fail summary and fail the build if any case is not PASS. Use this right
# after deploying to the server to confirm the whole data flow is intact.
check: cfgtest goldtest
	@fail=0; \
	for c in $$(ls -1 npu_demo_clean/cases 2>/dev/null); do \
	  out=$$(bash $(DEMO_SCRIPTS)/run_tb_demo_case.sh $$c build 2>&1 | grep -aE 'status:|RESULT'); \
	  if echo "$$out" | grep -q 'PASS'; then echo "  [PASS] $$c"; \
	  else echo "  [FAIL] $$c"; fail=1; fi; \
	done; \
	if [ $$fail -eq 0 ]; then echo "[check] ALL demo cases PASS"; \
	else echo "[check] SOME cases FAILED"; exit 1; fi

# Config-layout encode<->decode round-trip self-test (no SystemC needed).
cfgtest:
	@$(CXX) $(STD) -I. tools/test_cfg_layout.cpp -o /tmp/sauria_cfgtest && /tmp/sauria_cfgtest

# C reference-conv golden vs each captured case's gold_dram — NO Python needed.
# Proves the golden can be regenerated on the target without SAURIA/torch.
goldtest:
	@$(CXX) $(STD) -I. -Idriver tools/test_golden.cpp -o /tmp/sauria_goldtest
	@fail=0; for c in $$(ls -1 npu_demo_clean/cases 2>/dev/null); do \
	  /tmp/sauria_goldtest npu_demo_clean/cases/$$c || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo "[goldtest] C golden matches every case (no Python)"; \
	else echo "[goldtest] GOLDEN MISMATCH"; exit 1; fi

# C GoldenStimuli emitter vs each captured case's shipped GoldenStimuli.txt — NO Python.
stimtest:
	@$(CXX) $(STD) -I. -Idriver tools/test_stim.cpp -o /tmp/sauria_stimtest
	@fail=0; for c in $$(ls -1 npu_demo_clean/cases 2>/dev/null); do \
	  /tmp/sauria_stimtest npu_demo_clean/cases/$$c || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo "[stimtest] C GoldenStimuli matches every case (no Python)"; \
	else echo "[stimtest] STIM MISMATCH"; exit 1; fi

# End-to-end proof: GENERATE brand-new cases entirely in C (inputs + config +
# GoldenStimuli + golden), then run them through tb_demo. Fully Python-free.
SELFTEST_CASES := \
  "int8_8x16|1 1 1 1 128 8 1 16 16 8 0" \
  "int8_8x16|1 1 1 1 64 8 2 16 16 8 0" \
  "FP16_8x16|1 1 1 1 96 8 1 16 16 8 0" \
  "int16_8x16|1 1 1 1 64 8 1 16 16 8 0"
selftest:
	@$(CXX) $(STD) -I. -Idriver tools/gen_case.cpp -o /tmp/sauria_gencase
	@fail=0; i=0; for spec in $(SELFTEST_CASES); do \
	  ver=$${spec%|*}; sh=$${spec##*|}; name=selftest_$$i; i=$$((i+1)); \
	  rm -rf npu_demo_clean/cases/$$name; \
	  /tmp/sauria_gencase $$ver "$$sh" npu_demo_clean/cases/$$name 7 >/dev/null; \
	  out=$$(bash $(DEMO_SCRIPTS)/run_tb_demo_case.sh $$name build 2>&1 | grep -aE 'status:'); \
	  if echo "$$out" | grep -q PASS; then echo "  [PASS] $$ver  $$sh  (C-generated)"; \
	  else echo "  [FAIL] $$ver  $$sh"; fail=1; fi; \
	  rm -rf npu_demo_clean/cases/$$name; \
	done; \
	if [ $$fail -eq 0 ]; then echo "[selftest] C-generated cases PASS on tb_demo — fully Python-free"; \
	else echo "[selftest] SELFTEST FAILED"; exit 1; fi

# libsauria_cfg C encoder bit-exact vs SAURIA Python (needs SAURIA_PY; offline/CI).
# Runs the representative demo shapes through dump_cfg.py -> test_encoder.
ENC_SHAPES := \
  "1 1 1 1 64 8 1 16 16 8 0|int8_8x16" \
  "1 1 1 1 64 32 1 32 32 32 0|int8_32x32" \
  "1 1 1 1 256 64 1 64 64 64 0|int8_64x64" \
  "3 3 1 2 16 8 4 32 32 8 1|int8_32x32" \
  "3 3 1 1 16 8 4 64 32 8 1|int8_32x32" \
  "1 1 1 1 64 32 1 32 32 32 0|FP16_32x32" \
  "1 1 1 1 256 64 1 64 64 64 0|FP16_64x64" \
  "1 1 1 1 64 8 1 16 16 8 0|int16_8x16" \
  "1 1 1 1 64 32 1 32 32 32 0|int16_32x32"
enctest:
	@$(CXX) $(STD) -I. -Idriver tools/test_encoder.cpp -o /tmp/sauria_enctest
	@fail=0; for spec in $(ENC_SHAPES); do \
	  sh=$${spec%|*}; ver=$${spec##*|}; \
	  python3 tools/dump_cfg.py "$$sh" $$ver 2>/dev/null | /tmp/sauria_enctest || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo "[enctest] encoder BIT-EXACT vs Python on all shapes"; \
	else echo "[enctest] ENCODER MISMATCH"; exit 1; fi

# libsauria_mem tensor<->DRAM packer bit-exact vs SAURIA Python (needs SAURIA_PY).
memtest:
	@$(CXX) $(STD) -I. -Idriver tools/test_mem.cpp -o /tmp/sauria_memtest
	@fail=0; for spec in $(ENC_SHAPES); do \
	  sh=$${spec%|*}; ver=$${spec##*|}; \
	  python3 tools/dump_mem.py "$$sh" $$ver 2>/dev/null | /tmp/sauria_memtest || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo "[memtest] DRAM packer BIT-EXACT vs Python on all shapes"; \
	else echo "[memtest] PACKER MISMATCH"; exit 1; fi

# Shortcuts for the verified demos.
mvm:
	@$(MAKE) --no-print-directory demo CASE=demo_mvm_8x16
gemm32:
	@$(MAKE) --no-print-directory demo CASE=demo_gemm_32x32
gemm64:
	@$(MAKE) --no-print-directory demo CASE=demo_gemm_64x64

# Generate + build + run a fresh SAURIA shape (no capture). Uses EVAL_X/EVAL_Y/RB.
# Prefer the in-tree run_shape.sh (portable); fall back to $(HOME) for the dev VM.
# SAURIA_PY (path to the patched SAURIA Python) is passed through if set.
SHAPE ?=
VER   ?= int8_8x16
RUN_SHAPE := $(if $(wildcard ./run_shape.sh),./run_shape.sh,$(HOME)/run_shape.sh)
shape:
	@test -n "$(SHAPE)" || { echo 'Usage: make shape SHAPE="Bw Bh d s Cin Cw Ch Cout Xused Yused preload" EVAL_X=32 EVAL_Y=32 VER=int8_32x32 [RB=65536]'; exit 1; }
	bash $(RUN_SHAPE) "$(SHAPE)" $(EVAL_X) $(EVAL_Y) $(VER) $(RB)

clean:
	rm -f tb_unified_smoke tb_evaluate tb_demo tb_demo_* *.o
	@echo "[clean] done"

help:
	@echo "SAURIA NPU unified model — make targets"
	@echo "  make smoke                 profile-routing smoke test"
	@echo "  make mvm | gemm32 | gemm64 run a verified demo (PASS)"
	@echo "  make demo CASE=<name>      run any captured demo case"
	@echo "  make check                 run ALL demo cases + cfg round-trip (smoke-CI)"
	@echo "  make cfgtest               cfg-layout encode<->decode round-trip test"
	@echo "  make list                  list demo cases"
	@echo "  make eval                  build+run tb_evaluate on stimuli/"
	@echo "  make shape SHAPE=\"...\"      gen+build+run a fresh shape"
	@echo "  make all | clean"
	@echo ""
	@echo "Flexible build knobs (override on cmdline):"
	@echo "  GEO=8x16|32x32|64x64   EVAL_X= EVAL_Y=   IDX_FLAGS=\"-D...\""
	@echo "  RB=   DEBUG=1   OPT=-O0 -g   EXTRA=\"-D...\"   SYSTEMC_HOME=/path"
	@echo ""
	@echo "SystemC: $(if $(SC_INC),source @ $(SYSTEMC_HOME),system-wide (-lsystemc))"
