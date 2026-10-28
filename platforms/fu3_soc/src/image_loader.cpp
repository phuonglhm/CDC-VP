#include "image_loader.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>

#include "fu3_soc_top.h"

namespace cdc::platforms::fu3_soc {

namespace {

enum class kind { memory, flash, refused };
enum class owner { none, ibex, cva6 };

struct region {
    const char* name;
    std::uint64_t begin, end;   // [begin, end)
    kind k;
    const char* target;          // memory name, or why it is refused
    std::uint64_t target_base;   // system address of target offset 0
    owner cpu;
};

constexpr region kRegions[] = {
    {"ROM", 0x0000'0000, 0x0000'1000, kind::memory, "ROM", 0x0000'0000, owner::ibex},
    {"CVA6_ISRAM", 0x0001'0000, 0x0004'0000, kind::memory, "CVA6_ISRAM", 0x0001'0000, owner::cva6},
    {"CVA6 debug partition", 0x0004'0000, 0x0004'1000, kind::refused,
     "debug code is written by the Boot ROM during staging (FU3_VP_ABI_1 section 6)", 0, owner::none},
    {"IBEX_ISRAM", 0x0005'0000, 0x0006'0000, kind::memory, "IBEX_ISRAM", 0x0005'0000, owner::ibex},
    {"Ibex debug partition", 0x0006'0000, 0x0006'1000, kind::refused,
     "debug code is written by the Boot ROM during staging (FU3_VP_ABI_1 section 6)", 0, owner::none},
    {"DSRAM", 0x0100'0000, 0x0104'0000, kind::memory, "DSRAM", 0x0100'0000, owner::none},
    {"QSPI_XIP", 0x3000'0000, 0x3100'0000, kind::flash, "QSPI NOR", 0x3000'0000, owner::none},
    {"HYPERBUS", 0x6000'0000, 0x6200'0000, kind::refused,
     "HyperRAM needs its controller initialised; load it from firmware", 0, owner::none},
};

std::string hex(std::uint64_t v)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return out.str();
}

std::vector<std::uint8_t> read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw image_error("cannot read '" + path + "'");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool is_elf(const std::vector<std::uint8_t>& f)
{
    return f.size() >= 4 && f[0] == 0x7f && f[1] == 'E' && f[2] == 'L' && f[3] == 'F';
}

std::uint32_t u32(const std::vector<std::uint8_t>& f, std::uint64_t at)
{
    if (at + 4 > f.size())
        throw image_error("truncated ELF");
    return f[at] | f[at + 1] << 8 | f[at + 2] << 16 | std::uint32_t(f[at + 3]) << 24;
}
std::uint16_t u16(const std::vector<std::uint8_t>& f, std::uint64_t at)
{
    if (at + 2 > f.size())
        throw image_error("truncated ELF");
    return static_cast<std::uint16_t>(f[at] | f[at + 1] << 8);
}

const region& locate(std::uint64_t begin, std::uint64_t end, const std::string& what)
{
    // A refused region is reported first, also for a range that starts in a loadable one.
    for (const auto& r : kRegions)
        if (r.k == kind::refused && begin < r.end && end > r.begin)
            throw image_error(what + " [" + hex(begin) + ", " + hex(end) + ") touches the " + r.name + ": " +
                              r.target);
    for (const auto& r : kRegions) {
        if (begin >= r.end || end <= r.begin)
            continue;
        if (begin < r.begin || end > r.end)
            throw image_error(what + " [" + hex(begin) + ", " + hex(end) + ") crosses the " + r.name +
                              " boundary [" + hex(r.begin) + ", " + hex(r.end) + ")");
        return r;
    }
    throw image_error(what + " [" + hex(begin) + ", " + hex(end) +
                      ") is outside every preload region (ROM, CVA6_ISRAM/IBEX_ISRAM images, DSRAM, QSPI XIP)");
}

} // namespace

std::string image_loader::arch_of(const std::vector<std::uint8_t>& f)
{
    // Section headers: find ".riscv.attributes" (SHT_RISCV_ATTRIBUTES 0x70000003).
    // Absent attributes are accepted (the ELF flags are checked); present ones
    // must be well formed: every nested length, string and ULEB128 stays inside
    // its parent (review R02), or the image is refused before any write.
    const std::uint32_t shoff = u32(f, 0x20);
    const std::uint16_t shentsize = u16(f, 0x2e), shnum = u16(f, 0x30);
    if (shoff && shnum && shentsize < 40)
        throw image_error("malformed ELF: section header size " + std::to_string(shentsize));
    for (std::uint16_t i = 0; shoff && i < shnum; ++i) {
        const std::uint64_t sh = shoff + std::uint64_t(i) * shentsize;
        if (u32(f, sh + 4) != 0x70000003)
            continue;
        const std::uint64_t off = u32(f, sh + 16), size = u32(f, sh + 20);
        auto bad = [](const std::string& why) {
            return image_error("malformed .riscv.attributes: " + why);
        };
        if (off + size > f.size() || size < 1 || f[off] != 'A')
            throw bad("section outside the file or not format 'A'");
        // NUL-terminated string in [q, end).
        auto ntbs = [&](std::uint64_t& q, std::uint64_t end) {
            std::string out;
            while (q < end && f[q])
                out += static_cast<char>(f[q++]);
            if (q >= end)
                throw bad("unterminated string");
            ++q;
            return out;
        };
        // ULEB128 in [q, end), at most 5 bytes (32 bits).
        auto uleb = [&](std::uint64_t& q, std::uint64_t end) {
            std::uint32_t value = 0;
            for (unsigned shift = 0; shift < 35; shift += 7) {
                if (q >= end)
                    throw bad("unterminated ULEB128");
                const auto b = f[q++];
                value |= std::uint32_t(b & 0x7f) << shift;
                if (!(b & 0x80))
                    return value;
            }
            throw bad("ULEB128 longer than 5 bytes");
        };
        // Format: 'A', then subsections {u32 length, vendor NTBS, sub-subsections
        // {tag ULEB128, u32 length, attributes}}; the riscv vendor's Tag_File (1)
        // holds tag/value pairs, Tag_RISCV_arch = 5 is a string.
        const std::uint64_t end = off + size;
        std::uint64_t p = off + 1;
        std::string arch;
        while (p < end) {
            if (end - p < 4)
                throw bad("truncated subsection length");
            const std::uint32_t sub_len = u32(f, p);
            if (sub_len < 5 || sub_len > end - p)
                throw bad("subsection length " + std::to_string(sub_len) + " outside the section");
            const std::uint64_t sub_end = p + sub_len;
            std::uint64_t q = p + 4;
            const std::string vendor = ntbs(q, sub_end);
            while (q < sub_end) {
                const std::uint64_t at = q;
                const std::uint32_t tag = uleb(q, sub_end);
                if (sub_end - q < 4)
                    throw bad("truncated attribute block length");
                const std::uint32_t len = u32(f, q);
                if (len < q + 4 - at || len > sub_end - at)
                    throw bad("attribute block length " + std::to_string(len) + " outside the subsection");
                const std::uint64_t block_end = at + len;
                q += 4;
                if (vendor == "riscv" && tag == 1) {
                    while (q < block_end) {
                        const std::uint32_t attr = uleb(q, block_end);
                        // Tag_RISCV_arch (5) is a string; below 32 the other
                        // defined tags are ULEB128; from 32 odd tags are strings.
                        if (attr == 5 || (attr >= 32 && attr % 2 == 1)) {
                            const auto value = ntbs(q, block_end);
                            if (attr == 5 && arch.empty())
                                arch = value;
                        } else {
                            uleb(q, block_end);
                        }
                    }
                }
                q = block_end;
            }
            p = sub_end;
        }
        return arch;
    }
    return {};
}

void image_loader::check_arch(const std::string& arch, bool ibex, const std::string& source)
{
    if (arch.empty())
        return;   // no attributes: the ELF flags were checked
    if (arch.compare(0, 4, "rv32") != 0)
        throw image_error(source + ": arch '" + arch + "' is not RV32");
    std::set<std::string> ext;
    std::size_t i = 4;
    // Single-letter extensions up to the first '_' (each with an optional version "XpY").
    while (i < arch.size() && arch[i] != '_') {
        ext.insert(std::string(1, arch[i]));
        ++i;
        while (i < arch.size() && (std::isdigit(static_cast<unsigned char>(arch[i])) || arch[i] == 'p'))
            ++i;
    }
    while (i < arch.size()) {
        ++i;   // '_'
        std::string name;
        while (i < arch.size() && arch[i] != '_')
            name += arch[i++];
        while (!name.empty() && (std::isdigit(static_cast<unsigned char>(name.back())) || name.back() == 'p'))
            name.pop_back();
        if (!name.empty())
            ext.insert(name);
    }
    const std::set<std::string> ibex_ok = {"i", "m", "c", "zicsr", "zifencei", "zca", "zcb", "zmmul"};
    const std::set<std::string> cva6_ok = {"i", "m", "f", "c", "zicsr", "zifencei", "zca", "zcf", "zmmul"};
    const auto& ok = ibex ? ibex_ok : cva6_ok;
    for (const auto& e : ext)
        if (!ok.count(e))
            throw image_error(source + ": extension '" + e + "' of arch '" + arch + "' is not implemented by " +
                              (ibex ? "Ibex (RV32IMC + Zcb)" : "CVA6 (RV32IMFC)"));
}

namespace {

// Region of [address, address + memsz) and, for code in a CPU's region, the ISA check.
const region& check(std::uint64_t address, std::uint64_t memsz, const std::string& source, bool code,
                    std::uint32_t elf_flags, const std::string& arch)
{
    const auto& r = locate(address, address + memsz, source);
    if (code && r.cpu != owner::none) {
        const bool ibex = r.cpu == owner::ibex;
        const unsigned float_abi = (elf_flags >> 1) & 3;   // EF_RISCV_FLOAT_ABI
        if (elf_flags & 0x8)
            throw image_error(source + ": RVE image, the FU3 CPUs are RV32I");
        if (ibex ? float_abi != 0 : float_abi > 1)
            throw image_error(source + ": float ABI " + std::string(float_abi == 1 ? "single" : "double/quad") +
                              " in " + r.name + ", " + (ibex ? "Ibex has no FPU" : "CVA6 has single precision only"));
        image_loader::check_arch(arch, ibex, source);
    }
    return r;
}

} // namespace

void image_loader::write(std::uint64_t address, const std::uint8_t* data, std::uint64_t length, std::uint64_t memsz,
                         const std::string& source, bool code, std::uint32_t elf_flags, const std::string& arch)
{
    if (!memsz)
        return;
    const auto& r = check(address, memsz, source, code, elf_flags, arch);
    if (r.k == kind::flash) {
        std::vector<unsigned char> bytes(data, data + length);
        bytes.resize(memsz, 0);
        top_.peripherals().flash().load(bytes, address - r.target_base);
    } else {
        auto* mem = top_.memory(r.target);
        for (std::uint64_t i = 0; i < memsz; ++i)
            mem->poke(address - r.target_base + i, i < length ? data[i] : 0);
    }
    if (std::string(r.name) == "ROM")
        rom_loaded_ = true;
    records_.push_back({source, r.name, address, address + memsz});
}

void image_loader::load_elf(const std::string& path)
{
    const auto f = read_file(path);
    if (!is_elf(f))
        throw image_error("'" + path + "' is not an ELF file");
    if (f.size() < 52 || f[4] != 1 || f[5] != 1)
        throw image_error("'" + path + "' is not a 32-bit little-endian ELF");
    if (u16(f, 0x12) != 243)
        throw image_error("'" + path + "' is not a RISC-V ELF (e_machine " + std::to_string(u16(f, 0x12)) + ")");
    const std::uint32_t flags = u32(f, 0x24);
    const std::string arch = arch_of(f);
    const std::uint32_t phoff = u32(f, 0x1c);
    const std::uint16_t phentsize = u16(f, 0x2a), phnum = u16(f, 0x2c);
    struct segment {
        std::uint32_t offset, paddr, filesz, memsz;
        bool code;
        std::string source;
    };
    std::vector<segment> segments;
    for (std::uint16_t i = 0; i < phnum; ++i) {
        const std::uint64_t ph = phoff + std::uint64_t(i) * phentsize;
        if (u32(f, ph) != 1)   // PT_LOAD
            continue;
        const std::uint32_t offset = u32(f, ph + 4), paddr = u32(f, ph + 12), filesz = u32(f, ph + 16),
                            memsz = u32(f, ph + 20), pflags = u32(f, ph + 24);
        if (std::uint64_t(offset) + filesz > f.size())
            throw image_error("'" + path + "': segment " + std::to_string(i) + " beyond the end of the file");
        if (filesz > memsz)
            throw image_error("'" + path + "': segment " + std::to_string(i) + " has more file bytes than memory");
        if (memsz)
            segments.push_back({offset, paddr, filesz, memsz, (pflags & 1) != 0, path + " segment " + std::to_string(i)});
    }
    if (segments.empty())
        throw image_error("'" + path + "' has no loadable segment");
    // Every segment is checked before any is written: a refused ELF leaves the memories unchanged.
    for (const auto& s : segments)
        check(s.paddr, s.memsz, s.source, s.code, flags, arch);
    for (const auto& s : segments)
        write(s.paddr, f.data() + s.offset, s.filesz, s.memsz, s.source, s.code, flags, arch);
}

void image_loader::load_binary(std::uint64_t address, const std::string& path)
{
    const auto f = read_file(path);
    if (f.empty())
        throw image_error("'" + path + "' is empty");
    write(address, f.data(), f.size(), f.size(), path, false, 0, {});
}

void image_loader::load_flash(const std::string& path, std::uint64_t offset)
{
    const auto f = read_file(path);
    if (f.empty())
        throw image_error("'" + path + "' is empty");
    write(0x3000'0000 + offset, f.data(), f.size(), f.size(), path, false, 0, {});
}

void image_loader::load_rom(const std::string& path)
{
    const auto f = read_file(path);
    if (is_elf(f))
        load_elf(path);
    else if (!f.empty())
        write(0, f.data(), f.size(), f.size(), path, false, 0, {});
    if (!rom_loaded_)
        throw image_error("'" + path + "' does not place anything in the ROM (0x0000_0000-0x0000_0FFF)");
}

std::vector<std::uint8_t> image_loader::dump(std::uint64_t address, std::uint64_t length) const
{
    const auto& r = locate(address, address + length, "dump");
    if (r.k != kind::memory)
        throw image_error(std::string("dump of the ") + r.name + " is not supported (on-chip memories only)");
    std::vector<std::uint8_t> out(length);
    auto* mem = top_.memory(r.target);
    for (std::uint64_t i = 0; i < length; ++i)
        out[i] = mem->peek(address - r.target_base + i);
    return out;
}

} // namespace cdc::platforms::fu3_soc
