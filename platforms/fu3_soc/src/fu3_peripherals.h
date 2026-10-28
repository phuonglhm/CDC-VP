#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>

#include <fu3_plic/intmap.h>
#include <fu3_plic/plic.h>
#include <fu3/adc/adc.h>
#include <fu3/aes/aes_controller.h>
#include <fu3/dma/ap_dma.h>
#include <fu3/gpio/gpio.h>
#include <fu3/qspi/qspi_controller.h>
#include <fu3/qspi/qspi_flash_model.h>
#include <fu3_bus/dma_adapter.h>
#include <fu3_hyperram/controller.h>
#include <fu3_i2c/i2c.h>
#include <fu3_mailbox/mailbox.h>
#include <fu3_rng/rng.h>
#include <fu3_spi/spi.h>
#include <fu3_timer/timer.h>
#include <fu3_uart/uart.h>
#include <fu3_wdt/wdt.h>

#include "adc_macro.h"
#include "fu3_clock.h"
#include "fu3_pinmux.h"
#include "irq_ack_latch.h"
#include "uart_line.h"

namespace cdc::platforms::fu3_soc {

using target_socket = tlm::tlm_base_target_socket_b<32, tlm::tlm_fw_transport_if<>, tlm::tlm_bw_transport_if<>>;

// Reset and clock of one bus target as the platform derives them from the CRM:
// reset_n = system (or DBG) reset AND its SW_RST_N bit; clock_on = its
// CLK_GATE bit. Provided by fu3_soc_top for every gated target.
struct domain_signals {
    sc_core::sc_signal<bool>* reset_n = nullptr;
    sc_core::sc_signal<bool>* clock_on = nullptr;
};
using domain_lookup = std::function<domain_signals(const std::string& target)>;

// The FU3 peripheral models integrated on the platform (T08-T18), their
// interrupt network (PLIC for CVA6, INTMAP for Ibex, Common v2.1 Interrupt
// Mapping) and the board-side tie-offs of their pins. fu3_soc_top binds each
// returned socket behind the target's gate and forwards the CPU interrupt
// levels to the CPU models.
class fu3_peripherals : public sc_core::sc_module {
public:
    fu3_peripherals(sc_core::sc_module_name name, const domain_lookup& domains);

    // Socket of the model serving `target`, or nullptr (not integrated here).
    target_socket* socket(const std::string& target);
    // A register with a known reset value, for the platform self-test.
    struct probe {
        std::uint32_t offset = 0, value = 0;
    };
    const std::map<std::string, probe>& probes() const { return probes_; }

    // Bus clock period from the CRM (zero: keep the last one).
    void set_bus_clock(sc_core::sc_time period);

    // Interrupt nets. plic_source[k] is Common v2.1 irq_sources_i[k] (PLIC ID
    // k + 1); the INTMAP reads the same net for a shared source. Ibex-only
    // sources (no PLIC ID): WDT0 warning, MAILBOX_IBEX, Ibex software IRQ.
    sc_core::sc_vector<sc_core::sc_signal<bool>> plic_source{"plic_source", fu3_plic::kSources};
    sc_core::sc_signal<bool> wdt0_warning{"wdt0_warning"}, mailbox_ibex_irq{"mailbox_ibex_irq"},
        ibex_software{"ibex_software"};
    // CPU interrupt levels: CVA6 irq_i[0] (cause 11), time_irq_i (7), ipi_i (3);
    // Ibex inputs in fu3_plic::IntmapOutput order.
    sc_core::sc_signal<bool> cva6_external{"cva6_external"}, cva6_timer{"cva6_timer"}, cva6_software{"cva6_software"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> ibex_irq{"ibex_irq", fu3_plic::kIntmapOutputs};

    // OR of the WDT0/WDT1 reset requests, to the CRM watchdog input (T09).
    sc_core::sc_signal<bool> wdt_reset_request{"wdt_reset_request"};

    fu3_plic::Plic& plic() { return plic_; }
    fu3_plic::Intmap& intmap() { return intmap_; }
    fu3_timer::Timer& timer(unsigned i) { return *timers_[i]; }
    fu3_wdt::Watchdog& wdt(unsigned i) { return *wdts_[i]; }
    static constexpr unsigned kUarts = 8;
    fu3_uart::Uart& uart(unsigned i) { return *uarts_[i]; }
    // Board-side peer of UARTi (console), wired to its TXD_i/RXD_i die pads
    // (kUartPad). UART5's pads are the I2C pads (pull-ups): its peer is
    // detached at reset; attach_uart_peer() connects or detaches a peer's
    // RXD drive (another function using that pad).
    uart_line& uart_peer(unsigned i) { return *uart_lines_[i]; }
    void attach_uart_peer(unsigned i, bool on);
    struct uart_pads {
        unsigned txd, rxd, select;   // die pads and the csr_io_cfg value selecting TXD/RXD
    };
    static constexpr uart_pads kUartPad[8] = {{48, 49, 0}, {59, 60, 0}, {56, 57, 2}, {62, 63, 2},
                                              {28, 29, 1}, {22, 23, 3}, {24, 27, 2}, {30, 31, 2}};
    static constexpr unsigned kSpis = 4;
    fu3_spi::Spi& spi(unsigned i) { return *spis_[i]; }
    // Clock periods of the generators that follow the bus clock (QSPI, AES).
    sc_core::sc_time qspi_clock_period() const { return qspi_clock_.period(); }
    sc_core::sc_time aes_clock_period() const { return aes_clock_.period(); }
    fu3_i2c::I2c& i2c() { return *i2c_; }
    // Board: a wire from SPIi's MOSI die pad to its MISO die pad (kSpiPad).
    void set_spi_loopback(unsigned i, bool on);
    struct spi_pads {
        unsigned sck, miso, mosi, ssn, select;   // die pads and the csr_io_cfg value selecting them
    };
    static constexpr spi_pads kSpiPad[kSpis] = {
        {56, 57, 58, 61, 1}, {59, 60, 62, 63, 1}, {14, 15, 16, 17, 2}, {30, 31, 32, 46, 3}};
    // SPI DMA handshake (T17: SPI0/1 on DMA0 channels 0/1 and 6/7); acks low until then.
    sc_core::sc_vector<sc_core::sc_signal<bool>> spi_dma_tx_request{"spi_dma_tx_request", kSpis},
        spi_dma_rx_request{"spi_dma_rx_request", kSpis}, spi_dma_tx_ack{"spi_dma_tx_ack", kSpis},
        spi_dma_rx_ack{"spi_dma_rx_ack", kSpis};
    // I2C bus lines at their die pads (SDA_0 22, SCL_0 23; board pull-ups,
    // open drain): csr_io_cfg = b10 selects I2C there.
    static constexpr unsigned kI2cSdaPad = 22, kI2cSclPad = 23, kI2cSelect = 2;
    bool i2c_scl() const { return pinmux_.level(kI2cSclPad); }
    bool i2c_sda() const { return pinmux_.level(kI2cSdaPad); }

    // T12: GPIOA/B/C on the die pads of Common v2.1 IOPAD MAP (fu3_pinmux).
    fu3::gpio::Gpio& gpio(unsigned bank) { return *gpios_[bank]; }
    fu3_pinmux& pinmux() { return pinmux_; }
    static constexpr unsigned kPads = fu3_pinmux::kDiePads;
    // Board level driven into a die pad while the chip does not drive it.
    void set_board_pad(unsigned die, bool level) { pinmux_.board_level(die, level); }
    void release_board_pad(unsigned die) { pinmux_.board_release(die); }
    // Level of a die pad as the board sees it.
    bool pad_level(unsigned die) const { return pinmux_.level(die); }
    // csr_io_cfg[1:0] of a die pad. SYSCSR csr_io_cfg is not connected
    // (decision Q06-b: field -> die mapping pending); reset b00.
    void set_pad_select(unsigned die, unsigned select) { pinmux_.set_select(die, select); }
    // Straps and test inputs from their fixed die pads (2 BOOT, 3 MODE_1,
    // 4 MODE_0, 5 TESTMODE, 13 TEST_MODE_SEL), to SYSCSR. DEBUG_EN (die 6)
    // has no consumer yet (decision Q05-a: no FW-visible path).
    sc_core::sc_signal<bool> strap_boot{"strap_boot"}, strap_mode_0{"strap_mode_0"}, strap_mode_1{"strap_mode_1"},
        test_mode{"test_mode"}, test_select{"test_select"}, debug_en{"debug_en"};

    // T13: QSPI controller (AP_QSPIC + QSPI_XIP) and its board NOR flash.
    fu3::qspi::QspiController& qspi() { return *qspi_; }
    fu3::qspi::QspiFlashModel& flash() { return *flash_; }
    static constexpr std::size_t kFlashBytes = 16u << 20;   // 16 MB NOR (HAS v2.1)

    // T14: HyperRAM controller (HYPERBUS data window + AP_HBCTRL CSR).
    fu3_hyperram::Controller& hyperram() { return *hyperram_; }

    // T15: AES-256, ADC (+ its hardmacro), RNG.
    fu3::aes::AesController& aes() { return *aes_; }
    fu3::adc::Adc& adc() { return *adc_; }
    adc_macro& adc_input() { return *adc_macro_; }
    fu3_rng::Rng& rng() { return *rng_; }
    // Seed of the RNG bit source (decision Q07-h: reproducible, not entropy).
    void set_rng_seed(std::uint64_t seed) { rng_seed_ = seed; }
    std::uint64_t rng_seed() const { return rng_seed_; }
    static constexpr std::uint64_t kDefaultRngSeed = 0x46553352'4e470001ull;

    // T16: inter-CPU mailbox (port 0 CVA6 at MAILBOX_CVA6, port 1 Ibex at MAILBOX_IBEX).
    fu3_mailbox::Mailbox& mailbox() { return *mailbox_; }

    // T17: DMA0 (AP_DMA registers; its master goes through the bus DmaAdapter
    // to the DMA0 initiator port, which fu3_soc_top binds).
    fu3::dma::ApDma& dma() { return *dma_; }
    tlm_utils::simple_initiator_socket<fu3_bus::DmaAdapter>& dma_master() { return dma_adapter_.output; }
    // VP DMA request selector (Q02-c): peripheral ID of each endpoint.
    static constexpr unsigned kDmaIdSpi0 = 1, kDmaIdUart1 = 2, kDmaIdUart2 = 3, kDmaIdSpi1 = 4;

    // UART DMA handshake (T17: UART1/2 on DMA0 channels 2-5); clear inputs low until then.
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_dma_tx_request{"uart_dma_tx_request", kUarts},
        uart_dma_rx_request{"uart_dma_rx_request", kUarts}, uart_dma_tx_clear{"uart_dma_tx_clear", kUarts},
        uart_dma_rx_clear{"uart_dma_rx_clear", kUarts};

private:
    SC_HAS_PROCESS(fu3_peripherals);
    void or_wdt_requests();
    // CRM clock gates of the models with an internal clock enable (decision
    // Q02-i and HAS 6.1: a gated IP stops, not only its MMIO).
    struct clock_gate {
        const sc_core::sc_signal<bool>* clock_on;
        std::function<void(bool)> apply;
    };
    std::vector<clock_gate> clock_gates_;
    void apply_clock_gates();
    void tie_offs();
    void bind_pads();
    void publish_gpio();
    void dma_requests();
    void dma_clears();

    std::map<std::string, target_socket*> sockets_;
    std::map<std::string, probe> probes_;
    fu3_plic::Plic plic_{"plic"};
    fu3_plic::Intmap intmap_{"intmap"};

    // T09: TIMER0-3 behind their pending latches, WDT0/1.
    std::unique_ptr<fu3_timer::Timer> timers_[4];
    std::unique_ptr<irq_ack_latch> timer_latch_[4];
    sc_core::sc_vector<sc_core::sc_signal<bool>> timer_raw_{"timer_raw", 4}, timer_overflow_{"timer_overflow", 4},
        timer_compare_{"timer_compare", 4};
    std::unique_ptr<fu3_wdt::Watchdog> wdts_[2];
    sc_core::sc_vector<sc_core::sc_signal<bool>> wdt_reset_{"wdt_reset", 2};

    // T10: UART0-7 and their board-side lines; modem inputs tied per Q07-f.
    std::unique_ptr<fu3_uart::Uart> uarts_[kUarts];
    std::unique_ptr<uart_line> uart_lines_[kUarts];
    sc_core::sc_vector<sc_core::sc_signal<bool>> peer_txd_{"peer_txd", kUarts}, peer_rxd_{"peer_rxd", kUarts};
    sc_core::sc_vector<sc_core::sc_signal<bool>> uart_txd_{"uart_txd", kUarts}, uart_rxd_{"uart_rxd", kUarts},
        uart_rts_n_{"uart_rts_n", kUarts}, uart_dtr_n_{"uart_dtr_n", kUarts}, uart_out1_n_{"uart_out1_n", kUarts},
        uart_out2_n_{"uart_out2_n", kUarts}, uart_cts_n_{"uart_cts_n", kUarts}, uart_modem_high_{"uart_modem_high", kUarts};

    // T11: SPI0-3 (decision Q07-g: ss_n[0] is the bonded chip select, [3:1]
    // stay internal; slave inputs idle inactive) and I2C (Q06-e: open-drain
    // lines resolved on the board, wired-AND with pull-up; no target device).
    std::unique_ptr<fu3_spi::Spi> spis_[kSpis];
    sc_core::sc_vector<sc_core::sc_signal<bool>> spi_sck_{"spi_sck", kSpis}, spi_mosi_{"spi_mosi", kSpis},
        spi_miso_{"spi_miso", kSpis}, spi_sck_oe_{"spi_sck_oe", kSpis}, spi_ss_oe_{"spi_ss_oe", kSpis},
        spi_mosi_oe_{"spi_mosi_oe", kSpis}, spi_miso_oe_{"spi_miso_oe", kSpis}, spi_sck_in_{"spi_sck_in", kSpis},
        spi_ss_n_in_{"spi_ss_n_in", kSpis}, spi_mosi_in_{"spi_mosi_in", kSpis}, spi_miso_in_{"spi_miso_in", kSpis};
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<4>>> spi_ss_n_{"spi_ss_n", kSpis};
    std::unique_ptr<fu3_i2c::I2c> i2c_;
    sc_core::sc_signal<bool> i2c_scl_oe_n_{"i2c_scl_oe_n"}, i2c_sda_oe_n_{"i2c_sda_oe_n"}, i2c_scl_{"i2c_scl"},
        i2c_sda_{"i2c_sda"};   // I2C inputs from the pads

    // T12: three GPIO banks with their own CRM clock and reset (the GPIO
    // component's Subsystem shares one clock/reset and the 80-position IO_PAD
    // of its HDS; the platform uses the Common v2.1 die pads instead).
    std::unique_ptr<fu3::gpio::Gpio> gpios_[3];
    std::unique_ptr<fu3_clock> gpio_clock_[3];
    sc_core::sc_vector<sc_core::sc_signal<bool>> gpio_clk_{"gpio_clk", 3},
        gpio_wake_{"gpio_wake", 3};
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<32>>> gpio_in_{"gpio_in", 3}, gpio_out_{"gpio_out", 3},
        gpio_dir_{"gpio_dir", 3}, gpio_sync_{"gpio_sync", 3};
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<6>>> gpio_padcfg_{"gpio_padcfg", 96};
    std::uint32_t gpio_in_bits_[3] = {};
    fu3_pinmux pinmux_{"pinmux"};

    // T13: one clock for the QSPI core, APB, AHB (XIP) and DMA clock inputs
    // (all from the CRM bus clock, gate qspi); its DMA requests stay
    // unconnected (plan T13: DMA0 has no QSPI channel allocation).
    std::unique_ptr<fu3::qspi::QspiController> qspi_;
    std::unique_ptr<fu3::qspi::QspiFlashModel> flash_;
    fu3_clock qspi_clock_{"QSPI_clock"};
    // T14: HyperRAM; no interrupt is allocated to AP_HBCTRL (decision Q03-g):
    // its output ends on an unrouted net. HB_RESETN follows the HYPERBUS reset.
    std::unique_ptr<fu3_hyperram::Controller> hyperram_;

    // T15: AES and ADC have no interrupt output (decision Q03-f: tying ID3/ID17
    // low is not accepted as final; their assert/clear truth table is open).
    // The ADC clock is a fixed 10 MHz (controller contract), gated by the CRM.
    std::unique_ptr<fu3::aes::AesController> aes_;
    fu3_clock aes_clock_{"AES_clock"}, adc_clock_{"ADC_clock"};
    std::unique_ptr<fu3::adc::Adc> adc_;
    std::unique_ptr<adc_macro> adc_macro_;
    std::unique_ptr<fu3_rng::Rng> rng_;
    std::uint64_t rng_seed_ = kDefaultRngSeed;
    sc_core::sc_signal<bool> aes_clk_{"aes_clk"}, adc_clk_{"adc_clk"}, adc_eoc_{"adc_eoc"}, adc_sampclk_{"adc_sampclk"},
        adc_cs_{"adc_cs"}, adc_pwd_{"adc_pwd"}, adc_macro_reset_{"adc_macro_reset"}, adc_samp_sel_{"adc_samp_sel"};
    sc_core::sc_signal<sc_dt::sc_uint<12>> adc_data_{"adc_data"};
    sc_core::sc_signal<sc_dt::sc_uint<4>> adc_channel_{"adc_channel"};

    // T16: MAILBOX_IBEX IRQ -> Ibex irq_external_i (decision Q03-g, through
    // the INTMAP); MAILBOX_CVA6 gets no PLIC ID: its IRQ ends unrouted.
    std::unique_ptr<fu3_mailbox::Mailbox> mailbox_;

    // T17: DMA0 and its request/clear bitmaps (bit = VP peripheral ID).
    std::unique_ptr<fu3::dma::ApDma> dma_;
    fu3_bus::DmaAdapter dma_adapter_{"DMA0_adapter"};
    sc_core::sc_signal<sc_dt::sc_uint<32>> dma_rx_request_{"dma_rx_request"}, dma_tx_request_{"dma_tx_request"},
        dma_rx_clear_{"dma_rx_clear"}, dma_tx_clear_{"dma_tx_clear"};
    sc_core::sc_signal<bool> mailbox_cva6_irq_{"mailbox_cva6_irq_unrouted"};
    sc_core::sc_signal<bool> hbctrl_irq_{"hbctrl_irq_unrouted"};

    sc_core::sc_signal<bool> qspi_clk_{"qspi_clk"}, qspi_dma_rx_req_{"qspi_dma_rx_req"},
        qspi_dma_tx_req_{"qspi_dma_tx_req"}, qspi_dma_clear_{"qspi_dma_clear"}, flash_power_{"flash_power"};
};

} // namespace cdc::platforms::fu3_soc
