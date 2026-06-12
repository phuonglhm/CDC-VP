#pragma once

#include <cstdint>
#include <string>

#include <tlm_utils/simple_initiator_socket.h>

#include <cdc/cpu/cpu_base.h>

// Forward declaration keeps mariusmm's headers (and their include paths) out of
// the public interface — platforms only need cpu_base + this header.
namespace riscv_tlm {
class CPURV32;
}

namespace cdc::cpu {

// cpu_base wrapper around the mariusmm RISC-V-TLM core (RV32IMAC, single-core).
//
// Exposes the core's instruction and data buses through the cpu_base interface so
// a platform can bind them to a bus_router. The ELF image is loaded into memory
// (backdoor, via the data bus) in start_of_simulation(), after bindings resolve.
class riscv_tlm_cpu : public cpu_base {
public:
    explicit riscv_tlm_cpu(sc_core::sc_module_name name,
                           const cpu_config& config = cpu_config{});
    ~riscv_tlm_cpu() override;

    tlm::tlm_initiator_socket<>& instr_bus() override;
    tlm::tlm_initiator_socket<>& data_bus() override;

    void load_elf(const std::string& path) override;
    void reset_cpu() override;
    std::uint64_t get_pc() const override;
    std::string backend_name() const override;
    std::uint64_t get_instret() const override;

    // Inject an external interrupt with the given RISC-V cause code (e.g. 7 =
    // machine timer, 11 = machine external). The firmware must have enabled the
    // matching mie bit and mstatus.MIE for it to be taken.
    void raise_irq(std::uint32_t cause) override;
    void set_irq(unsigned cause, bool level) override;

private:
    void start_of_simulation() override;

    riscv_tlm::CPURV32* core_ = nullptr;
    std::string elf_path_;
    std::uint64_t entry_pc_ = 0;

    // Initiator bound to the core's irq_line_socket: satisfies the mandatory
    // binding and is the channel through which raise_irq() injects interrupts.
    tlm_utils::simple_initiator_socket<riscv_tlm_cpu> irq_port_;
};

} // namespace cdc::cpu
