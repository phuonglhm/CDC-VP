#pragma once

// Minimal, dependency-free ELF loader for bare-metal RISC-V images.
//
// Parses PT_LOAD segments and writes them into memory through a TLM-2.0 initiator
// socket using the debug (backdoor, untimed) transport, so it works with the
// bus_router's address translation and any memory_tlm target. Supports ELFCLASS32
// and ELFCLASS64, little-endian only (RISC-V). Returns the ELF entry point.
//
// Call this once bindings are resolved (e.g. from start_of_simulation()), not
// from a constructor.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <tlm.h>

namespace cdc::cpu {

namespace detail {

inline std::uint16_t rd16(const std::vector<std::uint8_t>& b, std::size_t off)
{
    return static_cast<std::uint16_t>(b.at(off) | (b.at(off + 1) << 8));
}

inline std::uint32_t rd32(const std::vector<std::uint8_t>& b, std::size_t off)
{
    return static_cast<std::uint32_t>(b.at(off)) |
           (static_cast<std::uint32_t>(b.at(off + 1)) << 8) |
           (static_cast<std::uint32_t>(b.at(off + 2)) << 16) |
           (static_cast<std::uint32_t>(b.at(off + 3)) << 24);
}

inline std::uint64_t rd64(const std::vector<std::uint8_t>& b, std::size_t off)
{
    std::uint64_t lo = rd32(b, off);
    std::uint64_t hi = rd32(b, off + 4);
    return lo | (hi << 32);
}

// Backdoor write of a memory block through an initiator socket.
template <class Socket>
void dbg_write(Socket& sock, std::uint64_t addr, const std::uint8_t* data, unsigned len)
{
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(const_cast<unsigned char*>(data));
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    const unsigned n = sock->transport_dbg(trans);
    if (n != len) {
        throw std::runtime_error("elf_loader: backdoor write incomplete (address not mapped?)");
    }
}

} // namespace detail

// Loads `path` into memory reachable through `sock`. Returns the entry point.
template <class Socket>
std::uint64_t load_elf(Socket& sock, const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("elf_loader: cannot open " + path);
    }
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

    if (b.size() < 64 || b[0] != 0x7F || b[1] != 'E' || b[2] != 'L' || b[3] != 'F') {
        throw std::runtime_error("elf_loader: not an ELF file: " + path);
    }

    const bool is64 = (b[4] == 2);          // EI_CLASS: 1=32, 2=64
    if (b[5] != 1) {                        // EI_DATA: 1=little-endian
        throw std::runtime_error("elf_loader: only little-endian ELF supported");
    }

    std::uint64_t entry, phoff;
    std::uint16_t phentsize, phnum;
    if (is64) {
        entry = detail::rd64(b, 24);
        phoff = detail::rd64(b, 32);
        phentsize = detail::rd16(b, 54);
        phnum = detail::rd16(b, 56);
    } else {
        entry = detail::rd32(b, 24);
        phoff = detail::rd32(b, 28);
        phentsize = detail::rd16(b, 42);
        phnum = detail::rd16(b, 44);
    }

    for (std::uint16_t i = 0; i < phnum; ++i) {
        const std::size_t ph = static_cast<std::size_t>(phoff) + i * phentsize;
        const std::uint32_t p_type = detail::rd32(b, ph);
        if (p_type != 1) {                  // PT_LOAD
            continue;
        }

        std::uint64_t p_offset, p_paddr, p_filesz;
        if (is64) {
            p_offset = detail::rd64(b, ph + 8);
            p_paddr = detail::rd64(b, ph + 16);
            p_filesz = detail::rd64(b, ph + 32);
        } else {
            p_offset = detail::rd32(b, ph + 4);
            p_paddr = detail::rd32(b, ph + 12);
            p_filesz = detail::rd32(b, ph + 16);
        }

        if (p_filesz == 0) {
            continue;
        }
        if (p_offset + p_filesz > b.size()) {
            throw std::runtime_error("elf_loader: segment exceeds file size");
        }
        detail::dbg_write(sock, p_paddr, b.data() + p_offset,
                          static_cast<unsigned>(p_filesz));
    }

    return entry;
}

} // namespace cdc::cpu
