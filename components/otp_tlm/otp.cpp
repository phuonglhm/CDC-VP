#include "otp.h"

#include <cstring>
#include <iostream>

using namespace sc_core;

otp::otp(sc_module_name name,
         uint32_t words,
         uint32_t part_words)
    : sc_module(name)
    , socket("socket")
    , irq_out("irq_out")
    , otp_words(words)
    , partition_words(part_words)
{
    if (otp_words == 0) {
        otp_words = 256;
    }

    if (partition_words == 0) {
        partition_words = 64;
    }

    num_partitions = (otp_words + partition_words - 1) / partition_words;

    mem.resize(otp_words, 0x00000000);

    reg_ctrl        = 0;
    reg_status      = 0;
    reg_addr        = 0;
    reg_wdata       = 0;
    reg_rdata       = 0;
    reg_lock        = 0;
    reg_read_lock   = 0;
    reg_err_status  = ERR_NONE;
    reg_intr_enable = 0;
    reg_intr_state  = 0;

    socket.register_b_transport(this, &otp::b_transport);
    irq_out.initialize(false);

    std::cout << "[OTP] Created OTP TLM model" << std::endl;
    std::cout << "[OTP] words = " << otp_words
              << ", partition_words = " << partition_words
              << ", partitions = " << num_partitions
              << std::endl;
}

void otp::b_transport(tlm::tlm_generic_payload& trans,
                      sc_time& delay)
{
    uint64_t addr = trans.get_address();
    unsigned char* data_ptr = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();
    tlm::tlm_command cmd = trans.get_command();

    delay += sc_time(10, SC_NS);

    if (data_ptr == nullptr || len != 4) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        set_error(ERR_BAD_ACCESS);
        return;
    }

    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        set_error(ERR_BYTE_ENABLE);
        return;
    }

    uint32_t offset = static_cast<uint32_t>(addr);
    uint32_t value = 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        value = read_reg(offset);
        std::memcpy(data_ptr, &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else if (cmd == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&value, data_ptr, sizeof(value));
        write_reg(offset, value);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        set_error(ERR_BAD_ACCESS);
    }
}

uint32_t otp::read_reg(uint32_t offset)
{
    switch (offset) {
    case REG_CTRL:
        return reg_ctrl;

    case REG_STATUS:
        return reg_status;

    case REG_ADDR:
        return reg_addr;

    case REG_WDATA:
        return reg_wdata;

    case REG_RDATA:
        return reg_rdata;

    case REG_LOCK:
        return reg_lock;

    case REG_READ_LOCK:
        return reg_read_lock;

    case REG_ERR_STATUS:
        return reg_err_status;

    case REG_INTR_ENABLE:
        return reg_intr_enable;

    case REG_INTR_STATE:
        return reg_intr_state;

    case REG_SIZE_WORDS:
        return otp_words;

    case REG_PARTITION_SIZE:
        return partition_words;

    default:
        set_error(ERR_BAD_ACCESS);
        return 0;
    }
}

void otp::write_reg(uint32_t offset, uint32_t value)
{
    switch (offset) {
    case REG_CTRL:
        reg_ctrl = value;

        if (value & CTRL_CLEAR_IRQ) {
            reg_intr_state = 0;
            reg_status &= ~STATUS_IRQ;
            update_irq();
        }

        if (value & CTRL_READ_START) {
            do_read();
        }

        if (value & CTRL_PROG_START) {
            do_program();
        }

        break;

    case REG_ADDR:
        reg_addr = value;
        break;

    case REG_WDATA:
        reg_wdata = value;
        break;

    case REG_LOCK:
        /*
         * Sticky write lock.
         * Once a partition lock bit is set, it cannot be cleared.
         */
        reg_lock |= value;
        break;

    case REG_READ_LOCK:
        /*
         * Sticky read lock.
         */
        reg_read_lock |= value;
        break;

    case REG_ERR_STATUS:
        /*
         * Write non-zero to clear error.
         */
        if (value != 0) {
            reg_err_status = ERR_NONE;
            reg_status &= ~STATUS_ERROR;
        }
        break;

    case REG_INTR_ENABLE:
        reg_intr_enable = value & 0x1;
        update_irq();
        break;

    case REG_INTR_STATE:
        /*
         * Write 1 to clear interrupt.
         */
        if (value & 0x1) {
            reg_intr_state = 0;
            reg_status &= ~STATUS_IRQ;
            update_irq();
        }
        break;

    case REG_STATUS:
    case REG_RDATA:
    case REG_SIZE_WORDS:
    case REG_PARTITION_SIZE:
        set_error(ERR_BAD_ACCESS);
        break;

    default:
        set_error(ERR_BAD_ACCESS);
        break;
    }
}

void otp::do_read()
{
    clear_done_error();

    if (reg_status & STATUS_BUSY) {
        set_error(ERR_BUSY);
        return;
    }

    reg_status |= STATUS_BUSY;

    uint32_t word_addr = reg_addr;

    if (!addr_valid(word_addr)) {
        reg_status &= ~STATUS_BUSY;
        set_error(ERR_ADDR_OUT_OF_RANGE);
        return;
    }

    if (read_locked(word_addr)) {
        reg_status &= ~STATUS_BUSY;
        set_error(ERR_READ_LOCKED);
        return;
    }

    wait(sc_time(100, SC_NS));

    reg_rdata = mem[word_addr];

    std::cout << "[OTP] READ addr=" << word_addr
              << " data=0x" << std::hex << reg_rdata << std::dec
              << std::endl;

    reg_status &= ~STATUS_BUSY;
    set_done();
}

void otp::do_program()
{
    clear_done_error();

    if (reg_status & STATUS_BUSY) {
        set_error(ERR_BUSY);
        return;
    }

    reg_status |= STATUS_BUSY;

    uint32_t word_addr = reg_addr;

    if (!addr_valid(word_addr)) {
        reg_status &= ~STATUS_BUSY;
        set_error(ERR_ADDR_OUT_OF_RANGE);
        return;
    }

    if (write_locked(word_addr)) {
        reg_status &= ~STATUS_BUSY;
        set_error(ERR_WRITE_LOCKED);
        return;
    }

    uint32_t old_value = mem[word_addr];
    uint32_t new_value = reg_wdata;

    /*
     * OTP rule:
     * erased bit = 0
     * programmed bit = 1
     *
     * So only 0 -> 1 is allowed.
     * 1 -> 0 is illegal.
     */
    if ((old_value & ~new_value) != 0) {
        reg_status &= ~STATUS_BUSY;
        set_error(ERR_PROGRAM_1_TO_0);
        return;
    }

    wait(sc_time(1, SC_US));

    mem[word_addr] = old_value | new_value;

    /*
     * Read-back verify.
     */
    if (mem[word_addr] != new_value) {
        reg_status &= ~STATUS_BUSY;
        set_error(ERR_PROGRAM_1_TO_0);
        return;
    }

    std::cout << "[OTP] PROGRAM addr=" << word_addr
              << " data=0x" << std::hex << new_value << std::dec
              << std::endl;

    reg_status &= ~STATUS_BUSY;
    set_done();
}

bool otp::addr_valid(uint32_t word_addr) const
{
    return word_addr < otp_words;
}

uint32_t otp::get_partition(uint32_t word_addr) const
{
    return word_addr / partition_words;
}

bool otp::write_locked(uint32_t word_addr) const
{
    uint32_t part = get_partition(word_addr);

    if (part >= 32) {
        return true;
    }

    return (reg_lock & (1u << part)) != 0;
}

bool otp::read_locked(uint32_t word_addr) const
{
    uint32_t part = get_partition(word_addr);

    if (part >= 32) {
        return true;
    }

    return (reg_read_lock & (1u << part)) != 0;
}

void otp::clear_done_error()
{
    reg_status &= ~STATUS_DONE;
    reg_status &= ~STATUS_ERROR;
    reg_err_status = ERR_NONE;
}

void otp::set_done()
{
    reg_status |= STATUS_DONE;

    reg_intr_state = 1;
    reg_status |= STATUS_IRQ;

    update_irq();
}

void otp::set_error(ErrorCode err)
{
    reg_err_status = static_cast<uint32_t>(err);
    reg_status |= STATUS_ERROR;

    reg_intr_state = 1;
    reg_status |= STATUS_IRQ;

    std::cout << "[OTP] ERROR code=" << static_cast<uint32_t>(err)
              << std::endl;

    update_irq();
}

void otp::update_irq()
{
    bool irq = ((reg_intr_enable & 0x1) != 0) &&
               ((reg_intr_state & 0x1) != 0);

    irq_out.write(irq);
}
