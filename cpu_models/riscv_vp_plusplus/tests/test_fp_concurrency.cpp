// SPDX-License-Identifier: Apache-2.0
//
// F5 concurrency control — is SoftFloat's global arithmetic state shared
// between harts in a way that corrupts results?
//
// The backend unit test already proves the state *is* process-global. This one
// answers the question that actually matters: whether two harts running vector
// FP with different rounding modes can corrupt each other.
//
// Three things make it a real test rather than a hopeful one:
//
//   * **the operands cannot agree.** 1.0f + 1.5*2^-24 sits 0.75 ulp above 1.0,
//     so round-to-nearest gives 0x3f800001 and round-toward-zero gives
//     0x3f800000. A hart that picks up the other's mode computes a visibly
//     different number, not a rounding-invisible one;
//   * **the interleaving is forced.** The TLM global quantum is set to a few
//     ISS cycles before the cores are built, so each hart yields every handful
//     of instructions instead of running its 200 iterations to completion
//     first. Sequential execution would pass trivially;
//   * **both creation orders are run**, as separate CTest cases. Creation order
//     decides SystemC scheduling order, and state left behind by whichever hart
//     ran first is exactly the failure mode being hunted.
//
// If this fails, the fix is to save and restore the SoftFloat globals around
// each ISS run slice in the wrapper. It is deliberately not implemented in
// advance: a mitigation with no demonstrated failure is a guess, and it would
// mask the evidence that justifies it.
//
// Usage: test_fp_concurrency <rvv_fp.elf> [swapped]
// Exit codes: 0 pass, 1 fail, 77 skip.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
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

/// Counts how often the accessing hart changes.
///
/// Without this the test could pass by never interleaving at all — core A runs
/// its 200 iterations, then core B runs its 200, and neither ever sees the
/// other's rounding mode. A green result would then be evidence of nothing.
/// The switch count turns "they interleaved" from an assumption into a
/// measurement.
struct interleave_tracker {
    int last_hart = -1;
    unsigned long switches = 0;
    unsigned long accesses[2] = {0, 0};

    void record(unsigned hart_index)
    {
        ++accesses[hart_index];
        const int hart = static_cast<int>(hart_index);
        if (last_hart != -1 && last_hart != hart) {
            ++switches;
        }
        last_hart = hart;
    }
};

class hart_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<hart_memory> tsock;
    std::vector<unsigned char> storage;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;
    std::uint32_t fp_hart = 0;
    std::uint32_t fp_frm = 0;
    std::uint32_t fp_expected = 0;
    std::uint32_t fp_iterations = 0;
    std::uint32_t result_mismatches = 0;
    std::uint32_t flag_mismatches = 0;
    std::uint32_t frm_mismatches = 0;
    std::uint32_t first_bad_value = 0;
    std::uint32_t first_bad_flags = 0;
    std::uint32_t last_result = 0;

    std::function<void()> on_exit;
    interleave_tracker* tracker = nullptr;
    unsigned tracker_index = 0;

    hart_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &hart_memory::b_transport);
        tsock.register_transport_dbg(this, &hart_memory::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        if (tracker != nullptr) {
            tracker->record(tracker_index);
        }
        if (!copy(trans)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND
            && trans.get_address() == SIM_EXIT_KIND) {
            capture();
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return copy(trans) ? trans.get_data_length() : 0;
    }

    void capture()
    {
        exit_kind = load_word(SIM_EXIT_KIND);
        exit_status = load_word(SIM_EXIT_STATUS);
        fp_hart = load_word(SIM_FP_HART);
        fp_frm = load_word(SIM_FP_FRM);
        fp_expected = load_word(SIM_FP_EXPECTED);
        fp_iterations = load_word(SIM_FP_ITERATIONS);
        result_mismatches = load_word(SIM_FP_RESULT_MISMATCHES);
        flag_mismatches = load_word(SIM_FP_FLAG_MISMATCHES);
        frm_mismatches = load_word(SIM_FP_FRM_MISMATCHES);
        first_bad_value = load_word(SIM_FP_FIRST_BAD_VALUE);
        first_bad_flags = load_word(SIM_FP_FIRST_BAD_FLAGS);
        last_result = load_word(SIM_FP_LAST_RESULT);
        exited = true;
        if (on_exit) {
            on_exit();
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

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: test_fp_concurrency <rvv_fp.elf> [swapped]\n";
        return 2;
    }
    const std::string elf_path = argv[1];
    const bool swapped = argc == 3 && std::string(argv[2]) == "swapped";
    {
        std::ifstream probe(elf_path, std::ios::binary);
        if (!probe) {
            std::cerr << "SKIP: " << elf_path << " was not built\n";
            return kSkip;
        }
    }

    // Force interleaving. 40 ns is four ISS cycles, so each hart yields every
    // few instructions rather than running its whole loop first. It must stay a
    // multiple of the 10 ns cycle time or the wrapper's quantum guard refuses
    // it. Set before the cores exist: the ISS reads it at construction.
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(40, sc_core::SC_NS));

    // Hart 0 uses round-toward-zero, hart 1 round-to-nearest; the image derives
    // that from `mhartid`. Swapping the creation order swaps which of them
    // SystemC schedules first, which is the point of the second case.
    std::uint32_t hart_ids[2] = {0, 1};
    if (swapped) {
        hart_ids[0] = 1;
        hart_ids[1] = 0;
    }

    interleave_tracker tracker;
    std::vector<std::unique_ptr<hart_memory>> memories;
    std::vector<std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu>> cpus;
    std::size_t exited_count = 0;

    for (std::size_t i = 0; i < 2; ++i) {
        const std::string suffix = std::to_string(hart_ids[i]);
        memories.push_back(std::make_unique<hart_memory>(
            sc_core::sc_module_name(("mem" + suffix).c_str()), kMemorySize));
        memories.back()->tracker = &tracker;
        memories.back()->tracker_index = static_cast<unsigned>(i);
        memories.back()->on_exit = [&exited_count] {
            if (++exited_count == 2) {
                sc_core::sc_stop();
            }
        };

        cdc::cpu::cpu_config config;
        config.hart_id = hart_ids[i];
        cpus.push_back(std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>(
            sc_core::sc_module_name(("core" + suffix).c_str()), config));
        cpus.back()->data_bus().bind(memories.back()->tsock);
        cpus.back()->load_elf(elf_path);
    }

    std::cout << "creation order       : hart " << hart_ids[0] << " then hart "
              << hart_ids[1] << (swapped ? "   [swapped]" : "   [normal]")
              << '\n'
              << "global quantum       : "
              << tlm::tlm_global_quantum::instance().get() << '\n';

    sc_core::sc_start(sc_core::sc_time(50, sc_core::SC_MS));

    std::cout << "\ninterleaving\n"
              << "  hart " << hart_ids[0] << " accesses    : "
              << tracker.accesses[0] << '\n'
              << "  hart " << hart_ids[1] << " accesses    : "
              << tracker.accesses[1] << '\n'
              << "  hart switches      : " << tracker.switches << '\n';

    // A pass means nothing if the two never ran concurrently. 200 iterations
    // each, yielding every four cycles, should produce switches in the
    // thousands; anything in single digits means one hart effectively ran to
    // completion first and the test did not exercise what it claims to.
    if (tracker.switches < 100) {
        std::cerr << "FAIL: only " << tracker.switches
                  << " hart switches — the harts did not meaningfully "
                     "interleave, so a clean result proves nothing about "
                     "SoftFloat state sharing\n";
        ++failures;
    }

    for (std::size_t i = 0; i < 2; ++i) {
        const auto& mem = *memories[i];
        std::cout << "\n--- hart " << hart_ids[i] << " ---\n"
                  << "  frm                : " << mem.fp_frm
                  << (mem.fp_frm == 1 ? "  (toward zero)" : "  (nearest-even)")
                  << '\n'
                  << "  expected result    : 0x" << std::hex << mem.fp_expected
                  << "   last: 0x" << mem.last_result << std::dec << '\n'
                  << "  iterations         : " << mem.fp_iterations << '\n'
                  << "  result mismatches  : " << mem.result_mismatches << '\n'
                  << "  fflags mismatches  : " << mem.flag_mismatches << '\n'
                  << "  frm readback bad   : " << mem.frm_mismatches << '\n';
        if (mem.result_mismatches != 0 || mem.flag_mismatches != 0) {
            std::cout << "  first bad value    : 0x" << std::hex
                      << mem.first_bad_value << "   flags 0x"
                      << mem.first_bad_flags << std::dec << '\n';
        }

        if (!mem.exited) {
            std::cerr << "FAIL: hart " << hart_ids[i]
                      << " never signalled exit\n";
            ++failures;
            continue;
        }
        if (mem.exit_kind == SIM_EXIT_KIND_TRAPPED) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " trapped\n";
            ++failures;
            continue;
        }

        // The leak, if it exists, shows here: a hart computing the *other*
        // mode's answer.
        if (mem.result_mismatches != 0) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " produced "
                      << mem.result_mismatches
                      << " results for the wrong rounding mode. SoftFloat's "
                         "global rounding state leaked between harts; the "
                         "wrapper must save and restore it around each ISS run "
                         "slice (audit F5).\n";
            ++failures;
        }
        if (mem.flag_mismatches != 0) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " saw "
                      << mem.flag_mismatches
                      << " unexpected fflags values (first 0x" << std::hex
                      << mem.first_bad_flags << std::dec
                      << "). softfloat_exceptionFlags accumulates and is shared "
                         "between harts (audit F5).\n";
            ++failures;
        }
        if (mem.frm_mismatches != 0) {
            std::cerr << "FAIL: hart " << hart_ids[i]
                      << " read back a frm it did not set\n";
            ++failures;
        }
        if (mem.exit_status != SIM_EXIT_PASS) {
            std::cerr << "FAIL: hart " << hart_ids[i] << " check id "
                      << mem.exit_status << " (see enum fp_check_id)\n";
            ++failures;
        }
    }

    // Both harts really ran, and really used different modes — otherwise the
    // whole test would pass by not testing anything.
    if (memories.size() == 2) {
        if (memories[0]->fp_frm == memories[1]->fp_frm) {
            std::cerr << "FAIL: both harts used the same rounding mode, so no "
                         "leak could have been detected\n";
            ++failures;
        }
        if (memories[0]->fp_expected == memories[1]->fp_expected) {
            std::cerr << "FAIL: the two modes produce the same expected value; "
                         "the operands do not discriminate\n";
            ++failures;
        }
        if (memories[0]->fp_iterations == 0 || memories[1]->fp_iterations == 0) {
            std::cerr << "FAIL: a hart ran no iterations\n";
            ++failures;
        }
    }

    if (failures != 0) {
        std::cerr << '\n' << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_fp_concurrency: two harts, different rounding modes, "
                 "interleaved — no SoftFloat state leak observed\n";
    return 0;
}
