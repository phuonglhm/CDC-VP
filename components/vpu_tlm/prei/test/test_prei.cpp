#include <systemc>

#include <cstdint>
#include <iostream>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
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
                (x * 7u + y * 13u + ((x * y) % 29u)) & 0xffu;

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
    std::cout << " PREI Unit Test\n";
    std::cout << "========================================\n";

    frame input = make_test_frame(64, 64);
    block ctu(16, 16, 16, block_type::ctu);

    prei dut;
    prei_result result = dut.run(input, ctu);

    CHECK(result.valid);

    std::cout << "\nPREI test PASSED\n";
    return 0;
}
