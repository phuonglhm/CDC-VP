#include <systemc>

#include <cstdint>
#include <iostream>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "posi.h"
#include "prediction_result.h"
#include "prei.h"
#include "prei_result.h"

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

static void check_intra_result(const cdc::components::prediction_result& result,
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
    check_intra_result(result, region);
}

static void test_posi_from_prei_flat()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI from PREI on flat frame\n";

    frame input(64, 64);
    input.fill(128, 128, 128);

    frame reconstructed = input;
    block region(0, 0, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);
    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    check_intra_result(result, region);
}

static void test_posi_compatibility_api()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] POSI compatibility API\n";

    frame input = make_gradient_frame(64, 64);
    block region(32, 16, 16, block_type::cu);

    posi posi_dut;
    prediction_result result = posi_dut.run(input, region);

    check_intra_result(result, region);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " POSI Unit Test\n";
    std::cout << "========================================\n";

    test_posi_from_prei_gradient();
    test_posi_from_prei_flat();
    test_posi_compatibility_api();

    if (g_failures == 0) {
        std::cout << "\nPOSI test PASSED\n";
    } else {
        std::cout << "\nPOSI test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
