#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "block.h"
#include "encoder_defs.h"
#include "fme.h"
#include "frame.h"
#include "ime.h"
#include "posi.h"
#include "prei.h"
#include "prediction_result.h"
#include "video_encoder_tlm.h"

namespace cdc {
namespace test {

static int g_failures = 0;

inline int failures()
{
    return g_failures;
}

inline void check(bool condition,
                  const char* expr,
                  const char* file,
                  int line)
{
    if (!condition) {
        ++g_failures;
        std::cerr << "[FAIL] " << file << ":" << line
                  << " check failed: " << expr << std::endl;
    } else {
        std::cout << "[PASS] " << expr << std::endl;
    }
}

class tlm_probe : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<tlm_probe> socket;

    explicit tlm_probe(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket")
    {
    }
};

} // namespace test
} // namespace cdc

#define CDC_CHECK(expr) \
    cdc::test::check((expr), #expr, __FILE__, __LINE__)

namespace {

using namespace cdc::components;

int abs_i(int value)
{
    return value < 0 ? -value : value;
}

frame make_test_frame(std::uint32_t width, std::uint32_t height)
{
    frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 7u + y * 13u + ((x * y) % 31u) + ((x ^ y) & 0x0fu)) & 0xffu;

            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

void test_prei_basic()
{
    std::cout << "\n[TEST] PREI basic intra analysis\n";

    const frame input = make_test_frame(64, 64);
    const block ctu(16, 16, 16);

    prei prei_unit;
    const prei_result prei_info = prei_unit.run(input, ctu);

    CDC_CHECK(prei_info.valid);
}

void test_posi_from_prei()
{
    std::cout << "\n[TEST] POSI intra prediction from PREI result\n";

    const frame input = make_test_frame(64, 64);
    const frame reconstructed = input;
    const block region(16, 16, 16);

    prei prei_unit;
    posi posi_unit;

    const prei_result prei_info = prei_unit.run(input, region);

    const prediction_result intra =
        posi_unit.run(input, reconstructed, region, prei_info, INIT_QP);

    CDC_CHECK(intra.valid);
    CDC_CHECK(intra.mode == prediction_mode::intra);
    CDC_CHECK(intra.cost < std::numeric_limits<std::uint32_t>::max());
    CDC_CHECK(intra.predicted_luma.size() == region.area());
    CDC_CHECK(intra.residual_luma.size() == region.area());
}

void test_ime_identical_reference()
{
    std::cout << "\n[TEST] IME integer motion estimation on identical frames\n";

    const frame input = make_test_frame(64, 64);
    const frame reference = input;
    const block ctu(16, 16, 16);

    ime ime_unit;
    const ime_result ime_info = ime_unit.run(input, reference, ctu, INIT_QP);

    CDC_CHECK(ime_info.valid);
    CDC_CHECK(ime_info.best_inter_result.valid);
    CDC_CHECK(ime_info.best_inter_result.mode == prediction_mode::inter);

    // With identical current/reference frames, best MV should be zero or very close
    // to zero. MV unit is quarter-pel.
    CDC_CHECK(abs_i(ime_info.best_mv.x) <= 4);
    CDC_CHECK(abs_i(ime_info.best_mv.y) <= 4);

    CDC_CHECK(ime_info.best_inter_result.predicted_luma.size() == ctu.area());
    CDC_CHECK(ime_info.best_inter_result.residual_luma.size() == ctu.area());
}

void test_fme_from_ime()
{
    std::cout << "\n[TEST] FME fractional refinement from IME result\n";

    const frame input = make_test_frame(64, 64);
    const frame reference = input;
    const block ctu(16, 16, 16);

    ime ime_unit;
    fme fme_unit;

    const ime_result ime_info = ime_unit.run(input, reference, ctu, INIT_QP);
    const fme_result fme_info = fme_unit.run(input, reference, ime_info, INIT_QP);

    CDC_CHECK(ime_info.valid);
    CDC_CHECK(fme_info.valid);
    CDC_CHECK(fme_info.best_inter_result.valid);
    CDC_CHECK(fme_info.best_inter_result.mode == prediction_mode::inter);

    // FME output MV is quarter-pel. On identical frames, it should stay near zero.
    CDC_CHECK(abs_i(fme_info.best_inter_result.mv.x) <= 4);
    CDC_CHECK(abs_i(fme_info.best_inter_result.mv.y) <= 4);

    CDC_CHECK(fme_info.best_inter_result.cost < std::numeric_limits<std::uint32_t>::max());
    CDC_CHECK(fme_info.best_inter_result.predicted_luma.size() == ctu.area());
}

void test_top_skeleton_still_instantiates()
{
    std::cout << "\n[TEST] Instantiate and bind video_encoder_tlm top\n";

    cdc::components::video_encoder_tlm encoder("encoder");
    cdc::test::tlm_probe probe("probe");

    probe.socket.bind(encoder.socket);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    CDC_CHECK(!sc_core::sc_end_of_simulation_invoked());
}

} // namespace

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Video Encoder TLM Unit Tests\n";
    std::cout << "========================================\n";

    test_prei_basic();
    test_posi_from_prei();
    test_ime_identical_reference();
    test_fme_from_ime();
    test_top_skeleton_still_instantiates();

    if (cdc::test::failures() == 0) {
        std::cout << "\n========================================\n";
        std::cout << " Video Encoder TLM Unit Tests PASSED\n";
        std::cout << "========================================\n";
    } else {
        std::cout << "\n========================================\n";
        std::cout << " Video Encoder TLM Unit Tests FAILED\n";
        std::cout << " Failures: " << cdc::test::failures() << "\n";
        std::cout << "========================================\n";
    }

    return cdc::test::failures() == 0 ? 0 : 1;
}
