#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <cdc/cpu/cpu_base.h>

namespace cdc::cpu {


class cva6_wrapper : public cpu_base {
public:
    explicit cva6_wrapper(sc_core::sc_module_name name, const cpu_config& config = cpu_config{});
    ~cva6_wrapper() override;

    // 2 cổng AXI riêng biệt cho lệnh và dữ liệu
    tlm::tlm_initiator_socket<>& instr_bus() override;
    tlm::tlm_initiator_socket<>& data_bus() override;

    // Hệ thống không xài bus gộp vì 2 cổng AXI riêng biệt
    bool has_unified_bus() const override { return false; }

    // Nhận ngắt từ bộ điều khiển ngắt (CLINT/PLIC - Soft, Timer, External)
    // Đẩy vào MIP register của CVA6
    void set_irq(unsigned cause, bool level) override;


    void load_elf(const std::string& path) override;
    void reset_cpu() override;
    
    std::uint64_t get_pc() const override;
    std::string backend_name() const override;
    
    // Số lệnh đã thực hiện (để đo hiệu năng). 0 nếu backend không thể báo cáo.
    std::uint64_t get_instret() const override;

private:
    
    // Hàm này tự động chạy ngay trước khi thời gian mô phỏng bắt đầu (t = 0).
    // Chỉ dùng để nạp file ELF và reset PC sau khi các dây nối Bus đã liên kết xong.
    void start_of_simulation() override;

    // Gom ISS + bộ nhớ + runner để cho phép triển khai riêng tư, tránh đưa các header của CVA6 vào giao diện công khai.
    struct impl;               
    std::unique_ptr<impl> impl_;
    std::string elf_path_;
    std::uint64_t entry_pc_ = 0;
    std::uint64_t elf_entry_pc_ = 0;
    bool initialised_ = false;

    void run(); 
};

}  // namespace cdc::cpu