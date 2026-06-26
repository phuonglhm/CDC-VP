#include <array>
#include <cstdint>

#include <systemc>
#include <tlm>

#include <flash_nor_tlm.h>
#include <qspi_tlm.h>
#include <tlm_probe.h>

namespace {

tlm::tlm_response_status write32(cdc::test::tlm_probe& probe,
                                 std::uint32_t offset,
                                 std::uint32_t value)
{
    return probe.write(offset, &value, sizeof(value));
}

tlm::tlm_response_status read32(cdc::test::tlm_probe& probe,
                                std::uint32_t offset,
                                std::uint32_t& value)
{
    value = 0;
    return probe.read(offset, &value, sizeof(value));
}

} // namespace

int sc_main(int, char**)
{
    cdc::components::qspi_tlm qspi("qspi");
    cdc::components::flash_nor_tlm flash("flash", 256);
    cdc::test::tlm_probe probe("probe");
    sc_core::sc_signal<bool> reset_n;
    sc_core::sc_signal<bool> irq;

    probe.socket.bind(qspi.from_apb_socket);
    qspi.to_flash_socket.bind(flash.from_qspi_socket);
    qspi.reset_n(reset_n);
    qspi.irq(irq);
    reset_n.write(true);

    const std::array<std::uint8_t, 8> image{{0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7}};
    flash.load(image.data(), image.size(), 0x20);

    sc_core::sc_spawn([&] {
        std::uint32_t value = 0;

        CDC_CHECK(write32(probe, cdc::components::QSPI_CTRL,
                          cdc::components::QSPI_CTRL_EN) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_INT_EN,
                          cdc::components::QSPI_INT_DONE) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_CMD, 0x03) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_ADDR, 0x20) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_LEN, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_CFG, 1) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_START, 1) == tlm::TLM_OK_RESPONSE);
        wait(1, sc_core::SC_NS);

        CDC_CHECK(irq.read());
        CDC_CHECK(read32(probe, cdc::components::QSPI_STATUS, value) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((value & cdc::components::QSPI_STATUS_BUSY) == 0);
        CDC_CHECK((value & cdc::components::QSPI_STATUS_RXNE) != 0);

        for (std::uint32_t expected = 0xa0; expected <= 0xa3; ++expected) {
            CDC_CHECK(read32(probe, cdc::components::QSPI_DATA, value) == tlm::TLM_OK_RESPONSE);
            CDC_CHECK(value == expected);
        }
        CDC_CHECK(read32(probe, cdc::components::QSPI_STATUS, value) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((value & cdc::components::QSPI_STATUS_RXNE) == 0);

        CDC_CHECK(write32(probe, cdc::components::QSPI_INT_STATUS,
                          cdc::components::QSPI_INT_DONE) == tlm::TLM_OK_RESPONSE);
        wait(1, sc_core::SC_NS);
        CDC_CHECK(!irq.read());

        CDC_CHECK(write32(probe, cdc::components::QSPI_CMD, 0x9f) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_ADDR, 0) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_LEN, 3) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(write32(probe, cdc::components::QSPI_START, 1) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(read32(probe, cdc::components::QSPI_DATA, value) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(value == 0xef);
        CDC_CHECK(read32(probe, cdc::components::QSPI_DATA, value) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(value == 0x40);
        CDC_CHECK(read32(probe, cdc::components::QSPI_DATA, value) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(value == 0x18);

        CDC_CHECK(read32(probe, 0xff, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
