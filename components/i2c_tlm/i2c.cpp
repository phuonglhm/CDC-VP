#include "i2c.h"
#include <iostream>

void i2c::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    uint32_t offset = (uint32_t)trans.get_address();
    uint8_t* data = trans.get_data_ptr();

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        uint32_t value = *reinterpret_cast<uint32_t*>(data);
        write_reg(offset,value);
    } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        uint32_t value = read_reg(offset);
        *reinterpret_cast<uint32_t*>(data) = value;
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
            if ((intr_state & intr_enable) == 0) {
                irq.write(false);
            }
            break;
        case I2C_INTR_ENABLE:
            intr_enable = data;
            break;
        case I2C_CTRL:
            ctrl = data;
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
            }
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

    fmt_fifo.push(cmd);
    update_status();
    if (cmd.stop) {
        process_fmt_fifo();
    }
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
    if (state == I2CState::Idle) {
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
    if (intr_state & intr_enable) {
        irq.write(true);
    }
}
