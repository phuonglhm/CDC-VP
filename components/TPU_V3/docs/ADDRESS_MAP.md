# TPU_V3 Address Map

Status: **frozen for Phase 1**. Region *layout* and *strides* below are the
architectural contract. Two *capacities* remain configurable and are marked as
such; changing a capacity may not change any base address.

The single source of truth is `components/TPU_V3/common/include/tpu_v3/address_map.h`.
This document explains it; the header defines it, and
`components/TPU_V3/common/tests/test_address_map.cpp` proves the properties
claimed here. If the two ever disagree, the header and its test win and this
file is the bug.

---

## 1. Rules this map satisfies

From plan §12:

1. every RV32-visible physical address is below 4 GiB — the map ends exactly at
   `0x1_0000_0000`, and nothing is placed above it;
2. every region base and size is a power of two and naturally aligned;
3. SVM capacity fits inside its core aperture (see §5);
4. no two chip or core apertures overlap — enumerated and proven by test;
5. address arithmetic is done in `std::uint64_t` and checked for overflow even
   though the target is RV32;
6. MMIO supported widths and alignment are stated in §6;
7. memory-like and MMIO targets are distinguished, because
   `noc_interconnect::target_kind` needs that distinction to decide whether a
   widened AXI read is safe (§7);
8. a resource has exactly one address, seen identically from the local core,
   the sibling core, a remote chip and the host (§4);
9. firmware headers are generated from the C++ map, not written twice (§8);
10. a unit test enumerates every region and proves non-overlap.

## 2. Top level

| Base | End (inclusive) | Size | Region | Kind |
| --- | --- | --- | --- | --- |
| `0x0000_0000` | `0x0000_FFFF` | 64 KiB | `GLOBAL_BOOT_ROM` | memory |
| `0x0001_0000` | `0x0001_FFFF` | 64 KiB | `GLOBAL_CONTROL` | mmio |
| `0x0002_0000` | `0x7FFF_FFFF` | ~2 GiB | *unmapped* | — |
| `0x8000_0000` | `0xBFFF_FFFF` | 1 GiB | `GLOBAL_RAM_OR_HBM` | memory |
| `0xC000_0000` | `0xFFFF_FFFF` | 1 GiB | chip apertures, 8 × 128 MiB | mixed |

The reset PC is `0x0000_0000`, the base of `GLOBAL_BOOT_ROM`. It is a static
property of each CPU instance, carried in `cdc::cpu::cpu_config` (decision
record D5).

`GLOBAL_RAM_OR_HBM` is a 1 GiB window. The instantiated capacity is a
configuration value and may be smaller; see §5 for what that means.

The large unmapped hole is deliberate. It gives `GLOBAL_RAM_OR_HBM` a
`0x8000_0000` base — the conventional RISC-V DRAM base, which keeps linker
scripts unsurprising — and leaves room to grow the low peripheral area without
moving anything.

## 3. Chip aperture

```text
CHIP_APERTURE_BASE   = 0xC000_0000
CHIP_APERTURE_STRIDE = 0x0800_0000   (128 MiB)
chip_base(chip_id)   = 0xC000_0000 + chip_id * 0x0800_0000
```

`chip_id` is the linear chip index, `0..MAX_CHIPS-1`, with `MAX_CHIPS = 8`.
Eight chips fill the region exactly: `0xC000_0000 + 8 * 0x0800_0000` =
`0x1_0000_0000`, the top of the RV32 space.

`MAX_CHIPS = 8` is not a layout choice, it is forced by the NoC: the frozen
FlooNoC chimney manager ID is 3 bits, so `noc_interconnect` accepts at most 8
upstream initiators, and plan §4.4 gives each chip exactly one aggregated
initiator. Raising it is a NoC protocol change (see `TPU_V3_PHASE0_AUDIT.md`
§5).

Inside one chip aperture, offsets from `chip_base`:

| Offset | Size | Region | Kind |
| --- | --- | --- | --- |
| `0x0000_0000` | 32 MiB | `CORE_APERTURE(0)` | mixed |
| `0x0200_0000` | 32 MiB | `CORE_APERTURE(1)` | mixed |
| `0x0400_0000` | 64 KiB | `CHIP_CONTROL` | mmio |
| `0x0401_0000` | 64 KiB | `CHIP_COUNTERS` | mmio |
| `0x0402_0000` | — | reserved to `0x07FF_FFFF` | — |

## 4. Core aperture

```text
CORE_APERTURE_STRIDE = 0x0200_0000   (32 MiB)
core_base(chip_id, core_id) = chip_base(chip_id) + core_id * 0x0200_0000
```

`core_id` is 0 or 1 — plan §4.1 freezes two cores per chip.

Offsets from `core_base`:

| Offset | Size | Region | Kind |
| --- | --- | --- | --- |
| `0x0000_0000` | 16 MiB window | `SVM` | memory (see §5) |
| `0x0100_0000` | 64 KiB | `CORE_CONTROL` | mmio |
| `0x0101_0000` | 64 KiB | `MXU0_CONTROL` | mmio |
| `0x0102_0000` | 64 KiB | `MXU1_CONTROL` | mmio |
| `0x0103_0000` | 64 KiB | `CORE_COUNTERS` | mmio |
| `0x0104_0000` | — | reserved to `0x01FF_FFFF` | — |

**There is one address per resource.** A core reaching its own SVM, the sibling
core in the same chip reaching it, a remote chip reaching it, and the host
loader reaching it all use the same `core_base(chip, core) + offset`. There is
no separate "local alias" window. What differs is the *path*: the core-local
fabric recognises its own core aperture and does not leave the core, the
chip-local fabric recognises the sibling aperture and does not leave the chip,
and only an address outside the chip aperture reaches the NoC endpoint. That is
the plan §11.5/§11.8 local-bypass requirement expressed as a decode rule rather
than as a second address.

## 5. Window versus capacity

Two memory regions have a window larger than the storage that may sit behind
it: SVM (16 MiB window) and global RAM (1 GiB window).

**The window always decodes. The capacity is what is backed.** These are
separate properties and the code keeps them in separate fields — `region::size`
is the decoded extent, `region::capacity` is the backed extent.

The rule, in full:

1. the full window is registered with the address decoder and, where
   applicable, with `noc_interconnect::add_target`;
2. an access inside the window and inside the capacity is served normally;
3. an access inside the window but **above** the capacity is a decode hit with
   an error response from the target — it must **never** alias down into valid
   storage;
4. an access outside the window does not decode at all.

Rule 3 is the whole reason the two are separate. If the map shrank to the
instantiated capacity, an address just past the SVM would be *unmapped* in a
bring-up configuration and *valid* in the reference one, so the same firmware
pointer bug would produce a decode error on one run and silent data corruption
on another. The decoded map must not depend on how much memory was
instantiated, and `test_address_map.cpp::decoded_extent_does_not_depend_on_capacity`
proves it does not.

### SVM capacity

| | |
| --- | --- |
| window | 16 MiB (frozen layout) |
| reference capacity | **16 MiB** — the full window (decision record D6) |
| permitted range | 4 KiB … 16 MiB, power of two |

The reference configuration instantiates the whole window. Smaller capacities
remain available for explicitly labelled bring-up or stress runs, and the
platform report labels them as such; they are not the TPU_V3 reference result.

D6 supersedes the temporary 4 MiB of Phase 0 decision P0-6.

Host cost, once Phase 3 gives SVM real storage: the largest configuration
(8 chips) is 16 SVMs × 16 MiB = 256 MiB, plus global RAM.

### Global RAM capacity

| | |
| --- | --- |
| window | 1 GiB (frozen layout) |
| bring-up default | 256 MiB (decision record D6) |
| permitted range | 1 MiB … 1 GiB, power of two |

This is simulated backing memory. It is not a model of TPU v3 HBM capacity or
bandwidth and must not be described as one.

## 6. MMIO access rules

Every `*_CONTROL` and `*_COUNTERS` region is a 32-bit register file:

* supported widths: **4 bytes only**;
* alignment: naturally aligned to 4 bytes;
* a 1, 2 or 8 byte access, or a misaligned 4-byte access, returns
  `TLM_BURST_ERROR_RESPONSE` — it is never silently widened, narrowed or split;
* byte enables must be all-ones over the four bytes; a partial-strobe MMIO
  write is refused;
* reads of unimplemented registers inside a mapped region return 0 with
  `TLM_OK_RESPONSE`; writes to them are dropped. Unimplemented is a defined
  state, not an error, so that a firmware register sweep does not have to know
  the implementation status of every offset.

Memory-like regions (`GLOBAL_BOOT_ROM`, `GLOBAL_RAM_OR_HBM`, `SVM`) accept 1,
2, 4, 8 and vector-sized (up to 64-byte) accesses with arbitrary byte enables.
`GLOBAL_BOOT_ROM` refuses writes from the simulated fabric; the host loader
writes it through the debug transport.

## 7. NoC target kinds

`noc_interconnect::add_target` takes a `target_kind` because AXI reads whole
beats, so a read that is not bus-aligned fetches neighbouring bytes too. That
is harmless for RAM and unacceptable for MMIO, and the interconnect cannot tell
them apart.

| Region | `target_kind` |
| --- | --- |
| `GLOBAL_BOOT_ROM` | `memory` |
| `GLOBAL_RAM_OR_HBM` | `memory` |
| `GLOBAL_CONTROL` | `mmio` |
| chip aperture (whole 128 MiB, when a chip is mapped as a NoC target) | `mmio` |

The chip aperture is declared `mmio` even though most of it is SVM. It is the
conservative answer: the aperture contains real MMIO, the interconnect maps a
region not a mixture, and an `mmio` declaration causes a widened read to be
*refused* rather than silently satisfied from a neighbouring register. The cost
is that remote vector-width reads of a chip's SVM must be bus-aligned. If that
becomes a real limitation, the fix is to map SVM as a separate `memory`
sub-region — a map change, made deliberately, not a `target_kind` downgrade.

## 8. Firmware view

`tpu_v3/address_map.h` is the source. Firmware must not carry a second copy of
these constants. The Phase 10 firmware build generates
`fw/TPU_V3_SoC/common/include/tpu_v3_address_map.h` from the C++ header and a
test compares them, so a divergence is a build failure rather than a runtime
mystery. Until Phase 10 exists, no firmware header exists either — which is the
correct state, not an omission.

## 9. Capacity arithmetic

Worst case, all eight chips populated, SVM at its 16 MiB reference capacity:

```text
chip apertures      8 * 128 MiB = 1024 MiB
global RAM window                 1024 MiB
low peripherals                    128 KiB
------------------------------------------
total mapped                     ~2048 MiB   of 4096 MiB
```

Half the RV32 space is still unmapped, so plan risk R8 (address-space
exhaustion) is not live at `MAX_CHIPS = 8`. It becomes live the moment the NoC
manager-ID width is raised: 16 chips at a 128 MiB stride needs 2 GiB of
aperture and forces `GLOBAL_RAM_OR_HBM` to shrink or move. Any manager-ID
extension must therefore revisit this map in the same change.
