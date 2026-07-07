#include <systemc>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "ime.h"
#include "ime_result.h"
#include "prediction_result.h"
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

static int abs_i(int value)
{
    return value < 0 ? -value : value;
}

static cdc::components::frame make_texture_frame(std::uint32_t width,
                                                  std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 17u + y * 31u + ((x * y) % 53u) + ((x ^ (y << 1u)) & 0x3fu)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

static cdc::components::frame make_shifted_frame(const cdc::components::frame& reference,
                                                  int shift_x,
                                                  int shift_y)
{
    cdc::components::frame current(reference.width, reference.height);

    for (std::uint32_t y = 0; y < reference.height; ++y) {
        for (std::uint32_t x = 0; x < reference.width; ++x) {
            const int src_x =
                std::clamp(static_cast<int>(x) - shift_x,
                           0,
                           static_cast<int>(reference.width) - 1);

            const int src_y =
                std::clamp(static_cast<int>(y) - shift_y,
                           0,
                           static_cast<int>(reference.height) - 1);

            current.set_luma(x, y,
                             reference.get_luma(static_cast<std::uint32_t>(src_x),
                                                static_cast<std::uint32_t>(src_y)));
        }
    }

    current.fill_chroma(128, 128);
    return current;
}

static cdc::components::frame make_noisy_reference(const cdc::components::frame& input)
{
    cdc::components::frame reference = input;

    for (std::uint32_t y = 0; y < reference.height; ++y) {
        for (std::uint32_t x = 0; x < reference.width; ++x) {
            const std::uint8_t old_value = reference.get_luma(x, y);
            const std::uint8_t noise =
                static_cast<std::uint8_t>(((x * 3u + y * 5u) & 0x07u));

            reference.set_luma(x, y, static_cast<std::uint8_t>(old_value ^ noise));
        }
    }

    return reference;
}

static void check_inter_result_shape(const cdc::components::ime_result& result,
                                     const cdc::components::block& ctu)
{
    using namespace cdc::components;

    CHECK(result.valid);
    CHECK(result.best_inter_result.valid);
    CHECK(result.best_inter_result.mode == prediction_mode::inter);
    CHECK(result.best_inter_result.predicted_luma.size() == ctu.area());
    CHECK(result.best_cost < std::numeric_limits<std::uint32_t>::max());
}

static void test_ime_identical_reference()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME identical current/reference frame\n";

    frame input = make_texture_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    check_inter_result_shape(result, ctu);

    CHECK(result.best_sad == 0);
    CHECK(abs_i(result.best_mv.x) <= 4);
    CHECK(abs_i(result.best_mv.y) <= 4);
}

static void test_ime_zero_search_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME zero-search config\n";

    frame input = make_texture_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime_search_config config;
    config.search_range_x = 0;
    config.search_range_y = 0;
    config.use_feedback = true;
    config.center_mv = motion_vector(0, 0);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_inter_result_shape(result, ctu);

    CHECK(result.best_mv.x == 0);
    CHECK(result.best_mv.y == 0);
    CHECK(result.best_sad == 0);
}

static void test_ime_known_horizontal_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME known horizontal shift\n";

    frame reference = make_texture_frame(96, 64);

    // Current frame is generated from reference shifted by +2 pixels.
    // Expected motion vector magnitude is around 2 pixels = 8 quarter-pel units.
    frame input = make_shifted_frame(reference, 2, 0);

    block ctu(32, 16, 16, block_type::ctu);

    ime_search_config config;
    config.search_range_x = 8;
    config.search_range_y = 4;
    config.use_feedback = true;
    config.center_mv = motion_vector(0, 0);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_inter_result_shape(result, ctu);

    CHECK(abs_i(abs_i(result.best_mv.x) - 8) <= 4);
    CHECK(abs_i(result.best_mv.y) <= 4);
}

static void test_ime_noisy_reference_still_valid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME noisy reference frame still produces valid result\n";

    frame input = make_texture_frame(64, 64);
    frame reference = make_noisy_reference(input);

    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    check_inter_result_shape(result, ctu);
}

static void test_ime_empty_frame_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME empty frame invalid input\n";

    frame empty;
    frame reference = make_texture_frame(64, 64);
    block ctu(0, 0, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(empty, reference, ctu, INIT_QP);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " IME Deep Unit Test\n";
    std::cout << "========================================\n";

    test_ime_identical_reference();
    test_ime_zero_search_config();
    test_ime_known_horizontal_shift();
    test_ime_noisy_reference_still_valid();
    test_ime_empty_frame_invalid();

    if (g_failures == 0) {
        std::cout << "\nIME deep test PASSED\n";
    } else {
        std::cout << "\nIME deep test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
