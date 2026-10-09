#include "df_top.h"

#include <cstring>
#include <iostream>

namespace h264::df {
namespace {
    template <typename T>
    void load_array_le(const unsigned char* src, T* dst, std::size_t count) { 
        std::memcpy(dst, src, sizeof(T) * count); 
    }

    template <typename T>
    void store_array_le(unsigned char* dst, const T* src, std::size_t count) { 
        std::memcpy(dst, src, sizeof(T) * count); 
    }

    inline void temp_store_u32_le(unsigned char* p, std::uint32_t v) {
        p[0] = static_cast<unsigned char>(v & 0xffu); 
        p[1] = static_cast<unsigned char>((v >> 8) & 0xffu);
        p[2] = static_cast<unsigned char>((v >> 16) & 0xffu); 
        p[3] = static_cast<unsigned char>((v >> 24) & 0xffu);
    }
}

DfTop::DfTop(sc_core::sc_module_name name) 
    : sc_core::sc_module(name), line_mem_("line_mem"), filter_("filter") {
    socket.register_b_transport(this, &DfTop::b_transport);
}

void DfTop::start() {
    result_.valid = false;
    
    // Kéo Old Block từ Line Memory ra làm Left Block
    line_mem_.read_block(result_.left_filtered);
    // Lấy New Block đang nhận làm Right Block
    result_.right_filtered = request_.current_block;

    // Đưa qua Engine lọc
    filter_.apply_filter(result_.left_filtered, result_.right_filtered, request_.bs, request_.qp);
    
    // Lọc xong => cất Right Block vào Line Memory để chuẩn bị cho chu kỳ tiếp theo
    line_mem_.write_block(result_.right_filtered);

    result_.valid = true;
}

void DfTop::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    delay += sc_core::sc_time(1, sc_core::SC_NS);
    auto* data = tx.get_data_ptr();
    if (!data) { 
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); 
        return; 
    }

    const auto addr = tx.get_address();
    const auto len = tx.get_data_length();
    
    if (tx.get_command() == tlm::TLM_WRITE_COMMAND) {
        if (addr == 0x00 && len >= 16) {
            // Testbench mồi sẵn data vào Line Memory 
            std::array<std::uint8_t, 16> init_buf;
            load_array_le(data, init_buf.data(), 16);
            line_mem_.write_block(init_buf);
        }
        else if (addr == 0x10 && len >= 16) 
            load_array_le(data, request_.current_block.data(), 16);
        else if (addr == 0x20 && len >= 1) 
            request_.bs = data[0];
        else if (addr == 0x24 && len >= 1) 
            request_.qp = data[0];
        else if (addr == 0x28 && len >= 1) { 
            if (data[0] & 0x80u) start(); 
        } else { 
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); 
            return; 
        }
    } else if (tx.get_command() == tlm::TLM_READ_COMMAND) {
        if (addr == 0x40 && len >= 4)
            temp_store_u32_le(data, result_.valid ? 1u : 0u);
        else if (addr == 0x50 && len >= 16)
             store_array_le(data, result_.left_filtered.data(), 16); 
        else if (addr == 0x60 && len >= 16) 
            store_array_le(data, result_.right_filtered.data(), 16); 
        else { 
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); 
            return; 
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

} // namespace h264::df