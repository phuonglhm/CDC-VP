#pragma once
#include "fx1/sparse_storage.h"
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace fx1 {
// TLM-2.0 RAM/ROM over SparseStorage. Addresses are target-local
// (0 .. size-1); the bus translates global addresses.
//
// b_transport: blocking, annotates `latency` (the FX1 bus consumes it), honours
// byte enables, rejects the whole access before touching memory when it does
// not fit. Responses are chosen so a RISC-V hart turns guest mistakes into
// access faults: out of range -> ADDRESS_ERROR, write to read-only -> GENERIC
// (slave error). Unsupported payload forms (streaming, bad byte-enable
// encoding) are integration defects and return BURST/BYTE_ENABLE errors.
// transport_dbg: untimed backdoor; also writes read-only memory so images can
// be preloaded. DMI is never granted, so every access stays visible on the bus.
//
// This is a functional RAM, not a DDR controller: no CSRs, init or training.
class SparseRam : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SparseRam> socket{"socket"};

    SparseRam(sc_core::sc_module_name name, std::uint64_t size,
              sc_core::sc_time latency = sc_core::sc_time(10, sc_core::SC_NS),
              bool read_only = false, std::uint8_t fill = 0);

    SparseStorage& storage() noexcept { return storage_; }
    const SparseStorage& storage() const noexcept { return storage_; }
    bool read_only() const noexcept { return read_only_; }
    std::uint64_t reads() const noexcept { return reads_; }
    std::uint64_t writes() const noexcept { return writes_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);

    SparseStorage storage_;
    sc_core::sc_time latency_;
    bool read_only_;
    std::uint64_t reads_ = 0, writes_ = 0;
};
} // namespace fx1
