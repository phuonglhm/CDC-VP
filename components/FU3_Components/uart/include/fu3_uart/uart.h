#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include <deque>
#include <vector>
#include "fu3_uart/registers.h"

namespace fu3_uart {
// These are integration choices where the HDS does not define a CSR encoding.
struct Config {
    sc_core::sc_time clock_period{10, sc_core::SC_NS};
    unsigned fifo_depth = 64; // SIZE_E = 5 or 6.
    unsigned oversampling = 16; // HDS parameter; the register table defines no selector bit.
};

class Uart : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Uart> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"}, rxd{"rxd"};
    sc_core::sc_in<bool> cts_n{"cts_n"}, dsr_n{"dsr_n"}, ri_n{"ri_n"}, dcd_n{"dcd_n"};
    sc_core::sc_in<bool> dma_tx_clear{"dma_tx_clear"}, dma_rx_clear{"dma_rx_clear"};
    sc_core::sc_out<bool> txd{"txd"}, rts_n{"rts_n"}, dtr_n{"dtr_n"};
    sc_core::sc_out<bool> out1_n{"out1_n"}, out2_n{"out2_n"}, irq{"irq"};
    sc_core::sc_out<bool> dma_tx_request{"dma_tx_request"}, dma_rx_request{"dma_rx_request"};
    SC_HAS_PROCESS(Uart);
    explicit Uart(sc_core::sc_module_name, Config = {});
private:
    struct Received {std::uint8_t data, errors; bool reported = false;};
    struct Segment {bool level; unsigned samples;};
    const Config config_;
    std::uint8_t ier_=0, fcr_=0, lcr_=0, mcr_=0, scr_=0, dll_=0, dlm_=0;
    std::uint8_t modem_=0, delta_=0;
    bool overrun_=false, thre_pending_=false, timeout_=false, tx_level_=true;
    bool auto_rts_=false, previous_rx_=true, rx_active_=false, rx_all_low_=true;
    unsigned tx_index_=0, tx_remaining_=0, tx_divider_=0, tx_divisor_=0;
    unsigned rx_divider_=0, rx_divisor_=0, rx_age_=0, rx_center_=0, rx_votes_=0;
    int rx_position_=-1;
    std::uint8_t rx_format_=0, rx_data_=0, rx_errors_=0;
    bool rx_parity_=false;
    std::uint64_t cycles_=0, last_rx_activity_=0;
    std::deque<std::uint8_t> tx_fifo_;
    std::deque<Received> rx_fifo_;
    std::vector<Segment> tx_frame_;
    tlm::tlm_generic_payload* pending_=nullptr;
    sc_core::sc_mutex access_;
    sc_core::sc_event completed_, outputs_;
    void run();
    void reset_delta_async();
    void reset_state();
    void cycle();
    void publish();
    void update_modem();
    void transmit();
    void receive();
    void finish_receive();
    void begin_transmit();
    void flush_rx();
    unsigned capacity() const;
    unsigned trigger() const;
    unsigned divisor() const {return unsigned(dll_) | (unsigned(dlm_) << 8);}
    unsigned frame_samples(unsigned format) const;
    static bool parity(unsigned value, unsigned format);
    unsigned line_status() const;
    unsigned interrupt_id() const;
    unsigned read(unsigned offset, bool side_effects);
    void write(unsigned offset, unsigned value);
    bool validate(tlm::tlm_generic_payload&);
    void execute(tlm::tlm_generic_payload&, bool side_effects);
    void transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    unsigned debug(tlm::tlm_generic_payload&);
};
} // namespace fu3_uart
