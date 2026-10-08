#include "fx1/clint.h"
#include <limits>
#include <stdexcept>

namespace fx1 {
namespace {
constexpr std::uint64_t kMsip = 0x0000, kMtimecmp = 0x4000, kMtime = 0xBFF8;

std::uint64_t load(const unsigned char* p, unsigned n) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < n; ++i) value |= std::uint64_t(p[i]) << (8 * i);
    return value;
}
void store(unsigned char* p, std::uint64_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(value >> (8 * i));
}
} // namespace

Clint::Clint(sc_core::sc_module_name name, unsigned harts, std::uint64_t mtime_hz,
             sc_core::sc_time access_latency)
    : sc_module(name), harts_(harts), latency_(access_latency),
      msip_(harts, false), mtimecmp_(harts, ~std::uint64_t{0}) {
    if (!harts || harts > 4095) throw std::invalid_argument("Clint: hart count must be 1..4095");
    if (!mtime_hz) throw std::invalid_argument("Clint: mtime frequency must be nonzero");
    const auto second = sc_core::sc_time(1, sc_core::SC_SEC).value();
    if (second % mtime_hz)
        throw std::invalid_argument("Clint: the mtime period must be a whole number of "
                                    "SystemC time-resolution units");
    tick_ = sc_core::sc_time::from_value(second / mtime_hz);
    msip_irq.init(harts);
    mtip_irq.init(harts);
    socket.register_b_transport(this, &Clint::b_transport);
    socket.register_transport_dbg(this, &Clint::transport_dbg);
    // Not dont_initialize(): the first run drives every output to its reset level.
    SC_METHOD(update);
    sensitive << update_event_;
}

std::uint64_t Clint::mtime() const {
    return sc_core::sc_time_stamp().value() / tick_.value() + offset_;
}

bool Clint::access64(std::uint64_t& reg, unsigned part, bool write, unsigned char* data,
                     unsigned length) {
    if (length == 8 && part == 0) {
        if (write) reg = load(data, 8);
        else store(data, reg, 8);
        return true;
    }
    if (length == 4 && (part == 0 || part == 4)) {
        const unsigned shift = part * 8;
        if (write) {
            reg = (reg & ~(std::uint64_t{0xFFFFFFFF} << shift)) | (load(data, 4) << shift);
        } else {
            store(data, reg >> shift, 4);
        }
        return true;
    }
    return false;
}

bool Clint::access(std::uint64_t offset, bool write, unsigned char* data, unsigned length) {
    if (offset < kMsip + 4ull * harts_) {
        if (offset % 4 || length != 4) return false;
        const auto hart = static_cast<unsigned>(offset / 4);
        if (write) msip_[hart] = (data[0] & 1) != 0;  // bits 31:1 are WARL zero
        else store(data, msip_[hart] ? 1 : 0, 4);
        return true;
    }
    if (offset >= kMtimecmp && offset < kMtimecmp + 8ull * harts_) {
        const auto relative = offset - kMtimecmp;
        return access64(mtimecmp_[relative / 8], static_cast<unsigned>(relative % 8), write, data,
                        length);
    }
    if (offset >= kMtime && offset < kMtime + 8) {
        auto value = mtime();
        if (!access64(value, static_cast<unsigned>(offset - kMtime), write, data, length))
            return false;
        if (write) offset_ = value - sc_core::sc_time_stamp().value() / tick_.value();
        return true;
    }
    return false;
}

void Clint::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    const auto length = tx.get_data_length();
    if (!tx.get_data_ptr() || !length || tx.get_streaming_width() < length ||
        tx.get_byte_enable_ptr() ||
        !access(tx.get_address(), tx.is_write(), tx.get_data_ptr(), length)) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (tx.is_write()) update_event_.notify(sc_core::SC_ZERO_TIME);
    delay += latency_;
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned Clint::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Side-effect free: reads only.
    tx.set_dmi_allowed(false);
    const auto length = tx.get_data_length();
    if (!tx.is_read() || !tx.get_data_ptr() ||
        !access(tx.get_address(), false, tx.get_data_ptr(), length))
        return 0;
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return length;
}

void Clint::update() {
    const auto now = mtime();
    const auto tick = tick_.value();
    // Keep the scheduled delay far from sc_time overflow; a compare value this
    // far ahead (hours of simulated time) is re-evaluated on its next write.
    const auto horizon = (std::numeric_limits<std::uint64_t>::max() / tick) / 2;
    bool have_deadline = false;
    sc_core::sc_time next;
    for (unsigned hart = 0; hart < harts_; ++hart) {
        msip_irq[hart].write(msip_[hart]);
        const bool pending = now >= mtimecmp_[hart];
        mtip_irq[hart].write(pending);
        if (pending) continue;
        const auto ahead = mtimecmp_[hart] - now;
        if (ahead > horizon) continue;
        const auto phase = sc_core::sc_time_stamp().value() % tick;
        const auto candidate = sc_core::sc_time::from_value(ahead * tick - phase);
        if (!have_deadline || candidate < next) next = candidate;
        have_deadline = true;
    }
    if (have_deadline) update_event_.notify(next);
}
} // namespace fx1
