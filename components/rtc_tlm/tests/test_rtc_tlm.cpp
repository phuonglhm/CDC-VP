#include <cstdint>

#include <systemc>
#include <tlm>

#include "rtc_tlm.h"
#include "tlm_probe.h"

namespace {

constexpr std::uint32_t REG_DR = 0x00;
constexpr std::uint32_t REG_MR = 0x04;
constexpr std::uint32_t REG_LR = 0x08;
constexpr std::uint32_t REG_CR = 0x0C;
constexpr std::uint32_t REG_IMSC = 0x10;
constexpr std::uint32_t REG_RIS = 0x14;
constexpr std::uint32_t REG_MIS = 0x18;
constexpr std::uint32_t REG_ICR = 0x1C;

constexpr std::uint32_t CR_EN = 1u << 0;
constexpr std::uint32_t INT_ALARM = 1u << 0;

} // namespace

int sc_main(int, char*[])
{
    // 1 count == 1 ns of simulated time so the counter advances quickly.
    cdc::components::rtc_tlm rtc("rtc", sc_core::sc_time(1, sc_core::SC_NS));
    sc_core::sc_signal<bool> reset_n("reset_n");
    sc_core::sc_signal<bool> irq("irq");
    rtc.reset_n(reset_n);
    rtc.irq_out(irq);

    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(rtc.socket);

    sc_core::sc_spawn([&] {
        auto settle = [] {
            wait(sc_core::SC_ZERO_TIME);
            wait(sc_core::SC_ZERO_TIME);
        };

        reset_n.write(true);
        settle();

        std::uint32_t value = 0;
        std::uint32_t rd = 0;

        // Out of reset everything is clear.
        CDC_CHECK(probe.read(REG_DR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd == 0u);
        CDC_CHECK(probe.read(REG_CR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd == 0u);

        // Program alarm at count 3, leave interrupt masked, then enable count.
        value = 3u;
        CDC_CHECK(probe.write(REG_MR, &value, 4) == tlm::TLM_OK_RESPONSE);
        value = 0u; // IMSC masked
        CDC_CHECK(probe.write(REG_IMSC, &value, 4) == tlm::TLM_OK_RESPONSE);
        value = CR_EN;
        CDC_CHECK(probe.write(REG_CR, &value, 4) == tlm::TLM_OK_RESPONSE);

        // Let the counter run past the match value.
        wait(sc_core::sc_time(10, sc_core::SC_NS));
        settle();

        CDC_CHECK(probe.read(REG_DR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd >= 3u);
        // Raw alarm latched, but masked so no IRQ yet.
        CDC_CHECK(probe.read(REG_RIS, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((rd & INT_ALARM) != 0u);
        CDC_CHECK(probe.read(REG_MIS, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((rd & INT_ALARM) == 0u);
        CDC_CHECK(irq.read() == false);

        // Unmask -> IRQ asserts immediately.
        value = INT_ALARM;
        CDC_CHECK(probe.write(REG_IMSC, &value, 4) == tlm::TLM_OK_RESPONSE);
        settle();
        CDC_CHECK(probe.read(REG_MIS, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((rd & INT_ALARM) != 0u);
        CDC_CHECK(irq.read() == true);

        // W1C clears the alarm and deasserts the IRQ.
        value = INT_ALARM;
        CDC_CHECK(probe.write(REG_ICR, &value, 4) == tlm::TLM_OK_RESPONSE);
        settle();
        CDC_CHECK(probe.read(REG_RIS, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((rd & INT_ALARM) == 0u);
        CDC_CHECK(irq.read() == false);

        // RTCEN is write-once: writing 0 must NOT disable the counter.
        value = 0u;
        CDC_CHECK(probe.write(REG_CR, &value, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(probe.read(REG_CR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK((rd & CR_EN) != 0u); // still enabled
        std::uint32_t before = 0;
        CDC_CHECK(probe.read(REG_DR, &before, 4) == tlm::TLM_OK_RESPONSE);
        wait(sc_core::sc_time(5, sc_core::SC_NS));
        settle();
        CDC_CHECK(probe.read(REG_DR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd > before); // keeps counting despite the CR=0 write

        // Load register seeds the counter immediately; counting continues.
        value = 100u;
        CDC_CHECK(probe.write(REG_LR, &value, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(probe.read(REG_DR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd == 100u);
        // RTCLR read returns the loaded base, not the live counter.
        CDC_CHECK(probe.read(REG_LR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd == 100u);
        wait(sc_core::sc_time(4, sc_core::SC_NS));
        settle();
        CDC_CHECK(probe.read(REG_DR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd > 100u);

        // Bad accesses are rejected.
        std::uint8_t small = 0;
        CDC_CHECK(probe.write(REG_CR, &small, 1) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CDC_CHECK(probe.read(0x01, &rd, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CDC_CHECK(probe.read(0x20, &rd, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        // Backdoor access has no IRQ side effects.
        value = INT_ALARM;
        CDC_CHECK(probe.debug(tlm::TLM_WRITE_COMMAND, REG_RIS, &value, 4) == 4);
        rd = 0;
        CDC_CHECK(probe.debug(tlm::TLM_READ_COMMAND, REG_RIS, &rd, 4) == 4);
        CDC_CHECK((rd & INT_ALARM) != 0u);

        // Active-low reset clears registers and deasserts the IRQ.
        reset_n.write(false);
        settle();
        reset_n.write(true);
        settle();
        CDC_CHECK(probe.read(REG_DR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd == 0u);
        CDC_CHECK(probe.read(REG_CR, &rd, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rd == 0u);
        CDC_CHECK(irq.read() == false);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
