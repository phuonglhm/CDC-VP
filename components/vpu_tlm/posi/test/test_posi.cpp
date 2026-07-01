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

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            std::cerr << "[FAIL] " << #expr << std::endl;                 \
            return 1;                                                      \
        }                                                                 \
        std::cout << "[PASS] " << #expr << std::endl;                     \
    } while (0)

static cdc::components::frame make_test_frame(std::uint32_t width,
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

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    using namespace cdc::components;

    std::cout << "========================================\n";
    std::cout << " POSI Unit Test\n";
    std::cout << "========================================\n";

    frame input = make_test_frame(64, 64);
    frame reconstructed = input;
    block region(16, 16, 16, block_type::cu);

    prei prei_dut;
    posi posi_dut;

    prei_result prei_info = prei_dut.run(input, region);

    prediction_result result =
        posi_dut.run(input, reconstructed, region, prei_info, INIT_QP);

    CHECK(prei_info.valid);
    CHECK(result.valid);
    CHECK(result.mode == prediction_mode::intra);
    CHECK(result.predicted_luma.size() == region.area());
    CHECK(result.residual_luma.size() == region.area());

    std::cout << "\nPOSI test PASSED\n";
    return 0;
}
