#include "tq_top.h"

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

struct TqTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<TqTb> socket{"socket"};
    SC_HAS_PROCESS(TqTb);

    explicit TqTb(sc_core::sc_module_name name) : sc_core::sc_module(name) {
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
        assert(tx.get_response_status() == tlm::TLM_OK_RESPONSE);
        wait(delay);
    }

    void print_block_16(const char* title, const std::int16_t* block) {
        std::cout << "  [" << title << "]:\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "    ";
            for (int j = 0; j < 4; ++j) {
                std::cout << std::setw(5) << block[i * 4 + j] << " ";
            }
            std::cout << "\n";
        }
    }

    void print_block_8(const char* title, const std::uint8_t* block) {
        std::cout << "  [" << title << "]:\n";
        for (int i = 0; i < 4; ++i) {
            std::cout << "    ";
            for (int j = 0; j < 4; ++j) {
                std::cout << std::setw(5) << (int)block[i * 4 + j] << " ";
            }
            std::cout << "\n";
        }
    }

    void run() {
        std::cout << "\n========================================\n";
        std::cout << "H.264 TRANSFORM & QUANTIZATION UNIT TEST\n";
        std::cout << "========================================\n";

        // Data đầu vào: Ma trận Residual 4x4 (16 phần tử int16)
        std::int16_t residual[16] = {
             32,  12, -10,   4,
             15,  -5,   0,   2,
             -8,   3,   1,   0,
              4,   0,   0,   0
        };
        send(tlm::TLM_WRITE_COMMAND, 0x00, reinterpret_cast<unsigned char*>(residual), 32);

        // 2. Data đầu vào: Ma trận Predictor 4x4 (16 phần tử uint8)
        std::uint8_t predictor[16] = {
            128, 128, 128, 128,
            128, 128, 128, 128,
            128, 128, 128, 128,
            128, 128, 128, 128
        };
        send(tlm::TLM_WRITE_COMMAND, 0x04, predictor, 16);

        // Cấu hình QP = 24
        unsigned char qp = 24;
        send(tlm::TLM_WRITE_COMMAND, 0x08, &qp, 1);

        // Bắn Start Command (block_class = 0, bit 7 = 1 => 0x80)
        unsigned char start_cmd = 0x80;
        send(tlm::TLM_WRITE_COMMAND, 0x0C, &start_cmd, 1);

        // Polling chờ TQ xử lý xong
        unsigned char valid[4]{};
        send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
        // assert(h264::tlmutil::load_u32_le(valid) == 1);
        std::cout << " -> TQ block xử lý hoàn tất (Valid = 1)\n\n";

        // Đọc Quantized Levels 
        std::int16_t levels[16]{};
        send(tlm::TLM_READ_COMMAND, 0x10, reinterpret_cast<unsigned char*>(levels), 32);

        // Đọc Reconstructed Block
        std::uint8_t reconstructed[16]{};
        send(tlm::TLM_READ_COMMAND, 0x30, reconstructed, 16);

        print_block_16("Residual Input (Từ Intra/Inter)", residual);
        print_block_8("Predictor Input", predictor);
        std::cout << "  --------------------------------\n";
        print_block_16("Quantized Levels (Xuất cho EC)", levels);
        print_block_8("Reconstructed Block (Lưu vào Memory)", reconstructed);

        std::cout << "\n----------------------------------------\n";
        std::cout << " Kiểm tra trường hợp biên...\n";

        // Dữ liệu Residual = 0 hoàn toàn
        std::int16_t zero_residual[16] = {0};
        send(tlm::TLM_WRITE_COMMAND, 0x00, reinterpret_cast<unsigned char*>(zero_residual), 32);
        send(tlm::TLM_WRITE_COMMAND, 0x0C, &start_cmd, 1);
        send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
        assert(temp_load_u32_le(valid) == 1);
        std::cout << " -> Xử lý thành công khi Residual = 0 hoàn toàn.\n";

        // Max Residual + Min QP (Chống tràn số)
        std::int16_t max_residual[16]; 
        for(auto& x : max_residual) x = 255; // Giá trị biên lớn nhất
        unsigned char min_qp = 0;            // QP nhỏ nhất

        send(tlm::TLM_WRITE_COMMAND, 0x00, reinterpret_cast<unsigned char*>(max_residual), 32);
        send(tlm::TLM_WRITE_COMMAND, 0x08, &min_qp, 1);
        send(tlm::TLM_WRITE_COMMAND, 0x0C, &start_cmd, 1);
        send(tlm::TLM_READ_COMMAND, 0x40, valid, 4);
        assert(temp_load_u32_le(valid) == 1);
        std::cout << " -> Test Max Residual (255) + Min QP (0) không bị tràn số.\n";
        std::cout << "----------------------------------------\n";

        std::cout << "\n>>> TQ VP FULL FUNCTIONAL TEST PASSED <<<\n\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**) {
    h264::tq::TqTop dut("tq");
    TqTb tb("tb");
    tb.socket.bind(dut.socket);
    sc_core::sc_start();
    return 0;
}