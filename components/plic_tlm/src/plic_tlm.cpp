#include "plic_tlm.h"

#include <cstring>

namespace cdc::components {

namespace {
constexpr std::uint64_t kPriorityBase = 0x000000;  // + 4*id
constexpr std::uint64_t kPending = 0x001000;
constexpr std::uint64_t kEnable = 0x002000;        // context 0
constexpr std::uint64_t kThreshold = 0x200000;     // context 0
constexpr std::uint64_t kClaim = 0x200004;         // context 0 (read=claim, write=complete)

constexpr unsigned kCauseExternal = 11;
} // namespace

plic_tlm::plic_tlm(sc_core::sc_module_name name, cdc::cpu::cpu_base& cpu, unsigned num_sources)
    : sc_core::sc_module(name)
    , socket("socket")
    , irq_in("irq_in", num_sources)
    , cpu_(cpu)
    , n_(num_sources)
    , priority_(num_sources + 1, 0)
    , claimed_(num_sources + 1, false)
{
    socket.register_b_transport(this, &plic_tlm::b_transport);

    SC_HAS_PROCESS(plic_tlm);
    SC_METHOD(on_sources);
    for (unsigned i = 0; i < n_; ++i) {
        sensitive << irq_in[i];
    }
    dont_initialize();
}

bool plic_tlm::eligible(unsigned id) const
{
    if (id < 1 || id > n_) {
        return false;
    }
    const bool line = irq_in[id - 1].read();
    const bool enabled = (enable_ >> id) & 0x1U;
    return line && enabled && !claimed_[id] && priority_[id] > threshold_;
}

void plic_tlm::update_meip()
{
    bool any = false;
    for (unsigned id = 1; id <= n_; ++id) {
        if (eligible(id)) {
            any = true;
            break;
        }
    }
    cpu_.set_irq(kCauseExternal, any);
}

void plic_tlm::on_sources()
{
    update_meip();
}

std::uint32_t plic_tlm::do_claim()
{
    std::uint32_t best = 0;
    std::uint32_t best_prio = 0;
    for (unsigned id = 1; id <= n_; ++id) {
        if (eligible(id) && priority_[id] > best_prio) {
            best = id;
            best_prio = priority_[id];
        }
    }
    if (best != 0) {
        claimed_[best] = true;  // gateway: do not re-raise until complete
    }
    update_meip();
    return best;
}

void plic_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += sc_core::sc_time(10, sc_core::SC_NS);

    const std::uint64_t addr = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();
    const bool is_read = trans.get_command() == tlm::TLM_READ_COMMAND;

    if (ptr == nullptr || len != 4) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    std::uint32_t value = 0;
    if (!is_read) {
        std::memcpy(&value, ptr, 4);
    }

    if (addr >= kPriorityBase && addr < kPriorityBase + 4 * (n_ + 1)) {
        const unsigned id = static_cast<unsigned>(addr / 4);
        if (is_read) {
            value = (id <= n_) ? priority_[id] : 0;
        } else if (id >= 1 && id <= n_) {
            priority_[id] = value;
            update_meip();
        }
    } else if (addr == kPending) {
        if (is_read) {
            value = 0;
            for (unsigned id = 1; id <= n_; ++id) {
                if (irq_in[id - 1].read()) {
                    value |= (1U << id);
                }
            }
        }
    } else if (addr == kEnable) {
        if (is_read) {
            value = enable_;
        } else {
            enable_ = value;
            update_meip();
        }
    } else if (addr == kThreshold) {
        if (is_read) {
            value = threshold_;
        } else {
            threshold_ = value;
            update_meip();
        }
    } else if (addr == kClaim) {
        if (is_read) {
            value = do_claim();
        } else {
            // complete: re-open the gateway for the given source id
            if (value >= 1 && value <= n_) {
                claimed_[value] = false;
            }
            update_meip();
        }
    } else {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (is_read) {
        std::memcpy(ptr, &value, 4);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

} // namespace cdc::components
