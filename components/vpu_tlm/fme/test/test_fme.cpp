#include <systemc>

#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "block.h"
#include "encoder_defs.h"
#include "fme.h"
#include "fme_result.h"
#include "frame.h"
#include "ime.h"
#include "ime_result.h"
#include "prediction_result.h"

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            std::cerr << "[FAIL] " << #expr << std::endl;                 \
            return 1;                                                      \
        }                                                                 \
        std::cout << "[PASS] " << #expr << std::endl;                     \
    } while (0)

static int abs_i(int value)
{
    return value < 0 ? -value : value;
}

static cdc::components::frame make_test_frame(std::uint32_t width,
                                               std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 9u + y * 17u + ((x * y) % 37u)) & 0xffu;

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
    std::cout << " FME Unit Test\n";
    std::cout << "========================================\n";

    frame input = make_test_frame(64, 64);
    frame reference = input;

    block ctu(16, 16, 16, block_type::ctu);

    ime ime_dut;
    fme fme_dut;

    ime_result ime_info = ime_dut.run(input, reference, ctu, INIT_QP);
    fme_result result = fme_dut.run(input, reference, ime_info, INIT_QP);

    CHECK(ime_info.valid);
    CHECK(result.valid);
    CHECK(result.best_inter_result.valid);
    CHECK(result.best_inter_result.mode == prediction_mode::inter);
    CHECK(result.best_inter_result.predicted_luma.size() == ctu.area());

    // Identical current/reference frame: FME refined MV should stay near zero.
    // MV unit is quarter-pel.
    CHECK(abs_i(result.best_inter_result.mv.x) <= 4);
    CHECK(abs_i(result.best_inter_result.mv.y) <= 4);

    std::cout << "\nFME test PASSED\n";
    return 0;
}
