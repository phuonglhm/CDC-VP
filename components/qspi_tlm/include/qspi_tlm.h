#ifndef QSPI_TLM_H
#define QSPI_TLM_H

#include <cstdint>
#include <queue>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

static constexpr std::uint32_t QSPI_CTRL = 0x00;
static constexpr std::uint32_t QSPI_STATUS = 0x04;
static constexpr std::uint32_t QSPI_CMD = 0x08;
static constexpr std::uint32_t QSPI_ADDR = 0x0c;
static constexpr std::uint32_t QSPI_LEN = 0x10;
static constexpr std::uint32_t QSPI_CFG = 0x14;
static constexpr std::uint32_t QSPI_DATA = 0x18;
static constexpr std::uint32_t QSPI_INT_EN = 0x1c;
static constexpr std::uint32_t QSPI_INT_STATUS = 0x20;
static constexpr std::uint32_t QSPI_START = 0x24;

static constexpr std::uint32_t QSPI_CTRL_EN = 1u << 0;
static constexpr std::uint32_t QSPI_STATUS_BUSY = 1u << 0;
static constexpr std::uint32_t QSPI_STATUS_RXNE = 1u << 1;
static constexpr std::uint32_t QSPI_INT_DONE = 1u << 0;

class qspi_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<qspi_tlm> from_apb_socket;
    tlm_utils::simple_initiator_socket<qspi_tlm> to_flash_socket;
    sc_core::sc_out<bool> irq;
    sc_core::sc_in<bool> reset_n;

    SC_HAS_PROCESS(qspi_tlm);
    explicit qspi_tlm(sc_core::sc_module_name name);

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

private:
    void handle_reset();
    void update_irq_output();
    void reset_state();
    void execute_transfer(sc_core::sc_time& delay);

    std::vector<unsigned char> build_frame() const;
    unsigned prefix_length_for_opcode(std::uint8_t opcode) const;
    unsigned dummy_bytes_for_opcode(std::uint8_t opcode) const;
    bool opcode_has_address(std::uint8_t opcode) const;

    void update_status_rxne();
    bool read_reg(std::uint32_t offset, std::uint32_t& value);
    bool write_reg(std::uint32_t offset, std::uint32_t value, sc_core::sc_time& delay);

    std::uint32_t ctrl_reg_ = 0;
    std::uint32_t status_reg_ = 0;
    std::uint32_t cmd_reg_ = 0;
    std::uint32_t addr_reg_ = 0;
    std::uint32_t len_reg_ = 0;
    std::uint32_t cfg_reg_ = 1;
    std::uint32_t int_en_reg_ = 0;
    std::uint32_t int_status_reg_ = 0;

    std::queue<std::uint8_t> rx_fifo_;
    bool irq_level_ = false;
    sc_core::sc_event irq_update_event_;
};

} // namespace cdc::components

#endif
