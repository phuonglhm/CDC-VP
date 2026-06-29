BUILD_DIR ?= ../../build
TARGET ?= test_video_encoder_tlm

.PHONY: all test

all:
	cmake --build $(BUILD_DIR) --target video_encoder_tlm

test:
	cmake --build $(BUILD_DIR) --target $(TARGET)
	$(BUILD_DIR)/components/video_encoder_tlm/tests/$(TARGET)
