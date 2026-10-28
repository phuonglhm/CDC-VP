#include "cva6_wrapper.h"
#include <stdexcept>
#include <iostream>
#include <optional>

#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cdc/cpu/elf_loader.h>

#include <fu3_bus/access.h>

#include "core/rv32/iss.h"

#include "core/common/clint_if.h"

namespace cdc::cpu {

struct stub_clint : public clint_if {
    std::uint64_t update_and_get_mtime() override {
        return static_cast<std::uint64_t>(sc_core::sc_time_stamp().value());
    }
};

// Đổi chữ cái extension cần bỏ thành mask misa của ISS (mặc định IMACF + NUS).
// Bitmanip (RVB = 1) không có trong ISS Bremen nên không bật được.
static std::uint32_t misa_mask_from_letters(const std::string& letters) {
    std::uint32_t mask = 0;
    for (char c : letters) {
        switch (c) {
            case 'A': mask |= fu3_rv32::csr_misa::A; 
                break;
            case 'C': mask |= fu3_rv32::csr_misa::C; 
                break;
            case 'D': mask |= fu3_rv32::csr_misa::D; 
                break;
            case 'F': mask |= fu3_rv32::csr_misa::F; 
                break;
            case 'M': mask |= fu3_rv32::csr_misa::M; 
                break;
            case 'N': mask |= fu3_rv32::csr_misa::N; 
                break;
            case 'S': mask |= fu3_rv32::csr_misa::S; break;
            case 'U': mask |= fu3_rv32::csr_misa::U; 
                break;
            default:
                throw std::invalid_argument(std::string("Cannot disable extension '") + c + "'");
        }
    }
    return mask;
}

// LT timing: each bus access starts at the ISS's local time offset and the
// bus's latency is taken back into the quantum keeper, so it counts once
// whether the target waits or only annotates.
static void timed_transport(tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& socket,
                            tlm::tlm_generic_payload& trans, fu3_rv32::ISS* iss) {
    sc_core::sc_time delay = iss ? iss->quantum_keeper.get_local_time() : sc_core::SC_ZERO_TIME;
    socket->b_transport(trans, delay);
    if (iss) {
        iss->quantum_keeper.set(delay);
    }
}

struct CVA6_InstrAdapter : public fu3_rv32::instr_memory_if {
    tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& i_socket;
    fu3_rv32::ISS* iss = nullptr;   // PMP state and quantum keeper, set once the ISS exists

    // i_socket truyền vào
    CVA6_InstrAdapter(tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& socket) 
        : i_socket(socket) {}

    // Đọc 1 word lệnh căn 4: fault_addr = địa chỉ báo vào mtval khi bus lỗi
    uint32_t fetch_word(uint64_t addr, uint64_t fault_addr)  {
        uint32_t instruction = 0;
        tlm::tlm_generic_payload trans;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&instruction));
        trans.set_data_length(4); // Lệnh RV32 luôn là 4 bytes
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        fu3_bus::ScopedAccess access(trans, 4, fu3_bus::Burst::Increment, /*instruction=*/true, 0,
                                     /*debug=*/iss && iss->in_debug);

        timed_transport(i_socket, trans, iss);

        if (trans.is_response_error()) {
            // Nếu Bus báo lỗi, ném ngoại lệ để ISS bắt thành trap Access Fault
            // throw std::runtime_error("Bus error luc fetch lenh tai dia chi: " + std::to_string(addr));
            // ISS chỉ bắt SimulationTrap, runtime_error làm dừng cả VP thay vì sinh trap.
            raise_trap(EXC_INSTR_ACCESS_FAULT, fault_addr);   // mcause 1
        }
        return instruction;
    }

    // Bus FU3 chỉ nhận fetch 4 byte căn 4, nhưng có RVC thì PC có thể chỉ căn 2 => đọc word căn hàng rồi ghép, giống fetch thật của CVA6/Ibex.
    // PMP checks the bytes of the instruction itself: 2 first, 4 once the
    // encoding shows a 32-bit instruction (mtval = instruction address).
    void check_execute(uint64_t addr, unsigned len) {
        if (iss && !iss->pmp_permits(static_cast<uint32_t>(addr), len, fu3_rv32::pmp_access::execute)) {
            raise_trap(EXC_INSTR_ACCESS_FAULT, addr);
        }
    }

    uint32_t load_instr(uint64_t addr) override {
        check_execute(addr, 2);
        if ((addr & 3) == 0) {
            const uint32_t word = fetch_word(addr, addr);
            if ((word & 3) == 3) {
                check_execute(addr, 4);
            }
            return word;
        }
        const uint64_t base = addr & ~uint64_t{3};
        const uint32_t low = fetch_word(base, addr) >> 16;
        if ((low & 3) != 3) {
            return low;   // Lệnh nén 16 bit: không đọc word sau (tránh fault giả ở cuối vùng nhớ)
        }
        check_execute(addr, 4);
        return low | (fetch_word(base + 4, base + 4) << 16);
    }
};

struct CVA6_DataAdapter : public fu3_rv32::data_memory_if {
    tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& d_socket;
    fu3_rv32::ISS* iss = nullptr;   // PMP state and quantum keeper, set once the ISS exists
    std::optional<bool> split_misaligned;   // see cva6_core_options

    CVA6_DataAdapter(tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& socket) 
        : d_socket(socket) {}

    void tlm_access(uint64_t addr, uint8_t* data, unsigned int len, tlm::tlm_command cmd) {
        if (split_misaligned && len > 1 && (addr % len)) {
            const bool read = cmd == tlm::TLM_READ_COMMAND;
            if (!*split_misaligned) {
                raise_trap(read ? EXC_LOAD_ADDR_MISALIGNED : EXC_STORE_AMO_ADDR_MISALIGNED, addr);
            }
            // Ibex LSU: up to two aligned words, only the addressed bytes enabled.
            // PMP judges the bytes of the original access, once.
            check_pmp(addr, len, read, addr);
            for (uint64_t word = addr & ~uint64_t{3}; word < addr + len; word += 4) {
                uint8_t lanes[4] = {}, buffer[4] = {};
                for (unsigned i = 0; i < 4; ++i) {
                    const uint64_t a = word + i;
                    if (a >= addr && a < addr + len) {
                        lanes[i] = TLM_BYTE_ENABLED;
                        buffer[i] = data[a - addr];
                    }
                }
                access_word(word, buffer, lanes, cmd, addr);
                for (unsigned i = 0; i < 4; ++i) {
                    const uint64_t a = word + i;
                    if (read && a >= addr && a < addr + len) {
                        data[a - addr] = buffer[i];
                    }
                }
            }
            return;
        }
        check_pmp(addr, len, cmd == tlm::TLM_READ_COMMAND, addr);
        single_access(addr, data, len, cmd, nullptr, addr);
    }

    void check_pmp(uint64_t addr, unsigned len, bool read, uint64_t fault_addr) {
        if (iss && !iss->pmp_permits(static_cast<uint32_t>(addr), len,
                                     read ? fu3_rv32::pmp_access::read : fu3_rv32::pmp_access::write)) {
            raise_trap(read ? EXC_LOAD_ACCESS_FAULT : EXC_STORE_AMO_ACCESS_FAULT, fault_addr);
        }
    }

    // One aligned word with byte lanes, part of a split misaligned access;
    // a fault reports the original (misaligned) address.
    void access_word(uint64_t word, uint8_t* data, uint8_t* lanes, tlm::tlm_command cmd, uint64_t fault_addr) {
        single_access(word, data, 4, cmd, lanes, fault_addr);
    }

    void single_access(uint64_t addr, uint8_t* data, unsigned int len, tlm::tlm_command cmd, uint8_t* lanes,
                       uint64_t fault_addr) {
        /*
        // Trick: Chặn toàn bộ khu vực UART (Từ 0x10000000 đến 0x10000007)
        if (addr >= 0x10000000 && addr <= 0x10000007) {
            if (cmd == tlm::TLM_WRITE_COMMAND) {
                // Ghi vào thanh ghi TX -> In thẳng ra Terminal
                if (addr == 0x10000000) std::cout << (char)(data[0]) << std::flush;
            } else {
                // Đọc bất kỳ thanh ghi trạng thái nào -> Nhét mã 0x60 vào mọi byte
                for (unsigned int i = 0; i < len; i++) {
                    data[i] = 0x60;
                }
            }
            return; // Phản hồi ngay tắp lự, cách ly hoàn toàn khỏi Bus Router ảo
        } */
        
        tlm::tlm_generic_payload trans;

        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(lanes);
        trans.set_byte_enable_length(lanes ? len : 0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        // 1 beat = len byte (1/2/4). 
        // Truy cập lệch hàng => bus trả ADDRESS_ERROR => access fault.
        fu3_bus::ScopedAccess access(trans, len, fu3_bus::Burst::Increment, false, 0,
                                     /*debug=*/iss && iss->in_debug);
        timed_transport(d_socket, trans, iss);

        if (trans.is_response_error()) {
            // throw std::runtime_error("Data Bus error tai dia chi: " + std::to_string(addr));
            // ISS chỉ bắt SimulationTrap, runtime_error làm dừng cả VP thay vì sinh trap.
            raise_trap(cmd == tlm::TLM_READ_COMMAND ? EXC_LOAD_ACCESS_FAULT        // mcause 5
                                                    : EXC_STORE_AMO_ACCESS_FAULT,  // mcause 7
                       fault_addr);
        }
    }

    int64_t load_double(uint64_t addr) override {
        int64_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 8, tlm::TLM_READ_COMMAND);
        return val;
    }
    int32_t load_word(uint64_t addr) override {
        int32_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 4, tlm::TLM_READ_COMMAND);
        return val;
    }
    int32_t load_half(uint64_t addr) override {
        int16_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 2, tlm::TLM_READ_COMMAND);
        return val; // Tự động Sign-extend lên 32-bit theo chuẩn C++
    }
    int32_t load_byte(uint64_t addr) override {
        int8_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 1, tlm::TLM_READ_COMMAND);
        return val;
    }
    uint32_t load_uhalf(uint64_t addr) override {
        uint16_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 2, tlm::TLM_READ_COMMAND);
        return val; // Tự động Zero-extend
    }
    uint32_t load_ubyte(uint64_t addr) override {
        uint8_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 1, tlm::TLM_READ_COMMAND);
        return val;
    }

    void store_double(uint64_t addr, uint64_t value) override {
        tlm_access(addr, (uint8_t*)&value, 8, tlm::TLM_WRITE_COMMAND);
    }
    void store_word(uint64_t addr, uint32_t value) override {
        tlm_access(addr, (uint8_t*)&value, 4, tlm::TLM_WRITE_COMMAND);
    }
    void store_half(uint64_t addr, uint16_t value) override {
        tlm_access(addr, (uint8_t*)&value, 2, tlm::TLM_WRITE_COMMAND);
    }
    void store_byte(uint64_t addr, uint8_t value) override {
        tlm_access(addr, (uint8_t*)&value, 1, tlm::TLM_WRITE_COMMAND);
    }


    int32_t atomic_load_word(uint64_t addr) override {
        int32_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 4, tlm::TLM_READ_COMMAND);
        return val;
    }
    void atomic_store_word(uint64_t addr, uint32_t value) override {
        tlm_access(addr, (uint8_t*)&value, 4, tlm::TLM_WRITE_COMMAND);
    }
    int32_t atomic_load_reserved_word(uint64_t addr) override {
        int32_t val = 0;
        tlm_access(addr, (uint8_t*)&val, 4, tlm::TLM_READ_COMMAND);
        return val;
    }
    bool atomic_store_conditional_word(uint64_t addr, uint32_t value) override {
        tlm_access(addr, (uint8_t*)&value, 4, tlm::TLM_WRITE_COMMAND);
        return true;    // VP chạy single-core nên SC luôn báo thành công
                        // FU3: RVA = 0 nên LR/SC/AMO báo illegal => hàm này không được gọi.
    }
    void atomic_unlock() override {}
    
    void flush_tlb() override {}
};

struct cva6_wrapper::impl {
    
    struct quantum_init {
        quantum_init() {
            auto& gq = tlm::tlm_global_quantum::instance();
            if (gq.get() < sc_core::sc_time(10, sc_core::SC_NS)) {
                gq.set(sc_core::sc_time(1, sc_core::SC_US));
            }
        }
    } quantum_init_;

    // tlm_utils::simple_initiator_socket<cva6_wrapper, 32> i_socket;
    // tlm_utils::simple_initiator_socket<cva6_wrapper, 32> d_socket;
    // FU3: 1 AXI4 master/CPU => Fetch và Data dùng chung 1 socket.
    tlm_utils::simple_initiator_socket<cva6_wrapper, 32> bus_socket;

    CVA6_InstrAdapter instr_adapter;
    CVA6_DataAdapter data_adapter;

    stub_clint clint;

    // Con trỏ chứa ISS
    std::unique_ptr<fu3_rv32::ISS> iss_core;

    /* impl(const cpu_config& config, const std::string& disable_extensions) : i_socket("i_socket"), 
                                                                            d_socket("d_socket"),
                                                                            instr_adapter(i_socket),  // i_socket cho Fetch
                                                                            data_adapter(d_socket)    // d_socket cho Data */
    impl(const cpu_config& config, const std::string& disable_extensions, const cva6_core_options& o)
        : bus_socket("bus_socket"),
          instr_adapter(bus_socket),  // Fetch
          data_adapter(bus_socket)    // Data
    {
        // VP++ đòi hỏi truyền hart_id vào constructor
        // Truyền giá trị hart_id từ cfg vào, tham số thứ 2 (use_E_base_isa) để mặc định là false
        iss_core = std::make_unique<fu3_rv32::ISS>(config.hart_id, false);
        // ISS kiểm tra bit misa khi Decode => Extension bị bỏ sẽ báo Illegal Instruction.
        fu3_rv32::core_options iss_options;
        iss_options.misa_disable = misa_mask_from_letters(disable_extensions);
        iss_options.fast_irqs = o.fast_irqs;
        iss_options.mtvec_vectored_only = o.mtvec_vectored_only;
        iss_options.mtvec_reset = o.mtvec_reset;
        iss_options.zcb = o.zcb;
        iss_options.pmp_regions = o.pmp_regions;
        iss_options.smepmp = o.smepmp;
        iss_options.mseccfg_reset = o.mseccfg_reset;
        iss_options.pmpcfg_reset = o.pmpcfg_reset;
        iss_options.pmpaddr_reset = o.pmpaddr_reset;
        iss_options.cycle_time = o.cycle_time;
        iss_options.bus_timed_memory = o.bus_timed_memory;
        iss_options.split_misaligned = o.split_misaligned.value_or(false);
        iss_options.debug = o.debug;
        iss_options.debug_halt_address = o.debug_halt_address;
        iss_options.debug_exception_address = o.debug_exception_address;
        iss_options.debug_module_base = o.debug_module_base;
        iss_options.debug_module_size = o.debug_module_size;
        iss_options.triggers = o.triggers;
        iss_core->configure(iss_options);
        instr_adapter.iss = iss_core.get();
        data_adapter.iss = iss_core.get();
        data_adapter.split_misaligned = o.split_misaligned;
    }
};

cva6_wrapper::cva6_wrapper(sc_core::sc_module_name name, const cpu_config& config,
                           const std::string& disable_extensions, const cva6_core_options& options)
    : cpu_base(name, config), impl_(std::make_unique<impl>(config, disable_extensions, options)),
      options_(options), clock_period_(options.cycle_time) {

    if (config.xlen != 32) {
        throw std::runtime_error("Invalid XLEN value. Must be 32.");
    } 
    /* if (!config.has_mmu) {
          throw std::runtime_error("Boot Linux requires MMU support.");
        } */ 
    //   FU3 có MmuPresent = 0 nên phải chạy được khi has_mmu = false
    //   Wrapper cũng chưa nối page-table walker => has_mmu = true trước đây chưa có tác dụng.

    SC_HAS_PROCESS(cva6_wrapper);
    SC_THREAD(run);
}

cva6_wrapper::~cva6_wrapper() = default;

// Gộp I/D: cả hai trả về cùng 1 socket (has_unified_bus() = true).
tlm::tlm_initiator_socket<>& cva6_wrapper::instr_bus() {
    // return impl_->i_socket;
    return impl_->bus_socket;
}

tlm::tlm_initiator_socket<>& cva6_wrapper::data_bus() {
    // return impl_->d_socket;
    return impl_->bus_socket;
}



void cva6_wrapper::set_irq(unsigned cause, bool level) {
    // The input level is kept apart from mip: a reset clears the CSR, not the
    // wire, so reset_cpu() re-applies the levels that are still driven.
    const bool known = cause == 3 || cause == 7 || cause == 11 || (cause >= 16 && cause - 16 < options_.fast_irqs);
    if (known) {
        irq_levels_ = level ? (irq_levels_ | (1u << cause)) : (irq_levels_ & ~(1u << cause));
    }
    apply_irq(cause, level);
}

void cva6_wrapper::apply_irq(unsigned cause, bool level) {
    if (cause == 7) {
        impl_->iss_core->trigger_timer_interrupt(level);
    } else if (cause == 3) {
        impl_->iss_core->trigger_software_interrupt(level);
    } else if (cause == 11) {
        if (level) {
            impl_->iss_core->trigger_external_interrupt(MachineMode);
        } else {
            impl_->iss_core->clear_external_interrupt(MachineMode);
        }
    } else if (cause >= 16 && cause - 16 < options_.fast_irqs) {
        impl_->iss_core->set_fast_irq(cause - 16, level);
    } else {
        // A wire into a cause the core does not have is an integration error.
        throw std::invalid_argument(std::string(name()) + ": no interrupt input for cause " + std::to_string(cause));
    }
}

void cva6_wrapper::load_elf(const std::string& path) {
    // Luồng chạy BootROM: Chỉ ghi nhận đường dẫn, không ghi đè cfg.reset_pc bằng entry_pc của file ELF
    elf_path_ = path;
    
}

void cva6_wrapper::reset_cpu() {
    // uint64_t start_pc = cfg.reset_pc_specified() ? cfg.reset_pc : 0x80000000;
    // Dòng code trên chỉ đúng khi start_of_simulation ghi đè cfg.reset_pc (C8). Do nay không ghi đè nữa => Không dùng
    uint64_t start_pc = cfg.reset_pc_specified() ? cfg.reset_pc
                      : (elf_loaded_ ? elf_entry_pc_ : 0x80000000);   

    // std::cout << "[CVA6] reset PC = 0x" << std::hex << start_pc << std::dec << std::endl;
    // Ibex dùng chung wrapper => In tên instance để log 2 CPU không lẫn nhau.
    std::cout << "[" << name() << "] reset PC = 0x" << std::hex << start_pc << std::dec << std::endl;
    
    // Architectural reset (registers, privilege, CSRs, PMP), then the memory
    // interfaces. FU3 options leave sp at 0: the spec defines no reset value and
    // firmware sets it in crt0; the upstream default keeps 0x80010000.
    impl_->iss_core->reset(static_cast<uint32_t>(start_pc));
    impl_->iss_core->init(
        &(impl_->instr_adapter),
        &(impl_->data_adapter),
        &(impl_->clint),
        static_cast<uint32_t>(start_pc),
        options_.reset_sp.value_or(0)
    );
    for (unsigned cause = 0; cause < 32; ++cause) {
        if (irq_levels_ & (1u << cause)) {
            apply_irq(cause, true);
        }
    }
}

std::uint64_t cva6_wrapper::get_pc() const {
    return impl_->iss_core->pc;
}

void cva6_wrapper::set_trace(bool on) {
    impl_->iss_core->trace = on;
}

void cva6_wrapper::set_debug_request(bool level) {
    if (!options_.debug) {
        throw std::logic_error(std::string(name()) + ": Debug Mode is not configured");
    }
    impl_->iss_core->debug_request = level;   // checked at every instruction boundary
    if (level) {
        impl_->iss_core->wfi_event.notify(sc_core::SC_ZERO_TIME);
    }
}

bool cva6_wrapper::debug_halted() const {
    return impl_->iss_core->in_debug && !resume_pending_;
}

void cva6_wrapper::debug_resume() {
    if (impl_->iss_core->in_debug) {
        resume_pending_ = true;
        impl_->iss_core->wfi_event.notify(sc_core::SC_ZERO_TIME);
    }
}

std::uint64_t cva6_wrapper::debug_entries() const {
    return impl_->iss_core->debug_entries;
}

bool cva6_wrapper::debug_access_register(std::uint16_t regno, bool write, std::uint32_t& value) {
    auto& iss = *impl_->iss_core;
    if (!debug_halted()) {
        return false;
    }
    if (regno >= 0x1000 && regno <= 0x101f) {
        const unsigned r = regno - 0x1000;
        if (write) {
            if (r) {
                iss.regs[r] = static_cast<int32_t>(value);
            }
        } else {
            value = static_cast<uint32_t>(iss.regs[r]);
        }
        return true;
    }
    if (regno >= 0x1020 && regno <= 0x103f) {
        if (!(iss.csrs.misa.fields.extensions & fu3_rv32::csr_misa::F)) {
            return false;
        }
        const unsigned r = regno - 0x1020;
        if (write) {
            iss.fp_regs.write(r, float32_t{value});
        } else {
            value = iss.fp_regs.u32(r);
        }
        return true;
    }
    if (regno > 0x0fff) {
        return false;
    }
    // CSRs with the access rules of Debug Mode (machine privilege).
    try {
        if (iss.is_invalid_csr_access(regno, write)) {
            return false;
        }
        if (write) {
            iss.set_csr_value(regno, value);
        } else {
            value = iss.get_csr_value(regno);
        }
    } catch (const SimulationTrap&) {
        return false;
    }
    return true;
}

std::string cva6_wrapper::backend_name() const {
    return "CVA6";
}

// Số lệnh đã thực hiện (để đo hiệu năng). 0 nếu backend không thể báo cáo.
std::uint64_t cva6_wrapper::get_instret() const {
    return impl_->iss_core->total_num_instr; 
}

void cva6_wrapper::start_of_simulation() {
    std::cout << "CVA6 Wrapper: Starting simulation..." << std::endl;
    /* if (elf_path_.empty()) {
        // elf_path_ = "../fw/hello_baremetal_riscv/hello.elf";
        // elf_path_ = "../fw/uart_test_riscv/uart_test.elf";
        // elf_path_ = "..fw/timer_irq_riscv/timer_irq.elf";
        
    } */
    
    if (!elf_path_.empty()) {
        std::cout << "Loading Firmware from: " << elf_path_ << std::endl;
        // Lúc này cáp Bus đã nối xong, ta mới bơm dữ liệu và lấy entry_pc chuẩn
        // cfg.reset_pc = cdc::cpu::load_elf(impl_->d_socket, elf_path_);
        // Dòng trên không dùng vì ghi đè vector reset do platform đặt (FU3 boot_addr_i = 0x0001_0000)
        elf_entry_pc_ = cdc::cpu::load_elf(impl_->bus_socket, elf_path_);
        elf_loaded_ = true;
    }
    // Gọi reset 1 lần duy nhất ở đây
    reset_cpu();
    reset_pending_ = false;
    initialised_ = true;
}

void cva6_wrapper::set_clock_period(sc_core::sc_time period) {
    clock_period_ = period;
    clock_pending_ = true;
    if (period != sc_core::SC_ZERO_TIME) {
        clock_started_.notify(sc_core::SC_ZERO_TIME);
    }
}

void cva6_wrapper::set_reset(bool asserted) {
    // The architectural reset itself runs in the CPU thread (run()) at a safe
    // point: an instruction that was waiting on the bus when reset came must
    // not commit into the state after reset (it is overwritten there).
    if (asserted) {
        in_reset_ = true;             // CPU dừng sau lệnh đang chạy
        reset_pending_ = true;
        // A core waiting in WFI must see the reset too, not sleep through it,
        // including a WFI still being fetched now (level, cleared by the reset).
        impl_->iss_core->halt_request = true;
        impl_->iss_core->wfi_event.notify(sc_core::SC_ZERO_TIME);
        return;
    }
    if (!in_reset_) {
        return;
    }
    in_reset_ = false;
    if (sc_core::sc_is_running()) {
        reset_released_.notify(sc_core::SC_ZERO_TIME);
    }
}


void cva6_wrapper::run() {
    sc_core::wait(sc_core::SC_ZERO_TIME); 
    auto& qk = impl_->iss_core->quantum_keeper;
    while (true) {
        if (in_reset_) {                      // FU3: chờ Ibex nhả reset 
            qk.sync();                        // may wait up to a quantum
            if (in_reset_) {                  // a short pulse can end during the sync
                sc_core::wait(reset_released_);
            }
            continue;
        }
        if (reset_pending_) {                 // reset asserted since the last instruction
            reset_pending_ = false;
            resume_pending_ = false;
            if (initialised_) {
                reset_cpu();
            }
        }
        if (resume_pending_) {                // Debug Module resume: DRET at a boundary
            resume_pending_ = false;
            if (impl_->iss_core->in_debug) {
                impl_->iss_core->leave_debug();
            }
        }
        if (clock_pending_) {                 // clock change at an instruction boundary
            if (clock_period_ == sc_core::SC_ZERO_TIME) {
                qk.sync();
                sc_core::wait(clock_started_);   // clock stopped: no instruction
                continue;
            }
            clock_pending_ = false;
            impl_->iss_core->set_cycle_time(clock_period_);
        }
        // Time advances only through the ISS: each instruction adds its cycle
        // cost and the quantum keeper yields once per global quantum (no extra
        // wrapper delay, decision Q01-g).
        impl_->iss_core->run_step();
    }
}

}  // namespace cdc::cpu
