#include <systemc>

#include <cstdint>
#include <iostream>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
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
                (x * 7u + y * 13u + ((x * y) % 29u) + ((x ^ y) & 0x0fu)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

static void test_prei_gradient_ctu()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI gradient CTU\n";

    frame input = make_gradient_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    CHECK(result.valid);
}

static void test_prei_flat_ctu()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] PREI flat CTU\n";

    frame input(64, 64);
    input.fill(128, 128, 128);

    block ctu(0, 0, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    CHECK(result.valid);
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

    const prei_result result0 = dut.run(input, block0);
    const prei_result result1 = dut.run(input, block1);
    const prei_result result2 = dut.run(input, block2);

    CHECK(result0.valid);
    CHECK(result1.valid);
    CHECK(result2.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " PREI Unit Test\n";
    std::cout << "========================================\n";

    test_prei_gradient_ctu();
    test_prei_flat_ctu();
    test_prei_multiple_positions();

    if (g_failures == 0) {
        std::cout << "\nPREI test PASSED\n";
    } else {
        std::cout << "\nPREI test FAILED, failures = " << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
