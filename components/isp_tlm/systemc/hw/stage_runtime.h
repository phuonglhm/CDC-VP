#ifndef ISP_STAGE_RUNTIME_H
#define ISP_STAGE_RUNTIME_H

#include "line_channel.h"

#include <systemc>

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace isp_tlm {

enum class timing_source { assumed, measured, rtl, hls };

struct stage_timing {
    std::uint32_t compute_latency_cycles = 1;
    std::uint32_t pixel_ii = 1;
    std::uint32_t pixels_per_cycle = 1;
    std::uint32_t max_in_flight_lines = 1;
    timing_source source = timing_source::assumed;
    sc_core::sc_time cycle_period = sc_core::sc_time(1, sc_core::SC_NS);
};

template <class T>
struct pending_line {
    write_handle<T> handle;
    line_meta meta{};
    sc_core::sc_time ready_at = sc_core::SC_ZERO_TIME;
    std::uint64_t sequence = 0;
};

template <class T>
using scheduled_line = pending_line<T>;

template <class T>
using pending = pending_line<T>;

template <class T>
using scheduled = pending_line<T>;

template <class T>
class stage_runtime {
public:
    using pending = pending_line<T>;
    using scheduled = scheduled_line<T>;

    struct snapshot_type {
        std::uint64_t issued = 0;
        std::uint64_t completed = 0;
        std::uint32_t in_flight = 0;
        std::uint32_t high_water_in_flight = 0;
        std::uint64_t issue_stalls = 0;
    };

    explicit stage_runtime(const stage_timing& timing = stage_timing{})
        : m_timing(timing), m_pending(timing.max_in_flight_lines == 0
                                           ? nullptr
                                           : std::make_unique<pending[]>(timing.max_in_flight_lines)) {
        validate_timing();
    }

    stage_runtime(const char* name, const stage_timing& timing)
        : m_timing(timing),
          m_pending(timing.max_in_flight_lines == 0
                        ? nullptr
                        : std::make_unique<pending[]>(timing.max_in_flight_lines)),
          m_completion_event(name) {
        validate_timing();
    }

    stage_runtime(const stage_runtime&) = delete;
    stage_runtime& operator=(const stage_runtime&) = delete;

    bool can_issue(const sc_core::sc_time& now, std::uint32_t width) const noexcept {
        return m_in_flight < m_timing.max_in_flight_lines && now >= m_next_issue;
    }

    void schedule(write_handle<T>&& handle, const line_meta& meta,
                  const sc_core::sc_time& now) {
        if (!handle.valid()) {
            throw std::logic_error("cannot schedule an invalid line handle");
        }
        if (m_in_flight >= m_timing.max_in_flight_lines || now < m_next_issue) {
            ++m_issue_stalls;
            throw std::logic_error("stage runtime cannot issue at this time");
        }
        std::size_t index = 0;
        while (index < m_timing.max_in_flight_lines && m_pending[index].handle.valid()) {
            ++index;
        }
        if (index == m_timing.max_in_flight_lines) {
            throw std::logic_error("stage runtime pending storage is full");
        }
        pending& item = m_pending[index];
        item.handle = std::move(handle);
        item.meta = meta;
        item.ready_at = now + m_timing.cycle_period *
                                  static_cast<double>(m_timing.compute_latency_cycles);
        item.sequence = m_next_sequence++;
        ++m_in_flight;
        ++m_issued;
        if (m_in_flight > m_high_water_in_flight) {
            m_high_water_in_flight = m_in_flight;
        }
        m_next_issue = now + issue_interval(width_or_meta_width(meta));
    }

    std::size_t retire_ready(line_channel<T>& channel, const sc_core::sc_time& now) {
        std::size_t retired = 0;
        while (m_in_flight != 0) {
            std::size_t index = find_next_sequence();
            if (index == m_timing.max_in_flight_lines || m_pending[index].ready_at > now) {
                break;
            }
            pending& item = m_pending[index];
            channel.publish(std::move(item.handle), item.meta);
            item.meta = line_meta{};
            item.ready_at = sc_core::SC_ZERO_TIME;
            item.sequence = 0;
            --m_in_flight;
            ++m_completed;
            ++retired;
            m_completion_event.notify(sc_core::SC_ZERO_TIME);
        }
        return retired;
    }

    bool has_in_flight() const noexcept { return m_in_flight != 0; }

    sc_core::sc_time next_wakeup() const {
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        sc_core::sc_time wake =
            m_next_issue > now ? m_next_issue : now;
        if (m_in_flight != 0) {
            const std::size_t index = find_next_sequence();
            if (index != m_timing.max_in_flight_lines) {
                const sc_core::sc_time ready = m_pending[index].ready_at;
                if (ready > now && (wake <= now || ready < wake)) {
                    wake = ready;
                }
            }
        }
        return wake;
    }

    sc_core::sc_event& completion_event() noexcept { return m_completion_event; }
    const sc_core::sc_event& completion_event() const noexcept { return m_completion_event; }

    snapshot_type snapshot() const noexcept {
        return {m_issued, m_completed, m_in_flight, m_high_water_in_flight, m_issue_stalls};
    }

    sc_core::sc_time issue_interval(std::uint32_t width) const {
        const std::uint32_t ppc = m_timing.pixels_per_cycle == 0 ? 1 : m_timing.pixels_per_cycle;
        const std::uint64_t transfer_cycles =
            (static_cast<std::uint64_t>(width) + ppc - 1u) / ppc;
        const std::uint64_t issue_cycles =
            transfer_cycles > m_timing.pixel_ii ? transfer_cycles : m_timing.pixel_ii;
        return m_timing.cycle_period * static_cast<double>(issue_cycles == 0 ? 1 : issue_cycles);
    }

    const stage_timing& timing() const noexcept { return m_timing; }

private:
    void validate_timing() const {
        if (m_timing.max_in_flight_lines == 0 || m_timing.pixel_ii == 0 ||
            m_timing.pixels_per_cycle == 0 || m_timing.cycle_period <= sc_core::SC_ZERO_TIME) {
            throw std::invalid_argument("stage timing values must be positive");
        }
    }

    std::uint32_t width_or_meta_width(const line_meta& meta) const noexcept {
        return meta.width_pixels;
    }

    std::size_t find_next_sequence() const noexcept {
        std::size_t best = m_timing.max_in_flight_lines;
        std::uint64_t sequence = std::numeric_limits<std::uint64_t>::max();
        for (std::size_t i = 0; i < m_timing.max_in_flight_lines; ++i) {
            if (m_pending[i].handle.valid() && m_pending[i].sequence < sequence) {
                sequence = m_pending[i].sequence;
                best = i;
            }
        }
        return best;
    }

    stage_timing m_timing;
    std::unique_ptr<pending[]> m_pending;
    sc_core::sc_event m_completion_event;
    sc_core::sc_time m_next_issue = sc_core::SC_ZERO_TIME;
    std::uint64_t m_next_sequence = 0;
    std::uint64_t m_issued = 0;
    std::uint64_t m_completed = 0;
    std::uint64_t m_issue_stalls = 0;
    std::uint32_t m_in_flight = 0;
    std::uint32_t m_high_water_in_flight = 0;
};

} // namespace isp_tlm

#endif // ISP_STAGE_RUNTIME_H
