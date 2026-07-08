#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>

#include "video_encoder_full_tlm.h"
#include "vpu_db_custom_packet.h"
#include "vpu_cabac_custom_packet.h"

static int g_failures = 0;

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            ++g_failures;                                                  \
            std::cerr << "[FAIL] " << #expr << std::endl;                 \
        } else {                                                           \
            std::cout << "[PASS] " << #expr << std::endl;                 \
        }                                                                 \
    } while (0)

namespace {

cdc::components::frame make_textured_frame(std::uint32_t width,
                                           std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 9u +
                 y * 17u +
                 ((x * y) % 19u) +
                 ((x ^ (y * 5u)) & 0x4fu)) & 0xffu;
            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

cdc::components::frame make_shifted_reference_from_current(
    const cdc::components::frame& current,
    int shift_x,
    int shift_y)
{
    cdc::components::frame reference(current.width, current.height);

    for (std::uint32_t y = 0; y < current.height; ++y) {
        for (std::uint32_t x = 0; x < current.width; ++x) {
            const int src_x =
                std::clamp(static_cast<int>(x) - shift_x,
                           0,
                           static_cast<int>(current.width) - 1);
            const int src_y =
                std::clamp(static_cast<int>(y) - shift_y,
                           0,
                           static_cast<int>(current.height) - 1);

            reference.set_luma(
                x,
                y,
                current.get_luma(static_cast<std::uint32_t>(src_x),
                                 static_cast<std::uint32_t>(src_y)));
        }
    }

    reference.fill_chroma(128, 128);
    return reference;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Video Encoder Full TLM Integration Test\n";
    std::cout << "========================================\n";

    cdc::components::video_encoder_full_tlm full_top;

    CHECK(!sc_core::sc_end_of_simulation_invoked());
    CHECK(full_top.rec.rec_mv.size() >= 1024);
    CHECK(full_top.db.db_mv.last_mv_p == 0);

    cdc::components::frame input = make_textured_frame(64, 64);
    cdc::components::frame reconstructed = input;
    cdc::components::frame reference =
        make_shifted_reference_from_current(input, 1, 1);
    cdc::components::block region(16, 16, 16, cdc::components::block_type::ctu);

    cdc::components::video_encoder_full_tlm_result result =
        full_top.run_prediction(
            input, reconstructed, reference, region, cdc::components::INIT_QP);

    CHECK(result.valid);
    CHECK(result.trace.encoder_result.valid);
    CHECK(!result.db_output.empty());
    CHECK(!result.cabac_output.empty());

    DbCustomPacket db_packet =
        unpackDbCustomPacket(result.db_output.data(), result.db_output.size());
    CHECK(db_packet.qp == result.trace.encoder_result.rec_request.packet.qp);
    CHECK(db_packet.pred_type == result.trace.encoder_result.rec_request.packet.pred_type);

    CabacCustomPacket cabac_packet =
        unpackCabacCustomPacket(
            result.cabac_output.data(), result.cabac_output.size());
    CHECK(cabac_packet.cmd == CabacCustomCmd::COEFF);
    CHECK(cabac_packet.block_idx ==
          result.trace.encoder_result.rec_request.packet.block_idx);
    CHECK(cabac_packet.x == result.trace.encoder_result.rec_request.packet.x);
    CHECK(cabac_packet.y == result.trace.encoder_result.rec_request.packet.y);

    cdc::components::frame intra_only_reference;
    cdc::components::video_encoder_full_tlm_result intra_only_result =
        full_top.run_prediction(
            input,
            reconstructed,
            intra_only_reference,
            region,
            cdc::components::INIT_QP);

    CHECK(intra_only_result.valid);
    CHECK(intra_only_result.trace.encoder_result.valid);
    CHECK(intra_only_result.trace.encoder_result.selected_prediction.mode ==
          cdc::components::prediction_mode::intra);
    CHECK(!intra_only_result.db_output.empty());
    CHECK(!intra_only_result.cabac_output.empty());

    cdc::components::video_encoder_frame_tlm_result frame_result =
        full_top.run_frame(
            input, reconstructed, intra_only_reference, 16, cdc::components::INIT_QP);

    CHECK(frame_result.valid);
    CHECK(frame_result.ctu_size == 16);
    CHECK(frame_result.total_regions == 16);
    CHECK(frame_result.completed_regions == frame_result.total_regions);
    CHECK(frame_result.reconstructed_frame.width == input.width);
    CHECK(frame_result.reconstructed_frame.height == input.height);
    CHECK(frame_result.regions.size() == 16);
    CHECK(frame_result.region_results.size() == frame_result.regions.size());
    CHECK(frame_result.regions.front().x == 0);
    CHECK(frame_result.regions.front().y == 0);
    CHECK(frame_result.regions.back().x == 48);
    CHECK(frame_result.regions.back().y == 48);
    CHECK(frame_result.region_results.front().valid);
    CHECK(frame_result.region_results.back().valid);
    CHECK(!frame_result.region_results.front().db_output.empty());
    CHECK(!frame_result.region_results.front().cabac_output.empty());
    CHECK(!frame_result.reconstructed_frame.empty());

    if (g_failures == 0) {
        std::cout << "\nVideo encoder full tlm integration test PASSED\n";
    } else {
        std::cout << "\nVideo encoder full tlm integration test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
