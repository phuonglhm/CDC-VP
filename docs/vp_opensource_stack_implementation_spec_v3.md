# Virtual Platform Open-Source Stack — Implementation Spec

> **Mục tiêu dự án**: Xây dựng Virtual Platform thay thế Arm FVP/Fast Models, hoàn toàn dựa trên open-source, ưu tiên CPU model dạng SystemC/TLM, bus/router do team tự control, phục vụ firmware/software bring-up và custom IP modeling nội bộ.

---

## 0. Environment Setup (one-time)

**Target host**: AlmaLinux 9.7 native host.

**Optional reproducible environment**: Docker/Podman container `ubuntu:24.04` nếu cần build theo môi trường Ubuntu sạch. Tuy nhiên mặc định tài liệu này dùng AlmaLinux 9.7.

### System packages — AlmaLinux 9.7

```bash
# Enable common development repositories
sudo dnf install -y dnf-plugins-core epel-release
sudo dnf config-manager --set-enabled crb || true

# Base build tools
sudo dnf groupinstall -y "Development Tools"

# Native development packages
sudo dnf install -y \
    gcc gcc-c++ clang cmake ninja-build git pkgconf-pkg-config \
    python3 python3-pip python3-devel \
    flex bison unzip wget curl which file make ccache \
    autoconf automake libtool \
    glib2-devel pixman-devel libslirp-devel \
    asio-devel lua-devel \
    libusbx-devel usbredir-devel \
    ncurses-devel zlib-devel expat-devel openssl-devel \
    perl perl-FindBin perl-IPC-Cmd perl-devel perl-ExtUtils-MakeMaker \
    gdb gdb-gdbserver

# Python test/build tools
python3 -m pip install --user --upgrade pip
python3 -m pip install --user meson pytest pexpect pyserial pyyaml jinja2
```

> Note: AlmaLinux/RHEL-family không có `apt` và thường không có `gdb-multiarch` giống Ubuntu. Với cross-debug, ưu tiên dùng `gdb` đi kèm cross toolchain, ví dụ `riscv64-unknown-elf-gdb`, `riscv64-linux-gnu-gdb`, hoặc toolchain-specific GDB tương ứng.

### Optional Docker/Podman Ubuntu 24.04 environment

Chỉ dùng phần này nếu muốn tái lập môi trường Ubuntu trong container, không phụ thuộc package của AlmaLinux host.

```bash
# Set CDC_VP_ROOT trỏ về repo path, hoặc dùng $(pwd) khi đang ở repo
export CDC_VP_ROOT="$(pwd)"

podman run --rm -it \
    -v "${CDC_VP_ROOT}":/work/cdc-vp:Z \
    -w /work/cdc-vp \
    ubuntu:24.04 bash
```

> **SELinux note**: AlmaLinux enforce SELinux mặc định — thêm `:Z` vào volume mount để Docker/Podman relabel cho phép container access. Podman CLI tương thích Docker trong đa số flow dev.

Bên trong container Ubuntu:

```bash
apt update && apt install -y \
    build-essential cmake ninja-build git pkg-config \
    g++ clang python3 python3-pip python3-dev \
    flex bison unzip wget curl ccache \
    libpixman-1-dev libglib2.0-dev libslirp-dev \
    libasio-dev liblua5.4-dev \
    libusb-1.0-0-dev libusbredirhost-dev \
    gcc-riscv64-unknown-elf gcc-riscv64-linux-gnu \
    gdb-multiarch

python3 -m pip install --break-system-packages \
    meson pytest pexpect pyserial pyyaml jinja2
```

### Toolchains — AlmaLinux 9.7 recommendation

| Target | Recommended install method on AlmaLinux 9.7 |
|---|---|
| RISC-V baremetal | Install prebuilt `riscv64-unknown-elf` toolchain to `/opt/toolchains/riscv-unknown-elf`, or build with crosstool-NG |
| RISC-V Linux | Install prebuilt `riscv64-linux-gnu` toolchain, or use Ubuntu Docker package `gcc-riscv64-linux-gnu` |
| Arm baremetal | Optional. Install official Arm GNU Toolchain 13.x+ tarball to `/opt/toolchains/arm-gnu-toolchain` if Arm firmware test is needed |
| Arm Linux | Optional. Use official Arm GNU Toolchain / Linaro `aarch64-none-linux-gnu`, or use Ubuntu Docker package `gcc-aarch64-linux-gnu` |
| Verilator | Build from source `v5.026+` and install to `/opt/verilator-5.x` |

Recommended environment variables:

```bash
export TOOLCHAIN_ROOT=/opt/toolchains

# RISC-V baremetal example
export RISCV_HOME=$TOOLCHAIN_ROOT/riscv-unknown-elf
export PATH=$RISCV_HOME/bin:$PATH

# Verilator example
export VERILATOR_HOME=/opt/verilator-5.x
export PATH=$VERILATOR_HOME/bin:$PATH
```

### SELinux context cho `/opt/` (AlmaLinux)

AlmaLinux 9 enforce SELinux. Binary install vào `/opt/toolchains/` hoặc `/opt/verilator-5.x` có thể bị deny execute do thiếu label `bin_t`. Nếu gặp `Permission denied` khi chạy toolchain:

```bash
# Cách nhanh — relabel
sudo restorecon -Rv /opt/verilator-5.x /opt/toolchains

# Cách persistent — set policy cho mọi /opt/*/bin
sudo semanage fcontext -a -t bin_t "/opt/(.*)?/bin(/.*)?"
sudo restorecon -Rv /opt/
```

Nếu vẫn fail, check bằng `ls -Z /opt/verilator-5.x/bin/verilator` — phải có `bin_t` trong label.

**Compiler requirement**: C++17 minimum. C++20 khuyến nghị nếu CPU backend hoặc utility layer cần.

---

## 1. Repo Skeleton

```text
cdc-vp/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── .gitignore
├── .clang-format
├── .editorconfig
│
├── cmake/
│   ├── toolchains/
│   │   ├── arm-none-eabi.cmake
│   │   └── riscv-none-elf.cmake
│   ├── modules/
│   └── CPM.cmake
│
├── containers/
│   └── Dockerfile                    # optional reproducible Ubuntu path
│
├── ci/
│   └── gitlab-ci.yml                 # hoặc .github/workflows/
│
├── cpu_models/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── cdc/cpu/cpu_base.h        # abstract CPU interface
│   ├── riscv_vp/                     # Bremen RISC-V VP wrapper, PRIMARY
│   ├── riscv_tlm/                    # mariusmm RISC-V-TLM wrapper, BACKUP
│   ├── dbt_rise/                     # optional backup, only if selected
│   └── simple_riscv_sc/              # Nikos reference, optional/learning
│
├── platforms/
│   ├── mini_tlm/                     # Step 1
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   ├── configs/
│   │   ├── run/
│   │   └── README.md
│   │
│   ├── riscv_cpu_eval/               # Step 2
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   ├── configs/
│   │   ├── run/
│   │   └── README.md
│   │
│   ├── riscv_custom_soc/             # Step 3
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   ├── configs/
│   │   ├── run/
│   │   └── README.md
│   │
│   ├── riscv_linux_vp/               # Step 4
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   ├── configs/
│   │   ├── run/
│   │   └── README.md
│   │
│   └── cosim_demo/                   # Step 6
│       ├── CMakeLists.txt
│       ├── src/
│       ├── rtl/
│       ├── configs/
│       ├── run/
│       └── README.md
│
├── components/
│   ├── common/                       # logger, types, helpers
│   ├── bus_router/
│   ├── memory_tlm/
│   ├── uart_tlm/
│   ├── timer_tlm/
│   ├── irq_aggregator/
│   ├── clint_tlm/
│   ├── plic_tlm/
│   ├── dma_tlm/                      # Step 5
│   └── <other_custom_ip>_tlm/        # Step 5
│
├── fw/
│   ├── common/
│   │   ├── startup/
│   │   ├── linker/
│   │   └── drivers/
│   ├── hello_baremetal_riscv/
│   ├── zephyr_app/
│   └── linux_minimal/                # kernel config + initramfs
│
├── tests/
│   ├── conftest.py
│   ├── unit/                         # per-component tests
│   ├── integration/                  # cross-component tests
│   ├── system/                       # full boot tests
│   └── perf/                         # nightly benchmark tests
│
├── tools/
│   ├── setup_env.sh
│   ├── run_platform.py
│   ├── gen_memory_map.py
│   ├── elf_to_bin.py
│   └── parse_log.py
│
├── docs/
│   ├── getting_started.md
│   ├── architecture.md
│   ├── cpu_integration_strategy.md
│   ├── cpu_benchmark_results.md
│   ├── performance_expectations.md
│   ├── linux_boot_fallback_plan.md
│   ├── interrupt_modeling_policy.md
│   └── memory_map.md
│
├── models/                           # Git LFS pointers or small reference artifacts only
├── third_party/                      # submodules: Bremen, mariusmm, libsystemctlm-soc, ...
├── build/                            # gitignored
├── out/                              # gitignored, runtime artifacts/logs/VCD
└── deps/                             # gitignored, CPM cache/downloads
```

### Repository policy

- `third_party/`: dùng cho upstream source/submodule. Default integration strategy là **submodule + thin wrapper**.
- `models/`: chỉ chứa Git LFS pointers hoặc artifact nhỏ. Không commit kernel/rootfs/image nặng trực tiếp vào git. Artifact lớn nên đặt ở internal artifact server hoặc release package.
- `build/`, `out/`, `deps/`: luôn gitignored.
- Mỗi platform giữ config local trong `platforms/<name>/configs/` để tránh global config bị rối khi số platform tăng.

---

## 2. Stack Dependencies

| Component | Role | Version/Policy | Source | License |
|---|---|---|---|---|
| **Core** | | | | |
| Accellera SystemC | TLM/SystemC kernel | 2.3.4 recommended | github.com/accellera-official/systemc | Apache 2.0 |
| SystemC CCI | Optional configuration layer | 1.0.0 | github.com/accellera-official/cci | Apache 2.0 |
| VCML | Optional helper library for TLM components | pin commit | github.com/machineware-gmbh/vcml | Apache 2.0 |
| Lua | Platform config | 5.4 | dnf/bundled/source | MIT |
| **CPU candidates** | | | | |
| RISC-V VP Bremen | PRIMARY CPU TLM candidate | submodule + thin wrapper | github.com/agra-uni-bremen/riscv-vp | check upstream |
| mariusmm RISC-V-TLM | BACKUP/reference CPU candidate | submodule or fork if needed | github.com/mariusmm/RISC-V-TLM | check upstream |
| NikosMouzakitis/RISCV-SystemC | Learning/reference only | optional | github.com/NikosMouzakitis/RISCV-SystemC | check upstream |
| DBT-RISE/TGC | Optional backup if performance/SMP requires | evaluate only if selected | TBD | check upstream |
| **Optional fallback / hybrid** | | | | |
| QBox/QEMU | Optional hybrid fallback, not primary path | use only if native SystemC CPU model fails expectation | github.com/qualcomm/qbox | QBox Apache 2.0, QEMU GPLv2 |
| **Optional RTL co-sim** | | | | |
| Verilator | RTL-to-SystemC model generation | v5.026+ | github.com/verilator/verilator | LGPL/Artistic |
| libsystemctlm-soc | TLM ↔ AXI bridge | pin commit | github.com/Xilinx/libsystemctlm-soc | BSD |
| **Test infra** | | | | |
| pytest, pexpect, pyserial | Regression | latest/pinned by requirements | pip | MIT |

**License note**: QBox/QEMU không còn là path chính. Nếu dùng QBox làm fallback/hybrid, cần review lại distribution policy vì QEMU là GPLv2.

---

## 3. Build Templates

### 3.1 Top-level `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.21)
project(cdc-vp
    VERSION 0.1.0
    LANGUAGES CXX C
    DESCRIPTION "Open-source Virtual Platform stack with SystemC/TLM CPU models")

# ─── Standards ─────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Build options ─────────────────────────────────────────
option(CDC_BUILD_MINI_TLM      "Build mini TLM platform (Step 1)"          ON)
option(CDC_BUILD_CPU_EVAL      "Build CPU TLM evaluation platform"        ON)
option(CDC_BUILD_CUSTOM_SOC    "Build custom RISC-V SoC platform"         ON)
option(CDC_BUILD_LINUX_VP      "Build RISC-V Linux VP platform"           OFF)
option(CDC_BUILD_COSIM         "Build RTL co-sim platform (Step 6)"        OFF)
option(CDC_BUILD_TESTS         "Build component/platform tests"            ON)
option(CDC_ENABLE_VCML         "Enable VCML helper components"             ON)
option(CDC_ENABLE_QBOX         "Enable QBox fallback/hybrid backend"       OFF)

set(CDC_CPU_BACKEND "riscv_vp" CACHE STRING
    "CPU backend: riscv_vp, riscv_tlm, dbt_rise")
set_property(CACHE CDC_CPU_BACKEND PROPERTY STRINGS
    riscv_vp riscv_tlm dbt_rise)

# ─── CPM bootstrap ─────────────────────────────────────────
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})

# ─── Core dependencies ─────────────────────────────────────
CPMAddPackage(
    NAME       systemc
    GITHUB_REPOSITORY accellera-official/systemc
    GIT_TAG    2.3.4
    OPTIONS    "ENABLE_PHASE_CALLBACKS_TRACING OFF")

if(CDC_ENABLE_VCML)
    CPMAddPackage(
        NAME       vcml
        GITHUB_REPOSITORY machineware-gmbh/vcml
        GIT_TAG    main)  # TODO: pin commit hash when stable
endif()

# QBox is optional fallback only, not the primary roadmap path.
if(CDC_ENABLE_QBOX)
    CPMAddPackage(
        NAME       qbox
        GITHUB_REPOSITORY qualcomm/qbox
        GIT_TAG    main)  # TODO: pin commit if used
endif()

# ─── Subdirectories ────────────────────────────────────────
add_subdirectory(components)
add_subdirectory(cpu_models)
add_subdirectory(platforms)
add_subdirectory(fw)

if(CDC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
endif()
```

### 3.2 `CMakePresets.json`

```json
{
  "version": 5,
  "configurePresets": [
    {
      "name": "debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CDC_BUILD_COSIM": "OFF",
        "CDC_CPU_BACKEND": "riscv_vp"
      }
    },
    {
      "name": "release",
      "inherits": "debug",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "cpu-bremen",
      "inherits": "debug",
      "binaryDir": "${sourceDir}/build/cpu-bremen",
      "cacheVariables": {
        "CDC_CPU_BACKEND": "riscv_vp"
      }
    },
    {
      "name": "cpu-mariusmm",
      "inherits": "debug",
      "binaryDir": "${sourceDir}/build/cpu-mariusmm",
      "cacheVariables": {
        "CDC_CPU_BACKEND": "riscv_tlm"
      }
    },
    {
      "name": "cosim",
      "inherits": "debug",
      "binaryDir": "${sourceDir}/build/cosim",
      "cacheVariables": {
        "CDC_BUILD_COSIM": "ON"
      }
    }
  ],
  "buildPresets": [
    {"name": "debug",       "configurePreset": "debug"},
    {"name": "release",     "configurePreset": "release"},
    {"name": "cpu-bremen",  "configurePreset": "cpu-bremen"},
    {"name": "cpu-mariusmm","configurePreset": "cpu-mariusmm"},
    {"name": "cosim",       "configurePreset": "cosim"}
  ]
}
```

Sử dụng:

```bash
cmake --preset debug
cmake --build --preset debug
```

### 3.3 CPU abstraction interface — `cpu_models/include/cdc/cpu/cpu_base.h`

```cpp
#pragma once

#include <string>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

namespace cdc::cpu {

struct cpu_config {
    unsigned xlen = 64;
    unsigned num_irq = 64;
    bool has_mmu = false;
    bool has_smp = false;
    bool split_instr_data_bus = false;
};

class cpu_base : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<cpu_base> instr_bus;
    tlm_utils::simple_initiator_socket<cpu_base> data_bus;
    sc_core::sc_vector<sc_core::sc_in<bool>> irq_in;

    cpu_config cfg;

    cpu_base(sc_core::sc_module_name name, const cpu_config& config)
        : sc_core::sc_module(name)
        , instr_bus("instr_bus")
        , data_bus("data_bus")
        , irq_in("irq_in", config.num_irq)
        , cfg(config) {}

    virtual ~cpu_base() = default;

    virtual void load_elf(const std::string& path) = 0;
    virtual void load_bin(const std::string& path, uint64_t load_addr) = 0;

    virtual void reset_cpu() = 0;
    virtual void halt_cpu() = 0;

    virtual uint64_t get_pc() const = 0;
    virtual uint64_t get_cycle_count() const = 0;

    virtual void set_quantum(sc_core::sc_time q) = 0;
    virtual std::string backend_name() const = 0;
};

} // namespace cdc::cpu
```

### 3.4 Per-platform `CMakeLists.txt` — example `platforms/riscv_custom_soc/`

```cmake
add_executable(riscv_custom_soc
    src/main.cpp
    src/platform.cpp)

target_link_libraries(riscv_custom_soc PRIVATE
    cdc::components::common
    cdc::components::bus_router
    cdc::components::memory_tlm
    cdc::components::uart_tlm
    cdc::components::timer_tlm
    cdc::components::plic_tlm
    cdc::components::clint_tlm
    cdc::cpu::${CDC_CPU_BACKEND})

target_include_directories(riscv_custom_soc PRIVATE src)

add_custom_command(TARGET riscv_custom_soc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/configs
            $<TARGET_FILE_DIR:riscv_custom_soc>/configs)
```

### 3.5 Per-component `CMakeLists.txt` — example `components/uart_tlm/`

```cmake
add_library(uart_tlm STATIC
    src/uart_tlm.cpp)
add_library(cdc::components::uart_tlm ALIAS uart_tlm)

target_include_directories(uart_tlm
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE src)

target_link_libraries(uart_tlm PUBLIC systemc)

if(CDC_ENABLE_VCML)
    target_link_libraries(uart_tlm PUBLIC vcml::vcml)
    target_compile_definitions(uart_tlm PUBLIC CDC_ENABLE_VCML=1)
endif()

if(CDC_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

### 3.6 `containers/Dockerfile` — optional Ubuntu dev image

```dockerfile
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git pkg-config \
    g++ clang python3 python3-pip python3-dev \
    flex bison unzip wget curl ca-certificates ccache \
    libpixman-1-dev libglib2.0-dev libslirp-dev \
    libasio-dev liblua5.4-dev \
    libusb-1.0-0-dev libusbredirhost-dev \
    gcc-riscv64-unknown-elf gcc-riscv64-linux-gnu \
    gdb-multiarch \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages \
    meson pytest pexpect pyserial pyyaml jinja2

# Build Verilator from source
ARG VERILATOR_VERSION=v5.026
RUN git clone --branch ${VERILATOR_VERSION} --depth 1 \
        https://github.com/verilator/verilator.git /tmp/verilator && \
    cd /tmp/verilator && autoconf && ./configure --prefix=/opt/verilator && \
    make -j$(nproc) && make install && \
    rm -rf /tmp/verilator
ENV PATH="/opt/verilator/bin:${PATH}"

WORKDIR /workspace
```

Build & run từ AlmaLinux host:

```bash
podman build -t cdc-vp:dev containers/
podman run --rm -it -v "$(pwd)":/workspace:Z cdc-vp:dev bash
```

**Tip cho dev session dài**: dùng `podman run --name cdc-dev -d` để keep container chạy nền, sau đó `podman exec -it cdc-dev bash` mỗi lần vào.

---

## 4. Roadmap Implementation Details

### Step 0 — Requirement Alignment

**Mục tiêu**: chốt expectation với sếp/architect trước khi đầu tư vào CPU model và Linux boot.

#### Checklist

- Confirm single-core / multi-core requirement.
- Nếu multi-core:
  - Confirm số core mục tiêu: 2 / 4 / 8.
  - Confirm có cần cache coherency model không.
  - Nếu cần multi-core, loại sớm candidate single-core-only khỏi path chính.
- Confirm Linux expectation:
  - Minimal initramfs only.
  - Không scope full Ubuntu/Debian.
  - Không scope GUI/X11.
- Confirm acceptable performance budget:
  - Linux boot: `< 10 phút` acceptable cho native SystemC.
  - Regression 100 test cases: `< 60 phút`.
- Confirm primary/fallback strategy:
  - Primary: Bremen RISC-V VP wrapper.
  - Backup: mariusmm hoặc DBT-RISE tùy single-core/multi-core decision.
  - QBox/QEMU chỉ là fallback/hybrid path, không phải main path.

#### Deliverable

- `docs/performance_expectations.md`
- `docs/linux_boot_fallback_plan.md`
- `docs/cpu_integration_strategy.md`
- Decision document signed-off bởi sếp/architect.

---

### Step 1 — `mini_tlm` — SystemC/TLM thuần

**Mục tiêu**: chứng minh TLM-2.0 backbone trước khi đưa CPU model thật vào.

#### Tasks

1. CMake project + link Accellera SystemC.
2. Implement `cpu_stub` sinh TLM-2.0 generic payload transaction tuần tự.
3. Implement `bus_router` với address decode.
4. Implement `memory_tlm` cho ROM/RAM với `b_transport`, `transport_dbg`, DMI.
5. Implement `uart_tlm` memory-mapped, stdout backend.
6. Implement `timer_tlm` memory-mapped, IRQ output.
7. Implement `irq_aggregator` hoặc IRQ controller đơn giản.
8. Top-level `sc_main()` wire mọi component.
9. Platform-local config trong `platforms/mini_tlm/configs/`.

#### Deliverable

- `platforms/mini_tlm/` chạy được Hello World qua UART.
- Timer trigger IRQ sau 10ms.
- TLM-2.0 bus/router hoạt động với register read/write.

#### Validation

```bash
cmake --preset debug
cmake --build --preset debug --target mini_tlm

./build/debug/platforms/mini_tlm/mini_tlm \
    -c platforms/mini_tlm/configs/default.yaml

# Expected:
#   Hello from mini VP
#   Timer IRQ fired at 10ms
```

---

### Step 2 — CPU TLM Evaluation

**Mục tiêu**: chọn CPU model dạng SystemC/TLM dựa trên data thật, không chọn theo cảm tính.

#### Candidate branch

```text
IF single-core OK:
    Evaluate Bremen / mariusmm / Nikos
IF multi-core required:
    Focus Bremen + backup DBT-RISE/TGC
    Loại mariusmm + Nikos khỏi path chính nếu single-core only
```

#### CPU candidates

| Candidate | Role | Expected use |
|---|---|---|
| Bremen RISC-V VP | PRIMARY | Linux/RTOS-capable candidate |
| mariusmm RISC-V-TLM | BACKUP/reference | Bare-metal/RTOS exploration, possible fallback |
| NikosMouzakitis/RISCV-SystemC | Learning/reference | Instruction decoder/datapath study |
| DBT-RISE/TGC | Optional backup | Consider if performance/SMP requirement requires DBT |

#### Benchmark requirements

Đo performance ở cả 3 mode:

```text
1. No quantum keeper       # worst case
2. Quantum keeper 1ms      # typical
3. Quantum keeper 10ms     # aggressive, may break timing if abused
```

Benchmark matrix cần có:

| CPU | Perf no-QK | Perf QK 1ms | Perf QK 10ms | RV32/64 | MMU | M/S/U | CLINT | PLIC | SMP | Activity | Notes |
|---|---:|---:|---:|---|---|---|---|---|---|---|---|
| Bremen | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | measured |
| mariusmm | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | measured |
| Nikos | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | TBD | measured |

#### Deliverable

- `docs/cpu_benchmark_results.md`
- Decision document chọn **primary + backup CPU**.
- `platforms/riscv_cpu_eval/` chạy được hello trên candidate được chọn.
- Kết quả benchmark có quantum keeper bật/tắt.

---

### Step 3 — CPU Abstraction + Custom SoC

**Mục tiêu**: platform không hardcode vào một CPU concrete class. Swap CPU backend qua CMake flag.

#### Tasks

1. Define `cpu_models/include/cdc/cpu/cpu_base.h`.
2. Wrap ít nhất 2 CPU candidates:
   - PRIMARY: `cpu_models/riscv_vp/`
   - BACKUP: `cpu_models/riscv_tlm/` hoặc `cpu_models/dbt_rise/`
3. Validate `cpu_base` interface với cả 2 wrapper.
4. Build `platforms/riscv_custom_soc/`:
   - PRIMARY CPU
   - custom `bus_router`
   - ROM/RAM
   - UART
   - timer
   - PLIC/CLINT policy rõ ràng
5. Swap CPU qua CMake:

```cmake
target_link_libraries(riscv_custom_soc PRIVATE
    cdc::cpu::${CDC_CPU_BACKEND})
# CDC_CPU_BACKEND ∈ {riscv_vp, riscv_tlm, dbt_rise}
```

#### Interrupt modeling policy

- `components/clint_tlm/` và `components/plic_tlm/` dùng cho custom SoC hoặc CPU backend chưa có interrupt platform đầy đủ.
- Bremen-based platform: ưu tiên dùng CLINT/PLIC integration có sẵn trong reference platform nếu wrapper giữ nguyên boundary đó.
- Nếu tách riêng CPU core khỏi reference platform, phải quyết định rõ CLINT/PLIC nằm ở wrapper hay standalone component.
- Không instantiate duplicate CLINT/PLIC trên cùng interrupt path.

#### Deliverable

- `cpu_base.h` stable interface.
- 2 wrapper functional: PRIMARY + BACKUP.
- `riscv_custom_soc` chạy bare-metal hello với PRIMARY.
- Smoke test BACKUP cũng chạy được hello.
- `docs/interrupt_modeling_policy.md`.

---

### Step 4 — RTOS/Linux Bring-up with Escalation Gates

**Mục tiêu**: bring-up theo layer, không nhảy thẳng vào Linux khi bare-metal/RTOS chưa ổn.

```text
GATE 1: Bare-metal hello
  ├─ PASS → tiếp Gate 2
  └─ FAIL → CPU model có issue cơ bản
            → STOP, escalate Step 2
            → KHÔNG thử Zephyr/Linux

GATE 2: Zephyr/FreeRTOS hello + thread + IRQ
  ├─ PASS → tiếp Gate 3
  └─ FAIL → IRQ/timer/MMU issue
            → STOP, debug CLINT/PLIC/timer model
            → KHÔNG thử Linux

GATE 3: Linux boot, timebox 2 tuần
  ├─ PASS → Step 4 complete
  ├─ FAIL after 2 weeks → fallback Zephyr là final deliverable
  └─ Fallback cũng fail → escalate meeting với sếp
                          → consider hybrid với QBox
                          → hoặc thử BACKUP CPU
```

#### Linux acceptance criteria

Linux boot = PASS khi tất cả điều kiện sau đạt:

```text
REQUIRED:
  ✓ Kernel print "Welcome to Linux" hoặc log boot equivalent
  ✓ Busybox shell prompt xuất hiện
  ✓ Commands work:
      - ls
      - cat
      - ps
      - mount
      - dmesg
  ✓ /proc/cpuinfo hiện đúng hoặc hợp lý với CPU backend
  ✓ /proc/meminfo hợp lý
  ✓ Boot time < 10 phút

NOT REQUIRED:
  ✗ Network stack
  ✗ Full distro, Ubuntu/Debian
  ✗ Package manager
  ✗ GUI/X11
  ✗ Boot time < 1 phút
```

#### Deliverable

- `platforms/riscv_linux_vp/`
- `fw/linux_minimal/` gồm kernel config, initramfs recipe, DTB notes.
- `docs/linux_boot_fallback_plan.md`
- Linux PASS report hoặc fallback Zephyr report.

---

### Step 5 — Custom IP TLM Modeling

**Mục tiêu**: tạo TLM behavioral model cho custom IP để software/driver có thể phát triển trước khi RTL ready.

#### Candidate custom IP

- DMA controller.
- Custom accelerator.
- Sensor controller.
- GPIO/controller variant.
- SoC-specific register blocks.
- Other `<ip_name>_tlm`.

#### Tasks

1. Identify custom IP cần model.
2. Collect register spec.
3. Implement TLM behavioral model matching register spec.
4. Integrate vào `riscv_custom_soc`.
5. Write bare-metal SW driver tests:
   - Register read/write.
   - Reset behavior.
   - IRQ flow.
   - Basic functional path.
6. Add pytest regression.

#### Deliverable

- Each custom IP có TLM model trong `components/<ip_name>_tlm/`.
- SW driver bare-metal test PASS cho mỗi IP.
- Documentation per IP:
  - Register map.
  - Reset state.
  - IRQ behavior.
  - Known limitations.

---

### Step 6 — RTL Co-sim

**Mục tiêu**: verify RTL behavior against TLM model. RTL co-sim không phải primary software development path.

#### Tasks

1. Build Verilator 5.x from source trên AlmaLinux 9.7, install vào `/opt/verilator-5.x`.
2. Clone/build `libsystemctlm-soc`.
3. Chọn 1 IP đơn giản test đầu tiên, ví dụ AXI4-Lite GPIO hoặc custom register block.
4. Verilator wrap:

```bash
verilator --sc --trace gpio_top.sv -CFLAGS ...
```

5. SystemC wrapper instantiate generated `Vgpio_top`.
6. Bridge TLM ↔ AXI bằng libsystemctlm-soc.
7. Plug vào `platforms/cosim_demo/`.
8. Compare register behavior với TLM model.

#### Verilator build sample on AlmaLinux 9.7

```bash
git clone https://github.com/verilator/verilator.git deps/verilator
cd deps/verilator
git checkout v5.026
autoconf
./configure --prefix=/opt/verilator-5.x
make -j$(nproc)
sudo make install
```

#### Deliverable

- `cosim_demo` chạy với 1 IP RTL thật.
- Register access đúng so với TLM model.
- VCD trace dump được.
- GTKWave hiển thị signal đúng.

---

## 5. Test Infrastructure

### pytest pattern

```python
import pytest
import pexpect
from pathlib import Path

@pytest.fixture
def vp_run():
    def _run(platform, config, fw, timeout=60):
        cmd = (
            f"./build/debug/platforms/{platform}/{platform} "
            f"-c {config} --fw {fw}"
        )
        return pexpect.spawn(cmd, timeout=timeout, encoding="utf-8")
    return _run

def test_riscv_custom_soc_hello(vp_run):
    p = vp_run(
        "riscv_custom_soc",
        "platforms/riscv_custom_soc/configs/default.yaml",
        "fw/hello_baremetal_riscv/hello.elf",
    )
    p.expect("Hello from")
```

### Test categories

| Category | Path | Purpose |
|---|---|---|
| Unit | `tests/unit/` | per-component register/model behavior |
| Integration | `tests/integration/` | CPU → bus → peripheral |
| System | `tests/system/` | full platform boot |
| Perf | `tests/perf/` | benchmark with/without quantum keeper |

### Regression targets

```bash
pytest tests/unit -v
pytest tests/integration -v
pytest tests/system -v
pytest tests/perf -v --junit-xml=out/perf_results.xml
```

Expected smoke suite:

```text
Unit + integration smoke: < 10 phút
System smoke:           < 30 phút
Perf/nightly:           can be longer
```

---

## 6. Integration Strategy

### CPU integration policy

Default:

```text
third_party upstream repo
+ thin wrapper in cpu_models/<backend>/
+ common cpu_base interface
```

Do not fork unless wrapper cannot solve required integration issue.

| Approach | Pros | Cons | Decision |
|---|---|---|---|
| Submodule, no touch | Easy upstream sync | Hard to patch internals | Default |
| Fork into `third_party/` | Full control | Rebase pain | Only if required |
| Thin wrapper | Clean separation | Small adapter overhead | Required |

Specific policy:

- Bremen RISC-V VP: prefer submodule + thin wrapper. Avoid deep fork unless absolutely required.
- mariusmm RISC-V-TLM: can fork if significant modification is needed.
- NikosMouzakitis/RISCV-SystemC: reference only, no deep integration initially.
- DBT-RISE/TGC: evaluate only if performance/SMP requirement needs DBT path.

### Interrupt modeling policy

Document in each platform README:

```text
Which CLINT/PLIC path is active?
Which IRQ wires go from peripheral to interrupt controller?
Which interrupt controller forwards to CPU?
Is there any built-in interrupt controller from CPU backend?
```

Avoid duplicate PLIC/CLINT instantiation.

### Performance expectation policy

Initial planning budget only; replace with measured data after Step 2.

| Workload | Native SystemC planning target |
|---|---:|
| Bare-metal hello | seconds |
| Zephyr/FreeRTOS hello | seconds to minutes |
| Linux minimal boot | < 10 minutes |
| 100-test regression | < 60 minutes |

Mitigation if performance is too low:

- Enable quantum keeper.
- Increase quantum carefully: 1ms typical, 10ms aggressive.
- Use DMI for memory.
- Compile Release with `-O3` and LTO.
- Disable verbose logging.
- Evaluate DBT-RISE/TGC or fallback hybrid QBox if needed.

---

## 7. Tips khi feed cho Claude Code

- Chia nhỏ session: mỗi step một session riêng.
- Không dump toàn bộ spec vào một prompt nếu chỉ đang làm Step 1.
- Sau mỗi step, commit + tag git:
  - `v0.1-mini-tlm`
  - `v0.2-cpu-eval`
  - `v0.3-custom-soc`
  - `v0.4-rtos-linux`
  - `v0.5-custom-ip-tlm`
  - `v0.6-cosim`
- Cung cấp reference links đúng scope:
  - SystemC/TLM examples.
  - Bremen RISC-V VP examples.
  - VCML examples nếu dùng VCML.
  - Verilator/libsystemctlm-soc nếu làm co-sim.
- Validation criteria phải rõ trước khi code.

### Pitfalls hay gặp

| Vấn đề | Giải pháp |
|---|---|
| CPU candidate API khác nhau | Bắt buộc đi qua `cpu_base` wrapper |
| CPU backend không support multi-core | Confirm multi-core ở Step 0 |
| Linux boot fail quá lâu | Timebox 2 tuần, fallback Zephyr hoặc backup CPU |
| Performance thấp | Quantum keeper + DMI + Release build |
| CLINT/PLIC double-wire | Document interrupt policy trong README platform |
| Upstream API thay đổi | Pin commit hash, không dùng floating `main` lâu dài |
| Artifact quá nặng | Dùng Git LFS/internal artifact server, không commit trực tiếp |

---

## 8. Recommended Starting Sequence

1. **Day 1-2**: Environment + repo skeleton + Step 0 decision docs.
2. **Week 1-2**: Step 1 — `mini_tlm` chạy Hello World, timer IRQ.
3. **Week 3-4**: Step 2 — CPU TLM evaluation + benchmark matrix.
4. **Month 2**: Step 3 — `cpu_base`, wrap 2 CPU candidates, custom SoC bare-metal.
5. **Month 3**: Step 4 — RTOS/Zephyr bring-up, then minimal Linux timeboxed.
6. **Month 4+**: Step 5 — custom IP TLM modeling.
7. **After Step 5 baseline**: Step 6 — RTL co-sim verification.

---

## References

- Accellera SystemC: https://github.com/accellera-official/systemc
- SystemC CCI: https://github.com/accellera-official/cci
- VCML: https://github.com/machineware-gmbh/vcml
- RISC-V VP (Uni Bremen): https://github.com/agra-uni-bremen/riscv-vp
- RISC-V-TLM: https://github.com/mariusmm/RISC-V-TLM
- RISCV-SystemC: https://github.com/NikosMouzakitis/RISCV-SystemC
- QBox fallback/hybrid: https://github.com/qualcomm/qbox
- libsystemctlm-soc: https://github.com/Xilinx/libsystemctlm-soc
- Verilator: https://github.com/verilator/verilator
