#include "fu3/qspi/qspi_controller.h"
#include <algorithm>
#include <stdexcept>

namespace fu3::qspi {
namespace {
unsigned lines(unsigned mode) { return mode ? 1u << (mode-1) : 0; }
std::uint32_t unpack(const unsigned char* p) {
    std::uint32_t v=0;
    for (unsigned i=0;i<4;++i) v |= std::uint32_t(p[i])<<(8*i);
    return v;
}
void pack(unsigned char* p, std::uint32_t v) {
    for (unsigned i=0;i<4;++i) p[i]=static_cast<unsigned char>(v>>(8*i));
}
}
void Config::validate() const {
    auto depth=[](unsigned n) { return n==4 || n==8 || n==16 || n==32; };
    auto async_depth=[](unsigned n) { return n==2 || n==4 || n==8 || n==16; };
    if (!depth(tx_fifo_words) || !depth(rx_fifo_words) ||
        bus_timeout_cycles<16 || bus_timeout_cycles>1048576 ||
        !mem_cache_count || mem_cache_count>16 || !prefetch_length || prefetch_length>mem_cache_count ||
        !async_depth(apb_async_fifo_depth) || !async_depth(ahb_async_fifo_depth))
        throw std::invalid_argument("QSPI parameters outside HDS Table 5-1");
}
QspiController::QspiController(sc_core::sc_module_name name, Config config) : sc_module(name),
    cfg_([&] { config.validate(); return config; }()), fifo_(cfg_.tx_fifo_words,cfg_.rx_fifo_words), cache_(cfg_.mem_cache_count) {
    ap_qspic.register_b_transport(this, &QspiController::ap_transport);
    ap_qspic.register_transport_dbg(this, &QspiController::debug);
    xip.register_b_transport(this, &QspiController::xip_transport);
    SC_METHOD(tick);
    sensitive << clk.pos() << reset_n.neg();
    SC_METHOD(ahb_tick);
    sensitive << hclk.pos();
    SC_METHOD(dma_tick);
    sensitive << dma_clk.pos() << dma_reset_n.neg() << reset_n.neg();
    SC_METHOD(interface_reset);
    sensitive << preset_n.neg() << hreset_n.neg();
    SC_METHOD(outputs);
    sensitive << changed_;
}
std::uint64_t QspiController::flash_size() const {
    return r_.flash_size();
}
std::uint32_t QspiController::status() const {
    return r_[reg::SR/4] | (busy_ ? reg::BUSY : 0) |
        fifo_.levels();
}
void QspiController::outputs() {
    irq.write(interrupt_.evaluate(r_[reg::CR/4],r_[reg::SR/4],r_[reg::DMACR/4],r_[reg::DMASR/4]));
}
void QspiController::flush_cache() {
    for (auto& line:cache_) line.valid=false;
    cache_fill_enabled_=false;
}
void QspiController::dma_tick() {
    if (!reset_n.read() || !dma_reset_n.read()) {
        rx_sync_={}; tx_sync_={};
        rx_clear_pending_=tx_clear_pending_=rx_clear_ack_=tx_clear_ack_=false;
        dma_seen_generation_=dma_generation_;
        dma_rx_req.write(false); dma_tx_req.write(false);
        return;
    }
    if (!dma_clk.posedge()) return;
    if (dma_inhibited_ || dma_seen_generation_!=dma_generation_) {
        rx_sync_={}; tx_sync_={};
        rx_clear_pending_=tx_clear_pending_=rx_clear_ack_=tx_clear_ack_=false;
        dma_seen_generation_=dma_generation_;
        dma_rx_req.write(false); dma_tx_req.write(false);
        return;
    }
    const bool rx=rx_sync_[1],tx=tx_sync_[1];
    rx_sync_[1]=rx_sync_[0]; rx_sync_[0]=dma_.rx_request();
    tx_sync_[1]=tx_sync_[0]; tx_sync_[0]=dma_.tx_request();
    if (dma_rx_clr.read()) { rx_clear_pending_=true; rx_clear_ack_=false; }
    if (dma_tx_clr.read()) { tx_clear_pending_=true; tx_clear_ack_=false; }
    dma_rx_req.write(rx && !rx_clear_pending_);
    dma_tx_req.write(tx && !tx_clear_pending_);
    // Acknowledge only after the core has sampled the clear and recomputed
    // eligibility; stale synchronized conditions cannot re-arm a burst.
    if (!dma_rx_clr.read() && rx_clear_ack_) {
        rx_clear_pending_=rx_clear_ack_=false;
        rx_sync_={}; // Discard the condition sampled before the burst finished.
    }
    if (!dma_tx_clr.read() && tx_clear_ack_) {
        tx_clear_pending_=tx_clear_ack_=false;
        tx_sync_={};
    }
}
void QspiController::ahb_tick() {
    if (!hclk.posedge() || !hreset_n.read() || !xip_pending_) return;
    if (!reset_n.read()) return; // An unavailable core is handled by the HCLK timeout.
    if (!(r_[0]&reg::EN) || mode()!=3) {
        // A frame already accepted is allowed to finish after EN is cleared.
        if (op_!=Operation::Xip) finish_xip(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    const auto address=xip_pending_->get_address();
    if (address>=flash_size()) { finish_xip(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    const auto aligned=static_cast<std::uint32_t>(address)&~3u;
    for (const auto& line:cache_) if (line.valid && line.address==aligned) {
        pack(xip_pending_->get_data_ptr(),line.value);
        finish_xip(tlm::TLM_OK_RESPONSE);
        return;
    }
    if (op_==Operation::None && !busy_) xip_start_=true;
}
void QspiController::finish_xip(tlm::tlm_response_status response) {
    if (xip_pending_) {
        xip_pending_->set_response_status(response);
        xip_pending_=nullptr;
        xip_start_=false;
        xip_done_.notify(sc_core::SC_ZERO_TIME);
    }
}
void QspiController::interface_reset() {
    if (!preset_n.read()) {
        ++p_epoch_;
        if (ap_pending_) {
            ap_pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            ap_pending_=nullptr;
            ap_done_.notify(sc_core::SC_ZERO_TIME);
        }
    }
    if (!hreset_n.read()) {
        ++h_epoch_;
        flush_cache();
        if (op_==Operation::Xip) stop(false);
        finish_xip(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}
bool QspiController::validate(tlm::tlm_generic_payload& tx, bool mm) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write())
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    else if (tx.get_data_length()!=4 || tx.get_streaming_width()<4)
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
    else if (!tx.get_data_ptr())
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    else if (tx.get_address()>(mm ? 0xffffffffULL : 0x3fULL))
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    else {
        if (tx.get_byte_enable_ptr()) {
            const unsigned n=tx.get_byte_enable_length();
            if (!n) {
                tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                return false;
            }
            for (unsigned i=0;i<4;++i) if (tx.get_byte_enable_ptr()[i%n]!=0xff) {
                tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                return false;
            }
        }
        return true;
    }
    return false;
}
void QspiController::ap_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) { transport(tx,delay,false); }
void QspiController::xip_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) { transport(tx,delay,true); }
void QspiController::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay, bool mm) {
    if (!validate(tx,mm)) {
        if (tx.is_read() && tx.get_response_status()==tlm::TLM_ADDRESS_ERROR_RESPONSE && tx.get_data_ptr())
            pack(tx.get_data_ptr(),0);
        return;
    }
    const auto* control=tx.get_extension<AhbTransaction>();
    if (mm && control && (!control->selected || control->transfer<AhbTransaction::Nonseq)) {
        tx.set_response_status(tlm::TLM_OK_RESPONSE); return;
    }
    auto& lock=mm ? xip_lock_ : ap_lock_;
    auto& reset=mm ? hreset_n : preset_n;
    auto& busclk=mm ? hclk : pclk;
    auto& pending=mm ? xip_pending_ : ap_pending_;
    auto& done=mm ? xip_done_ : ap_done_;
    const auto be=mm ? h_epoch_ : p_epoch_;
    auto alive=[&] { return reset.read() && be==(mm?h_epoch_:p_epoch_); };
    if (delay!=sc_core::SC_ZERO_TIME) { sc_core::wait(delay); delay=sc_core::SC_ZERO_TIME; }
    lock.lock();
    if (alive()) {
        sc_core::wait(busclk.posedge_event() | reset.negedge_event() | reset_n.negedge_event());
        if (alive()) {
            pending=&tx;
            tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            if (mm && tx.is_write()) finish_xip(tlm::TLM_COMMAND_ERROR_RESPONSE);
            unsigned elapsed=0;
            while (pending && alive() && (!mm || elapsed<cfg_.bus_timeout_cycles)) {
                sc_core::wait(done | busclk.posedge_event() | reset.negedge_event() | reset_n.negedge_event());
                if (busclk.posedge()) ++elapsed;
            }
            if (pending) {
                pending=nullptr;
                if (mm) { if (op_==Operation::Xip) stop(false); flush_cache(); xip_start_=false; }
                tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            }
            if (!alive()) tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            if (mm && tx.is_response_error() && alive()) {
                // Two HCLK response cycles (ERROR/wait then ERROR/ready).
                sc_core::wait(busclk.posedge_event() | reset.negedge_event());
            }
        } else tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    } else tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    lock.unlock();
}
unsigned QspiController::debug(tlm::tlm_generic_payload& tx) {
    if (!validate(tx,false)) return 0;
    if (!tx.is_read()) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return 0; }
    access(tx,false);
    return 4;
}
void QspiController::access(tlm::tlm_generic_payload& tx, bool effects) {
    const auto off=static_cast<unsigned>(tx.get_address())&~3u;
    if (tx.is_read()) pack(tx.get_data_ptr(),read(off,effects));
    else if (effects) write(off,unpack(tx.get_data_ptr()));
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
std::uint32_t QspiController::read(unsigned off, bool effects) {
    if (off==reg::SR) return status();
    if (off==reg::FCR || off==reg::DMAFCR) return 0;
    if (off!=reg::DR) return r_.read(off);
    if (mode()==2) return last_status_;
    if (mode()!=1) return 0;
    if (fifo_.rx.empty()) {
        if (effects) r_[reg::DMASR/4] |= reg::RXUDRF;
        return 0;
    }
    std::uint32_t value=0;
    const unsigned n=std::min<std::size_t>(4,fifo_.rx.size());
    for (unsigned i=0;i<n;++i) value |= std::uint32_t(fifo_.rx[i])<<(8*i);
    if (effects) {
        for (unsigned i=0;i<n;++i) fifo_.rx.pop_front();
        if (serial_done_ && fifo_.rx.empty()) busy_=false;
    }
    return value;
}
void QspiController::write(unsigned off, std::uint32_t value) {
    if (off==reg::DR) {
        if (mode()!=0 || abort_pending_) return;
        if ((r_[0]&reg::EN) && !busy_ && data_mode()) start(Operation::Write,r_[reg::AR/4]);
        if (!fifo_.tx.push_word(value,4)) { r_[reg::DMASR/4] |= reg::TXOVRF; return; }
        if (op_==Operation::Write) accepted_=std::min(total_,accepted_+4);
        return;
    }
    const auto old=r_.read(off);
    if (!r_.write(off,value,busy_)) return;
    if ((off==reg::DCR && ((old^r_.read(off))&0x1f0000)) || off==reg::CCR ||
        (off==reg::ABR && old!=r_.read(off))) flush_cache();
    if (off==reg::CR) {
        if (value&reg::ABORT) request_abort();
        if (!(value&reg::EN)) flush_cache();
        return;
    }
    if (off==reg::DMACR) return;
    if (off==reg::CCR) {
        instruction_sent_=false; serial_done_=false; dma_inhibited_=false; dma_.arm();
        accepted_=0;
    }
    if (off==reg::AR || off==reg::DLR) { accepted_=0; serial_done_=false; }
    if (!(r_[0]&reg::EN) || mode()==3 || abort_pending_) return;
    const bool address_phase=((r_[reg::CCR/4]>>10)&3)!=0;
    if ((off==reg::CCR && !address_phase) || (off==reg::AR && address_phase)) {
        if (mode()==0 && !data_mode()) start(Operation::Write,r_[reg::AR/4]);
        if (mode()==1) start(Operation::Read,r_[reg::AR/4]);
        if (mode()==2) start(Operation::Poll,r_[reg::AR/4]);
    }
}

bool QspiController::legal(bool mm) const {
    const auto c=r_[reg::CCR/4];
    if (!QspiTimingModel::legal_divider(r_[0],(c&reg::DDRM)!=0)) return false;
    if (mode()==2 && (r_[reg::DLR/4]>3 || !data_mode())) return false;
    if (mm && (!data_mode() || !((c>>10)&3))) return false;
    return true; // HDS Table 6-11: an all-skipped frame proceeds to STOP.
}
void QspiController::start(Operation operation, std::uint32_t address) {
    const bool mm=operation==Operation::Xip;
    const auto c=r_[reg::CCR/4];
    address_=((c>>10)&3) ? address : 0;
    if (mm) {
        xip_base_=address_;
        const auto* control=xip_pending_->get_extension<AhbTransaction>();
        const unsigned words=control && control->transfer==AhbTransaction::Seq ? cfg_.prefetch_length : 1;
        total_=std::min<std::uint64_t>(words*4,(flash_size()-address_+3)/4*4);
    } else total_=data_mode() ? std::uint64_t(r_[reg::DLR/4])+1 : 0;
    if (!mm && mode()!=2 && data_mode() && r_[reg::DLR/4]==0xffffffffu)
        total_=address_<flash_size() ? flash_size()-address_ : 0;
    const bool address_phase=((c>>10)&3)!=0;
    const bool range_error=!mm && address_phase &&
        (address_>=flash_size() || (data_mode() && r_[reg::DLR/4]!=0xffffffffu && total_>flash_size()-address_));
    if (!legal(mm) || range_error) {
        r_[reg::SR/4] |= reg::TEF;
        if (mm) finish_xip(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    // Range checks use full AR. Only the selected low bytes go on the wire.
    const unsigned address_bits=8*(((c>>12)&3)+1);
    if (address_bits<32) address_&=(1u<<address_bits)-1;
    if (mm) { flush_cache(); cache_fill_enabled_=true; }
    op_=operation;
    dma_.arm(!dma_inhibited_);
    busy_=true;
    serial_done_=false;
    transferred_=0;
    accepted_=operation==Operation::Write ? std::min<std::uint64_t>(total_,fifo_.tx.size()) : 0;
    chunk_pending_=backend_called_=false;
    timing_.poll_start=timing_.cycles;
    ++frame_id_;
}
std::uint64_t QspiController::phase_cycles(unsigned bytes, unsigned lane_count, bool ddr) const {
    return QspiTimingModel::phase_cycles(bytes,lane_count,ddr,r_[0]);
}
void QspiController::schedule_chunk() {
    chunk_size_=static_cast<unsigned>(std::min<std::uint64_t>(4,total_-transferred_));
    if (op_==Operation::Write && fifo_.tx.size()<chunk_size_) return;
    if (op_==Operation::Read && fifo_.rx.size()+chunk_size_>fifo_.rx.capacity()) return;
    chunk_.fill(0);
    if (op_==Operation::Write && chunk_size_) for (unsigned i=0;i<4;++i) {
        if (i<chunk_size_) chunk_[i]=fifo_.tx.front();
        fifo_.tx.pop_front(); // The unused lanes of the final word are discarded.
    }
    const auto c=r_[reg::CCR/4];
    const bool ddr=(c&reg::DDRM)!=0;
    std::uint64_t duration=0;
    byte_boundaries_.clear();
    auto phase=[&](unsigned bytes,unsigned width,bool rate) {
        for (unsigned i=0;width && i<bytes;++i) {
            duration+=phase_cycles(1,width,rate);
            byte_boundaries_.push_back(timing_.cycles+duration);
        }
    };
    if (!transferred_) {
        if (!(c&reg::SIOO) || !instruction_sent_) phase(1,lines((c>>8)&3),false);
        phase(((c>>12)&3)+1,lines((c>>10)&3),ddr);
        phase(((c>>16)&3)+1,lines((c>>14)&3),ddr);
        const auto period=(r_[0]>>24)+2;
        for (unsigned i=0;i<((c>>18)&31);++i) {
            duration+=period;
            byte_boundaries_.push_back(timing_.cycles+duration);
        }
    }
    phase(chunk_size_,lines(data_mode()),ddr);
    timing_.ready_cycle=timing_.cycles+std::max<std::uint64_t>(1,duration);
    chunk_pending_=true;
    backend_called_=false;
}
bool QspiController::send_chunk(bool cancel) {
    if (!flash.size()) return false;
    const auto c=r_[reg::CCR/4];
    FlashTransaction meta;
    meta.frame=frame_id_; meta.offset=transferred_; meta.length=total_;
    meta.address=address_; meta.alternate=r_[reg::ABR/4];
    const unsigned alternate_bits=8*(((c>>16)&3)+1);
    if (alternate_bits<32) meta.alternate&=(1u<<alternate_bits)-1;
    meta.instruction=static_cast<std::uint8_t>(c);
    meta.begin=transferred_==0; meta.end=transferred_+chunk_size_==total_;
    meta.cancel=cancel;
    meta.instruction_lines=meta.begin && (!(c&reg::SIOO)||!instruction_sent_) ? lines((c>>8)&3) : 0;
    meta.address_lines=lines((c>>10)&3); meta.address_bytes=meta.address_lines?((c>>12)&3)+1:0;
    meta.alternate_lines=lines((c>>14)&3); meta.alternate_bytes=meta.alternate_lines?((c>>16)&3)+1:0;
    meta.data_lines=lines(data_mode()); meta.dummy_cycles=(c>>18)&31;
    meta.ddr=(c&reg::DDRM)!=0; meta.clock_mode3=(r_[reg::DCR/4]&1)!=0;
    if (cancel) { meta.begin=false; meta.end=true; }
    tlm::tlm_generic_payload tx;
    tx.set_command(cancel || !chunk_size_ ? tlm::TLM_IGNORE_COMMAND :
        (op_==Operation::Write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND));
    tx.set_address(std::uint64_t(address_)+transferred_);
    tx.set_data_ptr(chunk_.data());
    tx.set_data_length(cancel ? 0 : chunk_size_);
    tx.set_streaming_width(cancel ? 0 : chunk_size_);
    tx.set_extension(&meta);
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    try { flash->b_transport(tx,delay); }
    catch (...) { tx.clear_extension<FlashTransaction>(); return false; }
    tx.clear_extension<FlashTransaction>();
    if (!tx.is_response_ok()) return false;
    if (!cancel) {
        instruction_sent_=true;
        frame_open_=!meta.end;
        timing_.backend_ready=sc_core::sc_time_stamp()+delay;
    }
    return true;
}
void QspiController::request_abort() {
    if (abort_pending_) return;
    flush_cache(); dma_inhibited_=true; dma_.reset();
    ++dma_generation_;
    abort_pending_=true; busy_=true;
    abort_cycle_=timing_.cycles;
    if (chunk_pending_) {
        for (auto boundary:byte_boundaries_) if (boundary>=timing_.cycles) {
            abort_cycle_=boundary; break;
        }
    }
    abort_cycle_+=QspiTimingModel::cs_high(r_[0],r_[reg::DCR/4]);
}
void QspiController::stop(bool abort, bool error) {
    if (frame_open_) send_chunk(true);
    frame_open_=false;
    if (op_==Operation::Xip || abort) finish_xip(tlm::TLM_GENERIC_ERROR_RESPONSE);
    op_=Operation::None;
    busy_=serial_done_=chunk_pending_=backend_called_=false;
    abort_pending_=completion_pending_=false; xip_start_=false;
    fifo_.reset();
    dma_.reset();
    if (abort) dma_inhibited_=true;
    r_[0] &= ~reg::ABORT;
    if (abort) r_[reg::SR/4] |= reg::TCF;
    if (error) r_[reg::SR/4] |= reg::TEF;
    timing_.ready_cycle=timing_.cycles+QspiTimingModel::cs_high(r_[0],r_[reg::DCR/4]);
}
void QspiController::complete_chunk() {
    chunk_pending_=false;
    if (op_==Operation::Read) for (unsigned i=0;i<chunk_size_;++i) fifo_.rx.push_back(chunk_[i]);
    transferred_+=chunk_size_;
    if (op_==Operation::Xip) {
        const auto word_address=static_cast<std::uint32_t>(std::uint64_t(xip_base_)+transferred_-chunk_size_);
        const auto index=static_cast<unsigned>((transferred_-chunk_size_)/4);
        if (cache_fill_enabled_ && index<cache_.size()) cache_[index]={word_address,unpack(chunk_.data()),true};
        if (xip_pending_ && (xip_pending_->get_address()&~3ULL)==word_address) {
            pack(xip_pending_->get_data_ptr(),unpack(chunk_.data()));
            finish_xip(tlm::TLM_OK_RESPONSE);
        }
    }
    if (transferred_<total_) return;
    timing_.ready_cycle=timing_.cycles+QspiTimingModel::cs_high(r_[0],r_[reg::DCR/4]);
    if (op_==Operation::Xip) {
        completion_pending_=true;
    } else if (op_==Operation::Poll) {
        last_status_=unpack(chunk_.data());
        const auto valid=total_==4 ? 0xffffffffu : (std::uint32_t{1}<<(8*total_))-1;
        const auto mask=r_[reg::PSMKR/4]&valid;
        const auto different=(last_status_^r_[reg::PSMAR/4])&mask;
        const bool match=(r_[0]&reg::PMM) ? ((~different&mask)!=0) : different==0;
        if (match) r_[reg::SR/4] |= reg::SMF;
        const bool expired=(r_[0]&reg::PTCEN) && timing_.poll_expired(r_[reg::PTR/4]);
        if (expired) r_[reg::SR/4] |= reg::STOF; // DEC-024: SMF and STOF can coincide.
        if (expired || (match && (r_[0]&reg::APMS)) || !(r_[0]&reg::EN)) {
            completion_pending_=true;
        } else {
            timing_.ready_cycle=timing_.cycles+QspiTimingModel::poll_gap(r_[0],r_[reg::DCR/4],r_[reg::PIR/4]);
            transferred_=0;
            ++frame_id_;
        }
    } else {
        completion_pending_=true;
    }
}
void QspiController::advance() {
    if (abort_pending_) {
        if (timing_.cycles>=abort_cycle_ && !dma_rx_req.read() && !dma_tx_req.read()) stop(true);
        return;
    }
    if (completion_pending_) {
        if (timing_.cycles<timing_.ready_cycle) return;
        if (op_==Operation::Read || op_==Operation::Write) {
            r_[reg::SR/4] |= reg::TCF; serial_done_=true;
        }
        busy_=op_==Operation::Read && !fifo_.rx.empty();
        completion_pending_=false; op_=Operation::None;
        return;
    }
    if (op_==Operation::None) return;
    if (chunk_pending_) {
        if (timing_.cycles<timing_.ready_cycle) return;
        if (!backend_called_) {
            if (!send_chunk()) { stop(false,op_!=Operation::Xip); return; }
            backend_called_=true;
        }
        if (timing_.backend_due()) complete_chunk();
    } else {
        if (op_==Operation::Poll && !(r_[0]&reg::EN)) {
            busy_=false; op_=Operation::None;
        } else if (op_==Operation::Poll && (r_[0]&reg::PTCEN) &&
            timing_.poll_expired(r_[reg::PTR/4]) && timing_.cycles!=timing_.poll_start) {
            r_[reg::SR/4] |= reg::STOF; busy_=false; op_=Operation::None;
        } else if (timing_.cycles>=timing_.ready_cycle) schedule_chunk();
    }
}
void QspiController::tick() {
    if (!reset_n.read()) {
        auto* waiting=xip_pending_; xip_pending_=nullptr;
        stop(false); xip_pending_=waiting;
        r_.reset(); last_status_=0; instruction_sent_=false; dma_inhibited_=false;
        flush_cache(); timing_.reset();
        // APB waits through core reset. AHB times out if the core cannot answer.
    } else if (clk.posedge()) {
        timing_.tick();
        // Reassert threshold flags on a subsequent core edge after W1C.
        if (mode()==0 && fifo_.tx_threshold((r_[0]>>8)&31)) r_[reg::SR/4] |= reg::TXFTF;
        if (mode()==1 && fifo_.rx_threshold((r_[0]>>3)&31)) r_[reg::SR/4] |= reg::RXFTF;
        if (ap_pending_) {
            if (preset_n.read()) access(*ap_pending_,true);
            else ap_pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            ap_pending_=nullptr; ap_done_.notify(sc_core::SC_ZERO_TIME);
        }
        if (xip_start_ && xip_pending_ && op_==Operation::None && !busy_) {
            xip_start_=false;
            if (hreset_n.read() && (r_[0]&reg::EN) && mode()==3)
                start(Operation::Xip,static_cast<std::uint32_t>(xip_pending_->get_address())&~3u);
        }
        advance();
        QspiDmaInterface::Input input;
        input.cr=r_[0]; input.dmacr=r_[reg::DMACR/4];
        input.mode=mode(); input.data_mode=data_mode();
        input.rx_bytes=fifo_.rx.size(); input.tx_free=fifo_.tx.free();
        input.total=busy_ ? total_ : std::uint64_t(r_[reg::DLR/4])+1;
        if (!busy_ && mode()==0 && r_[reg::DLR/4]==0xffffffffu) {
            const auto address=((r_[reg::CCR/4]>>10)&3) ? r_[reg::AR/4] : 0;
            input.total=address<flash_size() ? flash_size()-address : 0;
        }
        input.accepted=busy_ ? accepted_ : (serial_done_ ? input.total : fifo_.tx.size());
        input.rx_clear=rx_clear_pending_; input.tx_clear=tx_clear_pending_;
        if (dma_inhibited_) dma_.reset();
        if (rx_clear_pending_) rx_clear_ack_=true;
        if (tx_clear_pending_) tx_clear_ack_=true;
        dma_.tick(input);
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}
}
