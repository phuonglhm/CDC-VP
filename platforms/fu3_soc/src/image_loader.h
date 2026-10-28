#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace cdc::platforms::fu3_soc {

class fu3_soc_top;

// Test preload of images into the FU3 VP (plan T20), before the simulation
// starts and outside the hardware boot flow: data is written through the
// memories' backdoor and the QSPI NOR model, without simulated time or bus
// policy. It is not a Boot ROM: the ROM image is the mask content of the
// Ibex-only ROM, the rest stands for what a Boot ROM would have staged or what
// a board flash holds.
//
// Regions a preload may write (FU3_VP_ABI_1):
//   ROM            0x0000_0000-0x0000_0FFF  Ibex
//   CVA6_ISRAM     0x0001_0000-0x0003_FFFF  CVA6 image (not the debug code 0x0004_0000-0x0004_0FFF)
//   IBEX_ISRAM     0x0005_0000-0x0005_FFFF  Ibex image (not the debug code 0x0006_0000-0x0006_0FFF)
//   DSRAM          0x0100_0000-0x0103_FFFF  data
//   QSPI_XIP       0x3000_0000-0x30FF_FFFF  the QSPI NOR at offset address - 0x3000_0000
// A debug partition (the Boot ROM writes it), HyperRAM (needs its controller
// initialised), MMIO and unmapped space are refused, as is any range that
// crosses a region boundary. An ELF is checked for RV32 little-endian RISC-V
// and, for code placed in a CPU's region, for the ISA that CPU implements
// (ELF float ABI and RVE flags, `.riscv.attributes` arch string): Ibex
// RV32IMC + Zicsr/Zifencei/Zca/Zcb; CVA6 RV32IMFC + Zicsr/Zifencei/Zca/Zcf.
class image_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class image_loader {
public:
    explicit image_loader(fu3_soc_top& top) : top_(top) {}

    struct record {
        std::string source, region;
        std::uint64_t begin = 0, end = 0;   // [begin, end) system addresses
    };

    // Every PT_LOAD segment (file bytes, then zero fill up to its memory size).
    void load_elf(const std::string& path);
    // A raw file at a system address.
    void load_binary(std::uint64_t address, const std::string& path);
    // A raw file into the QSPI NOR at `offset`.
    void load_flash(const std::string& path, std::uint64_t offset);
    // A file into the ROM: an ELF (its segments must be ROM-only or allowed
    // regions) or a raw image at 0x0000_0000.
    void load_rom(const std::string& path);

    bool rom_loaded() const { return rom_loaded_; }
    const std::vector<record>& records() const { return records_; }
    // Memory content after a run (on-chip memories only).
    std::vector<std::uint8_t> dump(std::uint64_t address, std::uint64_t length) const;

    // Arch string of `.riscv.attributes` (empty if absent) and the extension
    // check, exposed for tests.
    static std::string arch_of(const std::vector<std::uint8_t>& elf);
    static void check_arch(const std::string& arch, bool ibex, const std::string& source);

private:
    fu3_soc_top& top_;
    bool rom_loaded_ = false;
    std::vector<record> records_;

    void write(std::uint64_t address, const std::uint8_t* data, std::uint64_t length, std::uint64_t memsz,
               const std::string& source, bool code, std::uint32_t elf_flags, const std::string& arch);
};

} // namespace cdc::platforms::fu3_soc
