# Sauria NPU v4.2 SystemC Emulator Makefile

SYSTEMC_HOME ?= /usr/local/systemc
CXX           ?= g++
CXXFLAGS      := -std=c++17 -O3 -Wall -I. -I$(SYSTEMC_HOME)/include
LINK          := -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib -lsystemc -lm -pthread

# Preprocessor defines from evaluated environment
EVAL_X        ?= 32
EVAL_Y        ?= 32
RB            ?= 16384

CXXFLAGS      += -DEVAL_X=$(EVAL_X) -DEVAL_Y=$(EVAL_Y)
CXXFLAGS      += -DA_REGION_BYTES=$(RB) -DB_REGION_BYTES=$(RB) -DC_REGION_BYTES=$(RB)

SRCS          := $(wildcard *.cpp)
OBJS          := $(SRCS:.cpp=.o)

.PHONY: all clean

all: tb_unified_smoke

tb_unified_smoke: tb_unified_smoke.cpp
	$(CXX) $(CXXFLAGS) $< $(LINK) -o $@

tb_evaluate: tb_evaluate.cpp
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

test_vit: tools/test_vit.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 $< $(LINK) -o $@

test_yolo: tools/test_yolo.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 $< $(LINK) -o $@

test_vit_encoder_int8: tools/test_vit_encoder_int8.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 -DSAURIA_ACT_IDX_W=18 -DSAURIA_WEI_IDX_W=18 -DSAURIA_OUT_IDX_W=17 $< $(LINK) -o $@

test_onnx_model: tools/test_onnx_model.cpp
	@echo "[BUILD] $@"
	$(CXX) $(CXXFLAGS) -DEVAL_X=32 -DEVAL_Y=32 $< $(LINK) -o $@

clean:
	rm -f $(OBJS) tb_unified_smoke tb_evaluate tb_utilization_optimization test_rich_isa test_lane_b_isolation test_dual_lane_fsm test_dual_instruction_queues test_nsplit_barrier test_nsplit_64_idle_b test_cycle_by_cycle test_vit_encoder_int8
