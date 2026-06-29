// SPDX-License-Identifier: Apache-2.0
#include "riscv_vp_wrapper.h"

#include <cdc/cpu/elf_loader.h>

#include "core/common/bus_lock_if.h"
#include "core/common/clint_if.h"
#include "iss.h"
#include "mem.h"

namespace cdc::cpu {

namespace {

// Top of the riscv_cpu_eval RAM region (0x80000000 + 64 KiB). Used as the initial
// stack pointer; the bare-metal startup overwrites SP anyway.
constexpr std::uint32_t kInitialSp = 0x8001'0000u;
constexpr std::uint32_t kResetPc = 0x8000'0000u;

// Single-core bus lock: there is never contention, so locking is trivial.
struct trivial_bus_lock : public bus_lock_if {
    bool locked = false;
    void lock(unsigned) override { locked = true; }
    void unlock(unsigned) override { locked = false; }
    bool is_locked() override { return locked; }
    bool is_locked(unsigned) override { return locked; }
    void wait_until_unlocked() override {}
};

// Minimal CLINT: no timer/software interrupts for the hello checkpoint; just
// reports a monotonically increasing mtime derived from simulation time.
struct stub_clint : public clint_if {
    std::uint64_t update_and_get_mtime() override {
        return static_cast<std::uint64_t>(sc_core::sc_time_stamp().value());
    }
};

}  // namespace

struct riscv_vp_cpu::impl {
    // Runs before `iss` (member init order): Bremen's ISS ctor asserts the TLM
    // global quantum is >= its cycle time (10ns), so set it first.
    struct quantum_init {
        quantum_init() {
            // Only set a default if nobody set a valid quantum yet (main may set a
            // benchmark quantum before constructing the platform). Must be >= 10ns.
            auto& gq = tlm::tlm_global_quantum::instance();
            if (gq.get() < sc_core::sc_time(10, sc_core::SC_NS)) {
                gq.set(sc_core::sc_time(1, sc_core::SC_US));
            }
        }
    } quantum_init_;

    std::shared_ptr<trivial_bus_lock> bus_lock = std::make_shared<trivial_bus_lock>();
    stub_clint clint;
    rv32::ISS iss;
    rv32::CombinedMemoryInterface mem_if;
    std::unique_ptr<rv32::DirectCoreRunner> runner;

    impl() : iss(/*hart_id=*/0), mem_if("core_mem", iss) {
        iss.systemc_name = "core";
        mem_if.bus_lock = bus_lock;
        // DirectCoreRunner registers the SC_THREAD that calls iss.run(); it reads
        // iss.systemc_name (set above) for its module name.
        runner = std::make_unique<rv32::DirectCoreRunner>(iss);
    }
};

riscv_vp_cpu::riscv_vp_cpu(sc_core::sc_module_name name, const cpu_config& config)
    : cpu_base(name, config)
    , impl_(std::make_unique<impl>())
{
}

riscv_vp_cpu::~riscv_vp_cpu() = default;

tlm::tlm_initiator_socket<>& riscv_vp_cpu::instr_bus()
{
    return impl_->mem_if.isock;
}

tlm::tlm_initiator_socket<>& riscv_vp_cpu::data_bus()
{
    return impl_->mem_if.isock;
}

void riscv_vp_cpu::set_irq(unsigned cause, bool level)
{
    switch (cause) {
        case 3:  // machine software interrupt (MSIP)
            impl_->iss.trigger_software_interrupt(level);
            break;
        case 7:  // machine timer interrupt (MTIP)
            impl_->iss.trigger_timer_interrupt(level);
            break;
        case 11:  // machine external interrupt (MEIP)
            if (level) {
                impl_->iss.trigger_external_interrupt(MachineMode);
            } else {
                impl_->iss.clear_external_interrupt(MachineMode);
            }
            break;
        default:
            break;
    }
}

void riscv_vp_cpu::load_elf(const std::string& path)
{
    elf_path_ = path;
}

void riscv_vp_cpu::start_of_simulation()
{
    if (elf_path_.empty()) {
        entry_pc_ = kResetPc;
        impl_->iss.init(&impl_->mem_if, &impl_->mem_if, &impl_->clint,
                        static_cast<std::uint32_t>(entry_pc_), kInitialSp);
        return;
    }
    // Load the image into memory via the (now-bound) bus, then point the ISS at it.
    entry_pc_ = cdc::cpu::load_elf(impl_->mem_if.isock, elf_path_);
    impl_->iss.init(&impl_->mem_if, &impl_->mem_if, &impl_->clint,
                    static_cast<std::uint32_t>(entry_pc_), kInitialSp);
}

void riscv_vp_cpu::reset_cpu()
{
    impl_->iss.pc = static_cast<std::uint32_t>(entry_pc_);
}

std::uint64_t riscv_vp_cpu::get_pc() const
{
    return impl_->iss.pc;
}

std::string riscv_vp_cpu::backend_name() const
{
    return "riscv_vp (Bremen rv32)";
}

std::uint64_t riscv_vp_cpu::get_instret() const
{
    return impl_->iss.total_num_instr;
}

}  // namespace cdc::cpu
