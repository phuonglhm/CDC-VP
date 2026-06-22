#ifndef OTP_TLM_H
#define OTP_TLM_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

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
    // Register map
    static constexpr uint32_t REG_CTRL           = 0x00;
    static constexpr uint32_t REG_STATUS         = 0x04;
    static constexpr uint32_t REG_ADDR           = 0x08;
    static constexpr uint32_t REG_WDATA          = 0x0C;
    static constexpr uint32_t REG_RDATA          = 0x10;
    static constexpr uint32_t REG_LOCK           = 0x14;
    static constexpr uint32_t REG_READ_LOCK      = 0x18;
    static constexpr uint32_t REG_ERR_STATUS     = 0x1C;
    static constexpr uint32_t REG_INTR_ENABLE    = 0x20;
    static constexpr uint32_t REG_INTR_STATE     = 0x24;
    static constexpr uint32_t REG_SIZE_WORDS     = 0x28;
    static constexpr uint32_t REG_PARTITION_SIZE = 0x2C;

    // CTRL bits
    static constexpr uint32_t CTRL_READ_START = 1u << 0;
    static constexpr uint32_t CTRL_PROG_START = 1u << 1;
    static constexpr uint32_t CTRL_CLEAR_IRQ  = 1u << 2;

    // STATUS bits
    static constexpr uint32_t STATUS_BUSY  = 1u << 0;
    static constexpr uint32_t STATUS_DONE  = 1u << 1;
    static constexpr uint32_t STATUS_ERROR = 1u << 2;
    static constexpr uint32_t STATUS_IRQ   = 1u << 3;

    enum ErrorCode : uint32_t {
        ERR_NONE              = 0,
        ERR_ADDR_OUT_OF_RANGE = 1,
        ERR_WRITE_LOCKED      = 2,
        ERR_READ_LOCKED       = 3,
        ERR_PROGRAM_1_TO_0    = 4,
        ERR_BUSY              = 5,
        ERR_BAD_ACCESS        = 6,
        ERR_BYTE_ENABLE       = 7
    };

private:
    std::vector<uint32_t> mem;

    uint32_t otp_words;
    uint32_t partition_words;
    uint32_t num_partitions;

    uint32_t reg_ctrl;
    uint32_t reg_status;
    uint32_t reg_addr;
    uint32_t reg_wdata;
    uint32_t reg_rdata;
    uint32_t reg_lock;
    uint32_t reg_read_lock;
    uint32_t reg_err_status;
    uint32_t reg_intr_enable;
    uint32_t reg_intr_state;

private:
    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);

    uint32_t read_reg(uint32_t offset);
    void write_reg(uint32_t offset, uint32_t value);

    void do_read();
    void do_program();

    bool addr_valid(uint32_t word_addr) const;
    uint32_t get_partition(uint32_t word_addr) const;
    bool write_locked(uint32_t word_addr) const;
    bool read_locked(uint32_t word_addr) const;

    void clear_done_error();
    void set_done();
    void set_error(ErrorCode err);
    void update_irq();
};

#endif
