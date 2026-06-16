#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <queue>
#include <cstdint>
#include <vector>

#define I2C_INTR_STATE 0x00
#define I2C_INTR_ENABLE 0x04
#define I2C_CTRL 0x10
#define I2C_STATUS 0x14
#define I2C_RDATA 0x18
#define I2C_FDATA 0x1c
#define I2C_FIFO_CTRL 0x20
#define I2C_HOST_FIFO_STATUS 0x2c

#define I2C_TARGET_FIFO_CONFIG  0x28
#define I2C_TARGET_FIFO_STATUS  0x30
#define I2C_TARGET_ID           0x54
#define I2C_ACQDATA             0x58
#define I2C_TXDATA              0x5c
#define I2C_TARGET_ACK_CTRL     0x6c
#define I2C_TARGET_EVENTS       0x7c

#define FDATA_BYTE_MASK 0xFF
#define FDATA_START (1 << 8)
#define FDATA_STOP (1 << 9)
#define FDATA_READB (1 << 10)

#define CTRL_ENABLETARGET (1 << 1)
#define CTRL_ENABLEHOST (1 << 0)

#define STATUS_FMTEMPTY (1 << 2)
#define STATUS_FMTFULL (1 << 0)
#define STATUS_RXEMPTY (1 << 5)
#define STATUS_RXFULL (1 << 1)
#define STATUS_HOSTIDLE (1 << 3)

#define STATUS_TXFULL      (1 << 6)
#define STATUS_ACQFULL     (1 << 7)
#define STATUS_TXEMPTY     (1 << 8)
#define STATUS_ACQEMPTY    (1 << 9)
#define STATUS_TARGETIDLE  (1 << 4)

#define INTR_CMD_COMPLETE (1 << 9)

#define INTR_TX_STRETCH     (1 << 6)
#define INTR_ACQ_THRESHOLD  (1 << 1)
#define INTR_ACQ_OVERFLOW   (1 << 3)
#define INTR_TX_THRESHOLD   (1 << 0)

#define TARGET_ID_ADDRESS0_MASK  0x7F 
#define TARGET_ID_MASK0_MASK     (0x7F << 7)

#define ACQDATA_SIGNAL_NONE  0x0
#define ACQDATA_SIGNAL_START 0x1
#define ACQDATA_SIGNAL_STOP  0x2

#define FMT_FIFO_DEPTH 64
#define RX_FIFO_DEPTH  64
#define ACQ_FIFO_DEPTH 64
#define TX_FIFO_DEPTH  64

enum class I2CState { Idle, Start, Transfer, Stop};
enum class I2CTargetState { Idle, AddressMatch, AcquireData, SendData, Stop };

struct I2CCommand {
    bool start;
    bool stop;
    bool read;
    uint8_t byte;
};

struct AcqEntry {
    uint8_t data;
    uint8_t signal;
};

class i2c : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<i2c> socket;
    sc_core::sc_out<bool> irq;
    SC_CTOR(i2c) : socket("socket"), irq("irq") {
        socket.register_b_transport(this, &i2c::b_transport);
    }

    void receive_transaction(uint8_t master_addr,
                             bool     is_read,
                             std::vector<uint8_t>& write_data,
                             std::vector<uint8_t>& read_data);
private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    
    uint32_t read_reg(uint32_t offset);
    void write_reg(uint32_t offset, uint32_t data);

    void handle_fdata(uint32_t value);
    void process_fmt_fifo();

     void process_target(uint8_t master_addr,
                        bool    is_read,
                        std::vector<uint8_t>& write_data,
                        std::vector<uint8_t>& read_data);
    bool address_match(uint8_t master_addr);

    void update_status();
    void fire_interrupt(uint32_t intr_bits);

    uint32_t intr_state  = 0x0;
    uint32_t intr_enable = 0x0;
    uint32_t ctrl        = 0x0;
    uint32_t status      = STATUS_FMTEMPTY | STATUS_RXEMPTY | STATUS_HOSTIDLE | STATUS_TXEMPTY |STATUS_ACQEMPTY | STATUS_TARGETIDLE;
    uint32_t fifo_ctrl   = 0x0;

    uint32_t target_fifo_config = 0x0;
    uint32_t target_id          = 0x0;
    uint32_t target_ack_ctrl    = 0x0;
    uint32_t target_events      = 0x0;

    std::queue<I2CCommand> fmt_fifo;
    std::queue<uint8_t>    rx_fifo;

    std::queue<AcqEntry>   acq_fifo;
    std::queue<uint8_t>    tx_fifo;

    I2CState state        = I2CState::Idle;
    uint8_t  current_addr = 0;

    I2CTargetState target_state = I2CTargetState::Idle;
};
