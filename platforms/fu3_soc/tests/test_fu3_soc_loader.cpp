// Plan T20: image loader rules (image_loader.h) with synthetic ELF files:
// region routing (ROM, ISRAM images, DSRAM, QSPI NOR through the XIP window),
// refusals (debug partitions, HyperRAM, MMIO, unmapped, region-crossing),
// ELF format checks, the ISA check of code per CPU (float ABI, RVE, arch
// string of .riscv.attributes), raw/flash loads, ROM detection and dumps.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>

#include "fu3_soc_top.h"
#include "image_loader.h"

using namespace cdc::platforms::fu3_soc;

namespace {

unsigned checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

void put32(std::vector<std::uint8_t>& f, std::size_t at, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        f[at + i] = (v >> (8 * i)) & 0xff;
}
void put16(std::vector<std::uint8_t>& f, std::size_t at, std::uint16_t v)
{
    f[at] = v & 0xff;
    f[at + 1] = v >> 8;
}

struct segment {
    std::uint32_t paddr;
    std::vector<std::uint8_t> data;
    std::uint32_t memsz;   // 0: data size
    bool exec;
};

// ELF32 RISC-V with the given segments, e_flags and (optional) arch string,
// or with `raw_attributes` as the whole .riscv.attributes content.
std::vector<std::uint8_t> make_elf(const std::vector<segment>& segs, std::uint32_t flags = 0,
                                   const std::string& arch = {}, std::uint16_t machine = 243, std::uint8_t cls = 1,
                                   const std::vector<std::uint8_t>& raw_attributes = {})
{
    const std::size_t ph = 52, data_at = ph + 32 * segs.size();
    std::vector<std::uint8_t> f(data_at);
    f[0] = 0x7f, f[1] = 'E', f[2] = 'L', f[3] = 'F', f[4] = cls, f[5] = 1, f[6] = 1;
    put16(f, 0x10, 2);
    put16(f, 0x12, machine);
    put32(f, 0x14, 1);
    put32(f, 0x1c, ph);
    put32(f, 0x24, flags);
    put16(f, 0x28, 52);
    put16(f, 0x2a, 32);
    put16(f, 0x2c, static_cast<std::uint16_t>(segs.size()));
    for (std::size_t i = 0; i < segs.size(); ++i) {
        const std::size_t p = ph + 32 * i;
        put32(f, p, 1);
        put32(f, p + 4, static_cast<std::uint32_t>(f.size()));
        put32(f, p + 8, segs[i].paddr);
        put32(f, p + 12, segs[i].paddr);
        put32(f, p + 16, static_cast<std::uint32_t>(segs[i].data.size()));
        put32(f, p + 20, segs[i].memsz ? segs[i].memsz : static_cast<std::uint32_t>(segs[i].data.size()));
        put32(f, p + 24, segs[i].exec ? 5 : 6);
        f.insert(f.end(), segs[i].data.begin(), segs[i].data.end());
    }
    if (!arch.empty() || !raw_attributes.empty()) {
        // .riscv.attributes: 'A', subsection {len, "riscv\0", Tag_File 1, len, Tag_RISCV_arch 5, NTBS}
        std::vector<std::uint8_t> a{'A'};
        std::vector<std::uint8_t> file{1, 0, 0, 0, 0, 5};
        file.insert(file.end(), arch.begin(), arch.end());
        file.push_back(0);
        put32(file, 1, static_cast<std::uint32_t>(file.size()));
        std::vector<std::uint8_t> sub(4);
        const std::string vendor = "riscv";
        sub.insert(sub.end(), vendor.begin(), vendor.end());
        sub.push_back(0);
        sub.insert(sub.end(), file.begin(), file.end());
        put32(sub, 0, static_cast<std::uint32_t>(sub.size()));
        a.insert(a.end(), sub.begin(), sub.end());
        if (!raw_attributes.empty())
            a = raw_attributes;
        const std::size_t attr_at = f.size();
        f.insert(f.end(), a.begin(), a.end());
        while (f.size() % 4)
            f.push_back(0);
        const std::size_t sh = f.size();
        f.resize(sh + 2 * 40, 0);   // null section + attributes section
        put32(f, sh + 40 + 4, 0x70000003);
        put32(f, sh + 40 + 16, static_cast<std::uint32_t>(attr_at));
        put32(f, sh + 40 + 20, static_cast<std::uint32_t>(a.size()));
        put32(f, 0x20, static_cast<std::uint32_t>(sh));
        put16(f, 0x2e, 40);
        put16(f, 0x30, 2);
    }
    return f;
}

std::string file_of(const std::string& name, const std::vector<std::uint8_t>& bytes)
{
    const std::string path = "loader_" + name;
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    return path;
}

// True when `action` throws image_error whose text contains `needle`.
bool refused(const std::function<void()>& action, const std::string& needle)
{
    try {
        action();
    } catch (const image_error& e) {
        if (std::string(e.what()).find(needle) == std::string::npos)
            std::cerr << "  unexpected message: " << e.what() << '\n';
        return std::string(e.what()).find(needle) != std::string::npos;
    }
    return false;
}

std::uint32_t word(fu3_soc_top& top, const char* mem, std::uint64_t off)
{
    auto* m = top.memory(mem);
    return m->peek(off) | m->peek(off + 1) << 8 | m->peek(off + 2) << 16 | std::uint32_t(m->peek(off + 3)) << 24;
}

} // namespace

int sc_main(int, char**)
{
    fu3_soc_top top("fu3_soc", fu3_soc_options{});
    image_loader loader(top);
    const std::vector<std::uint8_t> code{0x13, 0, 0, 0, 0x6f, 0, 0, 0};   // nop; j .

    // A refused ELF writes nothing: its valid ROM segment comes before the refused one.
    check(refused([&] { loader.load_elf(file_of("partial.elf", make_elf({{0x80, code, 0, true}, {0x60000, code, 0, true}}))); },
                  "Ibex debug partition"),
          "ELF with a debug partition segment refused");
    check(!loader.rom_loaded() && loader.records().empty() && word(top, "ROM", 0x80) == 0,
          "refused ELF left the ROM unchanged");

    // Stale DSRAM content where the image's bss goes.
    loader.load_binary(0x0102000c, file_of("stale.bin", {0xde, 0xad, 0xbe, 0xef}));

    // Routing of a valid Ibex image (ROM + IBEX_ISRAM + DSRAM with bss) and a CVA6 image.
    check(!loader.rom_loaded(), "no ROM content before a load");
    loader.load_elf(file_of("ibex.elf", make_elf({{0x80, code, 0, true}, {0x50000, code, 0, true},
                                                   {0x01020000, {1, 2, 3, 4}, 16, false}},
                                                  0x1, "rv32i2p1_m2p0_c2p0_zicsr2p0_zifencei2p0_zca1p0_zcb1p0")));
    check(loader.rom_loaded() && word(top, "ROM", 0x84) == 0x6f && word(top, "IBEX_ISRAM", 0) == 0x13,
          "Ibex ELF: ROM and IBEX_ISRAM segments loaded");
    check(word(top, "DSRAM", 0x20000) == 0x04030201 && word(top, "DSRAM", 0x2000c) == 0,
          "DSRAM data segment loaded, bss zero-filled");
    check(loader.records().size() == 4 && loader.records()[1].region == "ROM", "load records name their region");
    loader.load_elf(file_of("cva6.elf", make_elf({{0x10000, code, 0, true}}, 0x3, "rv32i2p1_m2p0_f2p2_c2p0_zicsr2p0")));
    check(word(top, "CVA6_ISRAM", 4) == 0x6f, "CVA6 ELF (single-float ABI, F) loaded into CVA6_ISRAM");

    // Refused regions and ranges.
    const auto blob = file_of("blob.bin", {0x6f, 0, 0, 0});
    check(refused([&] { loader.load_binary(0x60000, blob); }, "Ibex debug partition"),
          "raw load into the Ibex debug partition refused");
    check(refused([&] { loader.load_elf(file_of("dbg.elf", make_elf({{0x40000, code, 0, true}}))); }, "CVA6 debug partition"),
          "ELF segment in the CVA6 debug partition refused");
    check(refused([&] { loader.load_binary(0x3fffe, blob); }, "CVA6 debug partition"),
          "image ending inside a debug partition refused");
    check(refused([&] { loader.load_binary(0x60000000, blob); }, "HyperRAM needs its controller"), "HyperRAM refused");
    check(refused([&] { loader.load_binary(0x44000000, blob); }, "outside every preload region"), "MMIO refused");
    check(refused([&] { loader.load_binary(0x2000, blob); }, "outside every preload region"),
          "unmapped gap refused");
    check(refused([&] { loader.load_binary(0xffe, blob); }, "crosses the ROM boundary"), "region-crossing image refused");
    check(refused([&] { loader.load_binary(0x0103fffe, blob); }, "crosses the DSRAM boundary"), "DSRAM end crossing refused");

    // ELF format and ISA.
    check(refused([&] { loader.load_elf(blob); }, "not an ELF"), "raw file given as ELF refused");
    check(refused([&] { loader.load_elf(file_of("x86.elf", make_elf({{0x50000, code, 0, true}}, 0, {}, 62))); },
                  "not a RISC-V ELF"),
          "non-RISC-V machine refused");
    check(refused([&] { loader.load_elf(file_of("rv64.elf", make_elf({{0x50000, code, 0, true}}, 0, {}, 243, 2))); },
                  "32-bit"),
          "ELFCLASS64 refused");
    check(refused([&] { loader.load_elf(file_of("ibexf.elf", make_elf({{0x50000, code, 0, true}}, 0x2))); }, "Ibex has no FPU"),
          "Ibex code with a single-float ABI refused");
    check(refused([&] { loader.load_elf(file_of("cva6d.elf", make_elf({{0x10000, code, 0, true}}, 0x4))); },
                  "single precision only"),
          "CVA6 code with a double-float ABI refused");
    check(refused([&] { loader.load_elf(file_of("rve.elf", make_elf({{0x50000, code, 0, true}}, 0x8))); }, "RVE"),
          "RVE image refused");
    check(refused([&] { loader.load_elf(file_of("ibexa.elf", make_elf({{0x50000, code, 0, true}}, 0, "rv32i2p1_m2p0_a2p1_c2p0"))); },
                  "extension 'a'"),
          "Ibex code using A refused (arch string)");
    check(refused([&] { loader.load_elf(file_of("ibexb.elf", make_elf({{0x50000, code, 0, true}}, 0, "rv32i2p1_m2p0_c2p0_zba1p0"))); },
                  "extension 'zba'"),
          "Ibex code using Zba refused");
    check(refused([&] { loader.load_elf(file_of("cva6zcb.elf", make_elf({{0x10000, code, 0, true}}, 0x2, "rv32i2p1_m2p0_f2p2_c2p0_zcb1p0"))); },
                  "extension 'zcb'"),
          "CVA6 code using Zcb refused (RVZCB = 0)");
    loader.load_elf(file_of("ibexdata.elf", make_elf({{0x01000100, code, 0, false}}, 0x2)));
    check(word(top, "DSRAM", 0x100) == 0x13, "data in DSRAM is not ISA-checked (no owning CPU)");
    check(image_loader::arch_of(make_elf({{0x50000, code, 0, true}}, 0, "rv32imc")) == "rv32imc",
          "arch string read from .riscv.attributes");

    // Review R02: malformed attributes are refused (no read outside the file,
    // nothing written). Each case is complete apart from the one defect.
    auto bytes = [](std::initializer_list<int> list) {
        std::vector<std::uint8_t> out;
        for (const int b : list)
            out.push_back(static_cast<std::uint8_t>(b));
        return out;
    };
    const struct {
        const char* what;
        std::vector<std::uint8_t> attributes;
        const char* message;
    } malformed[] = {
        {"subsection length 0x7fffffff, vendor without NUL",
         bytes({'A', 0xff, 0xff, 0xff, 0x7f, 'r', 'i', 's', 'c', 'v'}), "subsection length"},
        {"vendor string not terminated inside its subsection",
         bytes({'A', 9, 0, 0, 0, 'r', 'i', 's', 'c', 'v', 0}), "unterminated string"},
        {"Tag_File length beyond its subsection",
         bytes({'A', 15, 0, 0, 0, 'r', 'i', 's', 'c', 'v', 0, 1, 0x00, 1, 0, 0}), "attribute block length"},
        {"arch string not terminated inside Tag_File",
         bytes({'A', 20, 0, 0, 0, 'r', 'i', 's', 'c', 'v', 0, 1, 10, 0, 0, 0, 5, 'r', 'v', '3', '2'}),
         "unterminated string"},
        {"ULEB128 tag not terminated inside Tag_File",
         bytes({'A', 16, 0, 0, 0, 'r', 'i', 's', 'c', 'v', 0, 1, 6, 0, 0, 0, 0x84}), "unterminated ULEB128"},
        {"truncated subsection length", bytes({'A', 3, 0}), "truncated subsection length"},
    };
    for (const auto& m : malformed) {
        image_loader fresh(top);
        const auto before = word(top, "IBEX_ISRAM", 0x100);
        const auto elf = make_elf({{0x50100, {0x6f, 0, 0, 0}, 0, true}}, 0, {}, 243, 1, m.attributes);
        check(refused([&] { image_loader::arch_of(elf); }, m.message), std::string("arch_of: ") + m.what);
        check(refused([&] { fresh.load_elf(file_of("bad_attr.elf", elf)); }, "malformed .riscv.attributes") &&
                  fresh.records().empty() && word(top, "IBEX_ISRAM", 0x100) == before,
              std::string("load refused, nothing written: ") + m.what);
    }

    // Flash through the XIP window and --flash offsets; ROM raw image; dump.
    loader.load_flash(file_of("flash.bin", {0xaa, 0xbb, 0xcc, 0xdd}), 0x100);
    loader.load_binary(0x30000200, blob);
    const auto nor = top.peripherals().flash().read_image(0x100, 4), nor2 = top.peripherals().flash().read_image(0x200, 1);
    check(nor == std::vector<unsigned char>{0xaa, 0xbb, 0xcc, 0xdd} && nor2[0] == 0x6f,
          "QSPI NOR loaded at an offset and through the XIP window");
    check(refused([&] { loader.load_flash(blob, 0xfffffe); }, "crosses the QSPI_XIP boundary"), "flash end crossing refused");
    image_loader rom_only(top);
    rom_only.load_rom(file_of("rom.bin", {0x6f, 0, 0, 0}));
    check(rom_only.rom_loaded() && word(top, "ROM", 0) == 0x6f, "raw ROM image at 0x0");
    image_loader no_rom(top);
    check(refused([&] { no_rom.load_rom(file_of("norom.elf", make_elf({{0x50000, code, 0, true}}))); },
                  "does not place anything in the ROM"),
          "--rom with an ELF lacking a ROM segment refused");
    const auto d = loader.dump(0x01020000, 4);
    check(d == std::vector<std::uint8_t>{1, 2, 3, 4}, "dump of DSRAM");
    check(refused([&] { loader.dump(0x30000000, 4); }, "on-chip memories only"), "dump of the NOR window refused");

    std::cout << "fu3_soc loader: " << checks << " checks, " << failures << " failures -> " << (failures ? "FAIL" : "PASS")
              << '\n';
    return failures ? 1 : 0;
}
