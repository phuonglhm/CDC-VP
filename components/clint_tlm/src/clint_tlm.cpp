#include "clint_tlm.h"

#include <cstring>

namespace cdc::components {

namespace {
constexpr std::uint64_t kRegMsip = 0x0000;
constexpr std::uint64_t kRegMtimecmp = 0x4000;
constexpr std::uint64_t kRegMtime = 0xBFF8;

// sc_time value() is in time-resolution units (default 1ps). Scale to microseconds
// so mtime/mtimecmp use a sane tick rate (matches the Bremen CLINT convention).
constexpr std::uint64_t kScaler = 1000000;

constexpr unsigned kCauseSoftware = 3;
constexpr unsigned kCauseTimer = 7;

// Read/write a slice [sub, sub+len) of a 64-bit register value.
void slice_read(std::uint64_t value, unsigned sub, unsigned len, unsigned char* dst)
{
    unsigned char bytes[8];
    std::memcpy(bytes, &value, 8);
    std::memcpy(dst, bytes + sub, len);
}

void slice_write(std::uint64_t& value, unsigned sub, unsigned len, const unsigned char* src)
{
    unsigned char bytes[8];
    std::memcpy(bytes, &value, 8);
    std::memcpy(bytes + sub, src, len);
    std::memcpy(&value, bytes, 8);
}
} // namespace

clint_tlm::clint_tlm(sc_core::sc_module_name name, cdc::cpu::cpu_base& cpu)
    : sc_core::sc_module(name)
    , socket("socket")
    , cpu_(cpu)
{
    socket.register_b_transport(this, &clint_tlm::b_transport);
    SC_HAS_PROCESS(clint_tlm);
    SC_THREAD(timer_proc);
}

std::uint64_t clint_tlm::now_mtime() const
{
    return sc_core::sc_time_stamp().value() / kScaler;
}

void clint_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += sc_core::sc_time(10, sc_core::SC_NS);

    const std::uint64_t addr = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();
    const bool is_read = trans.get_command() == tlm::TLM_READ_COMMAND;

    if (ptr == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (addr == kRegMsip && len == 4) {
        if (is_read) {
            std::memcpy(ptr, &msip_, 4);
        } else {
            std::uint32_t v = 0;
            std::memcpy(&v, ptr, 4);
            msip_ = v & 0x1U;
            cpu_.set_irq(kCauseSoftware, msip_ != 0);
        }
    } else if (addr >= kRegMtimecmp && addr + len <= kRegMtimecmp + 8) {
        const unsigned sub = static_cast<unsigned>(addr - kRegMtimecmp);
        if (is_read) {
            slice_read(mtimecmp_, sub, len, ptr);
        } else {
            slice_write(mtimecmp_, sub, len, ptr);
            cmp_event_.notify(sc_core::SC_ZERO_TIME);  // re-evaluate timer
        }
    } else if (addr >= kRegMtime && addr + len <= kRegMtime + 8) {
        const unsigned sub = static_cast<unsigned>(addr - kRegMtime);
        const std::uint64_t t = now_mtime();
        if (is_read) {
            slice_read(t, sub, len, ptr);
        }
        // mtime is read-only; writes are ignored.
    } else {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void clint_tlm::reschedule()
{
    if (mtimecmp_ == 0) {
        return;
    }
    const sc_core::sc_time goal = sc_core::sc_time::from_value(mtimecmp_ * kScaler);
    const sc_core::sc_time now = sc_core::sc_time_stamp();
    cmp_event_.notify(goal > now ? (goal - now) : sc_core::SC_ZERO_TIME);
}

void clint_tlm::timer_proc()
{
    while (true) {
        wait(cmp_event_);
        if (mtimecmp_ != 0 && now_mtime() >= mtimecmp_) {
            cpu_.set_irq(kCauseTimer, true);   // assert MTIP
        } else {
            cpu_.set_irq(kCauseTimer, false);  // clear MTIP, wait for compare time
            reschedule();
        }
    }
}

} // namespace cdc::components
