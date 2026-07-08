#include <systemc>

#include <cstdint>
#include <iostream>

#include "mode_decision.h"
#include "prediction_result.h"

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

static void test_pick_lower_cost()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] mode decision picks lower cost\n";

    prediction_result intra =
        prediction_result::make_intra(120, intra_prediction_mode::dc,
                                      partition_mode::part_2nx2n, INIT_QP);
    prediction_result inter =
        prediction_result::make_inter(80, motion_vector(0, 0),
                                      partition_mode::part_2nx2n, INIT_QP);

    mode_decision dut;
    mode_decision_result result = dut.run(intra, inter);

    CHECK(result.valid);
    CHECK(result.selected_mode == prediction_mode::inter);
    CHECK(result.selected.mode == prediction_mode::inter);
    CHECK(result.selected.cost == 80);
}

static void test_pick_only_valid_candidate()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] mode decision picks only valid candidate\n";

    prediction_result intra =
        prediction_result::make_intra(90, intra_prediction_mode::planar,
                                      partition_mode::part_2nx2n, INIT_QP);
    prediction_result inter = prediction_result::invalid();

    mode_decision dut;
    mode_decision_result result = dut.run(intra, inter);

    CHECK(result.valid);
    CHECK(result.selected_mode == prediction_mode::intra);
    CHECK(result.selected.mode == prediction_mode::intra);
    CHECK(result.selected.cost == 90);
}

static void test_tie_break_prefers_skip_inter()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] mode decision tie-break prefers skip inter\n";

    prediction_result intra =
        prediction_result::make_intra(64, intra_prediction_mode::dc,
                                      partition_mode::part_2nx2n, INIT_QP);
    prediction_result inter =
        prediction_result::make_inter(64, motion_vector(0, 0),
                                      partition_mode::part_2nx2n, INIT_QP);
    inter.skip = true;

    mode_decision dut;
    mode_decision_result result = dut.run(intra, inter);

    CHECK(result.valid);
    CHECK(result.selected_mode == prediction_mode::inter);
    CHECK(result.selected.skip);
}

static void test_tie_break_defaults_to_intra()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] mode decision tie-break defaults to intra\n";

    prediction_result intra =
        prediction_result::make_intra(64, intra_prediction_mode::dc,
                                      partition_mode::part_2nx2n, INIT_QP);
    prediction_result inter =
        prediction_result::make_inter(64, motion_vector(0, 0),
                                      partition_mode::part_2nx2n, INIT_QP);

    mode_decision dut;
    mode_decision_result result = dut.run(intra, inter);

    CHECK(result.valid);
    CHECK(result.selected_mode == prediction_mode::intra);
    CHECK(result.selected.mode == prediction_mode::intra);
}

static void test_invalid_when_no_candidates()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] mode decision invalid when no candidates\n";

    mode_decision dut;
    mode_decision_result result =
        dut.run(prediction_result::invalid(), prediction_result::invalid());

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Mode Decision Unit Test\n";
    std::cout << "========================================\n";

    test_pick_lower_cost();
    test_pick_only_valid_candidate();
    test_tie_break_prefers_skip_inter();
    test_tie_break_defaults_to_intra();
    test_invalid_when_no_candidates();

    if (g_failures == 0) {
        std::cout << "\nMode decision test PASSED\n";
    } else {
        std::cout << "\nMode decision test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
