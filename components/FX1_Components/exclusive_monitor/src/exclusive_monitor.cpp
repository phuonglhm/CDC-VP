#include "fx1/exclusive_monitor.h"

#include <algorithm>
#include <string>

namespace fx1 {

ExclusiveMonitor::ExclusiveMonitor(unsigned num_harts, std::uint64_t granule)
    : granule_(granule), reservations_(num_harts), brackets_(num_harts)
{
    if (num_harts == 0 || granule == 0 || (granule & (granule - 1)) != 0)
        SC_REPORT_FATAL("fx1::ExclusiveMonitor", "needs at least one hart and a power-of-two granule");
}

void ExclusiveMonitor::check_hart(unsigned hart, const char* what) const
{
    if (hart >= reservations_.size())
        SC_REPORT_FATAL("fx1::ExclusiveMonitor",
                        (std::string(what) + ": hart " + std::to_string(hart) + " out of range").c_str());
}

bool ExclusiveMonitor::foreign_write_in_flight(unsigned hart, std::uint64_t base) const
{
    return std::any_of(writes_.begin(), writes_.end(),
                       [&](const Write& w) { return w.master != hart && hits(base, w.lo, w.hi); });
}

void ExclusiveMonitor::load_reserved(unsigned hart, std::uint64_t addr, unsigned)
{
    check_hart(hart, "load_reserved");
    reservations_[hart] = {true, base_of(addr)};
    ++stats_.reservations;
}

void ExclusiveMonitor::drop_reservation(unsigned hart)
{
    check_hart(hart, "drop_reservation");
    reservations_[hart].valid = false;
}

void ExclusiveMonitor::atomic_begin(unsigned hart, std::uint64_t addr, unsigned)
{
    check_hart(hart, "atomic_begin");
    const std::uint64_t base = base_of(addr);
    brackets_[hart] = {true, base}; // new overlapping writes are refused from here on
    if (foreign_write_in_flight(hart, base)) {
        ++stats_.brackets_waited;
        do {
            sc_core::wait(changed_);
        } while (foreign_write_in_flight(hart, base));
    }
}

bool ExclusiveMonitor::take_reservation(unsigned hart, std::uint64_t addr, unsigned size)
{
    check_hart(hart, "take_reservation");
    Slot& r = reservations_[hart];
    const bool ok = r.valid && r.base == base_of(addr) && base_of(addr + size - 1) == r.base;
    r.valid = false;
    ++(ok ? stats_.sc_succeeded : stats_.sc_failed);
    return ok;
}

void ExclusiveMonitor::atomic_end(unsigned hart)
{
    check_hart(hart, "atomic_end");
    brackets_[hart].valid = false;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

bool ExclusiveMonitor::try_begin_write(unsigned master, std::uint64_t addr, unsigned size,
                                       std::uint64_t& ticket)
{
    const std::uint64_t lo = addr, hi = addr + (size ? size : 1u);
    for (unsigned h = 0; h < brackets_.size(); ++h) {
        if (h != master && brackets_[h].valid && hits(brackets_[h].base, lo, hi)) {
            ++stats_.writes_refused;
            return false;
        }
    }
    ticket = next_ticket_++;
    writes_.push_back({ticket, master, lo, hi});
    return true;
}

void ExclusiveMonitor::end_write(std::uint64_t ticket)
{
    const auto it = std::find_if(writes_.begin(), writes_.end(),
                                 [&](const Write& w) { return w.ticket == ticket; });
    if (it == writes_.end()) {
        SC_REPORT_FATAL("fx1::ExclusiveMonitor", "end_write: unknown ticket");
        return;
    }
    const Write w = *it;
    writes_.erase(it);
    const bool from_cpu = w.master < reservations_.size();
    for (unsigned h = 0; h < reservations_.size(); ++h) {
        Slot& r = reservations_[h];
        if (h != w.master && r.valid && hits(r.base, w.lo, w.hi)) {
            r.valid = false;
            ++(from_cpu ? stats_.cancelled_by_cpu : stats_.cancelled_by_device);
        }
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void ExclusiveMonitor::reset_hart(unsigned hart)
{
    check_hart(hart, "reset_hart");
    reservations_[hart].valid = false;
    brackets_[hart].valid = false;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

bool ExclusiveMonitor::reserved(unsigned hart, std::uint64_t* base) const
{
    check_hart(hart, "reserved");
    if (base) *base = reservations_[hart].base;
    return reservations_[hart].valid;
}

bool ExclusiveMonitor::bracket_open(unsigned hart) const
{
    check_hart(hart, "bracket_open");
    return brackets_[hart].valid;
}

WriteGuard::WriteGuard(sc_core::sc_module_name name, cdc::cpu::exclusive_monitor_if& monitor,
                       unsigned master_id)
    : sc_core::sc_module(name), monitor_(monitor), master_(master_id)
{
    target.register_b_transport(this, &WriteGuard::b_transport);
    target.register_transport_dbg(this, &WriteGuard::transport_dbg);
}

void WriteGuard::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
{
    if (tx.get_command() != tlm::TLM_WRITE_COMMAND) {
        out->b_transport(tx, delay);
        return;
    }
    unsigned span = tx.get_data_length();
    if (tx.get_streaming_width() != 0 && tx.get_streaming_width() < span) span = tx.get_streaming_width();
    std::uint64_t ticket = 0;
    if (!monitor_.try_begin_write(master_, tx.get_address(), span, ticket)) {
        ++held_;
        // Bring the annotation into simulated time before waiting on an event,
        // then re-check: the bracket may have closed during that wait, and its
        // notification would not be seen again.
        sc_core::wait(delay);
        delay = sc_core::SC_ZERO_TIME;
        while (!monitor_.try_begin_write(master_, tx.get_address(), span, ticket))
            sc_core::wait(monitor_.changed());
    }
    ++writes_;
    try {
        out->b_transport(tx, delay);
    } catch (...) {
        monitor_.end_write(ticket);
        throw;
    }
    monitor_.end_write(ticket);
}

unsigned WriteGuard::transport_dbg(tlm::tlm_generic_payload& tx)
{
    return out->transport_dbg(tx);
}

} // namespace fx1
