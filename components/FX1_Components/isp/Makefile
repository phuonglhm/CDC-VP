# Convenience wrapper around CMake for the FX1 ISP component.
#
# Every recipe runs the mandatory environment setup (project plan §8.1) in the
# same shell before invoking any tool, and logs the toolchain sanity output.
#
#   make            configure + build
#   make test       build + run CTest
#   make csr-check  verify generated CSR files (needs CSR_XLSX=<path>)
#   make csr-gen    regenerate CSR files        (needs CSR_XLSX=<path>)
#   make clean      remove the build directory

BUILD_DIR ?= build
BUILD_TYPE ?= RelWithDebInfo
CSR_XLSX ?=

ENV_SETUP = export CC=/usr/bin/gcc; export CXX=/usr/bin/g++; export PATH=/usr/bin:/bin:$$PATH; \
	mkdir -p $(BUILD_DIR); \
	{ date -Is; $$CC -dumpfullversion; $$CXX --version | head -1; } > $(BUILD_DIR)/toolchain.log; \
	cat $(BUILD_DIR)/toolchain.log;

CMAKE_ARGS = -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_CXX_COMPILER=/usr/bin/g++
ifneq ($(CSR_XLSX),)
CMAKE_ARGS += -DFX1_ISP_CSR_XLSX=$(abspath $(CSR_XLSX))
endif

.PHONY: all configure build test csr-check csr-gen clean

all: build

configure:
	@$(ENV_SETUP) \
	cached=$$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' $(BUILD_DIR)/CMakeCache.txt 2>/dev/null); \
	if [ -n "$$cached" ] && [ "$$cached" != "/usr/bin/g++" ]; then \
	  echo "error: $(BUILD_DIR) was configured with $$cached; use a new BUILD_DIR"; exit 1; fi; \
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

build: configure
	@$(ENV_SETUP) cmake --build $(BUILD_DIR)

test: build
	@$(ENV_SETUP) ctest --test-dir $(BUILD_DIR) --output-on-failure

csr-check:
	@test -n "$(CSR_XLSX)" || { echo "usage: make csr-check CSR_XLSX=<path>"; exit 1; }
	@$(ENV_SETUP) python3 tools/gen_csr.py --xlsx "$(CSR_XLSX)" --check && \
	python3 tools/check_csr_header.py --xlsx "$(CSR_XLSX)" --header include/fx1_isp/fx1_isp_csr.h

csr-gen:
	@test -n "$(CSR_XLSX)" || { echo "usage: make csr-gen CSR_XLSX=<path>"; exit 1; }
	@$(ENV_SETUP) python3 tools/gen_csr.py --xlsx "$(CSR_XLSX)"

clean:
	rm -rf $(BUILD_DIR)
