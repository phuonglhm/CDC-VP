/* SPDX-License-Identifier: Apache-2.0
 *
 * The `riscv_vpp_compiler_vp` memory map and simulator-only host-I/O contract.
 *
 * ── one file, both sides ────────────────────────────────────────────────────
 *
 * Plan §16 Phase 4.5 requires the map to be "written once ... and shared with
 * firmware through one generated or common header". This is that header. The
 * platform includes it from C++; `crt0.S` and the examples include it from C
 * and from assembly. It therefore contains preprocessor definitions only — no
 * types, no C++, nothing an assembler cannot skip past.
 *
 * The alternative, duplicating the addresses into the firmware tree, is what
 * `rvv_runner` does for the Phase 2 harness, and it is defensible there: that
 * runner is a two-register probe and copying two constants keeps it free of the
 * firmware tree. It is not defensible for a handoff package whose whole purpose
 * is that an outside team writes images against this map.
 *
 * ── the window is simulator-only ────────────────────────────────────────────
 *
 * Nothing below is a TPU_V3 architectural peripheral. The host-I/O block exists
 * so a freestanding image can print and exit without a libc, a UART model or an
 * `ecall` handler; it must not appear in full-SoC firmware, and the TPU_V3
 * address map does not reserve it. It sits in the low unmapped space precisely
 * because that space is unmapped: `boot_rom` ends at 0x0001_0000 and
 * `global_control` at 0x0002_0000, and the next architectural region is
 * `global_ram` at 0x8000_0000 (`tpu_v3/address_map.h`).
 *
 * ── the exit protocol is Phase 2's, extended, not replaced ──────────────────
 *
 * Phase 4.5 says to reuse the Phase 2 exit protocol where practical and, if its
 * 0x000F_0000 window is retained, to extend that one contract rather than
 * inventing a second exit address. It is retained. The first four words below
 * are bit-for-bit the first four words of `fw/TPU_V3_SoC/rvv_smoke/sim_exit.h`,
 * at the same offsets, with the same trigger rule. Everything Phase 4.5 adds
 * lives at 0x400 and above, clear of the whole block that header defines
 * (its highest offset is 0x11C), so an image written for either contract can be
 * read by someone who knows the other.
 */

#ifndef CDC_VP_RISCV_VPP_COMPILER_VP_HOST_IO_MAP_H
#define CDC_VP_RISCV_VPP_COMPILER_VP_HOST_IO_MAP_H

/* No `u` suffixes anywhere below, and no casts. Both would be the natural thing
 * to write, and both would make this header unusable from `.S` — the assembler
 * evaluates `0x000F0000u` as a syntax error and `(uint32_t)` as garbage. Being
 * usable from assembly is the reason `crt0.S` can spell the exit protocol with
 * the same names the C code and the platform use, instead of taking the address
 * as a `-D` from a Makefile, which is where a third copy of the map would come
 * from. C promotes each literal below to `unsigned int` on its own, so nothing
 * is lost by leaving the suffix off. */

/* ── regions ─────────────────────────────────────────────────────────────────
 *
 * Two mapped regions and nothing else. Every other address is unmapped, and an
 * access to one is refused by the decoder, which VP++ turns into an access
 * fault chosen by access origin (decision record D13). That is the intended
 * behaviour, not a gap: an image that runs off its map must trap rather than
 * read zeros.
 */

/* Program and data RAM. The base is the TPU_V3 global-RAM base so that an
 * image built for this package links at an address the full SoC also maps;
 * only the size is configurable. */
#define COMPILER_VP_RAM_BASE 0x80000000

/* Default size. `--config`/`--ram-size` may lower or raise it; the shipped
 * linker script sizes its RAM region from the same constant, so an image built
 * with the shipped SDK always fits the default. */
#define COMPILER_VP_RAM_SIZE_DEFAULT 0x04000000 /* 64 MiB */

/* The simulator-only host-I/O window. */
#define COMPILER_VP_HOSTIO_BASE 0x000F0000
#define COMPILER_VP_HOSTIO_SIZE 0x00001000 /* 4 KiB */

/* ── block A: the Phase 2 exit protocol, unchanged ───────────────────────────
 *
 * `EXIT_KIND` is written **last** and is the trigger: the platform stops the
 * simulation when it sees a write there, by which point status, cause and
 * `mepc` are already in place.
 */
#define COMPILER_VP_EXIT_KIND (COMPILER_VP_HOSTIO_BASE + 0x000)
#define COMPILER_VP_EXIT_STATUS (COMPILER_VP_HOSTIO_BASE + 0x004)
#define COMPILER_VP_EXIT_MCAUSE (COMPILER_VP_HOSTIO_BASE + 0x008)
#define COMPILER_VP_EXIT_MEPC (COMPILER_VP_HOSTIO_BASE + 0x00C)

#define COMPILER_VP_EXIT_KIND_NORMAL 0
#define COMPILER_VP_EXIT_KIND_TRAPPED 1
#define COMPILER_VP_EXIT_PASS 0

/* 0x010 .. 0x3FF belong to the Phase 2 contract (`sim_exit.h`). This platform
 * implements none of them: they are fault injection, trap signatures, D12/D13
 * conformance and interrupt plumbing, none of which a compiler handoff needs.
 * They read zero and ignore writes rather than faulting, so a Phase 2 image
 * that strays into them fails its own checks instead of dying in a trap that
 * hides the reason. */

/* ── block B: console ────────────────────────────────────────────────────────
 *
 * One byte per write. A `puts` is therefore one TLM transaction per character,
 * which is slow and is the point: the gate requires host-I/O MMIO to be
 * observable on TLM like every other access.
 */
#define COMPILER_VP_CONSOLE_DATA (COMPILER_VP_HOSTIO_BASE + 0x400)
#define COMPILER_VP_CONSOLE_FLUSH (COMPILER_VP_HOSTIO_BASE + 0x404)

/* ── block C: machine identity, read-only ────────────────────────────────────
 *
 * What the *host* believes it built. Firmware reads these and compares them
 * against what the *hart* reports through its CSRs. A package whose two halves
 * disagreed about VLEN would otherwise print a convincing banner assembled from
 * two different machines.
 */
#define COMPILER_VP_ID_IDENTITY (COMPILER_VP_HOSTIO_BASE + 0x420)
#define COMPILER_VP_ID_ABI_VERSION (COMPILER_VP_HOSTIO_BASE + 0x424)
#define COMPILER_VP_ID_XLEN (COMPILER_VP_HOSTIO_BASE + 0x428)
#define COMPILER_VP_ID_HART_COUNT (COMPILER_VP_HOSTIO_BASE + 0x42C)
#define COMPILER_VP_ID_HART_ID (COMPILER_VP_HOSTIO_BASE + 0x430)
#define COMPILER_VP_ID_VLEN_BITS (COMPILER_VP_HOSTIO_BASE + 0x434)
#define COMPILER_VP_ID_ELEN_BITS (COMPILER_VP_HOSTIO_BASE + 0x438)
#define COMPILER_VP_ID_VLENB (COMPILER_VP_HOSTIO_BASE + 0x43C)
#define COMPILER_VP_ID_RAM_BASE (COMPILER_VP_HOSTIO_BASE + 0x440)
#define COMPILER_VP_ID_RAM_SIZE (COMPILER_VP_HOSTIO_BASE + 0x444)
#define COMPILER_VP_ID_HOSTIO_BASE (COMPILER_VP_HOSTIO_BASE + 0x448)
#define COMPILER_VP_ID_HOSTIO_SIZE (COMPILER_VP_HOSTIO_BASE + 0x44C)

/* 'V','P','P','C' little-endian. Proves the image is talking to this platform
 * and not to a probe memory that answers every read with zero. */
#define COMPILER_VP_IDENTITY_VALUE 0x43505056
#define COMPILER_VP_ABI_VERSION 1

/* ── block D: observation ────────────────────────────────────────────────────
 *
 * The gate asks for evidence that *vector* memory traffic reaches TLM, and
 * "the run made a lot of transactions" is not that: a scalar loop makes a lot
 * of transactions too.
 *
 * So the firmware brackets the section it wants measured. Writing a non-zero id
 * to `TRACE_MARK` opens a window and snapshots the RAM access counters; writing
 * zero closes it. `EXPECT_ACCESSES`, written while a window is open, states the
 * minimum number of RAM accesses that section must have caused — a number the
 * firmware derives from its own element count, not from anything the host told
 * it. The platform checks the closed window against it and fails the run if the
 * traffic did not happen, so the property is enforced by the simulator rather
 * than asserted by a test that reads a log.
 *
 * VP++ implements vector load/store as a per-element loop, one access each, so
 * a vector section's expected access count is a property the firmware can
 * compute exactly. See `riscv_vp_plusplus_wrapper.h`.
 */
#define COMPILER_VP_TRACE_MARK (COMPILER_VP_HOSTIO_BASE + 0x460)
#define COMPILER_VP_EXPECT_ACCESSES (COMPILER_VP_HOSTIO_BASE + 0x464)

#endif /* CDC_VP_RISCV_VPP_COMPILER_VP_HOST_IO_MAP_H */
