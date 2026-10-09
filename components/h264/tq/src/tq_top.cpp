#include "tq_top.h"
// Mở comment dòng này khi có common
// #include "h264_tlm_utils.hpp" 
#include <cstring>

namespace h264::tq {

namespace {
template <typename T>
void load_array_le(const unsigned char* src, T* dst, std::size_t count) {
    std::memcpy(dst, src, sizeof(T) * count);
}

template <typename T>
void store_array_le(unsigned char* dst, const T* src, std::size_t count) {
    std::memcpy(dst, src, sizeof(T) * count);
}

// Hàm mock dùng tạm để không bị phụ thuộc common
inline void temp_store_u32_le(unsigned char* p, std::uint32_t v) {
    p[0] = static_cast<unsigned char>(v & 0xffu);
    p[1] = static_cast<unsigned char>((v >> 8) & 0xffu);
    p[2] = static_cast<unsigned char>((v >> 16) & 0xffu);
    p[3] = static_cast<unsigned char>((v >> 24) & 0xffu);
}
} // namespace

TqTop::TqTop(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      transpose_("transpose"),
      ftq_(transpose_),
      itq_() {
    socket.register_b_transport(this, &TqTop::b_transport);
}

void TqTop::start() {
    const auto coeff = ftq_.transform(request_.residual, request_.block_class);
    result_.levels = ftq_.quantize(coeff, request_.qp, request_.block_class);

    result_.reconstructed_residual =
        itq_.inverse_transform(result_.levels, request_.qp);

    result_.reconstructed =
        itq_.reconstruct(result_.reconstructed_residual, request_.predictor);

    result_.valid = true;
}

void TqTop::b_transport(tlm::tlm_generic_payload& tx,
                        sc_core::sc_time& delay) {
    delay += sc_core::sc_time(1, sc_core::SC_NS);
    auto* data = tx.get_data_ptr();
    
    if (data == nullptr) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    const auto addr = tx.get_address();

    if (tx.get_command() == tlm::TLM_WRITE_COMMAND) {
        if (addr == 0x00 && tx.get_data_length() == 32) {
            load_array_le(data, request_.residual.data(), 16);
        } else if (addr == 0x04 && tx.get_data_length() == 16) {
            load_array_le(data, request_.predictor.data(), 16);
        } else if (addr == 0x08 && tx.get_data_length() >= 1) {
            request_.qp = data[0];
        } else if (addr == 0x0C && tx.get_data_length() >= 1) {
            request_.block_class =
                static_cast<BlockClass>(data[0] & 0x3u);
            if (data[0] & 0x80u) start();
        } else {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    } else if (tx.get_command() == tlm::TLM_READ_COMMAND) {
        if (addr == 0x10 && tx.get_data_length() == 32) {
            store_array_le(data, result_.levels.data(), 16);
        } else if (addr == 0x30 && tx.get_data_length() == 16) {
            store_array_le(data, result_.reconstructed.data(), 16);
        } else if (addr == 0x40 && tx.get_data_length() == 4) {
            //Khi ráp code (có common) thì đổi thành h264::tlmutil::store_u32_le(data, result_.valid ? 1u : 0u)
            temp_store_u32_le(data, result_.valid ? 1u : 0u);
        } else {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

} // namespace h264::tq