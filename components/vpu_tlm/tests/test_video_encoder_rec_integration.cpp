#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include "rec_packet.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "video_encoder_tlm.h"
#include "video_encoder_to_rec.h"
#include "vpu_rec_top.h"

namespace {

class CoeffMonitor : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<CoeffMonitor> socket;
    std::vector<std::uint8_t> last_data;

    explicit CoeffMonitor(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket")
    {
        socket.register_b_transport(this, &CoeffMonitor::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        last_data.clear();
        if (trans.get_data_ptr() != nullptr && trans.get_data_length() > 0) {
            last_data.assign(trans.get_data_ptr(),
                             trans.get_data_ptr() + trans.get_data_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class DBMonitor : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<DBMonitor> socket;
    std::vector<std::uint8_t> last_data;

    explicit DBMonitor(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket")
    {
        socket.register_b_transport(this, &DBMonitor::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        last_data.clear();
        if (trans.get_data_ptr() != nullptr && trans.get_data_length() > 0) {
            last_data.assign(trans.get_data_ptr(),
                             trans.get_data_ptr() + trans.get_data_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class FrameSink : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<FrameSink> socket;

    explicit FrameSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket")
    {
        socket.register_b_transport(this, &FrameSink::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class RecDriver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<RecDriver> intra_socket;
    tlm_utils::simple_initiator_socket<RecDriver> mc_socket;

    explicit RecDriver(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          intra_socket("intra_socket"),
          mc_socket("mc_socket")
    {
    }
};

int g_failures = 0;

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (!(expr)) {                                                     \
            ++g_failures;                                                  \
            std::cerr << "[FAIL] " << #expr << std::endl;                 \
        } else {                                                           \
            std::cout << "[PASS] " << #expr << std::endl;                 \
        }                                                                 \
    } while (0)

cdc::components::frame make_textured_frame(std::uint32_t width,
                                           std::uint32_t height)
{
    cdc::components::frame f(width, height);

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t value =
                (x * 9u +
                 y * 17u +
                 ((x * y) % 19u) +
                 ((x ^ (y * 5u)) & 0x4fu)) & 0xffu;
            f.set_luma(x, y, static_cast<std::uint8_t>(value));
        }
    }

    f.fill_chroma(128, 128);
    return f;
}

cdc::components::frame make_shifted_reference_from_current(
    const cdc::components::frame& current,
    int shift_x,
    int shift_y)
{
    cdc::components::frame reference(current.width, current.height);

    for (std::uint32_t y = 0; y < current.height; ++y) {
        for (std::uint32_t x = 0; x < current.width; ++x) {
            const int src_x =
                std::clamp(static_cast<int>(x) - shift_x,
                           0,
                           static_cast<int>(current.width) - 1);
            const int src_y =
                std::clamp(static_cast<int>(y) - shift_y,
                           0,
                           static_cast<int>(current.height) - 1);

            reference.set_luma(
                x,
                y,
                current.get_luma(static_cast<std::uint32_t>(src_x),
                                 static_cast<std::uint32_t>(src_y)));
        }
    }

    reference.fill_chroma(128, 128);
    return reference;
}

void drive_request(RecDriver& driver,
                   RecTop& rec_top,
                   const cdc::components::prediction_to_rec_result& request,
                   const cdc::components::video_encoder_to_rec& bridge)
{
    CHECK(request.valid);
    CHECK(bridge.write_mv_if_needed(request, rec_top.rec_mv));

    std::vector<std::uint8_t> storage;
    tlm::tlm_generic_payload trans;
    bridge.make_transaction(request, trans, storage);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    if (bridge.targets_mc(request)) {
        driver.mc_socket->b_transport(trans, delay);
    } else {
        driver.intra_socket->b_transport(trans, delay);
    }
}

void test_video_encoder_to_rec_integration()
{
    using namespace cdc::components;

    std::cout << "\n[TEST] video encoder to rec integration\n";

    video_encoder_tlm encoder;
    frame input = make_textured_frame(64, 64);
    frame reconstructed = input;
    frame reference = make_shifted_reference_from_current(input, 1, 1);
    block region(16, 16, 16, block_type::ctu);

    video_encoder_tlm_result result =
        encoder.run_prediction(input, reconstructed, reference, region, INIT_QP);

    CHECK(result.valid);
    CHECK(result.rec_request.valid);

    RecDriver driver("driver");
    RecTop rec_top("rec_top");
    CoeffMonitor coeff_monitor("coeff_monitor");
    DBMonitor db_monitor("db_monitor");
    FrameSink frame_sink("frame_sink");

    driver.intra_socket.bind(rec_top.rec_intra.start_socket);
    driver.mc_socket.bind(rec_top.rec_mc.start_socket);
    rec_top.res_buffer.frame_socket.bind(frame_sink.socket);
    rec_top.rec_tq.cabac_socket.bind(coeff_monitor.socket);
    rec_top.inv_tq.db_socket.bind(db_monitor.socket);

    video_encoder_to_rec bridge;
    drive_request(driver, rec_top, result.rec_request, bridge);

    sc_core::sc_start(1, sc_core::SC_MS);

    CHECK(!coeff_monitor.last_data.empty());
    CHECK(!db_monitor.last_data.empty());

    RecPacket db_packet =
        unpackRecPacket(db_monitor.last_data.data(), db_monitor.last_data.size());
    CHECK(db_packet.cmd == RecCmd::RESIDUAL);
    CHECK(db_packet.pred_type == result.rec_request.packet.pred_type);
    CHECK(db_packet.qp == result.rec_request.packet.qp);
}

} // namespace

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Video Encoder Rec Integration Test\n";
    std::cout << "========================================\n";

    test_video_encoder_to_rec_integration();

    if (g_failures == 0) {
        std::cout << "\nVideo encoder rec integration test PASSED\n";
    } else {
        std::cout << "\nVideo encoder rec integration test FAILED, failures = "
                  << g_failures << "\n";
    }

    return g_failures == 0 ? 0 : 1;
}
