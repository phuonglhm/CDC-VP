// SPDX-License-Identifier: Apache-2.0

#include "elf_image.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

// For `vlen_bits()`. The image checks below compare what the image was promised
// against what the linked hart actually provides, so the number has to come
// from the hart rather than from a constant repeated here.
#include "riscv_vp_plusplus_wrapper.h"

namespace cdc::platforms::riscv_vpp_compiler_vp {

namespace {

// ELF constants. Spelled out rather than pulled from <elf.h>: this platform is
// packaged and rebuilt on machines whose libc headers are not this repository's
// business, and the eight values below have not changed since 1995.
constexpr unsigned char kElfClass32 = 1;
constexpr unsigned char kElfClass64 = 2;
constexpr unsigned char kElfData2Lsb = 1;
constexpr std::uint16_t kEtExec = 2;
constexpr std::uint16_t kEmRiscv = 243;
constexpr std::uint32_t kPtLoad = 1;
constexpr std::uint32_t kShtSymtab = 2;
constexpr std::uint32_t kShtStrtab = 3;

// e_flags, RISC-V psABI.
constexpr std::uint32_t kEfFloatAbiMask = 0x0006;
constexpr std::uint32_t kEfFloatAbiSoft = 0x0000;
constexpr std::uint32_t kEfFloatAbiSingle = 0x0002;
constexpr std::uint32_t kEfFloatAbiDouble = 0x0004;
constexpr std::uint32_t kEfFloatAbiQuad = 0x0006;
constexpr std::uint32_t kEfRvc = 0x0001;

std::string hex(std::uint64_t value, int width = 8)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(width) << std::setfill('0') << value;
    return out.str();
}

std::string float_abi_name(std::uint32_t flags)
{
    switch (flags & kEfFloatAbiMask) {
    case kEfFloatAbiSoft:
        return "soft-float (ilp32/lp64)";
    case kEfFloatAbiSingle:
        return "single-float (ilp32f/lp64f)";
    case kEfFloatAbiDouble:
        return "double-float (ilp32d/lp64d)";
    case kEfFloatAbiQuad:
    default:
        return "quad-float (ilp32q/lp64q)";
    }
}

class reader {
public:
    explicit reader(const std::vector<unsigned char>& bytes)
        : bytes_(bytes)
    {
    }

    /// Bounds-checked little-endian reads. `at()` on the vector would throw
    /// `std::out_of_range`, whose message says nothing about ELF; a truncated
    /// file is a normal thing to be handed and deserves a normal diagnostic.
    std::uint64_t read(std::size_t offset, unsigned width) const
    {
        if (offset + width > bytes_.size()) {
            throw std::runtime_error(
                "truncated ELF: a header field at offset " + std::to_string(offset)
                + " runs past the end of the " + std::to_string(bytes_.size())
                + "-byte file");
        }
        std::uint64_t value = 0;
        for (unsigned i = 0; i < width; ++i) {
            value |= static_cast<std::uint64_t>(bytes_[offset + i]) << (8 * i);
        }
        return value;
    }

    std::string read_string(std::size_t offset) const
    {
        std::string text;
        while (offset < bytes_.size() && bytes_[offset] != '\0') {
            text.push_back(static_cast<char>(bytes_[offset++]));
        }
        return text;
    }

    std::size_t size() const noexcept { return bytes_.size(); }
    const unsigned char* data() const noexcept { return bytes_.data(); }

private:
    const std::vector<unsigned char>& bytes_;
};

/// Parse `Tag_RISCV_arch` out of `.riscv.attributes`.
///
/// The format (RISC-V psABI, "ELF attributes") is a byte `'A'`, then per-vendor
/// subsections: `uint32 length`, NUL-terminated vendor name, then sub-subsections
/// of `uleb128 tag`, `uint32 size`, payload. Inside `Tag_File` (1) the payload is
/// a sequence of `uleb128 tag` followed by a value whose type is decided by the
/// tag's parity: odd tags carry a NUL-terminated string, even tags a ULEB128.
/// That rule is what makes an unknown tag skippable, and it is why tag 5
/// (`Tag_RISCV_arch`, the string this wants) can be found without a table of
/// every tag a future toolchain might emit.
///
/// Returning an empty string on anything unexpected is deliberate. A malformed
/// attributes section is not grounds to refuse an image whose program headers
/// are fine; it only means the checks that depend on the string cannot run, and
/// the report says "not declared" rather than inventing a value.
std::string parse_arch_attribute(const reader& file, std::size_t offset,
                                 std::size_t length)
{
    auto uleb = [&](std::size_t& pos) -> std::uint64_t {
        std::uint64_t value = 0;
        unsigned shift = 0;
        while (pos < offset + length) {
            const auto byte = static_cast<std::uint8_t>(file.data()[pos++]);
            value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) {
                break;
            }
            shift += 7;
            if (shift > 63) {
                return 0;
            }
        }
        return value;
    };

    if (length < 5 || offset + length > file.size()) {
        return {};
    }
    if (file.data()[offset] != 'A') {
        return {};
    }

    std::size_t pos = offset + 1;
    while (pos + 4 <= offset + length) {
        const std::size_t subsection_start = pos;
        const auto subsection_length = static_cast<std::size_t>(file.read(pos, 4));
        if (subsection_length < 5
            || subsection_start + subsection_length > offset + length) {
            return {};
        }
        pos += 4;

        const std::string vendor = file.read_string(pos);
        pos += vendor.size() + 1;

        if (vendor == "riscv") {
            while (pos < subsection_start + subsection_length) {
                const std::size_t tag_start = pos;
                const std::uint64_t tag = uleb(pos);
                if (tag != 1 /* Tag_File */) {
                    // Section- and symbol-scoped attribute blocks are not used
                    // by any toolchain this package targets, and skipping them
                    // correctly needs their own index lists. Stop rather than
                    // guess.
                    (void)tag_start;
                    return {};
                }
                const auto block_size = static_cast<std::size_t>(file.read(pos, 4));
                pos += 4;
                if (block_size < 5 || tag_start + block_size > offset + length) {
                    return {};
                }
                const std::size_t block_end = tag_start + block_size;

                while (pos < block_end) {
                    const std::uint64_t attribute = uleb(pos);
                    const bool is_string = (attribute % 2) == 1;
                    if (attribute == 5 /* Tag_RISCV_arch */) {
                        return file.read_string(pos);
                    }
                    if (is_string) {
                        pos += file.read_string(pos).size() + 1;
                    } else {
                        uleb(pos);
                    }
                }
                pos = block_end;
            }
        }
        pos = subsection_start + subsection_length;
    }
    return {};
}

} // namespace

bool elf_image::declares_vector() const
{
    if (architecture.empty()) {
        return false;
    }
    // `_v` as a separate extension, `v` immediately after the base ISA letters,
    // or any `zve*` subset. Substring-searching for a bare "v" would match
    // "zvl512b" and "rv32".
    if (architecture.find("_v") != std::string::npos
        || architecture.find("zve") != std::string::npos) {
        return true;
    }
    for (std::size_t i = 4; i < architecture.size(); ++i) {
        const char c = architecture[i];
        if (c == '_') {
            break;
        }
        if (c == 'v') {
            return true;
        }
    }
    return false;
}

unsigned elf_image::declared_zvl_bits() const
{
    // The *largest* `zvl<N>b`, not the first one found.
    //
    // `-march=rv32gcv_zvl512b` produces an attribute string listing every
    // implied subset, sorted alphabetically:
    //
    //     ..._zvl128b1p0_zvl256b1p0_zvl32b1p0_zvl512b1p0_zvl64b1p0
    //
    // so "the first zvl" is `zvl128b`, which says nothing about the image. Each
    // `zvl<N>b` guarantees VLEN is *at least* N, so what the image actually
    // requires is the maximum of them.
    unsigned largest = 0;
    std::size_t at = architecture.find("zvl");
    while (at != std::string::npos) {
        unsigned bits = 0;
        std::size_t i = at + 3;
        while (i < architecture.size()
               && std::isdigit(static_cast<unsigned char>(architecture[i])) != 0) {
            bits = bits * 10 + static_cast<unsigned>(architecture[i] - '0');
            ++i;
        }
        // `zvl512b`: the trailing 'b' is part of the name, not a multiplier.
        if (bits > largest && i < architecture.size() && architecture[i] == 'b') {
            largest = bits;
        }
        at = architecture.find("zvl", at + 3);
    }
    return largest;
}

bool elf_image::in_executable_segment(std::uint64_t address) const
{
    for (const auto& segment : segments) {
        if (!segment.executable() || segment.memsz == 0) {
            continue;
        }
        if (address >= segment.paddr && address - segment.paddr < segment.memsz) {
            return true;
        }
    }
    return false;
}

std::uint64_t elf_image::loaded_bytes() const
{
    std::uint64_t total = 0;
    for (const auto& segment : segments) {
        total += segment.filesz;
    }
    return total;
}

elf_image read_and_validate_elf(const std::string& path,
                                const memory_region& ram,
                                const memory_region& host_io)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open ELF image '" + path + "'");
    }
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
    const reader file(bytes);

    if (bytes.size() < 52) {
        throw std::runtime_error(
            "'" + path + "' is " + std::to_string(bytes.size())
            + " bytes, too short to be an ELF32 file (the header alone is 52)");
    }
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F') {
        throw std::runtime_error("'" + path + "' is not an ELF file (bad magic)");
    }

    if (bytes[4] == kElfClass64) {
        throw std::runtime_error(
            "'" + path
            + "' is an ELF64 image. This platform is a single RV32GCV hart; the "
              "frozen compiler contract is rv32gcv_zvl512b/ilp32d, so build with "
              "-march=rv32gcv_zvl512b -mabi=ilp32d.");
    }
    if (bytes[4] != kElfClass32) {
        throw std::runtime_error("'" + path + "' has an unknown ELF class "
                                 + std::to_string(bytes[4]));
    }
    if (bytes[5] != kElfData2Lsb) {
        throw std::runtime_error("'" + path
                                 + "' is big-endian; RISC-V images are little-endian");
    }

    const auto e_type = static_cast<std::uint16_t>(file.read(16, 2));
    const auto e_machine = static_cast<std::uint16_t>(file.read(18, 2));
    if (e_machine != kEmRiscv) {
        throw std::runtime_error("'" + path + "' targets machine "
                                 + std::to_string(e_machine)
                                 + ", not RISC-V (" + std::to_string(kEmRiscv) + ")");
    }
    if (e_type != kEtExec) {
        throw std::runtime_error(
            "'" + path + "' has e_type " + std::to_string(e_type)
            + "; this platform loads fully linked executables (ET_EXEC) only. A "
              "relocatable object or a position-independent executable has no "
              "fixed load address and nothing here relocates it.");
    }

    elf_image image;
    image.path = path;
    image.entry = file.read(24, 4);
    image.machine_flags = static_cast<std::uint32_t>(file.read(36, 4));

    if ((image.machine_flags & kEfFloatAbiMask) != kEfFloatAbiDouble) {
        throw std::runtime_error(
            "'" + path + "' declares the " + float_abi_name(image.machine_flags)
            + " ABI, but the frozen compiler contract for this package is ilp32d. "
              "The two pass floating-point arguments in different registers, so the "
              "image would run and produce wrong numbers. Rebuild with "
              "-mabi=ilp32d.");
    }
    (void)kEfRvc;

    // ── program headers ──────────────────────────────────────────────────────

    const auto phoff = static_cast<std::size_t>(file.read(28, 4));
    const auto phentsize = static_cast<std::size_t>(file.read(42, 2));
    const auto phnum = static_cast<std::size_t>(file.read(44, 2));

    for (std::size_t i = 0; i < phnum; ++i) {
        const std::size_t ph = phoff + i * phentsize;
        if (file.read(ph, 4) != kPtLoad) {
            continue;
        }
        elf_segment segment;
        segment.offset = file.read(ph + 4, 4);
        segment.vaddr = file.read(ph + 8, 4);
        segment.paddr = file.read(ph + 12, 4);
        segment.filesz = file.read(ph + 16, 4);
        segment.memsz = file.read(ph + 20, 4);
        segment.flags = static_cast<std::uint32_t>(file.read(ph + 24, 4));

        if (segment.memsz == 0) {
            continue;
        }
        if (segment.filesz > segment.memsz) {
            throw std::runtime_error(
                "'" + path + "': segment " + std::to_string(i) + " carries "
                + std::to_string(segment.filesz) + " bytes of file for "
                + std::to_string(segment.memsz) + " bytes of memory");
        }
        if (segment.offset + segment.filesz > bytes.size()) {
            throw std::runtime_error(
                "'" + path + "': segment " + std::to_string(i)
                + " runs past the end of the file (offset " + hex(segment.offset)
                + " + " + std::to_string(segment.filesz) + " > "
                + std::to_string(bytes.size()) + ")");
        }

        // The MMIO check comes first. An image that overlaps the host-I/O
        // window is also outside RAM, and "your segment is outside RAM" would
        // send the reader looking at the linker script's origin when the actual
        // mistake is that they linked over the console.
        if (host_io.overlaps(segment.paddr, segment.memsz)) {
            throw std::runtime_error(
                "'" + path + "': segment " + std::to_string(i) + " at "
                + hex(segment.paddr) + " + " + std::to_string(segment.memsz)
                + " bytes overlaps the simulator-only host-I/O window ["
                + hex(host_io.base) + ", " + hex(host_io.end())
                + "). That window is not memory: loading over it would silently "
                  "discard the segment and leave the console unreachable.");
        }
        if (!ram.contains(segment.paddr, segment.memsz)) {
            throw std::runtime_error(
                "'" + path + "': segment " + std::to_string(i) + " at "
                + hex(segment.paddr) + " + " + std::to_string(segment.memsz)
                + " bytes is outside the RAM window [" + hex(ram.base) + ", "
                + hex(ram.end()) + "). Link for " + hex(ram.base)
                + ", or raise --ram-size.");
        }

        for (const auto& existing : image.segments) {
            const bool overlap = segment.paddr < existing.paddr + existing.memsz
                                 && existing.paddr < segment.paddr + segment.memsz;
            if (overlap) {
                throw std::runtime_error(
                    "'" + path + "': segment " + std::to_string(i) + " at "
                    + hex(segment.paddr) + " overlaps an earlier segment at "
                    + hex(existing.paddr)
                    + ". Which one wins depends on load order, so neither does.");
            }
        }

        image.segments.push_back(segment);
    }

    if (image.segments.empty()) {
        throw std::runtime_error("'" + path
                                 + "' has no loadable segments; there is nothing to run");
    }
    if (!ram.contains(image.entry, 1)) {
        throw std::runtime_error("'" + path + "': the entry point " + hex(image.entry)
                                 + " is outside the RAM window [" + hex(ram.base)
                                 + ", " + hex(ram.end()) + ")");
    }

    // ── section headers: attributes and symbols ──────────────────────────────

    const auto shoff = static_cast<std::size_t>(file.read(32, 4));
    const auto shentsize = static_cast<std::size_t>(file.read(46, 2));
    const auto shnum = static_cast<std::size_t>(file.read(48, 2));
    const auto shstrndx = static_cast<std::size_t>(file.read(50, 2));

    if (shoff != 0 && shnum != 0 && shstrndx < shnum) {
        const std::size_t shstr = shoff + shstrndx * shentsize;
        const auto shstr_offset = static_cast<std::size_t>(file.read(shstr + 16, 4));

        for (std::size_t i = 0; i < shnum; ++i) {
            const std::size_t sh = shoff + i * shentsize;
            const auto name_offset = static_cast<std::size_t>(file.read(sh, 4));
            const auto type = static_cast<std::uint32_t>(file.read(sh + 4, 4));
            const auto offset = static_cast<std::size_t>(file.read(sh + 16, 4));
            const auto size = static_cast<std::size_t>(file.read(sh + 20, 4));
            const auto link = static_cast<std::size_t>(file.read(sh + 24, 4));
            const auto entsize = static_cast<std::size_t>(file.read(sh + 36, 4));
            const std::string name = file.read_string(shstr_offset + name_offset);

            if (name == ".riscv.attributes") {
                image.architecture = parse_arch_attribute(file, offset, size);
            } else if (type == kShtSymtab && entsize >= 16 && link < shnum) {
                const std::size_t strtab = shoff + link * shentsize;
                if (file.read(strtab + 4, 4) != kShtStrtab) {
                    continue;
                }
                const auto str_offset =
                    static_cast<std::size_t>(file.read(strtab + 16, 4));
                for (std::size_t s = 0; s + entsize <= size; s += entsize) {
                    const std::size_t sym = offset + s;
                    const auto sym_name_offset =
                        static_cast<std::size_t>(file.read(sym, 4));
                    if (sym_name_offset == 0) {
                        continue;
                    }
                    elf_symbol symbol;
                    symbol.value = file.read(sym + 4, 4);
                    symbol.size = file.read(sym + 8, 4);
                    image.symbols[file.read_string(str_offset + sym_name_offset)] =
                        symbol;
                }
            }
        }
    }

    if (!image.architecture.empty()) {
        if (image.architecture.rfind("rv32", 0) != 0) {
            throw std::runtime_error(
                "'" + path + "' declares architecture '" + image.architecture
                + "'; this platform is a single RV32 hart");
        }
        // Refused only when the image needs *more* than this hart has.
        //
        // `zvl<N>b` is a lower bound, and properly strip-mined RVV code is
        // VLEN-agnostic: an image built for `zvl256b` asks `vsetvli` how many
        // elements it may process and gets 512 bits' worth here, which is
        // correct. An image built for `zvl1024b` was allowed to assume at least
        // 1024 bits, and this hart cannot honour that — its loops may compute
        // the wrong answer with nothing to indicate it. Only the second case is
        // a defect, so only the second case is refused.
        const unsigned zvl = image.declared_zvl_bits();
        if (zvl > cdc::cpu::riscv_vp_plusplus_cpu::vlen_bits()) {
            throw std::runtime_error(
                "'" + path + "' declares architecture '" + image.architecture
                + "', which guarantees the image a minimum vector length of "
                + std::to_string(zvl) + " bits. This hart has VLEN="
                + std::to_string(cdc::cpu::riscv_vp_plusplus_cpu::vlen_bits())
                + ", so code that relied on the guarantee would quietly compute "
                  "the wrong answer. Rebuild with -march=rv32gcv_zvl512b.");
        }
    }

    return image;
}

} // namespace cdc::platforms::riscv_vpp_compiler_vp
