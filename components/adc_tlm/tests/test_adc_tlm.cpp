#include <cstdint>

#include <systemc>
#include <tlm>

#include "adc_tlm.h"
#include "tlm_probe.h"

namespace {

constexpr std::uint32_t REG_CONTROL = 0x00;
constexpr std::uint32_t REG_STATUS = 0x04;
constexpr std::uint32_t REG_DATA = 0x08;
constexpr std::uint32_t REG_INTR_ENABLE = 0x0C;

constexpr std::uint32_t CTRL_START = 1u << 0;
constexpr std::uint32_t CTRL_ADC_EN = 1u << 1;
constexpr std::uint32_t STATUS_EOC = 1u << 0;
constexpr std::uint32_t INTR_EOC = 1u << 0;

} // namespace

int sc_main(int, char*[])
{
    cdc::components::adc_tlm adc("adc");
    sc_core::sc_signal<bool> irq("irq");
    adc.irq_out(irq);

    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(adc.socket);

    sc_core::sc_spawn([&] {
        std::uint32_t value = INTR_EOC;
        CDC_CHECK(probe.write(REG_INTR_ENABLE, &value, 4) == tlm::TLM_OK_RESPONSE);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(irq.read() == false);

        // START is ignored while ADC_EN is clear.
        value = CTRL_START;
        CDC_CHECK(probe.write(REG_CONTROL, &value, 4) == tlm::TLM_OK_RESPONSE);
        std::uint32_t status = 0xFFFFFFFFu;
        CDC_CHECK(probe.read(REG_STATUS, &status, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(status == 0u);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(irq.read() == false);

        // Enable ADC and start a conversion. The model completes immediately.
        value = CTRL_ADC_EN | CTRL_START;
        CDC_CHECK(probe.write(REG_CONTROL, &value, 4) == tlm::TLM_OK_RESPONSE);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(irq.read() == true);

        CDC_CHECK(probe.read(REG_STATUS, &status, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((status & STATUS_EOC) != 0u);

        std::uint32_t data = 0;
        CDC_CHECK(probe.read(REG_DATA, &data, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(data <= 0x0FFFu);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(irq.read() == false);

        // Bad accesses are rejected.
        std::uint8_t small = 0;
        CDC_CHECK(probe.write(REG_CONTROL, &small, 1) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CDC_CHECK(probe.read(0x01, &status, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CDC_CHECK(probe.read(0x10, &status, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        // Backdoor access has no read side effects.
        value = STATUS_EOC;
        CDC_CHECK(probe.debug(tlm::TLM_WRITE_COMMAND, REG_STATUS, &value, 4) == 4);
        data = 0;
        CDC_CHECK(probe.debug(tlm::TLM_READ_COMMAND, REG_DATA, &data, 4) == 4);
        status = 0;
        CDC_CHECK(probe.debug(tlm::TLM_READ_COMMAND, REG_STATUS, &status, 4) == 4);
        CDC_CHECK((status & STATUS_EOC) != 0u);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
