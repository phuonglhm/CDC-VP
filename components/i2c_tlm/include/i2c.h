#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <queue>
#include <cstdint>
#include <map>
#include <vector>

#include "i2c_device.h"

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
#define FDATA_RCONT (1 << 11)  /* functional host: ACK the last byte read, the read continues */
#define FDATA_NAKOK (1 << 12)  /* functional host: a NACK of this byte does not halt */

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
#define INTR_NAK          (1 << 4)  /* functional host: an address or data byte was not acknowledged */

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
    bool rcont = false;
    bool nakok = false;
};

struct AcqEntry {
    uint8_t data;
    uint8_t signal;
};

// Host-mode behaviour. Legacy (default, for the platforms built on it so
// far): FDATA commands complete at once when a STOP is written, no device
// answers, a read returns 0xAB. Functional: the FMT FIFO is executed by the
// controller in bus order against the devices attached with attach(); every
// bit takes 1/scl_hz (START and STOP one bit, a byte and its ACK nine);
// repeated START; FDATA READB reads FBYTE bytes (0 = 256), the last one NACKed
// unless RCONT; an unacknowledged address or data byte (no device, or the
// device NACKs) raises INTR_NAK and halts the transaction: the commands up to
// its STOP are discarded, a STOP is sent, INTR_CMD_COMPLETE follows; NAKOK
// lets the transaction continue. A full RX FIFO stretches the clock (no byte
// is lost).
struct i2c_config {
    bool functional_host = false;
    double scl_hz = 400e3;
    bool verbose = false;  // interrupt trace on stdout
};

class i2c : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<i2c> socket;
    sc_core::sc_out<bool> irq;
    SC_HAS_PROCESS(i2c);
    explicit i2c(sc_core::sc_module_name name, const i2c_config& cfg = i2c_config());

    // Functional host mode: a device on the bus at 7-bit address addr7.
    void attach(uint8_t addr7, i2c_device* dev);

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
    void host_thread();
    void irq_method();
    void end_transaction(bool discard_rest, bool complete);  // functional: STOP, host idle
    sc_core::sc_event irq_ev_;
    sc_core::sc_time bits(unsigned n) const { return bit_time_ * static_cast<double>(n); }

    i2c_config cfg_;
    sc_core::sc_time bit_time_;
    std::map<uint8_t, i2c_device*> devices_;
    sc_core::sc_event fmt_ev_, rx_ev_;
    bool host_busy_ = false;   // functional: a transaction is open on the bus
    bool discarding_ = false;  // functional: commands of a NACKed transaction dropped up to its STOP
    bool abort_req_ = false;   // functional: FMT FIFO reset with a transaction open: end it with STOP
    i2c_device* cur_dev_ = nullptr;

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
