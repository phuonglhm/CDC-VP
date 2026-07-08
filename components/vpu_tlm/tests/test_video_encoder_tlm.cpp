#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "prediction_result.h"
#include "video_encoder_tlm.h"

static int g_failures = 0;

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            ++g_failures;                                                  \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__         \
                      << " check failed: " << #expr << std::endl;         \
        } else {                                                           \
            std::cout << "[PASS] " << #expr << std::endl;                 \
        }                                                                 \
    } while (0)

static int abs_i(int value)
{
    return value < 0 ? -value : value;
}

static cdc::components::frame make_textured_frame(std::uint32_t width,
                                                  std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 13u +
                 y * 29u +
                 ((x * y) % 37u) +
                 ((x ^ (y * 3u)) & 0x3fu)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

static cdc::components::frame make_shifted_reference_from_current(
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

static bool residual_matches_input_minus_prediction(
    const cdc::components::frame& input,
    const cdc::components::block& region,
    const cdc::components::prediction_result& result)
{
    if (result.predicted_luma.size() != region.area() ||
        result.residual_luma.size() != region.area()) {
        return false;
    }

    std::size_t index = 0;

    for (std::uint32_t y = 0; y < region.height; ++y) {
        for (std::uint32_t x = 0; x < region.width; ++x) {
            const std::int16_t expected =
                static_cast<std::int16_t>(input.get_luma(region.x + x, region.y + y)) -
                static_cast<std::int16_t>(result.predicted_luma[index]);

            if (result.residual_luma[index] != expected) {
                return false;
            }

            ++index;
        }
    }

    return true;
}

static void test_prei_and_posi()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI/POSI basic intra analysis\n";

    frame input = make_textured_frame(64, 64);
    frame reconstructed = input;
    block region(0, 0, 16, block_type::ctu);

    video_encoder_tlm dut;
    prediction_result intra = dut.run_intra(input, reconstructed, region, INIT_QP);

    CHECK(intra.valid);
    CHECK(intra.mode == prediction_mode::intra);
    CHECK(intra.cost < std::numeric_limits<std::uint32_t>::max());
    CHECK(intra.predicted_luma.size() == region.area());
    CHECK(intra.residual_luma.size() == region.area());
    CHECK(residual_matches_input_minus_prediction(input, region, intra));
}

static void test_ime_and_fme()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME/FME inter prediction\n";

    frame input = make_textured_frame(64, 64);
    frame reference = make_shifted_reference_from_current(input, 1, 0);
    block ctu(0, 0, 16, block_type::ctu);

    video_encoder_tlm dut;
    fme_result inter = dut.run_inter(input, reference, ctu, INIT_QP);

    CHECK(inter.valid);
    CHECK(inter.best_inter_result.valid);
    CHECK(inter.best_inter_result.mode == prediction_mode::inter);
    CHECK(abs_i(inter.best_mv.x) <= 8);
    CHECK(abs_i(inter.best_mv.y) <= 8);
    CHECK(inter.best_inter_result.predicted_luma.size() == ctu.area());
    CHECK(inter.best_inter_result.residual_luma.size() == ctu.area());
    CHECK(residual_matches_input_minus_prediction(input, ctu, inter.best_inter_result));
}

static void test_combined_top_intra_only()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] video_encoder_tlm intra-only top path\n";

    frame input = make_textured_frame(64, 64);
    frame reconstructed = input;
    frame reference;
    block ctu(16, 16, 16, block_type::ctu);

    video_encoder_tlm dut;
    video_encoder_tlm_result result =
        dut.run_prediction(input, reconstructed, reference, ctu, INIT_QP);

    CHECK(result.valid);
    CHECK(result.final_decision.valid);
    CHECK(result.selected_prediction.valid);
    CHECK(result.selected_prediction.mode == prediction_mode::intra);
    CHECK(!result.trace.ime_info.valid);
    CHECK(!result.trace.fme_info.valid);
    CHECK(!result.trace.best_inter.valid);
    CHECK(result.rec_request.valid);
}

static void test_combined_top()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] Instantiate and run video_encoder_tlm top\n";

    frame input = make_textured_frame(64, 64);
    frame reconstructed = input;
    frame reference = make_shifted_reference_from_current(input, 1, 1);
    block ctu(16, 16, 16, block_type::ctu);

    video_encoder_tlm dut;
    video_encoder_tlm_result result =
        dut.run_prediction(input, reconstructed, reference, ctu, INIT_QP);

    CHECK(result.valid);
    CHECK(result.final_decision.valid);
    CHECK(result.selected_prediction.valid);
    CHECK(result.rec_request.valid);
    CHECK(result.trace.prei_info.valid);
    CHECK(result.trace.best_intra.valid);
    CHECK(result.trace.ime_info.valid);
    CHECK(result.trace.fme_info.valid);
    CHECK(result.trace.best_inter.valid);
    CHECK(result.trace.best_intra.mode == prediction_mode::intra);
    CHECK(result.trace.best_inter.mode == prediction_mode::inter);
    CHECK(result.selected_prediction.mode == result.final_decision.selected_mode);
    CHECK(result.selected_prediction.cost <= result.trace.best_intra.cost ||
          result.selected_prediction.cost <= result.trace.best_inter.cost);
    CHECK(result.rec_request.packet.cmd == RecCmd::READ_REQ);
    CHECK(result.rec_request.packet.qp == result.selected_prediction.qp);
    CHECK(!sc_core::sc_end_of_simulation_invoked());
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Video Encoder TLM Unit Tests\n";
    std::cout << "========================================\n";

    test_prei_and_posi();
    test_ime_and_fme();
    test_combined_top_intra_only();
    test_combined_top();

    if (g_failures == 0) {
        std::cout << "\n========================================\n";
        std::cout << " Video Encoder TLM Unit Tests PASSED\n";
        std::cout << "========================================\n";
    } else {
        std::cout << "\n========================================\n";
        std::cout << " Video Encoder TLM Unit Tests FAILED\n";
        std::cout << " Failures: " << g_failures << "\n";
        std::cout << "========================================\n";
    }

    return g_failures == 0 ? 0 : 1;
}
