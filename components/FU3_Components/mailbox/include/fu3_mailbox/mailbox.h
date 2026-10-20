#pragma once
// FU3 MAILBOX: 1 khối, 2 cổng APB (MAILBOX_CVA6, MAILBOX_IBEX), 2 FIFO 32-bit hai chiều
// Bus đưa vào offset cục bộ của từng cổng
#include <cstdint>
#include <deque>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "fu3_mailbox/registers.h"

namespace fu3_mailbox {

enum Port : unsigned { kCva6 = 0, kIbex = 1 };

class Mailbox : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Mailbox> cva6_socket{"cva6_socket"};   // MAILBOX_CVA6, PERIBUS_1
    tlm_utils::simple_target_socket<Mailbox> ibex_socket{"ibex_socket"};   // MAILBOX_IBEX, PERIBUS_0
    sc_core::sc_in<bool> reset_n{"reset_n"};

    // Mức cao khi IRQ_PENDING != 0. irq_ibex -> INTMAP irq_fast_i[14]
    // irq_cva6: HAS chưa nối vào đâu (PLIC không có source cho mailbox)
    sc_core::sc_out<bool> irq_cva6{"irq_cva6"};
    sc_core::sc_out<bool> irq_ibex{"irq_ibex"};

    SC_HAS_PROCESS(Mailbox);
    explicit Mailbox(sc_core::sc_module_name name, unsigned depth = FU3_MAILBOX_DEPTH);

    // Đọc không side effect: FIFO_RD trả word đầu mà không lấy ra
    std::uint32_t read_register(Port port, std::uint32_t offset) const;

private:
    struct Side {
        std::uint32_t wr_threshold = 0, rd_threshold = 0, irq_status = 0, irq_enable = 0;
        bool send_active = false, recv_active = false;   // Giá trị trước, để bắt sườn lên
    };

    const unsigned depth_;
    std::deque<std::uint32_t> fifo_[2];   // fifo_[p]: FIFO gửi của cổng p = FIFO nhận của cổng kia
    Side side_[2];
    sc_core::sc_event changed_;

    std::uint32_t status(Port p) const;
    void refresh();                       // Bắt sườn threshold và báo update
    void reset_state();
    void on_reset();
    void update();                        // Process duy nhất lái irq
    void b_transport_cva6(tlm::tlm_generic_payload& tx, sc_core::sc_time&) { access(kCva6, tx, false); }
    void b_transport_ibex(tlm::tlm_generic_payload& tx, sc_core::sc_time&) { access(kIbex, tx, false); }
    unsigned dbg_cva6(tlm::tlm_generic_payload& tx) { return access(kCva6, tx, true); }
    unsigned dbg_ibex(tlm::tlm_generic_payload& tx) { return access(kIbex, tx, true); }
    unsigned access(Port p, tlm::tlm_generic_payload& tx, bool debug);
};

}  // namespace fu3_mailbox
