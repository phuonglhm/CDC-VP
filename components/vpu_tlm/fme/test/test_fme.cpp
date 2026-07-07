#include <systemc>

#include <algorithm>
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

static cdc::components::ime_result make_manual_ime_result(
    const cdc::components::block& ctu,
    cdc::components::motion_vector mv)
{
    using namespace cdc::components;

    ime_result info;
    info.valid = true;
    info.ctu = ctu;
    info.qp = INIT_QP;
    info.best_partition = partition_mode::part_2nx2n;
    info.best_mv = mv;
    info.best_sad = 0;
    info.best_rate = 0;
    info.best_cost = 0;
    info.best_inter_result = prediction_result::make_inter(0,
                                                           mv,
                                                           partition_mode::part_2nx2n,
                                                           INIT_QP);
    return info;
}

static void test_fme_default_refine()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME default refinement\n";

    frame input = make_texture_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime ime_dut;
    fme fme_dut;

    ime_result ime_info = ime_dut.run(input, reference, ctu, INIT_QP);
    fme_result result = fme_dut.run(input, reference, ime_info, INIT_QP);

    CHECK(ime_info.valid);
    check_fme_result_shape(result, ctu);

    CHECK(abs_i(result.best_inter_result.mv.x) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y) <= 4);
}

static void test_fme_with_explicit_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME explicit refine config\n";

    frame input = make_texture_frame(64, 64);
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
    config.use_hevc_luma_filter = true;

    fme_result result = fme_dut.run(input, reference, ime_info, config, INIT_QP);

    CHECK(ime_info.valid);
    check_fme_result_shape(result, ctu);
}

static void test_fme_known_integer_shift_from_manual_ime()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME known integer shift from manual IME result\n";

    frame reference = make_texture_frame(96, 64);
    frame input = make_shifted_frame(reference, 2, 0);

    block ctu(32, 16, 16, block_type::ctu);

    // Current was shifted by +2 pixels, so reference position is around -2 pixels.
    // In quarter-pel unit, -2 pixels = -8.
    ime_result ime_info = make_manual_ime_result(ctu, motion_vector(-8, 0));

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, INIT_QP);

    check_fme_result_shape(result, ctu);

    CHECK(abs_i(result.best_inter_result.mv.x - (-8)) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y) <= 4);
}

static void test_fme_invalid_ime_input()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME invalid IME input\n";

    frame input = make_texture_frame(64, 64);
    frame reference = input;

    ime_result invalid_ime = ime_result::invalid();

    fme dut;
    fme_result result = dut.run(input, reference, invalid_ime, INIT_QP);

    CHECK(!result.valid);
}

static void test_fme_empty_frame_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME empty frame invalid input\n";

    frame empty;
    frame reference = make_texture_frame(64, 64);
    block ctu(0, 0, 16, block_type::ctu);

    ime_result ime_info = make_manual_ime_result(ctu, motion_vector(0, 0));

    fme dut;
    fme_result result = dut.run(empty, reference, ime_info, INIT_QP);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " FME Deep Unit Test\n";
    std::cout << "========================================\n";

    test_fme_default_refine();
    test_fme_with_explicit_config();
    test_fme_known_integer_shift_from_manual_ime();
    test_fme_invalid_ime_input();
    test_fme_empty_frame_invalid();

    if (g_failures == 0) {
        std::cout << "\nFME deep test PASSED\n";
    } else {
        std::cout << "\nFME deep test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
