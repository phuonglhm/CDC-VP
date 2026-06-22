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

    intr_state = 0;
    intr_enable = 0;
    intr_test = 0;
    alert_test = 0;

    status = STATUS_DAI_IDLE;
    partition_status_0 = 0;

    err_code.fill(ERR_NO_ERROR);

    direct_access_regwen = 1;
    direct_access_cmd = 0;
    direct_access_address = 0;
    direct_access_wdata_0 = 0;
    direct_access_wdata_1 = 0;
    direct_access_rdata_0 = 0;
    direct_access_rdata_1 = 0;

    check_trigger_regwen = 1;
    check_trigger = 0;
    check_regwen = 1;
    check_timeout = 0x1000;
    integrity_check_period = 0;
    consistency_check_period = 0;

    // 1 = readable, 0 = locked
    read_lock_regs.fill(1);

    // Non-zero digest means partition write-locked in this simplified model.
    digest_regs.fill(0);

    socket.register_b_transport(this, &otp::b_transport);
    irq_out.initialize(false);

    std::cout << "[OTP] OpenTitan-like OTP TLM model created\n";
    std::cout << "[OTP] words=" << otp_words
              << ", partition_words=" << partition_words
              << ", partitions=" << num_partitions << "\n";
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
        set_error(ERR_ACCESS_ERROR);
        return;
    }

    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        set_error(ERR_ACCESS_ERROR);
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
        set_error(ERR_ACCESS_ERROR);
    }
}

bool otp::is_err_code_reg(uint32_t offset) const
{
    return offset >= REG_ERR_CODE_BASE &&
           offset <= REG_ERR_CODE_LAST &&
           ((offset - REG_ERR_CODE_BASE) % 4 == 0);
}

uint32_t otp::err_code_index(uint32_t offset) const
{
    return (offset - REG_ERR_CODE_BASE) / 4;
}

bool otp::is_read_lock_reg(uint32_t offset) const
{
    return offset >= REG_READ_LOCK_BASE &&
           offset <= REG_ROM_PATCH_READ_LOCK &&
           ((offset - REG_READ_LOCK_BASE) % 4 == 0);
}

uint32_t otp::read_lock_index(uint32_t offset) const
{
    return (offset - REG_READ_LOCK_BASE) / 4;
}

bool otp::is_digest_reg(uint32_t offset) const
{
    return offset >= REG_DIGEST_BASE &&
           offset <= REG_DIGEST_LAST &&
           ((offset - REG_DIGEST_BASE) % 4 == 0);
}

uint32_t otp::digest_index(uint32_t offset) const
{
    return (offset - REG_DIGEST_BASE) / 4;
}

uint32_t otp::read_reg(uint32_t offset)
{
    if (is_err_code_reg(offset)) {
        return err_code[err_code_index(offset)] & 0x7;
    }

    if (is_read_lock_reg(offset)) {
        return read_lock_regs[read_lock_index(offset)] & 0x1;
    }

    if (is_digest_reg(offset)) {
        return digest_regs[digest_index(offset)];
    }

    switch (offset) {
    case REG_INTR_STATE:
        return intr_state;

    case REG_INTR_ENABLE:
        return intr_enable;

    case REG_INTR_TEST:
        return intr_test;

    case REG_ALERT_TEST:
        return alert_test;

    case REG_STATUS:
        return status;

    case REG_PARTITION_STATUS_0:
        update_partition_status();
        return partition_status_0;

    case REG_DIRECT_ACCESS_REGWEN:
        return direct_access_regwen;

    case REG_DIRECT_ACCESS_CMD:
        return direct_access_cmd;

    case REG_DIRECT_ACCESS_ADDRESS:
        return direct_access_address;

    case REG_DIRECT_ACCESS_WDATA_0:
        return direct_access_wdata_0;

    case REG_DIRECT_ACCESS_WDATA_1:
        return direct_access_wdata_1;

    case REG_DIRECT_ACCESS_RDATA_0:
        return direct_access_rdata_0;

    case REG_DIRECT_ACCESS_RDATA_1:
        return direct_access_rdata_1;

    case REG_CHECK_TRIGGER_REGWEN:
        return check_trigger_regwen;

    case REG_CHECK_TRIGGER:
        return check_trigger;

    case REG_CHECK_REGWEN:
        return check_regwen;

    case REG_CHECK_TIMEOUT:
        return check_timeout;

    case REG_INTEGRITY_CHECK_PERIOD:
        return integrity_check_period;

    case REG_CONSISTENCY_CHECK_PERIOD:
        return consistency_check_period;

    case REG_MODEL_SIZE_WORDS:
        return otp_words;

    case REG_MODEL_PARTITION_SIZE:
        return partition_words;

    default:
        set_error(ERR_ACCESS_ERROR);
        return 0;
    }
}

void otp::write_reg(uint32_t offset, uint32_t value)
{
    if (is_err_code_reg(offset)) {
        // OpenTitan ERR_CODE registers are read-only.
        // This model ignores writes to them.
        return;
    }

    if (is_read_lock_reg(offset)) {
        // Runtime read lock registers.
        // Simplified behavior:
        // 1 = readable, 0 = locked.
        // Once cleared to 0, it remains locked.
        uint32_t idx = read_lock_index(offset);
        read_lock_regs[idx] &= (value & 0x1);
        return;
    }

    if (is_digest_reg(offset)) {
        // Simplified digest storage.
        // Real OpenTitan digest is computed/used for integrity and lock.
        digest_regs[digest_index(offset)] = value;
        update_partition_status();
        return;
    }

    switch (offset) {
    case REG_INTR_STATE:
        // W1C behavior.
        clear_interrupts(value);
        break;

    case REG_INTR_ENABLE:
        intr_enable = value & (INTR_OTP_OPERATION_DONE | INTR_OTP_ERROR);
        update_irq();
        break;

    case REG_INTR_TEST:
        intr_test = value & (INTR_OTP_OPERATION_DONE | INTR_OTP_ERROR);
        intr_state |= intr_test;
        update_irq();
        break;

    case REG_ALERT_TEST:
        // Stub: store only. No alert pin in this TLM model.
        alert_test = value;
        break;

    case REG_STATUS:
    case REG_PARTITION_STATUS_0:
        // Read-only registers.
        set_error(ERR_ACCESS_ERROR);
        break;

    case REG_DIRECT_ACCESS_REGWEN:
        // Simplified RW0C behavior.
        // Writing 0 locks, writing 1 does not unlock.
        if ((value & 0x1) == 0) {
            direct_access_regwen = 0;
        }
        break;

    case REG_DIRECT_ACCESS_CMD:
        if (direct_access_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        direct_access_cmd = value;
        execute_dai_command(value);
        break;

    case REG_DIRECT_ACCESS_ADDRESS:
        if (direct_access_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        direct_access_address = value;
        break;

    case REG_DIRECT_ACCESS_WDATA_0:
        if (direct_access_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        direct_access_wdata_0 = value;
        break;

    case REG_DIRECT_ACCESS_WDATA_1:
        if (direct_access_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        direct_access_wdata_1 = value;
        break;

    case REG_DIRECT_ACCESS_RDATA_0:
    case REG_DIRECT_ACCESS_RDATA_1:
        // Read-only registers.
        set_error(ERR_ACCESS_ERROR);
        break;

    case REG_CHECK_TRIGGER_REGWEN:
        if ((value & 0x1) == 0) {
            check_trigger_regwen = 0;
        }
        break;

    case REG_CHECK_TRIGGER:
        if (check_trigger_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        check_trigger = value;
        trigger_checks(value);
        break;

    case REG_CHECK_REGWEN:
        if ((value & 0x1) == 0) {
            check_regwen = 0;
        }
        break;

    case REG_CHECK_TIMEOUT:
        if (check_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        check_timeout = value;
        break;

    case REG_INTEGRITY_CHECK_PERIOD:
        if (check_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        integrity_check_period = value;
        break;

    case REG_CONSISTENCY_CHECK_PERIOD:
        if (check_regwen == 0) {
            set_error(ERR_ACCESS_ERROR);
            break;
        }
        consistency_check_period = value;
        break;

    case REG_MODEL_SIZE_WORDS:
    case REG_MODEL_PARTITION_SIZE:
        set_error(ERR_ACCESS_ERROR);
        break;

    default:
        set_error(ERR_ACCESS_ERROR);
        break;
    }
}

void otp::execute_dai_command(uint32_t cmd)
{
    if ((status & STATUS_DAI_IDLE) == 0) {
        set_error(ERR_FSM_STATE_ERROR);
        return;
    }

    clear_error_status();

    status &= ~STATUS_DAI_IDLE;
    direct_access_regwen = 0;

    if (cmd == DAI_CMD_READ) {
        dai_read();
    } else if (cmd == DAI_CMD_WRITE) {
        dai_write();
    } else if (cmd == DAI_CMD_DIGEST) {
        dai_digest();
    } else {
        set_error(ERR_ACCESS_ERROR);
    }

    direct_access_regwen = 1;
    status |= STATUS_DAI_IDLE;
}

void otp::dai_read()
{
    uint32_t word_addr = byte_addr_to_word_addr(direct_access_address);

    if (!addr_valid(word_addr)) {
        direct_access_rdata_0 = 0;
        direct_access_rdata_1 = 0;
        set_error(ERR_ACCESS_ERROR);
        return;
    }

    if (partition_read_locked(word_addr)) {
        direct_access_rdata_0 = 0;
        direct_access_rdata_1 = 0;
        set_error(ERR_ACCESS_ERROR);
        return;
    }

    wait(sc_time(100, SC_NS));

    direct_access_rdata_0 = mem[word_addr];

    if (addr_valid(word_addr + 1)) {
        direct_access_rdata_1 = mem[word_addr + 1];
    } else {
        direct_access_rdata_1 = 0;
    }

    std::cout << "[OTP] DAI READ byte_addr=0x"
              << std::hex << direct_access_address
              << " word=" << std::dec << word_addr
              << " rdata0=0x" << std::hex << direct_access_rdata_0
              << " rdata1=0x" << direct_access_rdata_1
              << std::dec << "\n";

    set_done();
}

void otp::dai_write()
{
    uint32_t word_addr = byte_addr_to_word_addr(direct_access_address);

    if (!addr_valid(word_addr)) {
        set_error(ERR_ACCESS_ERROR);
        return;
    }

    if (partition_write_locked_by_digest(word_addr)) {
        set_error(ERR_ACCESS_ERROR);
        return;
    }

    uint32_t old0 = mem[word_addr];
    uint32_t new0 = direct_access_wdata_0;

    // Simplified OTP rule:
    // erased bit = 0, programmed bit = 1.
    // Only 0 -> 1 is allowed.
    if ((old0 & ~new0) != 0) {
        set_error(ERR_MACRO_WRITE_BLANK_ERROR);
        return;
    }

    wait(sc_time(1, SC_US));

    mem[word_addr] = old0 | new0;

    // Optional second word write if address+1 exists and WDATA1 is nonzero.
    if (direct_access_wdata_1 != 0 && addr_valid(word_addr + 1)) {
        uint32_t old1 = mem[word_addr + 1];
        uint32_t new1 = direct_access_wdata_1;

        if ((old1 & ~new1) != 0) {
            set_error(ERR_MACRO_WRITE_BLANK_ERROR);
            return;
        }

        mem[word_addr + 1] = old1 | new1;
    }

    std::cout << "[OTP] DAI WRITE byte_addr=0x"
              << std::hex << direct_access_address
              << " word=" << std::dec << word_addr
              << " wdata0=0x" << std::hex << direct_access_wdata_0
              << " wdata1=0x" << direct_access_wdata_1
              << std::dec << "\n";

    set_done();
}

void otp::dai_digest()
{
    uint32_t word_addr = byte_addr_to_word_addr(direct_access_address);
    uint32_t part = get_partition(word_addr);

    if (part >= num_partitions) {
        set_error(ERR_ACCESS_ERROR);
        return;
    }

    wait(sc_time(500, SC_NS));

    // Simplified fake digest:
    // digest[2*part] and digest[2*part+1] become nonzero.
    // Nonzero digest is treated as write-lock for that partition.
    uint32_t di = part * 2;
    if (di + 1 < NUM_DIGEST_REGS) {
        digest_regs[di] = 0xD1650000u | part;
        digest_regs[di + 1] = 0xA5A50000u | part;
    }

    update_partition_status();

    std::cout << "[OTP] DAI DIGEST partition=" << part << "\n";

    set_done();
}

void otp::trigger_checks(uint32_t value)
{
    clear_error_status();

    // Stub behavior:
    // bit0: integrity check
    // bit1: consistency check
    // bit31: inject failure
    if (value & (1u << 31)) {
        status |= STATUS_CHECK_ERROR;
        set_error(ERR_CHECK_FAIL_ERROR);
        return;
    }

    wait(sc_time(200, SC_NS));

    std::cout << "[OTP] CHECK_TRIGGER value=0x"
              << std::hex << value << std::dec
              << " PASS\n";

    set_done();
}

void otp::update_partition_status()
{
    partition_status_0 = 0;

    for (uint32_t p = 0; p < num_partitions && p < 16; ++p) {
        uint32_t di = p * 2;

        // Simplified:
        // bit p = digest/lock status
        // bit p+16 = error status
        if (di + 1 < NUM_DIGEST_REGS &&
            (digest_regs[di] != 0 || digest_regs[di + 1] != 0)) {
            partition_status_0 |= (1u << p);
        }

        if (p < NUM_ERR_CODE_REGS && err_code[p] != ERR_NO_ERROR) {
            partition_status_0 |= (1u << (p + 16));
        }
    }
}

uint32_t otp::byte_addr_to_word_addr(uint32_t byte_addr) const
{
    // OpenTitan DAI address is byte address.
    // This simplified model uses 32-bit granule.
    return byte_addr >> 2;
}

uint32_t otp::get_partition(uint32_t word_addr) const
{
    return word_addr / partition_words;
}

bool otp::addr_valid(uint32_t word_addr) const
{
    return word_addr < otp_words;
}

bool otp::partition_read_locked(uint32_t word_addr) const
{
    uint32_t part = get_partition(word_addr);

    if (part >= NUM_PARTITION_LOCK_REGS) {
        return false;
    }

    // 1 = readable, 0 = locked.
    return (read_lock_regs[part] & 0x1) == 0;
}

bool otp::partition_write_locked_by_digest(uint32_t word_addr) const
{
    uint32_t part = get_partition(word_addr);
    uint32_t di = part * 2;

    if (di + 1 >= NUM_DIGEST_REGS) {
        return false;
    }

    // Simplified digest lock:
    // nonzero digest means partition write-locked.
    return digest_regs[di] != 0 || digest_regs[di + 1] != 0;
}

void otp::clear_error_status()
{
    status &= ~STATUS_DAI_ERROR;
    status &= ~STATUS_CHECK_ERROR;
    status &= ~STATUS_FSM_ERROR;

    err_code[0] = ERR_NO_ERROR;

    update_partition_status();
}

void otp::set_error(ErrorCode code, uint32_t agent_index)
{
    if (agent_index >= NUM_ERR_CODE_REGS) {
        agent_index = 0;
    }

    err_code[agent_index] = static_cast<uint32_t>(code) & 0x7;

    if (code == ERR_CHECK_FAIL_ERROR) {
        status |= STATUS_CHECK_ERROR;
    } else if (code == ERR_FSM_STATE_ERROR) {
        status |= STATUS_FSM_ERROR;
    } else {
        status |= STATUS_DAI_ERROR;
    }

    intr_state |= INTR_OTP_ERROR;

    update_partition_status();
    update_irq();

    std::cout << "[OTP] ERROR code=0x"
              << std::hex << static_cast<uint32_t>(code)
              << std::dec << "\n";
}

void otp::set_done()
{
    intr_state |= INTR_OTP_OPERATION_DONE;
    update_irq();
}

void otp::clear_interrupts(uint32_t value)
{
    intr_state &= ~value;
    update_irq();
}

void otp::update_irq()
{
    bool irq = (intr_state & intr_enable) != 0;
    irq_out.write(irq);
}
