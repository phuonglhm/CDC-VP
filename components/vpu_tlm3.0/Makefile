CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude -Ioptional_systemc
COMPONENT_NAME := vpu_tlm3.0
ENCODER_BIN := build/$(COMPONENT_NAME)
SYSTEMC_BIN := build/$(COMPONENT_NAME)_systemc
SYSTEMC_NATIVE_BIN := build/$(COMPONENT_NAME)_systemc_native

SYSTEMC_CPPFLAGS ?= -DSC_DISABLE_API_VERSION_CHECK -DSC_ALLOW_DEPRECATED_IEEE_API
SYSTEMC_LDFLAGS ?=
SYSTEMC_LDLIBS ?= -lsystemc
ifneq ($(strip $(SYSTEMC_HOME)),)
SYSTEMC_LIBDIR ?= $(SYSTEMC_HOME)/lib-linux64
SYSTEMC_CPPFLAGS += -I$(SYSTEMC_HOME)/include
SYSTEMC_LDFLAGS += -L$(SYSTEMC_LIBDIR) -Wl,-rpath,$(SYSTEMC_LIBDIR)
endif

CORE_SRC := src/bit_writer.cpp src/cabac.cpp src/yuv420.cpp src/transform.cpp src/hevc_pcm_encoder.cpp
FIFO_SRC := src/fifo_cycle_model.cpp
MODEL_SRC := src/tlm_pipeline.cpp src/vpu_mmio.cpp $(FIFO_SRC)

.PHONY: all clean test verify verify-mmio verify-intra verify-mmio-intra verify-m1 \
	verify-tq verify-mmio-tq verify-m2 verify-full verify-mmio-full verify-m21 \
	verify-tu16 verify-mmio-tu16 verify-adaptive verify-mmio-adaptive verify-m22 systemc \
	verify-directional verify-mmio-directional verify-m3 verify-fifo verify-m7a \
	systemc-native verify-systemc-native verify-systemc

all: $(ENCODER_BIN) build/vpu_mmio_demo build/yuvgen

$(ENCODER_BIN): app/main.cpp $(CORE_SRC) $(MODEL_SRC) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

build/yuvgen: tools/yuvgen.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build/vpu_mmio_demo: app/mmio_demo.cpp $(CORE_SRC) src/vpu_mmio.cpp $(FIFO_SRC) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

build/test_core: tests/test_core.cpp src/bit_writer.cpp src/cabac.cpp src/yuv420.cpp src/transform.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

build/test_vpu_mmio: tests/test_vpu_mmio.cpp $(CORE_SRC) src/vpu_mmio.cpp $(FIFO_SRC) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

build/test_fifo_cycle_model: tests/test_fifo_cycle_model.cpp $(FIFO_SRC) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

build:
	mkdir -p build

test: build/test_core build/test_vpu_mmio build/test_fifo_cycle_model
	./build/test_core
	./build/test_vpu_mmio
	./build/test_fifo_cycle_model

verify: all test
	./build/yuvgen build/test_96x66.yuv 96 66 2
	./$(ENCODER_BIN) -i build/test_96x66.yuv -w 96 -h 66 -n 2 -o build/test.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/test.h265
	ffmpeg -v error -i build/test.h265 -f rawvideo -pix_fmt yuv420p -y build/decoded.yuv
	cmp build/test_96x66.yuv build/decoded.yuv

verify-mmio: all test
	./build/yuvgen build/mmio_input.yuv 96 66 2
	./build/vpu_mmio_demo -i build/mmio_input.yuv -w 96 -h 66 -n 2 -o build/mmio_output.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/mmio_output.h265
	ffmpeg -v error -i build/mmio_output.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_decoded.yuv
	cmp build/mmio_input.yuv build/mmio_decoded.yuv

verify-intra: all test
	./build/yuvgen build/intra_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/intra_input.yuv -w 96 -h 66 -n 2 \
		--mode hybrid-dc --qp 26 --recon build/intra_recon.yuv \
		-o build/intra.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/intra.h265
	ffmpeg -v error -i build/intra.h265 -f rawvideo -pix_fmt yuv420p -y build/intra_decoded.yuv
	cmp build/intra_recon.yuv build/intra_decoded.yuv
	@test $$(stat -c %s build/intra.h265) -lt $$(stat -c %s build/intra_input.yuv)

verify-mmio-intra: all test
	./build/yuvgen build/mmio_intra_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/mmio_intra_input.yuv -w 96 -h 66 -n 2 \
		--mode hybrid-dc --qp 26 --recon build/mmio_intra_recon.yuv \
		-o build/mmio_intra_reference.h265
	./build/vpu_mmio_demo -i build/mmio_intra_input.yuv -w 96 -h 66 -n 2 \
		--mode hybrid-dc --qp 26 -o build/mmio_intra.h265
	ffmpeg -v error -i build/mmio_intra.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_intra_decoded.yuv
	cmp build/mmio_intra_recon.yuv build/mmio_intra_decoded.yuv

verify-m1: verify verify-mmio verify-intra verify-mmio-intra

verify-tq: all test
	./build/yuvgen build/tq_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/tq_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-dc-tq --qp 26 --recon build/tq_recon.yuv \
		-o build/tq.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/tq.h265
	ffmpeg -v error -i build/tq.h265 -f rawvideo -pix_fmt yuv420p -y build/tq_decoded.yuv
	cmp build/tq_recon.yuv build/tq_decoded.yuv
	@test $$(stat -c %s build/tq.h265) -lt $$(stat -c %s build/tq_input.yuv)

verify-mmio-tq: all test
	./build/yuvgen build/mmio_tq_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/mmio_tq_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-dc-tq --qp 26 --recon build/mmio_tq_recon.yuv \
		-o build/mmio_tq_reference.h265
	./build/vpu_mmio_demo -i build/mmio_tq_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-dc-tq --qp 26 -o build/mmio_tq.h265
	ffmpeg -v error -i build/mmio_tq.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_tq_decoded.yuv
	cmp build/mmio_tq_recon.yuv build/mmio_tq_decoded.yuv

verify-m2: verify-m1 verify-tq verify-mmio-tq

verify-full: all test
	./build/yuvgen build/full_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/full_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-full-tq --qp 26 --recon build/full_recon.yuv \
		-o build/full.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/full.h265
	ffmpeg -v error -i build/full.h265 -f rawvideo -pix_fmt yuv420p -y build/full_decoded.yuv
	cmp build/full_recon.yuv build/full_decoded.yuv
	@test $$(stat -c %s build/full.h265) -lt $$(stat -c %s build/full_input.yuv)

verify-mmio-full: all test
	./build/yuvgen build/mmio_full_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/mmio_full_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-full-tq --qp 26 --recon build/mmio_full_recon.yuv \
		-o build/mmio_full_reference.h265
	./build/vpu_mmio_demo -i build/mmio_full_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-full-tq --qp 26 -o build/mmio_full.h265
	cmp build/mmio_full_reference.h265 build/mmio_full.h265
	ffmpeg -v error -i build/mmio_full.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_full_decoded.yuv
	cmp build/mmio_full_recon.yuv build/mmio_full_decoded.yuv

verify-m21: verify-m2 verify-full verify-mmio-full

verify-tu16: all test
	./build/yuvgen build/tu16_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/tu16_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-full-tq16 --qp 26 --recon build/tu16_recon.yuv \
		-o build/tu16.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/tu16.h265
	ffmpeg -v error -i build/tu16.h265 -f rawvideo -pix_fmt yuv420p -y build/tu16_decoded.yuv
	cmp build/tu16_recon.yuv build/tu16_decoded.yuv
	@test $$(stat -c %s build/tu16.h265) -lt $$(stat -c %s build/tu16_input.yuv)

verify-mmio-tu16: all test
	./build/yuvgen build/mmio_tu16_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/mmio_tu16_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-full-tq16 --qp 26 --recon build/mmio_tu16_recon.yuv \
		-o build/mmio_tu16_reference.h265
	./build/vpu_mmio_demo -i build/mmio_tu16_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-full-tq16 --qp 26 -o build/mmio_tu16.h265
	cmp build/mmio_tu16_reference.h265 build/mmio_tu16.h265
	ffmpeg -v error -i build/mmio_tu16.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_tu16_decoded.yuv
	cmp build/mmio_tu16_recon.yuv build/mmio_tu16_decoded.yuv

verify-adaptive: all test
	./build/yuvgen build/adaptive_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/adaptive_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-adaptive-tq --qp 26 --recon build/adaptive_recon.yuv \
		-o build/adaptive.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/adaptive.h265
	ffmpeg -v error -i build/adaptive.h265 -f rawvideo -pix_fmt yuv420p -y build/adaptive_decoded.yuv
	cmp build/adaptive_recon.yuv build/adaptive_decoded.yuv
	@test $$(stat -c %s build/adaptive.h265) -lt $$(stat -c %s build/adaptive_input.yuv)

verify-mmio-adaptive: all test
	./build/yuvgen build/mmio_adaptive_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/mmio_adaptive_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-adaptive-tq --qp 26 --recon build/mmio_adaptive_recon.yuv \
		-o build/mmio_adaptive_reference.h265
	./build/vpu_mmio_demo -i build/mmio_adaptive_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-adaptive-tq --qp 26 -o build/mmio_adaptive.h265
	cmp build/mmio_adaptive_reference.h265 build/mmio_adaptive.h265
	ffmpeg -v error -i build/mmio_adaptive.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_adaptive_decoded.yuv
	cmp build/mmio_adaptive_recon.yuv build/mmio_adaptive_decoded.yuv

verify-m22: verify-m21 verify-tu16 verify-mmio-tu16 \
	verify-adaptive verify-mmio-adaptive

verify-directional: all test
	./build/yuvgen build/directional_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/directional_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-directional-tq --qp 26 --recon build/directional_recon.yuv \
		-o build/directional.h265
	ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 build/directional.h265
	ffmpeg -v error -i build/directional.h265 -f rawvideo -pix_fmt yuv420p -y build/directional_decoded.yuv
	cmp build/directional_recon.yuv build/directional_decoded.yuv
	@test $$(stat -c %s build/directional.h265) -lt $$(stat -c %s build/directional_input.yuv)

verify-mmio-directional: all test
	./build/yuvgen build/mmio_directional_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/mmio_directional_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-directional-tq --qp 26 --recon build/mmio_directional_recon.yuv \
		-o build/mmio_directional_reference.h265
	./build/vpu_mmio_demo -i build/mmio_directional_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-directional-tq --qp 26 -o build/mmio_directional.h265
	cmp build/mmio_directional_reference.h265 build/mmio_directional.h265
	ffmpeg -v error -i build/mmio_directional.h265 -f rawvideo -pix_fmt yuv420p -y build/mmio_directional_decoded.yuv
	cmp build/mmio_directional_recon.yuv build/mmio_directional_decoded.yuv

verify-m3: verify-m22 verify-directional verify-mmio-directional

verify-fifo: all test
	./build/yuvgen build/fifo_input.yuv 96 66 2
	./$(ENCODER_BIN) -i build/fifo_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-directional-tq --qp 26 --recon build/fifo_recon.yuv \
		-o build/fifo_reference.h265
	./build/vpu_mmio_demo -i build/fifo_input.yuv -w 96 -h 66 -n 2 \
		--mode intra-directional-tq --qp 26 -o build/fifo_mmio.h265
	cmp build/fifo_reference.h265 build/fifo_mmio.h265
	ffmpeg -v error -i build/fifo_mmio.h265 -f rawvideo -pix_fmt yuv420p -y build/fifo_decoded.yuv
	cmp build/fifo_recon.yuv build/fifo_decoded.yuv

verify-m7a: verify-m3 verify-fifo

systemc: | build
	$(CXX) $(CPPFLAGS) $(SYSTEMC_CPPFLAGS) $(CXXFLAGS) -DHEVC_HAVE_SYSTEMC \
		optional_systemc/sc_main.cpp $(CORE_SRC) src/vpu_mmio.cpp $(FIFO_SRC) \
		$(SYSTEMC_LDFLAGS) $(SYSTEMC_LDLIBS) -o $(SYSTEMC_BIN)

systemc-native: | build
	$(CXX) $(CPPFLAGS) $(SYSTEMC_CPPFLAGS) $(CXXFLAGS) -DHEVC_HAVE_SYSTEMC \
		optional_systemc/sc_native_main.cpp \
		optional_systemc/vpu_systemc_native.cpp $(CORE_SRC) \
		$(SYSTEMC_LDFLAGS) $(SYSTEMC_LDLIBS) -o $(SYSTEMC_NATIVE_BIN)

verify-systemc-native: all systemc-native
	./build/yuvgen build/systemc_native_input.yuv 96 66 8
	./$(ENCODER_BIN) -i build/systemc_native_input.yuv -w 96 -h 66 -n 8 \
		--mode intra-directional-tq --qp 26 \
		--recon build/systemc_native_recon.yuv \
		-o build/systemc_native_reference.h265
	./$(SYSTEMC_NATIVE_BIN) -i build/systemc_native_input.yuv -w 96 -h 66 -n 8 \
		--mode intra-directional-tq --qp 26 --fifo-depth 1 --require-stall \
		-o build/systemc_native_d1.h265
	./$(SYSTEMC_NATIVE_BIN) -i build/systemc_native_input.yuv -w 96 -h 66 -n 8 \
		--mode intra-directional-tq --qp 26 --fifo-depth 4 \
		-o build/systemc_native_d4.h265
	cmp build/systemc_native_reference.h265 build/systemc_native_d1.h265
	cmp build/systemc_native_reference.h265 build/systemc_native_d4.h265
	ffprobe -v error -select_streams v:0 \
		-show_entries stream=codec_name,width,height,pix_fmt -of default=nw=1 \
		build/systemc_native_d4.h265
	ffmpeg -v error -i build/systemc_native_d4.h265 -f rawvideo \
		-pix_fmt yuv420p -y build/systemc_native_decoded.yuv
	cmp build/systemc_native_recon.yuv build/systemc_native_decoded.yuv
	./$(SYSTEMC_NATIVE_BIN) -i build/systemc_native_input.yuv -w 96 -h 66 -n 1 \
		--mode intra-directional-tq --qp 26 --fifo-depth 1 \
		--dst-capacity 1 --expect-error 3 \
		-o build/systemc_native_must_not_exist.h265

verify-systemc: verify-systemc-native

clean:
	rm -rf build
