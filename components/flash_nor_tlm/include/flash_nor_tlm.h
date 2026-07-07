#ifndef FLASH_NOR_TLM_H
#define FLASH_NOR_TLM_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

class flash_nor_tlm : public sc_core::sc_module {
public:
    // Both faces are optional so an instance can serve QSPI-only (flash0)
    // or plain-SPI-only (spi_flash0) without dangling-port elaboration errors.
    tlm_utils::simple_target_socket_optional<flash_nor_tlm> from_qspi_socket;

    // Byte-stream face for a plain SPI master (spi_tlm): one full-duplex
    // frame per b_transport (TLM_WRITE, 2-byte payload, low byte = MOSI;
    // the MISO byte is written back into the low byte). 8-bit frames only.
    // A state machine decodes NOR READ (CMD 0x03 + 3 addr bytes -> data out);
    // other opcodes are ignored until chip-select deasserts. Optional: the
    // QSPI-only instantiation leaves it unbound.
    tlm_utils::simple_target_socket_optional<flash_nor_tlm> from_spi_socket;

    SC_HAS_PROCESS(flash_nor_tlm);
    explicit flash_nor_tlm(sc_core::sc_module_name name, std::size_t size_bytes);

    void load(const std::uint8_t* data, std::size_t len, std::uint64_t offset = 0);
    std::size_t size() const { return mem_.size(); }

    // Chip-select for the byte-stream face (active state, not line level:
    // true = selected). Deassert resets the command state machine. Wired by
    // the platform from the SPI controller's CS output; while deselected the
    // flash ignores MOSI and leaves the frame untouched (loopback, matching
    // the legacy dummy-sink behavior masters relied on before CS existed).
    void spi_cs(bool selected);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void spi_b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    std::uint8_t spi_exchange(std::uint8_t mosi);

    static std::uint32_t read_addr24(const unsigned char* data);
    std::uint8_t read_mem(std::uint32_t addr) const;

    enum class spi_state { idle, addr, data, ignore };
    spi_state spi_state_ = spi_state::idle;
    std::uint32_t spi_addr_ = 0;
    unsigned spi_addr_cnt_ = 0;
    bool spi_selected_ = false;

    std::vector<std::uint8_t> mem_;
};

} // namespace cdc::components

#endif
