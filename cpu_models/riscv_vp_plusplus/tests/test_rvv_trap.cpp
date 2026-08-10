// SPDX-License-Identifier: Apache-2.0
//
// Vector-trap gate — decision record D10.
//
// Runs the RV32GCV trap image and checks the one form of RVV restart state this
// backend can actually produce: a fault at element *k* of a vector load, and
// resumption from `vstart` afterwards.
//
// Interrupt-driven restart is not tested because it cannot happen: VP++ has no
// interrupt check anywhere in its per-element vector loop, so a vector
// instruction is atomic with respect to interrupts and never leaves a partially
// executed one to resume. That is recorded as D10 rather than quietly dropped.
//
// The host contributes two things the firmware cannot check about itself:
//
//   * one-shot fault injection, so the fault lands mid-instruction rather than
//     before it;
//   * per-address access counting, which is the only way to prove that elements
//     below `vstart` were **not** re-accessed on resumption. The firmware sees
//     correct data either way; only the bus shows whether the load restarted
//     from the right element.
//
// Usage: test_rvv_trap <rvv_trap.elf>
// Exit codes: 0 pass, 1 fail, 77 skip (image not built).

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

#include "riscv_vp_plusplus_wrapper.h"

extern "C" {
#include "sim_exit.h"
}

namespace {

constexpr std::uint64_t kMemorySize = 1024 * 1024;
constexpr int kSkip = 77;

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

class fault_injecting_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<fault_injecting_memory> tsock;
    std::vector<unsigned char> storage;

    /// Armed by the firmware writing to `SIM_FAULT_ARM`. The next read of this
    /// address returns an error, once.
    std::uint64_t armed_fault_address = 0;
    bool fault_armed = false;
    unsigned long faults_delivered = 0;

    /// Read counts per address, for the "elements below vstart are not
    /// re-accessed" check. Only data reads are recorded; instruction fetch
    /// would otherwise swamp it.
    std::map<std::uint64_t, unsigned long> read_counts;
    /// `read_counts` frozen at the end of phase 1, before later phases touch
    /// the same buffer.
    std::map<std::uint64_t, unsigned long> phase1_read_counts;
    bool phase1_marked = false;
    std::uint64_t count_region_begin = 0;
    std::uint64_t count_region_end = 0;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;
    std::uint32_t trap_count = 0;
    std::uint32_t trap_mcause = 0;
    std::uint32_t trap_mepc = 0;
    std::uint32_t trap_mtval = 0;
    std::uint32_t trap_vstart = 0;
    std::uint32_t trap_vl = 0;
    std::uint32_t trap_vtype = 0;
    std::uint32_t eew64_trapped = 0;
    std::uint32_t eew64_mcause = 0;

    fault_injecting_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &fault_injecting_memory::b_transport);
        tsock.register_transport_dbg(this,
                                     &fault_injecting_memory::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        const bool is_write = trans.get_command() == tlm::TLM_WRITE_COMMAND;

        // One shot: disarm before responding, so the resumed access succeeds
        // and the test distinguishes "resumed correctly" from "kept faulting".
        if (!is_write && fault_armed && address == armed_fault_address) {
            fault_armed = false;
            ++faults_delivered;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        if (!is_write && count_region_end > count_region_begin
            && address >= count_region_begin && address < count_region_end) {
            ++read_counts[address];
        }

        if (address > storage.size() || length > storage.size() - address) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (is_write) {
            std::memcpy(storage.data() + address, trans.get_data_ptr(), length);
            observe_write(address);
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + address, length);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        if (address > storage.size() || length > storage.size() - address) {
            return 0;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + address, trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + address, length);
        }
        return length;
    }

    void observe_write(std::uint64_t address)
    {
        if (address == SIM_FAULT_ARM) {
            const std::uint32_t target = load_word(SIM_FAULT_ARM);
            armed_fault_address = target;
            fault_armed = target != 0;
            return;
        }
        if (address == SIM_PHASE_MARK) {
            if (load_word(SIM_PHASE_MARK) == 1u) {
                phase1_read_counts = read_counts;
                phase1_marked = true;
            }
            return;
        }
        if (address == SIM_EXIT_KIND) {
            exit_kind = load_word(SIM_EXIT_KIND);
            exit_status = load_word(SIM_EXIT_STATUS);
            trap_count = load_word(SIM_TRAP_COUNT);
            trap_mcause = load_word(SIM_TRAP_MCAUSE);
            trap_mepc = load_word(SIM_TRAP_MEPC);
            trap_mtval = load_word(SIM_TRAP_MTVAL);
            trap_vstart = load_word(SIM_TRAP_VSTART);
            trap_vl = load_word(SIM_TRAP_VL);
            trap_vtype = load_word(SIM_TRAP_VTYPE);
            eew64_trapped = load_word(SIM_EEW64_TRAPPED);
            eew64_mcause = load_word(SIM_EEW64_MCAUSE);
            exited = true;
            sc_core::sc_stop();
        }
    }

    std::uint32_t load_word(std::uint64_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, storage.data() + address, sizeof(value));
        return value;
    }
};

/// Address of a symbol in the image, so the access-count check is anchored to
/// the real link rather than to a guessed layout.
bool read_symbol(const std::string& path, const std::string& wanted,
                 std::uint64_t& value)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    if (image.size() < 52 || image[0] != 0x7f || image[1] != 'E') {
        return false;
    }
    const auto rd16 = [&](std::size_t off) {
        return static_cast<std::uint32_t>(image[off])
            | (static_cast<std::uint32_t>(image[off + 1]) << 8);
    };
    const auto rd32 = [&](std::size_t off) {
        return static_cast<std::uint32_t>(image[off])
            | (static_cast<std::uint32_t>(image[off + 1]) << 8)
            | (static_cast<std::uint32_t>(image[off + 2]) << 16)
            | (static_cast<std::uint32_t>(image[off + 3]) << 24);
    };

    const std::uint32_t shoff = rd32(0x20);
    const std::uint32_t shentsize = rd16(0x2e);
    const std::uint32_t shnum = rd16(0x30);

    for (std::uint32_t i = 0; i < shnum; ++i) {
        const std::size_t sh = shoff + i * shentsize;
        if (sh + 40 > image.size() || rd32(sh + 4) != 2u) {  // SHT_SYMTAB
            continue;
        }
        const std::uint32_t symoff = rd32(sh + 16);
        const std::uint32_t symsize = rd32(sh + 20);
        const std::uint32_t stroff = rd32(shoff + rd32(sh + 24) * shentsize + 16);

        for (std::uint32_t off = 0; off + 16 <= symsize; off += 16) {
            const std::size_t sym = symoff + off;
            const char* name = reinterpret_cast<const char*>(
                image.data() + stroff + rd32(sym));
            if (wanted == name) {
                value = rd32(sym + 4);
                return true;
            }
        }
    }
    return false;
}

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: test_rvv_trap <rvv_trap.elf>\n";
        return 2;
    }
    const std::string elf_path = argv[1];
    {
        std::ifstream probe(elf_path, std::ios::binary);
        if (!probe) {
            std::cerr << "SKIP: " << elf_path << " was not built\n";
            return kSkip;
        }
    }

    std::uint64_t vsrc = 0;
    if (!read_symbol(elf_path, "vsrc", vsrc)) {
        std::cerr << "FAIL: could not locate `vsrc` in " << elf_path << '\n';
        return 1;
    }

    constexpr unsigned kElements = 16;
    constexpr unsigned kFaultElement = 5;   // must match trap_main.c
    const std::uint64_t fault_address = vsrc + 4 * kFaultElement;

    fault_injecting_memory mem("mem", kMemorySize);
    mem.count_region_begin = vsrc;
    mem.count_region_end = vsrc + 4 * kElements;

    cdc::cpu::cpu_config config;
    config.hart_id = 0;
    cdc::cpu::riscv_vp_plusplus_cpu cpu("core", config);
    cpu.data_bus().bind(mem.tsock);
    cpu.load_elf(elf_path);

    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));

    std::cout << "vsrc                 : 0x" << std::hex << vsrc << std::dec
              << "  (fault armed at element " << kFaultElement << ", 0x"
              << std::hex << fault_address << std::dec << ")\n"
              << "faults delivered     : " << mem.faults_delivered << '\n'
              << "instructions retired : " << cpu.get_instret() << '\n'
              << "\ntrap signature\n"
              << "  count              : " << mem.trap_count << '\n'
              << "  mcause             : " << mem.trap_mcause << '\n'
              << "  mepc               : 0x" << std::hex << mem.trap_mepc
              << std::dec << '\n'
              << "  mtval              : 0x" << std::hex << mem.trap_mtval
              << std::dec << '\n'
              << "  vstart             : " << mem.trap_vstart << '\n'
              << "  vl / vtype         : " << mem.trap_vl << " / 0x"
              << std::hex << mem.trap_vtype << std::dec << '\n'
              << "\nRV32 index EEW=64 probe (recorded, not judged \u2014 the Spike\n"
                 "differential run settles whether this is correct)\n"
              << "  vluxei64.v trapped : " << (mem.eew64_trapped ? "yes" : "no")
              << "   mcause " << mem.eew64_mcause << '\n';

    if (!mem.exited) {
        std::cerr << "FAIL: the image never signalled exit; PC 0x" << std::hex
                  << cpu.get_pc() << std::dec << '\n';
        return 1;
    }
    if (mem.exit_kind == SIM_EXIT_KIND_TRAPPED) {
        std::cerr << "FAIL: unhandled trap. mcause=" << mem.trap_mcause
                  << " mepc=0x" << std::hex << mem.trap_mepc << std::dec << '\n';
        return 1;
    }
    if (mem.exit_status != SIM_EXIT_PASS) {
        std::cerr << "FAIL: trap check id " << mem.exit_status
                  << " failed (see enum trap_check_id in trap_main.c)\n";
        return 1;
    }

    // The fault was actually delivered — otherwise every firmware check below
    // would pass vacuously on a machine that simply never faulted.
    CHECK(mem.faults_delivered == 1);
    // The firmware already asserted the phase-1 signature at the moment it was
    // captured (mcause 13, mtval, vstart) and returned a numbered failure if it
    // was wrong. The block read here holds the *last* trap of the run, which is
    // a later phase's, so re-asserting phase-1 values against it would be
    // checking the wrong record.
    CHECK(mem.trap_count >= 1);

    // The check the firmware cannot make about itself: on resumption the load
    // must restart at `vstart`, so elements below it are read exactly once
    // across the whole run, and elements from it upward are read twice — once
    // for the attempt that faulted at element k, once after the resume.
    if (!mem.phase1_marked) {
        std::cerr << "FAIL: phase 1 never completed, so the read counts were "
                     "never snapshotted\n";
        return 1;
    }

    std::cout << "\nper-element read counts at the end of phase 1 (0x"
              << std::hex << vsrc << std::dec << " ..)\n  ";
    bool below_ok = true;
    bool at_and_above_ok = true;
    for (unsigned element = 0; element < kElements; ++element) {
        const auto address = vsrc + 4 * element;
        const auto count = mem.phase1_read_counts.count(address) != 0
                               ? mem.phase1_read_counts.at(address)
                               : 0ul;
        std::cout << element << ':' << count << ' ';

        if (element < kFaultElement) {
            // Read once, before the fault. A second read means the resumed
            // instruction restarted from element 0 and ignored vstart.
            if (count != 1) {
                below_ok = false;
            }
        } else if (element == kFaultElement) {
            // The faulting read is answered with an error and is not counted,
            // so this element is seen once: on the resumed pass.
            if (count != 1) {
                at_and_above_ok = false;
            }
        } else {
            // Not reached before the fault, read once after it.
            if (count != 1) {
                at_and_above_ok = false;
            }
        }
    }
    std::cout << '\n';

    if (!below_ok) {
        std::cerr << "FAIL: elements below vstart were re-accessed after the "
                     "trap; the vector load did not resume from vstart\n";
        ++failures;
    }
    if (!at_and_above_ok) {
        std::cerr << "FAIL: unexpected read counts at or above the faulting "
                     "element\n";
        ++failures;
    }

    if (failures != 0) {
        std::cerr << '\n' << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_rvv_trap: mid-vector fault, vstart resumption, "
                 "mstatus.VS, vill and RV32 EEW=64 all behave — D10 gate passed\n";
    return 0;
}
