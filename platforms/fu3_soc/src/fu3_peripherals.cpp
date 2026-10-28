#include "fu3_peripherals.h"

#include <fu3_plic/registers.h>
#include <fu3/qspi/registers.h>
#include <fu3_hyperram/registers.h>
#include <fu3_mailbox/registers.h>
#include <fu3_spi/registers.h>
#include <fu3_timer/registers.h>
#include <fu3_uart/registers.h>
#include <fu3_wdt/registers.h>

namespace cdc::platforms::fu3_soc {

namespace {

using namespace fu3_plic;

// PLIC ID of the net each INTMAP source reads (Common v2.1: src[n] = ID n + 1);
// 0: an Ibex-only source without PLIC ID.
constexpr unsigned kIntmapPlicId[kIntmapSources] = {
    0,                                                      // WDT0 warning
    FU3_PLIC_ID_CRM, FU3_PLIC_ID_GPIO0_WAKE, FU3_PLIC_ID_EDHOC_OSCORE, FU3_PLIC_ID_AES, FU3_PLIC_ID_RNG,
    FU3_PLIC_ID_DMA0, FU3_PLIC_ID_QSPI,
    FU3_PLIC_ID_UART0, FU3_PLIC_ID_UART1, FU3_PLIC_ID_UART2, FU3_PLIC_ID_UART3, FU3_PLIC_ID_UART4,
    FU3_PLIC_ID_UART5, FU3_PLIC_ID_UART6, FU3_PLIC_ID_UART7,
    FU3_PLIC_ID_SPI0, FU3_PLIC_ID_SPI1, FU3_PLIC_ID_SPI2, FU3_PLIC_ID_SPI3,
    FU3_PLIC_ID_CAN,
    FU3_PLIC_ID_GPIOA, FU3_PLIC_ID_GPIOB, FU3_PLIC_ID_GPIOC,
    FU3_PLIC_ID_I2C0,
    FU3_PLIC_ID_ADC,
    FU3_PLIC_ID_PWM0, FU3_PLIC_ID_PWM1,
    FU3_PLIC_ID_TIMER2, FU3_PLIC_ID_TIMER3,
    FU3_PLIC_ID_WDT1,
    0,                                                      // MAILBOX_IBEX
    0,                                                      // Ibex software IRQ
    FU3_PLIC_ID_TIMER1,
};

} // namespace

fu3_peripherals::fu3_peripherals(sc_core::sc_module_name name, const domain_lookup& domains)
    : sc_core::sc_module(name)
{
    // T08: PLIC (CVA6 external interrupt) and INTMAP (Ibex inputs).
    plic_.reset_n(*domains("AP_PLIC").reset_n);
    for (unsigned k = 0; k < fu3_plic::kSources; ++k)
        plic_.irq_sources[k](plic_source[k]);
    plic_.irq(cva6_external);
    sockets_["AP_PLIC"] = &plic_.socket;
    probes_["AP_PLIC"] = {FU3_PLIC_ENABLE_OFFSET, 0};
    for (unsigned s = 0; s < fu3_plic::kIntmapSources; ++s) {
        if (const unsigned id = kIntmapPlicId[s])
            intmap_.sources[s](plic_source[id - 1]);
    }
    intmap_.sources[fu3_plic::kSrcWdt0Warning](wdt0_warning);
    intmap_.sources[fu3_plic::kSrcMailboxIbex](mailbox_ibex_irq);
    intmap_.sources[fu3_plic::kSrcSoftwareIbex](ibex_software);   // FU3_VP_ABI_1 SWIRQ bit 1
    for (unsigned o = 0; o < fu3_plic::kIntmapOutputs; ++o)
        intmap_.irq[o](ibex_irq[o]);

    // T09: TIMER0-3. A timer tick raises IRQ for one tick only, so a latch
    // holds it until software writes CMP (decision Q03-d). TIMER0 -> CVA6
    // time_irq_i; TIMER1 -> PLIC ID6 and Ibex irq_timer_i; TIMER2/3 -> PLIC
    // ID7/8 and Ibex irq_external_i (Common v2.1 Interrupt Mapping).
    sc_core::sc_signal<bool>* timer_irq[4] = {&cva6_timer, &plic_source[FU3_PLIC_ID_TIMER1 - 1],
                                              &plic_source[FU3_PLIC_ID_TIMER2 - 1], &plic_source[FU3_PLIC_ID_TIMER3 - 1]};
    for (unsigned i = 0; i < 4; ++i) {
        const std::string target = "TIMER" + std::to_string(i);
        const auto d = domains(target);
        timers_[i] = std::make_unique<fu3_timer::Timer>(target.c_str(), sc_core::sc_time(50, sc_core::SC_NS));
        timers_[i]->reset_n(*d.reset_n);
        timers_[i]->clock_enabled(*d.clock_on);
        timers_[i]->irq(timer_raw_[i]);
        timers_[i]->overflow_irq(timer_overflow_[i]);
        timers_[i]->compare_irq(timer_compare_[i]);
        timer_latch_[i] = std::make_unique<irq_ack_latch>((target + "_latch").c_str(), 0x8 /* CMP */);
        timer_latch_[i]->raw(timer_raw_[i]);
        timer_latch_[i]->reset_n(*d.reset_n);
        timer_latch_[i]->irq(*timer_irq[i]);
        timer_latch_[i]->out.bind(timers_[i]->socket);
        sockets_[target] = &timer_latch_[i]->socket;
        probes_[target] = {0x8, 0};   // CMP, reset 0
    }

    // WDT0 -> Ibex irq_fast_i[0] (warning); WDT1 -> PLIC ID18 and Ibex
    // irq_external_i. Both reset requests -> CRM (system reset incl. DBG, Q05-e).
    sc_core::sc_signal<bool>* wdt_irq[2] = {&wdt0_warning, &plic_source[FU3_PLIC_ID_WDT1 - 1]};
    for (unsigned i = 0; i < 2; ++i) {
        const std::string target = "WDT" + std::to_string(i);
        const auto d = domains(target);
        wdts_[i] = std::make_unique<fu3_wdt::Watchdog>(target.c_str(), sc_core::sc_time(50, sc_core::SC_NS), 10,
                                                       sc_core::sc_time(50, sc_core::SC_NS));
        wdts_[i]->preset_n(*d.reset_n);
        wdts_[i]->wdogres_n(*d.reset_n);
        wdts_[i]->irq(*wdt_irq[i]);
        wdts_[i]->reset_request(wdt_reset_[i]);
        sockets_[target] = &wdts_[i]->socket;
        probes_[target] = {FU3_WDT_CNT_LD, FU3_WDT_RESET_VALUE};
    }
    SC_METHOD(or_wdt_requests);
    sensitive << wdt_reset_[0] << wdt_reset_[1];

    // T10: UART0-7 (Common v2.1 names, decision Q02-c: UART2 is the new
    // instance at 0x4409_0000, UART4 the one at 0x5005_0000). PLIC IDs 2, 10,
    // 11, 24-28; Ibex irq_fast_i[8] for all. Modem inputs (Q07-f): CTS_n low
    // (peer always ready), DSR_n/RI_n/DCD_n inactive high; RXD idles high.
    const unsigned uart_id[kUarts] = {FU3_PLIC_ID_UART0, FU3_PLIC_ID_UART1, FU3_PLIC_ID_UART2, FU3_PLIC_ID_UART3,
                                      FU3_PLIC_ID_UART4, FU3_PLIC_ID_UART5, FU3_PLIC_ID_UART6, FU3_PLIC_ID_UART7};
    for (unsigned i = 0; i < kUarts; ++i) {
        const std::string target = "UART" + std::to_string(i);
        fu3_uart::Config config;
        config.clock_period = sc_core::sc_time(50, sc_core::SC_NS);
        uarts_[i] = std::make_unique<fu3_uart::Uart>(target.c_str(), config);
        auto& u = *uarts_[i];
        u.reset_n(*domains(target).reset_n);
        u.rxd(uart_rxd_[i]);
        u.cts_n(uart_cts_n_[i]);
        u.dsr_n(uart_modem_high_[i]);
        u.ri_n(uart_modem_high_[i]);
        u.dcd_n(uart_modem_high_[i]);
        u.dma_tx_clear(uart_dma_tx_clear[i]);
        u.dma_rx_clear(uart_dma_rx_clear[i]);
        u.txd(uart_txd_[i]);
        u.rts_n(uart_rts_n_[i]);
        u.dtr_n(uart_dtr_n_[i]);
        u.out1_n(uart_out1_n_[i]);
        u.out2_n(uart_out2_n_[i]);
        u.irq(plic_source[uart_id[i] - 1]);
        u.dma_tx_request(uart_dma_tx_request[i]);
        u.dma_rx_request(uart_dma_rx_request[i]);
        uart_lines_[i] = std::make_unique<uart_line>((target + "_line").c_str(), u);
        uart_lines_[i]->txd(peer_txd_[i]);   // the board peer is wired to die pads (bind_pads)
        uart_lines_[i]->rxd(peer_rxd_[i]);
        sockets_[target] = &u.socket;
        probes_[target] = {FU3_UART_SCR, 0};
    }
    SC_METHOD(tie_offs);

    // T11: SPI0-3: PLIC IDs 9, 16, 22, 23; Ibex irq_fast_i[9].
    const unsigned spi_id[kSpis] = {FU3_PLIC_ID_SPI0, FU3_PLIC_ID_SPI1, FU3_PLIC_ID_SPI2, FU3_PLIC_ID_SPI3};
    for (unsigned i = 0; i < kSpis; ++i) {
        const std::string target = "SPI" + std::to_string(i);
        spis_[i] = std::make_unique<fu3_spi::Spi>(target.c_str(), 16, sc_core::sc_time(50, sc_core::SC_NS),
                                                  sc_core::sc_time(50, sc_core::SC_NS));
        auto& s = *spis_[i];
        s.reset_n(*domains(target).reset_n);
        s.apb_reset_n(*domains(target).reset_n);
        s.sck_in(spi_sck_in_[i]);
        s.ss_n_in(spi_ss_n_in_[i]);
        s.mosi_in(spi_mosi_in_[i]);
        s.miso_in(spi_miso_in_[i]);
        s.dma_tx_ack(spi_dma_tx_ack[i]);
        s.dma_rx_ack(spi_dma_rx_ack[i]);
        s.sck(spi_sck_[i]);
        s.mosi(spi_mosi_[i]);
        s.miso(spi_miso_[i]);
        s.ss_n(spi_ss_n_[i]);
        s.sck_oe(spi_sck_oe_[i]);
        s.ss_oe(spi_ss_oe_[i]);
        s.mosi_oe(spi_mosi_oe_[i]);
        s.miso_oe(spi_miso_oe_[i]);
        s.irq(plic_source[spi_id[i] - 1]);
        s.dma_tx_req(spi_dma_tx_request[i]);
        s.dma_rx_req(spi_dma_rx_request[i]);
        sockets_[target] = &s.socket;
        probes_[target] = {FU3_SPI_CTRL, 0xe0};   // FSZ = 7 (8-bit frames) at reset
    }

    // I2C: PLIC ID15, Ibex irq_fast_i[12].
    fu3_i2c::Config i2c_config;
    i2c_config.clock_period = sc_core::sc_time(50, sc_core::SC_NS);
    i2c_ = std::make_unique<fu3_i2c::I2c>("I2C", i2c_config);
    i2c_->reset_n(*domains("I2C").reset_n);
    i2c_->scl_i(i2c_scl_);
    i2c_->sda_i(i2c_sda_);
    i2c_->scl_oe_n(i2c_scl_oe_n_);
    i2c_->sda_oe_n(i2c_sda_oe_n_);
    i2c_->irq(plic_source[FU3_PLIC_ID_I2C0 - 1]);
    sockets_["I2C"] = &i2c_->socket;
    probes_["I2C"] = {fu3_i2c::reg::PRESCALER, 0};

    // T12: GPIOA/B/C (PLIC IDs 12-14, Ibex irq_fast_i[11]); GPIO0_WAKE (PLIC
    // ID19, Ibex irq_fast_i[2]) is the OR of the banks' power events for now
    // (decision Q06-c: not accepted as final, a latched/clearable WAKE is open).
    // Their pins reach the Common v2.1 die pads through fu3_pinmux (bind_pads).
    const char* bank_target[3] = {"GPIOA", "GPIOB", "GPIOC"};
    const unsigned gpio_id[3] = {FU3_PLIC_ID_GPIOA, FU3_PLIC_ID_GPIOB, FU3_PLIC_ID_GPIOC};
    for (unsigned b = 0; b < 3; ++b) {
        const auto d = domains(bank_target[b]);
        gpio_clock_[b] = std::make_unique<fu3_clock>((std::string(bank_target[b]) + "_clock").c_str());
        gpio_clock_[b]->clk(gpio_clk_[b]);
        gpio_clock_[b]->enable(*d.clock_on);
        gpios_[b] = std::make_unique<fu3::gpio::Gpio>(bank_target[b]);
        auto& g = *gpios_[b];
        g.clk(gpio_clk_[b]);
        g.reset_n(*d.reset_n);
        g.gpio_in(gpio_in_[b]);
        g.gpio_out(gpio_out_[b]);
        g.gpio_dir(gpio_dir_[b]);
        g.gpio_in_sync(gpio_sync_[b]);
        for (unsigned i = 0; i < 32; ++i)
            g.gpio_padcfg[i](gpio_padcfg_[b * 32 + i]);
        g.interrupt(plic_source[gpio_id[b] - 1]);
        g.power_event(gpio_wake_[b]);
        sockets_[bank_target[b]] = &g.socket;
        probes_[bank_target[b]] = {fu3::gpio::reg::PADCFG0, fu3::gpio::reg::PADCFG_RESET};
    }
    // T13: QSPI (PLIC ID4, Ibex irq_fast_i[7]); flash on the board, powered.
    {
        const auto d = domains("AP_QSPIC");
        qspi_clock_.clk(qspi_clk_);
        qspi_clock_.enable(*d.clock_on);
        qspi_ = std::make_unique<fu3::qspi::QspiController>("QSPI");
        auto& q = *qspi_;
        q.clk(qspi_clk_);
        q.pclk(qspi_clk_);
        q.hclk(qspi_clk_);
        q.dma_clk(qspi_clk_);
        q.reset_n(*d.reset_n);
        q.preset_n(*d.reset_n);
        q.hreset_n(*d.reset_n);
        q.dma_reset_n(*d.reset_n);
        q.dma_rx_clr(qspi_dma_clear_);
        q.dma_tx_clr(qspi_dma_clear_);
        q.irq(plic_source[FU3_PLIC_ID_QSPI - 1]);
        q.dma_rx_req(qspi_dma_rx_req_);
        q.dma_tx_req(qspi_dma_tx_req_);
        fu3::qspi::FlashConfig flash_config;
        flash_config.size_bytes = kFlashBytes;
        flash_ = std::make_unique<fu3::qspi::QspiFlashModel>("FLASH", flash_config);
        flash_->reset_n(flash_power_);
        q.flash.bind(flash_->socket);
        sockets_["AP_QSPIC"] = &q.ap_qspic;
        sockets_["QSPI_XIP"] = &q.xip;
        probes_["AP_QSPIC"] = {fu3::qspi::reg::CR, 0};
    }
    // T14: HyperRAM (data path reset: SW_RST_N_0.hyperbus; the CSR side is
    // gated separately by AP_HBCTRL's bits at the bus).
    fu3_hyperram::Config hyperram_config;
    hyperram_config.core_period = sc_core::sc_time(50, sc_core::SC_NS);
    hyperram_ = std::make_unique<fu3_hyperram::Controller>("HYPERRAM", hyperram_config);
    // HB_RESETN (Common v2.1 IOPAD MAP die 94) is a SoC output to the device's
    // RESET#. No HBCTRL CSR controls it yet (Q07-a), so it follows the
    // HYPERBUS domain reset (POR, system reset, SW_RST_N_0.hyperbus): the
    // device and its memory reset with the controller.
    hyperram_->reset_n(*domains("HYPERBUS").reset_n);
    hyperram_->device_reset_n(*domains("HYPERBUS").reset_n);
    hyperram_->irq(hbctrl_irq_);
    sockets_["HYPERBUS"] = &hyperram_->memory;
    sockets_["AP_HBCTRL"] = &hyperram_->csr;
    probes_["AP_HBCTRL"] = {fu3_hyperram::reg::ID, 0x48525650};   // "HRVP": provisional VP ABI v1

    // T15: AES-256 (SYSBUS_0, clocked by the bus clock).
    {
        const auto d = domains("AES");
        aes_clock_.clk(aes_clk_);
        aes_clock_.enable(*d.clock_on);
        aes_ = std::make_unique<fu3::aes::AesController>("AES");
        aes_->clk(aes_clk_);
        aes_->reset_n(*d.reset_n);
        sockets_["AES"] = &aes_->socket;
        probes_["AES"] = {fu3::aes::reg::NAME0, 0x61657320};   // "aes "
    }
    // ADC controller and hardmacro.
    {
        const auto d = domains("ADC");
        adc_clock_.clk(adc_clk_);
        adc_clock_.enable(*d.clock_on);
        adc_clock_.set_period(sc_core::sc_time(100, sc_core::SC_NS));
        adc_ = std::make_unique<fu3::adc::Adc>("ADC");
        adc_->adc_clk(adc_clk_);
        adc_->reset_n(*d.reset_n);
        adc_->eoc(adc_eoc_);
        adc_->data_in(adc_data_);
        adc_->sampclk(adc_sampclk_);
        adc_->cs(adc_cs_);
        adc_->pwd(adc_pwd_);
        adc_->macro_reset(adc_macro_reset_);
        adc_->samp_sel(adc_samp_sel_);
        adc_->channel(adc_channel_);
        adc_macro_ = std::make_unique<adc_macro>("ADC_macro", sc_core::sc_time(100, sc_core::SC_NS));
        adc_macro_->sampclk(adc_sampclk_);
        adc_macro_->cs(adc_cs_);
        adc_macro_->pwd(adc_pwd_);
        adc_macro_->macro_reset(adc_macro_reset_);
        adc_macro_->channel(adc_channel_);
        adc_macro_->eoc(adc_eoc_);
        adc_macro_->data(adc_data_);
        sockets_["ADC"] = &adc_->socket;
        probes_["ADC"] = {fu3::adc::reg::VERSION, 0x10000};
    }
    // RNG: PLIC ID21, Ibex irq_fast_i[5]. The bit source is a seeded,
    // index-addressed generator (deterministic per index, as the model requires).
    {
        fu3_rng::Config rng_config;
        rng_config.clock_period = sc_core::sc_time(50, sc_core::SC_NS);
        rng_config.sample_bit = [this](std::uint64_t index) {
            std::uint64_t z = rng_seed_ + 0x9e3779b97f4a7c15ull * ((index >> 6) + 1);   // splitmix64
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            z ^= z >> 31;
            return ((z >> (index & 63)) & 1) != 0;
        };
        rng_ = std::make_unique<fu3_rng::Rng>("RNG", rng_config);
        rng_->reset_n(*domains("RNG").reset_n);
        rng_->irq(plic_source[FU3_PLIC_ID_RNG - 1]);
        sockets_["RNG"] = &rng_->socket;
        probes_["RNG"] = {fu3_rng::reg::CONTROL, 0};
    }

    // T16: mailbox.
    mailbox_ = std::make_unique<fu3_mailbox::Mailbox>("MAILBOX");
    mailbox_->presetn0(*domains("MAILBOX_CVA6").reset_n);
    mailbox_->presetn1(*domains("MAILBOX_IBEX").reset_n);
    mailbox_->irq_cva6(mailbox_cva6_irq_);
    mailbox_->irq_ibex(mailbox_ibex_irq);
    sockets_["MAILBOX_CVA6"] = &mailbox_->cva6_socket;
    sockets_["MAILBOX_IBEX"] = &mailbox_->ibex_socket;
    probes_["MAILBOX_CVA6"] = {FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 0};
    probes_["MAILBOX_IBEX"] = {FU3_MAILBOX_WR_THRESHOLDS_OFFSET, 0};

    // T17: DMA0 (PLIC ID5, Ibex irq_fast_i[6]). Common v2.1 DMA_Mapping
    // allocates channels 0/1 SPI0 RX/TX, 2/3 UART1, 4/5 UART2, 6/7 SPI1; the
    // model selects the endpoint by peripheral ID per channel, so the VP binds
    // the request lines by ID (kDmaId*) and firmware programs each channel's
    // READ_ID/WRITE_ID. SPI2/3, UART0/3-7 and QSPI have no DMA request line.
    {
        const auto d = domains("AP_DMA");
        dma_ = std::make_unique<fu3::dma::ApDma>("DMA0", sc_core::sc_time(50, sc_core::SC_NS));
        dma_->reset_n(*d.reset_n);
        dma_->irq(plic_source[FU3_PLIC_ID_DMA0 - 1]);
        dma_->rx_request(dma_rx_request_);
        dma_->tx_request(dma_tx_request_);
        dma_->rx_clear(dma_rx_clear_);
        dma_->tx_clear(dma_tx_clear_);
        dma_->master_socket.bind(dma_adapter_.input);
        sockets_["AP_DMA"] = &dma_->target_socket;
    }
    SC_METHOD(dma_requests);
    sensitive << spi_dma_rx_request[0] << spi_dma_tx_request[0] << spi_dma_rx_request[1] << spi_dma_tx_request[1]
              << uart_dma_rx_request[1] << uart_dma_tx_request[1] << uart_dma_rx_request[2] << uart_dma_tx_request[2];
    SC_METHOD(dma_clears);
    sensitive << dma_rx_clear_ << dma_tx_clear_;

    SC_METHOD(publish_gpio);
    sensitive << gpio_wake_[0] << gpio_wake_[1] << gpio_wake_[2];
    bind_pads();

    // Clock gates of the models clocked internally. TIMER0-3, GPIOA/B/C,
    // QSPI, AES and ADC take theirs through clock_enabled / fu3_clock above;
    // INTMAP is combinational and the mailbox has no clocked state.
    auto gate = [&](const char* target, std::function<void(bool)> apply) {
        clock_gates_.push_back({domains(target).clock_on, std::move(apply)});
    };
    gate("AP_PLIC", [this](bool on) { plic_.set_clock_enabled(on); });
    for (unsigned i = 0; i < 2; ++i)
        gate(i ? "WDT1" : "WDT0", [this, i](bool on) { wdts_[i]->set_clock_enabled(on); });
    for (unsigned i = 0; i < kUarts; ++i)
        gate(("UART" + std::to_string(i)).c_str(), [this, i](bool on) { uarts_[i]->set_clock_enabled(on); });
    for (unsigned i = 0; i < kSpis; ++i)
        gate(("SPI" + std::to_string(i)).c_str(), [this, i](bool on) { spis_[i]->set_clock_enabled(on); });
    gate("I2C", [this](bool on) { i2c_->set_clock_enabled(on); });
    gate("RNG", [this](bool on) { rng_->set_clock_enabled(on); });
    gate("HYPERBUS", [this](bool on) { hyperram_->set_clock_enabled(on); });
    gate("AP_DMA", [this](bool on) { dma_->set_clock_enabled(on); });
    SC_METHOD(apply_clock_gates);
    for (const auto& g : clock_gates_)
        sensitive << *g.clock_on;
}

void fu3_peripherals::apply_clock_gates()
{
    for (const auto& g : clock_gates_)
        g.apply(g.clock_on->read());
}

void fu3_peripherals::dma_requests()
{
    sc_dt::sc_uint<32> rx = 0, tx = 0;
    rx[kDmaIdSpi0] = spi_dma_rx_request[0].read();
    tx[kDmaIdSpi0] = spi_dma_tx_request[0].read();
    rx[kDmaIdUart1] = uart_dma_rx_request[1].read();
    tx[kDmaIdUart1] = uart_dma_tx_request[1].read();
    rx[kDmaIdUart2] = uart_dma_rx_request[2].read();
    tx[kDmaIdUart2] = uart_dma_tx_request[2].read();
    rx[kDmaIdSpi1] = spi_dma_rx_request[1].read();
    tx[kDmaIdSpi1] = spi_dma_tx_request[1].read();
    dma_rx_request_.write(rx);
    dma_tx_request_.write(tx);
}

void fu3_peripherals::dma_clears()
{
    // Every handshake input has this one writer; endpoints without a DMA
    // request line stay low.
    const auto rx = dma_rx_clear_.read(), tx = dma_tx_clear_.read();
    for (unsigned i = 0; i < kUarts; ++i) {
        uart_dma_rx_clear[i].write(i == 1 ? bool(rx[kDmaIdUart1]) : i == 2 ? bool(rx[kDmaIdUart2]) : false);
        uart_dma_tx_clear[i].write(i == 1 ? bool(tx[kDmaIdUart1]) : i == 2 ? bool(tx[kDmaIdUart2]) : false);
    }
    for (unsigned i = 0; i < kSpis; ++i) {
        spi_dma_rx_ack[i].write(i == 0 ? bool(rx[kDmaIdSpi0]) : i == 1 ? bool(rx[kDmaIdSpi1]) : false);
        spi_dma_tx_ack[i].write(i == 0 ? bool(tx[kDmaIdSpi0]) : i == 1 ? bool(tx[kDmaIdSpi1]) : false);
    }
}

void fu3_peripherals::publish_gpio()
{
    plic_source[FU3_PLIC_ID_GPIO0_WAKE - 1].write(gpio_wake_[0].read() || gpio_wake_[1].read() || gpio_wake_[2].read());
}

void fu3_peripherals::bind_pads()
{
    // Chip side: every modelled pin function of Common v2.1 IOPAD MAP.
    auto bit = [](const sc_core::sc_signal<sc_dt::sc_uint<32>>& v, unsigned n) { return bool(v.read()[n]); };
    const char bank_name[3] = {'A', 'B', 'C'};
    for (unsigned b = 0; b < 3; ++b)
        for (unsigned n = 0; n < 8; ++n) {
            const std::string fn = std::string("GPIO") + bank_name[b] + "_" + std::to_string(n);
            pinmux_.chip_output(
                fn, [this, b, n, bit] { return bit(gpio_out_[b], n); }, [this, b, n, bit] { return bit(gpio_dir_[b], n); },
                {&gpio_out_[b].value_changed_event(), &gpio_dir_[b].value_changed_event()});
            pinmux_.chip_input(
                fn, [this, b, n](bool v) { gpio_in_bits_[b] = (gpio_in_bits_[b] & ~(1u << n)) | (std::uint32_t(v) << n); },
                false);
        }
    pinmux_.after_inputs([this] {
        for (unsigned b = 0; b < 3; ++b)
            gpio_in_[b].write(gpio_in_bits_[b]);   // unbonded bank bits read 0
    });
    for (unsigned k = 0; k < kUarts; ++k) {
        const auto n = std::to_string(k);
        pinmux_.chip_output("TXD_" + n, [this, k] { return uart_txd_[k].read(); }, [] { return true; },
                            {&uart_txd_[k].value_changed_event()});
        pinmux_.chip_input("RXD_" + n, [this, k](bool v) { uart_rxd_[k].write(v); }, true);
    }
    for (unsigned k = 0; k < kSpis; ++k) {
        const auto n = std::to_string(k);
        pinmux_.chip_output("SCK_" + n, [this, k] { return spi_sck_[k].read(); }, [this, k] { return spi_sck_oe_[k].read(); },
                            {&spi_sck_[k].value_changed_event(), &spi_sck_oe_[k].value_changed_event()});
        pinmux_.chip_output("MOSI_" + n, [this, k] { return spi_mosi_[k].read(); },
                            [this, k] { return spi_mosi_oe_[k].read(); },
                            {&spi_mosi_[k].value_changed_event(), &spi_mosi_oe_[k].value_changed_event()});
        pinmux_.chip_output("MISO_" + n, [this, k] { return spi_miso_[k].read(); },
                            [this, k] { return spi_miso_oe_[k].read(); },
                            {&spi_miso_[k].value_changed_event(), &spi_miso_oe_[k].value_changed_event()});
        // ss_n[0] is the bonded chip select (Q07-g); [3:1] stay internal.
        pinmux_.chip_output("SSN_" + n, [this, k] { return bool(spi_ss_n_[k].read()[0]); },
                            [this, k] { return spi_ss_oe_[k].read(); },
                            {&spi_ss_n_[k].value_changed_event(), &spi_ss_oe_[k].value_changed_event()});
        pinmux_.chip_input("SCK_" + n, [this, k](bool v) { spi_sck_in_[k].write(v); }, false);
        pinmux_.chip_input("MOSI_" + n, [this, k](bool v) { spi_mosi_in_[k].write(v); }, false);
        pinmux_.chip_input("MISO_" + n, [this, k](bool v) { spi_miso_in_[k].write(v); }, true);
        pinmux_.chip_input("SSN_" + n, [this, k](bool v) { spi_ss_n_in_[k].write(v); }, true);
    }
    // I2C: open drain (oe_n low pulls the line low), Q06-e.
    pinmux_.chip_output("SCL_0", [] { return false; }, [this] { return !i2c_scl_oe_n_.read(); },
                        {&i2c_scl_oe_n_.value_changed_event()}, true);
    pinmux_.chip_output("SDA_0", [] { return false; }, [this] { return !i2c_sda_oe_n_.read(); },
                        {&i2c_sda_oe_n_.value_changed_event()}, true);
    pinmux_.chip_input("SCL_0", [this](bool v) { i2c_scl_.write(v); }, true);
    pinmux_.chip_input("SDA_0", [this](bool v) { i2c_sda_.write(v); }, true);
    // Fixed input pads to SYSCSR (and DEBUG_EN, no consumer yet).
    pinmux_.chip_input("BOOT", [this](bool v) { strap_boot.write(v); }, false);
    pinmux_.chip_input("MODE_1", [this](bool v) { strap_mode_1.write(v); }, false);
    pinmux_.chip_input("MODE_0", [this](bool v) { strap_mode_0.write(v); }, false);
    pinmux_.chip_input("TESTMODE", [this](bool v) { test_mode.write(v); }, false);
    pinmux_.chip_input("TEST_MODE_SEL", [this](bool v) { test_select.write(v); }, false);
    pinmux_.chip_input("DEBUG_EN", [this](bool v) { debug_en.write(v); }, false);

    // Board side: UART peers on their TXD/RXD pads (UART5 detached: its pads
    // carry the I2C pull-ups), I2C pull-ups.
    for (unsigned k = 0; k < kUarts; ++k) {
        pinmux_.board_observe(kUartPad[k].txd, peer_txd_[k]);
        pinmux_.board_signal(kUartPad[k].rxd, peer_rxd_[k], k != 5);
    }
    pinmux_.board_pull(kI2cSdaPad, fu3_pinmux::pull::up);
    pinmux_.board_pull(kI2cSclPad, fu3_pinmux::pull::up);
}

void fu3_peripherals::attach_uart_peer(unsigned i, bool on)
{
    if (i < kUarts)
        pinmux_.board_attach(kUartPad[i].rxd, on);
}

void fu3_peripherals::set_spi_loopback(unsigned i, bool on)
{
    if (i < kSpis)
        pinmux_.board_link(kSpiPad[i].mosi, kSpiPad[i].miso, on);
}

void fu3_peripherals::tie_offs()
{
    for (unsigned i = 0; i < kUarts; ++i) {
        uart_cts_n_[i].write(false);
        uart_modem_high_[i].write(true);
    }
    flash_power_.write(true);
}

void fu3_peripherals::or_wdt_requests()
{
    wdt_reset_request.write(wdt_reset_[0].read() || wdt_reset_[1].read());
}

target_socket* fu3_peripherals::socket(const std::string& target)
{
    const auto it = sockets_.find(target);
    return it == sockets_.end() ? nullptr : it->second;
}

void fu3_peripherals::set_bus_clock(sc_core::sc_time period)
{
    if (period == sc_core::SC_ZERO_TIME)
        return;
    // Timer/WDT run on the PERIBUS clock of the CRM (decision Q05-f; VP binding).
    for (auto& t : timers_)
        t->set_period(period);
    for (auto& w : wdts_)
        w->set_clock(period, period);
    for (auto& u : uarts_)
        u->set_clock_period(period);   // UART clock = PCLK (VP binding)
    for (auto& s : spis_)
        s->set_clock(period, period);
    i2c_->set_clock_period(period);
    for (auto& clock : gpio_clock_)
        clock->set_period(period);   // GPIO HCLK/PCLK = bus clock
    qspi_clock_.set_period(period);
    hyperram_->set_core_period(period);
    aes_clock_.set_period(period);
    rng_->set_clock_period(period);
    dma_->set_peripheral_cycle(period);
}

} // namespace cdc::platforms::fu3_soc
