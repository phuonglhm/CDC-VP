#include "fu3_mailbox/mailbox.h"

namespace fu3_mailbox {
namespace {

constexpr std::uint32_t kWindow = 0x1'0000;   // Cửa sổ 64 KiB mỗi cổng 

}  // namespace

Mailbox::Mailbox(sc_core::sc_module_name name, unsigned depth) : sc_core::sc_module(name), depth_(depth) {
    cva6_socket.register_b_transport(this, &Mailbox::b_transport_cva6);
    cva6_socket.register_transport_dbg(this, &Mailbox::dbg_cva6);
    ibex_socket.register_b_transport(this, &Mailbox::b_transport_ibex);
    ibex_socket.register_transport_dbg(this, &Mailbox::dbg_ibex);
    latch_thresholds();

    SC_METHOD(on_reset);
    sensitive << presetn0 << presetn1;

    // IRQ chỉ được lái từ đây, BUS THREAD chỉ notify changed_
    SC_METHOD(update);
    sensitive << changed_ << presetn0 << presetn1;
}

void Mailbox::latch_thresholds() {
    // Chốt mức threshold để reset không tạo sườn lên giả
    for (unsigned p = 0; p < 2; ++p) {
        side_[p].send_active = fifo_[p].size() <= side_[p].wr_threshold;
        side_[p].recv_active = fifo_[1 - p].size() > side_[p].rd_threshold;
    }
}

void Mailbox::on_reset() {
    // Reset cổng P, xoá thanh ghi của cổng P
    // FIFO không giữ dữ liệu qua reset endpoint => sườn xuống reset của cổng nào đều xoá cả 2 FIFO
    bool asserted = false;
    for (Port p : {kCva6, kIbex}) {
        const bool on = out_of_reset(p);
        if (!on) side_[p] = Side{};
        asserted |= !on && last_rst_n_[p];
        last_rst_n_[p] = on;
    }
    if (asserted)
        for (auto& f : fifo_) f.clear();
    latch_thresholds();
    changed_.notify(sc_core::SC_ZERO_TIME);
}

std::uint32_t Mailbox::status(Port p) const {
    std::uint32_t s = 0;
    if (fifo_[1 - p].empty()) s |= FU3_MAILBOX_STATUS_EMPTY;
    if (fifo_[p].size() >= depth_) s |= FU3_MAILBOX_STATUS_FULL;
    if (fifo_[p].size() <= side_[p].wr_threshold) s |= FU3_MAILBOX_STATUS_SEND_THRESHOLD;
    if (fifo_[1 - p].size() > side_[p].rd_threshold) s |= FU3_MAILBOX_STATUS_RECV_THRESHOLD;
    return s;
}

void Mailbox::refresh() {
    for (unsigned p = 0; p < 2; ++p) {
        Side& s = side_[p];
        const std::uint32_t st = status(Port(p));
        const bool send = st & FU3_MAILBOX_STATUS_SEND_THRESHOLD;
        const bool recv = st & FU3_MAILBOX_STATUS_RECV_THRESHOLD;
        // Cổng đang reset giữ thanh ghi ở giá trị reset: chỉ chốt mức, không ghi sự kiện
        if (!out_of_reset(Port(p))) {
            s.send_active = send;
            s.recv_active = recv;
            continue;
        }
        if (send && !s.send_active) s.irq_status |= FU3_MAILBOX_IRQ_WR_THRESHOLD;
        if (recv && !s.recv_active) s.irq_status |= FU3_MAILBOX_IRQ_RD_THRESHOLD;
        s.send_active = send;
        s.recv_active = recv;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Mailbox::update() {
    irq_cva6.write(out_of_reset(kCva6) && (side_[kCva6].irq_status & side_[kCva6].irq_enable));
    irq_ibex.write(out_of_reset(kIbex) && (side_[kIbex].irq_status & side_[kIbex].irq_enable));
}

std::uint32_t Mailbox::read_register(Port p, std::uint32_t offset) const {
    const Side& s = side_[p];
    switch (offset) {
        case FU3_MAILBOX_FIFO_RD_OFFSET:       
            return fifo_[1 - p].empty() ? 0 : fifo_[1 - p].front();
        case FU3_MAILBOX_STATUS_OFFSET:        
            return status(p);
        case FU3_MAILBOX_WR_THRESHOLDS_OFFSET: 
            return s.wr_threshold;
        case FU3_MAILBOX_RD_THRESHOLDS_OFFSET: 
            return s.rd_threshold;
        case FU3_MAILBOX_IRQ_STATUS_OFFSET:    
            return s.irq_status;
        case FU3_MAILBOX_IRQ_ENABLE_OFFSET:    
            return s.irq_enable;
        case FU3_MAILBOX_IRQ_PENDING_OFFSET:   
            return s.irq_status & s.irq_enable;
        default:                              
            return 0;
    }
}

unsigned Mailbox::access(Port p, tlm::tlm_generic_payload& tx, bool debug) {
    const std::uint64_t addr = tx.get_address();
    unsigned char* data = tx.get_data_ptr();
    const unsigned char* be = tx.get_byte_enable_ptr();
    const unsigned be_len = tx.get_byte_enable_length();
    bool all_lanes = true;
    for (unsigned i = 0; be && be_len && i < 4; ++i) 
        all_lanes &= be[i % be_len] != TLM_BYTE_DISABLED;

    // Chỉ truy cập 32-bit thẳng hàng, đủ PSTRB
    // Còn lại là truy cập không hợp lệ -> PSLVERR
    if (tx.get_data_length() != 4 || addr % 4 || addr >= kWindow || !all_lanes) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return 0;
    }
    const std::uint32_t offset = static_cast<std::uint32_t>(addr);
    Side& s = side_[p];
    auto& tx_fifo = fifo_[p];
    auto& rx_fifo = fifo_[1 - p];
    bool ok = true;

    if (debug) {
        // Backdoor chỉ đọc, không lấy word khỏi FIFO, không set cờ lỗi
        if (!tx.is_read()) 
            return 0;
        const std::uint32_t v = read_register(p, offset);
        for (unsigned i = 0; i < 4; ++i) 
            data[i] = (v >> (8 * i)) & 0xFF;
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        return 4;
    }

    if (!out_of_reset(p)) {
        // Cổng đang giữ reset: đọc 0, ghi bỏ qua
        if (tx.is_read()) for (unsigned i = 0; i < 4; ++i) data[i] = 0;
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        return 4;
    }

    if (tx.is_read()) {
        std::uint32_t v = 0;
        switch (offset) {
            case FU3_MAILBOX_FIFO_RD_OFFSET:
                if (rx_fifo.empty()) {
                    s.irq_status |= FU3_MAILBOX_IRQ_FIFO_RD_ERROR;
                    ok = false;
                } else {
                    v = rx_fifo.front();
                    rx_fifo.pop_front();
                }
                break;
            case FU3_MAILBOX_STATUS_OFFSET:
            case FU3_MAILBOX_WR_THRESHOLDS_OFFSET:
            case FU3_MAILBOX_RD_THRESHOLDS_OFFSET:
            case FU3_MAILBOX_IRQ_STATUS_OFFSET:
            case FU3_MAILBOX_IRQ_ENABLE_OFFSET:
            case FU3_MAILBOX_IRQ_PENDING_OFFSET:
                v = read_register(p, offset);
                break;
            default:   // FIFO_WR (WO) và offset không map
                ok = false;
                break;
        }
        for (unsigned i = 0; i < 4; ++i) data[i] = (v >> (8 * i)) & 0xFF;
    } else if (tx.is_write()) {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) 
            v |= std::uint32_t(data[i]) << (8 * i);
        switch (offset) {
            case FU3_MAILBOX_FIFO_WR_OFFSET:
                if (tx_fifo.size() >= depth_) {
                    s.irq_status |= FU3_MAILBOX_IRQ_FIFO_WR_ERROR;
                    ok = false;
                } else {
                    tx_fifo.push_back(v);
                }
                break;
            case FU3_MAILBOX_WR_THRESHOLDS_OFFSET: 
                s.wr_threshold = v; 
                break;
            case FU3_MAILBOX_RD_THRESHOLDS_OFFSET: 
                s.rd_threshold = v; 
                break;
            case FU3_MAILBOX_IRQ_STATUS_OFFSET:    
                s.irq_status &= ~(v & FU3_MAILBOX_IRQ_MASK); 
                break;
            case FU3_MAILBOX_IRQ_ENABLE_OFFSET:    
                s.irq_enable = v & FU3_MAILBOX_IRQ_MASK; 
                break;
            default:   // FIFO_RD, STATUS, IRQ_PENDING (RO) và offset không map
                ok = false;
                break;
        }
    } else {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }
    
    refresh();
    tx.set_response_status(ok ? tlm::TLM_OK_RESPONSE : tlm::TLM_GENERIC_ERROR_RESPONSE);
    return 4;
}

}  // namespace fu3_mailbox
