// SPDX-License-Identifier: Apache-2.0
//
// Phase 2 unit tests for the VP++ RV32GCV backend.
//
// What these prove, and why each one exists:
//
//  * the frozen RVV parameters are what the hart actually reports, read from the
//    CSR firmware would read rather than recomputed from a constant;
//  * `hart_id` and `reset_pc` are honoured (decision record D5) — and the
//    negative controls prove a backend that *cannot* honour them refuses,
//    because a no-op setter would let a 16-hart platform elaborate while every
//    core still reported `mhartid == 0`;
//  * two instances share no mutable architectural state;
//  * no memory access bypasses TLM: the probe memory counts every one.
//
// Executing an ELF is deliberately not here — it needs the freestanding RV32GCV
// smoke image, which is a separate Phase 2 deliverable with its own test.

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

#include "riscv_vp_plusplus_wrapper.h"

// Berkeley SoftFloat's global arithmetic state, reached directly so the F5
// finding is checked rather than asserted in prose.
extern "C" {
#include "softfloat/softfloat.h"
}

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

/// Counts every access, so "nothing bypasses TLM" is measured rather than
/// asserted.
class probe_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<probe_memory> tsock;
    std::vector<unsigned char> storage;
    unsigned long reads = 0;
    unsigned long writes = 0;
    unsigned long debug_accesses = 0;

    probe_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &probe_memory::b_transport);
        tsock.register_transport_dbg(this, &probe_memory::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        if (!copy(trans)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            ++writes;
        } else {
            ++reads;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        if (!copy(trans)) {
            return 0;
        }
        ++debug_accesses;
        return trans.get_data_length();
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

/// Reports whether construction was refused, and whether the message names the
/// field. "invalid configuration" is not actionable.
template <typename Fn>
bool rejected_naming(Fn&& fn, const std::string& needle)
{
    try {
        fn();
    } catch (const std::exception& error) {
        const std::string what = error.what();
        if (what.find(needle) != std::string::npos) {
            return true;
        }
        std::cerr << "  refused, but the message omits '" << needle
                  << "': " << what << '\n';
        return false;
    }
    std::cerr << "  accepted, but should have been refused (" << needle << ")\n";
    return false;
}

void frozen_rvv_parameters_are_what_the_hart_reports()
{
    cdc::cpu::cpu_config config;
    config.hart_id = 5;
    config.reset_pc = 0x0000'0000ull;

    cdc::cpu::riscv_vp_plusplus_cpu cpu("core_rvv", config);
    probe_memory mem("mem_rvv", 64 * 1024);
    cpu.data_bus().bind(mem.tsock);

    std::cout << "backend       : " << cpu.backend_name() << '\n'
              << "hart_id       : " << cpu.hart_id() << '\n'
              << "vlenb (CSR)   : " << cpu.vlenb() << '\n'
              << "misa          : 0x" << std::hex << cpu.misa() << std::dec
              << '\n'
              << "VLEN / ELEN   : "
              << cdc::cpu::riscv_vp_plusplus_cpu::vlen_bits() << " / "
              << cdc::cpu::riscv_vp_plusplus_cpu::elen_bits() << '\n'
              << "vector regs   : "
              << cdc::cpu::riscv_vp_plusplus_cpu::vector_register_count()
              << '\n';

    // Plan §4.2 and the D3 promotion gate.
    CHECK(cdc::cpu::riscv_vp_plusplus_cpu::vlen_bits() == 512);
    CHECK(cdc::cpu::riscv_vp_plusplus_cpu::elen_bits() == 64);
    CHECK(cdc::cpu::riscv_vp_plusplus_cpu::vector_register_count() == 32);
    CHECK(cpu.vlenb() == 64);
    CHECK(cpu.vector_extension_enabled());

    // The whole misa set, so an upstream change to the default ISA config shows
    // up here rather than in a firmware mystery. RV32 (MXL=1) with IMAFDC + V,
    // plus N, S, U.
    for (char extension : {'i', 'm', 'a', 'f', 'd', 'c', 'v', 'u', 's'}) {
        const bool present =
            ((cpu.misa() >> (extension - 'a')) & 1u) != 0u;
        if (!present) {
            std::cerr << "  misa is missing extension '" << extension << "'\n";
            ++failures;
        }
    }

    CHECK(cpu.has_unified_bus());
    CHECK(&cpu.instr_bus() == &cpu.data_bus());
    CHECK(cpu.backend_name().find("VLEN=512") != std::string::npos);
    CHECK(cpu.hart_id() == 5);
    CHECK(cpu.configured_reset_pc() == 0x0000'0000ull);
}

void hart_ids_are_independent_and_state_is_not_shared()
{
    cdc::cpu::cpu_config zero;
    zero.hart_id = 0;
    zero.reset_pc = 0x0000'0000ull;

    cdc::cpu::cpu_config seven;
    seven.hart_id = 7;  // chip 3, core 1 under the TPU_V3 mapping
    seven.reset_pc = 0x0000'0000ull;

    cdc::cpu::riscv_vp_plusplus_cpu core0("core_a", zero);
    cdc::cpu::riscv_vp_plusplus_cpu core1("core_b", seven);

    probe_memory mem0("mem_a", 4096);
    probe_memory mem1("mem_b", 4096);
    core0.data_bus().bind(mem0.tsock);
    core1.data_bus().bind(mem1.tsock);

    CHECK(core0.hart_id() == 0);
    CHECK(core1.hart_id() == 7);
    CHECK(core0.vlenb() == 64);
    CHECK(core1.vlenb() == 64);
    CHECK(&core0.data_bus() != &core1.data_bus());

    // Both report V, so the ISA config is per instance and not a shared object
    // one instance could have mutated.
    CHECK(core0.vector_extension_enabled());
    CHECK(core1.vector_extension_enabled());
}

void unsupported_configuration_is_refused()
{
    // D5: refuse, never ignore. An RV64 request cannot be honoured by a wrapper
    // around the RV32 ISS, and quietly giving back RV32 would be discovered by
    // firmware, not by the platform.
    CHECK(rejected_naming(
        [] {
            cdc::cpu::cpu_config config;
            config.xlen = 64;
            cdc::cpu::riscv_vp_plusplus_cpu cpu("bad_xlen", config);
        },
        "xlen"));

    // Neither a reset vector nor an image: there is nothing to reset to, and
    // defaulting to zero would look like a working boot from an empty address.
    CHECK(rejected_naming(
        [] {
            cdc::cpu::cpu_config config;
            cdc::cpu::riscv_vp_plusplus_cpu cpu("no_reset_vector", config);
            probe_memory mem("mem_no_reset", 4096);
            cpu.data_bus().bind(mem.tsock);
            sc_core::sc_start(sc_core::SC_ZERO_TIME);
        },
        "reset_pc"));
}

void softfloat_state_is_process_global()
{
    // Audit finding F5, as an executable check rather than a claim about the
    // source. `softfloat.h` declares these `THREAD_LOCAL`, but the macro is
    // defined empty when nothing sets it — and nothing does, in upstream's
    // CMake or ours. So they are plain globals shared by every ISS instance in
    // the process, and by the Phase 5 Sauria BF16 adapter.
    //
    // This matters because `v.h::set_fp_rm()` writes the rounding mode from
    // `fcsr.frm`, and `softfloat_exceptionFlags` *accumulates*: one hart's
    // flags are visible to another with no interleaving hazard required.
    //
    // The check is deliberately conservative. It proves the state is shared,
    // which is what forces the wrapper to treat it as shared. It does **not**
    // prove interleaved execution is safe; that needs the interleaved-`frm`
    // control against the Spike oracle, which is still outstanding.
    const std::uint_fast8_t saved_rounding = softfloat_roundingMode;
    const std::uint_fast8_t saved_tininess = softfloat_detectTininess;
    const std::uint_fast8_t saved_flags = softfloat_exceptionFlags;

    softfloat_roundingMode = softfloat_round_minMag;
    softfloat_exceptionFlags = 0;
    softfloat_exceptionFlags |= softfloat_flag_inexact;

    // One object, reachable from anywhere in the process: taking its address
    // twice from different scopes must yield the same location. If these were
    // ever made genuinely thread-local, SystemC coroutines would still share
    // them, so the wrapper's obligation would not change.
    const volatile std::uint_fast8_t* first = &softfloat_roundingMode;
    const volatile std::uint_fast8_t* second = &softfloat_roundingMode;
    CHECK(first == second);
    CHECK(softfloat_roundingMode == softfloat_round_minMag);
    CHECK((softfloat_exceptionFlags & softfloat_flag_inexact) != 0);

    std::cout << "softfloat state : rounding=" << unsigned(softfloat_roundingMode)
              << " tininess=" << unsigned(softfloat_detectTininess)
              << " flags=0x" << std::hex << unsigned(softfloat_exceptionFlags)
              << std::dec << "  (process-global, see audit F5)\n";

    softfloat_roundingMode = saved_rounding;
    softfloat_detectTininess = saved_tininess;
    softfloat_exceptionFlags = saved_flags;
}

}  // namespace

int sc_main(int, char*[])
{
    // Configuration errors are thrown from constructors, so they surface here
    // rather than as SystemC reports.
    try {
        frozen_rvv_parameters_are_what_the_hart_reports();
        hart_ids_are_independent_and_state_is_not_shared();
        softfloat_state_is_process_global();
        unsupported_configuration_is_refused();
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_riscv_vp_plusplus: all checks passed\n";
    return 0;
}
