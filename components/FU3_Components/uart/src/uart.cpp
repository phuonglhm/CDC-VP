#include "fu3_uart/uart.h"
#include <algorithm>
#include <stdexcept>

namespace fu3_uart {
Uart::Uart(sc_core::sc_module_name name, Config config) : sc_module(name), config_(config) {
    if (config.clock_period == sc_core::SC_ZERO_TIME ||
        (config.fifo_depth != 32 && config.fifo_depth != 64) ||
        (config.oversampling != 8 && config.oversampling != 16))
        throw std::invalid_argument("UART requires a positive clock period, depth 32/64 and oversampling 8/16");
    socket.register_b_transport(this, &Uart::transport);
    socket.register_transport_dbg(this, &Uart::debug);
    SC_THREAD(run);
    SC_METHOD(reset_delta_async);
    sensitive << reset_n.neg();
    dont_initialize();
    SC_METHOD(publish); sensitive << outputs_;
}

unsigned Uart::capacity() const {
    return !(fcr_&1) ? 1 : (fcr_&32) ? config_.fifo_depth : 16;
}
unsigned Uart::trigger() const {
    static constexpr unsigned small[]{1,4,8,14}, large[]{1,16,32,56};
    return !(fcr_&1) ? 1 : (fcr_&32) ? large[fcr_>>6] : small[fcr_>>6];
}
unsigned Uart::frame_samples(unsigned format) const {
    const unsigned bits=5+(format&3);
    const unsigned stops=(format&4) ? (bits==5 ? 3 : 4) : 2;
    return (1+bits+unsigned(bool(format&8)))*config_.oversampling + stops*config_.oversampling/2;
}
bool Uart::parity(unsigned value, unsigned format) {
    if (format&32) return !(format&16);
    bool odd=false;
    for (unsigned i=0; i<5+(format&3); ++i) odd ^= bool(value&(1u<<i));
    return (format&16) ? odd : !odd;
}
void Uart::flush_rx() {
    rx_fifo_.clear(); overrun_=timeout_=false; last_rx_activity_=cycles_;
}
void Uart::reset_state() {
    ier_=fcr_=lcr_=mcr_=scr_=dll_=dlm_=0;
    modem_=delta_=0; overrun_=thre_pending_=timeout_=false;
    tx_fifo_.clear(); rx_fifo_.clear(); tx_frame_.clear();
    tx_level_=previous_rx_=true; auto_rts_=rx_active_=false;
    tx_index_=tx_remaining_=tx_divider_=tx_divisor_=0;
    rx_divider_=rx_divisor_=rx_age_=rx_center_=rx_votes_=0;
    cycles_=last_rx_activity_=0;
}
void Uart::reset_delta_async() {
    // HDS v1.3 explicitly clears MSR delta bits asynchronously on preset_n.
    if (!reset_n.read()) {
        delta_=0;
        outputs_.notify(sc_core::SC_ZERO_TIME);
    }
}
void Uart::run() {
    // Reset is synchronous: pulses that miss a clock edge do not reset the IP.
    while (true) {
        wait(config_.clock_period);
        if (!reset_n.read()) {
            reset_state();
            if (pending_) {
                pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                pending_=nullptr; completed_.notify(sc_core::SC_ZERO_TIME);
            }
        } else cycle();
        outputs_.notify(sc_core::SC_ZERO_TIME);
    }
}
void Uart::cycle() {
    ++cycles_;
    // Defined VP ordering: MMIO, modem sampling, RX sampling, TX progression.
    // An error received on this tick therefore survives an LSR read on this tick.
    if (pending_) {
        execute(*pending_, true); pending_=nullptr; completed_.notify(sc_core::SC_ZERO_TIME);
    }
    update_modem();
    receive();
    transmit();
    if ((fcr_&1) && !rx_fifo_.empty() && divisor() &&
        cycles_-last_rx_activity_ >= std::uint64_t(4)*frame_samples(lcr_)*divisor()) timeout_=true;
    // Threshold hysteresis: stop at trigger, resume once the FIFO is empty.
    if (rx_fifo_.size() >= trigger()) auto_rts_=false;
    else if (rx_fifo_.empty()) auto_rts_=true;
}
void Uart::update_modem() {
    const unsigned current=(mcr_&16)
        ? ((mcr_&2 ? 16u:0u)|(mcr_&1 ? 32u:0u)|(mcr_&4 ? 64u:0u)|(mcr_&8 ? 128u:0u))
        : ((!cts_n.read()?16u:0u)|(!dsr_n.read()?32u:0u)|(!ri_n.read()?64u:0u)|(!dcd_n.read()?128u:0u));
    const unsigned changes=current^modem_;
    if (changes&16) delta_|=1;
    if (changes&32) delta_|=2;
    if ((modem_&64) && !(current&64)) delta_|=4;
    if (changes&128) delta_|=8;
    modem_=static_cast<std::uint8_t>(current);
}
void Uart::begin_transmit() {
    const auto value=tx_fifo_.front(); tx_fifo_.pop_front();
    if (tx_fifo_.empty()) thre_pending_=true;
    tx_frame_.clear();
    const unsigned os=config_.oversampling;
    tx_frame_.push_back({false,os});
    for (unsigned i=0; i<5u+(lcr_&3u); ++i) tx_frame_.push_back({bool(value&(1u<<i)),os});
    if (lcr_&8) tx_frame_.push_back({parity(value,lcr_),os});
    unsigned stops=(lcr_&4) ? ((lcr_&3)==0 ? 3:4) : 2;
    tx_frame_.push_back({true,stops*os/2});
    tx_index_=tx_divider_=0; tx_divisor_=divisor();
    tx_remaining_=tx_frame_[0].samples; tx_level_=false;
}
void Uart::transmit() {
    if (!tx_frame_.empty() && ++tx_divider_ >= tx_divisor_) {
        tx_divider_=0;
        if (--tx_remaining_==0) {
            if (++tx_index_==tx_frame_.size()) {tx_frame_.clear(); tx_level_=true;}
            else {tx_remaining_=tx_frame_[tx_index_].samples; tx_level_=tx_frame_[tx_index_].level;}
        }
    }
    // Auto-CTS gates new frames only; an active character must finish intact.
    if (tx_frame_.empty() && !tx_fifo_.empty() && divisor() &&
        (!(mcr_&FU3_UART_MCR_AFE) || (modem_&16))) begin_transmit();
}
void Uart::receive() {
    const bool input=(mcr_&16) ? ((lcr_&64)?false:tx_level_) : rxd.read();
    if (!rx_active_) {
        if (previous_rx_ && !input && divisor()) {
            rx_active_=true; rx_divisor_=divisor(); rx_format_=lcr_;
            rx_age_=rx_divider_=rx_votes_=0; rx_center_=config_.oversampling/2;
            rx_position_=-1; rx_data_=rx_errors_=0; rx_parity_=false; rx_all_low_=true;
        }
        previous_rx_=input;
        return;
    }
    previous_rx_=input;
    if (++rx_divider_ < rx_divisor_) return;
    rx_divider_=0; ++rx_age_;
    if (rx_age_>=rx_center_-1 && rx_age_<=rx_center_+1) rx_votes_+=unsigned(input);
    if (rx_age_!=rx_center_+1) return;
    const bool bit=rx_votes_>=2; rx_votes_=0; rx_all_low_ &= !bit;
    const int bits=5+(rx_format_&3);
    if (rx_position_<0) {
        if (bit) {rx_active_=false; return;} // Reject a false start.
    } else if (rx_position_<bits) {
        if (bit) rx_data_|=std::uint8_t(1u<<rx_position_);
    } else if (rx_position_==bits && (rx_format_&8)) {
        rx_parity_=bit;
        if (bit!=parity(rx_data_,rx_format_)) rx_errors_|=FU3_UART_PE;
    } else {
        if (!bit) rx_errors_|=FU3_UART_FE;
        if (!bit && rx_all_low_) rx_errors_|=FU3_UART_BI;
        finish_receive(); rx_active_=false; return;
    }
    ++rx_position_; rx_center_+=config_.oversampling;
}
void Uart::finish_receive() {
    if (rx_fifo_.size()>=capacity()) {
        overrun_=true; // Preserve buffered data; discard the newly received frame.
    } else rx_fifo_.push_back({rx_data_,rx_errors_,false});
    last_rx_activity_=cycles_;
    // An already asserted timeout remains pending until RBR is read.
}
unsigned Uart::line_status() const {
    unsigned value=(!rx_fifo_.empty()?FU3_UART_DR:0u)|(overrun_?FU3_UART_OE:0u);
    if (!rx_fifo_.empty() && !rx_fifo_.front().reported) value|=rx_fifo_.front().errors;
    if (tx_fifo_.empty()) value|=FU3_UART_THRE;
    if (tx_fifo_.empty() && tx_frame_.empty()) value|=FU3_UART_TEMT;
    if ((fcr_&1) && std::any_of(rx_fifo_.begin(),rx_fifo_.end(),[](const Received& r){return r.errors!=0;}))
        value|=FU3_UART_FIFO_ERROR;
    return value;
}
unsigned Uart::interrupt_id() const {
    if ((ier_&4) && (line_status()&0x1e)) return 6;
    if ((ier_&1) && rx_fifo_.size()>=trigger()) return 4;
    if ((ier_&1) && timeout_ && !rx_fifo_.empty()) return 12;
    if ((ier_&2) && thre_pending_) return 2;
    if ((ier_&8) && delta_) return 0;
    return 1;
}
void Uart::publish() {
    txd.write((mcr_&16) ? true : (lcr_&64) ? false : tx_level_);
    const bool loop=mcr_&16;
    dtr_n.write(loop || !(mcr_&1)); out1_n.write(loop || !(mcr_&4)); out2_n.write(loop || !(mcr_&8));
    const bool auto_rts_enabled=(mcr_&FU3_UART_MCR_AFE) && (mcr_&2);
    rts_n.write(loop || !(mcr_&2) || (auto_rts_enabled && !auto_rts_));
    irq.write(interrupt_id()!=1);
    dma_tx_request.write((fcr_&8) && tx_fifo_.size()<capacity() && !dma_tx_clear.read());
    dma_rx_request.write((fcr_&8) && !rx_fifo_.empty() &&
        (rx_fifo_.size()>=trigger() || timeout_) && !dma_rx_clear.read());
}
unsigned Uart::read(unsigned offset, bool side_effects) {
    if ((lcr_&128) && offset<2) return offset==0?dll_:dlm_;
    switch (offset) {
    case 0: {
        const unsigned value=rx_fifo_.empty()?0:rx_fifo_.front().data;
        if (side_effects) {
            if (!rx_fifo_.empty()) rx_fifo_.pop_front();
            timeout_=false; last_rx_activity_=cycles_;
        }
        return value;
    }
    case 1: return ier_;
    case 2: {
        const unsigned id=interrupt_id();
        // HDS: IIR acknowledges THRE only when it is the only active source.
        if (side_effects && id==2 && !((ier_&8)&&delta_)) thre_pending_=false;
        return id|((fcr_&1)?((fcr_&32)?0xe0:0xc0):0);
    }
    case 3: return lcr_;
    case 4: return mcr_;
    case 5: {
        const auto value=line_status();
        if (side_effects) {overrun_=false; if (!rx_fifo_.empty()) rx_fifo_.front().reported=true;}
        return value;
    }
    case 6: {const auto value=modem_|delta_; if (side_effects) delta_=0; return value;}
    default: return scr_;
    }
}
void Uart::write(unsigned offset, unsigned value) {
    value&=255;
    if ((lcr_&128) && offset<2) {
        if (offset==0) dll_=std::uint8_t(value); else dlm_=std::uint8_t(value);
        return;
    }
    switch (offset) {
    case 0:
        thre_pending_=false;
        if (tx_fifo_.size()<capacity()) tx_fifo_.push_back(std::uint8_t(value));
        break;
    case 1:
        if (!(ier_&2) && (value&2) && tx_fifo_.empty()) thre_pending_=true;
        ier_=value&15; break;
    case 2: {
        // The HDS explicitly requires DLAB to set the extended FIFO bit.
        const unsigned next=(value&0xc9)|((lcr_&128)?(value&32):(fcr_&32));
        const bool mode_changed=((next^fcr_)&33)!=0;
        fcr_=std::uint8_t(next);
        if (mode_changed || (value&2)) flush_rx();
        if (mode_changed || (value&4)) {tx_fifo_.clear(); thre_pending_=true;}
        break;
    }
    case 3: lcr_=std::uint8_t(value); break;
    case 4: mcr_=value&0x3f; break;
    case 7: scr_=std::uint8_t(value); break;
    default: break; // LSR/MSR are read-only.
    }
}
bool Uart::validate(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    auto fail=[&](tlm::tlm_response_status status){tx.set_response_status(status); return false;};
    if (!tx.is_read() && !tx.is_write()) return fail(tlm::TLM_COMMAND_ERROR_RESPONSE);
    if (!tx.get_data_ptr()) return fail(tlm::TLM_GENERIC_ERROR_RESPONSE);
    const unsigned n=tx.get_data_length();
    if ((n!=1 && n!=4) || tx.get_streaming_width()<n) return fail(tlm::TLM_BURST_ERROR_RESPONSE);
    if (tx.get_address()>7) return fail(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    if (auto* bytes=tx.get_byte_enable_ptr()) {
        if (!tx.get_byte_enable_length()) return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        // Common Spec Full strobes override the older APB3 endpoint profile.
        for (unsigned i=0; i<n; ++i)
            if (bytes[i%tx.get_byte_enable_length()]!=255 && bytes[i%tx.get_byte_enable_length()]!=0) return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    }
    return true;
}
void Uart::execute(tlm::tlm_generic_payload& tx, bool side_effects) {
    const bool enabled=!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[0];
    if (tx.is_write()) {if(enabled)write(unsigned(tx.get_address()),tx.get_data_ptr()[0]);}
    else {
        const auto value=enabled?read(unsigned(tx.get_address()),side_effects):0;
        if(enabled)tx.get_data_ptr()[0]=std::uint8_t(value);
        for (unsigned i=1; i<tx.get_data_length(); ++i)
            if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])tx.get_data_ptr()[i]=0;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
void Uart::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    const auto incoming=delay; delay=sc_core::SC_ZERO_TIME; wait(incoming);
    struct Guard {
        sc_core::sc_mutex& mutex;
        explicit Guard(sc_core::sc_mutex& m):mutex(m){mutex.lock();}
        ~Guard(){mutex.unlock();}
    } guard(access_);
    if (!validate(tx)) {wait(config_.clock_period); return;}
    pending_=&tx;
    try {wait(completed_);} catch (...) {if (pending_==&tx) pending_=nullptr; throw;}
}
unsigned Uart::debug(tlm::tlm_generic_payload& tx) {
    if (!validate(tx)) return 0;
    if (!tx.is_read()) {tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return 0;}
    // Deliberate non-destructive debugger view; unlike normal MMIO it never pops or clears.
    execute(tx,false); return tx.get_data_length();
}
} // namespace fu3_uart
