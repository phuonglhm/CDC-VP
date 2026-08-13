// SPDX-License-Identifier: Apache-2.0
//
// ELF inspection and validation for `riscv_vpp_compiler_vp`.
//
// ## Why this exists next to `cdc::cpu::load_elf`
//
// `cpu_models/include/cdc/cpu/elf_loader.h` already reads PT_LOAD segments and
// writes them through a TLM socket. It is the right tool for *loading* and it
// is deliberately minimal: it does not know what a valid address is, and it
// discovers an unmapped segment as a failed backdoor write with the message
// "address not mapped?".
//
// That message is acceptable for a fixed in-tree image. It is not acceptable
// for a package whose entire audience is people bringing their own ELF files:
// the plan requires that malformed, RV64, out-of-range and MMIO-overlapping
// images are "rejected with a non-zero host exit code and a useful diagnostic",
// and "useful" means naming the segment, its range and the window it missed.
//
// So this reads the file first and decides, and the loader then runs on an
// image already known to fit. The two do read the file twice; a package that
// starts once per run can afford it, and merging them would put a
// platform-specific address map inside a shared CPU-model header.
//
// ## What is checked, and what is deliberately not
//
// Refused, because the run would otherwise produce confidently wrong results:
//
//   * not an ELF, truncated, or a segment running past the end of the file;
//   * ELFCLASS64 — an RV64 image on an RV32 hart;
//   * a machine other than EM_RISCV, or big-endian data;
//   * `e_type` other than ET_EXEC (a PIE or relocatable object has no fixed
//     load address, and nothing here relocates);
//   * a float ABI other than `ilp32d`, read from `e_flags` — a soft-float or
//     single-float image passes arguments in different registers, and the
//     symptom is wrong numbers rather than a crash;
//   * a declared minimum vector length *above* 512 bits, read from
//     `.riscv.attributes` — the image was promised a guarantee this hart cannot
//     honour. A declared minimum *below* 512 is fine and is not refused:
//     properly strip-mined RVV code asks `vsetvli` how many elements it may
//     process, so it is VLEN-agnostic upwards;
//   * an architecture string that is not `rv32*`;
//   * a PT_LOAD segment outside RAM, overlapping the host-I/O window, or
//     overlapping another PT_LOAD segment;
//   * an entry point outside RAM.
//
// Accepted, with the value reported rather than judged:
//
//   * a scalar-only architecture string. Someone checking scalar code
//     generation has a legitimate reason to build without V, and refusing it
//     would make the package useless for half its job. The banner prints what
//     the image declared.
//   * a missing `.riscv.attributes` section. Hand-written assembly assembled
//     without it is still a valid image; only the checks that need the string
//     are skipped, and `--print-config` says so rather than implying they
//     passed.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cdc::platforms::riscv_vpp_compiler_vp {

/// One mapped window of the platform address map.
struct memory_region {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::string name;

    std::uint64_t end() const noexcept { return base + size; }

    /// Overflow-safe containment of `[address, address + length)`.
    bool contains(std::uint64_t address, std::uint64_t length) const noexcept
    {
        if (address < base) {
            return false;
        }
        const std::uint64_t offset = address - base;
        return offset <= size && length <= size - offset;
    }

    bool overlaps(std::uint64_t address, std::uint64_t length) const noexcept
    {
        if (length == 0 || size == 0) {
            return false;
        }
        return address < end() && base < address + length;
    }
};

struct elf_segment {
    std::uint64_t offset = 0;
    std::uint64_t vaddr = 0;
    std::uint64_t paddr = 0;
    std::uint64_t filesz = 0;
    std::uint64_t memsz = 0;
    std::uint32_t flags = 0;

    static constexpr std::uint32_t flag_x = 0x1;
    static constexpr std::uint32_t flag_w = 0x2;
    static constexpr std::uint32_t flag_r = 0x4;

    bool executable() const noexcept { return (flags & flag_x) != 0; }
};

struct elf_symbol {
    std::uint64_t value = 0;
    std::uint64_t size = 0;
};

struct elf_image {
    std::string path;
    std::uint64_t entry = 0;
    std::uint32_t machine_flags = 0;

    /// `Tag_RISCV_arch` from `.riscv.attributes`; empty when the section is
    /// absent, which is legal and is reported as "not declared".
    std::string architecture;

    std::vector<elf_segment> segments;
    std::map<std::string, elf_symbol> symbols;

    /// True when the architecture string declares a vector extension.
    bool declares_vector() const;

    /// The largest `zvl<N>b` in the architecture string — the minimum vector
    /// length the image was guaranteed — or 0 when the string names none.
    ///
    /// The largest, because a toolchain emits every implied subset
    /// (`zvl128b`, `zvl256b`, `zvl32b`, `zvl512b`, `zvl64b`, alphabetically),
    /// and each is a lower bound; the binding one is the highest.
    unsigned declared_zvl_bits() const;

    /// True when `address` falls inside a loadable executable segment. Used to
    /// separate instruction fetch from data traffic in the access counters;
    /// see `platform_top.h` for exactly how far that claim goes.
    bool in_executable_segment(std::uint64_t address) const;

    /// Total bytes the image will write into memory.
    std::uint64_t loaded_bytes() const;
};

/// Read `path`, check it against the platform map, and return what it says.
///
/// Throws `std::runtime_error` whose message names the specific problem. The
/// caller turns that into a non-zero host exit code; nothing here writes to
/// memory or touches SystemC.
elf_image read_and_validate_elf(const std::string& path,
                                const memory_region& ram,
                                const memory_region& host_io);

} // namespace cdc::platforms::riscv_vpp_compiler_vp
