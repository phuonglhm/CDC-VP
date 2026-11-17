#include "isp_stream_bridge.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <debug/sha256.h>  // fx1_isp_core (header only): the SHA-256 the ISP tools use
#include <fx1/sim_control.h>

#include "ddr3_subsystem.h"

namespace cdc::platforms::fx1_soc {
namespace {

volatile std::sig_atomic_t g_user_stops = 0;

constexpr std::uint32_t kAckOffset = 12;        // fx1_isp_stream_record.ack_seq
constexpr std::uint32_t kSessionHiOffset = 20;  // fx1_isp_stream_record.session_hi

std::uint64_t u64(std::uint32_t lo, std::uint32_t hi) { return (std::uint64_t{hi} << 32) | lo; }

std::string hex32(std::uint32_t v)
{
    std::ostringstream os;
    os << "0x" << std::hex << v;
    return os.str();
}

const char* state_name(std::uint32_t s)
{
    static const char* const names[] = {"IDLE", "PREPARED", "RUNNING", "DRAINING", "STOPPED", "ERROR"};
    return s < 6 ? names[s] : "UNKNOWN";
}

const char* stop_name(std::uint32_t s)
{
    static const char* const names[] = {"NONE", "DONE", "USER", "LIMIT", "ERROR"};
    return s < 5 ? names[s] : "UNKNOWN";
}

const char* error_name(std::uint32_t e)
{
    static const char* const names[] = {"NONE", "DESC", "BUSY", "PROFILE", "CONFIG", "COPY", "ISP", "TIMEOUT", "HOST"};
    return e < 9 ? names[e] : "UNKNOWN";
}

const char* reason_name(std::uint32_t r)
{
    return r == FX1_ISP_STREAM_REASON_USER ? "USER" : r == FX1_ISP_STREAM_REASON_LIMIT ? "LIMIT" : "HOST";
}

std::string json_string(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out + "\"";
}

}  // namespace

void IspStreamBridge::request_user_stop() noexcept
{
    if (g_user_stops < 1000) g_user_stops = g_user_stops + 1;
}

unsigned IspStreamBridge::user_stop_requests() noexcept { return static_cast<unsigned>(g_user_stops); }

IspStreamBridge::IspStreamBridge(sc_core::sc_module_name name, Ddr3Subsystem& ddr, fx1::SimControl& sim,
                                 const Options& options)
    : sc_module(name), ddr_(ddr), sim_(sim), options_(options)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir(options_.dir);
    if (options_.dir.empty()) throw std::invalid_argument("--isp-stream-spool: empty directory name");
    if (fs::exists(dir, ec)) {
        if (!fs::is_directory(dir, ec)) throw std::invalid_argument(options_.dir + " is not a directory");
        if (!fs::is_empty(dir, ec))
            throw std::invalid_argument(options_.dir + " is not empty (nothing is removed: give a new or empty directory)");
    } else if (!fs::create_directories(dir, ec)) {
        throw std::invalid_argument("cannot create " + options_.dir + ": " + ec.message());
    }
    const auto open = [&](const char* leaf) {
        const std::string path = (dir / leaf).string();
        const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (fd < 0) throw std::invalid_argument("cannot create " + path + ": " + std::strerror(errno));
        return fd;
    };
    nv12_fd_ = open("frames.nv12");
    jsonl_fd_ = open("frames.jsonl");
    sim_.enable_doorbell(true);
    SC_THREAD(run);
}

IspStreamBridge::~IspStreamBridge()
{
    if (nv12_fd_ >= 0) ::close(nv12_fd_);
    if (jsonl_fd_ >= 0) ::close(jsonl_fd_);
}

template <class T> T IspStreamBridge::read_block(std::uint64_t address) const
{
    T block;
    ddr_.dump(address, reinterpret_cast<std::uint8_t*>(&block), sizeof block);  // LE host, LE words
    return block;
}

void IspStreamBridge::write_u32(std::uint64_t address, std::uint32_t value)
{
    std::uint8_t b[4];
    for (unsigned i = 0; i < 4; ++i) b[i] = static_cast<std::uint8_t>(value >> (8 * i));
    ddr_.poke(address, b, sizeof b);
}

void IspStreamBridge::run()
{
    if (!ddr_.released()) wait(ddr_.release_event());
    // Before the CPUs run a cycle: attach the session the host prepared (the
    // firmware refuses to start one the bridge has not attached).
    if (read_desc()) attach();
    for (;;) {
        // The doorbell, or the fallback poll: a ring may arrive while no record is new
        // and several rings may coalesce; the records decide (drain by seq).
        wait(sc_core::sc_time(1, sc_core::SC_MS), sim_.doorbell_event());
        ++wakes_;
        forward_requests();
        drain();
    }
}

bool IspStreamBridge::read_desc()
{
    const auto d = read_block<fx1_isp_stream_desc>(FX1_ISP_STREAM_DESC_ADDR);
    if (d.magic != FX1_ISP_STREAM_DESC_MAGIC || d.version != FX1_ISP_STREAM_VERSION || d.size != sizeof d ||
        d.repeat == 0 || d.source_count == 0)
        return false;
    desc_ = d;
    session_ = u64(d.session_lo, d.session_hi);
    have_desc_ = true;
    return true;
}

void IspStreamBridge::write_control(std::uint32_t request, std::uint32_t reason)
{
    fx1_isp_stream_control c{};
    c.magic = FX1_ISP_STREAM_CONTROL_MAGIC;
    c.version = FX1_ISP_STREAM_VERSION;
    c.request = request;
    c.reason = reason;
    c.session_lo = desc_.session_lo;
    c.session_hi = desc_.session_hi;
    c.max_bytes_lo = static_cast<std::uint32_t>(options_.max_spool_bytes);
    c.max_bytes_hi = static_cast<std::uint32_t>(options_.max_spool_bytes >> 32);
    ddr_.poke(FX1_ISP_STREAM_CONTROL_ADDR, reinterpret_cast<const std::uint8_t*>(&c), sizeof c);  // one debug write
}

void IspStreamBridge::attach()
{
    write_control(FX1_ISP_STREAM_REQ_NONE, 0);
    attached_ = true;
}

void IspStreamBridge::request_stop(std::uint32_t reason, const char* why)
{
    if (stop_sent_ || !attached_) return;
    write_control(FX1_ISP_STREAM_REQ_STOP, reason);
    stop_sent_ = reason;
    std::cout << "[isp-stream] stop requested (" << reason_name(reason) << "): " << why << " at "
              << sc_core::sc_time_stamp() << std::endl;
}

void IspStreamBridge::fail(const std::string& what)
{
    errors_.push_back(what);
    std::cout << "[isp-stream] ERROR " << what << " (no further ACK)" << std::endl;
    failed_ = true;
    if (!stop_sent_) request_stop(FX1_ISP_STREAM_REASON_HOST, "host bridge error");
}

// A pending Ctrl+C becomes STOP, USER. Called on every wake and between frames of a
// drain (a held host may drain without returning while the firmware keeps publishing).
// The spool budget needs no request: the firmware admits only the frames it holds.
void IspStreamBridge::forward_requests()
{
    if (attached_ && !stop_sent_ && user_stop_requests()) request_stop(FX1_ISP_STREAM_REASON_USER, "Ctrl+C");
}

void IspStreamBridge::drain()
{
    if (failed_ || !attached_) return;  // no session in DDR at release: nothing to capture
    for (;;) {
        forward_requests();
        std::uint64_t pending = 0;  // frames of this session waiting at once (coalesced rings)
        int found = -1, ahead = -1;
        std::uint32_t ahead_seq = 0;
        fx1_isp_stream_record found_rec{};
        for (unsigned i = 0; i < FX1_ISP_STREAM_SLOTS; ++i) {
            const auto r = read_block<fx1_isp_stream_record>(FX1_ISP_STREAM_RECORD_ADDR + i * FX1_ISP_STREAM_RECORD_BYTES);
            if (r.state != FX1_ISP_STREAM_REC_PUBLISHED) continue;
            if (r.magic != FX1_ISP_STREAM_RECORD_MAGIC || u64(r.session_lo, r.session_hi) != session_) {
                ++stale_;  // not this session's (left over, or before the firmware initialized it): never taken
                continue;
            }
            if (r.seq >= next_seq_) ++pending;
            if (r.seq == next_seq_) {
                if (found >= 0) {
                    fail("records " + std::to_string(found) + " and " + std::to_string(i) + " both publish seq " +
                         std::to_string(r.seq));
                    return;
                }
                found = static_cast<int>(i);
                found_rec = r;
            } else if (r.seq < next_seq_ && r.ack_seq != r.seq) {
                fail("record " + std::to_string(i) + ": seq " + std::to_string(r.seq) +
                     " published again after the bridge took it (ack_seq " + std::to_string(r.ack_seq) + ")");
                return;
            } else if (r.seq > next_seq_ && (ahead < 0 || r.seq < ahead_seq)) {
                ahead = static_cast<int>(i);
                ahead_seq = r.seq;
            }
        }
        max_batch_ = std::max(max_batch_, pending);
        if (found < 0) {
            // Later frames may be published together with seq == next (queue > 1); a later
            // one without it is a gap.
            if (ahead >= 0)
                fail("record " + std::to_string(ahead) + ": seq " + std::to_string(ahead_seq) +
                     " published while seq " + std::to_string(next_seq_) + " is not (a sequence gap)");
            return;
        }
        if (options_.hold_us) {
            // A slow host: the frame stays held; the record is read again afterwards.
            wait(sc_core::sc_time(options_.hold_us, sc_core::SC_US));
            forward_requests();
            found_rec = read_block<fx1_isp_stream_record>(FX1_ISP_STREAM_RECORD_ADDR +
                                                          static_cast<unsigned>(found) * FX1_ISP_STREAM_RECORD_BYTES);
            if (found_rec.state != FX1_ISP_STREAM_REC_PUBLISHED || found_rec.seq != next_seq_) {
                fail("record " + std::to_string(found) + ": seq " + std::to_string(next_seq_) +
                     " withdrawn or replaced while the host held it (state " + std::to_string(found_rec.state) +
                     ", seq " + std::to_string(found_rec.seq) + ")");
                return;
            }
        }
        if (!capture(static_cast<unsigned>(found), found_rec)) return;
    }
}

bool IspStreamBridge::capture(unsigned index, const fx1_isp_stream_record& r)
{
    const std::uint64_t ordinal = u64(r.ordinal_lo, r.ordinal_hi);
    const std::string where = "record " + std::to_string(index) + " seq " + std::to_string(r.seq);
    const std::uint64_t slot_base = FX1_ISP_STREAM_OUT_ADDR(index);
    if (ordinal != std::uint64_t{next_seq_} - 1) {
        fail(where + ": ordinal " + std::to_string(ordinal) + ", expected " + std::to_string(next_seq_ - 1));
        return false;
    }
    if (r.slot != index || r.status != 0) {
        fail(where + ": slot " + std::to_string(r.slot) + " status " + std::to_string(r.status));
        return false;
    }
    const std::uint64_t k = (ordinal / desc_.repeat) % desc_.source_count, rep = ordinal % desc_.repeat;
    if (r.source_index != k || r.repeat_index != rep) {
        fail(where + ": identity source " + std::to_string(r.source_index) + " repeat " +
             std::to_string(r.repeat_index) + ", the schedule gives " + std::to_string(k) + " / " + std::to_string(rep));
        return false;
    }
    // UV sits at the first 1 MiB boundary after the largest Y the session can write: the
    // descriptor's stride times the active height (the resizer only reduces), as the
    // firmware programs it before the output geometry is known.
    const std::uint32_t active_h = (desc_.height - ((desc_.bayer >> 1) & 1u)) & ~1u;
    const std::uint64_t y_max = std::uint64_t{desc_.out_stride} * active_h;
    if (!r.width || !r.height || (r.width & 1u) || (r.height & 1u) || r.width > r.stride ||
        r.stride != desc_.out_stride || r.height > active_h || r.y_addr != slot_base ||
        r.uv_addr != slot_base + FX1_ISP_STREAM_UV_OFFSET(y_max) ||
        r.uv_addr + std::uint64_t{r.stride} * r.height / 2 > slot_base + FX1_ISP_STREAM_SLOT_BYTES) {
        fail(where + ": geometry " + std::to_string(r.width) + "x" + std::to_string(r.height) + " stride " +
             std::to_string(r.stride) + ", Y " + hex32(r.y_addr) + " UV " + hex32(r.uv_addr) +
             " outside output slot " + std::to_string(index));
        return false;
    }
    // The active bytes, Y rows then UV rows, no row padding.
    std::vector<std::uint8_t> frame(std::size_t{r.width} * r.height * 3 / 2);
    try {
        for (std::uint32_t y = 0; y < r.height; ++y)
            ddr_.dump(r.y_addr + std::uint64_t{y} * r.stride, frame.data() + std::size_t{y} * r.width, r.width);
        std::uint8_t* uv = frame.data() + std::size_t{r.width} * r.height;
        for (std::uint32_t y = 0; y < r.height / 2; ++y)
            ddr_.dump(r.uv_addr + std::uint64_t{y} * r.stride, uv + std::size_t{y} * r.width, r.width);
    } catch (const std::exception& e) {
        fail(where + ": " + e.what());
        return false;
    }
    cdc::components::fx1_isp::debug::sha256 h;
    h.update(frame.data(), frame.size());
    const std::string digest = h.hex();
    std::ostringstream line;
    line << "{\"ordinal\": " << ordinal << ", \"seq\": " << r.seq << ", \"source_index\": " << r.source_index
         << ", \"repeat_index\": " << r.repeat_index << ", \"driver_seq\": " << r.driver_seq << ", \"tag\": " << r.tag
         << ", \"frame_id\": " << r.frame_id << ", \"profile_id\": " << r.profile_id << ", \"ts\": " << r.ts
         << ", \"sof_ts\": " << r.sof_ts << ", \"eof_ts\": " << r.eof_ts << ", \"copy_ts\": " << r.copy_ts << ", \"slot\": " << r.slot
         << ", \"width\": " << r.width << ", \"height\": " << r.height << ", \"stride\": " << r.stride
         << ", \"colorimetry\": " << r.colorimetry << ", \"offset\": " << spooled_bytes_ << ", \"bytes\": "
         << frame.size() << ", \"sha256\": " << json_string(digest) << ", \"sim_time_ns\": "
         << static_cast<std::uint64_t>(sc_core::sc_time_stamp().to_seconds() * 1e9) << "}\n";
    const std::string text = line.str();
    // The firmware admits only what the budget holds; never write past it regardless.
    if (options_.max_spool_bytes && spooled_bytes_ + frame.size() > options_.max_spool_bytes) {
        fail(where + ": " + std::to_string(frame.size()) + " bytes would exceed the spool budget (" +
             std::to_string(options_.max_spool_bytes - spooled_bytes_) + " left of " +
             std::to_string(options_.max_spool_bytes) + ")");
        return false;
    }
    if (!append(nv12_fd_, frame.data(), frame.size(), "frames.nv12") ||
        !append(jsonl_fd_, text.data(), text.size(), "frames.jsonl"))
        return false;
    if (::fsync(nv12_fd_) != 0 || ::fsync(jsonl_fd_) != 0) {
        fail(std::string("fsync of the spool: ") + std::strerror(errno));
        return false;
    }
    // Stored: only now the firmware may reuse the slot.
    const std::uint64_t record = FX1_ISP_STREAM_RECORD_ADDR + std::uint64_t{index} * FX1_ISP_STREAM_RECORD_BYTES;
    if (options_.fault == Options::Fault::foreign_ack && !fault_done_) {
        // Fault injection: the ACK lands in a record that no longer names this
        // session (as a host confusing sessions would); the firmware must not take it.
        write_u32(record + kSessionHiOffset, r.session_hi ^ 1u);
        fault_done_ = true;
        std::cout << "[isp-stream] fault injected: foreign-ack on record " << index << " seq " << r.seq << std::endl;
    }
    write_u32(record + kAckOffset, r.seq);
    spooled_bytes_ += frame.size();
    ++captured_;
    ++next_seq_;
    return true;
}

bool IspStreamBridge::append(int fd, const void* data, std::size_t size, const char* what)
{
    const auto* p = static_cast<const char*>(data);
    while (size) {
        const ssize_t n = ::write(fd, p, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            fail(std::string("write of ") + what + ": " + (n < 0 ? std::strerror(errno) : "no progress"));
            return false;
        }
        p += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

void IspStreamBridge::finish()
{
    fx1_isp_stream_status s{};
    bool status_ok = false;
    try {
        s = read_block<fx1_isp_stream_status>(FX1_ISP_STREAM_STATUS_ADDR);
        status_ok = s.magic == FX1_ISP_STREAM_STATUS_MAGIC && have_desc_ && u64(s.session_lo, s.session_hi) == session_;
    } catch (const std::exception&) {
    }
    if (!status_ok) {
        final_state_ = "no status block of this session";
    } else {
        final_state_ = std::string(state_name(s.state)) + " (" +
                       (s.state == FX1_ISP_STREAM_ERROR ? std::string("ERROR ") + error_name(s.error) + " detail " +
                                                              hex32(s.detail)
                                                        : std::string(stop_name(s.stop_reason))) +
                       ")";
        const std::uint64_t completed = u64(s.completed_lo, s.completed_hi);
        if (!failed_ && completed != captured_)
            fail("the firmware published " + std::to_string(completed) + " frame(s), the bridge captured " +
                 std::to_string(captured_));
    }
    std::ostringstream j;
    j << "{\n  \"session_id\": \"" << std::hex;
    j.width(16);
    j.fill('0');
    j << session_ << std::dec << "\",\n  \"captured\": " << captured_ << ",\n  \"spooled_bytes\": " << spooled_bytes_
      << ",\n  \"doorbells\": " << sim_.doorbells() << ",\n  \"wakes\": " << wakes_
      << ",\n  \"max_batch\": " << max_batch_ << ",\n  \"stale_records_ignored\": " << stale_ << ",\n  \"hold_us\": " << options_.hold_us
      << ",\n  \"max_spool_bytes\": " << options_.max_spool_bytes << ",\n  \"attached\": " << (attached_ ? "true" : "false")
      << ",\n  \"fault\": " << (options_.fault == Options::Fault::foreign_ack ? "\"foreign-ack\"" : "null")
      << ",\n  \"stop_requested\": " << (stop_sent_ ? json_string(reason_name(stop_sent_)) : std::string("null"))
      << ",\n  \"user_stops\": " << user_stop_requests() << ",\n  \"firmware\": ";
    if (status_ok) {
        j << "{\"state\": " << json_string(state_name(s.state)) << ", \"stop_reason\": "
          << json_string(stop_name(s.stop_reason)) << ", \"error\": " << json_string(error_name(s.error))
          << ", \"detail\": " << s.detail << ", \"submitted\": " << u64(s.submitted_lo, s.submitted_hi)
          << ", \"completed\": " << u64(s.completed_lo, s.completed_hi) << ", \"cancelled\": " << s.cancelled
          << ", \"failed\": " << s.failed << ", \"out_width\": " << s.out_width << ", \"out_height\": " << s.out_height
          << ", \"out_stride\": " << s.out_stride << ", \"colorimetry\": " << s.colorimetry
          << ", \"dma_err\": " << s.dma_err << ", \"idma_frames\": " << s.idma_frames
          << ", \"odma_frames\": " << s.odma_frames << ", \"publish_seq\": " << s.publish_seq
          << ", \"elapsed_ticks\": " << s.elapsed_ticks << ", \"isp_drain\": "
          << json_string(s.isp_drain == FX1_ISP_STREAM_DRAIN_NONE      ? "NONE"
                         : s.isp_drain == FX1_ISP_STREAM_DRAIN_PENDING ? "PENDING"
                         : s.isp_drain == FX1_ISP_STREAM_DRAIN_DONE    ? "DONE"
                                                                       : "?")
          << "}";
    } else {
        j << "null";
    }
    j << ",\n  \"errors\": [";
    for (std::size_t i = 0; i < errors_.size(); ++i) j << (i ? ", " : "") << json_string(errors_[i]);
    j << "]\n}\n";
    const std::string path = (std::filesystem::path(options_.dir) / "status.json").string();
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    const std::string text = j.str();
    if (fd < 0 || ::write(fd, text.data(), text.size()) != static_cast<ssize_t>(text.size()) || ::fsync(fd) != 0)
        errors_.push_back("cannot write " + path);
    if (fd >= 0) ::close(fd);
}

std::string IspStreamBridge::report() const
{
    std::ostringstream os;
    os << captured_ << " frame(s) captured, " << spooled_bytes_ << " bytes in " << options_.dir
       << "/frames.nv12, firmware " << (final_state_.empty() ? std::string("?") : final_state_) << ", "
       << sim_.doorbells() << " doorbell(s)";
    if (stop_sent_) os << ", stop requested (" << reason_name(stop_sent_) << ")";
    if (!errors_.empty()) os << ", " << errors_.size() << " ERROR(S)";
    return os.str();
}

}  // namespace cdc::platforms::fx1_soc
