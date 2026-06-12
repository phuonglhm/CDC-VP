#include "riscv_tlm_wrapper.h"

#include <cdc/cpu/elf_loader.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>

#include "CPU.h" // mariusmm RISC-V-TLM core

namespace cdc::cpu {

riscv_tlm_cpu::riscv_tlm_cpu(sc_core::sc_module_name name, const cpu_config& config)
    : cpu_base(name, config)
    , irq_port_("irq_port")
{
    // mariusmm's CPU base expects a spdlog logger named "my_logger" to already
    // exist (it calls spdlog::get + logger->info in its constructor). Register a
    // discarding logger so it does not pollute stdout (where our UART prints).
    if (!spdlog::get("my_logger")) {
        auto lg = spdlog::create<spdlog::sinks::null_sink_mt>("my_logger");
        lg->set_level(spdlog::level::off);
    }

    // PC is a placeholder; the real entry point is set in start_of_simulation()
    // after the ELF is loaded.
    core_ = new riscv_tlm::CPURV32("core", /*PC=*/0u, /*debug=*/false);

    // Bind to the core's mandatory irq_line_socket; also used by raise_irq().
    irq_port_.bind(core_->irq_line_socket);
}

riscv_tlm_cpu::~riscv_tlm_cpu()
{
    delete core_;
}

tlm::tlm_initiator_socket<>& riscv_tlm_cpu::instr_bus()
{
    return core_->instr_bus;
}

tlm::tlm_initiator_socket<>& riscv_tlm_cpu::data_bus()
{
    return core_->mem_intf->data_bus;
}

void riscv_tlm_cpu::load_elf(const std::string& path)
{
    // Defer the actual load until bindings are resolved (start_of_simulation).
    elf_path_ = path;
}

void riscv_tlm_cpu::start_of_simulation()
{
    if (elf_path_.empty()) {
        return;
    }
    entry_pc_ = cdc::cpu::load_elf(core_->mem_intf->data_bus, elf_path_);
    core_->getRegisterBank()->setPC(static_cast<std::uint32_t>(entry_pc_));
}

void riscv_tlm_cpu::reset_cpu()
{
    if (core_ != nullptr) {
        core_->getRegisterBank()->setPC(static_cast<std::uint32_t>(entry_pc_));
    }
}

std::uint64_t riscv_tlm_cpu::get_pc() const
{
    return core_ != nullptr ? core_->getRegisterBank()->getPC() : 0u;
}

std::string riscv_tlm_cpu::backend_name() const
{
    return "riscv_tlm (mariusmm RV32IMAC)";
}

std::uint64_t riscv_tlm_cpu::get_instret() const
{
    return ::Performance::getInstance()->getInstructions();
}

void riscv_tlm_cpu::set_irq(unsigned cause, bool level)
{
    // mariusmm's interrupt model is edge-based (the core auto-clears mip after
    // delivery), so we only act on a rising level by injecting the cause.
    if (level) {
        raise_irq(cause);
    }
}

void riscv_tlm_cpu::raise_irq(std::uint32_t cause)
{
    // mariusmm's call_interrupt() reads the cause from the payload data and sets
    // its pending-interrupt flag; the core takes it on the next step.
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    trans.set_command(tlm::TLM_IGNORE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&cause));
    trans.set_data_length(sizeof(cause));
    trans.set_streaming_width(sizeof(cause));
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    irq_port_->b_transport(trans, delay);
}

} // namespace cdc::cpu
