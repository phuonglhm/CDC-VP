#include "video_encoder_tlm.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "block.h"
#include "prediction_result.h"

namespace cdc::components {


video_encoder_tlm::video_encoder_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &video_encoder_tlm::b_transport);
    socket.register_transport_dbg(this, &video_encoder_tlm::transport_dbg);
}

void video_encoder_tlm::load_input_frame(const frame& input)
{
    input_frame_ = input;
    status_ = 0;
}

std::vector<std::uint8_t> video_encoder_tlm::encode_frame()
{
    run_pipeline();
    return last_bitstream_;
}

std::size_t video_encoder_tlm::last_bitstream_size() const
{
    return last_bitstream_.size();
}

void video_encoder_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += access_latency_;

    const auto addr = trans.get_address();
    const auto len = trans.get_data_length();
    auto* ptr = trans.get_data_ptr();

    if (ptr == nullptr || len != sizeof(std::uint32_t) || (addr % sizeof(std::uint32_t)) != 0u) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        bool ok = false;
        const std::uint32_t value = read_reg(addr, ok);
        if (!ok) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        std::memcpy(ptr, &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        if (!write_reg(addr, value)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
}

unsigned int video_encoder_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const auto addr = trans.get_address();
    const auto len = trans.get_data_length();
    auto* ptr = trans.get_data_ptr();

    if (ptr == nullptr || len != sizeof(std::uint32_t) || (addr % sizeof(std::uint32_t)) != 0u) {
        return 0;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        bool ok = false;
        const std::uint32_t value = read_reg(addr, ok);
        if (!ok) {
            return 0;
        }
        std::memcpy(ptr, &value, sizeof(value));
        return sizeof(value);
    }

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        return write_reg(addr, value) ? sizeof(value) : 0u;
    }

    return 0;
}

std::uint32_t video_encoder_tlm::read_reg(std::uint64_t addr, bool& ok) const
{
    ok = true;
    switch (addr) {
    case REG_CONTROL:
        return control_;
    case REG_STATUS:
        return status_;
    case REG_OUTPUT_SIZE:
        return static_cast<std::uint32_t>(last_bitstream_.size());
    default:
        ok = false;
        return 0;
    }
}

bool video_encoder_tlm::write_reg(std::uint64_t addr, std::uint32_t value)
{
    switch (addr) {
    case REG_CONTROL:
        control_ = value;
        if ((value & CONTROL_START) != 0u) {
            run_pipeline();
        }
        return true;
    case REG_STATUS:
    case REG_OUTPUT_SIZE:
        return false;
    default:
        return false;
    }
}

void video_encoder_tlm::run_pipeline()
{
    const std::uint32_t min_dim = std::min(input_frame_.width, input_frame_.height);
    block region {0, 0, min_dim == 0 ? 16u : std::min(min_dim, 16u)};

    std::vector<prediction_result> candidates;
    candidates.push_back(prei_.run(input_frame_, region));
    candidates.push_back(posi_.run(input_frame_, region));
    candidates.push_back(fme_.refine(ime_.run(input_frame_, region)));

    const prediction_result best = mode_decision_.choose(candidates);
    reconstructed_frame_ = rec_.reconstruct(input_frame_, region);
    last_bitstream_ = cabac_.encode(reconstructed_frame_, best);
    status_ = STATUS_DONE;
}

} // namespace cdc::components
