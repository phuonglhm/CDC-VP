#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "fme.h"
#include "fme_result.h"
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

static std::uint32_t clamp_qp_for_test(std::uint32_t qp)
{
    return std::clamp(qp,
                      cdc::components::MIN_QP,
                      cdc::components::MAX_QP);
}

static cdc::components::frame make_textured_frame(std::uint32_t width,
                                                   std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 19u +
                 y * 31u +
                 ((x * y) % 43u) +
                 ((x ^ (y * 5u)) & 0x7fu)) & 0xffu;

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

static cdc::components::fme_refine_config default_config()
{
    cdc::components::fme_refine_config config;
    config.enable_half_pel = true;
    config.enable_quarter_pel = true;
    config.enable_skip_decision = true;
    config.half_pel_radius_qpel = 2;
    config.quarter_pel_radius_qpel = 1;
    config.use_hevc_luma_filter = true;
    return config;
}

static cdc::components::fme_refine_config integer_only_config()
{
    cdc::components::fme_refine_config config;
    config.enable_half_pel = false;
    config.enable_quarter_pel = false;
    config.enable_skip_decision = true;
    config.half_pel_radius_qpel = 0;
    config.quarter_pel_radius_qpel = 0;
    config.use_hevc_luma_filter = false;
    return config;
}

static cdc::components::fme_refine_config half_only_config()
{
    cdc::components::fme_refine_config config;
    config.enable_half_pel = true;
    config.enable_quarter_pel = false;
    config.enable_skip_decision = true;
    config.half_pel_radius_qpel = 2;
    config.quarter_pel_radius_qpel = 0;
    config.use_hevc_luma_filter = true;
    return config;
}

static cdc::components::fme_refine_config quarter_enabled_config()
{
    cdc::components::fme_refine_config config;
    config.enable_half_pel = true;
    config.enable_quarter_pel = true;
    config.enable_skip_decision = true;
    config.half_pel_radius_qpel = 2;
    config.quarter_pel_radius_qpel = 1;
    config.use_hevc_luma_filter = true;
    return config;
}

static cdc::components::ime_result make_manual_ime_result(
    const cdc::components::block& ctu,
    cdc::components::motion_vector mv,
    std::uint32_t qp = cdc::components::INIT_QP)
{
    using namespace cdc::components;

    prediction_result pred =
        prediction_result::make_inter(0, mv, partition_mode::part_2nx2n, qp);

    pred.valid = true;
    pred.mode = prediction_mode::inter;
    pred.mv = mv;
    pred.qp = qp;
    pred.partition = partition_mode::part_2nx2n;
    pred.predicted_luma.resize(static_cast<std::size_t>(ctu.area()), 128);

    ime_candidate candidate;
    candidate.valid = true;
    candidate.pu = ctu;
    candidate.partition = partition_mode::part_2nx2n;
    candidate.mv = mv;
    candidate.sad = 0;
    candidate.rate = 0;
    candidate.cost = 0;

    ime_result result;
    result.valid = true;
    result.ctu = ctu;
    result.qp = qp;
    result.best_partition = partition_mode::part_2nx2n;
    result.best_mv = mv;
    result.best_sad = 0;
    result.best_rate = 0;
    result.best_cost = 0;
    result.candidates.push_back(candidate);
    result.best_inter_result = pred;

    return result;
}

static bool residual_matches_input_minus_prediction(
    const cdc::components::frame& input,
    const cdc::components::block& ctu,
    const cdc::components::prediction_result& result)
{
    if (result.predicted_luma.size() != ctu.area()) {
        return false;
    }

    if (result.residual_luma.size() != ctu.area()) {
        return false;
    }

    std::size_t index = 0;

    for (std::uint32_t y = 0; y < ctu.height; ++y) {
        for (std::uint32_t x = 0; x < ctu.width; ++x) {
            const std::uint8_t original =
                input.get_luma(ctu.x + x, ctu.y + y);

            const std::int16_t expected =
                static_cast<std::int16_t>(original) -
                static_cast<std::int16_t>(result.predicted_luma[index]);

            if (result.residual_luma[index] != expected) {
                return false;
            }

            ++index;
        }
    }

    return true;
}

static bool fme_candidates_are_valid(const cdc::components::fme_result& result,
                                     const cdc::components::block& ctu)
{
    if (result.candidates.empty()) {
        return false;
    }

    for (const cdc::components::fme_candidate& candidate : result.candidates) {
        if (!candidate.valid) {
            return false;
        }

        if (candidate.pu.area() == 0) {
            return false;
        }

        if (candidate.pu.x < ctu.x || candidate.pu.y < ctu.y) {
            return false;
        }

        if (candidate.pu.right() > ctu.right() ||
            candidate.pu.bottom() > ctu.bottom()) {
            return false;
        }

        if (candidate.cost < candidate.satd) {
            return false;
        }

        if (candidate.cost >= std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
    }

    return true;
}

static std::uint32_t min_candidate_cost(
    const std::vector<cdc::components::fme_candidate>& candidates)
{
    std::uint32_t best = std::numeric_limits<std::uint32_t>::max();

    for (const cdc::components::fme_candidate& candidate : candidates) {
        if (candidate.valid && candidate.cost < best) {
            best = candidate.cost;
        }
    }

    return best;
}

static bool has_half_pel_candidate(
    const std::vector<cdc::components::fme_candidate>& candidates)
{
    for (const cdc::components::fme_candidate& candidate : candidates) {
        if (candidate.valid && candidate.half_pel) {
            return true;
        }
    }

    return false;
}

static bool has_quarter_pel_candidate(
    const std::vector<cdc::components::fme_candidate>& candidates)
{
    for (const cdc::components::fme_candidate& candidate : candidates) {
        if (candidate.valid && candidate.quarter_pel) {
            return true;
        }
    }

    return false;
}

static void check_valid_fme_result_common(
    const cdc::components::frame& input,
    const cdc::components::fme_result& result,
    const cdc::components::block& ctu,
    std::uint32_t expected_qp)
{
    using namespace cdc::components;

    CHECK(result.valid);

    CHECK(result.ctu.x == ctu.x);
    CHECK(result.ctu.y == ctu.y);
    CHECK(result.ctu.width == ctu.width);
    CHECK(result.ctu.height == ctu.height);

    CHECK(result.qp == expected_qp);

    CHECK(result.best_inter_result.valid);
    CHECK(result.best_inter_result.mode == prediction_mode::inter);
    CHECK(result.best_inter_result.qp == expected_qp);

    CHECK(result.best_inter_result.predicted_luma.size() == ctu.area());
    CHECK(result.best_inter_result.residual_luma.size() == ctu.area());
    CHECK(residual_matches_input_minus_prediction(
        input,
        ctu,
        result.best_inter_result));

    CHECK(result.best_satd < std::numeric_limits<std::uint32_t>::max());
    CHECK(result.best_rate < std::numeric_limits<std::uint32_t>::max());
    CHECK(result.best_cost < std::numeric_limits<std::uint32_t>::max());

    CHECK(result.best_cost >= result.best_satd);
    CHECK(result.best_cost == result.best_inter_result.cost);

    CHECK(result.best_inter_result.mv.x == result.best_mv.x);
    CHECK(result.best_inter_result.mv.y == result.best_mv.y);

    CHECK(!result.candidates.empty());
    CHECK(fme_candidates_are_valid(result, ctu));
    CHECK(result.best_cost == min_candidate_cost(result.candidates));
}

static bool same_fme_result_summary(const cdc::components::fme_result& a,
                                    const cdc::components::fme_result& b)
{
    return a.valid == b.valid &&
           a.qp == b.qp &&
           a.best_partition == b.best_partition &&
           a.best_mv.x == b.best_mv.x &&
           a.best_mv.y == b.best_mv.y &&
           a.best_satd == b.best_satd &&
           a.best_rate == b.best_rate &&
           a.best_cost == b.best_cost &&
           a.skip == b.skip &&
           a.best_inter_result.valid == b.best_inter_result.valid &&
           a.best_inter_result.mode == b.best_inter_result.mode &&
           a.best_inter_result.cost == b.best_inter_result.cost &&
           a.best_inter_result.rate == b.best_inter_result.rate &&
           a.best_inter_result.distortion == b.best_inter_result.distortion &&
           a.best_inter_result.qp == b.best_inter_result.qp &&
           a.best_inter_result.mv.x == b.best_inter_result.mv.x &&
           a.best_inter_result.mv.y == b.best_inter_result.mv.y &&
           a.best_inter_result.predicted_luma == b.best_inter_result.predicted_luma &&
           a.best_inter_result.residual_luma == b.best_inter_result.residual_luma;
}

static void test_fme_default_refinement()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME default refinement\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    ime ime_dut;
    fme fme_dut;

    ime_result ime_info = ime_dut.run(input, reference, ctu, INIT_QP);
    fme_result result = fme_dut.run(input, reference, ime_info, INIT_QP);

    CHECK(ime_info.valid);
    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(result.best_inter_result.mv.x) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y) <= 4);
}

static void test_fme_explicit_refine_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME explicit refine config\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    ime ime_dut;
    fme fme_dut;

    ime_result ime_info = ime_dut.run(input, reference, ctu, INIT_QP);

    fme_refine_config config = default_config();

    fme_result result =
        fme_dut.run(input, reference, ime_info, config, INIT_QP);

    CHECK(ime_info.valid);
    check_valid_fme_result_common(input, result, ctu, INIT_QP);
}

static void test_fme_integer_only_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME integer-only refinement config\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme_refine_config config = integer_only_config();

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, config, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(result.best_mv.x == mv.x);
    CHECK(result.best_mv.y == mv.y);
    CHECK(!has_half_pel_candidate(result.candidates));
    CHECK(!has_quarter_pel_candidate(result.candidates));
}

static void test_fme_half_only_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME half-pel only refinement config\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme_refine_config config = half_only_config();

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, config, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(has_half_pel_candidate(result.candidates));
    CHECK(!has_quarter_pel_candidate(result.candidates));
}

static void test_fme_quarter_enabled_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME quarter-pel enabled config\n";

    frame input = make_textured_frame(64, 64);
    frame reference = make_shifted_reference_from_current(input, 1, 0);
    block ctu(16, 16, 16, block_type::ctu);

    motion_vector mv(-4, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme_refine_config config = quarter_enabled_config();

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, config, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);
    CHECK(has_quarter_pel_candidate(result.candidates));
    CHECK(result.candidates.size() > 1);
}

static void test_fme_known_integer_shift_from_manual_ime()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME known integer shift from manual IME result\n";

    frame input = make_textured_frame(96, 96);

    // reference(x + 2, y) matches input(x, y), so the useful MV is around -2 pixels.
    frame reference = make_shifted_reference_from_current(input, 2, 0);

    block ctu(32, 32, 16, block_type::ctu);

    motion_vector ime_mv(-8, 0);
    ime_result ime_info = make_manual_ime_result(ctu, ime_mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(result.best_inter_result.mv.x - (-8)) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y) <= 4);
}

static void test_fme_known_vertical_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME known vertical shift from manual IME result\n";

    frame input = make_textured_frame(96, 96);
    frame reference = make_shifted_reference_from_current(input, 0, 2);

    block ctu(32, 32, 16, block_type::ctu);

    motion_vector ime_mv(0, -8);
    ime_result ime_info = make_manual_ime_result(ctu, ime_mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(result.best_inter_result.mv.x) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y - (-8)) <= 4);
}

static void test_fme_known_diagonal_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME known diagonal shift from manual IME result\n";

    frame input = make_textured_frame(96, 96);
    frame reference = make_shifted_reference_from_current(input, 2, 2);

    block ctu(32, 32, 16, block_type::ctu);

    motion_vector ime_mv(-8, -8);
    ime_result ime_info = make_manual_ime_result(ctu, ime_mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(result.best_inter_result.mv.x - (-8)) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y - (-8)) <= 4);
}

static void test_fme_skip_decision_on_flat_frame()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME skip decision on flat frame\n";

    frame input = make_flat_frame(64, 64, 128);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme_refine_config config = default_config();
    config.enable_skip_decision = true;

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, config, INIT_QP);

    check_valid_fme_result_common(input, result, ctu, INIT_QP);

    CHECK(result.best_satd == 0);
    CHECK(result.skip);
    CHECK(result.best_inter_result.skip);
}

static void test_fme_different_block_sizes()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME different CTU sizes\n";

    frame input = make_textured_frame(128, 128);
    frame reference = input;

    fme dut;

    const block b8(16, 16, 8, block_type::ctu);
    const block b16(32, 32, 16, block_type::ctu);
    const block b32(64, 64, 32, block_type::ctu);

    const block blocks[] = {b8, b16, b32};

    for (const block& ctu : blocks) {
        motion_vector mv(0, 0);
        ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

        fme_result result = dut.run(input, reference, ime_info, INIT_QP);

        check_valid_fme_result_common(input, result, ctu, INIT_QP);
        CHECK(result.best_satd == 0);
    }
}

static void test_fme_qp_clamp()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME QP clamp\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    const std::uint32_t high_qp = 1000;
    const std::uint32_t expected_qp = clamp_qp_for_test(high_qp);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, high_qp);

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, high_qp);

    check_valid_fme_result_common(input, result, ctu, expected_qp);
}

static void test_fme_deterministic()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME deterministic output\n";

    frame input = make_textured_frame(64, 64);
    frame reference = make_shifted_reference_from_current(input, 1, 1);
    block ctu(16, 16, 16, block_type::ctu);

    motion_vector mv(-4, -4);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme_refine_config config = default_config();

    fme dut;

    fme_result result0 = dut.run(input, reference, ime_info, config, INIT_QP);
    fme_result result1 = dut.run(input, reference, ime_info, config, INIT_QP);

    CHECK(result0.valid);
    CHECK(result1.valid);
    CHECK(same_fme_result_summary(result0, result1));
}

static void test_fme_invalid_ime_input()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME invalid IME input\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;

    ime_result invalid_ime;
    invalid_ime.valid = false;

    fme dut;
    fme_result result = dut.run(input, reference, invalid_ime, INIT_QP);

    CHECK(!result.valid);
}

static void test_fme_empty_current_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME empty current frame invalid input\n";

    frame empty;
    frame reference = make_textured_frame(64, 64);
    block ctu(0, 0, 16, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(empty, reference, ime_info, INIT_QP);

    CHECK(!result.valid);
}

static void test_fme_empty_reference_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME empty reference frame invalid input\n";

    frame input = make_textured_frame(64, 64);
    frame empty;
    block ctu(0, 0, 16, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(input, empty, ime_info, INIT_QP);

    CHECK(!result.valid);
}

static void test_fme_zero_size_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME zero-size block invalid input\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(0, 0, 0, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, INIT_QP);

    CHECK(!result.valid);
}

static void test_fme_out_of_bound_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] FME out-of-bound block invalid input\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;

    // 60 + 16 > 64, so this CTU is outside the frame.
    block ctu(60, 60, 16, block_type::ctu);

    motion_vector mv(0, 0);
    ime_result ime_info = make_manual_ime_result(ctu, mv, INIT_QP);

    fme dut;
    fme_result result = dut.run(input, reference, ime_info, INIT_QP);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " FME Full Functional Unit Test\n";
    std::cout << "========================================\n";

    test_fme_default_refinement();
    test_fme_explicit_refine_config();

    test_fme_integer_only_config();
    test_fme_half_only_config();
    test_fme_quarter_enabled_config();

    test_fme_known_integer_shift_from_manual_ime();
    test_fme_known_vertical_shift();
    test_fme_known_diagonal_shift();

    test_fme_skip_decision_on_flat_frame();
    test_fme_different_block_sizes();
    test_fme_qp_clamp();
    test_fme_deterministic();

    test_fme_invalid_ime_input();
    test_fme_empty_current_invalid();
    test_fme_empty_reference_invalid();
    test_fme_zero_size_invalid();
    test_fme_out_of_bound_invalid();

    if (g_failures == 0) {
        std::cout << "\nFME full functional test PASSED\n";
    } else {
        std::cout << "\nFME full functional test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
