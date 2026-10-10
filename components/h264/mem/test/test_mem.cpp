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
        std::cout << "\n=========================================================\n";
        std::cout << "          H.264 MEMORY MAP UNIT TEST REPORT              \n";
        std::cout << "=========================================================\n\n";

        // Test Internal Memory (SRAM) <=> Ghi và đọc lại ở địa chỉ 0x00000100
        std::cout << "Kiểm tra phân vùng SRAM (Internal Memory)\n";
        std::uint8_t write_sram[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
        std::uint8_t read_sram[8] = {0};
        
        std::uint32_t sram_addr = 0x00000100;
        send(tlm::TLM_WRITE_COMMAND, sram_addr, write_sram, 8);
        send(tlm::TLM_READ_COMMAND,  sram_addr, read_sram, 8);
        
        std::cout << "  - Địa chỉ kiểm tra : 0x" << std::hex << sram_addr << std::dec << "\n";
        std::cout << "  - Dữ liệu GHI vào  : ";
        for (int i = 0; i < 8; ++i) std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)write_sram[i] << " ";
        std::cout << std::dec << "\n";

        std::cout << "  - Dữ liệu ĐỌC ra   : ";
        for (int i = 0; i < 8; ++i) std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)read_sram[i] << " ";
        std::cout << std::dec << "\n";

        bool sram_match = true;
        for(int i=0; i<8; ++i) if(write_sram[i] != read_sram[i]) sram_match = false;
        
        if (sram_match) {
            std::cout << "  => TRẠNG THÁI     : \033[1;32m[PASS - ĐÚNG CHÍNH XÁC]\033[0m\n\n";
        } else {
            std::cout << "  => TRẠNG THÁI     : \033[1;31m[FAIL - DỮ LIỆU BỊ SAI LỆCH]\033[0m\n\n";
            sc_core::sc_stop();
        }

        // Test External Memory (DRAM) <= Ghi và đọc lại ở địa chỉ 0x10000500
        std::cout << "iểm tra phân vùng DRAM (External Memory)\n";
        std::uint8_t write_dram[8] = {0xFF, 0xEE, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44};
        std::uint8_t read_dram[8] = {0};
        
        std::uint32_t dram_addr = 0x10000500;
        send(tlm::TLM_WRITE_COMMAND, dram_addr, write_dram, 8);
        send(tlm::TLM_READ_COMMAND,  dram_addr, read_dram, 8);
        
        std::cout << "  - Địa chỉ kiểm tra : 0x" << std::hex << dram_addr << std::dec << "\n";
        std::cout << "  - Dữ liệu GHI vào  : ";
        for (int i = 0; i < 8; ++i) std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)write_dram[i] << " ";
        std::cout << std::dec << "\n";
    
        std::cout << "  - Dữ liệu ĐỌC ra   : ";
        for (int i = 0; i < 8; ++i) std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)read_dram[i] << " ";
        std::cout << std::dec << "\n";

        bool dram_match = true;
        for(int i=0; i<8; ++i) if(write_dram[i] != read_dram[i]) dram_match = false;

        if (dram_match) {
            std::cout << "  => TRẠNG THÁI     : \033[1;32m[PASS - ĐÚNG CHÍNH XÁC]\033[0m\n\n";
        } else {
            std::cout << "  => TRẠNG THÁI     : \033[1;31m[FAIL - DỮ LIỆU BỊ SAI LỆCH]\033[0m\n\n";
            sc_core::sc_stop();
        }

        //Test lỗi tràn địa chỉ <= Truy cập vào khoảng trống giữa SRAM và DRAM)
        std::cout << "Kiểm tra bảo vệ vùng nhớ trống (Invalid Address)\n";
        std::uint32_t invalid_addr = 0x00080000;
        std::cout << "  - Cố tình truy cập : 0x" << std::hex << invalid_addr << std::dec << " (Khoảng trống giữa SRAM và DRAM)\n";
        
        std::uint8_t dummy = 0;
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND); 
        tx.set_address(invalid_addr); 
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
            std::cout << "  - Phản hồi phần cứng: Phát hiện TLM_ADDRESS_ERROR_RESPONSE\n";
            std::cout << "  => TRẠNG THÁI     : \033[1;32m[PASS - CHẶN LỖI THÀNH CÔNG]\033[0m\n\n";
        } else {
            std::cout << "  => TRẠNG THÁI     : \033[1;31m[FAIL - KHÔNG BẮT ĐƯỢC LỖI ĐỊA CHỈ]\033[0m\n\n";
            sc_core::sc_stop();
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
                std::cout << "    Rớt gói tại vòng lặp thứ " << i << ", địa chỉ: 0x" << std::hex << addr << std::dec << "\n";
                break;
            }
        }
        
        if (back_to_back_passed) {
            std::cout << "  - Đã thực thi thành công 100/100 cặp Ghi/Đọc liên tục.\n";
            std::cout << "  => TRẠNG THÁI     : \033[1;32m[PASS - BUS STABILITY HOÀN HẢO]\033[0m\n\n";
        } else {
            std::cout << "  => TRẠNG THÁI     : \033[1;31m[FAIL - BUS BỊ TREO HOẶC RỚT GÓI]\033[0m\n\n";
            sc_core::sc_stop();
        }

        std::cout << "=========================================================\n";
        std::cout << " \033[1;32m>>> MEMORY MAP VP FULL FUNCTIONAL TEST PASSED <<<\033[0m\n";
        std::cout << "=========================================================\n\n";
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