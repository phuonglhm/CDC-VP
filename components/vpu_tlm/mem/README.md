# MEM Block

## Overview

MEM is the shared memory subsystem of the VPU TLM model.

This block is the TLM equivalent of the `rtl/mem` folder in the original xk265 RTL repository. In RTL, the `mem` folder contains many SRAM/register-file wrappers for different encoder blocks. In this TLM model, those memories are abstracted into a reusable configurable memory model.

This is a **functional TLM memory model**, not a cycle-accurate SRAM model.

---

## Directory Structure

```text
mem/
├── include/
│   ├── mem.h
│   ├── mem_instance.h
│   ├── mem_types.h
│   └── xk265_mem_map.h
├── src/
│   ├── mem.cpp
│   ├── mem_instance.cpp
│   └── xk265_mem_map.cpp
├── test/
│   ├── CMakeLists.txt
│   └── test_mem.cpp
└── README.md
```

---

## Role in VPU Pipeline

The MEM block provides shared memory abstraction for VPU blocks.

```text
FETCH / PREI / POSI / IME / FME / REC / CABAC
        ↓
       MEM
        ↓
Configurable memory instances
```

In the current unit-test level, prediction blocks such as PREI, POSI, IME and FME mainly operate on frame-level data structures.

For higher-level VPU integration, MEM can be used to model internal RAMs/register files used by these blocks.

---

## Relationship with Original RTL

The original RTL has many memory modules such as:

```text
ram_sp_256x32
ram_sp_be_192x512
fetch_rf_1p_128x512
fetch_ram_2p_64x208
prei_md_ram_sp_85x6
posi_md_ram_sp_64x6
ime_mv_ram_sp_64x13
fme_mv_ram_dp_64x20
mc_mv_ram_sp_512x20
cabac_ram_sp_1024x8
```

Instead of writing one C++ class for each Verilog RAM module, this TLM model uses one generic memory class:

```text
mem_instance
```

Then each RTL memory is represented as a named memory configuration.

Example:

```cpp
mem_instance ime_mv_ram({
    "ime_mv_ram_sp_64x13",
    64,
    13,
    mem_port_kind::single_port,
    false,
    1,
    1,
    true
});
```

---

## Main Components

### 1. `mem_instance`

`mem_instance` models one configurable memory.

It supports:

```text
- configurable depth
- configurable width in bits
- non-byte-aligned width
- read
- write
- byte-enable write
- reset
- u64 helper read/write
- out-of-bound checking
- width mismatch checking
```

Example:

```cpp
mem_config config;
config.name = "ime_mv_ram_sp_64x13";
config.depth = 64;
config.width_bits = 13;

mem_instance ram(config);
```

---

### 2. `mem`

`mem` is a container for multiple named memory instances.

It supports:

```text
- add memory instance
- get memory by name
- read by memory name
- write by memory name
- byte-enable write by memory name
- reset all memories
- list memory names
```

Example:

```cpp
mem memory;

memory.add_instance({
    "prei_md_ram_sp_85x6",
    85,
    6,
    mem_port_kind::single_port,
    false,
    1,
    1,
    true
});
```

---

### 3. `xk265_mem_map`

`xk265_mem_map` creates default memory instances based on the original xk265 RTL memory naming.

Main APIs:

```cpp
std::vector<mem_config> make_xk265_default_mem_configs();

mem make_xk265_mem();
```

Example:

```cpp
mem memory = make_xk265_mem();

memory.has("ime_mv_ram_sp_64x13");
memory.has("fme_mv_ram_dp_64x20");
```

---

## Memory Types

The model supports several memory kinds:

```cpp
enum class mem_port_kind {
    single_port,
    simple_dual_port,
    true_dual_port,
    register_file
};
```

Meaning:

```text
single_port       one read/write access style
simple_dual_port  separate read/write style abstraction
true_dual_port    two-port RAM abstraction
register_file     register-file style memory
```

At the current functional level, these types are mainly used as configuration metadata. The model does not yet implement cycle-level port conflict behavior.

---

## Memory Status

Memory APIs return `mem_status`.

```cpp
enum class mem_status {
    ok,
    invalid_config,
    out_of_range,
    width_mismatch,
    byte_enable_unsupported,
    byte_enable_size_mismatch,
    instance_not_found
};
```

Meaning:

```text
ok                         operation succeeded
invalid_config             invalid memory configuration
out_of_range               address is outside memory depth
width_mismatch             data width does not match memory width
byte_enable_unsupported    byte-enable write used on non-BE memory
byte_enable_size_mismatch  byte-enable vector size is wrong
instance_not_found         memory name does not exist
```

---

## Data Width Handling

The model stores memory data as:

```cpp
std::vector<std::uint8_t>
```

This allows the model to support wide memories such as:

```text
128-bit
256-bit
512-bit
```

It also supports non-byte-aligned memories such as:

```text
ime_mv_ram_sp_64x13
fme_mv_ram_dp_64x20
prei_md_ram_sp_85x6
posi_md_ram_sp_64x6
```

For non-byte-aligned width, unused bits in the last byte are masked.

Example:

```text
13-bit memory
width_bytes = 2
valid bits  = 13
unused bits in last byte are cleared
```

---

## Byte-Enable Write

For memories with byte-enable support, partial write is supported.

Example:

```cpp
std::vector<std::uint8_t> data = {
    0x10, 0x20, 0x30, 0x40,
    0x50, 0x60, 0x70, 0x80
};

std::vector<bool> be = {
    false, true, false, true,
    false, true, false, true
};

ram.write_be(5, data, be);
```

Only enabled bytes are updated.

If byte-enable write is used on a memory that does not support byte-enable, the model returns:

```text
mem_status::byte_enable_unsupported
```

---

## Default xk265 Memory Map

The default memory map includes representative memories from the original xk265 RTL memory subsystem.

Current default instances include:

```text
ram_sp_256x32
ram_sp_1024x32
ram_sp_1536x32

ram_sp_be_128x64
ram_sp_be_192x128
ram_sp_be_192x512

fetch_rf_1p_128x512
fetch_rf_1p_64x256
fetch_ram_2p_64x208

prei_md_ram_sp_85x6
prei_ram_dp_16x32
posi_md_ram_sp_64x6

ime_mv_ram_sp_64x13
fme_mv_ram_dp_64x20

mc_mv_ram_sp_512x20
cabac_ram_sp_1024x8
tq_ram_sp_256x32
db_ram_sp_256x32
```

This list can be extended later when more RTL memory wrappers are mapped into the TLM model.

---

## Relationship with PREI / POSI / IME / FME

The prediction blocks do not need separate memory files.

Instead, they can use the shared MEM subsystem when integration requires memory-level behavior.

Example mapping:

```text
PREI  -> prei_md_ram_sp_85x6, prei_ram_dp_16x32
POSI  -> posi_md_ram_sp_64x6
IME   -> ime_mv_ram_sp_64x13
FME   -> fme_mv_ram_dp_64x20
FETCH -> fetch_rf_1p_128x512, fetch_ram_2p_64x208
```

This keeps memory modeling centralized and avoids duplicating memory code inside each block.

---

## Simplified Functional Flow

```text
Create memory config
      ↓
Create mem_instance
      ↓
Write data
      ↓
Read data
      ↓
Check status
```

For the full memory subsystem:

```text
Create xk265 MEM map
      ↓
Access memory by name
      ↓
Read/write memory word
      ↓
Return mem_status
```

---

## Main APIs

### `mem_instance`

```cpp
mem_instance(const mem_config& config);

mem_status read(std::uint32_t addr,
                std::vector<std::uint8_t>& data) const;

mem_status write(std::uint32_t addr,
                 const std::vector<std::uint8_t>& data);

mem_status write_be(std::uint32_t addr,
                    const std::vector<std::uint8_t>& data,
                    const std::vector<bool>& byte_enable);

mem_status read_u64(std::uint32_t addr,
                    std::uint64_t& value) const;

mem_status write_u64(std::uint32_t addr,
                     std::uint64_t value);

mem_status reset();
```

### `mem`

```cpp
bool add_instance(const mem_config& config);

bool has(const std::string& name) const;

mem_instance* get(const std::string& name);

mem_status read(const std::string& name,
                std::uint32_t addr,
                std::vector<std::uint8_t>& data) const;

mem_status write(const std::string& name,
                 std::uint32_t addr,
                 const std::vector<std::uint8_t>& data);

mem_status write_be(const std::string& name,
                    std::uint32_t addr,
                    const std::vector<std::uint8_t>& data,
                    const std::vector<bool>& byte_enable);

mem_status reset_all();
```

---

## Test Description

Test file:

```text
mem/test/test_mem.cpp
```

The MEM test is a full functional unit test for the memory subsystem.

It verifies:

```text
- basic read/write
- zero value for unwritten memory
- out-of-bound access
- width mismatch handling
- non-byte-aligned memory width
- byte-enable write
- byte-enable unsupported case
- u64 helper read/write
- memory container access by name
- xk265 default memory map
- reset_all behavior
```

---

## Test Cases

### 1. Basic Read/Write

Test name:

```text
[TEST] MEM basic read/write
```

Purpose:

```text
- Verify normal write and read operation
- Check depth and width metadata
```

Main checks:

```text
- memory is valid
- depth is correct
- width_bits is correct
- width_bytes is correct
- read data equals written data
```

---

### 2. Unwritten Read Returns Zero

Test name:

```text
[TEST] MEM unwritten read returns zero
```

Purpose:

```text
- Verify newly created memory is initialized to zero
```

Expected behavior:

```text
read_data == 0
```

---

### 3. Out-of-Bound Access

Test name:

```text
[TEST] MEM out-of-bound access
```

Purpose:

```text
- Verify read/write outside memory depth is rejected
```

Expected behavior:

```text
mem_status::out_of_range
```

---

### 4. Width Mismatch

Test name:

```text
[TEST] MEM width mismatch
```

Purpose:

```text
- Verify write data size must match memory word width
```

Expected behavior:

```text
mem_status::width_mismatch
```

---

### 5. Non-Byte-Aligned Width

Test name:

```text
[TEST] MEM non-byte-aligned width
```

Purpose:

```text
- Verify memories with bit widths not divisible by 8 are handled correctly
```

Example:

```text
ime_mv_ram_sp_64x13
width_bits  = 13
width_bytes = 2
```

Expected behavior:

```text
unused bits in the last byte are masked
```

---

### 6. Byte-Enable Write

Test name:

```text
[TEST] MEM byte-enable write
```

Purpose:

```text
- Verify partial byte update behavior
- Only enabled bytes are overwritten
```

Expected behavior:

```text
disabled bytes keep old value
enabled bytes take new value
```

---

### 7. Byte-Enable Unsupported

Test name:

```text
[TEST] MEM byte-enable unsupported
```

Purpose:

```text
- Verify byte-enable write is rejected on memories without byte-enable support
```

Expected behavior:

```text
mem_status::byte_enable_unsupported
```

---

### 8. u64 Helpers

Test name:

```text
[TEST] MEM u64 helpers
```

Purpose:

```text
- Verify simple 64-bit read/write helper APIs
```

Expected behavior:

```text
read_u64 returns the same value written by write_u64
```

---

### 9. MEM Subsystem Container

Test name:

```text
[TEST] MEM subsystem container
```

Purpose:

```text
- Verify memory instances can be added and accessed by name
```

Main checks:

```text
- add_instance works
- has(name) works
- read/write by name works
- missing instance returns instance_not_found
```

---

### 10. xk265 Default Memory Map

Test name:

```text
[TEST] MEM xk265 default memory map
```

Purpose:

```text
- Verify default xk265 memory instances are created
```

Main checks:

```text
- prei_md_ram_sp_85x6 exists
- posi_md_ram_sp_64x6 exists
- ime_mv_ram_sp_64x13 exists
- fme_mv_ram_dp_64x20 exists
- fetch_rf_1p_128x512 exists
- ram_sp_be_192x512 exists
```

---

### 11. Reset All

Test name:

```text
[TEST] MEM reset all
```

Purpose:

```text
- Verify all memory instances can be cleared
```

Expected behavior:

```text
data written before reset becomes zero after reset_all()
```

---

## Test Summary

The full functional MEM test verifies:

```text
- memory configuration
- memory instance validity
- read/write correctness
- zero initialization
- address boundary checking
- width checking
- non-byte-aligned RAM behavior
- byte-enable write behavior
- named memory container behavior
- xk265 memory map creation
- reset behavior
```

This test validates the MEM block as a reusable functional memory subsystem for higher-level VPU integration.

---

## Build and Run Test

From repository root:

```bash
cd ~/CDC-VP
cmake --build build --target test_mem
```

Run:

```bash
./build/components/vpu_tlm/mem/test/test_mem
```

Expected result:

```text
MEM full functional test PASSED
```

---

## Notes

The MEM block is currently modeled at functional level.

It does not model:

```text
- exact SRAM macro behavior
- clock-by-clock timing
- read latency
- write latency
- same-cycle read/write conflicts
- true dual-port conflict resolution
- valid/ready handshakes
- physical SRAM banking
```

These details can be added later if cycle-level or RTL-equivalent behavior is required.

At the current stage, MEM provides a clean and reusable abstraction for storing and accessing VPU internal data.
