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
    tlm_utils::simple_target_socket<flash_nor_tlm> from_qspi_socket;

    SC_HAS_PROCESS(flash_nor_tlm);
    explicit flash_nor_tlm(sc_core::sc_module_name name, std::size_t size_bytes);

    void load(const std::uint8_t* data, std::size_t len, std::uint64_t offset = 0);
    std::size_t size() const { return mem_.size(); }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    static std::uint32_t read_addr24(const unsigned char* data);
    std::uint8_t read_mem(std::uint32_t addr) const;

    std::vector<std::uint8_t> mem_;
};

} // namespace cdc::components

#endif
