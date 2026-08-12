// SPDX-License-Identifier: Apache-2.0
//
// Runs the freestanding RV32GCV smoke image through `cdc::cpu::riscv_vp_plusplus`
// and observes the traffic it produces.
//
// This is the Phase 2 gate item that the unit test cannot cover. The unit test
// shows memory access is *routed* to a TLM socket; this one shows there is
// actually traffic on it — instruction fetch, scalar data, and vector data —
// and that a full RVV program executes correctly through it.
//
// Two things it deliberately does not do:
//
//  * it does not assume a vector transaction granularity. VP++ decomposes
//    vector accesses per active element (decision record D7), so the counters
//    here are named for TLM requests and the vector evidence is "the vector
//    buffers were accessed", not "a 64-byte transaction occurred";
//  * it does not run without a watchdog. `sim_exit` spins after signalling, so
//    a harness that failed to stop would hang the suite rather than fail it.
//
// Usage: test_rvv_smoke <rvv_smoke.elf>
// Exit codes: 0 pass, 1 fail, 77 skip (image not built — no cross toolchain).

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <memory>
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

constexpr std::uint64_t kMemorySize = 1024 * 1024;   // matches link.ld
constexpr std::uint64_t kVectorDataBase = 0x0000'1000;  // refined below
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

/// Flat memory that records what the CPU did to it, and ends the simulation
/// when the image signals its exit.
class observed_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<observed_memory> tsock;

    std::vector<unsigned char> storage;

    // Named for what they measure, per D7: these count TLM requests, not vector
    // instructions and not hardware bus transactions.
    unsigned long tlm_request_count = 0;
    unsigned long read_requests = 0;
    unsigned long write_requests = 0;
    unsigned long transferred_bytes = 0;
    unsigned long error_count = 0;
    unsigned long debug_requests = 0;

    /// Requests landing in the image's `.vdata` block — the buffers only the
    /// vector instructions touch.
    unsigned long vector_buffer_requests = 0;
    std::uint64_t vector_region_begin = 0;
    std::uint64_t vector_region_end = 0;

    /// Distinct payload sizes seen, so the element-wise granularity is measured
    /// rather than assumed.
    std::vector<unsigned long> requests_by_size = std::vector<unsigned long>(65, 0);

    /// Simulated time at the first workload request. The F11 gate needs it:
    /// a bogus cycle baseline is charged *before* the first fetch, so it shows
    /// up here even though every functional check still passes.
    bool saw_first_request = false;
    sc_core::sc_time first_request_time = sc_core::SC_ZERO_TIME;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;
    std::uint32_t exit_mcause = 0;
    std::uint32_t exit_mepc = 0;
    std::uint32_t mcycle_start = 0;
    std::uint32_t mcycle_mid = 0;
    std::uint32_t mcycle_end = 0;

    /// Called when this memory sees its image signal exit, so the harness can
    /// stop only once *every* hart has finished.
    std::function<void()> on_exit;

    observed_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &observed_memory::b_transport);
        tsock.register_transport_dbg(this, &observed_memory::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();

        ++tlm_request_count;
        if (!saw_first_request) {
            saw_first_request = true;
            first_request_time = sc_core::sc_time_stamp();
        }
        if (length < requests_by_size.size()) {
            ++requests_by_size[length];
        }
        if (vector_region_end > vector_region_begin
            && address >= vector_region_begin && address < vector_region_end) {
            ++vector_buffer_requests;
        }

        if (!copy(trans)) {
            ++error_count;
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        transferred_bytes += length;

        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            ++write_requests;
            capture_exit(address);
        } else {
            ++read_requests;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        // Debug traffic is the loader, not workload traffic, so it is counted
        // separately and never folded into `tlm_request_count`.
        if (!copy(trans)) {
            return 0;
        }
        ++debug_requests;
        return trans.get_data_length();
    }

    /// The image writes kind last; that write is the trigger.
    void capture_exit(std::uint64_t address)
    {
        if (address == SIM_EXIT_STATUS) {
            exit_status = load_word(SIM_EXIT_STATUS);
        } else if (address == SIM_EXIT_MCAUSE) {
            exit_mcause = load_word(SIM_EXIT_MCAUSE);
        } else if (address == SIM_EXIT_MEPC) {
            exit_mepc = load_word(SIM_EXIT_MEPC);
        } else if (address == SIM_EXIT_KIND) {
            exit_kind = load_word(SIM_EXIT_KIND);
            exit_status = load_word(SIM_EXIT_STATUS);
            exit_mcause = load_word(SIM_EXIT_MCAUSE);
            exit_mepc = load_word(SIM_EXIT_MEPC);
            mcycle_start = load_word(SIM_EXIT_MCYCLE_START);
            mcycle_mid = load_word(SIM_EXIT_MCYCLE_MID);
            mcycle_end = load_word(SIM_EXIT_MCYCLE_END);
            exited = true;
            if (on_exit) {
                on_exit();
            }
        }
    }

    std::uint32_t load_word(std::uint64_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, storage.data() + address, sizeof(value));
        return value;
    }

    bool copy(tlm::tlm_generic_payload& trans)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        if (address > storage.size() || length > storage.size() - address) {
            return false;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + address, trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + address, length);
        }
        return true;
    }
};

/// Reads `__vdata_start` / `__vdata_end` from the image's symbol table so the
/// vector-traffic check is anchored to the actual link, not to a guess.
bool read_vdata_range(const std::string& path, std::uint64_t& begin,
                      std::uint64_t& end)
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
        if (sh + 40 > image.size()) {
            break;
        }
        if (rd32(sh + 4) != 2u) {  // SHT_SYMTAB
            continue;
        }
        // Elf32_Shdr: name(0) type(4) flags(8) addr(12) offset(16) size(20)
        // link(24) info(28) addralign(32) entsize(36). `sh_link` on a SYMTAB
        // names its string table.
        const std::uint32_t symoff = rd32(sh + 16);
        const std::uint32_t symsize = rd32(sh + 20);
        const std::uint32_t strtab_index = rd32(sh + 24);
        const std::size_t strsh = shoff + strtab_index * shentsize;
        const std::uint32_t stroff = rd32(strsh + 16);

        bool found_begin = false;
        bool found_end = false;
        for (std::uint32_t off = 0; off + 16 <= symsize; off += 16) {
            const std::size_t sym = symoff + off;
            const std::uint32_t name_off = rd32(sym);
            const std::uint32_t value = rd32(sym + 4);
            const char* name =
                reinterpret_cast<const char*>(image.data() + stroff + name_off);
            if (std::strcmp(name, "__vdata_start") == 0) {
                begin = value;
                found_begin = true;
            } else if (std::strcmp(name, "__vdata_end") == 0) {
                end = value;
                found_end = true;
            }
        }
        if (found_begin && found_end) {
            return true;
        }
    }
    return false;
}

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: test_rvv_smoke <rvv_smoke.elf>\n";
        return 2;
    }
    const std::string elf_path = argv[1];

    {
        std::ifstream probe(elf_path, std::ios::binary);
        if (!probe) {
            std::cerr << "SKIP: " << elf_path
                      << " was not built (no RISC-V cross toolchain?)\n";
            return kSkip;
        }
    }

    std::uint64_t vdata_begin = 0;
    std::uint64_t vdata_end = 0;
    if (!read_vdata_range(elf_path, vdata_begin, vdata_end)) {
        std::cerr << "FAIL: could not read __vdata_start/__vdata_end from "
                  << elf_path << "; the vector-traffic check needs them\n";
        return 1;
    }

    // Two harts, as the F11 gate requires. They run the same image in separate
    // memories: the point is that two ISS instances coexist and each keeps its
    // own cycle baseline, not that they cooperate. Hart 7 is chip 3 core 1
    // under the TPU_V3 mapping, so the id is a realistic one rather than 1.
    constexpr std::size_t kHarts = 2;
    const std::uint32_t hart_ids[kHarts] = {0, 7};

    std::vector<std::unique_ptr<observed_memory>> memories;
    std::vector<std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu>> cpus;
    std::size_t exited_count = 0;

    for (std::size_t i = 0; i < kHarts; ++i) {
        const std::string suffix = std::to_string(hart_ids[i]);

        memories.push_back(std::make_unique<observed_memory>(
            sc_core::sc_module_name(("mem" + suffix).c_str()), kMemorySize));
        auto& mem = *memories.back();
        mem.vector_region_begin = vdata_begin;
        mem.vector_region_end = vdata_end;
        mem.on_exit = [&exited_count] {
            // Stop only once every hart has finished; stopping on the first
            // would truncate the others and silently weaken the test.
            if (++exited_count == kHarts) {
                sc_core::sc_stop();
            }
        };

        cdc::cpu::cpu_config config;
        config.hart_id = hart_ids[i];
        // Reset vector left unspecified: the ELF entry point is used, which
        // also exercises that path in the wrapper.
        cpus.push_back(std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>(
            sc_core::sc_module_name(("core" + suffix).c_str()), config));
        cpus.back()->data_bus().bind(mem.tsock);
        cpus.back()->load_elf(elf_path);
    }

    // Watchdog: 1 ms of simulated time for a workload that takes ~28 us.
    //
    // The size of this bound is itself part of the F11 gate. Before the
    // approved backport of upstream `b710fa7b`, the ISS banked ~2.29 s of
    // simulated time before its first instruction, and any sane watchdog
    // expired before the core fetched anything. If the defect ever returns,
    // this test fails here rather than quietly passing with a longer bound.
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));

    for (std::size_t i = 0; i < kHarts; ++i) {
        const auto& mem = *memories[i];
        const auto& cpu = *cpus[i];

        std::cout << "\n--- hart " << hart_ids[i] << " ---\n"
                  << "first TLM request at : " << mem.first_request_time << '\n'
                  << "simulated time (end) : " << sc_core::sc_time_stamp() << '\n'
                  << "instructions retired : " << cpu.get_instret() << '\n'
                  << "mcycle start/mid/end : " << mem.mcycle_start << " / "
                  << mem.mcycle_mid << " / " << mem.mcycle_end << '\n'
                  << "tlm_request_count    : " << mem.tlm_request_count
                  << "  (" << mem.read_requests << " read, "
                  << mem.write_requests << " write)\n"
                  << "transferred_bytes    : " << mem.transferred_bytes << '\n'
                  << "error_count          : " << mem.error_count << '\n'
                  << ".vdata requests      : " << mem.vector_buffer_requests
                  << '\n';

        std::cout << "payload sizes seen   :";
        for (std::size_t size = 1; size < mem.requests_by_size.size(); ++size) {
            if (mem.requests_by_size[size] != 0) {
                std::cout << ' ' << size << "B x" << mem.requests_by_size[size];
            }
        }
        std::cout << "   [VP++ element-wise granularity]\n";

        if (!mem.exited) {
            std::cerr << "FAIL: hart " << hart_ids[i]
                      << " never signalled exit within the 1 ms watchdog; PC 0x"
                      << std::hex << cpu.get_pc() << std::dec
                      << ". If simulated time is in seconds, the F11 cycle-"
                         "baseline defect has returned — check that the "
                         "approved backport is applied.\n";
            ++failures;
            continue;
        }

        if (mem.exit_kind == SIM_EXIT_KIND_TRAPPED) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " trapped. mcause=0x"
                      << std::hex << mem.exit_mcause << " mepc=0x"
                      << mem.exit_mepc << std::dec << '\n';
            ++failures;
            continue;
        }

        CHECK(mem.exit_kind == SIM_EXIT_KIND_NORMAL);
        if (mem.exit_status != SIM_EXIT_PASS) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " RVV check id "
                      << mem.exit_status
                      << " failed (see enum check_id in main.c)\n";
            ++failures;
            continue;
        }

        // Traffic actually happened on the socket.
        CHECK(mem.tlm_request_count > 0);
        CHECK(mem.read_requests > 0);
        CHECK(mem.write_requests > 0);
        CHECK(mem.transferred_bytes > 0);
        CHECK(mem.error_count == 0);
        CHECK(mem.debug_requests > 0);
        CHECK(mem.vector_buffer_requests > 0);
        CHECK(cpu.get_instret() > 100);

        // ── P2-5 gate: the ISS-internal caches are off ──────────────────────
        //
        // Every instruction the core retires must have been fetched over the
        // socket, so reads (fetches *plus* data loads) cannot be fewer than
        // instructions retired. `dbbcache` caches decoded basic blocks and
        // elides the fetches that go with them, which breaks that inequality
        // immediately: measured 1006 requests against 1403 retired with the
        // cache on, versus 1849 reads against 1403 retired with it off.
        //
        // This exists because the flags were briefly `true` while F11 was being
        // diagnosed — F11 lives in `dbbcache` — while the comment beside them
        // still read `false`. A contract asserted only in a comment is not
        // asserted. The rule itself is plan §11.2 and `INTERFACE_CONTRACT.md`
        // §9: all CPU traffic traverses CDC-VP TLM.
        CHECK(mem.read_requests >= cpu.get_instret());

        // ── F11 gate ────────────────────────────────────────────────────────
        //
        // The first workload request must not already carry a large simulated
        // time. That is where the defect showed: a bogus cycle delta was
        // charged before the first fetch, so the core slept through it. 1 us is
        // generous for a handful of startup instructions and still four orders
        // of magnitude below the 2.29 s the defect produced.
        CHECK(mem.saw_first_request);
        if (mem.first_request_time > sc_core::sc_time(1, sc_core::SC_US)) {
            std::cerr << "FAIL: hart " << hart_ids[i]
                      << " first TLM request at " << mem.first_request_time
                      << ", which carries a startup time offset. The F11 cycle-"
                         "baseline defect appears to have returned.\n";
            ++failures;
        }

        // mcycle must start sane and increase. Sampled by the firmware through
        // the CSR, because that is the value the defect corrupted and the one
        // firmware would act on.
        if (mem.mcycle_start > 100000u) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " mcycle starts at "
                      << mem.mcycle_start
                      << ", which is not a plausible reset baseline (F11)\n";
            ++failures;
        }
        CHECK(mem.mcycle_mid > mem.mcycle_start);
        CHECK(mem.mcycle_end > mem.mcycle_mid);
    }

    // Both harts ran, and independently.
    CHECK(exited_count == kHarts);
    if (memories.size() == kHarts) {
        CHECK(memories[0]->tlm_request_count > 0);
        CHECK(memories[1]->tlm_request_count > 0);
    }

    if (failures != 0) {
        std::cerr << '\n' << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_rvv_smoke: RV32GCV image executed through TLM on "
              << kHarts << " harts, F11 cycle baseline clean, all checks passed\n";
    return 0;
}
