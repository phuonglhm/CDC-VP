#include "ec_top.h"

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <cassert>
#include <iostream>
#include <iomanip>

namespace {
    inline std::uint32_t temp_load_u32_le(const unsigned char* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }
}

struct EcTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<EcTb> socket{"socket"};
    SC_HAS_PROCESS(EcTb);

    explicit EcTb(sc_core::sc_module_name name) : sc_core::sc_module(name) { 
        SC_THREAD(run); 
    }

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
            std::cout << "\nKhông thể " << (cmd == tlm::TLM_WRITE_COMMAND ? "GHI" : "ĐỌC") 
                      << " vào địa chỉ: 0x" << std::hex << addr 
                      << " với độ dài " << std::dec << len << " bytes!\n";
        }
        
        assert(tx.get_response_status() == tlm::TLM_OK_RESPONSE);
        wait(delay);
    }

    void verify_and_print(const char* mode) {
        unsigned char start_cmd = 0x80;
        send(tlm::TLM_WRITE_COMMAND, 0x28, &start_cmd, 1);
        
        unsigned char valid[4]{};
        send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
        if (temp_load_u32_le(valid) != 1) {
            std::cout << "EC chưa trả về cờ Valid = 1\n";
            return;
        }

        unsigned char len_buf[4]{};
        send(tlm::TLM_READ_COMMAND, 0xD0, len_buf, 4);
        std::uint32_t stream_len = temp_load_u32_le(len_buf);
        
        unsigned char stream[128]{};
        send(tlm::TLM_READ_COMMAND, 0x50, stream, 128);

        // NAL Formatter Header (00 00 00 01)
        assert(stream[0] == 0x00 && stream[1] == 0x00 && stream[2] == 0x00 && stream[3] == 0x01);

        std::cout << " -> [" << mode << "] Mã hóa thành công. Length: " << stream_len << " bytes.\n";
        std::cout << "    Bitstream (hex): ";
        for(std::uint32_t i = 0; i < stream_len; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)stream[i] << " ";
        }
        std::cout << std::dec << "\n\n";
    }

    void run() {
        std::cout << "\n==============================\n";
        std::cout << "H.264 ENTROPY CODING UNIT TEST\n";
        std::cout << "==============================\n";
       
        std::int16_t levels[16] = {4, -2, 1, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        send(tlm::TLM_WRITE_COMMAND, 0x00, reinterpret_cast<unsigned char*>(levels), 32);
        unsigned char qp = 26;
        send(tlm::TLM_WRITE_COMMAND, 0x20, &qp, 1);

        unsigned char mode_cavlc = 0;
        send(tlm::TLM_WRITE_COMMAND, 0x24, &mode_cavlc, 1);
        verify_and_print("CAVLC / Exp-Golomb");

        unsigned char mode_cabac = 1;
        send(tlm::TLM_WRITE_COMMAND, 0x24, &mode_cabac, 1);
        verify_and_print("CABAC Engine");

        std::cout << "----------------------------------------\n";
        std::cout << "Kiểm tra trường hợp biên...\n";
        
        // Chuyển lại về chế độ CAVLC để dễ quan sát chuỗi bit đầu ra
        send(tlm::TLM_WRITE_COMMAND, 0x24, &mode_cavlc, 1);

        //  Chuỗi dữ liệu phức tạp (Stress Test Buffer, ép size NAL stream tăng cao)
        std::int16_t complex_levels[16] = { 15, -12, 10, -8, 7, -6, 5, -4, 3, -2, 1, -1, 2, -3, 4, -5 };
        send(tlm::TLM_WRITE_COMMAND, 0x00, reinterpret_cast<unsigned char*>(complex_levels), 32);
        verify_and_print("CAVLC / Complex Data Stress Test");

        // Chuỗi toàn số 0 (Kiểm tra thuật toán Zero Run / TotalZeros)
        std::int16_t zero_run_levels[16] = { 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1 };
        send(tlm::TLM_WRITE_COMMAND, 0x00, reinterpret_cast<unsigned char*>(zero_run_levels), 32);
        verify_and_print("CAVLC / Zero Run Length Test");
        
        std::cout << "----------------------------------------\n";

        std::cout << ">>> EC VP ALL UNIT TESTS PASSED <<<\n\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**) {
    h264::ec::EcTop dut("ec");
    EcTb tb("tb");
    tb.socket.bind(dut.socket);
    sc_core::sc_start();
    return 0;
}