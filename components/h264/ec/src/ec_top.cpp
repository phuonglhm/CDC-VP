#include "ec_top.h"
#include <h264/block_transport.h>
#include "syntax_builder.h"
#include "nal_formatter.h"
// #include "h264_tlm_utils.h"

#include <cstring>

namespace h264::ec {

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

EcTop::EcTop(sc_core::sc_module_name name, bool test_payload, bool variable_payload) : sc_core::sc_module(name), transport_test_payload_(test_payload), variable_test_payload_(variable_payload), cavlc_("cavlc"), cabac_("cabac") {
    socket.register_b_transport(this, &EcTop::b_transport);
}

void EcTop::reset() { request_={}; result_={}; loaded_=0; }

void EcTop::start() {
    result_.valid = false;
    if(transport_test_payload_) {
        if(variable_test_payload_) {
            // Non-codec sparse record: TV, QP, nonzero count; (index, LE16 level).
            // Length is unpadded bytes; the DMA adapter owns final word padding.
            result_.nal_stream.fill(0);
            result_.nal_stream[0]='T';result_.nal_stream[1]='V';result_.nal_stream[2]=request_.qp;
            unsigned n=4,count=0;
            for(unsigned i=0;i<16;++i) if(request_.levels[i]) {
                auto v=static_cast<std::uint16_t>(request_.levels[i]);
                result_.nal_stream[n++]=i;result_.nal_stream[n++]=v&255;result_.nal_stream[n++]=v>>8;++count;
            }
            result_.nal_stream[3]=count;result_.stream_length=n;result_.valid=true;return;
        }
        // Test record: "TC", QP, mode, then 16 signed little-endian levels.
        // NOT Annex-B, NOT an H.264 syntax implementation.
        result_.nal_stream.fill(0);
        result_.nal_stream[0]='T'; result_.nal_stream[1]='C';
        result_.nal_stream[2]=request_.qp;
        result_.nal_stream[3]=request_.entropy_coding_mode;
        for(unsigned i=0;i<16;++i) {
            auto v=static_cast<std::uint16_t>(request_.levels[i]);
            result_.nal_stream[4+2*i]=v&255;
            result_.nal_stream[5+2*i]=v>>8;
        }
        result_.stream_length=36; result_.valid=true; return;
    }
    BitWriter bw(result_.nal_stream, result_.stream_length);

    SyntaxBuilder::build_slice_header(request_, bw);

    if (request_.entropy_coding_mode == 0)
        cavlc_.encode(request_, bw);
    else
        cabac_.encode(request_, bw);

    bw.rbsp_trailing_bits();
    NalFormatter::wrap_nal_unit(result_, request_.nal_ref_idc, request_.nal_unit_type);

    result_.valid = true;
}

void EcTop::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    if (!h264::block_payload(tx)) return;
    delay += sc_core::sc_time(1, sc_core::SC_NS);
    auto* data = tx.get_data_ptr();

    if (!data) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    const auto addr = tx.get_address();
    const auto len = tx.get_data_length();

    if (tx.get_command() == tlm::TLM_WRITE_COMMAND) {
        if (addr == 0x00 && len == 32) {
            load_array_le(data, request_.levels.data(), 16); loaded_|=1; result_.valid=false;
        } else if (addr == 0x20 && len == 1) {
            if(data[0]>51) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            request_.qp = data[0]; loaded_|=2;
        } else if (addr == 0x24 && len == 1) {
            if(data[0]>1) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            request_.entropy_coding_mode = data[0]; loaded_|=4;
        } else if (addr == 0x28 && len == 1) {
            if (data[0] & 0x80u) {
                if(loaded_!=7) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
                try { start(); loaded_ &= ~1u; }
                catch(const std::exception&) {
                    result_={}; tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
                }
            }
        } else {
            std::cout << "\n[DUT ERROR] WRITE bị từ chối. Địa chỉ: 0x" << std::hex << addr << " Độ dài: " << std::dec << len << "\n";
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    } else if (tx.get_command() == tlm::TLM_READ_COMMAND) {
        if (addr == 0x40 && len == 4) {
            temp_store_u32_le(data, result_.valid ? 1u : 0u);
        } else if (addr == 0x50 && len == 128) {
            store_array_le(data, result_.nal_stream.data(), 128);
        } else if (addr == 0xD0 && len == 4) {
            temp_store_u32_le(data, result_.stream_length);
        } else {
            std::cout << "\n[DUT ERROR] READ bị từ chối. Địa chỉ: 0x" << std::hex << addr << " Độ dài: " << std::dec << len << "\n";
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

} // namespace h264::ec
