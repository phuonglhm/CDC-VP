#include <systemc>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "block.h"
#include "encoder_defs.h"
#include "fme.h"
#include "fme_result.h"
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

static void check_fme_result_shape(const cdc::components::fme_result& result,
                                   const cdc::components::block& ctu)
{
    using namespace cdc::components;

    CHECK(result.valid);
    CHECK(result.best_inter_result.valid);
    CHECK(result.best_inter_result.mode == prediction_mode::inter);
    CHECK(result.best_inter_result.predicted_luma.size() == ctu.area());
    CHECK(result.best_inter_result.cost < std::numeric_limits<std::uint32_t>::max());
}

static void test_fme_default_refine()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME default refinement\n";

    frame input = make_gradient_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime ime_dut;
    fme fme_dut;

    ime_result ime_info = ime_dut.run(input, reference, ctu, INIT_QP);
    fme_result result = fme_dut.run(input, reference, ime_info, INIT_QP);

    CHECK(ime_info.valid);
    check_fme_result_shape(result, ctu);

    // Identical current/reference frame: refined MV should stay near zero.
    CHECK(abs_i(result.best_inter_result.mv.x) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y) <= 4);
}

static void test_fme_with_explicit_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME explicit refine config\n";

    frame input = make_gradient_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime ime_dut;
    fme fme_dut;

    ime_result ime_info = ime_dut.run(input, reference, ctu, INIT_QP);

    fme_refine_config config;
    config.enable_half_pel = true;
    config.enable_quarter_pel = true;
    config.enable_skip_decision = true;
    config.half_pel_radius_qpel = 2;
    config.quarter_pel_radius_qpel = 1;

    fme_result result = fme_dut.run(input, reference, ime_info, config, INIT_QP);

    CHECK(ime_info.valid);
    check_fme_result_shape(result, ctu);
}

static void test_fme_invalid_ime_input()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME invalid IME input\n";

    frame input = make_gradient_frame(64, 64);
    frame reference = input;

    ime_result invalid_ime = ime_result::invalid();

    fme dut;
    fme_result result = dut.run(input, reference, invalid_ime, INIT_QP);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " FME Unit Test\n";
    std::cout << "========================================\n";

    test_fme_default_refine();
    test_fme_with_explicit_config();
    test_fme_invalid_ime_input();

    if (g_failures == 0) {
        std::cout << "\nFME test PASSED\n";
    } else {
        std::cout << "\nFME test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
