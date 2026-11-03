BUILD_DIR ?= build
SYSTEMC_HOME ?= /opt/systemc-2.3.4
JOBS ?= 2
.PHONY: all configure test
all: configure
	cmake --build "$(BUILD_DIR)" --parallel $(JOBS)
configure:
	cmake -S . -B "$(BUILD_DIR)" -DSYSTEMC_HOME="$(SYSTEMC_HOME)" -DCMAKE_BUILD_TYPE=Release
test: all
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure --no-tests=error
