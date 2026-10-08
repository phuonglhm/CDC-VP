#pragma once
#include <cstdint>
#include <vector>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace fx1 {
// Core-local interruptor for N harts, SiFive register layout:
//   0x0000 + 4*h   msip[h]      bit 0 drives msip_irq[h]
//   0x4000 + 8*h   mtimecmp[h]  64-bit, mtip_irq[h] = (mtime >= mtimecmp[h])
//   0xBFF8         mtime        64-bit, shared, counts at `mtime_hz`
// Registers take naturally aligned 4- or 8-byte accesses (RV32 uses 32-bit
// halves). Any other offset inside the aperture or any other size is a slave
// error, which a hart reports as an access fault. mtime is derived from
// SystemC time, so it never drifts and costs nothing while idle; writing it
// re-bases the counter. mtimecmp resets to all-ones (no interrupt).
//
// Each access annotates `access_latency`. The FX1 bus consumes it after the
// register update, so a write that clears an interrupt has already lowered the
// line when the initiating hart resumes.
class Clint : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Clint> socket{"socket"};
    sc_core::sc_vector<sc_core::sc_out<bool>> msip_irq{"msip_irq"};
    sc_core::sc_vector<sc_core::sc_out<bool>> mtip_irq{"mtip_irq"};

    SC_HAS_PROCESS(Clint);
    Clint(sc_core::sc_module_name name, unsigned harts, std::uint64_t mtime_hz,
          sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

    // Current mtime, e.g. for the `time` CSR of each hart.
    std::uint64_t mtime() const;
    std::uint64_t mtimecmp(unsigned hart) const { return mtimecmp_.at(hart); }
    unsigned harts() const noexcept { return harts_; }
    sc_core::sc_time tick() const noexcept { return tick_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    // Returns false for an unmapped offset or unsupported size/alignment.
    bool access(std::uint64_t offset, bool write, unsigned char* data, unsigned length);
    bool access64(std::uint64_t& reg, unsigned part, bool write, unsigned char* data,
                  unsigned length);
    void update();

    unsigned harts_;
    sc_core::sc_time tick_;
    sc_core::sc_time latency_;
    std::vector<bool> msip_;
    std::vector<std::uint64_t> mtimecmp_;
    std::uint64_t offset_ = 0;  // mtime = elapsed ticks + offset_ (mod 2^64)
    sc_core::sc_event update_event_;
};
} // namespace fx1
