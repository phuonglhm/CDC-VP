#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "prei.h"
#include "prei_result.h"
#include "../../fetch/include/fetch_loader_registry.h"
#include "../../fetch/include/fetch_frame_loader.h"

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
    if (auto loader = fetch::get_loader()) {
        std::vector<uint8_t> data;
        if (loader->load_rect(0, 0, 0, width, height, data)) {
            cdc::components::frame f(width, height);
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    f.set_luma(x, y, data[y * width + x]);
                }
            }
            f.fill_chroma(128, 128);
            return f;
        }
    }

    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 7u + y * 13u + ((x * y) % 29u) + ((x ^ y) & 0x0fu)) & 0xffu;

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

static bool is_dc_or_planar(cdc::components::intra_prediction_mode mode)
{
    using namespace cdc::components;

    return mode == intra_prediction_mode::dc ||
           mode == intra_prediction_mode::planar;
}

static bool mode_index_in_range(std::uint32_t mode_index)
{
    return mode_index <= 34;
}

static std::uint32_t min_candidate_cost(
    const std::vector<cdc::components::prei_mode_candidate>& candidates)
{
    std::uint32_t min_cost = std::numeric_limits<std::uint32_t>::max();

    for (const auto& candidate : candidates) {
        if (candidate.valid) {
            min_cost = std::min(min_cost, candidate.cost);
        }
    }

    return min_cost;
}

static const cdc::components::prei_mode_candidate* find_best_candidate(
    const cdc::components::prei_mode_entry& entry)
{
    const cdc::components::prei_mode_candidate* best = nullptr;

    for (const auto& candidate : entry.candidates) {
        if (!candidate.valid) {
            continue;
        }

        if (best == nullptr || candidate.cost < best->cost) {
            best = &candidate;
        }
    }

    return best;
}

static void check_valid_result_common(const cdc::components::prei_result& result,
                                      const cdc::components::block& ctu)
{
    using namespace cdc::components;

    CHECK(result.valid);

    CHECK(result.ctu.x == ctu.x);
    CHECK(result.ctu.y == ctu.y);
    CHECK(result.ctu.width == ctu.width);
    CHECK(result.ctu.height == ctu.height);

    CHECK(result.qp >= MIN_QP);
    CHECK(result.qp <= MAX_QP);

    CHECK(!result.mode_entries.empty());
    CHECK(result.modebest64_sum < std::numeric_limits<std::uint32_t>::max());

    for (const auto& entry : result.mode_entries) {
        CHECK(entry.valid);
        CHECK(entry.cu.area() > 0);
        CHECK(entry.cu.right() <= ctu.right());
        CHECK(entry.cu.bottom() <= ctu.bottom());

        CHECK(!entry.candidates.empty());
        CHECK(mode_index_in_range(entry.best_mode_index));
        CHECK(entry.best_cost < std::numeric_limits<std::uint32_t>::max());

        const auto* best = find_best_candidate(entry);
        CHECK(best != nullptr);

        if (best != nullptr) {
            CHECK(best->valid);
            CHECK(mode_index_in_range(best->mode_index));
            CHECK(best->cost == min_candidate_cost(entry.candidates));
            CHECK(entry.best_cost == best->cost);
            CHECK(entry.best_mode_index == best->mode_index);
        }

        for (const auto& candidate : entry.candidates) {
            CHECK(candidate.valid);
            CHECK(mode_index_in_range(candidate.mode_index));
            CHECK(candidate.cost < std::numeric_limits<std::uint32_t>::max());
        }
    }
}

static bool same_prei_result_summary(const cdc::components::prei_result& a,
                                     const cdc::components::prei_result& b)
{
    if (a.valid != b.valid) {
        return false;
    }

    if (a.qp != b.qp) {
        return false;
    }

    if (a.modebest64_sum != b.modebest64_sum) {
        return false;
    }

    if (a.mode_entries.size() != b.mode_entries.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.mode_entries.size(); ++i) {
        const auto& ea = a.mode_entries[i];
        const auto& eb = b.mode_entries[i];

        if (ea.valid != eb.valid) {
            return false;
        }

        if (ea.best_mode_index != eb.best_mode_index) {
            return false;
        }

        if (ea.best_cost != eb.best_cost) {
            return false;
        }

        if (ea.candidates.size() != eb.candidates.size()) {
            return false;
        }
    }

    return true;
}

static void test_prei_gradient_ctu()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI gradient CTU\n";

    frame input = make_gradient_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    check_valid_result_common(result, ctu);
}

static void test_prei_flat_ctu()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI flat CTU\n";

    frame input = make_flat_frame(64, 64, 128);
    block ctu(0, 0, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    check_valid_result_common(result, ctu);

    for (const auto& entry : result.mode_entries) {
        CHECK(is_dc_or_planar(entry.best_mode));
    }
}

static void test_prei_horizontal_edge_ctu()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI horizontal edge CTU\n";

    frame input = make_horizontal_edge_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    check_valid_result_common(result, ctu);
}

static void test_prei_vertical_edge_ctu()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI vertical edge CTU\n";

    frame input = make_vertical_edge_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    check_valid_result_common(result, ctu);
}

static void test_prei_multiple_positions()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI multiple block positions\n";

    frame input = make_gradient_frame(64, 64);
    prei dut;

    const block block0(0, 0, 16, block_type::ctu);
    const block block1(16, 16, 16, block_type::ctu);
    const block block2(32, 32, 16, block_type::ctu);
    const block block3(48, 48, 16, block_type::ctu);

    check_valid_result_common(dut.run(input, block0), block0);
    check_valid_result_common(dut.run(input, block1), block1);
    check_valid_result_common(dut.run(input, block2), block2);
    check_valid_result_common(dut.run(input, block3), block3);
}

static void test_prei_different_block_sizes()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI different CTU/CU sizes\n";

    frame input = make_gradient_frame(128, 128);
    prei dut;

    const block b8(16, 16, 8, block_type::ctu);
    const block b16(32, 32, 16, block_type::ctu);
    const block b32(64, 64, 32, block_type::ctu);

    check_valid_result_common(dut.run(input, b8), b8);
    check_valid_result_common(dut.run(input, b16), b16);
    check_valid_result_common(dut.run(input, b32), b32);
}

static void test_prei_deterministic()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI deterministic output\n";

    frame input = make_gradient_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei dut;

    prei_result result0 = dut.run(input, ctu);
    prei_result result1 = dut.run(input, ctu);

    CHECK(result0.valid);
    CHECK(result1.valid);
    CHECK(same_prei_result_summary(result0, result1));
}

static void test_prei_rate_control_config()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI rate-control config clamp\n";

    frame input = make_gradient_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei_rate_control_config config;
    config.initial_qp = 40;
    config.min_qp = 30;
    config.max_qp = 35;

    prei dut;
    prei_result result = dut.run(input, ctu, config);

    check_valid_result_common(result, ctu);
    CHECK(result.qp >= config.min_qp);
    CHECK(result.qp <= config.max_qp);
}

static void test_prei_empty_frame_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI empty frame invalid input\n";

    frame empty;
    block ctu(0, 0, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(empty, ctu);

    CHECK(!result.valid);
}

static void test_prei_zero_size_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI zero-size block invalid input\n";

    frame input = make_gradient_frame(64, 64);
    block ctu(0, 0, 0, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    CHECK(!result.valid);
}

static void test_prei_out_of_bound_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI out-of-bound block invalid input\n";

    frame input = make_gradient_frame(64, 64);

    // 60 + 16 > 64, so this CTU is outside the frame.
    block ctu(60, 60, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " PREI Full Functional Unit Test\n";
    std::cout << "========================================\n";

    test_prei_gradient_ctu();
    test_prei_flat_ctu();
    test_prei_horizontal_edge_ctu();
    test_prei_vertical_edge_ctu();
    test_prei_multiple_positions();
    test_prei_different_block_sizes();
    test_prei_deterministic();
    test_prei_rate_control_config();

    test_prei_empty_frame_invalid();
    test_prei_zero_size_invalid();
    test_prei_out_of_bound_invalid();

    if (g_failures == 0) {
        std::cout << "\nPREI full functional test PASSED\n";
    } else {
        std::cout << "\nPREI full functional test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
