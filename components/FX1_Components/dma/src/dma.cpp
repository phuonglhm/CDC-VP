#include "dma/dma.h"
#include <algorithm>
#include <stdexcept>

namespace fx1::dma {
namespace {
std::uint32_t load_le(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
void store_le(unsigned char* p, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (8*i));
}
unsigned swap_bytes(const std::uint32_t mode) {
    const auto encoding = (mode >> 28) & 3;
    return encoding == 0 ? 1 : encoding == 1 ? 2 : 4;
}
}

Dma::Dma(sc_core::sc_module_name name, const Config& config)
    : sc_module(name), config_(config) {
    if (config_.cycle <= sc_core::SC_ZERO_TIME ||
        config_.transaction_timeout < sc_core::SC_ZERO_TIME ||
        config_.watchdog_timeout < sc_core::SC_ZERO_TIME)
        throw std::invalid_argument("DMA timing parameters must be nonnegative; cycle must be positive");
    if ((config_.design_capability & 15) != 1 ||
        config_.memory_read_cache > 15 || config_.memory_write_cache > 15 ||
        config_.peripheral_read_cache > 15 || config_.peripheral_write_cache > 15 ||
        config_.memory_read_prot > 7 || config_.memory_write_prot > 7)
        throw std::invalid_argument("DMA integration attributes or interrupt destination count invalid");
    target_socket.register_b_transport(this, &Dma::b_transport);
    target_socket.register_transport_dbg(this, &Dma::transport_dbg);
    SC_METHOD(reset);
    sensitive << reset_n.neg();
    SC_METHOD(inputs_changed);
    sensitive << rx_request << tx_request << reset_n.pos();
    SC_METHOD(drive_outputs);
    sensitive << outputs_;
    SC_THREAD(scheduler);
    SC_THREAD(read0); SC_THREAD(read1); SC_THREAD(read2); SC_THREAD(read3);
    SC_THREAD(write0); SC_THREAD(write1); SC_THREAD(write2); SC_THREAD(write3);
}

void Dma::reset() {
    if (reset_n.read()) return;
    ++epoch_;
    channels_ = {};
    priority_ = software_rx_ = software_tx_ = 0;
    consumed_rx_ = consumed_tx_ = owned_rx_ = owned_tx_ = 0;
    clear_rx_ = clear_tx_ = 0;
    read_order_.clear(); write_order_.clear();
    read_arbiter_ = {}; write_arbiter_ = {};
    rx_clear_until_.fill(sc_core::SC_ZERO_TIME);
    tx_clear_until_.fill(sc_core::SC_ZERO_TIME);
    // A blocking call already inside an external target cannot be cancelled.
    // Keep those slots occupied until they return; discard their old epoch.
    outputs_.notify(sc_core::SC_ZERO_TIME);
    kick_.notify(sc_core::SC_ZERO_TIME);
}

void Dma::inputs_changed() {
    consumed_rx_ &= rx_request.read();
    consumed_tx_ &= tx_request.read();
    kick_.notify(sc_core::SC_ZERO_TIME);
}

std::uint32_t Dma::core_status() const {
    std::uint32_t result = 0;
    for (unsigned i = 0; i < channels_.size(); ++i)
        if (channels_[i].raw & channels_[i].interrupt_enable) result |= 1u << i;
    return result;
}
bool Dma::idle() const {
    for (const auto& ch : channels_) if (ch.active) return false;
    for (const auto& task : reads_) if (task.busy) return false;
    for (const auto& task : writes_) if (task.busy) return false;
    return true;
}
std::uint32_t Dma::capabilities() const {
    // Token, outstanding, independent R/W, peripheral, command chain, swap.
    std::uint32_t value = 0x0f14;
    if (config_.watchdog_timeout != sc_core::SC_ZERO_TIME) value |= 1;
    if (config_.transaction_timeout != sc_core::SC_ZERO_TIME) value |= 2;
    return value;
}
void Dma::drive_outputs() {
    irq.write(reset_n.read() && core_status() != 0);
    rx_clear.write(reset_n.read() ? clear_rx_ : 0);
    tx_clear.write(reset_n.read() ? clear_tx_ : 0);
}

void Dma::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    // A register operation takes effect at its annotated arrival time.
    if (delay != sc_core::SC_ZERO_TIME) { sc_core::wait(delay); delay = sc_core::SC_ZERO_TIME; }
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    if (!tx.get_data_ptr()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
    }
    if (tx.get_byte_enable_ptr()) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
    }
    if (tx.get_data_length() != 4 || tx.get_streaming_width() < 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    if (!reset_n.read()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
    }
    auto value = tx.is_write() ? load_le(tx.get_data_ptr()) : 0;
    if (!register_access(tx.get_address(), tx.is_write(), value)) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
    }
    if (tx.is_read()) store_le(tx.get_data_ptr(), value);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned Dma::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Debug is side-effect-free: reads only, with no time advancement.
    tx.set_dmi_allowed(false);
    if (!tx.is_read() || !tx.get_data_ptr() || tx.get_data_length() != 4 ||
        tx.get_byte_enable_ptr() || tx.get_streaming_width() < 4) return 0;
    std::uint32_t value = 0;
    if (!register_access(tx.get_address(), false, value)) return 0;
    store_le(tx.get_data_ptr(), value);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}

bool Dma::register_access(std::uint64_t address, bool write, std::uint32_t& value) {
    if ((address & 3) || address >= reg::REGISTER_BYTES) return false;
    if (address < reg::CHANNEL_COUNT * reg::CHANNEL_STRIDE)
        return channel_access(static_cast<unsigned>(address / reg::CHANNEL_STRIDE),
                              static_cast<unsigned>(address % reg::CHANNEL_STRIDE), write, value);
    switch (address) {
    case reg::CORE_STATUS: if (!write) value = core_status(); break;
    case reg::CORE_JOINT_CONFIG:
    case reg::CORE_CLOCK_DIVIDER: if (!write) value = 0; break;
    case reg::CORE_PRIORITY_CONFIG:
        if (write) priority_ = value & 0xffff; else value = priority_; break;
    case reg::CORE_CHANNEL_START:
        if (write) { for (unsigned i = 0; i < channels_.size(); ++i) if (value & (1u << i)) start(i); }
        else value = 0;
        break;
    case reg::PERIPHERAL_RX_REQUEST:
        if (write) software_rx_ = value & 0xfffffffe; else value = software_rx_; break;
    case reg::PERIPHERAL_TX_REQUEST:
        if (write) software_tx_ = value & 0xfffffffe; else value = software_tx_; break;
    case reg::CORE_IDLE_STATUS: if (!write) value = idle() ? 1 : 0; break;
    case reg::DESIGN_CAPABILITY_STATUS: if (!write) value = config_.design_capability; break;
    case reg::CORE_CAPABILITY_STATUS0: if (!write) value = reg::CAPABILITY0; break;
    case reg::CORE_CAPABILITY_STATUS1: if (!write) value = capabilities(); break;
    default: return false;
    }
    if (write) { kick_.notify(sc_core::SC_ZERO_TIME); outputs_.notify(sc_core::SC_ZERO_TIME); }
    return true;
}

bool Dma::channel_access(unsigned n, std::uint32_t offset, bool write, std::uint32_t& value) {
    auto& ch = channels_[n];
    const auto rw = [&](std::uint32_t& target, std::uint32_t mask) {
        // Undefined live reprogramming is ignored; controls/interrupts remain usable.
        if (write) { if (!ch.active && !ch.read_pending && !ch.write_pending) target = value & mask; }
        else value = target;
    };
    switch (offset) {
    case reg::CH_CMD_READ_ADDR: rw(ch.source, 0xffffffff); break;
    case reg::CH_CMD_WRITE_ADDR: rw(ch.destination, 0xffffffff); break;
    case reg::CH_CMD_TRANSFER_SIZE: rw(ch.size, reg::TRANSFER_SIZE_MASK); break;
    case reg::CH_CMD_CONTROL: rw(ch.control, 0xffffffff); break;
    case reg::CH_READ_CONFIG: rw(ch.read_config, reg::CONFIG_MASK); break;
    case reg::CH_WRITE_CONFIG: rw(ch.write_config, reg::CONFIG_MASK); break;
    case reg::CH_MODE_CONFIG: rw(ch.mode, reg::MODE_MASK); break;
    case reg::CH_PERIPHERAL_CONFIG: rw(ch.peripheral, reg::PERIPHERAL_MASK); break;
    case reg::CH_AXI_ATTR_REG: rw(ch.attributes, reg::ATTR_MASK); break;
    case reg::CH_SCHEDULE_CONFIG:
    case reg::CH_READ_OFFSET_STATUS:
    case reg::CH_WRITE_OFFSET_STATUS: if (!write) value = 0; break;
    case reg::CH_RESTRICTION_STATUS: if (!write) value = ch.restriction; break;
    case reg::CH_FIFO_STATUS:
        if (!write) value = (reg::FIFO_BYTES - ch.fifo.size() - ch.swap_pending.size() - ch.reserved) |
                           ((ch.fifo.size() + ch.swap_pending.size()) << 16);
        break;
    case reg::CH_OUTSTANDING_STATUS:
        if (!write) value = ch.read_pending | (ch.write_pending << 8);
        break;
    case reg::CH_ENABLE:
        if (write) {
            const bool enabled = (value & 1) != 0;
            if (enabled && !ch.enabled) ch.progress = sc_core::sc_time_stamp();
            ch.enabled = enabled;
        } else value = ch.enabled;
        break;
    case reg::CH_START: if (write) { if (value & 1) start(n); } else value = 0; break;
    case reg::CH_ACTIVE_STATUS:
        if (!write) value = ch.active ?
            ((ch.read_remaining || ch.read_pending || ch.need_command || ch.fetching_command ? 1u : 0u) |
             (ch.write_remaining || ch.write_pending || ch.need_command || ch.fetching_command ? 2u : 0u)) : 0;
        break;
    case reg::CH_TRANSFER_COUNT:
        if (!write) value = ch.completed | (ch.completion_events << 16);
        break;
    case reg::CH_INTERRUPT_RAW_STATUS:
        if (write) ch.raw |= value & reg::INTERRUPT_MASK; else value = ch.raw; break;
    case reg::CH_INTERRUPT_CLEAR:
        if (write) {
            if ((value & reg::COMMAND_COMPLETE) && ch.completion_events) --ch.completion_events;
            ch.raw &= ~(value & reg::INTERRUPT_MASK);
            if (ch.completion_events) ch.raw |= reg::COMMAND_COMPLETE;
        } else value = 0;
        break;
    case reg::CH_INTERRUPT_ENABLE:
        if (write) ch.interrupt_enable = value & reg::INTERRUPT_MASK; else value = ch.interrupt_enable; break;
    case reg::CH_INTERRUPT_STATUS: if (!write) value = ch.raw & ch.interrupt_enable; break;
    default: return false;
    }
    if (write) { outputs_.notify(sc_core::SC_ZERO_TIME); kick_.notify(sc_core::SC_ZERO_TIME); }
    return true;
}

unsigned Dma::peripheral(const Channel& ch, bool read) const {
    return (ch.peripheral >> (read ? 0 : 16)) & 31;
}
unsigned Dma::pending_limit(const Channel& ch, bool read) const {
    const auto conf = read ? ch.read_config : ch.write_config;
    if (peripheral(ch, read) || !(conf & reg::OUTSTANDING_ENABLE)) return 1;
    return std::clamp((conf >> 24) & 15u, 1u, 4u);
}
void Dma::start(unsigned n) {
    auto& ch = channels_[n];
    if (!reset_n.read() || !ch.enabled || ch.active || ch.read_pending || ch.write_pending) return;
    ch.completed = 0;
    // Existing unhandled completion events persist until software acknowledges them.
    ch.fault = false; ch.active = true;
    ch.fifo.clear(); ch.swap_pending.clear(); ch.reserved = 0;
    ch.need_command = ch.fetching_command = false;
    ch.read_ready = ch.write_ready = sc_core::sc_time_stamp();
    begin_command(n);
    kick_.notify(sc_core::SC_ZERO_TIME);
}
void Dma::begin_command(unsigned n) {
    auto& ch = channels_[n];
    ch.read_remaining = ch.write_remaining = ch.size;
    ch.progress = sc_core::sc_time_stamp();
    const bool source_aligned = (ch.source & (reg::FIFO_BYTES - 1)) == 0;
    const bool destination_aligned = (ch.destination & (reg::FIFO_BYTES - 1)) == 0;
    ch.restriction = unsigned(source_aligned) | (unsigned(destination_aligned) << 1) |
                     (unsigned(source_aligned && destination_aligned) << 2) |
                     (unsigned(pending_limit(ch, true) > 1) << 5) |
                     (unsigned(pending_limit(ch, false) > 1) << 6) |
                     (unsigned(!peripheral(ch, true) && !peripheral(ch, false)) << 8);
    if ((ch.mode >> 28) == 3 || (ch.size % swap_bytes(ch.mode)) != 0 ||
        (peripheral(ch, true) && peripheral(ch, false))) {
        fail(n, reg::READ_DECERR); return;
    }
    if (ch.size && !(ch.read_config & 127)) { fail(n, reg::READ_DECERR); return; }
    if (ch.size && !(ch.write_config & 127)) { fail(n, reg::WRITE_DECERR); return; }
}
void Dma::finish_command(unsigned n) {
    auto& ch = channels_[n];
    ch.completed = (ch.completed + 1) & 0xfff;
    if (ch.control & reg::CMD_SET_INT) {
        ch.completion_events = std::min(ch.completion_events + 1, 15u);
        ch.raw |= reg::COMMAND_COMPLETE;
        outputs_.notify(sc_core::SC_ZERO_TIME);
    }
    if (ch.control & reg::CMD_LAST) ch.active = false;
    else ch.need_command = true;
    ch.progress = sc_core::sc_time_stamp();
}
void Dma::fail(unsigned n, std::uint32_t bits) {
    auto& ch = channels_[n];
    ch.raw |= bits;
    ch.fault = true;
    ch.need_command = false;
    if (!ch.read_pending && !ch.write_pending) ch.active = false;
    outputs_.notify(sc_core::SC_ZERO_TIME);
}

Dma::Plan Dma::plan(unsigned n, bool read) const {
    const auto& ch = channels_[n];
    const auto conf = read ? ch.read_config : ch.write_config;
    const auto address = read ? ch.source : ch.destination;
    const auto remaining = read ? ch.read_remaining : ch.write_remaining;
    const auto p = peripheral(ch, read);
    const unsigned capacity = read ? reg::FIFO_BYTES - static_cast<unsigned>(ch.fifo.size() + ch.swap_pending.size()) - ch.reserved :
                                    static_cast<unsigned>(ch.fifo.size());
    const unsigned maximum = conf & 127;
    // No transfer-size field exists in the workbook. Infer the largest legal
    // aligned 1/2/4-byte beat from the byte burst maximum and remaining amount.
    unsigned unit = 4;
    while (unit > 1 && ((address % unit) || maximum < unit || remaining < unit ||
                       (p && maximum % unit))) unit /= 2;
    auto bytes = std::min({maximum, remaining, capacity, reg::MAX_BURST_BEATS * unit});
    if (conf & reg::ADDRESS_INCREMENT) {
        bytes = std::min(bytes, 4096u - (address & 4095));
        // Reduce a burst at a FIFO-sized alignment boundary.
        bytes = std::min(bytes, reg::FIFO_BYTES - (address & (reg::FIFO_BYTES - 1)));
    }
    bytes -= bytes % unit;
    return {bytes, unit};
}

bool Dma::eligible(unsigned n, bool read) const {
    const auto& ch = channels_[n];
    if (!ch.active || ch.fault || !ch.enabled) return false;
    if (read && ch.need_command && !ch.fetching_command) return ch.read_pending == 0;
    if (ch.need_command || ch.fetching_command) return false;
    if ((read ? ch.read_pending : ch.write_pending) >= pending_limit(ch, read)) return false;
    if (sc_core::sc_time_stamp() < (read ? ch.read_ready : ch.write_ready)) return false;
    const auto p = peripheral(ch, read);
    if (p) {
        const auto bit = 1u << p;
        if ((read ? owned_rx_ : owned_tx_) & bit) return false;
        const auto requests = (read ? software_rx_ : software_tx_) |
                              ((read ? rx_request.read() : tx_request.read()) &
                               ~(read ? consumed_rx_ : consumed_tx_));
        if (!(requests & bit)) return false;
    }
    return plan(n, read).bytes != 0;
}

unsigned Dma::select(bool read) {
    auto& arb = read ? read_arbiter_ : write_arbiter_;
    if (arb.current < channels_.size() && arb.tokens && eligible(arb.current, read)) {
        --arb.tokens; return arb.current;
    }
    for (unsigned k = 0; k < channels_.size(); ++k) {
        const auto n = (arb.next + k) % channels_.size();
        if (!eligible(n, read)) continue;
        arb.current = n; arb.next = (n + 1) % channels_.size();
        const auto conf = read ? channels_[n].read_config : channels_[n].write_config;
        arb.tokens = std::max(1u, (conf >> 16) & 63u) - 1;
        return n;
    }
    return reg::CHANNEL_COUNT;
}

bool Dma::issue(bool read) {
    auto& slots = read ? reads_ : writes_;
    unsigned slot = 0;
    while (slot < slots.size() && slots[slot].busy) ++slot;
    if (slot == slots.size()) return false;
    const auto n = select(read);
    if (n == reg::CHANNEL_COUNT) return false;
    auto& ch = channels_[n];
    auto& task = slots[slot];
    task = {};
    task.busy = true; task.channel = n; task.epoch = epoch_;
    task.issued = sc_core::sc_time_stamp();
    task.descriptor = read && ch.need_command;
    task.peripheral = task.descriptor ? 0 : peripheral(ch, read);
    const auto conf = read ? ch.read_config : ch.write_config;
    task.increment = task.descriptor || (conf & reg::ADDRESS_INCREMENT);
    const auto transfer = task.descriptor ? Plan{16, 4} : plan(n, read);
    task.unit = transfer.unit;
    task.address = task.descriptor ? ch.control & ~3u : read ? ch.source : ch.destination;
    task.data.resize(transfer.bytes);
    if (read) {
        ++ch.read_pending;
        if (task.descriptor) { ch.need_command = false; ch.fetching_command = true; }
        else {
            ch.read_remaining -= transfer.bytes;
            ch.reserved += transfer.bytes;
            if (task.increment) ch.source += transfer.bytes;
        }
    } else {
        ++ch.write_pending;
        ch.write_remaining -= transfer.bytes;
        for (auto& byte : task.data) { byte = ch.fifo.front(); ch.fifo.pop_front(); }
        if (task.increment) ch.destination += transfer.bytes;
    }
    const auto override_bit = 1u << (read ? 7 : 15);
    if (!task.descriptor && (ch.attributes & override_bit)) {
        task.cache = (ch.attributes >> (read ? 0 : 8)) & 15;
        task.prot = (ch.attributes >> (read ? 4 : 12)) & 3;
    } else {
        task.cache = task.peripheral ? (read ? config_.peripheral_read_cache : config_.peripheral_write_cache) :
                                      (read ? config_.memory_read_cache : config_.memory_write_cache);
        task.prot = task.peripheral ? 0 : (read ? config_.memory_read_prot : config_.memory_write_prot);
    }
    if (task.peripheral) {
        const auto bit = 1u << task.peripheral;
        (read ? owned_rx_ : owned_tx_) |= bit;
        task.external_credit = ((read ? rx_request.read() : tx_request.read()) & bit) != 0;
        if (task.external_credit) (read ? consumed_rx_ : consumed_tx_) |= bit;
    }
    (read ? read_order_ : write_order_).push_back(slot);
    (read ? read_jobs_ : write_jobs_)[slot].notify(sc_core::SC_ZERO_TIME);
    return true;
}

void Dma::worker(bool read, unsigned slot) {
    auto& task = (read ? reads_ : writes_)[slot];
    auto& job = (read ? read_jobs_ : write_jobs_)[slot];
    while (true) {
        sc_core::wait(job);
        if (!task.busy) continue;
        AxiExtension ext;
        ext.channel = task.channel; ext.beat_bytes = task.unit;
        ext.beats = static_cast<unsigned>(task.data.size()) / task.unit;
        ext.cache = task.cache; ext.prot = task.prot;
        ext.increment = task.increment; ext.fragmented = !task.increment && ext.beats > 1;
        ext.command_fetch = task.descriptor;
        for (unsigned offset = 0; offset < task.data.size();) {
            if (task.epoch != epoch_ || !reset_n.read()) break;
            const std::uint32_t address = task.increment ? task.address + offset : task.address;
            const unsigned chunk = task.increment ?
                std::min(static_cast<unsigned>(task.data.size()) - offset, 4096u - (address & 4095)) : task.unit;
            tlm::tlm_generic_payload tx;
            tx.set_command(read ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND);
            tx.set_address(address);
            tx.set_data_ptr(task.data.data() + offset);
            tx.set_data_length(chunk); tx.set_streaming_width(chunk);
            tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            ext.beat_index = offset / task.unit;
            ext.fragmented = chunk != task.data.size();
            tx.set_extension(&ext);
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            master_socket->b_transport(tx, delay);
            tx.clear_extension<AxiExtension>();
            if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
            task.response = tx.get_response_status();
            if (task.response != tlm::TLM_OK_RESPONSE || task.timed_out) break;
            offset += chunk;
        }
        task.done = true;
        kick_.notify(sc_core::SC_ZERO_TIME);
    }
}
#define DMA_WORKER(NAME, READ, SLOT) void Dma::NAME() { worker(READ, SLOT); }
DMA_WORKER(read0, true, 0) DMA_WORKER(read1, true, 1)
DMA_WORKER(read2, true, 2) DMA_WORKER(read3, true, 3)
DMA_WORKER(write0, false, 0) DMA_WORKER(write1, false, 1)
DMA_WORKER(write2, false, 2) DMA_WORKER(write3, false, 3)
#undef DMA_WORKER

void Dma::release_peripheral(const Task& task, bool read) {
    if (!task.peripheral) return;
    const auto bit = 1u << task.peripheral;
    (read ? owned_rx_ : owned_tx_) &= ~bit;
    (read ? software_rx_ : software_tx_) &= ~bit;
    (read ? clear_rx_ : clear_tx_) |= bit;
    (read ? rx_clear_until_ : tx_clear_until_)[task.peripheral] = sc_core::sc_time_stamp() + config_.cycle;
    const auto delay = (channels_[task.channel].peripheral >> (read ? 8 : 24)) & 7;
    (read ? channels_[task.channel].read_ready : channels_[task.channel].write_ready) =
        sc_core::sc_time_stamp() + config_.cycle * delay;
    outputs_.notify(sc_core::SC_ZERO_TIME);
}

void Dma::retire(bool read) {
    auto& slots = read ? reads_ : writes_;
    auto& order = read ? read_order_ : write_order_;
    for (auto& task : slots)
        if (task.busy && task.done && task.epoch != epoch_) task = {};
    // All transactions carry ID 4: retain results in issue order even if a
    // reentrant TLM target returns different calls in a different order.
    while (!order.empty() && slots[order.front()].done) {
        auto& task = slots[order.front()];
        auto& ch = channels_[task.channel];
        if (read) {
            --ch.read_pending;
            if (!task.descriptor) ch.reserved -= static_cast<unsigned>(task.data.size());
        } else --ch.write_pending;
        release_peripheral(task, read);
        if (!task.timed_out && task.response != tlm::TLM_OK_RESPONSE) {
            const bool decode = task.response == tlm::TLM_ADDRESS_ERROR_RESPONSE ||
                                task.response == tlm::TLM_BURST_ERROR_RESPONSE ||
                                task.response == tlm::TLM_COMMAND_ERROR_RESPONSE;
            fail(task.channel, read ? (decode ? reg::READ_DECERR : reg::READ_SLVERR) :
                                      (decode ? reg::WRITE_DECERR : reg::WRITE_SLVERR));
        }
        if (read && task.descriptor) {
            ch.fetching_command = false;
            if (!ch.fault) {
                ch.source = load_le(task.data.data()); ch.destination = load_le(task.data.data() + 4);
                ch.size = load_le(task.data.data() + 8) & reg::TRANSFER_SIZE_MASK;
                ch.control = load_le(task.data.data() + 12);
                begin_command(task.channel);
            }
        } else if (read && !ch.fault) {
            const auto group = swap_bytes(ch.mode);
            for (auto byte : task.data) {
                ch.swap_pending.push_back(byte);
                if (ch.swap_pending.size() == group) {
                    for (auto i = ch.swap_pending.rbegin(); i != ch.swap_pending.rend(); ++i) ch.fifo.push_back(*i);
                    ch.swap_pending.clear();
                }
            }
            if (ch.fifo.size() + ch.swap_pending.size() + ch.reserved > reg::FIFO_BYTES)
                fail(task.channel, reg::FIFO_OVERFLOW);
        }
        ch.progress = sc_core::sc_time_stamp();
        if (ch.fault && !ch.read_pending && !ch.write_pending) ch.active = false;
        order.pop_front(); task = {};
    }
}

void Dma::scheduler() {
    while (true) {
        const auto now = sc_core::sc_time_stamp();
        retire(true); retire(false);
        for (unsigned p = 1; p < 32; ++p) {
            if (now >= rx_clear_until_[p]) clear_rx_ &= ~(1u << p);
            if (now >= tx_clear_until_[p]) clear_tx_ &= ~(1u << p);
        }
        if (reset_n.read()) {
            if (config_.transaction_timeout != sc_core::SC_ZERO_TIME) {
                for (const bool read : {true, false}) {
                    for (auto& task : read ? reads_ : writes_) {
                        if (task.busy && task.epoch == epoch_ && !task.done && !task.timed_out &&
                            now - task.issued >= config_.transaction_timeout) {
                            task.timed_out = true;
                            fail(task.channel, read ? reg::READ_DATA_TIMEOUT : reg::WRITE_RESPONSE_TIMEOUT);
                        }
                    }
                }
            }
            for (unsigned n = 0; n < channels_.size(); ++n) {
                auto& ch = channels_[n];
                if (!ch.active || ch.fault) continue;
                if (config_.watchdog_timeout != sc_core::SC_ZERO_TIME && ch.enabled &&
                    now - ch.progress >= config_.watchdog_timeout) { fail(n, reg::WATCHDOG_TIMEOUT); continue; }
                if (!ch.need_command && !ch.fetching_command && !ch.read_remaining && !ch.write_remaining &&
                    !ch.read_pending && !ch.write_pending && ch.fifo.empty() && ch.swap_pending.empty()) finish_command(n);
            }
            issue(true); issue(false);
        }
        outputs_.notify(sc_core::SC_ZERO_TIME);
        if (idle() && !clear_rx_ && !clear_tx_) sc_core::wait(kick_);
        else sc_core::wait(config_.cycle, kick_);
    }
}
} // namespace fx1::dma
