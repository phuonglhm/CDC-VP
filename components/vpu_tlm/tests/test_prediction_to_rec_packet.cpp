#include <systemc>

#include <cstdint>
#include <iostream>

#include "block_coord_codec.h"
#include "prediction_result.h"
#include "prediction_to_rec_packet.h"

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

static void test_intra_mapping()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] prediction to rec packet intra mapping\n";

    block region(16, 8, 16, block_type::ctu);
    prediction_result intra =
        prediction_result::make_intra(42, intra_prediction_mode::angular_26,
                                      partition_mode::part_2nx2n, 27);

    prediction_to_rec_packet dut;
    prediction_to_rec_result result = dut.run(region, intra);

    CHECK(result.valid);
    CHECK(result.packet.cmd == RecCmd::READ_REQ);
    CHECK(result.packet.pred_type == static_cast<uint8_t>(PredType::INTRA));
    CHECK(result.packet.mode == 26);
    CHECK(result.packet.size == 2);
    CHECK(result.packet.x == 4);
    CHECK(result.packet.y == 2);
    CHECK(!result.has_mv);
}

static void test_inter_mapping()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] prediction to rec packet inter mapping\n";

    block region(4, 12, 8, block_type::ctu);
    prediction_result inter =
        prediction_result::make_inter(17, motion_vector(8, -4),
                                      partition_mode::part_2nx2n, 22);

    prediction_to_rec_packet dut;
    prediction_to_rec_result result = dut.run(region, inter);

    CHECK(result.valid);
    CHECK(result.packet.cmd == RecCmd::READ_REQ);
    CHECK(result.packet.pred_type == static_cast<uint8_t>(PredType::MC));
    CHECK(result.packet.size == 1);
    CHECK(result.has_mv);
    CHECK(result.mv.x == 2);
    CHECK(result.mv.y == -1);
    CHECK(result.mv_addr ==
          ((static_cast<std::uint32_t>(result.packet.block_idx) << 16u) |
           (static_cast<std::uint32_t>(result.packet.y) << 8u) |
           static_cast<std::uint32_t>(result.packet.x)));
}

static void test_extended_mv_address_mapping()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] prediction to rec packet extended mv address mapping\n";

    prediction_result inter =
        prediction_result::make_inter(7, motion_vector(4, 0),
                                      partition_mode::part_2nx2n, 22);

    prediction_to_rec_packet dut;
    prediction_to_rec_result left = dut.run(block(0, 0, 16, block_type::ctu), inter);
    prediction_to_rec_result right = dut.run(block(64, 0, 16, block_type::ctu), inter);
    prediction_to_rec_result far =
        dut.run(block(4096, 4096, 16, block_type::ctu), inter);

    CHECK(left.valid);
    CHECK(right.valid);
    CHECK(far.valid);
    CHECK(left.packet.x == 0);
    CHECK(right.packet.x == 16);
    CHECK(left.mv_addr != right.mv_addr);
    CHECK(far.packet.block_idx != left.packet.block_idx ||
          far.packet.x != left.packet.x ||
          far.packet.y != left.packet.y);
    CHECK(far.mv_addr != left.mv_addr);

    const block_coord_4x4 decoded_far =
        decode_block_coord_4x4(far.packet.block_idx, far.packet.x, far.packet.y);
    CHECK(decoded_far.x == 1024);
    CHECK(decoded_far.y == 1024);
}

static void test_invalid_mapping()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] prediction to rec packet invalid mapping\n";

    prediction_to_rec_packet dut;
    prediction_to_rec_result result =
        dut.run(block(0, 0, 0, block_type::ctu), prediction_result::invalid());

    CHECK(!result.valid);
}

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Prediction To Rec Packet Test\n";
    std::cout << "========================================\n";

    test_intra_mapping();
    test_inter_mapping();
    test_extended_mv_address_mapping();
    test_invalid_mapping();

    if (g_failures == 0) {
        std::cout << "\nPrediction to rec packet test PASSED\n";
    } else {
        std::cout << "\nPrediction to rec packet test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
