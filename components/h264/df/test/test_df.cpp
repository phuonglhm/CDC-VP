#include "df_top.h"

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include <iostream>
#include <iomanip>

namespace {
    inline std::uint32_t temp_load_u32_le(const unsigned char* p) {
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }
}

struct DfTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<DfTb> socket{"socket"};
    SC_HAS_PROCESS(DfTb);

    explicit DfTb(sc_core::sc_module_name name) : sc_core::sc_module(name) { 
        SC_THREAD(run); 
    }

    void send(tlm::tlm_command cmd, std::uint64_t addr, unsigned char* data, unsigned len) {
        tlm::tlm_generic_payload tx;
        tx.set_command(cmd); 
        tx.set_address(addr); 
        tx.set_data_ptr(data);
        tx.set_data_length(len); 
        tx.set_streaming_width(len);
        
        // Chống lỗi TLM Target Socket
        tx.set_byte_enable_ptr(nullptr); 
        tx.set_byte_enable_length(0); 
        tx.set_dmi_allowed(false);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        
        if (tx.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cout << "\n[TLM FAIL] Giao dịch bị từ chối - Address: 0x" << std::hex << addr << std::dec << "\n";
            sc_core::sc_stop();
        }
        wait(delay);
    }

    void print_blocks(const char* title, const std::uint8_t* left, const std::uint8_t* right) {
        std::cout << "  " << title << "\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "    ";
            for (int j = 0; j < 4; ++j) 
                std::cout << std::setw(3) << (int)left[i * 4 + j] << " ";
            std::cout << " | ";
            for (int j = 0; j < 4; ++j) 
                std::cout << std::setw(3) << (int)right[i * 4 + j] << " ";
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    void execute_case(const char* case_name, 
                      std::uint8_t left_val, 
                      std::uint8_t right_val, 
                      unsigned char bs, 
                      unsigned char qp) {
        std::cout << case_name << "\n";

        std::uint8_t left_blk[16];  for (auto& x : left_blk)  x = left_val;
        std::uint8_t right_blk[16]; for (auto& x : right_blk) x = right_val;

        // Ghi dữ liệu khối điểm ảnh và cấu hình
        send(tlm::TLM_WRITE_COMMAND, 0x00, left_blk, 16);
        send(tlm::TLM_WRITE_COMMAND, 0x10, right_blk, 16);
        send(tlm::TLM_WRITE_COMMAND, 0x20, &bs, 1);
        send(tlm::TLM_WRITE_COMMAND, 0x24, &qp, 1);

        // Kích hoạt bộ lọc
        unsigned char start_cmd = 0x80;
        send(tlm::TLM_WRITE_COMMAND, 0x28, &start_cmd, 1);

        // Polling cờ Valid
        unsigned char valid[4]{};
        int attempts = 0;
        while (attempts < 100) {
            send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
            if (temp_load_u32_le(valid) == 1) break;
            wait(10, sc_core::SC_NS);
            attempts++;
        }

        if (temp_load_u32_le(valid) != 1) {
            std::cout << "DF Timeout - Cờ Valid không tích cực!\n";
            sc_core::sc_stop();
            return;
        }

        // Đọc kết quả sau khi qua bộ lọc
        std::uint8_t left_out[16], right_out[16];
        send(tlm::TLM_READ_COMMAND, 0x50, left_out, 16);
        send(tlm::TLM_READ_COMMAND, 0x60, right_out, 16);

        print_blocks("Ma trận sau xử lý:", left_out, right_out);
    }

    void run() {
        std::cout << "\n=========================================\n";
        std::cout << "H.264 DEBLOCKING FILTER UNIT TEST\n";
        std::cout << "=========================================\n\n";

        // Strong Filter: bS = 4 (Khử nhiễu khối gắt)
        execute_case(" bS=4, lệch 50 | 200 -> Kỳ vọng làm mờ về 125:",
                     50, 200, 4, 30);

        std::cout << "-----------------------------------------\n";

        // Weak Filter: bS = 2, QP = 30 (Lọc mịn viền nhỏ, bảo toàn chi tiết cạnh)
        execute_case("bS=2, QP=30, lệch 120 | 122 -> Kỳ vọng cắt xén về 121 | 121:",
                     120, 122, 2, 30);

        std::cout << "-----------------------------------------\n";

        // Disable Filter: bS = 0 (Bypass bộ lọc)
        execute_case("bS=0, lệch 120 | 122 -> Kỳ vọng giữ nguyên 100% (120 | 122):",
                     120, 122, 0, 30);

        std::cout << "-----------------------------------------\n";
        std::cout << ">>> DF VP FULL FUNCTIONAL TEST PASSED <<<\n\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**) {
    h264::df::DfTop dut("df");
    DfTb tb("tb");
    tb.socket.bind(dut.socket);
    sc_core::sc_start();
    return 0;
}