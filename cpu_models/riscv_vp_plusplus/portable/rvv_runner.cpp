// SPDX-License-Identifier: Apache-2.0
//
// `rvv_runner` — the RV32GCV backend inside a portable executable.
//
// Plan §11.2's gate asks for two things the unit tests do not give:
//
//   * the backend statically linked into a portable test executable;
//   * that executable having no runtime dependency on a VP++ or Spike binary,
//     Qt, VNC, the source tree or the build tree.
//
// The unit tests link the backend but are built with the host's absolute
// SystemC path in their RPATH, so they stop working the moment `/opt` differs.
// The packaged platform is portable but does not link the backend yet, because
// no core is instantiated before Phase 5. Neither one demonstrates the gate, so
// this does: a minimal harness — flat memory, one hart, the exit protocol —
// built with `cdc_make_portable`, packaged with its SystemC libraries beside
// it, and executed from a directory that contains nothing else.
//
// Deliberately not a test binary. `run_portable_check.cmake` copies exactly
// this executable, its sibling `.so` files and one ELF into an empty directory
// and runs it there, so anything it silently depended on would be missing.
//
// Usage: rvv_runner <image.elf> [memory-bytes]
// Exit codes: 0 the image reported success, 1 otherwise.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

#include "riscv_vp_plusplus_wrapper.h"

namespace {

// Mirrors `fw/TPU_V3_SoC/rvv_smoke/sim_exit.h`. Duplicated rather than included
// on purpose: this executable must not depend on the firmware source tree at
// build time any more than it does at run time. The runner only needs the
// trigger and the two words beside it, and `rvv_smoke_execution` is what checks
// the full protocol against the header.
constexpr std::uint64_t kExitBase = 0x000F'0000;
constexpr std::uint64_t kExitKind = kExitBase + 0;
constexpr std::uint64_t kExitStatus = kExitBase + 4;

class flat_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<flat_memory> tsock;
    std::vector<unsigned char> storage;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;
    unsigned long requests = 0;

    flat_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &flat_memory::b_transport);
        tsock.register_transport_dbg(this, &flat_memory::transport_dbg);
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
        ++requests;

        if (address >= storage.size() || length > storage.size() - address) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + address, trans.get_data_ptr(), length);
            if (address == kExitKind) {
                exit_kind = load_word(kExitKind);
                exit_status = load_word(kExitStatus);
                exited = true;
                sc_core::sc_stop();
            }
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
};

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: rvv_runner <image.elf> [memory-bytes]\n";
        return 2;
    }
    const std::string elf_path = argv[1];
    const std::size_t memory_bytes =
        argc == 3 ? std::strtoul(argv[2], nullptr, 0) : 1024 * 1024;

    {
        std::ifstream probe(elf_path, std::ios::binary);
        if (!probe) {
            std::cerr << "rvv_runner: cannot open " << elf_path << '\n';
            return 1;
        }
    }

    flat_memory mem("mem", memory_bytes);

    cdc::cpu::cpu_config config;
    config.hart_id = 0;
    cdc::cpu::riscv_vp_plusplus_cpu cpu("core", config);
    cpu.data_bus().bind(mem.tsock);
    cpu.load_elf(elf_path);

    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));

    std::cout << "rvv_runner: instret " << cpu.get_instret() << ", TLM requests "
              << mem.requests << ", exit kind " << mem.exit_kind << ", status "
              << mem.exit_status << '\n';

    if (!mem.exited) {
        std::cerr << "rvv_runner: image did not signal exit\n";
        return 1;
    }
    if (mem.exit_kind != 0 || mem.exit_status != 0) {
        std::cerr << "rvv_runner: image reported failure\n";
        return 1;
    }
    std::cout << "rvv_runner: PASS\n";
    return 0;
}
