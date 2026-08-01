# Sauria NPU v4.2 SystemC Emulator Makefile

SYSTEMC_HOME ?= /usr/local/systemc
CXX           ?= g++
CXXFLAGS      := -std=c++17 -O3 -Wall -I. -I$(SYSTEMC_HOME)/include
LINK          := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib -lsystemc -lm -pthread

# Preprocessor defines from evaluated environment
EVAL_X        ?= 64
EVAL_Y        ?= 64
RB            ?= 16384

CXXFLAGS      += -DEVAL_X=$(EVAL_X) -DEVAL_Y=$(EVAL_Y)
CXXFLAGS      += -DA_REGION_BYTES=$(RB) -DB_REGION_BYTES=$(RB) -DC_REGION_BYTES=$(RB)

SRCS          := $(wildcard *.cpp)
OBJS          := $(SRCS:.cpp=.o)

.PHONY: all clean check demo

all: tb_unified_smoke

tb_unified_smoke: tb_unified_smoke.cpp
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

tb_evaluate: tb_evaluate.cpp
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

tb_demo: tb_demo.cpp
	@echo "[BUILD] $@  EVAL=$(EVAL_X)x$(EVAL_Y) RB=$(RB)"
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

tb_utilization_optimization: tb_utilization_optimization.cpp
	@echo "[BUILD] $@  EVAL=32x32"
	$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 $< $(LINK) -o $@

test_rich_isa: tools/test_rich_isa.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_lane_b_isolation: tools/test_lane_b_isolation.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_dual_lane_fsm: tools/test_dual_lane_fsm.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_dual_instruction_queues: tools/test_dual_instruction_queues.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_nsplit_barrier: tools/test_nsplit_barrier.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_nsplit_64_idle_b: tools/test_nsplit_64_idle_b.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_cycle_by_cycle: tools/test_cycle_by_cycle.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 $< $(LINK) -o $@

test_vit_encoder_int8: tools/test_vit_encoder_int8.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 -DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17 $< $(LINK) -o $@

# Run single demo case (Usage: make demo CASE=demo_gemm_32x32)
CASE ?= demo_gemm_32x32
demo:
	@echo "[DEMO] Building & Running $(CASE)..."
	@case "$(CASE)" in \
		"conv5x5_demo") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/conv5x5_demo\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_fp16_gemm_32x32") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_fp16_gemm_32x32\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_fp16_gemm_64x64") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_fp16_gemm_64x64\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_fp16_mvm_8x16") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=16 -DEVAL_Y=8 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384 -DCASE_DIR=\"cases/demo_fp16_mvm_8x16\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_gemm_32x32") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_gemm_32x32\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_gemm_64x64") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_gemm_64x64\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_int16_gemm_32x32") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_int16_gemm_32x32\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_int16_mvm_8x16") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=16 -DEVAL_Y=8 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384 -DCASE_DIR=\"cases/demo_int16_mvm_8x16\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_multitile_32x32") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_multitile_32x32\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_mvm_8x16") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=16 -DEVAL_Y=8 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384 -DCASE_DIR=\"cases/demo_mvm_8x16\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		"demo_strided_32x32") \
			$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/demo_strided_32x32\" tb_demo.cpp $(LINK) -o tb_demo && ./tb_demo ;; \
		*) \
			echo "Unknown case: $(CASE)"; exit 1 ;; \
	esac

# Run all 11 demo cases sequentially and print a summary table
check:
	@echo "=========================================================================="
	@echo "           Sauria NPU v4.2 Verification Regression Suite                  "
	@echo "=========================================================================="
	@PASSED=0; FAILED=0; \
	CASES="conv5x5_demo demo_fp16_gemm_32x32 demo_fp16_gemm_64x64 demo_fp16_mvm_8x16 demo_gemm_32x32 demo_gemm_64x64 demo_int16_gemm_32x32 demo_int16_mvm_8x16 demo_multitile_32x32 demo_mvm_8x16 demo_strided_32x32"; \
	for c in $$CASES; do \
		echo -n "  Running $$c ... "; \
		case "$$c" in \
			"conv5x5_demo") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_fp16_gemm_32x32") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_fp16_gemm_64x64") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_fp16_mvm_8x16") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=16 -DEVAL_Y=8 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_gemm_32x32") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_gemm_64x64") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=64 -DEVAL_Y=64 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_int16_gemm_32x32") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_int16_mvm_8x16") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=16 -DEVAL_Y=8 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_multitile_32x32") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_mvm_8x16") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=16 -DEVAL_Y=8 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
			"demo_strided_32x32") \
				$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=65536 -DB_REGION_BYTES=65536 -DC_REGION_BYTES=65536 -DCASE_DIR=\"cases/$$c\" tb_demo.cpp $(LINK) -o tb_demo > /dev/null 2>&1 ;; \
		esac; \
		if ./tb_demo > /dev/null 2>&1; then \
			echo "\033[32mPASS\033[0m"; \
			PASSED=$$((PASSED + 1)); \
		else \
			echo "\033[31mFAIL\033[0m"; \
			FAILED=$$((FAILED + 1)); \
		fi; \
	done; \
	echo "--------------------------------------------------------------------------"; \
	echo "  Results: $$PASSED / 11 PASSED, $$FAILED FAILED"; \
	echo "=========================================================================="; \
	if [ $$FAILED -ne 0 ]; then exit 1; fi

clean:
	rm -f $(OBJS) tb_unified_smoke tb_evaluate tb_demo tb_utilization_optimization test_rich_isa test_lane_b_isolation test_dual_lane_fsm test_dual_instruction_queues test_nsplit_barrier test_nsplit_64_idle_b test_cycle_by_cycle test_vit_encoder_int8
