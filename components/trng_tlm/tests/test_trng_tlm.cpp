#include <cstdint>

#include <systemc>
#include <tlm>

#include "trng_tlm.h"
#include "tlm_probe.h"

namespace {

constexpr std::uint32_t REG_IMR = 0x100;
constexpr std::uint32_t REG_ISR = 0x104;
constexpr std::uint32_t REG_ICR = 0x108;
constexpr std::uint32_t REG_CONFIG = 0x10C;
constexpr std::uint32_t REG_VALID = 0x110;
constexpr std::uint32_t REG_EHR_DATA0 = 0x114;
constexpr std::uint32_t REG_EHR_DATA1 = 0x118;
constexpr std::uint32_t REG_EHR_DATA2 = 0x11C;
constexpr std::uint32_t REG_EHR_DATA3 = 0x120;
constexpr std::uint32_t REG_EHR_DATA4 = 0x124;
constexpr std::uint32_t REG_EHR_DATA5 = 0x128;
constexpr std::uint32_t REG_SRC_EN = 0x12C;
constexpr std::uint32_t REG_SAMPLE_CNT1 = 0x130;
constexpr std::uint32_t REG_AUTOCORR_STAT = 0x134;
constexpr std::uint32_t REG_DBG_CONTROL = 0x138;
constexpr std::uint32_t REG_SW_RESET = 0x140;
constexpr std::uint32_t REG_BUSY = 0x1B8;
constexpr std::uint32_t REG_RESET_BITS_COUNTER = 0x1BC;
constexpr std::uint32_t REG_BIST_CNTR0 = 0x1E0;
constexpr std::uint32_t REG_BIST_CNTR1 = 0x1E4;
constexpr std::uint32_t REG_BIST_CNTR2 = 0x1E8;

} // namespace

int sc_main(int, char *[]) {
   cdc::components::trng_tlm trng("trng");
   sc_core::sc_clock clk("clk", 10, sc_core::SC_NS);
   sc_core::sc_signal<bool> reset_n("reset_n");
   sc_core::sc_signal<bool> irq("irq");
   trng.clk(clk);
   trng.reset_n(reset_n);
   trng.irq_out(irq);

   cdc::test::tlm_probe probe("probe");
   probe.socket.bind(trng.socket);

   sc_core::sc_spawn([&] {
      auto settle_irq = [] {
         wait(sc_core::SC_ZERO_TIME);
         wait(sc_core::SC_ZERO_TIME);
      };

      // Elaboration & Reset Check
      reset_n.write(false);
      settle_irq();
      reset_n.write(true);
      settle_irq();

      std::uint32_t val = 0;
      CDC_CHECK(probe.read(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0xFu); // RNG_IMR default value is 0x0000000F

      CDC_CHECK(probe.read(REG_SAMPLE_CNT1, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0xFFFFu); // SAMPLE_CNT1 default value is 0x0000FFFF

      // Ensure other registers default to 0
      CDC_CHECK(probe.read(REG_ISR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_CONFIG, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_VALID, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_SRC_EN, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_AUTOCORR_STAT, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_BUSY, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);

      // RNG_IMR read/write check
      val = 0x5u;
      CDC_CHECK(probe.write(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(probe.read(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0x5u);

      // Entropy Generation Flow
      val = 1u; // Enable RNG source (RND_SOURCE_ENABLE.RND_SRC_EN = 1)
      CDC_CHECK(probe.write(REG_SRC_EN, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();

      // Valid should be asserted
      CDC_CHECK(probe.read(REG_VALID, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 1u);

      // EHR_VALID bit in ISR should be set to 1
      CDC_CHECK(probe.read(REG_ISR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK((val & 1u) == 1u);

      // Verify EHR_DATA0-5 contain non-zero generated entropy
      for (std::uint32_t offset = REG_EHR_DATA0; offset <= REG_EHR_DATA5; offset += 4) {
         std::uint32_t data = 0;
         CDC_CHECK(probe.read(offset, &data, 4) == tlm::TLM_OK_RESPONSE);
      }

      // Interrupt line assertion verification
      // Unmask the EHR_VALID interrupt (bit 0 = 0)
      val = 0u;
      CDC_CHECK(probe.write(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();
      CDC_CHECK(irq.read() == true); // Interrupt should be active

      // Clear the interrupt using RNG_ICR
      val = 1u; // Write 1 to clear EHR_VALID interrupt status
      CDC_CHECK(probe.write(REG_ICR, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();

      // Verify interrupt is deasserted and valid/ISR flags are set to 0
      CDC_CHECK(irq.read() == false);
      CDC_CHECK(probe.read(REG_VALID, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_ISR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK((val & 1u) == 0u);

      // Software Reset (TRNG_SW_RESET) & Sticky Register check
      // Write configurations to modify states
      val = 3u;
      CDC_CHECK(probe.write(REG_CONFIG, &val, 4) == tlm::TLM_OK_RESPONSE);
      val = 1u;
      CDC_CHECK(probe.write(REG_SRC_EN, &val, 4) == tlm::TLM_OK_RESPONSE);
      val = 0xAu;
      CDC_CHECK(probe.write(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      val = 0xAA55u;
      CDC_CHECK(probe.write(REG_AUTOCORR_STAT, &val, 4) == tlm::TLM_OK_RESPONSE);

      // Perform Software Reset
      val = 1u;
      CDC_CHECK(probe.write(REG_SW_RESET, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();

      // Verify all registers reset to default values after software reset
      CDC_CHECK(probe.read(REG_CONFIG, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_SRC_EN, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_VALID, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);

      CDC_CHECK(probe.read(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0xFu); // Resets to default 0xF
      CDC_CHECK(probe.read(REG_AUTOCORR_STAT, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u); // Resets to default 0

      // Reset Bits Counter (RST_BITS_COUNTER) check
      // Re-generate entropy first
      val = 1u;
      CDC_CHECK(probe.write(REG_SRC_EN, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();
      CDC_CHECK(probe.read(REG_VALID, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 1u);

      // Disable entropy source
      val = 0u;
      CDC_CHECK(probe.write(REG_SRC_EN, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();

      // Write to RST_BITS_COUNTER
      val = 1u;
      CDC_CHECK(probe.write(REG_RESET_BITS_COUNTER, &val, 4) == tlm::TLM_OK_RESPONSE);
      settle_irq();

      // Verify valid register and ISR bit 0 are cleared
      CDC_CHECK(probe.read(REG_VALID, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0u);
      CDC_CHECK(probe.read(REG_ISR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK((val & 1u) == 0u);

      // Debug Access (backdoor access)
      val = 0xCu;
      CDC_CHECK(probe.debug(tlm::TLM_WRITE_COMMAND, REG_IMR, &val, 4) == 4);
      val = 0u;
      CDC_CHECK(probe.debug(tlm::TLM_READ_COMMAND, REG_IMR, &val, 4) == 4);
      CDC_CHECK(val == 0xCu);

      // Reject bad access size
      std::uint8_t byte_val = 0;
      CDC_CHECK(probe.write(REG_CONFIG, &byte_val, 1) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

      // Reject misaligned address
      CDC_CHECK(probe.read(REG_CONFIG + 1, &val, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

      // Reject out-of-bounds address
      CDC_CHECK(probe.read(0x200, &val, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

      // Hardware reset again
      reset_n.write(false);
      settle_irq();
      reset_n.write(true);
      settle_irq();
      CDC_CHECK(probe.read(REG_IMR, &val, 4) == tlm::TLM_OK_RESPONSE);
      CDC_CHECK(val == 0xFu); // Verifies hard reset clears the sticky registers too

      sc_core::sc_stop();
   });

   sc_core::sc_start();
   if (cdc::test::failures() == 0) {
      std::cout << "TRNG TEST PASS" << std::endl;
      return 0;
   }
   std::cout << "TRNG TEST FAILED" << std::endl;
   return 1;
}
