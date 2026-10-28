#include "fu3/dma/ap_dma.h"
#include "fu3/dma/transfer_extension.h"
#include <algorithm>
#if SYSTEMC_VERSION != 20221128
#error "FU3 AP_DMA requires SystemC 2.3.4"
#endif
namespace fu3::dma {
namespace {
constexpr std::uint32_t config_mask = 0xcf3f007f;
constexpr std::uint32_t interrupt_mask = 0x1fff;
}
ApDma::ApDma(sc_core::sc_module_name name, sc_core::sc_time peripheral_cycle,
             const EngineConfig& config)
    : sc_module(name), config_(config), peripheral_cycle_(peripheral_cycle) {
    if (peripheral_cycle_ < sc_core::SC_ZERO_TIME)
        SC_REPORT_FATAL("AP_DMA", "peripheral cycle must not be negative");
    if (config_.transaction_timeout < sc_core::SC_ZERO_TIME ||
        config_.watchdog_timeout < sc_core::SC_ZERO_TIME ||
        config_.scheduler_quantum < sc_core::SC_ZERO_TIME)
        SC_REPORT_FATAL("AP_DMA", "engine timing parameters must not be negative");
    for (const auto& threshold : config_.phase_timeouts)
        if (threshold < sc_core::SC_ZERO_TIME) SC_REPORT_FATAL("AP_DMA", "phase timeout must not be negative");
    if ((config_.design_capability & 15) != 1 ||
        config_.memory_read_cache > 15 || config_.memory_write_cache > 15 ||
        config_.peripheral_read_cache > 15 || config_.peripheral_write_cache > 15 ||
        config_.memory_read_prot > 7 || config_.memory_write_prot > 7 ||
        config_.peripheral_read_prot > 7 || config_.peripheral_write_prot > 7)
        SC_REPORT_FATAL("AP_DMA", "invalid integration capabilities or AXI attributes");
    SC_THREAD(read0); SC_THREAD(read1); SC_THREAD(read2); SC_THREAD(read3);
    SC_THREAD(write0); SC_THREAD(write1); SC_THREAD(write2); SC_THREAD(write3);
    target_socket.register_b_transport(this, &ApDma::b_transport);
    SC_THREAD(engine);
    SC_METHOD(reset_registers);
    sensitive << reset_n.neg();
    SC_METHOD(update_outputs);
    sensitive << reset_n << state_changed_ << pulse_changed_;
    SC_METHOD(requests_changed);
    sensitive << rx_request << tx_request;
}
void ApDma::reset_registers() {
    // Initialization also sets known power-on state. Assertion is modeled
    // immediately; hardware synchronous/asynchronous choice remains unresolved.
    ++reset_epoch_;
    next_dispatch_=sc_core::sc_time_stamp();
    read_order_.clear(); write_order_.clear();
    read_arbiter_ = Arbiter{}; write_arbiter_ = Arbiter{};
    rx_owned_ = tx_owned_ = 0;
    // Busy worker buffers must survive external blocking calls across reset.
    for (auto& ch : channels_) {
        ch = Channel{};
        ch.words[0x10 / 4] = ch.words[0x14 / 4] = 0x84010000;
        ch.words[0x40 / 4] = 1;
        ch.words[0xa8 / 4] = interrupt_mask;
    }
    priority_ = software_rx_ = software_tx_ = 0;
    rx_blocked_ = tx_blocked_ = 0;
    rx_until_.fill(sc_core::SC_ZERO_TIME);
    tx_until_.fill(sc_core::SC_ZERO_TIME);
    rx_ready_.fill(sc_core::SC_ZERO_TIME);
    tx_ready_.fill(sc_core::SC_ZERO_TIME);
    pulse_changed_.cancel();
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}
std::uint32_t ApDma::core_status() const {
    std::uint32_t result = 0;
    for (unsigned n = 0; n < channels_.size(); ++n)
        if (channels_[n].words[0xa0 / 4] & channels_[n].words[0xa8 / 4] & interrupt_mask)
            result |= 1u << n;
    return result;
}
void ApDma::update_outputs() {
    irq.write(reset_n.read() && core_status() != 0);
    std::uint32_t rx = 0, tx = 0;
    auto next = sc_core::sc_max_time();
    const auto now = sc_core::sc_time_stamp();
    for (unsigned id = 1; id < 32; ++id) {
        if (rx_until_[id] > now) { rx |= 1u << id; next = std::min(next, rx_until_[id]); }
        if (tx_until_[id] > now) { tx |= 1u << id; next = std::min(next, tx_until_[id]); }
    }
    rx_clear.write(reset_n.read() ? rx : 0);
    tx_clear.write(reset_n.read() ? tx : 0);
    pulse_changed_.cancel();
    if (next != sc_core::sc_max_time()) pulse_changed_.notify(next - now);
}
void ApDma::requests_changed() {
    // A serviced external request must drop before it can provide another credit.
    rx_blocked_ &= rx_request.read().to_uint();
    tx_blocked_ &= tx_request.read().to_uint();
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}
bool ApDma::request_ready(unsigned id, bool read) const {
    if (!id) return true;
    const auto external = (read ? rx_request.read() : tx_request.read()).to_uint();
    const auto blocked = read ? rx_blocked_ : tx_blocked_;
    const auto software = read ? software_rx_ : software_tx_;
    const auto until = read ? rx_until_[id] : tx_until_[id];
    const auto ready = read ? rx_ready_[id] : tx_ready_[id];
    return !((read ? rx_owned_ : tx_owned_) & (1u << id)) &&
           std::max(until, ready) <= sc_core::sc_time_stamp() &&
           (((external & ~blocked) | software) & (1u << id));
}
void ApDma::acknowledge(unsigned n, unsigned id, bool read) {
    const auto bit = 1u << id;
    auto& software = read ? software_rx_ : software_tx_;
    auto& blocked = read ? rx_blocked_ : tx_blocked_;
    const auto external = (read ? rx_request.read() : tx_request.read()).to_uint();
    software &= ~bit;
    blocked |= external & bit;
    auto& until = read ? rx_until_[id] : tx_until_[id];
    until = sc_core::sc_time_stamp() + peripheral_cycle_;
    const auto pc = channels_[n].words[0x20/4];
    const auto delay = (pc >> (read ? 8 : 24)) & 7;
    // Recovery belongs to the endpoint, including when the next service uses
    // another channel or a new command. Clear pulse width remains one cycle.
    (read ? rx_ready_[id] : tx_ready_[id]) =
        sc_core::sc_time_stamp() + peripheral_cycle_ * double(delay);
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}
tlm::tlm_response_status ApDma::access(bool write, std::uint32_t address,
                                      std::uint32_t& value) {
    if (address < 0x800) {
        const auto channel = address / 0x100;
        auto& ch = channels_[channel];
        auto& words = ch.words;
        // Keep the live configuration stable while an engine operation can wait.
        if (write && ch.active && (address % 0x100) <= 0x24)
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        const auto off = address % 0x100;
        std::uint32_t mask = 0;
        bool writable = true;
        switch (off) {
        case 0x00: case 0x04: case 0x0c: mask = 0xffffffff; break;
        case 0x08: mask = 0x3ff; break;
        case 0x10: case 0x14: mask = config_mask; break;
        case 0x18: mask = 0x30000000; break;
        case 0x1c: mask = 0; break; // Compatibility register.
        case 0x20: mask = 0x071f071f; break;
        case 0x24: mask = 0x0000bfbf; break;
        case 0x40:
            if (write && (value & 1) && !words[0x40/4]) ch.progress = sc_core::sc_time_stamp();
            mask = 1; break;
        case 0xa8: mask = interrupt_mask; break;
        case 0x30: case 0x34: case 0x3c: case 0x48: case 0x50:
            writable = false; break;
        case 0xac:
            if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            value = words[0xa0 / 4] & words[0xa8 / 4] & interrupt_mask;
            return tlm::TLM_OK_RESPONSE;
        case 0xa0:
            // Explicit W1S debug policy: does not fabricate completed-command counts.
            if (write) words[off/4] |= value & interrupt_mask;
            else value = words[off/4];
            return tlm::TLM_OK_RESPONSE;
        case 0xa4:
            if (!write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            if ((value & 1) && ((words[0x50 / 4] >> 16) & 15))
                words[0x50 / 4] -= 1u << 16;
            words[0xa0 / 4] &= ~(value & interrupt_mask);
            if (words[0x50 / 4] & 0xf0000) words[0xa0 / 4] |= 1;
            return tlm::TLM_OK_RESPONSE;
        case 0x38:
            if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            value = (32 - ch.used - ch.swap_count - ch.read_reserved - ch.write_reserved) | (ch.used << 16);
            return tlm::TLM_OK_RESPONSE;
        case 0x44:
            if (!write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            if (value & 1) {
                if (!can_start(channel)) return tlm::TLM_COMMAND_ERROR_RESPONSE;
                start(channel);
            }
            return tlm::TLM_OK_RESPONSE;
        case 0x2c:
            if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            value = words[off/4]; return tlm::TLM_OK_RESPONSE;
        default: return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        if (write) {
            // Explicit model policy pending specification of RO write behavior.
            if (!writable) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            words[off / 4] = value & mask;
            if (off == 0 || off == 4 || off == 0x10 || off == 0x14 || off == 0x20) update_restriction(channel);
        } else value = words[off / 4];
        return tlm::TLM_OK_RESPONSE;
    }
    switch (address) {
    case 0x1000:
        if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        value = core_status(); break;
    case 0x1030: case 0x1040:
        if (!write) value = 0;
        break;
    case 0x1038:
        // Register storage only. No fixed-priority arbitration is implemented.
        if (write) priority_ = value & 0xffff;
        else value = priority_;
        break;
    case 0x1050: case 0x1054: {
        auto& requests = address == 0x1050 ? software_rx_ : software_tx_;
        if (write) requests = value & 0xfffffffe;
        else value = requests;
        break;
    }
    case 0x10d0:
        if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        value = 1;
        for (const auto& ch : channels_) if (ch.active) value = 0;
        for (const auto& job : reads_) if (job.busy) value = 0;
        for (const auto& job : writes_) if (job.busy) value = 0;
        break;
    case 0x10f0:
        if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        value = 0x0a602258; break;
    case 0x1048:
        if (!write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        for (unsigned n = 0; n < 8; ++n)
            if ((value & (1u << n)) && !can_start(n)) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        for (unsigned n = 0; n < 8; ++n) if (value & (1u << n)) start(n);
        break;
    case 0x10e0:
        if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        value = config_.design_capability; break;
    case 0x10f4:
        if (write) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        value = capabilities(); break;
    default: return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    }
    return tlm::TLM_OK_RESPONSE;
}
std::uint32_t ApDma::capabilities() const {
    std::uint32_t result = (1u<<2) | (1u<<4) | (1u<<8) | (1u<<9) | (1u<<11);
    if (config_.command_chain) result |= 1u<<10;
    // Capability describes implemented timer support; threshold=0 only disables it.
    result |= 3;
    return result;
}
void ApDma::update_restriction(unsigned n) {
    auto& ch = channels_[n]; const auto pc = ch.words[0x20/4];
    const bool source = (ch.words[0] & 31) == 0, destination = (ch.words[1] & 31) == 0;
    // Full-FIFO permission uses 32-byte alignment as an explicit functional policy.
    ch.words[0x2c/4] = unsigned(source) | (unsigned(destination)<<1) |
        (unsigned(source && destination)<<2) | (unsigned(pending_limit(n,true)>1)<<5) |
        (unsigned(pending_limit(n,false)>1)<<6) | (unsigned(!(pc & 0x001f001f))<<8);
}
bool ApDma::fifo_valid(unsigned n) {
    const auto& ch = channels_[n];
    if (std::uint64_t(ch.used)+ch.swap_count+ch.read_reserved+ch.write_reserved <= 32) return true;
    fail(n,1u<<5); return false;
}
std::uint32_t ApDma::phase_interrupt(BusPhase phase) {
    switch (phase) {
    case BusPhase::ReadAddress: return 1u<<8;
    case BusPhase::ReadData: return 1u<<7;
    case BusPhase::WriteAddress: return 1u<<11;
    case BusPhase::WriteData: return 1u<<10;
    case BusPhase::WriteResponse: return 1u<<9;
    }
    return 0;
}
// Unit selection is local to each endpoint, not a new register field.
unsigned ApDma::unit_bytes(std::uint32_t address, unsigned limit) {
    if (!(address & 3) && limit >= 4) return 4;
    if (!(address & 1) && limit >= 2) return 2;
    return 1;
}
unsigned ApDma::burst_bytes(std::uint32_t address, unsigned remaining,
                            unsigned capacity, std::uint32_t config) {
    unsigned limit = std::min({remaining, capacity, unsigned(config & 0x7f)});
    if (config & 0x80000000) limit = std::min(limit, 4096u - (address & 4095));
    const auto unit = unit_bytes(address, limit);
    limit = std::min(limit, 16 * unit);
    return limit - limit % unit;
}
bool ApDma::can_start(unsigned n) const {
    const auto& ch = channels_[n]; const auto& w = ch.words;
    // Explicit milestone limits, not assertions that these are illegal in RTL.
    const auto read_id = w[0x20/4] & 31;
    const auto write_id = (w[0x20/4] >> 16) & 31;
    const auto swap_mode = (w[0x18/4] >> 28) & 3;
    const auto group = 1u << swap_mode;
    // Partial final swap groups are unspecified: reject before any bus access.
    return swap_mode != 3 && w[2] % group == 0 &&
           !(read_id && write_id) &&
           (!(read_id || write_id) || peripheral_cycle_ > sc_core::SC_ZERO_TIME) &&
           !ch.active && !ch.read_pending && !ch.write_pending && w[0x40/4] && w[0x08/4] &&
           ((w[0x0c/4] & 2) || config_.command_chain) &&
           (w[0x10/4] & 0x7f) && (w[0x14/4] & 0x7f);
}
void ApDma::load_command(unsigned n) {
    auto& ch = channels_[n]; auto& w = ch.words;
    ch.source = w[0]; ch.destination = w[1];
    ch.read_remaining = ch.write_remaining = w[2];
    ch.used = ch.read_reserved = ch.write_reserved = ch.swap_count = 0;
    ch.need_command = ch.fetching = false;
    ch.progress = sc_core::sc_time_stamp();
    update_restriction(n);
    const auto mode = (w[0x18/4] >> 28) & 3;
    if (!w[2] || mode == 3 || w[2] % (1u << mode) ||
        !(w[0x10/4] & 127) || !(w[0x14/4] & 127) ||
        (!(w[3] & 2) && !config_.command_chain)) fail(n, 1u << 3);
    refresh_status(n);
}
void ApDma::start(unsigned n) {
    auto& ch = channels_[n];
    ch.fault = false; ch.active = true;
    ch.words[0x50/4] = 0; ch.words[0xa0/4] &= ~1u;
    load_command(n);
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}
void ApDma::refresh_status(unsigned n) {
    auto& ch = channels_[n];
    ch.words[0x3c/4] = ch.read_pending | (ch.write_pending << 8);
    ch.words[0x48/4] = !ch.active ? 0 :
        (unsigned(ch.read_remaining || ch.read_pending || ch.need_command || ch.fetching) |
         (unsigned(ch.write_remaining || ch.write_pending || ch.need_command || ch.fetching) << 1));
}
void ApDma::fail(unsigned n, std::uint32_t error) {
    auto& ch = channels_[n];
    ch.fault = true; ch.need_command = false;
    ch.words[0xa0/4] |= error;
    if (!ch.read_pending && !ch.write_pending) ch.active = false;
    refresh_status(n);
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}
void ApDma::finish(unsigned n, std::uint32_t error) {
    if (error) { fail(n,error); return; }
    auto& ch = channels_[n]; auto& w = ch.words;
    auto commands = ((w[0x50/4] & 0xfff) + 1) & 0xfff;
    auto events = (w[0x50/4] >> 16) & 15;
    if (w[3] & 1) { events = std::min(events + 1, 15u); w[0xa0/4] |= 1; }
    w[0x50/4] = commands | (events << 16);
    if (w[3] & 2) ch.active = false;
    else ch.need_command = true;
    ch.progress = sc_core::sc_time_stamp();
    refresh_status(n);
    state_changed_.notify(sc_core::SC_ZERO_TIME);
}
unsigned ApDma::pending_limit(unsigned n, bool read) const {
    const auto& w = channels_[n].words;
    const auto id = (w[0x20/4] >> (read ? 0 : 16)) & 31;
    const auto cfg = w[(read ? 0x10 : 0x14)/4];
    if (id || !(cfg & 0x40000000)) return 1;
    return std::clamp((cfg >> 24) & 15u, 1u, 4u);
}
bool ApDma::eligible(unsigned n, bool read) const {
    const auto& ch = channels_[n]; const auto& w = ch.words;
    if (!ch.active || ch.fault || !w[0x40/4]) return false;
    if (read && ch.need_command) return !ch.fetching && !ch.read_pending && !ch.write_pending;
    if (ch.need_command || ch.fetching) return false;
    if ((read ? ch.read_pending : ch.write_pending) >= pending_limit(n,read)) return false;
    if (!request_ready((w[0x20/4] >> (read ? 0 : 16)) & 31,read)) return false;
    const auto capacity = read ? 32-ch.used-ch.swap_count-ch.read_reserved-ch.write_reserved : ch.used;
    return capacity && (read ? ch.read_remaining : ch.write_remaining);
}
unsigned ApDma::select(bool read) {
    auto& arb = read ? read_arbiter_ : write_arbiter_;
    if (arb.current < 8 && arb.tokens && eligible(arb.current,read)) { --arb.tokens; return arb.current; }
    for (unsigned i=0;i<8;++i) {
        const auto n=(arb.next+i)%8;
        if (!eligible(n,read)) continue;
        arb.current=n; arb.next=(n+1)%8;
        arb.tokens=std::max(1u,(channels_[n].words[(read?0x10:0x14)/4]>>16)&63)-1;
        return n;
    }
    return 8;
}
bool ApDma::issue(bool read) {
    auto& slots=read?reads_:writes_;
    unsigned slot=0; while(slot<4 && slots[slot].busy) ++slot;
    if(slot==4) return false;
    const auto n=select(read); if(n==8) return false;
    auto& ch=channels_[n]; auto& w=ch.words; auto& job=slots[slot];
    if (!fifo_valid(n)) return true;
    job=Job{}; job.busy=true; job.channel=n; job.epoch=reset_epoch_;
    job.issued=sc_core::sc_time_stamp(); job.descriptor=read && ch.need_command;
    const auto cfg=w[(read?0x10:0x14)/4];
    job.peripheral=job.descriptor?0:(w[0x20/4]>>(read?0:16))&31;
    job.increment=job.descriptor || (cfg & 0x80000000);
    job.address=job.descriptor?(w[3]&~3u):(read?ch.source:ch.destination);
    unsigned count=16;
    if(job.descriptor) job.unit=4;
    else {
        const auto capacity=read?32-ch.used-ch.swap_count-ch.read_reserved-ch.write_reserved:ch.used;
        count=burst_bytes(job.address,read?ch.read_remaining:ch.write_remaining,capacity,cfg);
        job.unit=unit_bytes(job.address,count);
        if(!job.peripheral && !job.increment) count=job.unit;
    }
    if (!read && count > ch.used) { job=Job{}; fail(n,1u<<6); return true; }
    job.data.resize(count);
    const auto attributes = w[0x24/4];
    if (!job.descriptor && (attributes & (1u << (read?7:15)))) {
        job.cache = (attributes >> (read?0:8)) & 15;
        job.prot = (attributes >> (read?4:12)) & 3;
    } else {
        job.cache = job.peripheral ? (read?config_.peripheral_read_cache:config_.peripheral_write_cache) :
            (read?config_.memory_read_cache:config_.memory_write_cache);
        job.prot = job.peripheral ? (read?config_.peripheral_read_prot:config_.peripheral_write_prot) :
            (read?config_.memory_read_prot:config_.memory_write_prot);
    }
    if(read) {
        ++ch.read_pending;
        if(job.descriptor) { ch.need_command=false; ch.fetching=true; }
        else { ch.read_reserved+=count; ch.read_remaining-=count; if(job.increment) ch.source+=count; }
    } else {
        ++ch.write_pending; ch.write_reserved+=count; ch.write_remaining-=count;
        std::copy_n(ch.fifo.begin(),count,job.data.begin());
        std::move(ch.fifo.begin()+count,ch.fifo.begin()+ch.used,ch.fifo.begin()); ch.used-=count;
        if(job.increment) ch.destination+=count;
    }
    if(job.peripheral) {
        const auto bit=1u<<job.peripheral;
        (read?rx_owned_:tx_owned_)|=bit;
        if((read?rx_request.read():tx_request.read()).to_uint()&bit)
            (read?rx_blocked_:tx_blocked_)|=bit;
    }
    (read?read_order_:write_order_).push_back(slot);
    refresh_status(n);
    (read?read_jobs_:write_jobs_)[slot].notify(sc_core::SC_ZERO_TIME);
    return true;
}
void ApDma::worker(bool read,unsigned slot) {
    auto& job=(read?reads_:writes_)[slot];
    while(true) {
        wait((read?read_jobs_:write_jobs_)[slot]);
        for(unsigned offset=0;offset<job.data.size();) {
            while(!clock_enabled_ && job.epoch==reset_epoch_ && reset_n.read())
                wait(clock_gate_changed_|reset_n.negedge_event());
            if(job.epoch!=reset_epoch_ || !reset_n.read() || job.timed_out) break;
            const std::uint32_t address=job.increment?job.address+offset:job.address;
            const auto count=job.increment?std::min(unsigned(job.data.size())-offset,4096u-(address&4095)):job.unit;
            TransferExtension meta;
            meta.channel=job.channel; meta.unit_bytes=job.unit; meta.beats=count/job.unit;
            meta.increment=job.increment; meta.command_fetch=job.descriptor;
            meta.cache=job.cache; meta.prot=job.prot; meta.beat_index=offset/job.unit;
            meta.logical_beats=job.data.size()/job.unit;
            meta.phase=read?BusPhase::ReadData:BusPhase::WriteResponse;
            meta.phase_started=sc_core::sc_time_stamp(); meta.phase_changed=&state_changed_;
            if (!read) for (unsigned beat=0;beat<meta.beats;++beat) {
                const auto beat_address=address+(job.increment?beat*job.unit:0);
                meta.write_strobes.push_back(((1u<<job.unit)-1)<<(beat_address&3));
            }
            job.live_metadata=&meta;
            tlm::tlm_generic_payload tx;
            tx.set_command(read?tlm::TLM_READ_COMMAND:tlm::TLM_WRITE_COMMAND);
            tx.set_address(address); tx.set_data_ptr(job.data.data()+offset);
            tx.set_data_length(count); tx.set_streaming_width(count); tx.set_extension(&meta);
            sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
            master_socket->b_transport(tx,delay);
            if(delay!=sc_core::SC_ZERO_TIME) wait(delay);
            // Check the actual completion time before done/retirement can hide
            // a late response from a scheduler waking at the same timestamp.
            check_timeout(read,job);
            tx.clear_extension<TransferExtension>();
            job.live_metadata=nullptr;
            job.response=tx.get_response_status();
            if (meta.response_id_valid && meta.response_id != 4)
                job.error_bits |= 1u<<(read?3:4);
            if (meta.faults & FifoOverflow) job.error_bits |= 1u<<5;
            if (meta.faults & FifoUnderflow) job.error_bits |= 1u<<6;
            if (meta.faults & (ReadLastError|WriteCreditError|WriteStrobeError|MultipleFifoReturns))
                job.error_bits |= 1u<<(read?3:4);
            if(job.response!=tlm::TLM_OK_RESPONSE || job.error_bits) break;
            offset+=count;
        }
        job.done=true; state_changed_.notify(sc_core::SC_ZERO_TIME);
    }
}
#define FU3_WORKER(NAME, READ, SLOT) void ApDma::NAME() { worker(READ,SLOT); }
FU3_WORKER(read0,true,0) FU3_WORKER(read1,true,1) FU3_WORKER(read2,true,2) FU3_WORKER(read3,true,3)
FU3_WORKER(write0,false,0) FU3_WORKER(write1,false,1) FU3_WORKER(write2,false,2) FU3_WORKER(write3,false,3)
#undef FU3_WORKER
void ApDma::check_timeout(bool read, Job& job) {
    if (!reset_n.read() || !job.busy || job.done || job.timed_out || job.epoch!=reset_epoch_) return;
    const auto now=sc_core::sc_time_stamp();
    std::uint32_t bits=0;
    if (config_.transaction_timeout!=sc_core::SC_ZERO_TIME && now-job.issued>config_.transaction_timeout)
        bits=1u<<(read?7:9);
    else if (job.live_metadata) {
        const auto& meta=*job.live_metadata;
        for (unsigned index=0;index<config_.phase_timeouts.size();++index) {
            const auto threshold=config_.phase_timeouts[index];
            if (threshold==sc_core::SC_ZERO_TIME) continue;
            auto elapsed=meta.completed_phase_durations[index];
            if (index==static_cast<unsigned>(meta.phase)) elapsed=std::max(elapsed,now-meta.phase_started);
            if (elapsed>threshold) { bits=phase_interrupt(static_cast<BusPhase>(index)); break; }
        }
    }
    if (bits) { job.timed_out=true; fail(job.channel,bits); }
}
bool ApDma::retire(bool read) {
    bool changed=false; auto& slots=read?reads_:writes_; auto& order=read?read_order_:write_order_;
    for(auto& job:slots) if(job.busy && job.done && job.epoch!=reset_epoch_) { job=Job{}; changed=true; }
    while(!order.empty() && slots[order.front()].done) {
        auto& job=slots[order.front()]; auto& ch=channels_[job.channel]; auto& w=ch.words;
        if(read) { --ch.read_pending; if(!job.descriptor) ch.read_reserved-=job.data.size(); }
        else { --ch.write_pending; ch.write_reserved-=job.data.size(); }
        if(job.peripheral) (read?rx_owned_:tx_owned_)&=~(1u<<job.peripheral);
        const bool success=!job.timed_out && !job.error_bits && job.response==tlm::TLM_OK_RESPONSE;
        if(!success && !job.timed_out) {
            const bool decode=job.response==tlm::TLM_ADDRESS_ERROR_RESPONSE ||
                job.response==tlm::TLM_BURST_ERROR_RESPONSE || job.response==tlm::TLM_COMMAND_ERROR_RESPONSE;
            auto bits=job.error_bits;
            if (job.response!=tlm::TLM_OK_RESPONSE) bits |= 1u<<(read?(decode?3:1):(decode?4:2));
            fail(job.channel,bits);
        }
        if(success && !ch.fault) {
            if(read && job.descriptor) {
                for(unsigned i=0;i<4;++i) {
                    w[i]=0; for(unsigned j=0;j<4;++j) w[i]|=std::uint32_t(job.data[i*4+j])<<(8*j);
                }
                w[2]&=0x3ff; load_command(job.channel);
            } else if(read) {
                const auto group=1u<<((w[0x18/4]>>28)&3);
                for(auto byte:job.data) {
                    ch.swap_pending[ch.swap_count++]=byte;
                    if(ch.swap_count==group) {
                        if (ch.used+group > ch.fifo.size()) { fail(job.channel,1u<<5); break; }
                        for(unsigned j=0;j<group;++j) ch.fifo[ch.used++]=ch.swap_pending[group-1-j];
                        ch.swap_count=0;
                    }
                }
            }
            if(job.peripheral && !ch.fault) acknowledge(job.channel,job.peripheral,read);
            if(!ch.fault) ch.progress=sc_core::sc_time_stamp();
        }
        if(job.descriptor) ch.fetching=false;
        if(ch.fault && !ch.read_pending && !ch.write_pending) ch.active=false;
        fifo_valid(job.channel);
        refresh_status(job.channel);
        order.pop_front(); job=Job{}; changed=true;
    }
    return changed;
}
sc_core::sc_time ApDma::next_deadline() const {
    const auto now=sc_core::sc_time_stamp(); auto result=sc_core::sc_max_time();
    const auto consider=[&](sc_core::sc_time t) { if(t>now) result=std::min(result,t); };
    for(unsigned id=1;id<32;++id) {
        consider(rx_until_[id]); consider(tx_until_[id]);
        consider(rx_ready_[id]); consider(tx_ready_[id]);
    }
    if (reset_n.read()) consider(next_dispatch_);
    for(const auto& ch:channels_) if(ch.active && !ch.fault && ch.words[0x40/4]) {
        if(config_.watchdog_timeout!=sc_core::SC_ZERO_TIME) consider(ch.progress+config_.watchdog_timeout+sc_core::sc_get_time_resolution());
    }
    if(config_.transaction_timeout!=sc_core::SC_ZERO_TIME)
        for(bool read:{true,false}) for(const auto& job:read?reads_:writes_)
            if(job.busy && job.epoch==reset_epoch_ && !job.done && !job.timed_out)
                consider(job.issued+config_.transaction_timeout+sc_core::sc_get_time_resolution());
    for (bool read : {true,false}) for (const auto& job : read?reads_:writes_)
        if (job.busy && job.epoch==reset_epoch_ && !job.done && !job.timed_out && job.live_metadata) {
            const auto& meta=*job.live_metadata;
            const auto index=static_cast<unsigned>(meta.phase);
            if (index<config_.phase_timeouts.size() && config_.phase_timeouts[index]!=sc_core::SC_ZERO_TIME)
                consider(meta.phase_started+config_.phase_timeouts[index]+sc_core::sc_get_time_resolution());
        }
    return result;
}
void ApDma::engine() {
    while(true) {
        if(!clock_enabled_ && reset_n.read()) {
            wait(clock_gate_changed_|reset_n.negedge_event());
            continue;
        }
        ++scheduler_wakeups_;
        bool changed=retire(true); changed=retire(false)||changed;
        if(reset_n.read()) {
            const auto now=sc_core::sc_time_stamp();
            for (bool read : {true,false}) for (auto& job : read?reads_:writes_)
                check_timeout(read,job);
            for(unsigned n=0;n<8;++n) {
                auto& ch=channels_[n];
                if(!ch.active || ch.fault) continue;
                if(config_.watchdog_timeout!=sc_core::SC_ZERO_TIME && ch.words[0x40/4] &&
                   now-ch.progress>config_.watchdog_timeout) { fail(n,1u<<12); continue; }
                if(!ch.need_command && !ch.fetching && !ch.read_remaining && !ch.write_remaining &&
                   !ch.read_pending && !ch.write_pending && !ch.used && !ch.swap_count) { finish(n); changed=true; }
            }
            if (now>=next_dispatch_) {
                const bool read_issued=issue(true),write_issued=issue(false);
                if (read_issued || write_issued) {
                    const auto quantum=config_.scheduler_quantum==sc_core::SC_ZERO_TIME?
                        sc_core::sc_get_time_resolution():config_.scheduler_quantum;
                    next_dispatch_=now+quantum;
                    changed=true;
                }
            }
        }
        pulse_changed_.notify(sc_core::SC_ZERO_TIME);
        if(changed) {
            if(config_.scheduler_quantum==sc_core::SC_ZERO_TIME)
                wait(sc_core::sc_get_time_resolution(),state_changed_);
            else {
                const auto deadline=next_deadline();
                const auto duration=deadline==sc_core::sc_max_time()?config_.scheduler_quantum:
                    std::min(config_.scheduler_quantum,deadline-sc_core::sc_time_stamp());
                wait(duration,state_changed_);
            }
        } else {
            const auto deadline=next_deadline();
            if(deadline==sc_core::sc_max_time()) wait(state_changed_);
            else wait(deadline-sc_core::sc_time_stamp(),state_changed_);
        }
    }
}
void ApDma::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    // An autonomous engine must not observe an MMIO write before its arrival.
    if (delay!=sc_core::SC_ZERO_TIME) { wait(delay); delay=sc_core::SC_ZERO_TIME; }
    const auto command = tx.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    if (tx.get_byte_enable_ptr()) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
    }
    if (!tx.get_data_ptr() || tx.get_data_length() != 4 || tx.get_streaming_width() < 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    const auto address = tx.get_address();
    if (address >= 0x1100 || (address & 3)) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
    }
    if (!reset_n.read()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    auto* bytes = tx.get_data_ptr();
    std::uint32_t value = 0;
    const bool write = command == tlm::TLM_WRITE_COMMAND;
    if (write)
        for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[i]) << (8 * i);
    const auto result = access(write, static_cast<std::uint32_t>(address), value);
    if (!write && result == tlm::TLM_OK_RESPONSE)
        for (unsigned i = 0; i < 4; ++i) bytes[i] = (value >> (8 * i)) & 0xff;
    tx.set_response_status(result);
    if (write && result == tlm::TLM_OK_RESPONSE)
        state_changed_.notify(sc_core::SC_ZERO_TIME);
}
} // namespace fu3::dma
