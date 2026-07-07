#include <systemc>

#include <cstdint>
#include <iostream>
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
                (x * 5u + y * 11u + ((x ^ y) & 0x1fu)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
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

static void check_intra_result_shape(const cdc::components::prediction_result& result,
                                     const cdc::components::block& region)
{
    using namespace cdc::components;

    CHECK(result.valid);
    CHECK(result.mode == prediction_mode::intra);
    CHECK(result.predicted_luma.size() == region.area());
    CHECK(result.residual_luma.size() == region.area());
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
    check_intra_result_shape(result, region);
    CHECK(result.cost < UINT32_MAX);
}

static void test_posi_flat_frame_zero_residual()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI flat frame should produce zero residual\n";

    frame input(64, 64);
    input.fill(128, 128, 128);

    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);
    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    check_intra_result_shape(result, region);

    CHECK(all_predicted_equal(result.predicted_luma, 128));
    CHECK(abs_residual_sum(result.residual_luma) == 0);
}

static void test_posi_compatibility_api()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI compatibility API\n";

    frame input = make_gradient_frame(64, 64);
    block region(32, 16, 16, block_type::cu);

    posi posi_dut;
    prediction_result result = posi_dut.run(input, region);

    check_intra_result_shape(result, region);
}

static void test_posi_empty_frame_invalid()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI empty frame invalid input\n";

    frame empty;
    block region(0, 0, 16, block_type::cu);

    posi posi_dut;
    prediction_result result = posi_dut.run(empty, region);

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " POSI Deep Unit Test\n";
    std::cout << "========================================\n";

    test_posi_from_prei_gradient();
    test_posi_flat_frame_zero_residual();
    test_posi_compatibility_api();
    test_posi_empty_frame_invalid();

    if (g_failures == 0) {
        std::cout << "\nPOSI deep test PASSED\n";
    } else {
        std::cout << "\nPOSI deep test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
