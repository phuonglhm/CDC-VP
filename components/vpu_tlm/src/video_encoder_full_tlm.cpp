#include "video_encoder_full_tlm.h"

namespace cdc::components {

namespace {

void update_reconstructed_region(frame& reconstructed,
                                 const block& region,
                                 const prediction_result& prediction)
{
    if (!prediction.valid || prediction.predicted_luma.empty()) {
        return;
    }

    const bool has_region_residual =
        prediction.residual_luma.size() == region.area();

    std::size_t index = 0;
    for (std::uint32_t y = 0; y < region.height; ++y) {
        for (std::uint32_t x = 0; x < region.width; ++x) {
            if (region.x + x >= reconstructed.width ||
                region.y + y >= reconstructed.height ||
                index >= prediction.predicted_luma.size()) {
                ++index;
                continue;
            }

            int sample = static_cast<int>(prediction.predicted_luma[index]);
            if (has_region_residual) {
                sample += static_cast<int>(prediction.residual_luma[index]);
            }

            sample = std::clamp(sample, 0, 255);
            reconstructed.set_luma(region.x + x,
                                   region.y + y,
                                   static_cast<std::uint8_t>(sample));
            ++index;
        }
    }
}

} // namespace

full_top_packet_sink::full_top_packet_sink(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      socket("socket")
{
    socket.register_b_transport(this, &full_top_packet_sink::b_transport);
}

void full_top_packet_sink::b_transport(tlm::tlm_generic_payload& trans,
                                       sc_core::sc_time& delay)
{
    (void)delay;
    last_data.clear();
    if (trans.get_data_ptr() != nullptr && trans.get_data_length() > 0) {
        last_data.assign(trans.get_data_ptr(),
                         trans.get_data_ptr() + trans.get_data_length());
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

full_top_rec_driver::full_top_rec_driver(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      intra_socket("intra_socket"),
      mc_socket("mc_socket")
{
}

video_encoder_full_tlm::video_encoder_full_tlm()
{
    rec_driver.intra_socket.bind(rec.rec_intra.start_socket);
    rec_driver.mc_socket.bind(rec.rec_mc.start_socket);

    rec.res_buffer.frame_socket.bind(frame_sink.socket);
    rec.rec_tq.cabac_socket.bind(rec_to_cabac.start_socket);
    rec.inv_tq.db_socket.bind(rec_to_db.start_socket);

    rec_to_db.bs_socket.bind(db.db_bs.start_socket);
    rec_to_db.mv_socket.bind(db.db_mv.start_socket);
    db.db_filter_sao.out_socket.bind(db_sink.socket);

    rec_to_cabac.bind_memory(cabac.simple_mem);
    rec_to_cabac.cabac_socket.bind(cabac.cabac.start_socket);
    cabac.cabac.out_socket.bind(cabac_sink.socket);
}

video_encoder_full_tlm_result video_encoder_full_tlm::run_prediction(
    const frame& input,
    const frame& reconstructed,
    const frame& reference,
    const block& region,
    std::uint32_t qp)
{
    video_encoder_full_tlm_result result;
    result.trace.encoder_result =
        encoder.run_prediction(input, reconstructed, reference, region, qp);
    result.valid = result.trace.encoder_result.valid &&
                   result.trace.encoder_result.rec_request.valid;

    db_sink.last_data.clear();
    cabac_sink.last_data.clear();
    frame_sink.last_data.clear();

    if (!result.valid) {
        return result;
    }

    encoder_to_rec_.write_mv_if_needed(result.trace.encoder_result.rec_request, rec.rec_mv);

    std::vector<std::uint8_t> storage;
    tlm::tlm_generic_payload trans;
    encoder_to_rec_.make_transaction(result.trace.encoder_result.rec_request, trans, storage);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    if (encoder_to_rec_.targets_mc(result.trace.encoder_result.rec_request)) {
        rec_driver.mc_socket->b_transport(trans, delay);
    } else {
        rec_driver.intra_socket->b_transport(trans, delay);
    }

    sc_core::sc_start(1, sc_core::SC_MS);

    result.db_output = db_sink.last_data;
    result.cabac_output = cabac_sink.last_data;
    return result;
}

video_encoder_frame_tlm_result video_encoder_full_tlm::run_frame(
    const frame& input,
    const frame& reconstructed,
    const frame& reference,
    std::uint32_t ctu_size,
    std::uint32_t qp)
{
    video_encoder_frame_tlm_result frame_result;
    frame_result.ctu_size = ctu_size;

    if (input.empty() || reconstructed.empty() || ctu_size == 0) {
        return frame_result;
    }

    frame_result.valid = true;
    frame reconstructed_working = reconstructed;

    for (std::uint32_t y = 0; y < input.height; y += ctu_size) {
        for (std::uint32_t x = 0; x < input.width; x += ctu_size) {
            const std::uint32_t region_width = std::min(ctu_size, input.width - x);
            const std::uint32_t region_height = std::min(ctu_size, input.height - y);
            block region(x, y, region_width, region_height, block_type::ctu);

            frame_result.regions.push_back(region);
            frame_result.region_results.push_back(
                run_prediction(input, reconstructed_working, reference, region, qp));
            ++frame_result.total_regions;

            if (!frame_result.region_results.back().valid) {
                frame_result.valid = false;
            } else {
                update_reconstructed_region(
                    reconstructed_working,
                    region,
                    frame_result.region_results.back().trace.encoder_result.selected_prediction);
                ++frame_result.completed_regions;
            }
        }
    }

    frame_result.reconstructed_frame = std::move(reconstructed_working);

    return frame_result;
}

} // namespace cdc::components
