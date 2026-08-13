// SPDX-License-Identifier: Apache-2.0

#include "platform_top.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "riscv_vp_plusplus_wrapper.h"

#include "compiler_vp/host_io_map.h"

namespace cdc::platforms::riscv_vpp_compiler_vp {

namespace {

std::string hex(std::uint64_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

/// Common payload rules, applied once at the decoder.
///
/// `streaming_width == 0` is accepted as "the initiator did not set it". TLM
/// requires a compliant initiator to set it to at least the data length, and
/// VP++'s combined memory interface never touches the field — its payload is a
/// reused member left at the default. Refusing that would refuse every access
/// this platform exists to serve, so the check is written to catch the case
/// that actually indicates a wrapped transfer: a *non-zero* width smaller than
/// the data.
///
/// Byte enables are refused outright rather than honoured. Nothing in this
/// platform issues them, so an arriving one means an initiator this model has
/// never been tested against, and silently ignoring the mask would corrupt
/// memory in a way that looks like a program defect. It is refused with a
/// protocol status, not an address error, so VP++ aborts the model instead of
/// handing the guest a plausible access fault (decision record D13).
bool check_payload(tlm::tlm_generic_payload& payload)
{
    if (payload.get_byte_enable_ptr() != nullptr) {
        payload.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return false;
    }
    const unsigned width = payload.get_streaming_width();
    if (width != 0 && width < payload.get_data_length()) {
        payload.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (payload.get_command() != tlm::TLM_READ_COMMAND
        && payload.get_command() != tlm::TLM_WRITE_COMMAND) {
        payload.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    return true;
}

} // namespace

// ── RAM ──────────────────────────────────────────────────────────────────────

ram_target::ram_target(sc_core::sc_module_name name, std::uint64_t base,
                       std::uint64_t size)
    : sc_core::sc_module(name)
    , tsock("tsock")
    , base_(base)
    , storage_(static_cast<std::size_t>(size), 0)
{
    tsock.register_b_transport(this, &ram_target::b_transport);
    tsock.register_transport_dbg(this, &ram_target::transport_dbg);
}

bool ram_target::access(tlm::tlm_generic_payload& payload)
{
    const std::uint64_t address = payload.get_address();
    const std::uint64_t length = payload.get_data_length();

    if (address < base_) {
        return false;
    }
    const std::uint64_t offset = address - base_;
    if (offset > storage_.size() || length > storage_.size() - offset) {
        return false;
    }
    if (payload.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(storage_.data() + offset, payload.get_data_ptr(),
                    static_cast<std::size_t>(length));
    } else {
        std::memcpy(payload.get_data_ptr(), storage_.data() + offset,
                    static_cast<std::size_t>(length));
    }
    return true;
}

void ram_target::b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time&)
{
    payload.set_response_status(access(payload) ? tlm::TLM_OK_RESPONSE
                                                : tlm::TLM_ADDRESS_ERROR_RESPONSE);
}

unsigned int ram_target::transport_dbg(tlm::tlm_generic_payload& payload)
{
    return access(payload) ? payload.get_data_length() : 0;
}

bool ram_target::backdoor_read(std::uint64_t address, unsigned char* buffer,
                               std::uint64_t length) const
{
    if (address < base_) {
        return false;
    }
    const std::uint64_t offset = address - base_;
    if (offset > storage_.size() || length > storage_.size() - offset) {
        return false;
    }
    std::memcpy(buffer, storage_.data() + offset, static_cast<std::size_t>(length));
    return true;
}

// ── host I/O ─────────────────────────────────────────────────────────────────

host_io_target::host_io_target(sc_core::sc_module_name name, std::uint64_t base,
                               std::uint64_t size, const identity& id)
    : sc_core::sc_module(name)
    , tsock("tsock")
    , base_(base)
    , size_(size)
    , id_(id)
{
    tsock.register_b_transport(this, &host_io_target::b_transport);
    tsock.register_transport_dbg(this, &host_io_target::transport_dbg);
}

void host_io_target::flush()
{
    if (!line_.empty()) {
        std::cout << line_ << std::flush;
        line_.clear();
    }
}

std::uint32_t host_io_target::read_register(std::uint64_t offset)
{
    switch (offset) {
    case COMPILER_VP_ID_IDENTITY - COMPILER_VP_HOSTIO_BASE:
        return COMPILER_VP_IDENTITY_VALUE;
    case COMPILER_VP_ID_ABI_VERSION - COMPILER_VP_HOSTIO_BASE:
        return COMPILER_VP_ABI_VERSION;
    case COMPILER_VP_ID_XLEN - COMPILER_VP_HOSTIO_BASE:
        return id_.xlen;
    case COMPILER_VP_ID_HART_COUNT - COMPILER_VP_HOSTIO_BASE:
        return id_.hart_count;
    case COMPILER_VP_ID_HART_ID - COMPILER_VP_HOSTIO_BASE:
        return id_.hart_id;
    case COMPILER_VP_ID_VLEN_BITS - COMPILER_VP_HOSTIO_BASE:
        return id_.vlen_bits;
    case COMPILER_VP_ID_ELEN_BITS - COMPILER_VP_HOSTIO_BASE:
        return id_.elen_bits;
    case COMPILER_VP_ID_VLENB - COMPILER_VP_HOSTIO_BASE:
        return id_.vlenb;
    case COMPILER_VP_ID_RAM_BASE - COMPILER_VP_HOSTIO_BASE:
        return id_.ram_base;
    case COMPILER_VP_ID_RAM_SIZE - COMPILER_VP_HOSTIO_BASE:
        return id_.ram_size;
    case COMPILER_VP_ID_HOSTIO_BASE - COMPILER_VP_HOSTIO_BASE:
        return id_.hostio_base;
    case COMPILER_VP_ID_HOSTIO_SIZE - COMPILER_VP_HOSTIO_BASE:
        return id_.hostio_size;
    default:
        // Counted on the read side too. The write side already did, and a
        // counter that means "writes to undefined registers, plus reads of the
        // ones above 0x400, but not reads below it" is a number nobody can use.
        if (offset >= 0x400) {
            ++undefined_register_accesses;
        }
        return 0;
    }
}

void host_io_target::write_register(std::uint64_t offset, std::uint32_t value)
{
    switch (offset) {
    case COMPILER_VP_CONSOLE_DATA - COMPILER_VP_HOSTIO_BASE: {
        const char c = static_cast<char>(value & 0xffu);
        ++console_bytes;
        line_.push_back(c);
        // Buffered by line so guest output cannot interleave mid-line with the
        // platform's own messages. `\r` is treated as a terminator too: an
        // image printing a progress line would otherwise never flush.
        if (c == '\n' || c == '\r' || line_.size() >= 4096) {
            std::cout << line_ << std::flush;
            line_.clear();
        }
        return;
    }
    case COMPILER_VP_CONSOLE_FLUSH - COMPILER_VP_HOSTIO_BASE:
        flush();
        return;

    case COMPILER_VP_EXIT_STATUS - COMPILER_VP_HOSTIO_BASE:
        exit_status = value;
        return;
    case COMPILER_VP_EXIT_MCAUSE - COMPILER_VP_HOSTIO_BASE:
        exit_mcause = value;
        return;
    case COMPILER_VP_EXIT_MEPC - COMPILER_VP_HOSTIO_BASE:
        exit_mepc = value;
        return;

    case COMPILER_VP_EXIT_KIND - COMPILER_VP_HOSTIO_BASE:
        // The trigger. Status, cause and `mepc` are already in place.
        exit_kind = value;
        exited = true;
        flush();
        sc_core::sc_stop();
        return;

    case COMPILER_VP_TRACE_MARK - COMPILER_VP_HOSTIO_BASE:
        if (on_mark) {
            on_mark(value);
        }
        return;
    case COMPILER_VP_EXPECT_ACCESSES - COMPILER_VP_HOSTIO_BASE:
        if (on_expect) {
            on_expect(value);
        }
        return;

    default:
        if (offset < 0x400) {
            // The rest of the Phase 2 block. Ignored on purpose: an image built
            // for that contract should fail its own checks rather than take a
            // trap here, which would hide which check it was.
            return;
        }
        ++undefined_register_accesses;
        return;
    }
}

void host_io_target::b_transport(tlm::tlm_generic_payload& payload,
                                 sc_core::sc_time&)
{
    const std::uint64_t address = payload.get_address();
    const unsigned length = payload.get_data_length();

    if (address < base_ || address - base_ >= size_) {
        payload.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    // Word access only. A byte or halfword store to a control register is
    // almost always a mistake in the image, and answering it as though the
    // register were memory would leave three quarters of a value behind.
    if (length != 4 || ((address - base_) % 4) != 0) {
        payload.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const std::uint64_t offset = address - base_;
    if (payload.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, payload.get_data_ptr(), sizeof(value));
        write_register(offset, value);
    } else {
        const std::uint32_t value = read_register(offset);
        std::memcpy(payload.get_data_ptr(), &value, sizeof(value));
    }
    payload.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int host_io_target::transport_dbg(tlm::tlm_generic_payload& payload)
{
    // Deliberately refuses. `transport_dbg` is how the ELF loader writes an
    // image, and a segment landing here would be silently discarded and the
    // console left unreachable. `read_and_validate_elf` rejects such an image
    // with a diagnostic before anything is loaded; this is the backstop for a
    // path that skipped it.
    (void)payload;
    return 0;
}

// ── the decoder ──────────────────────────────────────────────────────────────

bus_decoder::bus_decoder(sc_core::sc_module_name name,
                         const platform_config& config, const elf_image& image)
    : sc_core::sc_module(name)
    , tsock("tsock")
    , ram_socket("ram_socket")
    , host_io_socket("host_io_socket")
    , config_(config)
    , image_(image)
{
    tsock.register_b_transport(this, &bus_decoder::b_transport);
    tsock.register_transport_dbg(this, &bus_decoder::transport_dbg);
    tsock.register_get_direct_mem_ptr(this, &bus_decoder::get_direct_mem_ptr);
}

bool bus_decoder::get_direct_mem_ptr(tlm::tlm_generic_payload& payload,
                                     tlm::tlm_dmi& dmi)
{
    ++dmi_requests;
    dmi.allow_none();
    dmi.set_start_address(0);
    dmi.set_end_address(static_cast<sc_dt::uint64>(~0ull));
    (void)payload;
    return false;
}

void bus_decoder::b_transport(tlm::tlm_generic_payload& payload,
                              sc_core::sc_time& delay)
{
    if (!check_payload(payload)) {
        return;
    }

    const std::uint64_t address = payload.get_address();
    const std::uint64_t length = payload.get_data_length();
    const bool write = payload.get_command() == tlm::TLM_WRITE_COMMAND;

    if (config_.ram.contains(address, length)
        || config_.host_io.contains(address, length)) {
        consecutive_unmapped = 0;
    }

    if (config_.ram.contains(address, length)) {
        // Classified by address, not by anything the payload says; see the
        // header comment for exactly what that does and does not prove.
        if (!write && image_.in_executable_segment(address)) {
            fetch.record(write, length);
        } else {
            data.record(write, length);
        }
        if (traced_ < config_.trace_limit) {
            ++traced_;
            std::cerr << "[trace] " << sc_core::sc_time_stamp() << ' '
                      << (write ? "W" : "R") << ' ' << hex(address) << " +"
                      << length << (image_.in_executable_segment(address)
                                        ? "  (executable segment)"
                                        : "")
                      << '\n';
        }
        ram_socket->b_transport(payload, delay);
        return;
    }

    if (config_.host_io.contains(address, length)) {
        host_io.record(write, length);
        if (traced_ < config_.trace_limit) {
            ++traced_;
            std::cerr << "[trace] " << sc_core::sc_time_stamp() << ' '
                      << (write ? "W" : "R") << ' ' << hex(address) << " +"
                      << length << "  (host I/O)\n";
        }
        host_io_socket->b_transport(payload, delay);
        return;
    }

    ++unmapped;
    ++consecutive_unmapped;
    if (traced_ < config_.trace_limit) {
        ++traced_;
        std::cerr << "[trace] " << sc_core::sc_time_stamp() << ' '
                  << (write ? "W" : "R") << ' ' << hex(address) << " +" << length
                  << "  (unmapped, refused)\n";
    }
    payload.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);

    // Thrown, not reported, because reporting needs the kernel and the kernel
    // is not going to run again. See `guest_fault_loop`.
    if (consecutive_unmapped >= fault_loop_limit) {
        std::ostringstream message;
        message << "the guest has taken " << consecutive_unmapped
                << " consecutive faulting accesses with no successful one "
                   "between them, the last at "
                << hex(address)
                << ". It is in a fault loop: it retires no instructions, so the "
                   "instruction watchdog cannot advance, and it never reaches a "
                   "quantum boundary, so simulated time cannot advance either. "
                   "The usual cause is an entry point or a trap vector pointing "
                   "at memory the image never wrote.";
        fault_loop_detected = true;
        fault_loop_message = message.str();
        throw guest_fault_loop(fault_loop_message);
    }
}

unsigned int bus_decoder::transport_dbg(tlm::tlm_generic_payload& payload)
{
    // Not counted. The only user is the ELF loader, and the measurement window
    // is expressed in accesses the *running program* made; folding the image
    // load into it would let a large image satisfy a vector loop's expectation.
    const std::uint64_t address = payload.get_address();
    const std::uint64_t length = payload.get_data_length();

    if (config_.ram.contains(address, length)) {
        return ram_socket->transport_dbg(payload);
    }
    if (config_.host_io.contains(address, length)) {
        return host_io_socket->transport_dbg(payload);
    }
    return 0;
}

void bus_decoder::mark(std::uint32_t id)
{
    if (id != 0) {
        mark_id_ = id;
        mark_data_at_open_ = data.transactions;
        mark_expect_ = 0;
        return;
    }

    if (mark_id_ == 0) {
        return; // closing a window that was never opened; nothing to check
    }

    const std::uint64_t observed = data.transactions - mark_data_at_open_;
    if (mark_expect_ != 0 && observed < mark_expect_) {
        window_violated = true;
        std::ostringstream message;
        message << "measurement window " << mark_id_ << " saw " << observed
                << " RAM data accesses, but the image declared it would need at "
                   "least "
                << mark_expect_
                << ". The traffic the program issued did not reach the bus: "
                   "either the model serviced it from a direct pointer or a "
                   "cache, or the accesses were coalesced. Either way this "
                   "platform can no longer claim that every access is observable "
                   "on TLM.";
        window_message = message.str();
        std::cerr << "riscv_vpp_compiler_vp: " << window_message << '\n';
        sc_core::sc_stop();
    }
    mark_id_ = 0;
    mark_expect_ = 0;
}

void bus_decoder::expect(std::uint32_t accesses)
{
    mark_expect_ = accesses;
}

// ── the top level ────────────────────────────────────────────────────────────

compiler_vp_top::compiler_vp_top(sc_core::sc_module_name name,
                                 const platform_config& config,
                                 const elf_image& image)
    : sc_core::sc_module(name)
    , config_(config)
    , image_(image)
    , ram_("ram", config.ram.base, config.ram.size)
    , host_io_("host_io", config.host_io.base, config.host_io.size,
               [&] {
                   host_io_target::identity id;
                   id.xlen = 32;
                   id.hart_count = 1;
                   id.hart_id = config.hart_id;
                   id.vlen_bits = cdc::cpu::riscv_vp_plusplus_cpu::vlen_bits();
                   id.elen_bits = cdc::cpu::riscv_vp_plusplus_cpu::elen_bits();
                   // `vlenb` is filled in below, from the constructed hart's own
                   // CSR rather than from VLEN/8: the point of the register is
                   // to let firmware compare the host's view against the hart's,
                   // and computing it here from the same constant would compare
                   // a value with itself.
                   id.vlenb = 0;
                   id.ram_base = static_cast<std::uint32_t>(config.ram.base);
                   id.ram_size = static_cast<std::uint32_t>(config.ram.size);
                   id.hostio_base =
                       static_cast<std::uint32_t>(config.host_io.base);
                   id.hostio_size =
                       static_cast<std::uint32_t>(config.host_io.size);
                   return id;
               }())
    , bus_("bus", config_, image_)
{
    cdc::cpu::cpu_config cpu_config;
    cpu_config.xlen = 32;
    cpu_config.hart_id = config.hart_id;
    cpu_ = std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>("hart", cpu_config);

    host_io_.set_vlenb(cpu_->vlenb());

    // One socket: the backend reports a unified bus, so instruction fetch and
    // data share it. Binding `instr_bus()` as well would bind the same socket
    // twice.
    cpu_->data_bus().bind(bus_.tsock);
    bus_.ram_socket.bind(ram_.tsock);
    bus_.host_io_socket.bind(host_io_.tsock);

    host_io_.on_mark = [this](std::uint32_t id) { bus_.mark(id); };
    host_io_.on_expect = [this](std::uint32_t n) { bus_.expect(n); };

    cpu_->load_elf(image_.path);
}

compiler_vp_top::~compiler_vp_top() = default;

bool compiler_vp_top::debug_read(std::uint64_t address, unsigned char* buffer,
                                 std::uint64_t length)
{
    // Straight to the storage, deliberately off the bus: a signature dump is a
    // host facility, and routing it through the decoder would add transactions
    // to the counters the run is being judged on.
    return ram_.backdoor_read(address, buffer, length);
}

std::string compiler_vp_top::traffic_report() const
{
    std::ostringstream out;
    auto line = [&out](const char* label, const access_counters& c) {
        out << "  " << std::left << std::setw(22) << label << std::right
            << std::setw(12) << c.transactions << " transactions ("
            << c.reads << " read, " << c.writes << " write, " << c.bytes
            << " bytes)\n";
    };

    out << "TLM traffic (every access; no DMI, no ISS cache)\n";
    line("executable-segment R", bus_.fetch);
    line("RAM data", bus_.data);
    line("host I/O", bus_.host_io);
    out << "  " << std::left << std::setw(22) << "unmapped, refused"
        << std::right << std::setw(12) << bus_.unmapped << '\n'
        << "  " << std::left << std::setw(22) << "DMI requests (refused)"
        << std::right << std::setw(12) << bus_.dmi_requests << '\n';
    if (host_io_.undefined_register_accesses != 0) {
        out << "  " << std::left << std::setw(22) << "undefined host-I/O reg"
            << std::right << std::setw(12)
            << host_io_.undefined_register_accesses << '\n';
    }
    return out.str();
}

} // namespace cdc::platforms::riscv_vpp_compiler_vp
