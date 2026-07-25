# A normal makefile assignment intentionally overrides a stale environment
# variable, while `make SYSTEMC_HOME=/other/prefix` still takes precedence.
SYSTEMC_HOME := /opt/systemc-2.3.4
BUILD_DIR ?= build
CMAKE ?= cmake

export CC := /usr/bin/gcc
export CXX := /usr/bin/g++
export PATH := /usr/bin:/bin:$(PATH)

.PHONY: all sanity configure test clean

all: test

sanity:
	$(CC) -dumpfullversion
	$(CXX) --version | head

configure: sanity
	$(CMAKE) -S . -B $(BUILD_DIR) \
		-DSYSTEMC_HOME=$(SYSTEMC_HOME) \
		-DFLOO_NOC_MODEL_BUILD_TESTS=ON

test: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	$(CMAKE) -E remove_directory $(BUILD_DIR)
