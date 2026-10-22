#include "ddr_sysctrl.h"

#include <algorithm>

#include <fx1/fx1_ddr_sysctrl.h>
#include <fx1_ddr3/fx1_ddr3_regs.h>
#include <fx1_ddr3/provisional_encodings.h>

namespace cdc::platforms::fx1_soc {

namespace {
// Times are functions, not namespace-scope sc_time objects: main() sets the
// time resolution (1 fs) before any sc_time exists.
// Register access latency of the glue block (VP placeholder).
sc_core::sc_time kAccessLatency() { return sc_core::sc_time(10, sc_core::SC_NS); }
// Reset pulse width and the reset-safe wait of a coordinated core reset (VP placeholders).
sc_core::sc_time kResetPulse() { return sc_core::sc_time(100, sc_core::SC_NS); }
sc_core::sc_time kResetSafeLimit() { return sc_core::sc_time(1, sc_core::SC_MS); }
// Back-off of a denied automatic wake: doubles from 1 us up to 1 ms.
sc_core::sc_time kWakeRetryMin() { return sc_core::sc_time(1, sc_core::SC_US); }
sc_core::sc_time kWakeRetryMax() { return sc_core::sc_time(1, sc_core::SC_MS); }

std::uint32_t load_le32(const unsigned char* p)
{
    return std::uint32_t{p[0]} | std::uint32_t{p[1]} << 8 | std::uint32_t{p[2]} << 16 | std::uint32_t{p[3]} << 24;
}
void store_le32(unsigned char* p, std::uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}
} // namespace

DdrSysctrl::DdrSysctrl(sc_core::sc_module_name name, fx1::ddr3::Ddr3Controller& ddr, bool preinit)
    : sc_module(name), ddr_(ddr), preinit_(preinit)
{
    socket.register_b_transport(this, &DdrSysctrl::b_transport);
    socket.register_transport_dbg(this, &DdrSysctrl::transport_dbg);
    SC_THREAD(sequencer);
}

bool DdrSysctrl::read_word(std::uint32_t off, std::uint32_t& v) const
{
    v = 0;
    switch (off) {
    case FX1_DDRSC_ID: v = FX1_DDRSC_ID_VALUE; return true;
    case FX1_DDRSC_STATUS:
        v = (ddr_.mission_ready() ? FX1_DDRSC_ST_MISSION_READY : 0u) |
            (ddr_.init_complete() ? FX1_DDRSC_ST_INIT_COMPLETE : 0u) |
            (ddr_.global_idle() ? FX1_DDRSC_ST_GLOBAL_IDLE : 0u) | (ddr_.fe_drained() ? FX1_DDRSC_ST_FE_DRAINED : 0u) |
            (ddr_.irq_level() ? FX1_DDRSC_ST_IRQ : 0u) | (preinit_ ? FX1_DDRSC_ST_PREINIT : 0u);
        return true;
    case FX1_DDRSC_PWR_REQ: v = std::uint32_t{pending_state_} & 3u; return true;
    case FX1_DDRSC_PWR_STATUS:
        v = (busy_ || start_pending_ ? FX1_DDRSC_PWR_BUSY : 0u) | (accepted_ ? FX1_DDRSC_PWR_ACCEPTED : 0u) |
            (denied_ ? FX1_DDRSC_PWR_DENIED : 0u) | (wake_denied_ ? FX1_DDRSC_PWR_AUTO_WAKE_DENIED : 0u) |
            (std::uint32_t{last_state_} << 4) |
            ((ddr_.power_resident() & 3u) << 8) | (ddr_.pchannel().pactive ? FX1_DDRSC_PWR_PACTIVE : 0u);
        return true;
    case FX1_DDRSC_PWR_CTRL: v = auto_wake_ ? FX1_DDRSC_PWR_AUTO_WAKE : 0u; return true;
    case FX1_DDRSC_PWR_COUNT: v = (entries_ & 0xFFFFu) | (auto_wakes_ & 0xFFFFu) << 16; return true;
    case FX1_DDRSC_RST_CTRL: return true;  // command register, reads 0
    case FX1_DDRSC_RST_STATUS:
        v = (rst_busy_ || rst_pending_ ? FX1_DDRSC_RST_BUSY : 0u) | (rst_done_ ? FX1_DDRSC_RST_DONE : 0u) |
            (rst_safe_timeout_ ? FX1_DDRSC_RST_SAFE_TIMEOUT : 0u) | (cold_resets_ & 0xFFu) << 8 |
            (core_resets_ & 0xFFu) << 16;
        return true;
    case FX1_DDRSC_QOS_CFG:
        for (unsigned p = 0; p < 4; ++p) v |= ddr_.qos_configured(p) ? 1u << p : 0u;
        return true;
    default: break;
    }
    for (unsigned p = 0; p < 4; ++p) {
        if (off == FX1_DDRSC_QOS_LUT(p)) {
            for (unsigned i = 0; i < 16; ++i) v |= std::uint32_t{ddr_.qos_class(p, i)} << (2 * i);
            return true;
        }
        if (off == FX1_DDRSC_RSV_RATE(p)) {
            v = rsv_rate_[p];
            return true;
        }
        if (off == FX1_DDRSC_RSV_DEPTH(p)) {
            v = rsv_depth_[p];
            return true;
        }
    }
    return false;
}

tlm::tlm_response_status DdrSysctrl::write_word(std::uint32_t off, std::uint32_t v)
{
    switch (off) {
    case FX1_DDRSC_PWR_REQ:
        if (v & ~0x103u) return tlm::TLM_GENERIC_ERROR_RESPONSE;
        if ((v & (1u << 8)) && !busy_ && !start_pending_) {
            pending_state_ = static_cast<std::uint8_t>(v & 3u);
            start_pending_ = true;
            start_.notify(sc_core::SC_ZERO_TIME);
        }
        return tlm::TLM_OK_RESPONSE;
    case FX1_DDRSC_RST_CTRL:
        if ((v != FX1_DDRSC_RST_COLD && v != FX1_DDRSC_RST_CORE) || rst_busy_ || rst_pending_)
            return tlm::TLM_GENERIC_ERROR_RESPONSE;
        rst_pending_ = v;
        start_.notify(sc_core::SC_ZERO_TIME);
        return tlm::TLM_OK_RESPONSE;
    case FX1_DDRSC_PWR_CTRL:
        if (v & ~1u) return tlm::TLM_GENERIC_ERROR_RESPONSE;
        auto_wake_ = (v & FX1_DDRSC_PWR_AUTO_WAKE) != 0;
        start_.notify(sc_core::SC_ZERO_TIME);
        return tlm::TLM_OK_RESPONSE;
    case FX1_DDRSC_QOS_CFG:
        if (v & ~0xFu) return tlm::TLM_GENERIC_ERROR_RESPONSE;
        for (unsigned p = 0; p < 4; ++p) ddr_.qos_set_configured(p, (v >> p) & 1u);
        return tlm::TLM_OK_RESPONSE;
    default: break;
    }
    for (unsigned p = 0; p < 4; ++p) {
        if (off == FX1_DDRSC_QOS_LUT(p)) {
            for (unsigned i = 0; i < 16; ++i)
                ddr_.qos_write(p, i, static_cast<fx1::ddr3::QosClass>((v >> (2 * i)) & 3u));
            return tlm::TLM_OK_RESPONSE;
        }
        if (off == FX1_DDRSC_RSV_RATE(p) || off == FX1_DDRSC_RSV_DEPTH(p)) {
            if (v >= (1u << 20)) return tlm::TLM_GENERIC_ERROR_RESPONSE;
            (off == FX1_DDRSC_RSV_RATE(p) ? rsv_rate_[p] : rsv_depth_[p]) = v;
            ddr_.set_reserved_rate(p, rsv_rate_[p], rsv_depth_[p]);
            return tlm::TLM_OK_RESPONSE;
        }
    }
    std::uint32_t dummy = 0;
    return read_word(off, dummy) ? tlm::TLM_GENERIC_ERROR_RESPONSE   // read-only word
                                 : tlm::TLM_ADDRESS_ERROR_RESPONSE;  // not decoded
}

void DdrSysctrl::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
{
    sc_core::wait(delay);
    delay = sc_core::SC_ZERO_TIME;
    const auto off = static_cast<std::uint32_t>(tx.get_address());
    if (tx.get_data_length() != 4 || off % 4 || !tx.get_data_ptr() || tx.get_byte_enable_ptr()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    sc_core::wait(kAccessLatency());
    if (tx.is_read()) {
        std::uint32_t v = 0;
        const bool ok = read_word(off, v);
        store_le32(tx.get_data_ptr(), v);
        tx.set_response_status(ok ? tlm::TLM_OK_RESPONSE : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    } else if (tx.is_write()) {
        tx.set_response_status(write_word(off, load_le32(tx.get_data_ptr())));
    } else {
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
}

unsigned DdrSysctrl::transport_dbg(tlm::tlm_generic_payload& tx)
{
    // Debug reads only, no side effects.
    const auto off = static_cast<std::uint32_t>(tx.get_address());
    std::uint32_t v = 0;
    if (!tx.is_read() || tx.get_data_length() != 4 || off % 4 || !read_word(off, v)) return 0;
    store_le32(tx.get_data_ptr(), v);
    return 4;
}

void DdrSysctrl::handshake(std::uint8_t pstate)
{
    // AMBA IHI0068 4-phase: PREQ with PSTATE, wait for PACCEPT or PDENY, drop PREQ, wait for
    // both responses to fall.
    busy_ = true;
    last_state_ = pstate;
    ddr_.pchannel_request(true, pstate);
    while (!ddr_.pchannel().paccept && !ddr_.pchannel().pdeny) sc_core::wait(ddr_.pchannel_event());
    accepted_ = ddr_.pchannel().paccept;
    denied_ = !accepted_;
    if (accepted_ && pstate != FX1_DDR3_PSTATE_ACTIVE) ++entries_;
    if (accepted_) clear_wake_backoff();
    ddr_.pchannel_request(false, pstate);
    while (ddr_.pchannel().paccept || ddr_.pchannel().pdeny) sc_core::wait(ddr_.pchannel_event());
    busy_ = false;
}

void DdrSysctrl::clear_wake_backoff()
{
    wake_denied_ = false;
    wake_backoff_ = wake_retry_at_ = sc_core::SC_ZERO_TIME;
}

void DdrSysctrl::reset_sequence(std::uint32_t op)
{
    rst_busy_ = true;
    rst_done_ = false;
    rst_safe_timeout_ = false;
    if (op == FX1_DDRSC_RST_COLD) {
        ddr_.set_cold_reset(true);
        sc_core::wait(kResetPulse());
        ddr_.set_cold_reset(false);
        ++cold_resets_;
        rst_done_ = true;
    } else {
        // Coordinated core reset (HAS §7.1 Table 7-4): request, reset-safe, pulse.
        ddr_.core_reset_request(true);
        const sc_core::sc_time end = sc_core::sc_time_stamp() + kResetSafeLimit();
        while (!ddr_.core_reset_safe() && sc_core::sc_time_stamp() < end)
            sc_core::wait(end - sc_core::sc_time_stamp(), ddr_.lifecycle_event());
        if (ddr_.core_reset_safe()) {
            ddr_.set_core_reset(true);
            sc_core::wait(kResetPulse());
            ddr_.set_core_reset(false);
            ++core_resets_;
            rst_done_ = true;
        } else {
            rst_safe_timeout_ = true;
        }
        ddr_.core_reset_request(false);
    }
    clear_wake_backoff();  // the controller's power state was reset
    rst_busy_ = false;
}

void DdrSysctrl::sequencer()
{
    for (;;) {
        if (rst_pending_) {
            const std::uint32_t op = rst_pending_;
            rst_pending_ = 0;
            reset_sequence(op);
            continue;
        }
        if (start_pending_) {
            start_pending_ = false;
            handshake(pending_state_);
            continue;
        }
        // The controller raises PACTIVE when it needs the active state (pending traffic or
        // refresh urgency in a low-power state): wake it.
        const bool wake_needed = auto_wake_ && ddr_.power_resident() != FX1_DDR3_PSTATE_ACTIVE &&
                                 ddr_.pchannel().pactive && !ddr_.pchannel().preq;
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        if (wake_needed && now >= wake_retry_at_) {
            handshake(FX1_DDR3_PSTATE_ACTIVE);
            if (accepted_) {
                ++auto_wakes_;
            } else {
                // Denied (recovery required, initialization not done, mutex owner...). The
                // condition may hold for ever and PACTIVE stays high: retry only after
                // simulated time has passed, so the kernel advances and a reset or a
                // firmware request is served meanwhile (review S5-R1).
                wake_denied_ = true;
                ++wake_denials_;
                wake_backoff_ = wake_backoff_ == sc_core::SC_ZERO_TIME ? kWakeRetryMin()
                              : std::min(wake_backoff_ * 2.0, kWakeRetryMax());
                wake_retry_at_ = sc_core::sc_time_stamp() + wake_backoff_;
            }
            continue;
        }
        if (wake_needed)
            sc_core::wait(wake_retry_at_ - now, start_);  // back-off; RST_CTRL/PWR_* end it
        else
            sc_core::wait(start_ | ddr_.pchannel_event() | ddr_.lifecycle_event());
    }
}

} // namespace cdc::platforms::fx1_soc
