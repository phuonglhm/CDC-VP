// SPDX-License-Identifier: Apache-2.0
//
// Gate for the two downstream conformance patches, D12 and D13.
//
// The Spike differential corpus already proves that the *results* now match the
// oracle. This gate proves the properties an oracle comparison cannot reach,
// because they are about what the model does on the way to the result:
//
//   D12 — all 32 index-EEW=64 encodings are refused, and an illegal
//         instruction must not issue a bus request. Only the host
//         can see that. Spike and VP++ can agree perfectly on `mcause` while
//         one of them has already driven the bus, counted a load and dirtied
//         `mstatus.VS`, and the differential run would call it a match.
//
//   D13 — every access origin must map to its own access-fault cause. The
//         firmware checks the causes; the host checks that the accesses it
//         refused are the ones the firmware meant to make, so a fault caused by
//         some unrelated stray access cannot be mistaken for the probe working.
//
// Usage: test_conformance <rvv_conf.elf> [protocol-error]
//
// `protocol-error` runs the second half of D13's response policy: the memory
// answers with a malformed-transaction status instead of a refusal, and the
// model must abort rather than synthesise a guest fault out of a model defect.
//
// Exit codes: 0 pass, 1 fail, 77 skip (image not built).

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
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

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "FAIL: " << (what) << "  [" #cond " @ " << __LINE__   \
                      << "]\n";                                               \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

class probe_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<probe_memory> tsock;
    std::vector<unsigned char> storage;

    /// Accesses to the region the D12 probes point at, counted so the gate can
    /// assert that an illegal instruction produced *no* bus traffic.
    std::uint64_t watch_begin = 0;
    std::uint64_t watch_end = 0;
    unsigned long watched_accesses = 0;
    unsigned long watched_at_d12_end = 0;
    bool d12_phase_begun = false;
    bool d12_phase_marked = false;

    /// Everything the model asked for outside the memory, in order. The D13
    /// probes are checked against this so a cause can be tied to the access
    /// that produced it.
    std::vector<std::uint64_t> refused;

    /// D13 response policy. A target may decline an access two ways, and both
    /// must become the same guest access fault; a malformed transaction is a
    /// different thing entirely and must not.
    std::uint64_t generic_error_address = 0;
    bool protocol_error_mode = false;
    bool protocol_error_delivered = false;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;
    std::uint32_t exit_mcause = 0;

    probe_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &probe_memory::b_transport);
        tsock.register_transport_dbg(this, &probe_memory::transport_dbg);
    }

    std::uint32_t load_word(std::uint64_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, storage.data() + address, sizeof(value));
        return value;
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();

        if (address >= storage.size() || length > storage.size() - address) {
            refused.push_back(address);
            if (protocol_error_mode) {
                // A malformed transaction, not a refusal. D13 requires the
                // model to abort instead of inventing a guest fault, so this
                // run expects an exception rather than a trap.
                protocol_error_delivered = true;
                trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
                return;
            }
            // Both refusal statuses are exercised: the address probes use
            // ADDRESS_ERROR, and one dedicated address uses GENERIC_ERROR, so
            // the gate covers each branch of the policy rather than the one
            // the harness happens to prefer.
            trans.set_response_status(address == generic_error_address
                                          ? tlm::TLM_GENERIC_ERROR_RESPONSE
                                          : tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (watch_end > watch_begin && address >= watch_begin
            && address < watch_end) {
            ++watched_accesses;
        }

        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
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
        if (address >= storage.size() || length > storage.size() - address) {
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
        if (address == SIM_D12_PHASE_BEGIN) {
            // The setup writes the operand buffer legitimately; only what
            // happens after this point can be attributed to the probes.
            watched_accesses = 0;
            d12_phase_begun = true;
            return;
        }
        if (address == SIM_D12_PHASE_END) {
            watched_at_d12_end = watched_accesses;
            d12_phase_marked = true;
            return;
        }
        if (address == SIM_EXIT_KIND) {
            exit_kind = load_word(SIM_EXIT_KIND);
            exit_status = load_word(SIM_EXIT_STATUS);
            exit_mcause = load_word(SIM_EXIT_MCAUSE);
            exited = true;
            sc_core::sc_stop();
        }
    }
};

bool read_symbol(const std::string& path, const std::string& wanted,
                 std::uint64_t& value)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(file)),
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
            const char* name =
                reinterpret_cast<const char*>(image.data() + stroff + rd32(sym));
            if (wanted == name) {
                value = rd32(sym + 4);
                return true;
            }
        }
    }
    return false;
}

// Must match `UNMAPPED_BASE` and `UNMAPPED_GENERIC` in conf_main.c. The vector
// probes straddle the end of memory instead, so they are refused at
// `kMemorySize`.
constexpr std::uint64_t kUnmappedBase = 0x0040'0000ull;
constexpr std::uint64_t kUnmappedGeneric = 0x0050'0000ull;

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: test_conformance <rvv_conf.elf> [protocol-error]\n";
        return 2;
    }
    const std::string elf_path = argv[1];
    const bool protocol_error_mode =
        argc == 3 && std::string(argv[2]) == "protocol-error";
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

    probe_memory mem("mem", kMemorySize);
    mem.watch_begin = vsrc;
    mem.watch_end = vsrc + 4 * 16;
    mem.generic_error_address = kUnmappedGeneric;
    mem.protocol_error_mode = protocol_error_mode;

    cdc::cpu::cpu_config config;
    config.hart_id = 0;
    cdc::cpu::riscv_vp_plusplus_cpu cpu("core", config);
    cpu.data_bus().bind(mem.tsock);
    cpu.load_elf(elf_path);

    if (protocol_error_mode) {
        // The model must refuse to turn a malformed transaction into a guest
        // fault. Anything else — a trap, or a clean run — means a protocol
        // error is being laundered into something the firmware can catch and
        // ignore, which is how an integration bug becomes invisible.
        std::string message;
        try {
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));
        } catch (const std::exception& error) {
            message = error.what();
        }
        std::cout << "D13 protocol-error policy\n"
                  << "  malformed response delivered : "
                  << (mem.protocol_error_delivered ? "yes" : "no") << '\n'
                  << "  model reported               : "
                  << (message.empty() ? "(nothing)" : message) << '\n';
        if (!mem.protocol_error_delivered) {
            std::cerr << "FAIL: the run never reached an out-of-range access, "
                         "so the policy was never exercised\n";
            return 1;
        }
        if (message.find("protocol error") == std::string::npos) {
            std::cerr << "FAIL: a malformed TLM response did not abort the "
                         "model; it must not become a guest fault\n";
            return 1;
        }
        std::cout << "\ntest_conformance: a malformed TLM response aborts "
                     "instead of becoming a guest fault\n";
        return 0;
    }

    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));

    if (!mem.exited) {
        std::cerr << "FAIL: the image never signalled exit; PC 0x" << std::hex
                  << cpu.get_pc() << std::dec << '\n';
        return 1;
    }
    if (mem.exit_kind == SIM_EXIT_KIND_TRAPPED) {
        std::cerr << "FAIL: unhandled trap, mcause=" << mem.exit_mcause << '\n';
        return 1;
    }
    if (mem.exit_status != SIM_EXIT_PASS) {
        std::cerr << "FAIL: conformance check id " << mem.exit_status
                  << " failed (see enum conf_check_id in conf_main.c)\n";
        return 1;
    }

    const auto w = [&](std::uint64_t a) { return mem.load_word(a); };

    std::cout << "D12 — RV32 indexed access with index EEW=64 (RVV 1.0 §18.2)\n"
              << "  encodings trapped   : 0x" << std::hex << w(SIM_D12_TRAP_MASK)
              << std::dec << " (all 32 = 0xffffffff)\n"
              << "  every mcause == 2   : " << w(SIM_D12_MCAUSE_OK) << '\n'
              << "  vstart preserved    : " << w(SIM_D12_VSTART_KEPT) << '\n'
              << "  vd preserved        : " << w(SIM_D12_VD_KEPT) << '\n'
              << "  mstatus.VS after all 32 runs from VS=Clean : "
              << w(SIM_D12_VS_KEPT) << " (2 = all Clean, 3 = at least one Dirty)\n"
              << "  bus accesses to the operand buffer during D12 : "
              << mem.watched_at_d12_end << '\n';

    // All 32, not just the four unit forms: the restriction is on the index
    // EEW, so `v[sl][ou]xseg[2-8]ei64.v` are equally illegal at XLEN=32.
    CHECK(w(SIM_D12_TRAP_MASK) == 0xffffffffu,
          "not every index-EEW=64 encoding raised an illegal instruction "
          "(bit n = the nth encoding, unit forms first, then seg2..seg8)");
    CHECK(w(SIM_D12_MCAUSE_OK) == 1u, "an encoding trapped with the wrong cause");
    CHECK(w(SIM_D12_VSTART_KEPT) == 1u,
          "vstart was modified by an instruction that never executed");
    CHECK(w(SIM_D12_VD_KEPT) == 1u,
          "the destination vector register was modified by an illegal instruction");
    // The sharpest of the five: `prepInstr()` sets VS to Dirty, so this is what
    // distinguishes the fix at the decode site from one inside vLoadStore().
    CHECK(w(SIM_D12_VS_KEPT) == 2u,
          "mstatus.VS was dirtied by at least one illegal encoding — its check "
          "is placed after prepInstr()");
    // Only the host can see this one.
    CHECK(mem.d12_phase_begun && mem.d12_phase_marked,
          "the D12 probe window was never delimited");
    CHECK(mem.watched_at_d12_end == 0,
          "an illegal instruction issued a bus request");

    std::cout << "\nD13 — bus error becomes an access fault, by access origin\n";
    struct expectation {
        const char* what;
        std::uint64_t mcause_addr;
        std::uint64_t mtval_addr;
        std::uint32_t want;
    };
    const expectation expectations[] = {
        {"instruction fetch", SIM_D13_FETCH_MCAUSE, SIM_D13_FETCH_MTVAL, 1},
        {"scalar load", SIM_D13_LOAD_MCAUSE, SIM_D13_LOAD_MTVAL, 5},
        {"scalar store", SIM_D13_STORE_MCAUSE, SIM_D13_STORE_MTVAL, 7},
        {"vector load", SIM_D13_VLOAD_MCAUSE, SIM_D13_VLOAD_MTVAL, 5},
        {"vector store", SIM_D13_VSTORE_MCAUSE, SIM_D13_VSTORE_MTVAL, 7},
        {"AMO", SIM_D13_AMO_MCAUSE, SIM_D13_AMO_MTVAL, 7},
        {"generic refusal", SIM_D13_GENERIC_MCAUSE, SIM_D13_GENERIC_MTVAL, 5},
    };
    for (const auto& e : expectations) {
        const std::uint32_t got = w(e.mcause_addr);
        std::cout << "  " << e.what << std::string(20 - std::min<std::size_t>(20, std::strlen(e.what)), ' ')
                  << ": mcause " << got << " (want " << e.want << ")  mtval 0x"
                  << std::hex << w(e.mtval_addr) << std::dec << '\n';
        CHECK(got == e.want, e.what);
        // A page fault here is the exact defect D13 fixes, so name it rather
        // than reporting a bare number mismatch.
        CHECK(got != 12u && got != 13u && got != 15u,
              "a page fault was reported on a core running with satp.MODE = Bare");
    }
    std::cout << "  vector load  vstart : " << w(SIM_D13_VLOAD_VSTART)
              << " (want 4)\n"
              << "  vector store vstart : " << w(SIM_D13_VSTORE_VSTART)
              << " (want 4)\n"
              << "  addresses the memory refused : " << mem.refused.size() << '\n';
    CHECK(w(SIM_D13_VLOAD_VSTART) == 4u,
          "the faulting vector load did not name the element it stopped at");
    CHECK(w(SIM_D13_VSTORE_VSTART) == 4u,
          "the faulting vector store did not name the element it stopped at");

    // Ties the causes to the accesses that produced them: six probes, six
    // refusals, all at the address the firmware aimed at. Without this a stray
    // access elsewhere could have produced a plausible-looking fault.
    CHECK(mem.refused.size() == 7,
          "expected exactly seven refused accesses, one per D13 probe");
    unsigned at_unmapped = 0;
    unsigned at_ram_end = 0;
    unsigned at_generic = 0;
    for (std::uint64_t address : mem.refused) {
        if (address == kUnmappedBase) {
            ++at_unmapped;
        } else if (address == kMemorySize) {
            ++at_ram_end;
        } else if (address == kUnmappedGeneric) {
            ++at_generic;
        } else {
            CHECK(false, "a refused access was at none of the probe addresses");
        }
    }
    // Four probes aim at an arbitrary unmapped address, two vector probes
    // straddle the end of memory so that element 4 is the first to fail, and
    // one probes the address the harness refuses with a generic error.
    CHECK(at_unmapped == 4, "expected four refusals at the unmapped probe address");
    CHECK(at_ram_end == 2, "expected two refusals at the end of RAM");
    CHECK(at_generic == 1, "expected one refusal at the generic-error address");

    if (failures != 0) {
        std::cerr << '\n' << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_conformance: D12 and D13 both hold\n";
    return 0;
}
