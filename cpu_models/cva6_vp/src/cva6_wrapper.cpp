#include "cva6_wrapper.h"
#include <stdexcept>
#include <iostream>

#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cdc/cpu/elf_loader.h>

#include "core/rv32/iss.h"

#include "core/common/clint_if.h"

namespace cdc::cpu {

struct stub_clint : public clint_if {
    std::uint64_t update_and_get_mtime() override {
        return static_cast<std::uint64_t>(sc_core::sc_time_stamp().value());
    }
};

struct CVA6_InstrAdapter : public rv32::instr_memory_if {
    tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& i_socket;

    // i_socket truyền vào
    CVA6_InstrAdapter(tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& socket) 
        : i_socket(socket) {}

    uint32_t load_instr(uint64_t addr) override {
        uint32_t instruction = 0;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::sc_time(10, sc_core::SC_NS); // Đồng bộ 10ns cho Global Quantum

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&instruction));
        trans.set_data_length(4); // Lệnh RV32 luôn là 4 bytes
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        i_socket->b_transport(trans, delay);

        if (trans.is_response_error()) {
            // Nếu Bus báo lỗi, ném ngoại lệ để ISS bắt thành trap Access Fault
            throw std::runtime_error("Bus error luc fetch lenh tai dia chi: " + std::to_string(addr));
        }
        return instruction;
    }
};

struct CVA6_DataAdapter : public rv32::data_memory_if {
    tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& d_socket;

    CVA6_DataAdapter(tlm_utils::simple_initiator_socket<cva6_wrapper, 32>& socket) 
        : d_socket(socket) {}

    void tlm_access(uint64_t addr, uint8_t* data, unsigned int len, tlm::tlm_command cmd) {
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
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME; 

        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        d_socket->b_transport(trans, delay);

        if (trans.is_response_error()) {
            throw std::runtime_error("Data Bus error tai dia chi: " + std::to_string(addr));
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
        return true; // VP chạy single-core nên SC luôn báo thành công
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

    tlm_utils::simple_initiator_socket<cva6_wrapper, 32> i_socket;
    tlm_utils::simple_initiator_socket<cva6_wrapper, 32> d_socket;

    CVA6_InstrAdapter instr_adapter;
    CVA6_DataAdapter data_adapter;

    stub_clint clint;

    // Con trỏ chứa ISS
    std::unique_ptr<rv32::ISS> iss_core;

    impl(const cpu_config& config) : i_socket("i_socket"), 
                                   d_socket("d_socket"),
                                   instr_adapter(i_socket),  // i_socket cho Fetch
                                   data_adapter(d_socket)    // d_socket cho Data
    {
        // VP++ đòi hỏi truyền hart_id vào constructor
        // Truyền giá trị hart_id từ cfg vào, tham số thứ 2 (use_E_base_isa) để mặc định là false
        iss_core = std::make_unique<rv32::ISS>(config.hart_id, false);
    }
};

cva6_wrapper::cva6_wrapper(sc_core::sc_module_name name, const cpu_config& config)
    : cpu_base(name, config), impl_(std::make_unique<impl>(config)) {

    if (config.xlen != 32) {
        throw std::runtime_error("Invalid XLEN value. Must be 32.");
    } 
    if (!config.has_mmu) {
        throw std::runtime_error("Boot Linux requires MMU support.");
    }

    SC_HAS_PROCESS(cva6_wrapper);
    SC_THREAD(run);
}

cva6_wrapper::~cva6_wrapper() = default;

tlm::tlm_initiator_socket<>& cva6_wrapper::instr_bus() {
    return impl_->i_socket;
}

tlm::tlm_initiator_socket<>& cva6_wrapper::data_bus() {
    return impl_->d_socket;
}

void cva6_wrapper::set_irq(unsigned cause, bool level) {
    if (cause == 7) {
        impl_->iss_core->trigger_timer_interrupt(level);
    } else if (cause == 3) {
        impl_->iss_core->trigger_software_interrupt(level);
    }
}

void cva6_wrapper::load_elf(const std::string& path) {
    // Luồng chạy BootROM: Chỉ ghi nhận đường dẫn, không ghi đè cfg.reset_pc bằng entry_pc của file ELF
    elf_path_ = path;
    
}

void cva6_wrapper::reset_cpu() {
    uint64_t start_pc = cfg.reset_pc_specified() ? cfg.reset_pc : 0x80000000;

    std::cout << "[CVA6] reset PC = 0x" << std::hex << start_pc << std::dec << std::endl;
    
    impl_->iss_core->init(
        &(impl_->instr_adapter),
        &(impl_->data_adapter),
        &(impl_->clint),
        start_pc,
        0x80010000 
    );
}

std::uint64_t cva6_wrapper::get_pc() const {
    return impl_->iss_core->pc;
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
        
    } */
    
    if (!elf_path_.empty()) {
        std::cout << "Loading Firmware from: " << elf_path_ << std::endl;
        // Lúc này cáp Bus đã nối xong, ta mới bơm dữ liệu và lấy entry_pc chuẩn
        cfg.reset_pc = cdc::cpu::load_elf(impl_->d_socket, elf_path_);
    }
    // Gọi reset 1 lần duy nhất ở đây
    reset_cpu();
    initialised_ = true;
}

void cva6_wrapper::run() {
    sc_core::wait(sc_core::SC_ZERO_TIME); 
    while (true) {
        for (int i = 0; i < 1000; i++) {
            impl_->iss_core->run_step();
        }
        sc_core::wait(10, sc_core::SC_US); 
    }
}

}  // namespace cdc::cpu
