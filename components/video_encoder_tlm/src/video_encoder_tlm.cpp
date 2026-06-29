#include "video_encoder_tlm.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace cdc::components {

video_encoder_tlm::video_encoder_tlm(sc_core::sc_module_name name,
                                     sc_core::sc_time latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , access_latency_(latency)
{
    socket.register_b_transport(this, &video_encoder_tlm::b_transport);
    socket.register_transport_dbg(this, &video_encoder_tlm::transport_dbg);
}

void video_encoder_tlm::load_input_frame(const frame& input)
{
    input_frame_ = input;

    configured_width_ = input.width;
    configured_height_ = input.height;

    // REC is not connected yet, so keep reconstructed_frame_ empty.
    // POSI will fall back to input samples for reference when this is empty.
    reconstructed_frame_ = frame {};
}

void video_encoder_tlm::load_reference_frame(const frame& reference)
{
    reference_frame_ = reference;
}

void video_encoder_tlm::set_frame_type(frame_type type)
{
    frame_type_ = type;
}

void video_encoder_tlm::set_qp(std::uint32_t qp)
{
    qp_ = clamp_qp(qp);
}

std::vector<std::uint8_t> video_encoder_tlm::encode_frame()
{
    run_pipeline();
    return last_bitstream_;
}

const frame& video_encoder_tlm::input_frame() const
{
    return input_frame_;
}

const frame& video_encoder_tlm::reference_frame() const
{
    return reference_frame_;
}

const frame& video_encoder_tlm::reconstructed_frame() const
{
    return reconstructed_frame_;
}

const std::vector<prediction_result>& video_encoder_tlm::ctu_predictions() const
{
    return ctu_predictions_;
}

const std::vector<mode_decision_result>& video_encoder_tlm::ctu_decisions() const
{
    return ctu_decisions_;
}

const std::vector<std::uint8_t>& video_encoder_tlm::last_bitstream() const
{
    return last_bitstream_;
}

std::size_t video_encoder_tlm::last_bitstream_size() const
{
    return last_bitstream_.size();
}

std::uint32_t video_encoder_tlm::bit_count() const
{
    return bit_count_;
}

void video_encoder_tlm::b_transport(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
{
    delay += access_latency_;

    const auto cmd = trans.get_command();
    const std::uint64_t addr = trans.get_address();
    unsigned char* data = trans.get_data_ptr();
    const unsigned int len = trans.get_data_length();

    if (data == nullptr || len < sizeof(std::uint32_t)) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        bool ok = false;
        const std::uint32_t value = read_reg(addr, ok);

        if (!ok) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        std::memcpy(data, &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, data, sizeof(value));

        write_reg(addr, value);

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
}

unsigned int video_encoder_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const auto cmd = trans.get_command();
    const std::uint64_t addr = trans.get_address();
    unsigned char* data = trans.get_data_ptr();
    const unsigned int len = trans.get_data_length();

    if (data == nullptr || len < sizeof(std::uint32_t)) {
        return 0;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        bool ok = false;
        const std::uint32_t value = read_reg(addr, ok);

        if (!ok) {
            return 0;
        }

        std::memcpy(data, &value, sizeof(value));
        return sizeof(value);
    }

    if (cmd == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, data, sizeof(value));

        write_reg(addr, value);
        return sizeof(value);
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

    case REG_FRAME_TYPE:
        return static_cast<std::uint32_t>(frame_type_);

    case REG_QP:
        return qp_;

    case REG_WIDTH:
        return configured_width_;

    case REG_HEIGHT:
        return configured_height_;

    default:
        ok = false;
        return 0;
    }
}

void video_encoder_tlm::write_reg(std::uint64_t addr, std::uint32_t value)
{
    switch (addr) {
    case REG_CONTROL:
        control_ = value;

        if ((value & CONTROL_CLEAR) != 0u) {
            clear_status();
            last_bitstream_.clear();
            ctu_predictions_.clear();
            ctu_decisions_.clear();
            bit_count_ = 0;
        }

        if ((value & CONTROL_START) != 0u) {
            run_pipeline();
        }

        break;

    case REG_FRAME_TYPE:
        frame_type_ =
            value == static_cast<std::uint32_t>(frame_type::intra)
                ? frame_type::intra
                : frame_type::inter;
        break;

    case REG_QP:
        qp_ = clamp_qp(value);
        break;

    case REG_WIDTH:
        configured_width_ = value;
        break;

    case REG_HEIGHT:
        configured_height_ = value;
        break;

    default:
        // Ignore unsupported writes for now.
        break;
    }
}

void video_encoder_tlm::run_pipeline()
{
    clear_status();
    set_busy();

    ctu_predictions_.clear();
    ctu_decisions_.clear();
    last_bitstream_.clear();
    bit_count_ = 0;

    if (!input_valid()) {
        set_error();
        return;
    }

    for (std::uint32_t y = 0; y < input_frame_.height; y += CTU_SIZE) {
        for (std::uint32_t x = 0; x < input_frame_.width; x += CTU_SIZE) {
            const std::uint32_t width =
                std::min<std::uint32_t>(CTU_SIZE, input_frame_.width - x);

            const std::uint32_t height =
                std::min<std::uint32_t>(CTU_SIZE, input_frame_.height - y);

            block ctu {
                x,
                y,
                width,
                height,
                block_type::ctu,
                0
            };

            prediction_result final_prediction = encode_ctu(ctu);

            if (final_prediction.valid) {
                ctu_predictions_.push_back(final_prediction);
                append_prediction_summary(final_prediction);
            }
        }
    }

    // This is only an estimated bit count until CABAC is connected.
    std::uint64_t estimated_bits = 0;

    for (const prediction_result& pred : ctu_predictions_) {
        estimated_bits += pred.rate;

        if (pred.rate == 0) {
            estimated_bits += 8;
        }
    }

    bit_count_ =
        estimated_bits > std::numeric_limits<std::uint32_t>::max()
            ? std::numeric_limits<std::uint32_t>::max()
            : static_cast<std::uint32_t>(estimated_bits);

    set_done();
}

prediction_result video_encoder_tlm::encode_ctu(const block& ctu)
{
    const prei_rate_control_config rc_config =
        make_prei_rc_config();

    prei_result prei_info =
        prei_.run(input_frame_, ctu, rc_config);

    if (!prei_info.valid) {
        return prediction_result::invalid();
    }

    prediction_result intra_result =
        posi_.run(input_frame_,
                  reconstructed_frame_,
                  ctu,
                  prei_info,
                  prei_info.qp);

    if (!intra_result.valid) {
        return prediction_result::invalid();
    }

    if (frame_type_ == frame_type::intra || reference_frame_.empty()) {
        mode_decision_config md_config =
            make_mode_decision_config(prei_info.qp);

        md_config.current_frame_type = frame_type::intra;

        mode_decision_result decision =
            mode_decision_.run(intra_result,
                               prediction_result::invalid(),
                               md_config);

        if (decision.valid) {
            ctu_decisions_.push_back(decision);
            return decision.final_prediction;
        }

        return intra_result;
    }

    ime_result ime_info =
        ime_.run(input_frame_,
                 reference_frame_,
                 ctu,
                 prei_info.qp);

    if (!ime_info.valid) {
        return intra_result;
    }

    fme_result fme_info =
        fme_.run(input_frame_,
                 reference_frame_,
                 ime_info,
                 prei_info.qp);

    if (!fme_info.valid) {
        mode_decision_result decision =
            mode_decision_.run(intra_result,
                               ime_info,
                               frame_type_,
                               prei_info.qp);

        if (decision.valid) {
            ctu_decisions_.push_back(decision);
            return decision.final_prediction;
        }

        return intra_result;
    }

    mode_decision_result decision =
        mode_decision_.run(intra_result,
                           fme_info,
                           frame_type_,
                           prei_info.qp);

    if (decision.valid) {
        ctu_decisions_.push_back(decision);
        return decision.final_prediction;
    }

    return intra_result;
}

prei_rate_control_config video_encoder_tlm::make_prei_rc_config() const
{
    prei_rate_control_config config;

    config.initial_qp = qp_;
    config.min_qp = MIN_QP;
    config.max_qp = MAX_QP;
    config.delta_qp = 0;

    // Enable LCU-level RC behavior in PREI.
    config.lcu_rc_enable = true;

    // Until CABAC reports actual bits, use previous estimated bit count.
    config.actual_bitnum = bit_count_;

    const std::uint32_t ctu_count_x =
        configured_width_ == 0
            ? 1
            : (configured_width_ + CTU_SIZE - 1u) / CTU_SIZE;

    const std::uint32_t ctu_count_y =
        configured_height_ == 0
            ? 1
            : (configured_height_ + CTU_SIZE - 1u) / CTU_SIZE;

    const std::uint32_t ctu_count =
        std::max<std::uint32_t>(1u, ctu_count_x * ctu_count_y);

    // Placeholder target bits per CTU.
    // Real target comes from rate-control/config registers later.
    config.target_bitnum = ctu_count * 128u;

    config.roi_enable = false;

    return config;
}

mode_decision_config video_encoder_tlm::make_mode_decision_config(
    std::uint32_t qp) const
{
    mode_decision_config config;

    config.current_frame_type = frame_type_;
    config.qp = clamp_qp(qp);

    config.enable_intra_in_p = true;
    config.enable_skip = true;

    config.intra_in_p_bias = 0;
    config.inter_bias = 0;
    config.skip_bias = 0;

    config.skip_residual_threshold_per_pixel = 1;
    config.force_intra_for_i_frame = true;

    return config;
}

void video_encoder_tlm::append_prediction_summary(
    const prediction_result& result)
{
    // Placeholder output summary.
    // Actual entropy-coded bytes will be produced by CABAC later.
    std::uint8_t mode_byte = 0;

    if (result.mode == prediction_mode::intra) {
        mode_byte = 0x01;
    } else if (result.skip) {
        mode_byte = 0x03;
    } else {
        mode_byte = 0x02;
    }

    const std::uint8_t qp_byte =
        static_cast<std::uint8_t>(std::min<std::uint32_t>(result.qp, 255u));

    const std::uint8_t cost_low =
        static_cast<std::uint8_t>(result.cost & 0xFFu);

    const std::uint8_t rate_low =
        static_cast<std::uint8_t>(result.rate & 0xFFu);

    last_bitstream_.push_back(mode_byte);
    last_bitstream_.push_back(qp_byte);
    last_bitstream_.push_back(cost_low);
    last_bitstream_.push_back(rate_low);
}

bool video_encoder_tlm::input_valid() const
{
    if (input_frame_.empty()) {
        return false;
    }

    if (configured_width_ != 0 && configured_width_ != input_frame_.width) {
        return false;
    }

    if (configured_height_ != 0 && configured_height_ != input_frame_.height) {
        return false;
    }

    return true;
}

void video_encoder_tlm::clear_status()
{
    status_ = 0;
}

void video_encoder_tlm::set_busy()
{
    status_ &= ~STATUS_DONE;
    status_ &= ~STATUS_ERROR;
    status_ |= STATUS_BUSY;
}

void video_encoder_tlm::set_done()
{
    status_ &= ~STATUS_BUSY;
    status_ &= ~STATUS_ERROR;
    status_ |= STATUS_DONE;
}

void video_encoder_tlm::set_error()
{
    status_ &= ~STATUS_BUSY;
    status_ &= ~STATUS_DONE;
    status_ |= STATUS_ERROR;
}

std::uint32_t video_encoder_tlm::clamp_qp(std::uint32_t qp) const
{
    return std::clamp(qp, MIN_QP, MAX_QP);
}

} // namespace cdc::components
