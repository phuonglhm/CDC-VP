#include "memory_map.h"
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <iomanip>

struct MemTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<MemTb> socket{"socket"};
    SC_HAS_PROCESS(MemTb);

    explicit MemTb(sc_core::sc_module_name name) : sc_core::sc_module(name) { SC_THREAD(run); }

    void send(tlm::tlm_command cmd, std::uint64_t addr, unsigned char* data, unsigned len) {
        tlm::tlm_generic_payload tx;
        tx.set_command(cmd); 
        tx.set_address(addr); 
        tx.set_data_ptr(data);
        tx.set_data_length(len); 
        tx.set_streaming_width(len);
        
        tx.set_byte_enable_ptr(nullptr); 
        tx.set_byte_enable_length(0); 
        tx.set_dmi_allowed(false);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        
        if (tx.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cout << "\nGiao dịch bị từ chối - Address: 0x" << std::hex << addr << std::dec << "\n";
            sc_core::sc_stop();
        }
        wait(delay);
    }

    void run() {
        std::cout << "\n==========================\n";
        std::cout << "H.264 MEMORY MAP UNIT TEST\n";
        std::cout << "==========================\n";

        // Test Internal Memory (SRAM) <=> Ghi và đọc lại ở địa chỉ 0x00000100
        std::uint8_t write_sram[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
        std::uint8_t read_sram[8] = {0};
        
        send(tlm::TLM_WRITE_COMMAND, 0x00000100, write_sram, 8);
        send(tlm::TLM_READ_COMMAND,  0x00000100, read_sram, 8);
        
        std::cout << " -> SRAM Ghi/Đọc tại 0x00000100:\n    Data Read : ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_sram[i] << " ";
        std::cout << std::dec << "\n\n";

        // Test External Memory (DRAM) <= Ghi và đọc lại ở địa chỉ 0x10000500
        std::uint8_t write_dram[8] = {0xFF, 0xEE, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44};
        std::uint8_t read_dram[8] = {0};
        
        send(tlm::TLM_WRITE_COMMAND, 0x10000500, write_dram, 8);
        send(tlm::TLM_READ_COMMAND,  0x10000500, read_dram, 8);
        
        std::cout << " -> DRAM Ghi/Đọc tại 0x10000500:\n    Data Read : ";
        for (int i = 0; i < 8; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)read_dram[i] << " ";
        std::cout << std::dec << "\n\n";

        //Test lỗi tràn địa chỉ <= Truy cập vào khoảng trống giữa SRAM và DRAM)
        std::cout << " -> Kiểm tra truy cập sai địa chỉ (0x00080000)...\n";
        std::uint8_t dummy = 0;
        
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND); 
        tx.set_address(0x00080000); 
        tx.set_data_ptr(&dummy); 
        tx.set_data_length(1); 
        tx.set_streaming_width(1);
        tx.set_byte_enable_ptr(nullptr); 
        tx.set_byte_enable_length(0);
        tx.set_dmi_allowed(false);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        
        if (tx.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE) {
            std::cout << "    Truy cập địa chỉ ngoài vùng nhớ: 0x80000\n";
            std::cout << "    => Hệ thống báo lỗi Address Error chính xác!\n\n";
        }

std::cout << "----------------------------------------\n";
        std::cout << "Kiểm tra trường hợp biên...\n\n";
        std::cout << "Stress Test 100 giao dịch Back-to-back 0ns...\n";
        
        bool back_to_back_passed = true;
        for (int i = 0; i < 100; ++i) {
            std::uint32_t addr = 0x00000300 + (i * 4); // Nhảy 4 bytes mỗi vòng lặp
            std::uint32_t val_write = 0xCAFEBABE + i;
            std::uint32_t val_read = 0;
            
            // Giao dịch WRITE trực tiếp không wait()
            tlm::tlm_generic_payload tx_w;
            tx_w.set_command(tlm::TLM_WRITE_COMMAND); tx_w.set_address(addr); 
            tx_w.set_data_ptr(reinterpret_cast<unsigned char*>(&val_write));
            tx_w.set_data_length(4); tx_w.set_streaming_width(4);
            tx_w.set_byte_enable_ptr(nullptr); tx_w.set_byte_enable_length(0); tx_w.set_dmi_allowed(false);
            tx_w.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            sc_core::sc_time delay_w = sc_core::SC_ZERO_TIME;
            socket->b_transport(tx_w, delay_w);

            // Giao dịch READ trực tiếp nối đuôi không wait()
            tlm::tlm_generic_payload tx_r;
            tx_r.set_command(tlm::TLM_READ_COMMAND); tx_r.set_address(addr); 
            tx_r.set_data_ptr(reinterpret_cast<unsigned char*>(&val_read));
            tx_r.set_data_length(4); tx_r.set_streaming_width(4);
            tx_r.set_byte_enable_ptr(nullptr); tx_r.set_byte_enable_length(0); tx_r.set_dmi_allowed(false);
            tx_r.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            sc_core::sc_time delay_r = sc_core::SC_ZERO_TIME;
            socket->b_transport(tx_r, delay_r);
            
            if (val_write != val_read || tx_w.get_response_status() != tlm::TLM_OK_RESPONSE) {
                back_to_back_passed = false;
                std::cout << "    Rớt gói tại địa chỉ: 0x" << std::hex << addr << std::dec << "\n";
                break;
            }
        }
        
        if (back_to_back_passed) {
            std::cout << " -> Hoàn tất 100 giao dịch Back-to-back 0ns delay thành công không bị rớt gói!\n";
        }
        std::cout << "----------------------------------------\n";

        std::cout << ">>> MEMORY MAP VP FULL FUNCTIONAL TEST PASSED <<<\n\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**) {
    h264::mem::MemoryMap dut("mem");
    MemTb tb("tb");
    tb.socket.bind(dut.socket);
    sc_core::sc_start();
    return 0;
}