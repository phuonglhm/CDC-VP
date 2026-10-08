#include "fx1/plic.h"
#include <stdexcept>

namespace fx1 {
namespace {
constexpr std::uint64_t kPriority = 0x000000, kPending = 0x001000, kEnable = 0x002000,
                        kEnableStride = 0x80, kContext = 0x200000, kContextStride = 0x1000;

std::uint32_t load32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[3]) << 24);
}
void store32(unsigned char* p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}
} // namespace

Plic::Plic(sc_core::sc_module_name name, unsigned sources, unsigned contexts,
           unsigned max_priority, sc_core::sc_time access_latency)
    : sc_module(name), sources_(sources), contexts_(contexts), max_priority_(max_priority),
      latency_(access_latency), priority_(sources, 0), pending_(sources, false),
      in_flight_(sources, false), enable_(contexts, std::vector<bool>(sources, false)),
      threshold_(contexts, 0) {
    if (sources < 2 || sources > 1024) throw std::invalid_argument("Plic: sources must be 2..1024");
    if (!contexts || contexts > 15872) throw std::invalid_argument("Plic: contexts must be 1..15872");
    if (!max_priority) throw std::invalid_argument("Plic: max_priority must be nonzero");
    irq_in.init(sources - 1);
    eip.init(contexts);
    socket.register_b_transport(this, &Plic::b_transport);
    socket.register_transport_dbg(this, &Plic::transport_dbg);
    // One method: gateway then outputs. Not dont_initialize(): drives eip at reset.
    SC_METHOD(sample);
    for (auto& line : irq_in) sensitive << line;
    sensitive << changed_;
}

unsigned Plic::best(unsigned context) const {
    // Claim candidate: pending, enabled, priority > 0; highest priority, lowest
    // ID on ties. Independent of the threshold, which only gates notification.
    unsigned id = 0;
    std::uint32_t level = 0;
    for (unsigned s = 1; s < sources_; ++s) {
        if (pending_[s] && enable_[context][s] && priority_[s] > level) {
            id = s;
            level = priority_[s];
        }
    }
    return id;
}

bool Plic::notify(unsigned context) const {
    const auto id = best(context);
    return id && priority_[id] > threshold_[context];
}

void Plic::sample() {
    for (unsigned s = 1; s < sources_; ++s)
        if (irq_in[s - 1].read() && !in_flight_[s]) pending_[s] = true;
    drive();
}

void Plic::drive() {
    for (unsigned c = 0; c < contexts_; ++c) eip[c].write(notify(c));
}

std::uint32_t Plic::claim(unsigned context) {
    const auto id = best(context);
    if (id) {
        pending_[id] = false;
        in_flight_[id] = true;
        ++claims_;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
    return id;
}

void Plic::complete(unsigned context, std::uint32_t source) {
    // Ignored unless the source is valid, enabled for this context and in flight.
    if (source == 0 || source >= sources_ || !enable_[context][source] || !in_flight_[source])
        return;
    in_flight_[source] = false;
    changed_.notify(sc_core::SC_ZERO_TIME);  // a line still high re-pends at once
}

bool Plic::read(std::uint64_t offset, std::uint32_t& value) const {
    const unsigned words = (sources_ + 31) / 32;
    if (offset < kPriority + 4ull * sources_) {
        const auto s = static_cast<unsigned>(offset / 4);
        value = s ? priority_[s] : 0;
        return true;
    }
    if (offset >= kPending && offset < kPending + 4ull * words) {
        const auto first = static_cast<unsigned>((offset - kPending) / 4) * 32;
        value = 0;
        for (unsigned b = 0; b < 32 && first + b < sources_; ++b)
            if (pending_[first + b]) value |= 1u << b;
        return true;
    }
    if (offset >= kEnable && offset < kEnable + kEnableStride * contexts_) {
        const auto c = static_cast<unsigned>((offset - kEnable) / kEnableStride);
        const auto word = (offset - kEnable) % kEnableStride / 4;
        if (word >= words) return false;
        const auto first = static_cast<unsigned>(word) * 32;
        value = 0;
        for (unsigned b = 0; b < 32 && first + b < sources_; ++b)
            if (enable_[c][first + b]) value |= 1u << b;
        return true;
    }
    if (offset >= kContext && offset < kContext + kContextStride * contexts_) {
        const auto c = static_cast<unsigned>((offset - kContext) / kContextStride);
        const auto reg = (offset - kContext) % kContextStride;
        if (reg == 0) { value = threshold_[c]; return true; }
        if (reg == 4) { value = best(c); return true; }  // claim, without its side effect
        return false;
    }
    return false;
}

bool Plic::write(std::uint64_t offset, std::uint32_t value) {
    const unsigned words = (sources_ + 31) / 32;
    if (offset < kPriority + 4ull * sources_) {
        const auto s = static_cast<unsigned>(offset / 4);
        if (s) priority_[s] = value > max_priority_ ? max_priority_ : value;
    } else if (offset >= kPending && offset < kPending + 4ull * words) {
        // read only: writes ignored
    } else if (offset >= kEnable && offset < kEnable + kEnableStride * contexts_) {
        const auto c = static_cast<unsigned>((offset - kEnable) / kEnableStride);
        const auto word = (offset - kEnable) % kEnableStride / 4;
        if (word >= words) return false;
        const auto first = static_cast<unsigned>(word) * 32;
        for (unsigned b = 0; b < 32 && first + b < sources_; ++b)
            enable_[c][first + b] = first + b != 0 && ((value >> b) & 1);
    } else if (offset >= kContext && offset < kContext + kContextStride * contexts_) {
        const auto c = static_cast<unsigned>((offset - kContext) / kContextStride);
        const auto reg = (offset - kContext) % kContextStride;
        if (reg == 0) threshold_[c] = value > max_priority_ ? max_priority_ : value;
        else if (reg == 4) complete(c, value);
        else return false;
    } else {
        return false;
    }
    changed_.notify(sc_core::SC_ZERO_TIME);
    return true;
}

void Plic::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    const auto offset = tx.get_address();
    if ((!tx.is_read() && !tx.is_write()) || !tx.get_data_ptr() || tx.get_data_length() != 4 ||
        tx.get_streaming_width() < 4 || tx.get_byte_enable_ptr() || offset % 4) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    bool ok;
    if (tx.is_write()) {
        ok = write(offset, load32(tx.get_data_ptr()));
    } else {
        std::uint32_t value = 0;
        const bool claim_reg = offset >= kContext && offset < kContext + kContextStride * contexts_ &&
                               (offset - kContext) % kContextStride == 4;
        if (claim_reg) {
            value = claim(static_cast<unsigned>((offset - kContext) / kContextStride));
            ok = true;
        } else {
            ok = read(offset, value);
        }
        if (ok) store32(tx.get_data_ptr(), value);
    }
    if (!ok) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    delay += latency_;
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned Plic::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Reads only, without side effects (a debug read of claim does not claim).
    std::uint32_t value = 0;
    if (!tx.is_read() || !tx.get_data_ptr() || tx.get_data_length() != 4 || tx.get_address() % 4 ||
        !read(tx.get_address(), value))
        return 0;
    store32(tx.get_data_ptr(), value);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}
} // namespace fx1
