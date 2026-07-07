#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "posi.h"
#include "prediction_result.h"
#include "prei.h"
#include "prei_result.h"
#include "../../fetch/include/fetch_loader_registry.h"
#include "../../fetch/include/fetch_frame_loader.h"

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

static cdc::components::frame make_gradient_frame(std::uint32_t width,
                                                   std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 5u + y * 11u + ((x ^ y) & 0x1fu) + ((x * y) % 17u)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

static cdc::components::frame make_flat_frame(std::uint32_t width,
                                               std::uint32_t height,
                                               std::uint8_t value)
{
    cdc::components::frame f(width, height);
    f.fill(value, 128, 128);
    return f;
}

static cdc::components::frame make_horizontal_edge_frame(std::uint32_t width,
                                                         std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint8_t value = y < (height / 2) ? 32 : 220;
            f.set_luma(x, y, value);
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

static cdc::components::frame make_vertical_edge_frame(std::uint32_t width,
                                                       std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint8_t value = x < (width / 2) ? 32 : 220;
            f.set_luma(x, y, value);
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

static std::uint64_t abs_residual_sum(const std::vector<std::int16_t>& residual)
{
    std::uint64_t sum = 0;

    for (std::int16_t value : residual) {
        sum += value < 0 ? static_cast<std::uint64_t>(-value)
                         : static_cast<std::uint64_t>(value);
    }

    return sum;
}

static bool all_predicted_equal(const std::vector<std::uint8_t>& pixels,
                                std::uint8_t expected)
{
    for (std::uint8_t pixel : pixels) {
        if (pixel != expected) {
            return false;
        }
    }

    return true;
}

static bool residual_matches_input_minus_prediction(
    const cdc::components::frame& input,
    const cdc::components::block& region,
    const cdc::components::prediction_result& result)
{
    if (result.predicted_luma.size() != region.area()) {
        return false;
    }

    if (result.residual_luma.size() != region.area()) {
        return false;
    }

    std::size_t index = 0;

    for (std::uint32_t y = 0; y < region.height; ++y) {
        for (std::uint32_t x = 0; x < region.width; ++x) {
            const std::uint8_t original =
                input.get_luma(region.x + x, region.y + y);

            const std::int16_t expected_residual =
                static_cast<std::int16_t>(original) -
                static_cast<std::int16_t>(result.predicted_luma[index]);

            if (result.residual_luma[index] != expected_residual) {
                return false;
            }

            ++index;
        }
    }

    return true;
}

static cdc::components::intra_prediction_mode map_mode_for_test(
    std::uint32_t mode_index)
{
    using namespace cdc::components;

    switch (mode_index) {
    case 0:
        return intra_prediction_mode::planar;
    case 1:
        return intra_prediction_mode::dc;
    case 2:
        return intra_prediction_mode::angular_2;
    case 10:
        return intra_prediction_mode::angular_10;
    case 18:
        return intra_prediction_mode::angular_18;
    case 26:
        return intra_prediction_mode::angular_26;
    case 34:
        return intra_prediction_mode::angular_34;
    default:
        return intra_prediction_mode::angular_26;
    }
}

static cdc::components::prei_result make_manual_prei_result(
    const cdc::components::block& region,
    std::uint32_t mode_index,
    std::uint32_t qp = cdc::components::INIT_QP)
{
    using namespace cdc::components;

    prei_mode_candidate candidate;
    candidate.valid = true;
    candidate.mode_index = mode_index;
    candidate.cost = 0;

    prei_mode_entry entry;
    entry.valid = true;
    entry.cu = region;
    entry.best_mode_index = mode_index;
    entry.best_mode = map_mode_for_test(mode_index);
    entry.best_cost = candidate.cost;
    entry.candidates.push_back(candidate);

    prei_result result;
    result.valid = true;
    result.ctu = region;
    result.qp = qp;
    result.mode_entries.push_back(entry);
    result.modebest64_sum = 0;

    return result;
}

static void check_intra_result_common(
    const cdc::components::frame& input,
    const cdc::components::prediction_result& result,
    const cdc::components::block& region,
    std::uint32_t qp)
{
    using namespace cdc::components;

    CHECK(result.valid);
    CHECK(result.mode == prediction_mode::intra);

    CHECK(result.qp == qp);

    CHECK(result.predicted_luma.size() == region.area());
    CHECK(result.residual_luma.size() == region.area());

    CHECK(result.cost < std::numeric_limits<std::uint32_t>::max());
    CHECK(result.rate < std::numeric_limits<std::uint32_t>::max());
    CHECK(result.distortion < std::numeric_limits<std::uint32_t>::max());

    CHECK(residual_matches_input_minus_prediction(input, region, result));
}

static bool same_prediction_summary(const cdc::components::prediction_result& a,
                                    const cdc::components::prediction_result& b)
{
    return a.valid == b.valid &&
           a.mode == b.mode &&
           a.cost == b.cost &&
           a.rate == b.rate &&
           a.distortion == b.distortion &&
           a.qp == b.qp &&
           a.partition == b.partition &&
           a.intra_mode == b.intra_mode &&
           a.predicted_luma == b.predicted_luma &&
           a.residual_luma == b.residual_luma;
}

static void test_posi_from_prei_gradient()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI from PREI on gradient frame\n";

    frame input = make_gradient_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);
    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    check_intra_result_common(input, result, region, INIT_QP);
}

static void test_posi_flat_frame_zero_residual()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI flat frame should produce zero residual\n";

    frame input = make_flat_frame(64, 64, 128);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);
    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    check_intra_result_common(input, result, region, INIT_QP);

    CHECK(all_predicted_equal(result.predicted_luma, 128));
    CHECK(abs_residual_sum(result.residual_luma) == 0);
    CHECK(result.distortion == 0);
}

static void test_posi_manual_dc_candidate()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI manual DC candidate\n";

    frame input = make_flat_frame(64, 64, 96);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei_result prei_info = make_manual_prei_result(region, 1, INIT_QP);

    posi dut;
    prediction_result result =
        dut.run(input, reconstructed, region, prei_info, INIT_QP);

    check_intra_result_common(input, result, region, INIT_QP);

    CHECK(result.intra_mode == intra_prediction_mode::dc);
    CHECK(all_predicted_equal(result.predicted_luma, 96));
    CHECK(abs_residual_sum(result.residual_luma) == 0);
}

static void test_posi_manual_angular_candidate()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI manual angular candidate\n";

    frame input = make_vertical_edge_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei_result prei_info = make_manual_prei_result(region, 26, INIT_QP);

    posi dut;
    prediction_result result =
        dut.run(input, reconstructed, region, prei_info, INIT_QP);

    check_intra_result_common(input, result, region, INIT_QP);

    CHECK(result.intra_mode == intra_prediction_mode::angular_26);
}

static void test_posi_horizontal_edge()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI horizontal edge frame\n";

    frame input = make_horizontal_edge_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);
    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    check_intra_result_common(input, result, region, INIT_QP);
}

static void test_posi_vertical_edge()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI vertical edge frame\n";

    frame input = make_vertical_edge_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);
    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    check_intra_result_common(input, result, region, INIT_QP);
}

static void test_posi_multiple_positions()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI multiple block positions\n";

    frame input = make_gradient_frame(64, 64);
    frame reconstructed = input;

    prei prei_dut;
    posi posi_dut;

    const block block0(0, 0, 16, block_type::cu);
    const block block1(16, 16, 16, block_type::cu);
    const block block2(32, 32, 16, block_type::cu);
    const block block3(48, 48, 16, block_type::cu);

    const block blocks[] = {block0, block1, block2, block3};

    for (const block& region : blocks) {
        prei_result prei_info = prei_dut.run(input, region);
        prediction_result result =
            posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

        CHECK(prei_info.valid);
        check_intra_result_common(input, result, region, INIT_QP);
    }
}

static void test_posi_different_block_sizes()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI different block sizes\n";

    frame input = make_gradient_frame(128, 128);
    frame reconstructed = input;

    prei prei_dut;
    posi posi_dut;

    const block b8(16, 16, 8, block_type::cu);
    const block b16(32, 32, 16, block_type::cu);
    const block b32(64, 64, 32, block_type::cu);

    const block blocks[] = {b8, b16, b32};

    for (const block& region : blocks) {
        prei_result prei_info = prei_dut.run(input, region);
        prediction_result result =
            posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

        CHECK(prei_info.valid);
        check_intra_result_common(input, result, region, INIT_QP);
    }
}

static void test_posi_deterministic()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI deterministic output\n";

    frame input = make_gradient_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);

    prediction_result result0 =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    prediction_result result1 =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    CHECK(result0.valid);
    CHECK(result1.valid);
    CHECK(same_prediction_summary(result0, result1));
}

static void test_posi_compatibility_api()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI compatibility API\n";

    frame input = make_gradient_frame(64, 64);
    block region(32, 16, 16, block_type::cu);

    posi dut;
    prediction_result result = dut.run(input, region);

    check_intra_result_common(input, result, region, INIT_QP);
}

static void test_posi_empty_frame_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI empty frame invalid input\n";

    frame empty;
    block region(0, 0, 16, block_type::cu);

    posi dut;
    prediction_result result = dut.run(empty, region);

    CHECK(!result.valid);
}

static void test_posi_invalid_prei_input()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI invalid PREI input\n";

    frame input = make_gradient_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei_result invalid_prei;
    invalid_prei.valid = false;

    posi dut;
    prediction_result result =
        dut.run(input, reconstructed, region, invalid_prei, INIT_QP);

    CHECK(!result.valid);
}

static void test_posi_zero_size_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI zero-size block invalid input\n";

    frame input = make_gradient_frame(64, 64);
    frame reconstructed = input;
    block region(0, 0, 0, block_type::cu);

    prei_result prei_info = make_manual_prei_result(region, 1, INIT_QP);

    posi dut;
    prediction_result result =
        dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(!result.valid);
}

static void test_posi_out_of_bound_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI out-of-bound block invalid input\n";

    frame input = make_gradient_frame(64, 64);
    frame reconstructed = input;

    // 60 + 16 > 64, so this region is outside the frame.
    block region(60, 60, 16, block_type::cu);

    prei_result prei_info = make_manual_prei_result(region, 1, INIT_QP);

    posi dut;
    prediction_result result =
        dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " POSI Full Functional Unit Test\n";
    std::cout << "========================================\n";

    test_posi_from_prei_gradient();
    test_posi_flat_frame_zero_residual();
    test_posi_manual_dc_candidate();
    test_posi_manual_angular_candidate();
    test_posi_horizontal_edge();
    test_posi_vertical_edge();
    test_posi_multiple_positions();
    test_posi_different_block_sizes();
    test_posi_deterministic();
    test_posi_compatibility_api();

    test_posi_empty_frame_invalid();
    test_posi_invalid_prei_input();
    test_posi_zero_size_invalid();
    test_posi_out_of_bound_invalid();

    if (g_failures == 0) {
        std::cout << "\nPOSI full functional test PASSED\n";
    } else {
        std::cout << "\nPOSI full functional test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
