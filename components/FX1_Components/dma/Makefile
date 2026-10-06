CMAKE ?= cmake
CTEST ?= ctest
BUILD_DIR ?= build
SYSTEMC_HOME ?= /opt/systemc-2.3.4
CMAKE_ARGS ?=

.PHONY: all configure test
all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel 2
configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DSYSTEMC_HOME="$(SYSTEMC_HOME)" -DFX1_DMA_BUILD_TESTS=ON $(CMAKE_ARGS)
test: all
	$(CTEST) --test-dir "$(BUILD_DIR)" --output-on-failure
