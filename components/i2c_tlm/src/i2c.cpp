#include "../include/i2c.h"
#include <cstring>
#include <iostream>

i2c::i2c(sc_core::sc_module_name name, const i2c_config& cfg)
    : sc_core::sc_module(name), socket("socket"), irq("irq"), cfg_(cfg),
      bit_time_(sc_core::sc_time(1.0 / (cfg.scl_hz > 0 ? cfg.scl_hz : 400e3), sc_core::SC_SEC)) {
    socket.register_b_transport(this, &i2c::b_transport);
    // One driver of the interrupt line: level = intr_state & intr_enable.
    SC_METHOD(irq_method);
    sensitive << irq_ev_;
    if (cfg_.functional_host) SC_THREAD(host_thread);
}

void i2c::irq_method() { irq.write((intr_state & intr_enable) != 0); }

void i2c::attach(uint8_t addr7, i2c_device* dev) { devices_[addr7 & 0x7F] = dev; }

void i2c::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& /*delay*/) {
    uint32_t offset = (uint32_t)trans.get_address();
    uint8_t* data = trans.get_data_ptr();
    // Registers are 32-bit: copy at most four bytes, never past the payload.
    const unsigned len = trans.get_data_length() < 4 ? trans.get_data_length() : 4;

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        uint32_t value = 0;
        std::memcpy(&value, data, len);
        write_reg(offset,value);
    } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        uint32_t value = read_reg(offset);
        std::memcpy(data, &value, len);
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

uint32_t i2c::read_reg(uint32_t offset) {
    switch(offset) {
        case I2C_INTR_STATE:
            return intr_state;
        case I2C_INTR_ENABLE:
            return intr_enable;
        case I2C_CTRL:
            return ctrl;
        case I2C_STATUS:
            return status;
        case I2C_RDATA:
            if (!rx_fifo.empty()) {
                uint8_t byte = rx_fifo.front();
                rx_fifo.pop();
                update_status();
                rx_ev_.notify(sc_core::SC_ZERO_TIME);  // a stretched read may continue
                return byte;
            } else {
                return 0;
            }
        case I2C_HOST_FIFO_STATUS: {
            uint32_t fmtlvl = fmt_fifo.size();
            uint32_t rxlvl = rx_fifo.size();
            uint32_t val = (rxlvl << 8) | fmtlvl;
            return val;
        }
        case I2C_TARGET_FIFO_STATUS: {
            uint32_t acqlvl = acq_fifo.size();
            uint32_t txlvl  = tx_fifo.size();
            uint32_t val    = (acqlvl << 8) | txlvl;
            return val;
        }
        case I2C_TARGET_ID:
            return target_id;

        case I2C_ACQDATA:
            if (!acq_fifo.empty()) {
                AcqEntry entry = acq_fifo.front();
                acq_fifo.pop();
                uint32_t val = ((uint32_t)entry.signal << 8) | entry.data;
                update_status();
                return val;
            }
            return 0;

        case I2C_TARGET_ACK_CTRL:
            return target_ack_ctrl;

        case I2C_TARGET_EVENTS:
            return target_events;
        default:
            return 0;
    }
}

void i2c::write_reg(uint32_t offset, uint32_t data) {
    switch(offset) {
        case I2C_INTR_STATE:
            intr_state &= ~data;
            irq_ev_.notify(sc_core::SC_ZERO_TIME);
            break;
        case I2C_INTR_ENABLE:
            intr_enable = data;
            irq_ev_.notify(sc_core::SC_ZERO_TIME);
            break;
        case I2C_CTRL:
            ctrl = data;
            fmt_ev_.notify(sc_core::SC_ZERO_TIME);  // functional host: enable resumes the queue
            break;
        case I2C_STATUS:
            break;
        case I2C_RDATA:
            break;
        case I2C_FDATA:
            if (!(ctrl & CTRL_ENABLEHOST)) {
                break;
            }
            handle_fdata(data);
            break;
        case I2C_FIFO_CTRL:
            fifo_ctrl = data;
            if (data & (1 << 0)) {
                while (!rx_fifo.empty()) rx_fifo.pop();
            }
            if (data & (1 << 1)) {
                while (!fmt_fifo.empty()) fmt_fifo.pop();
                discarding_ = false;
                // Functional host: an open transaction lost its remaining
                // commands, STOP included; it is ended (M5b review F2).
                if (cfg_.functional_host && host_busy_) abort_req_ = true;
                fmt_ev_.notify(sc_core::SC_ZERO_TIME);
            }
            rx_ev_.notify(sc_core::SC_ZERO_TIME);
            update_status();
            break;
        case I2C_TARGET_FIFO_CONFIG:
            target_fifo_config = data;
            break;

        case I2C_TARGET_ID:
            target_id = data;
            break;

        case I2C_TXDATA:
            if (!(ctrl & CTRL_ENABLETARGET)) {
                break;
            }
            if (tx_fifo.size() >= TX_FIFO_DEPTH) {
                break;
            }
            tx_fifo.push((uint8_t)(data & 0xFF));
            update_status();
            break;

        case I2C_TARGET_ACK_CTRL:
            target_ack_ctrl = data;
            break;

        case I2C_ACQDATA:
            break;

        case I2C_TARGET_EVENTS:
            target_events &= ~data;
            break;
        default:
            break;
    }
}

void i2c::handle_fdata(uint32_t value) {
    I2CCommand cmd;
    cmd.byte = value & FDATA_BYTE_MASK;
    cmd.start = (value & FDATA_START) != 0;
    cmd.stop = (value & FDATA_STOP) != 0;
    cmd.read = (value & FDATA_READB) != 0;
    cmd.rcont = (value & FDATA_RCONT) != 0;
    cmd.nakok = (value & FDATA_NAKOK) != 0;

    if (cfg_.functional_host) {
        if (fmt_fifo.size() >= FMT_FIFO_DEPTH) return;  // FMT FIFO full: the command is dropped
        fmt_fifo.push(cmd);
        update_status();
        fmt_ev_.notify(sc_core::SC_ZERO_TIME);
        return;
    }
    fmt_fifo.push(cmd);
    update_status();
    if (cmd.stop) {
        process_fmt_fifo();
    }
}

// Functional host mode: the FMT FIFO executed in bus order with bit timing.
// CTRL.enablehost = 0 pauses the host between transactions (M5a review F2):
// a transaction already started runs to its STOP (or the end of a NACK
// discard), then the queue waits, unchanged, until the host is enabled again.
// FIFO_CTRL FMT reset with a transaction open aborts it (M5b review F2): the
// byte on the bus completes (a READB stops after its current byte), then STOP
// at once; no NAK and no CMD_COMPLETE are reported for it.
void i2c::host_thread() {
    for (;;) {
        for (;;) {
            if (abort_req_) {  // aborted while waiting for its next command
                end_transaction(false, false);
                continue;
            }
            if (!fmt_fifo.empty() && ((ctrl & CTRL_ENABLEHOST) || host_busy_ || discarding_)) break;
            wait(fmt_ev_);
        }
        const I2CCommand cmd = fmt_fifo.front();
        fmt_fifo.pop();
        if (discarding_) {  // rest of a transaction halted by a NACK
            if (cmd.stop) discarding_ = false;
            update_status();
            continue;
        }
        host_busy_ = true;
        update_status();
        bool ack = true;
        if (cmd.start) {
            // START or repeated START, then the address byte and its ACK.
            wait(bits(1));
            const auto it = devices_.find(static_cast<uint8_t>(cmd.byte >> 1));
            cur_dev_ = it == devices_.end() ? nullptr : it->second;
            wait(bits(9));
            ack = cur_dev_ && cur_dev_->i2c_start(cmd.byte & 1);
        } else if (cmd.read) {
            const unsigned n = cmd.byte ? cmd.byte : 256u;
            for (unsigned i = 0; i < n && !abort_req_; ++i) {
                while (rx_fifo.size() >= RX_FIFO_DEPTH && !abort_req_) wait(rx_ev_ | fmt_ev_);  // clock stretched
                if (abort_req_) break;
                wait(bits(9));
                const uint8_t byte = cur_dev_ ? cur_dev_->i2c_read(i + 1 < n || cmd.rcont) : 0xFF;
                if (!abort_req_) rx_fifo.push(byte);  // a byte of an aborted read is not kept
                update_status();
            }
        } else {
            wait(bits(9));
            ack = cur_dev_ && cur_dev_->i2c_write(cmd.byte);
        }
        const bool nak = !ack && !cmd.nakok && !abort_req_;
        if (nak) fire_interrupt(INTR_NAK);
        // STOP: requested, after a NACK at once (the commands up to the STOP of
        // that transaction are then discarded), or after an abort.
        if (cmd.stop || nak || abort_req_) end_transaction(nak && !cmd.stop, !abort_req_);
    }
}

// discard_rest / complete are what the transaction asked for when its STOP
// began; an abort (FMT reset) that arrives during the STOP bit wins over them
// (M5b re-review F2): the queue it emptied is not discarded into, and the
// aborted transaction reports no completion.
void i2c::end_transaction(bool discard_rest, bool complete) {
    wait(bits(1));
    const bool aborted = abort_req_;
    if (cur_dev_) cur_dev_->i2c_stop();
    cur_dev_ = nullptr;
    host_busy_ = false;
    discarding_ = discard_rest && !aborted;
    abort_req_ = false;
    update_status();
    if (complete && !aborted) fire_interrupt(INTR_CMD_COMPLETE);
}

void i2c::process_fmt_fifo() {
    while (!fmt_fifo.empty()) {
        I2CCommand cmd = fmt_fifo.front();
        fmt_fifo.pop();

        switch (state) {
            case I2CState::Idle: {
                if (cmd.start) {
                    state = I2CState::Start;
                    current_addr = cmd.byte >> 1;
                    // bool is_read = (cmd.byte & 0x1) != 0;
                    state = I2CState::Transfer;
                }
                break;
            }
            case I2CState::Start: {
                current_addr = cmd.byte >> 1;
                // bool is_read = (cmd.byte & 0x1) != 0;
                state = I2CState::Transfer;
                break;
            }
            case I2CState::Transfer:
                if (cmd.stop) {
                    state = I2CState::Stop;
                } else if (cmd.read) {
                    uint8_t fake_byte = 0xAB;
                    rx_fifo.push(fake_byte);
                }
                break;
            case I2CState::Stop:
                break;
            default:
                break;
        }
    }
    if (state == I2CState::Stop) {
        state = I2CState::Idle;
        update_status();
        fire_interrupt(INTR_CMD_COMPLETE);
    }
}

bool i2c::address_match(uint8_t master_addr) {
    uint8_t my_addr = target_id & TARGET_ID_ADDRESS0_MASK;
    uint8_t mask    = (target_id >> 7) & 0x7F;

    bool match = ((master_addr ^ my_addr) & ~mask) == 0;
    return match;
}

void i2c::process_target(uint8_t master_addr,
                         bool    is_read,
                         std::vector<uint8_t>& write_data,
                         std::vector<uint8_t>& read_data) {
    target_state = I2CTargetState::AddressMatch;

    AcqEntry start_entry;
    start_entry.data   = (master_addr << 1) | (is_read ? 1 : 0);
    start_entry.signal = ACQDATA_SIGNAL_START;
    if (acq_fifo.size() < ACQ_FIFO_DEPTH) {
        acq_fifo.push(start_entry);
    } else {
        fire_interrupt(INTR_ACQ_OVERFLOW);
        target_events |= (1 << 0);
    }

    if (!is_read) {
        target_state = I2CTargetState::AcquireData;

        for (uint8_t byte : write_data) {
            if (acq_fifo.size() >= ACQ_FIFO_DEPTH) {
                fire_interrupt(INTR_ACQ_OVERFLOW);
                target_events |= (1 << 0);
                break;
            }
            AcqEntry entry;
            entry.data   = byte;
            entry.signal = ACQDATA_SIGNAL_NONE;
            acq_fifo.push(entry);
        }

        uint32_t acq_threshold = (target_fifo_config >> 8) & 0xFF;
        if (acq_fifo.size() > acq_threshold) {
            fire_interrupt(INTR_ACQ_THRESHOLD);
        }

    } else {
        target_state = I2CTargetState::SendData;

        if (tx_fifo.empty()) {
            fire_interrupt(INTR_TX_STRETCH);
            read_data.push_back(0xFF);
        } else {
            while (!tx_fifo.empty()) {
                uint8_t byte = tx_fifo.front();
                tx_fifo.pop();
                read_data.push_back(byte);
            }
            uint32_t tx_threshold = target_fifo_config & 0xFF;
            if (tx_fifo.size() < tx_threshold) {
                fire_interrupt(INTR_TX_THRESHOLD);
            }
        }
    }

    target_state = I2CTargetState::Stop;

    AcqEntry stop_entry;
    stop_entry.data   = 0;
    stop_entry.signal = ACQDATA_SIGNAL_STOP;
    if (acq_fifo.size() < ACQ_FIFO_DEPTH) {
        acq_fifo.push(stop_entry);
    }

    target_state = I2CTargetState::Idle;
    update_status();
    fire_interrupt(INTR_CMD_COMPLETE);
}

void i2c::receive_transaction(uint8_t master_addr,
                              bool    is_read,
                              std::vector<uint8_t>& write_data,
                              std::vector<uint8_t>& read_data) {
    if (!(ctrl & CTRL_ENABLETARGET)) {
        return;
    }
    if (!address_match(master_addr)) {
        return;
    }
    process_target(master_addr, is_read, write_data, read_data);
}

void i2c::update_status() {
    status = 0;
    if (fmt_fifo.empty()) {
        status |= STATUS_FMTEMPTY;
    }
    if (fmt_fifo.size() >= 64) {
        status |= STATUS_FMTFULL;
    }
    if (rx_fifo.empty()) {
        status |= STATUS_RXEMPTY;
    }
    if (rx_fifo.size() >= 64) {
        status |= STATUS_RXFULL;
    }
    if (cfg_.functional_host ? (!host_busy_ && fmt_fifo.empty()) : (state == I2CState::Idle)) {
        status |= STATUS_HOSTIDLE;
    }
    if (tx_fifo.empty()) status |= STATUS_TXEMPTY;
    if (tx_fifo.size() >= TX_FIFO_DEPTH) status |= STATUS_TXFULL;
    if (acq_fifo.empty()) status |= STATUS_ACQEMPTY;
    if (acq_fifo.size() >= ACQ_FIFO_DEPTH) status |= STATUS_ACQFULL;
    if (target_state == I2CTargetState::Idle) status |= STATUS_TARGETIDLE;
}

void i2c::fire_interrupt(uint32_t intr_bits) {
    intr_state |= intr_bits;
    // Legacy builds keep their trace; functional mode prints only on request.
    const bool trace = cfg_.functional_host ? cfg_.verbose : true;
    if (trace)
        std::cout
            << "[I2C] fire_interrupt bits=0x"
            << std::hex << intr_bits
            << " intr_state=0x"
            << intr_state
            << " intr_enable=0x"
            << intr_enable
            << std::dec
            << std::endl;

    if (intr_state & intr_enable) {
        if (trace) std::cout << "[I2C] IRQ asserted" << std::endl;
    }
    irq_ev_.notify(sc_core::SC_ZERO_TIME);
}
