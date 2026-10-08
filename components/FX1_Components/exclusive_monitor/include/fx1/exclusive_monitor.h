#pragma once
#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/exclusive_monitor_if.h>

namespace fx1 {

// The exclusive monitor of one FX1 address space (plan C7): the LR/SC
// reservations of the CPU harts, and the brackets that make an SC's
// check-and-store and an AMO's load-modify-store atomic against every other
// master. The CPU backend calls it directly (riscv_vp_plusplus_options::
// exclusive_monitor); a non-CPU master reaches it through a WriteGuard in
// front of its bus port. Hook contract: cdc/cpu/exclusive_monitor_if.h.
//
// Reservation granule: `granule` bytes, naturally aligned; 64 B on FX1 (VP
// placeholder, pending HAS). A write by any other master that overlaps the
// granule cancels the reservation, whichever bytes of it the LR read. A
// hart's own stores do not cancel its reservation. A bracket covers the same
// granule: while it is open, an overlapping write by another master waits, and
// opening it waits for overlapping writes already in flight.
//
// Not covered: debug transport (ELF loading, --dump) bypasses the monitor.
class ExclusiveMonitor final : public cdc::cpu::exclusive_monitor_if {
public:
    struct Stats {
        std::uint64_t reservations = 0;       // LR
        std::uint64_t sc_succeeded = 0;
        std::uint64_t sc_failed = 0;          // no, lost or mismatched reservation
        std::uint64_t cancelled_by_cpu = 0;   // reservation cancelled by another hart's write
        std::uint64_t cancelled_by_device = 0;// ... by a non-CPU master's write
        std::uint64_t writes_refused = 0;     // try_begin_write refused (each retry counts)
        std::uint64_t brackets_waited = 0;    // atomic_begin had to drain in-flight writes
    };

    ExclusiveMonitor(unsigned num_harts, std::uint64_t granule);

    void load_reserved(unsigned hart, std::uint64_t addr, unsigned size) override;
    void drop_reservation(unsigned hart) override;
    void atomic_begin(unsigned hart, std::uint64_t addr, unsigned size) override;
    bool take_reservation(unsigned hart, std::uint64_t addr, unsigned size) override;
    void atomic_end(unsigned hart) override;
    bool try_begin_write(unsigned master, std::uint64_t addr, unsigned size,
                         std::uint64_t& ticket) override;
    void end_write(std::uint64_t ticket) override;
    void reset_hart(unsigned hart) override;
    const sc_core::sc_event& changed() const override { return changed_; }

    std::uint64_t granule() const noexcept { return granule_; }
    unsigned num_harts() const noexcept { return static_cast<unsigned>(reservations_.size()); }
    const Stats& stats() const noexcept { return stats_; }
    // Introspection for tests.
    bool reserved(unsigned hart, std::uint64_t* base = nullptr) const;
    bool bracket_open(unsigned hart) const;
    std::size_t writes_in_flight() const noexcept { return writes_.size(); }

private:
    struct Slot {
        bool valid = false;
        std::uint64_t base = 0; // granule base
    };
    struct Write {
        std::uint64_t ticket;
        unsigned master;
        std::uint64_t lo, hi; // [lo, hi)
    };

    std::uint64_t base_of(std::uint64_t addr) const { return addr & ~(granule_ - 1); }
    bool hits(std::uint64_t base, std::uint64_t lo, std::uint64_t hi) const
    {
        return lo < base + granule_ && base < hi;
    }
    void check_hart(unsigned hart, const char* what) const;
    bool foreign_write_in_flight(unsigned hart, std::uint64_t base) const;

    std::uint64_t granule_;
    std::vector<Slot> reservations_;
    std::vector<Slot> brackets_;
    std::vector<Write> writes_;
    std::uint64_t next_ticket_ = 1;
    sc_core::sc_event changed_;
    Stats stats_;
};

// Pass-through in front of a non-CPU master's bus port (SYS_DMA, ISP_ODMA).
// Every write is bracketed with the monitor: it waits while a CPU atomic
// bracket overlaps it, and on completion cancels the CPU reservations it
// overlapped. Reads and debug accesses pass straight through.
class WriteGuard : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<WriteGuard> target{"target"};
    tlm_utils::simple_initiator_socket<WriteGuard> out{"out"};

    WriteGuard(sc_core::sc_module_name name, cdc::cpu::exclusive_monitor_if& monitor,
               unsigned master_id);

    std::uint64_t writes() const noexcept { return writes_; }
    std::uint64_t writes_held() const noexcept { return held_; } // waited for a CPU bracket

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);

    cdc::cpu::exclusive_monitor_if& monitor_;
    unsigned master_;
    std::uint64_t writes_ = 0, held_ = 0;
};

} // namespace fx1
