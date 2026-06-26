#ifndef OTP_TLM_H
#define OTP_TLM_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <vector>

class otp : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<otp> socket;
    sc_core::sc_out<bool> irq_out;

    otp(sc_core::sc_module_name name,
        uint32_t words = 256,
        uint32_t partition_words = 64);

private:
    // ============================================================
    // OpenTitan-like OTP_CTRL register map
    // ============================================================
    static constexpr uint32_t REG_INTR_STATE               = 0x000;
    static constexpr uint32_t REG_INTR_ENABLE              = 0x004;
    static constexpr uint32_t REG_INTR_TEST                = 0x008;
    static constexpr uint32_t REG_ALERT_TEST               = 0x00C;
    static constexpr uint32_t REG_STATUS                   = 0x010;
    static constexpr uint32_t REG_PARTITION_STATUS_0       = 0x014;

    static constexpr uint32_t REG_ERR_CODE_BASE            = 0x018;
    static constexpr uint32_t REG_ERR_CODE_LAST            = 0x074;
    static constexpr uint32_t NUM_ERR_CODE_REGS            = 24;

    static constexpr uint32_t REG_DIRECT_ACCESS_REGWEN     = 0x078;
    static constexpr uint32_t REG_DIRECT_ACCESS_CMD        = 0x07C;
    static constexpr uint32_t REG_DIRECT_ACCESS_ADDRESS    = 0x080;
    static constexpr uint32_t REG_DIRECT_ACCESS_WDATA_0    = 0x084;
    static constexpr uint32_t REG_DIRECT_ACCESS_WDATA_1    = 0x088;
    static constexpr uint32_t REG_DIRECT_ACCESS_RDATA_0    = 0x08C;
    static constexpr uint32_t REG_DIRECT_ACCESS_RDATA_1    = 0x090;

    static constexpr uint32_t REG_CHECK_TRIGGER_REGWEN     = 0x094;
    static constexpr uint32_t REG_CHECK_TRIGGER            = 0x098;
    static constexpr uint32_t REG_CHECK_REGWEN             = 0x09C;
    static constexpr uint32_t REG_CHECK_TIMEOUT            = 0x0A0;
    static constexpr uint32_t REG_INTEGRITY_CHECK_PERIOD   = 0x0A4;
    static constexpr uint32_t REG_CONSISTENCY_CHECK_PERIOD = 0x0A8;

    // Runtime read-lock registers.
    static constexpr uint32_t REG_READ_LOCK_BASE                 = 0x0AC;
    static constexpr uint32_t REG_VENDOR_TEST_READ_LOCK          = 0x0AC;
    static constexpr uint32_t REG_CREATOR_SW_CFG_READ_LOCK       = 0x0B0;
    static constexpr uint32_t REG_OWNER_SW_CFG_READ_LOCK         = 0x0B4;
    static constexpr uint32_t REG_OWNERSHIP_SLOT_STATE_READ_LOCK = 0x0B8;
    static constexpr uint32_t REG_ROT_CREATOR_AUTH_READ_LOCK     = 0x0BC;
    static constexpr uint32_t REG_ROT_OWNER_AUTH_SLOT0_READ_LOCK = 0x0C0;
    static constexpr uint32_t REG_ROT_OWNER_AUTH_SLOT1_READ_LOCK = 0x0C4;
    static constexpr uint32_t REG_PLAT_INTEG_AUTH_SLOT0_READ_LOCK = 0x0C8;
    static constexpr uint32_t REG_PLAT_INTEG_AUTH_SLOT1_READ_LOCK = 0x0CC;
    static constexpr uint32_t REG_PLAT_OWNER_AUTH_SLOT0_READ_LOCK = 0x0D0;
    static constexpr uint32_t REG_PLAT_OWNER_AUTH_SLOT1_READ_LOCK = 0x0D4;
    static constexpr uint32_t REG_PLAT_OWNER_AUTH_SLOT2_READ_LOCK = 0x0D8;
    static constexpr uint32_t REG_PLAT_OWNER_AUTH_SLOT3_READ_LOCK = 0x0DC;
    static constexpr uint32_t REG_EXT_NVM_READ_LOCK              = 0x0E0;
    static constexpr uint32_t REG_ROM_PATCH_READ_LOCK            = 0x0E4;

    static constexpr uint32_t NUM_PARTITION_LOCK_REGS = 15;

    // Digest registers. Simplified exposed area.
    static constexpr uint32_t REG_DIGEST_BASE = 0x0E8;
    static constexpr uint32_t NUM_DIGEST_REGS = 32;
    static constexpr uint32_t REG_DIGEST_LAST = REG_DIGEST_BASE + NUM_DIGEST_REGS * 4 - 4;

    // Extra debug/info registers for this TLM model.
    // These are not OpenTitan architectural registers.
    static constexpr uint32_t REG_MODEL_SIZE_WORDS     = 0x200;
    static constexpr uint32_t REG_MODEL_PARTITION_SIZE = 0x204;

    // ============================================================
    // Interrupt bits
    // ============================================================
    static constexpr uint32_t INTR_OTP_OPERATION_DONE = 1u << 0;
    static constexpr uint32_t INTR_OTP_ERROR          = 1u << 1;

    // ============================================================
    // Direct access command encoding
    // 0x1 = read, 0x2 = write/program, 0x4 = digest
    // ============================================================
    static constexpr uint32_t DAI_CMD_READ   = 0x1;
    static constexpr uint32_t DAI_CMD_WRITE  = 0x2;
    static constexpr uint32_t DAI_CMD_DIGEST = 0x4;

    // ============================================================
    // Simplified STATUS bits for this TLM model
    // ============================================================
    static constexpr uint32_t STATUS_DAI_IDLE    = 1u << 0;
    static constexpr uint32_t STATUS_DAI_ERROR   = 1u << 1;
    static constexpr uint32_t STATUS_CHECK_ERROR = 1u << 2;
    static constexpr uint32_t STATUS_FSM_ERROR   = 1u << 3;

    // ============================================================
    // OpenTitan-like error code values
    // ============================================================
    enum ErrorCode : uint32_t {
        ERR_NO_ERROR                = 0x0,
        ERR_MACRO_ERROR             = 0x1,
        ERR_MACRO_ECC_CORR_ERROR    = 0x2,
        ERR_MACRO_ECC_UNCORR_ERROR  = 0x3,
        ERR_MACRO_WRITE_BLANK_ERROR = 0x4,
        ERR_ACCESS_ERROR            = 0x5,
        ERR_CHECK_FAIL_ERROR        = 0x6,
        ERR_FSM_STATE_ERROR         = 0x7
    };

private:
    std::vector<uint32_t> mem;

    uint32_t otp_words;
    uint32_t partition_words;
    uint32_t num_partitions;

    uint32_t intr_state;
    uint32_t intr_enable;
    uint32_t intr_test;
    uint32_t alert_test;
    uint32_t status;
    uint32_t partition_status_0;

    std::array<uint32_t, NUM_ERR_CODE_REGS> err_code;

    uint32_t direct_access_regwen;
    uint32_t direct_access_cmd;
    uint32_t direct_access_address;
    uint32_t direct_access_wdata_0;
    uint32_t direct_access_wdata_1;
    uint32_t direct_access_rdata_0;
    uint32_t direct_access_rdata_1;

    uint32_t check_trigger_regwen;
    uint32_t check_trigger;
    uint32_t check_regwen;
    uint32_t check_timeout;
    uint32_t integrity_check_period;
    uint32_t consistency_check_period;

    std::array<uint32_t, NUM_PARTITION_LOCK_REGS> read_lock_regs;
    std::array<uint32_t, NUM_DIGEST_REGS> digest_regs;

private:
    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);

    uint32_t read_reg(uint32_t offset);
    void write_reg(uint32_t offset, uint32_t value);

    bool is_err_code_reg(uint32_t offset) const;
    uint32_t err_code_index(uint32_t offset) const;

    bool is_read_lock_reg(uint32_t offset) const;
    uint32_t read_lock_index(uint32_t offset) const;

    bool is_digest_reg(uint32_t offset) const;
    uint32_t digest_index(uint32_t offset) const;

    void execute_dai_command(uint32_t cmd);
    void dai_read();
    void dai_write();
    void dai_digest();

    void trigger_checks(uint32_t value);
    void update_partition_status();

    uint32_t byte_addr_to_word_addr(uint32_t byte_addr) const;
    uint32_t get_partition(uint32_t word_addr) const;
    bool addr_valid(uint32_t word_addr) const;
    bool partition_read_locked(uint32_t word_addr) const;
    bool partition_write_locked_by_digest(uint32_t word_addr) const;

    void clear_error_status();
    void set_error(ErrorCode code, uint32_t agent_index = 0);
    void set_done();
    void clear_interrupts(uint32_t value);
    void update_irq();
};

#endif
