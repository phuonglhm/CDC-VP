#include <systemc>

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

static int g_failures = 0;

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            ++g_failures;                                                  \
            std::cerr << "[FAIL] " << #expr << std::endl;                 \
        } else {                                                           \
            std::cout << "[PASS] " << #expr << std::endl;                 \
        }                                                                  \
    } while (0)

static int abs_i(int value)
{
    return value < 0 ? -value : value;
}

static cdc::components::frame make_gradient_frame(std::uint32_t width,
                                                   std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 9u + y * 17u + ((x * y) % 37u) + ((x ^ y) & 0x1fu)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
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

    // IME is allowed to use residual internally only.
    // So this test does not require residual_luma to be exported.
    CHECK(result.best_cost < std::numeric_limits<std::uint32_t>::max());
}

static void test_ime_identical_reference()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME identical current/reference frame\n";

    frame input = make_gradient_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    check_inter_result_shape(result, ctu);

    // Identical current/reference frame: best MV should stay near zero.
    // MV unit is quarter-pel, so 4 means 1 integer pixel.
    CHECK(abs_i(result.best_mv.x) <= 4);
    CHECK(abs_i(result.best_mv.y) <= 4);
}

static void test_ime_zero_search_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME zero-search config\n";

    frame input = make_gradient_frame(64, 64);
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
}

static void test_ime_noisy_reference_still_valid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME noisy reference frame still produces valid result\n";

    frame input = make_gradient_frame(64, 64);
    frame reference = make_noisy_reference(input);

    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    check_inter_result_shape(result, ctu);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " IME Unit Test\n";
    std::cout << "========================================\n";

    test_ime_identical_reference();
    test_ime_zero_search_config();
    test_ime_noisy_reference_still_valid();

    if (g_failures == 0) {
        std::cout << "\nIME test PASSED\n";
    } else {
        std::cout << "\nIME test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
