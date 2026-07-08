# Legacy helper for running one encoder test from an existing top-level CMake
# build tree. Canonical build/test flow for vpu_tlm is CMake + CTest.

BUILD_DIR := ../../build
TARGET := test_video_encoder_tlm
EXE := $(BUILD_DIR)/components/video_encoder_tlm/tests/$(TARGET)

.PHONY: all run clean

all:
	cmake --build $(BUILD_DIR) --target $(TARGET)

run: all
	$(EXE)

clean:
	cmake --build $(BUILD_DIR) --target clean
