#include "fu3_wdt/wdt.h"
#include <algorithm>
#include <stdexcept>

namespace fu3_wdt {
namespace {
const sc_core::sc_time kNoDeadline = sc_core::sc_max_time();
}

Watchdog::Watchdog(sc_core::sc_module_name name, sc_core::sc_time tick,
                   unsigned apb_wait_cycles, sc_core::sc_time apb_period)
    : sc_module(name), apb_wait_cycles_(apb_wait_cycles), tick_(tick),
      apb_latency_(apb_period * static_cast<double>(apb_wait_cycles)) {
    if (tick <= sc_core::SC_ZERO_TIME || apb_period <= sc_core::SC_ZERO_TIME || apb_wait_cycles == 0)
        throw std::invalid_argument("WDT clock period and APB wait cycles must be positive");
    socket.register_b_transport(this, &Watchdog::transport);
    socket.register_transport_dbg(this, &Watchdog::debug);
    SC_THREAD(run);
    SC_METHOD(publish);
    sensitive << outputs_;
}

void Watchdog::set_clock(sc_core::sc_time tick, sc_core::sc_time apb_period) {
    if (tick == sc_core::SC_ZERO_TIME || apb_period == sc_core::SC_ZERO_TIME || tick == tick_)
        return;
    if (sc_core::sc_is_running())
        synchronize();   // commit the ticks elapsed at the old period
    tick_ = tick;
    apb_latency_ = apb_period * static_cast<double>(apb_wait_cycles_);
    if (sc_core::sc_is_running())
        changed_.notify(sc_core::SC_ZERO_TIME);
}

void Watchdog::set_clock_enabled(bool on) {
    if (on == clock_enabled_)
        return;
    if (sc_core::sc_is_running())
        synchronize();   // ticks before the gate change count with the old state
    clock_enabled_ = on;
    if (sc_core::sc_is_running())
        changed_.notify(sc_core::SC_ZERO_TIME);
}

void Watchdog::programming_reset() {
    load_=FU3_WDT_RESET_VALUE;control_=FU3_WDT_RESEN;locked_=false;halt_=false;
}
void Watchdog::watchdog_reset() {
    count_ = FU3_WDT_RESET_VALUE;
    core_reset_ = true;
    timeout_seen_ = false;
    reset_pending_ = false;
}

void Watchdog::watchdog_tick() {
    if (!wdogres_n.read() || reset_pending_)
        return;
    if (!(control_ & FU3_WDT_INTEN)) {
        count_ = load_;
        timeout_seen_ = false;
        return;
    }
    if (halt_)
        return;

    if (count_ != 0) {
        --count_;
        return;
    }

    if (!timeout_seen_) {
        timeout_seen_ = true;
        count_ = load_;
    } else if (control_ & FU3_WDT_RESEN) {
        timeout_seen_ = false;
        // The request remains asserted until watchdog-side reset.
        reset_pending_ = true;
    } else {
        // V1.2 clears the second-timeout flag even when reset generation is masked.
        timeout_seen_ = false;count_ = load_;
    }
}

void Watchdog::synchronize() {
    const auto now = sc_core::sc_time_stamp();
    if (!preset_n.read())
        programming_reset();
    if (!wdogres_n.read()) {
        watchdog_reset(); // Watchdog-side reset leaves programming state intact.
        last_tick_ = now - sc_core::sc_time::from_value(now.value() % tick_.value());
        outputs_.notify(sc_core::SC_ZERO_TIME);
        return;
    }

    // VALUE holds its documented reset value while reset is asserted. On
    // release, start a fresh interval using the retained programming state.
    if(core_reset_){count_=load_;core_reset_=false;}
    const auto elapsed_ticks = (now.value() - last_tick_.value()) / tick_.value();
    auto remaining = elapsed_ticks;
    while (remaining && clock_enabled_ && !reset_pending_ && (control_ & FU3_WDT_INTEN) && !halt_) {
        // A count of N expires on the N+1th eligible edge, when old count is zero.
        const std::uint64_t until_timeout = static_cast<std::uint64_t>(count_) + 1;
        const auto step = std::min<std::uint64_t>(remaining, until_timeout);
        last_tick_ += sc_core::sc_time::from_value(step * tick_.value());
        remaining -= step;
        if (step == until_timeout) {
            count_ = 0; // All preceding decrement edges have elapsed before this timeout edge.
            watchdog_tick();
        } else
            count_ -= static_cast<std::uint32_t>(step);
    }
    if (remaining)
        last_tick_ += sc_core::sc_time::from_value(remaining * tick_.value());

    if (!(control_ & FU3_WDT_INTEN) && !reset_pending_) {
        count_ = load_;
        timeout_seen_ = false;
    }
    outputs_.notify(sc_core::SC_ZERO_TIME);
}

void Watchdog::run() {
    while (true) {
        synchronize();
        const auto events = changed_ | preset_n.value_changed_event() |
                            wdogres_n.value_changed_event();
        const auto now = sc_core::sc_time_stamp();
        auto next = kNoDeadline;
        if (clock_enabled_ && wdogres_n.read() && !reset_pending_ && !halt_ &&
            (control_ & FU3_WDT_INTEN)) {
            const auto until_timeout = static_cast<std::uint64_t>(count_) + 1;
            next = last_tick_ + tick_ * static_cast<double>(until_timeout);
        }
        if (next == kNoDeadline)
            wait(events);
        else
            wait(next - now, events);
    }
}

void Watchdog::publish() {
    if(wdogpause.size())wdogpause->write(halt_ && wdogres_n.read());
    irq.write(timeout_seen_ && (control_ & FU3_WDT_INTEN));
    reset_request.write(reset_pending_ && wdogres_n.read());
}

std::uint32_t Watchdog::read(unsigned offset) const {
    switch (offset) {
    case FU3_WDT_LOCK: return locked_?1u:0u;
    case FU3_WDT_CNT_LD: return load_;
    case FU3_WDT_CNT_VAL: return count_;
    case FU3_WDT_CNT_CTRL: return control_;
    case FU3_WDT_INT_RAW_STAT: return timeout_seen_ ? FU3_WDT_TIMEOUT_SEEN : 0;
    case FU3_WDT_INT_MASK_STAT:
        return timeout_seen_ && (control_ & FU3_WDT_INTEN) ? FU3_WDT_TIMEOUT_SEEN : 0;
    case FU3_WDT_INT_CLR: return 0;
    case FU3_WDT_DBG_STAT: return halt_ ? 1u : 0u;
    default: return 0;
    }
}

void Watchdog::write(unsigned offset, std::uint32_t value, std::uint32_t mask) {
    if(!mask)return;
    if(offset==FU3_WDT_LOCK){locked_=mask!=UINT32_MAX || value!=FU3_WDT_UNLOCK_KEY;return;}
    if (locked_)
        return; // Locked writes complete without changing any WDT state.

    switch (offset) {
    case FU3_WDT_CNT_LD:
        load_ = (load_&~mask)|(value&mask);
        if (!reset_pending_) {
            count_ = load_;
            timeout_seen_ = load_ == 0 && (control_ & FU3_WDT_INTEN);
            // With INTEN set, zero load reports immediately; reset waits for a WDT edge.
        }
        break;
    case FU3_WDT_CNT_CTRL:
        control_ = ((control_&~mask)|(value&mask)) & 0x7fu;
        halt_ = (control_ & FU3_WDT_DBGEN) != 0;
        if (!(control_ & FU3_WDT_INTEN) && !reset_pending_) {
            count_ = load_;
            timeout_seen_ = false;
        }
        break;
    case FU3_WDT_INT_CLR:
        if (!reset_pending_) {
            timeout_seen_ = false;
            count_ = load_;
        }
        break;
    default:
        break; // Read-only registers ignore writes.
    }
}

bool Watchdog::validate(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    auto fail = [&](tlm::tlm_response_status status) {
        tx.set_response_status(status);
        return false;
    };
    if (!tx.is_read() && !tx.is_write())
        return fail(tlm::TLM_COMMAND_ERROR_RESPONSE);
    if (!tx.get_data_ptr())
        return fail(tlm::TLM_GENERIC_ERROR_RESPONSE);
    if (tx.get_data_length() != 4 || tx.get_streaming_width() < 4)
        return fail(tlm::TLM_BURST_ERROR_RESPONSE);
    const auto address = tx.get_address();
    if ((address & 3u) != 0 || address >= FU3_WDT_WINDOW_END)
        return fail(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    switch (address) {
    case FU3_WDT_LOCK:
    case FU3_WDT_CNT_LD:
    case FU3_WDT_CNT_VAL:
    case FU3_WDT_CNT_CTRL:
    case FU3_WDT_INT_RAW_STAT:
    case FU3_WDT_INT_MASK_STAT:
    case FU3_WDT_INT_CLR:
    case FU3_WDT_DBG_STAT:
        break;
    default:
        return fail(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
    if (auto* byte_enable = tx.get_byte_enable_ptr()) {
        const auto length = tx.get_byte_enable_length();
        if (!length)
            return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        for (unsigned i = 0; i < 4; ++i)
            if (byte_enable[i % length] != 0xff && byte_enable[i % length] != 0)
                return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    }
    return true;
}

void Watchdog::transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    const auto incoming = delay;
    delay = sc_core::SC_ZERO_TIME;
    if (incoming != sc_core::SC_ZERO_TIME)
        wait(incoming);

    struct AccessGuard {
        sc_core::sc_mutex& mutex;
        explicit AccessGuard(sc_core::sc_mutex& value) : mutex(value) { mutex.lock(); }
        ~AccessGuard() { mutex.unlock(); }
    } access_guard(access_mutex_);

    if (!preset_n.read() || !wdogres_n.read()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    const auto reset_events = preset_n.value_changed_event() | wdogres_n.value_changed_event();
    wait(apb_latency_, reset_events); // Functional abstraction of the documented CDC wait states.
    if (!preset_n.read() || !wdogres_n.read()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    synchronize();
    if (!validate(tx))
        return;
    if (tx.is_read()) {
        const auto value = read(static_cast<unsigned>(tx.get_address()));
        for (unsigned i = 0; i < 4; ++i)
            if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])
                tx.get_data_ptr()[i] = static_cast<unsigned char>(value >> (8 * i));
    } else {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::uint32_t(tx.get_data_ptr()[i]) << (8 * i);
        std::uint32_t mask=0;
        for(unsigned i=0;i<4;++i)if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])mask|=255u<<(8*i);
        write(static_cast<unsigned>(tx.get_address()), value, mask);
    }
    synchronize();
    changed_.notify(sc_core::SC_ZERO_TIME);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned Watchdog::debug(tlm::tlm_generic_payload& tx) {
    if (!validate(tx))
        return 0;
    if (!tx.is_read()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }
    synchronize();
    const auto value = read(static_cast<unsigned>(tx.get_address()));
    for (unsigned i = 0; i < 4; ++i)
        if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])
                tx.get_data_ptr()[i] = static_cast<unsigned char>(value >> (8 * i));
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}
} // namespace fu3_wdt
