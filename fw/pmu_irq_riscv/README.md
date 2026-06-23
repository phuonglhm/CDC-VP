# CDC-VP PMU Interrupt Demo

This demo verifies the PMU/PWRMGR TLM peripheral inside a small RISC-V virtual platform.

The test uses the Bremen `riscv-vp` CPU backend and open-source Accellera SystemC 2.3.4.

The verified path is:

```text
RISC-V firmware
  -> CPU TLM initiator
  -> bus_router
  -> PMU MMIO registers
  -> PMU wakeup_irq output
  -> PLIC source 1
  -> CPU machine external interrupt
  -> firmware trap handler
```

## Folder Structure

```text
fw/pmu_irq_riscv/
├── src/
│   └── main.c
├── .gitignore
├── Makefile
├── README.md
├── linker.ld
└── startup.S
```

## One-Time Setup

From the repository root:

```bash
cd ~/CDC-VP

./tools/third_party/setup_third_party.sh
source ./tools/third_party/setup_env.sh
```

`setup_third_party.sh` clones the external CPU model repositories into `third_party/`, including Bremen `riscv-vp`.

`setup_env.sh` prepares the shell environment for this repo. It selects the host compilers and adds the RISC-V bare-metal toolchain to `PATH`.

If the toolchain is already installed, make sure this path is available:

```bash
export PATH=/opt/toolchains/riscv-none-elf/bin:/usr/bin:/bin:$PATH
```

## Build Firmware

From the repository root:

```bash
cd ~/CDC-VP

make -C fw/pmu_irq_riscv
```

This produces:

```text
fw/pmu_irq_riscv/pmu_irq.elf
fw/pmu_irq_riscv/pmu_irq.dis
```

To clean:

```bash
make -C fw/pmu_irq_riscv clean
```

## Configure Platform Build

From the repository root:

```bash
cd ~/CDC-VP

cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
```

## Build PMU Platform

```bash
cmake --build build/bremen --target pmu_platform -j
```

## Run Simulation

```bash
./build/bremen/platforms/tests/pmu_platform/pmu_platform \
  -c platforms/tests/pmu_platform/configs/default.yaml \
  --fw fw/pmu_irq_riscv/pmu_irq.elf \
  --sim-ms 5
```

## What The Platform Contains

The `pmu_platform` executable is a SystemC virtual platform containing:

```text
Bremen RISC-V CPU
RAM
UART
CLINT
PLIC
PMU/PWRMGR TLM model
bus_router
```

## Platform Memory Map

| Region |          Base | Purpose                         |
| ------ | ------------: | ------------------------------- |
| RAM    | `0x8000_0000` | Firmware code/data/stack        |
| UART   | `0x1000_0000` | Firmware console output         |
| CLINT  | `0x0200_0000` | Local timer/software interrupts |
| PLIC   | `0x0C00_0000` | External interrupt controller   |
| PMU    | `0x1007_0000` | PMU/PWRMGR MMIO register window |

## PMU Register Map Used By Firmware

| Register          |       Address | Purpose                           |
| ----------------- | ------------: | --------------------------------- |
| `PMU_INTR_STATE`  | `0x1007_0000` | PMU interrupt state               |
| `PMU_INTR_ENABLE` | `0x1007_0004` | PMU interrupt enable              |
| `PMU_INTR_TEST`   | `0x1007_0008` | Software-triggered interrupt test |
| `PMU_CONTROL`     | `0x1007_0014` | PMU control register              |
| `PMU_WAKEUP_EN`   | `0x1007_0020` | Wakeup source enable              |
| `PMU_WAKE_STATUS` | `0x1007_0024` | Wakeup status                     |

## IRQ Map

| Source | Signal           | Destination               |
| -----: | ---------------- | ------------------------- |
|      1 | `pmu.wakeup_irq` | PLIC source 1 -> CPU MEIP |

## What The Firmware Does

`fw/pmu_irq_riscv/src/main.c` runs on the simulated RISC-V CPU.

It performs these steps:

1. Prints `PMU platform start`.
2. Sets the machine trap vector using `mtvec`.
3. Configures PLIC source 1 for the PMU interrupt.
4. Enables machine external interrupt through `mie.MEIE`.
5. Enables global machine interrupt through `mstatus.MIE`.
6. Reads the PMU `CONTROL` register and checks the reset value.
7. Writes `0x1` to `PMU_WAKEUP_EN`.
8. Reads back `PMU_WAKEUP_EN`.
9. Enables PMU interrupt through `PMU_INTR_ENABLE`.
10. Triggers a PMU interrupt by writing `PMU_INTR_TEST`.
11. Waits for interrupt using `wfi`.
12. In the trap handler:

    * claims PLIC source 1,
    * clears `PMU_INTR_STATE`,
    * completes the interrupt by writing back to `PLIC_CLAIM`.
13. Prints `PMU PASS` if the register and interrupt behavior is valid.

## Expected Successful Output

A successful run should include output similar to:

```text
PMU platform start
READ  PMU_CONTROL [0x10070014] => 0x00000180
WRITE PMU_WAKEUP_EN [0x10070020] <= 0x00000001
READ  PMU_WAKEUP_EN [0x10070020] => 0x00000001
WRITE PMU_INTR_ENABLE [0x10070004] <= 0x00000001
WRITE PMU_INTR_TEST [0x10070008] <= 0x00000001
PMU IRQ
PMU PASS
```

## Notes

Use open-source Accellera SystemC 2.3.4 for this demo:

```text
/opt/systemc-2.3.4
```

Do not use the Arm Fast Models SystemC library for this build.

Also avoid SystemC 3.x for the Bremen backend in this repo because the pinned Bremen `riscv-vp` code uses older SystemC process macros.

If `pmu_platform` is not found, make sure `platforms/CMakeLists.txt` contains:

```cmake
if(TARGET cdc::components::pmu_tlm)
   add_subdirectory(tests/pmu_platform)
else()
   message(STATUS "Skipping pmu_platform: cdc::components::pmu_tlm is not available")
endif()
```

If `cdc::components::pmu_tlm` is not available, make sure `components/CMakeLists.txt` contains:

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pmu_tlm/CMakeLists.txt")
   add_subdirectory(pmu_tlm)
endif()
```
