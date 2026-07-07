#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

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

static std::uint32_t clamp_qp_for_test(std::uint32_t qp)
{
    return std::clamp(qp, cdc::components::MIN_QP, cdc::components::MAX_QP);
}

static cdc::components::frame make_textured_frame(std::uint32_t width,
                                                   std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 17u +
                 y * 29u +
                 ((x * y) % 37u) +
                 ((x ^ (y * 3u)) & 0x3fu)) & 0xffu;

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

static cdc::components::frame make_noisy_reference(
    const cdc::components::frame& input)
{
    cdc::components::frame reference = input;

    for (std::uint32_t y = 0; y < reference.height; ++y) {
        for (std::uint32_t x = 0; x < reference.width; ++x) {
            std::uint8_t base = reference.get_luma(x, y);

            const int noise =
                static_cast<int>((x * 13u + y * 7u + (x ^ y)) % 11u) - 5;

            const int value = std::clamp(static_cast<int>(base) + noise, 0, 255);

            reference.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    return reference;
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

static std::uint32_t sad_for_mv(const cdc::components::frame& input,
                                const cdc::components::frame& reference,
                                const cdc::components::block& ctu,
                                cdc::components::motion_vector mv)
{
    std::uint64_t sad = 0;

    const int mv_x = mv.integer_x();
    const int mv_y = mv.integer_y();

    for (std::uint32_t y = 0; y < ctu.height; ++y) {
        for (std::uint32_t x = 0; x < ctu.width; ++x) {
            const int ref_x =
                std::clamp(static_cast<int>(ctu.x + x) + mv_x,
                           0,
                           static_cast<int>(reference.width) - 1);

            const int ref_y =
                std::clamp(static_cast<int>(ctu.y + y) + mv_y,
                           0,
                           static_cast<int>(reference.height) - 1);

            const int cur =
                static_cast<int>(input.get_luma(ctu.x + x, ctu.y + y));

            const int ref =
                static_cast<int>(
                    reference.get_luma(static_cast<std::uint32_t>(ref_x),
                                       static_cast<std::uint32_t>(ref_y)));

            sad += static_cast<std::uint64_t>(abs_i(cur - ref));
        }
    }

    return sad > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(sad);
}

static std::uint32_t golden_best_sad_small_search(
    const cdc::components::frame& input,
    const cdc::components::frame& reference,
    const cdc::components::block& ctu,
    std::uint32_t search_range_x,
    std::uint32_t search_range_y)
{
    std::uint32_t best_sad = std::numeric_limits<std::uint32_t>::max();

    for (int dy = -static_cast<int>(search_range_y);
         dy <= static_cast<int>(search_range_y);
         ++dy) {
        for (int dx = -static_cast<int>(search_range_x);
             dx <= static_cast<int>(search_range_x);
             ++dx) {
            cdc::components::motion_vector mv(dx * 4, dy * 4);

            const std::uint32_t sad = sad_for_mv(input, reference, ctu, mv);

            if (sad < best_sad) {
                best_sad = sad;
            }
        }
    }

    return best_sad;
}

static bool candidates_are_valid(const cdc::components::ime_result& result,
                                 const cdc::components::block& ctu)
{
    if (result.candidates.empty()) {
        return false;
    }

    for (const cdc::components::ime_candidate& candidate : result.candidates) {
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

        if (candidate.cost < candidate.sad) {
            return false;
        }

        if (candidate.cost >= std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
    }

    return true;
}

static std::uint32_t min_candidate_cost(
    const std::vector<cdc::components::ime_candidate>& candidates)
{
    std::uint32_t best = std::numeric_limits<std::uint32_t>::max();

    for (const cdc::components::ime_candidate& candidate : candidates) {
        if (candidate.valid && candidate.cost < best) {
            best = candidate.cost;
        }
    }

    return best;
}

static std::uint32_t min_candidate_sad(
    const std::vector<cdc::components::ime_candidate>& candidates)
{
    std::uint32_t best = std::numeric_limits<std::uint32_t>::max();

    for (const cdc::components::ime_candidate& candidate : candidates) {
        if (candidate.valid && candidate.sad < best) {
            best = candidate.sad;
        }
    }

    return best;
}

static void check_valid_ime_result_common(
    const cdc::components::frame& input,
    const cdc::components::ime_result& result,
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
    if (!result.best_inter_result.residual_luma.empty()) {
        CHECK(result.best_inter_result.residual_luma.size() == ctu.area());
        CHECK(residual_matches_input_minus_prediction(
            input,
            ctu,
            result.best_inter_result));
    } else {
        std::cout << "[INFO] result.best_inter_result.residual_luma is empty in this IME model\n";
    }

    CHECK(result.best_sad < std::numeric_limits<std::uint32_t>::max());
    CHECK(result.best_rate < std::numeric_limits<std::uint32_t>::max());
    CHECK(result.best_cost < std::numeric_limits<std::uint32_t>::max());

    CHECK(result.best_cost >= result.best_sad);
    CHECK(result.best_cost == result.best_inter_result.cost);

    CHECK(result.best_inter_result.mv.x == result.best_mv.x);
    CHECK(result.best_inter_result.mv.y == result.best_mv.y);

    CHECK(candidates_are_valid(result, ctu));
    CHECK(result.best_cost == min_candidate_cost(result.candidates));

}

static bool same_ime_result_summary(const cdc::components::ime_result& a,
                                    const cdc::components::ime_result& b)
{
    return a.valid == b.valid &&
           a.qp == b.qp &&
           a.best_partition == b.best_partition &&
           a.best_mv.x == b.best_mv.x &&
           a.best_mv.y == b.best_mv.y &&
           a.best_sad == b.best_sad &&
           a.best_rate == b.best_rate &&
           a.best_cost == b.best_cost &&
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

static cdc::components::ime_search_config only_2nx2n_config(
    std::uint32_t range_x,
    std::uint32_t range_y)
{
    cdc::components::ime_search_config config;
    config.search_range_x = range_x;
    config.search_range_y = range_y;
    config.enable_2nx2n = true;
    config.enable_2nxn = false;
    config.enable_nx2n = false;
    config.enable_split = false;
    config.use_feedback = false;
    config.downsample = false;
    return config;
}

static void test_ime_identical_current_reference()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME identical current/reference frame\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    CHECK(result.best_sad == 0);
    CHECK(abs_i(result.best_mv.x) <= 4);
    CHECK(abs_i(result.best_mv.y) <= 4);
}

static void test_ime_zero_search_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME zero-search config\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(0, 0);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    CHECK(result.best_mv.x == 0);
    CHECK(result.best_mv.y == 0);
    CHECK(result.best_sad == 0);
}

static void test_ime_known_horizontal_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME known horizontal shift\n";

    frame input = make_textured_frame(96, 96);

    // reference(x + 2, y) matches input(x, y), so expected MV magnitude is 2 pixels.
    const int shift_x = 2;
    const int shift_y = 0;

    frame reference =
        make_shifted_reference_from_current(input, shift_x, shift_y);

    block ctu(32, 32, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(4, 4);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(abs_i(result.best_mv.x) - abs_i(shift_x * 4)) <= 4);
    CHECK(abs_i(result.best_mv.y - shift_y * 4) <= 4);
}

static void test_ime_known_vertical_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME known vertical shift\n";

    frame input = make_textured_frame(96, 96);

    const int shift_x = 0;
    const int shift_y = 3;

    frame reference =
        make_shifted_reference_from_current(input, shift_x, shift_y);

    block ctu(32, 32, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(4, 4);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(result.best_mv.x - shift_x * 4) <= 4);
    CHECK(abs_i(abs_i(result.best_mv.y) - abs_i(shift_y * 4)) <= 4);
}

static void test_ime_known_diagonal_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME known diagonal shift\n";

    frame input = make_textured_frame(96, 96);

    const int shift_x = 2;
    const int shift_y = 2;

    frame reference =
        make_shifted_reference_from_current(input, shift_x, shift_y);

    block ctu(32, 32, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(4, 4);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(abs_i(result.best_mv.x) - abs_i(shift_x * 4)) <= 4);
    CHECK(abs_i(abs_i(result.best_mv.y) - abs_i(shift_y * 4)) <= 4);
}

static void test_ime_negative_horizontal_shift()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME negative horizontal shift\n";

    frame input = make_textured_frame(96, 96);

    const int shift_x = -2;
    const int shift_y = 0;

    frame reference =
        make_shifted_reference_from_current(input, shift_x, shift_y);

    block ctu(32, 32, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(4, 4);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    CHECK(abs_i(abs_i(result.best_mv.x) - abs_i(shift_x * 4)) <= 4);
    CHECK(abs_i(result.best_mv.y - shift_y * 4) <= 4);
}

static void test_ime_noisy_reference()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME noisy reference frame still produces valid result\n";

    frame input = make_textured_frame(64, 64);
    frame reference = make_noisy_reference(input);
    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);
}

static void test_ime_golden_sad_small_search()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME golden SAD comparison with small search\n";

    frame input = make_textured_frame(64, 64);
    frame reference = make_shifted_reference_from_current(input, 1, 1);
    block ctu(24, 24, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(2, 2);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, config, INIT_QP);

    check_valid_ime_result_common(input, result, ctu, INIT_QP);

    const std::uint32_t golden_sad =
        golden_best_sad_small_search(input,
                                     reference,
                                     ctu,
                                     config.search_range_x,
                                     config.search_range_y);

    CHECK(result.best_sad == golden_sad);
}

static void test_ime_different_block_sizes()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME different CTU sizes\n";

    frame input = make_textured_frame(128, 128);
    frame reference = input;

    ime dut;

    const block b8(16, 16, 8, block_type::ctu);
    const block b16(32, 32, 16, block_type::ctu);
    const block b32(64, 64, 32, block_type::ctu);

    const block blocks[] = {b8, b16, b32};

    for (const block& ctu : blocks) {
        ime_result result = dut.run(input, reference, ctu, INIT_QP);
        check_valid_ime_result_common(input, result, ctu, INIT_QP);
        CHECK(result.best_sad == 0);
    }
}

static void test_ime_qp_clamp()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME QP clamp\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(16, 16, 16, block_type::ctu);

    const std::uint32_t high_qp = 1000;
    const std::uint32_t expected_qp = clamp_qp_for_test(high_qp);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, high_qp);

    check_valid_ime_result_common(input, result, ctu, expected_qp);
}

static void test_ime_deterministic()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME deterministic output\n";

    frame input = make_textured_frame(64, 64);
    frame reference = make_shifted_reference_from_current(input, 2, 1);
    block ctu(16, 16, 16, block_type::ctu);

    ime_search_config config = only_2nx2n_config(4, 4);

    ime dut;

    ime_result result0 = dut.run(input, reference, ctu, config, INIT_QP);
    ime_result result1 = dut.run(input, reference, ctu, config, INIT_QP);

    CHECK(result0.valid);
    CHECK(result1.valid);
    CHECK(same_ime_result_summary(result0, result1));
}

static void test_ime_compatibility_api()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME compatibility API\n";

    frame input = make_textured_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    ime dut;
    prediction_result result = dut.run(input, ctu);

    CHECK(result.valid);
    CHECK(result.mode == prediction_mode::inter);
    CHECK(result.qp == INIT_QP);
    CHECK(result.predicted_luma.size() == ctu.area());
    if (!result.residual_luma.empty()) {
        CHECK(result.residual_luma.size() == ctu.area());
    } else {
        std::cout << "[INFO] result.residual_luma is empty in this IME model\n";
    }
    CHECK(result.cost < std::numeric_limits<std::uint32_t>::max());
}

static void test_ime_empty_current_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME empty current frame invalid input\n";

    frame empty;
    frame reference = make_textured_frame(64, 64);
    block ctu(0, 0, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(empty, reference, ctu, INIT_QP);

    CHECK(!result.valid);
}

static void test_ime_empty_reference_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME empty reference frame invalid input\n";

    frame input = make_textured_frame(64, 64);
    frame empty;
    block ctu(0, 0, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, empty, ctu, INIT_QP);

    CHECK(!result.valid);
}

static void test_ime_zero_size_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME zero-size block invalid input\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;
    block ctu(0, 0, 0, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    CHECK(!result.valid);
}

static void test_ime_out_of_bound_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] IME out-of-bound block invalid input\n";

    frame input = make_textured_frame(64, 64);
    frame reference = input;

    // 60 + 16 > 64, so this CTU is outside the frame.
    block ctu(60, 60, 16, block_type::ctu);

    ime dut;
    ime_result result = dut.run(input, reference, ctu, INIT_QP);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " IME Full Functional Unit Test\n";
    std::cout << "========================================\n";

    test_ime_identical_current_reference();
    test_ime_zero_search_config();

    test_ime_known_horizontal_shift();
    test_ime_known_vertical_shift();
    test_ime_known_diagonal_shift();
    test_ime_negative_horizontal_shift();

    test_ime_noisy_reference();
    test_ime_golden_sad_small_search();

    test_ime_different_block_sizes();
    test_ime_qp_clamp();
    test_ime_deterministic();
    test_ime_compatibility_api();

    test_ime_empty_current_invalid();
    test_ime_empty_reference_invalid();
    test_ime_zero_size_invalid();
    test_ime_out_of_bound_invalid();

    if (g_failures == 0) {
        std::cout << "\nIME full functional test PASSED\n";
    } else {
        std::cout << "\nIME full functional test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
