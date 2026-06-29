#include <cstddef>
#include <cstdint>

#include <systemc>
#include <tlm>

#include "tlm_probe.h"
#include "video_encoder_tlm.h"

namespace {

constexpr std::uint32_t REG_CONTROL = 0x00;
constexpr std::uint32_t REG_STATUS = 0x04;
constexpr std::uint32_t REG_OUTPUT_SIZE = 0x08;

constexpr std::uint32_t CONTROL_START = 1u << 0;
constexpr std::uint32_t STATUS_DONE = 1u << 0;

} // namespace

int sc_main(int, char*[])
{
    cdc::components::video_encoder_tlm encoder("encoder");
    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(encoder.socket);

    cdc::components::frame input(16, 16);
    for (std::size_t i = 0; i < input.luma.size(); ++i) {
        input.luma[i] = static_cast<std::uint8_t>(i & 0xffu);
    }
    encoder.load_input_frame(input);

    sc_core::sc_spawn([&] {
        std::uint32_t value = CONTROL_START;
        CDC_CHECK(probe.write(REG_CONTROL, &value, 4) == tlm::TLM_OK_RESPONSE);

        std::uint32_t status = 0;
        CDC_CHECK(probe.read(REG_STATUS, &status, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((status & STATUS_DONE) != 0u);

        std::uint32_t output_size = 0;
        CDC_CHECK(probe.read(REG_OUTPUT_SIZE, &output_size, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(output_size > 0u);
        CDC_CHECK(output_size == encoder.last_bitstream_size());

        std::uint32_t bad = 0;
        CDC_CHECK(probe.read(0x0c, &bad, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
