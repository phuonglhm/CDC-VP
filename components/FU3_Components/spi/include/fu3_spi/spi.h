#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <array>
#include <deque>
#include <cstdint>
#include "fu3_spi/registers.h"

namespace fu3_spi {
class Spi : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Spi> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"}, apb_reset_n{"apb_reset_n"};
    sc_core::sc_in<bool> sck_in{"sck_in"}, ss_n_in{"ss_n_in"}, mosi_in{"mosi_in"}, miso_in{"miso_in"};
    sc_core::sc_in<bool> dma_tx_ack{"dma_tx_ack"}, dma_rx_ack{"dma_rx_ack"};
    sc_core::sc_out<bool> sck{"sck"}, mosi{"mosi"}, miso{"miso"};
    sc_core::sc_out<sc_dt::sc_uint<4>> ss_n{"ss_n"};
    sc_core::sc_out<bool> sck_oe{"sck_oe"}, ss_oe{"ss_oe"}, mosi_oe{"mosi_oe"}, miso_oe{"miso_oe"};
    sc_core::sc_out<bool> irq{"irq"}, dma_tx_req{"dma_tx_req"}, dma_rx_req{"dma_rx_req"};
    SC_HAS_PROCESS(Spi);
    explicit Spi(sc_core::sc_module_name,unsigned depth=16,
        sc_core::sc_time sys_period=sc_core::sc_time(10,sc_core::SC_NS),
        sc_core::sc_time apb_period=sc_core::sc_time(10,sc_core::SC_NS));
    // System and APB clocks from the clock controller (platform CRM); used from
    // the next cycle, zero is ignored.
    // Clock gate from the clock controller (platform CRM): while disabled the
    // IP gets no clock edge, keeps its state and resumes on the next enabled
    // edge. Enabled by default.
    void set_clock_enabled(bool on){if(on!=clock_enabled_){clock_enabled_=on;clock_gate_changed_.notify(sc_core::SC_ZERO_TIME);}}
    bool clock_enabled() const {return clock_enabled_;}
    void set_clock(sc_core::sc_time sys_period, sc_core::sc_time apb_period) {
        if(sys_period!=sc_core::SC_ZERO_TIME) sys_=sys_period;
        if(apb_period!=sc_core::SC_ZERO_TIME) apb_=apb_period;
    }
private:
    enum class Phase {Idle,Setup,Shift,Gap,Hold,CsIdle};
    unsigned depth_;
    sc_core::sc_time sys_,apb_;
    std::array<std::uint32_t,12> regs_{};
    std::deque<std::uint32_t> tx_,rx_;
    Phase phase_=Phase::Idle;
    unsigned ticks_=0,edges_=0,bits_=8,flush_=0,tx_dma_=0,rx_dma_=0;
    std::uint32_t shift_tx_=0,shift_rx_=0;
    bool slave_=false,loaded_=false,prev_ss_=true,prev_sck_=false;
    bool out_sck_=false,out_mosi_=false,out_miso_=false,irq_=false,ens_=false;
    std::array<bool,2> tx_ack_{},rx_ack_{};
    std::array<sc_core::sc_time,7> last_set_{};
    std::array<bool,7> has_set_{};
    sc_core::sc_mutex access_;
    sc_core::sc_event complete_,changed_;
    bool clock_enabled_=true;
    sc_core::sc_event clock_gate_changed_;
    tlm::tlm_generic_payload* pending_=nullptr;
    unsigned apb_epoch_=0,sys_epoch_=0;
    void run();void reset();void cycle();void publish();void observe_reset();
    void master();void slave();void load(bool pop);void edge(bool input,bool master_mode);
    void flag(unsigned mask);unsigned status() const;unsigned interrupt_status() const;
    unsigned read(unsigned offset,bool pop);void write(unsigned offset,unsigned value,unsigned mask);
    bool validate(tlm::tlm_generic_payload&);void execute(tlm::tlm_generic_payload&);
    void transport(tlm::tlm_generic_payload&,sc_core::sc_time&);unsigned debug(tlm::tlm_generic_payload&);
    unsigned half_cycles() const {return (regs_[1]&0xffff)+1;}
    bool space() const {return (regs_[0]&FU3_SPI_TXONLY)||rx_.size()<depth_;}
    void dma(unsigned& state,bool on,bool condition,bool ack);
};
}
