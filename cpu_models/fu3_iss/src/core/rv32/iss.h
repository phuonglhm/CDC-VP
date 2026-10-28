#pragma once

#include "core/common/bus_lock_if.h"
#include "core/common/clint_if.h"
#include "core/common/instr.h"
#include "core/common/irq_if.h"
#include "core/common/trap.h"
#include "core/common/debug.h"
#include "csr.h"
#include "fp.h"
#include "mem_if.h"
#include "syscall_if.h"
#include "util/common.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>
#include <systemc>

namespace fu3_rv32 {

struct RegFile {
	static constexpr unsigned NUM_REGS = 32;

	int32_t regs[NUM_REGS];

	RegFile();

	RegFile(const RegFile &other);

	void write(uint32_t index, int32_t value);

	int32_t read(uint32_t index);

	uint32_t shamt(uint32_t index);

	int32_t &operator[](const uint32_t idx);

	void show();

	enum e : uint16_t {
		x0 = 0,
		x1,
		x2,
		x3,
		x4,
		x5,
		x6,
		x7,
		x8,
		x9,
		x10,
		x11,
		x12,
		x13,
		x14,
		x15,
		x16,
		x17,
		x18,
		x19,
		x20,
		x21,
		x22,
		x23,
		x24,
		x25,
		x26,
		x27,
		x28,
		x29,
		x30,
		x31,

		zero = x0,
		ra = x1,
		sp = x2,
		gp = x3,
		tp = x4,
		t0 = x5,
		t1 = x6,
		t2 = x7,
		s0 = x8,
		fp = x8,
		s1 = x9,
		a0 = x10,
		a1 = x11,
		a2 = x12,
		a3 = x13,
		a4 = x14,
		a5 = x15,
		a6 = x16,
		a7 = x17,
		s2 = x18,
		s3 = x19,
		s4 = x20,
		s5 = x21,
		s6 = x22,
		s7 = x23,
		s8 = x24,
		s9 = x25,
		s10 = x26,
		s11 = x27,
		t3 = x28,
		t4 = x29,
		t5 = x30,
		t6 = x31,
	};
};

// NOTE: on this branch, currently the *simple-timing* model is still directly
// integrated in the ISS. Merge the *timedb* branch to use the timing_if.
struct ISS;

struct timing_if {
	virtual ~timing_if() {}

	virtual void update_timing(Instruction instr, Opcode::Mapping op, ISS &iss) = 0;
};

/* Buffer to be used between the ISS and instruction memory interface to cache compressed instructions.
 * In case the ISS does not support compressed instructions, then this buffer is not necessary and the ISS
 * can use the memory interface directly. */
struct InstructionBuffer {
	instr_memory_if *instr_mem = nullptr;
	uint32_t last_fetch_addr = 0;
	uint32_t buffer = 0;

	uint32_t load_instr(uint64_t addr) {
		if (addr == (last_fetch_addr + 2))
			return (buffer >> 16);

		last_fetch_addr = addr;
		buffer = instr_mem->load_instr(addr);
		return buffer;
	}
};

struct PendingInterrupts {
	PrivilegeLevel target_mode;
	uint32_t pending;
};

// FU3 fork: the per-core features in which the FU3 CPUs differ from the
// upstream ISS. The defaults reproduce upstream behaviour exactly, so a core
// that is never configured (riscv_cpu_eval) runs as before.
struct core_options {
	uint32_t misa_disable = 0;        // csr_misa extension bits to clear
	unsigned fast_irqs = 0;           // Ibex irq_fast_i[n-1:0], mcause 16..16+n-1
	bool mtvec_vectored_only = false; // Ibex: MODE reads 1, BASE is 256-byte aligned
	uint32_t mtvec_reset = 0;
	bool zcb = false;                 // Zcb without the Zbb-dependent instructions
	unsigned pmp_regions = 0;         // 0: PMP CSRs stored but not enforced (upstream)
	bool smepmp = false;              // mseccfg at 0x747 (Smepmp 1.0 rules)
	uint32_t mseccfg_reset = 0;       // bit0 MML, bit1 MMWP, bit2 RLB
	std::array<uint8_t, 16> pmpcfg_reset{};
	std::array<uint32_t, 16> pmpaddr_reset{};
	sc_core::sc_time cycle_time = sc_core::sc_time(10, sc_core::SC_NS);
	// true: loads/stores cost one cycle like other instructions, because the
	// bus model adds its own latency; false: upstream fixed costs.
	bool bus_timed_memory = false;
	// Misaligned loads/stores are carried out by the memory interface (Ibex
	// LSU splits them) instead of raising the misaligned exception.
	bool split_misaligned = false;
	// Debug Mode (RISC-V debug spec 0.13.2, execution based): entry jumps to
	// debug_halt_address, an exception in Debug Mode to debug_exception_address.
	bool debug = false;
	uint32_t debug_halt_address = 0;
	uint32_t debug_exception_address = 0;
	// Debug Module aperture: Debug Mode accesses to it bypass PMP (Ibex).
	uint32_t debug_module_base = 0, debug_module_size = 0;
	unsigned triggers = 0;  // mcontrol execute-address triggers (load/store hardwired 0)
};

enum class pmp_access { read, write, execute };

struct ISS : public external_interrupt_target, public clint_interrupt_target, public iss_syscall_if, public debug_target_if {
	clint_if *clint = nullptr;
	instr_memory_if *instr_mem = nullptr;
	data_memory_if *mem = nullptr;
	syscall_emulator_if *sys = nullptr;  // optional, if provided, the iss will intercept and handle syscalls directly
	RegFile regs;
	FpRegs fp_regs;
	uint32_t pc = 0;
	uint32_t last_pc = 0;
	bool trace = false;
	bool shall_exit = false;
    bool ignore_wfi = false;
    bool error_on_zero_traphandler = false;
	csr_table csrs;
	PrivilegeLevel prv = MachineMode;
	int64_t lr_sc_counter = 0;
	uint64_t total_num_instr = 0;

	// last decoded and executed instruction and opcode
	Instruction instr;
	Opcode::Mapping op;

	CoreExecStatus status = CoreExecStatus::Runnable;
	std::unordered_set<uint32_t> breakpoints;
	bool debug_mode = false;

	sc_core::sc_event wfi_event;

	std::string systemc_name;
	tlm_utils::tlm_quantumkeeper quantum_keeper;
	sc_core::sc_time cycle_time;
	// FU3 fork: cycles are counted as integers (instr_cost per opcode), so
	// mcycle stays exact when the clock period changes (set_cycle_time).
	uint64_t cycle_counter = 0;  // use a separate cycle counter, since cycle count can be inhibited
	std::array<uint32_t, Opcode::NUMBER_OF_INSTRUCTIONS> instr_cost;
	std::array<sc_core::sc_time, Opcode::NUMBER_OF_INSTRUCTIONS> instr_cycles;  // instr_cost * cycle_time

	static constexpr int32_t REG_MIN = INT32_MIN;
    static constexpr unsigned xlen = 32;

	// FU3 fork state (see core_options).
	core_options opts;
	uint32_t mie_mask = csr::MIE_MASK;  // MIE/MIP bits that exist on this core
	uint32_t mseccfg = 0;
	uint64_t cycle_offset = 0;  // mcycle = counted cycles + offset (mcycle writes)
	// A level, not an event: set by the core's owner when the hart must stop
	// (reset, later debug halt), so WFI never goes to sleep while it is set,
	// whenever the request arrived. Cleared by reset().
	bool halt_request = false;

	// FU3 fork: Debug Mode state.
	bool in_debug = false;      // hart is in Debug Mode (executing debug code)
	bool debug_request = false; // debug_req_i level
	bool step_armed = false;    // dcsr.step was set at the last DRET/resume
	uint32_t dcsr = 0, dpc = 0, dscratch[2] = {0, 0};
	uint32_t tselect = 0;
	std::array<uint32_t, 4> tdata1{}, tdata2{};
	uint64_t debug_entries = 0;  // diagnostics

	ISS(uint32_t hart_id, bool use_E_base_isa = false);

	// FU3 fork: apply the core options (before reset()).
	void configure(const core_options &options);
	// FU3 fork: new clock period (non-zero), effective from the next instruction;
	// cycle counts are unaffected. May exceed the global quantum (slow clock).
	void set_cycle_time(sc_core::sc_time period);
	// FU3 fork: architectural reset: registers, privilege, CSRs, PMP to their
	// reset values, execution restarts at entrypoint.
	void reset(uint32_t entrypoint);
	// FU3 fork: level of Ibex irq_fast_i[index] (mip bit 16 + index).
	void set_fast_irq(unsigned index, bool level);
	// FU3 fork: whether PMP/Smepmp allows the access; effective privilege
	// includes MPRV for data accesses. Always true when PMP is not enforced.
	bool pmp_permits(uint32_t addr, unsigned len, pmp_access access);
	PrivilegeLevel data_privilege();
	// FU3 fork: Debug Mode. enter_debug() takes the hart in with dcsr.cause and
	// dpc; leave_debug() is DRET (also used by a Debug Module resume).
	void enter_debug(unsigned cause, uint32_t next_dpc);
	void leave_debug();
	bool trigger_hit(uint32_t address);
	uint32_t tdata1_read(unsigned index);

	void exec_step();

	uint64_t _compute_and_get_current_cycles();

	void init(instr_memory_if *instr_mem, data_memory_if *data_mem, clint_if *clint, uint32_t entrypoint, uint32_t sp);

	void trigger_external_interrupt(PrivilegeLevel level) override;

	void clear_external_interrupt(PrivilegeLevel level) override;

	void trigger_timer_interrupt(bool status) override;

	void trigger_software_interrupt(bool status) override;


	void sys_exit() override;
	unsigned get_syscall_register_index() override;
	uint64_t read_register(unsigned idx) override;
	void write_register(unsigned idx, uint64_t value) override;

    std::vector<uint64_t> get_registers(void) override;

    Architecture get_architecture(void) override {
        return RV32;
    }

    uint64_t get_progam_counter(void) override;
    void enable_debug(void) override;
    CoreExecStatus get_status(void) override;
    void set_status(CoreExecStatus) override;
    void block_on_wfi(bool) override;

    void insert_breakpoint(uint64_t) override;
    void remove_breakpoint(uint64_t) override;

	uint64_t get_hart_id() override;


	void release_lr_sc_reservation() {
		lr_sc_counter = 0;
		mem->atomic_unlock();
	}

	void fp_prepare_instr();
	void fp_finish_instr();
	void fp_set_dirty();
	void fp_update_exception_flags();
	void fp_setup_rm();
	void fp_require_not_off();

	uint32_t get_csr_value(uint32_t addr);
	void set_csr_value(uint32_t addr, uint32_t value);
	// FU3 fork: enforced PMP CSR writes (WARL, lock and Smepmp rules).
	bool pmp_entry_locked(unsigned index);
	void write_pmpcfg_byte(unsigned index, uint8_t value);
	void write_pmpaddr(unsigned index, uint32_t value);
	uint8_t pmpcfg_byte(unsigned index);

	bool is_invalid_csr_access(uint32_t csr_addr, bool is_write);
	void validate_csr_counter_read_access_rights(uint32_t addr);

	unsigned pc_alignment_mask() {
		if (csrs.misa.has_C_extension())
			return ~0x1;
		else
			return ~0x3;
	}

	inline void trap_check_pc_alignment() {
		assert(!(pc & 0x1) && "not possible due to immediate formats and jump execution");

		if (unlikely((pc & 0x3) && (!csrs.misa.has_C_extension()))) {
			// NOTE: misaligned instruction address not possible on machines supporting compressed instructions
			raise_trap(EXC_INSTR_ADDR_MISALIGNED, pc);
		}
	}

	template <unsigned Alignment, bool isLoad>
	inline void trap_check_addr_alignment(uint32_t addr) {
		if (unlikely(addr % Alignment) && !opts.split_misaligned) {
			raise_trap(isLoad ? EXC_LOAD_ADDR_MISALIGNED : EXC_STORE_AMO_ADDR_MISALIGNED, addr);
		}
	}

	inline void execute_amo(Instruction &instr, std::function<int32_t(int32_t, int32_t)> operation) {
		uint32_t addr = regs[instr.rs1()];
		trap_check_addr_alignment<4, false>(addr);
		uint32_t data;
		try {
			data = mem->atomic_load_word(addr);
		} catch (SimulationTrap &e) {
			if (e.reason == EXC_LOAD_ACCESS_FAULT)
				e.reason = EXC_STORE_AMO_ACCESS_FAULT;
			throw e;
		}
		uint32_t val = operation(data, regs[instr.rs2()]);
		mem->atomic_store_word(addr, val);
		regs[instr.rd()] = data;
	}

	inline bool m_mode() {
		return prv == MachineMode;
	}

	inline bool s_mode() {
		return prv == SupervisorMode;
	}

	inline bool u_mode() {
		return prv == UserMode;
	}

	PrivilegeLevel prepare_trap(SimulationTrap &e);

	void prepare_interrupt(const PendingInterrupts &x);

	PendingInterrupts compute_pending_interrupts();

	bool has_pending_enabled_interrupts() {
		return compute_pending_interrupts().target_mode != NoneMode;
	}

	bool has_local_pending_enabled_interrupts() {
		return csrs.mie.reg & csrs.mip.reg;
	}

	void return_from_trap_handler(PrivilegeLevel return_mode);

	void switch_to_trap_handler(PrivilegeLevel target_mode);

	void performance_and_sync_update(Opcode::Mapping executed_op);

	void run_step() override;

	void run() override;

	void show();
};

/* Do not call the run function of the ISS directly but use one of the Runner
 * wrappers. */
struct DirectCoreRunner : public sc_core::sc_module {
	ISS &core;
	std::string thread_name;

	SC_HAS_PROCESS(DirectCoreRunner);

	DirectCoreRunner(ISS &core) : sc_module(sc_core::sc_module_name(core.systemc_name.c_str())), core(core) {
		thread_name = "run" + std::to_string(core.get_hart_id());
		SC_NAMED_THREAD(run, thread_name.c_str());
	}

	void run() {
		core.run();

		if (core.status == CoreExecStatus::HitBreakpoint) {
			throw std::runtime_error(
			    "Breakpoints are not supported in the direct runner, use the debug "
			    "runner instead.");
		}
		assert(core.status == CoreExecStatus::Terminated);

		sc_core::sc_stop();
	}
};

}  // namespace fu3_rv32
