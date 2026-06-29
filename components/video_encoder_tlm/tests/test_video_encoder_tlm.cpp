#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <iostream>

#include "video_encoder_tlm.h"

namespace cdc {
namespace test {

static int g_failures = 0;

inline int failures() {
    return g_failures;
}

inline void check(bool condition, const char* expr, const char* file, int line) {
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
          socket("socket") {}
};

}  // namespace test
}  // namespace cdc

#define CDC_CHECK(expr) \
    cdc::test::check((expr), #expr, __FILE__, __LINE__)

int sc_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "========================================\n";
    std::cout << " Video Encoder TLM Skeleton Test\n";
    std::cout << "========================================\n";

    std::cout << "\n[TEST] Instantiate video_encoder_tlm\n";

    cdc::components::video_encoder_tlm encoder("encoder");

    CDC_CHECK(true);

    std::cout << "\n[TEST] Bind TLM socket\n";

    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(encoder.socket);

    CDC_CHECK(true);

    std::cout << "\n[TEST] Start SystemC simulation\n";

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    CDC_CHECK(!sc_core::sc_end_of_simulation_invoked());

    if (cdc::test::failures() == 0) {
        std::cout << "\n========================================\n";
        std::cout << " Video Encoder TLM Skeleton Test PASSED\n";
        std::cout << "========================================\n";
    } else {
        std::cout << "\n========================================\n";
        std::cout << " Video Encoder TLM Skeleton Test FAILED\n";
        std::cout << " Failures: " << cdc::test::failures() << "\n";
        std::cout << "========================================\n";
    }

    return cdc::test::failures() == 0 ? 0 : 1;
}
