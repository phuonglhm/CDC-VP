#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <cdc/cpu/cpu_base.h>

namespace cdc::cpu {

// Per-core features of the FU3 ISS fork (cpu_models/fu3_iss). The defaults
// reproduce the upstream Bremen ISS, so a platform that passes none
// (riscv_cpu_eval) runs as before.
struct cva6_core_options {
    unsigned fast_irqs = 0;                 // Ibex irq_fast_i[n-1:0] -> set_irq(16 + i)
    bool mtvec_vectored_only = false;       // Ibex: MODE fixed at 1, BASE 256-byte aligned
    std::uint32_t mtvec_reset = 0;
    bool zcb = false;                       // Zcb subset without Zbb
    unsigned pmp_regions = 0;               // 0: PMP CSRs stored, not enforced
    bool smepmp = false;                    // mseccfg (Smepmp 1.0)
    std::uint32_t mseccfg_reset = 0;        // bit0 MML, bit1 MMWP, bit2 RLB
    std::array<std::uint8_t, 16> pmpcfg_reset{};
    std::array<std::uint32_t, 16> pmpaddr_reset{};
    sc_core::sc_time cycle_time{10, sc_core::SC_NS};
    // true: loads/stores cost one cycle and the bus adds its own latency once.
    bool bus_timed_memory = false;
    // Misaligned load/store: false raises the misaligned exception (cause 4/6,
    // CVA6); true splits it into aligned word accesses with byte enables (Ibex
    // LSU). The upstream default passes it to the bus unchanged.
    std::optional<bool> split_misaligned;
    // Debug Mode (RISC-V debug spec 0.13.2, execution based): entry jumps to
    // debug_halt_address (debug code in ISRAM), an exception in Debug Mode to
    // debug_exception_address. Debug Mode accesses carry fu3_bus::Access::debug
    // and bypass PMP inside the Debug Module aperture.
    bool debug = false;
    std::uint32_t debug_halt_address = 0;
    std::uint32_t debug_exception_address = 0;
    std::uint32_t debug_module_base = 0, debug_module_size = 0;
    unsigned triggers = 0;  // execute-address mcontrol triggers
    // x2 after reset. FU3 cores leave it unset (0): the spec defines no value
    // and firmware sets sp itself.
    std::optional<std::uint32_t> reset_sp = 0x80010000u;
};

class cva6_wrapper : public cpu_base {
public:
    // FU3: truyền AFNSU để misa = RV32IMC (chỉ M-mode)
    // Default: rỗng = giữ misa IMACF + NUS
    explicit cva6_wrapper(sc_core::sc_module_name name, const cpu_config& config = cpu_config{},
                          const std::string& disable_extensions = "",
                          const cva6_core_options& options = cva6_core_options{});
    ~cva6_wrapper() override;

    // Default: 2 cổng AXI riêng biệt cho lệnh và dữ liệu
    tlm::tlm_initiator_socket<>& instr_bus() override;
    tlm::tlm_initiator_socket<>& data_bus() override;

    // FU3: Mỗi CPU chỉ có 1 AXi4 Master (bus FU3 có 1 Initiator "CVA6"/"IBEX") => instr_bus() và data_bus() trả về cùng 1 socket
    // Đã gộp I/D để khớp bus FU3 => platform chỉ bind data_bus().
    bool has_unified_bus() const override {
        return true;
    }

    // Interrupt inputs, as mcause numbers: 3 software, 7 timer, 11 external
    // (CVA6: ipi_i, time_irq_i, irq_i[0]); 16 + i for Ibex irq_fast_i[i] when
    // cva6_core_options::fast_irqs > i. Any other cause throws.
    void set_irq(unsigned cause, bool level) override;


    void load_elf(const std::string& path) override;
    void reset_cpu() override;

    // Chân reset của CVA6 (rst_ni): true = giữ reset, false = nhả và chạy từ reset PC.
    // FU3: gọi set_reset(true) ngay sau khi tạo và Ibex handoff gọi set_reset(false).
    void set_reset(bool asserted);
    bool in_reset() const noexcept { return in_reset_; }

    // Core clock: the period of one cycle (cva6_core_options::cycle_time until
    // set), applied before the next instruction; cycle counts are unaffected.
    // SC_ZERO_TIME stops the clock: the core executes nothing until a non-zero
    // period is set.
    void set_clock_period(sc_core::sc_time period);
    sc_core::sc_time clock_period() const { return clock_period_; }

    std::uint64_t get_pc() const override;
    // Print every executed instruction and trap (ISS trace) to stdout.
    void set_trace(bool on);

    // Debug control, for a Debug Module (fu3_dbg::DebugHart) or a GDB stub.
    // The request is a level (debug_req_i) and survives reset. The hart is
    // halted while it is in Debug Mode; resume() is a DRET applied by the CPU
    // thread before its next instruction (dcsr.step makes it a single step).
    void set_debug_request(bool level);
    bool debug_halted() const;
    void debug_resume();
    // Abstract register access while halted: regno 0x0000-0x0FFF CSR,
    // 0x1000-0x101F GPR, 0x1020-0x103F FPR (with F). False when not halted or
    // the register does not exist.
    bool debug_access_register(std::uint16_t regno, bool write, std::uint32_t& value);
    std::uint64_t debug_entries() const;
    std::string backend_name() const override;

    // Số lệnh đã thực hiện => 0 nếu Backend không thể báo cáo.
    std::uint64_t get_instret() const override;

private:

    // Hàm này tự động chạy ngay trước khi thời gian mô phỏng bắt đầu (t = 0).
    // Chỉ dùng để nạp file ELF và reset PC sau khi các dây nối Bus đã liên kết xong.
    void start_of_simulation() override;

    // Gom ISS + bộ nhớ + runner để cho phép triển khai riêng tư, tránh đưa các header của CVA6 vào giao diện công khai.
    struct impl;
    std::unique_ptr<impl> impl_;
    cva6_core_options options_;
    std::string elf_path_;
    std::uint64_t elf_entry_pc_ = 0;  // Lưu entry của ELF và xem C7/C8
    bool elf_loaded_ = false;
    bool initialised_ = false;
    bool in_reset_ = false;
    bool reset_pending_ = false;     // architectural reset owed before the next instruction
    bool resume_pending_ = false;    // Debug Module resume owed before the next instruction
    std::uint32_t irq_levels_ = 0;   // interrupt input levels by cause (survive reset)
    sc_core::sc_time clock_period_;
    bool clock_pending_ = false;     // new clock period owed before the next instruction
    sc_core::sc_event reset_released_, clock_started_;

    void apply_irq(unsigned cause, bool level);
    void run();
};

}  // namespace cdc::cpu
