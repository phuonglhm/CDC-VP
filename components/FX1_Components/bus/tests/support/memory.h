#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <vector>

namespace bus::test {
// Byte storage for ROM/SRAM and stand-in AES/QSPI/PP register banks.
// AES encryption, QSPI protocol and real peripheral side effects are TODO.
class Memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Memory> target{"target"};
    Memory(sc_core::sc_module_name name, std::size_t bytes, bool readonly = false,
           unsigned latency_ns = 10);
    void load(std::size_t offset, const std::vector<unsigned char>& bytes);
private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    std::vector<unsigned char> bytes_;
    bool readonly_;
    unsigned latency_ns_;
};
} // namespace bus::test
