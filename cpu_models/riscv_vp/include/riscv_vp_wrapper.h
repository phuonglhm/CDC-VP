#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <cdc/cpu/cpu_base.h>

// Forward declarations keep Bremen's headers (and their include paths + boost)
// out of the public interface — platforms only need cpu_base + this header.
namespace rv32 {
struct ISS;
struct CombinedMemoryInterface;
struct DirectCoreRunner;
}  // namespace rv32

namespace cdc::cpu {

// cpu_base wrapper around the Bremen riscv-vp rv32 ISS (single-core).
//
// Bremen's CombinedMemoryInterface exposes ONE combined instruction+data TLM
// socket, so instr_bus() and data_bus() return the same socket and
// has_unified_bus() is true. The ELF image is loaded (backdoor, via the bus) and
// the ISS is init()'d in start_of_simulation(), after bindings resolve.
class riscv_vp_cpu : public cpu_base {
public:
    explicit riscv_vp_cpu(sc_core::sc_module_name name,
                          const cpu_config& config = cpu_config{});
    ~riscv_vp_cpu() override;

    tlm::tlm_initiator_socket<>& instr_bus() override;
    tlm::tlm_initiator_socket<>& data_bus() override;
    bool has_unified_bus() const override { return true; }

    void set_irq(unsigned cause, bool level) override;

    void load_elf(const std::string& path) override;
    void reset_cpu() override;
    std::uint64_t get_pc() const override;
    std::string backend_name() const override;
    std::uint64_t get_instret() const override;

private:
    void start_of_simulation() override;

    struct impl;                 // owns the Bremen ISS + mem interface + runner
    std::unique_ptr<impl> impl_;
    std::string elf_path_;
    std::uint64_t entry_pc_ = 0;
};

}  // namespace cdc::cpu
