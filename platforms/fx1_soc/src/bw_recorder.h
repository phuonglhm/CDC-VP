// VP-only bandwidth-measurement recorder (fx1_soc --bw-report FILE; plan
// CSI_ISP_BW1, design reviewed 2026-11-27). Not a model of the SoC: it only
// listens. Every event is one JSON object per line ("fx1-bw/1"):
//
//   header      schema, build identity, time resolution, clocks, options
//   mark        START / STOP of a measurement window (SIM_CTRL BW_MARK), or an error
//   rx_frame    one CSI frame as the Packet Analyzer parsed it (FS..FE): payload
//               bytes as received, before every forward / discard gate
//   capture_*   CSI capture start / end (outcome) / stream boundary / source reset
//   axis_capture  AXI4-Stream totals of one capture, at its stream boundary
//   axis_window / rx_window  the same totals counted inside a window: handshakes
//               and payload bytes whose time lies in [effective START, effective
//               STOP); written once simulation time has passed the effective STOP
//               (or at the end of the run, `drained` 0)
//   dma         one DMA transaction: issue (ID, master, R/W, address, bytes) and
//               response (ID, ok), each at its effective time; responses may be
//               written before earlier-timed events: order by `t`, not by line
//   isp_admit / isp_drop  ingress admission of a capture (seq -> TAG) or not
//   output      one ODMA frame end: done / failed / aborted, first write issue,
//               last write response, DONE, plane geometry, optional NV12 dump
//   retire      an ISP frame retired (ok: every output DONE)
//   footer      complete, end time, reason, totals; a file without it is incomplete
//
// Times are integer ticks of the SystemC time resolution (`t`, `t_*`), stated
// in the header as time_resolution_fs; no unit is assumed. AXIS and payload
// are aggregated, never logged per beat. Nothing here waits, changes a
// payload, a TREADY or a callback of the models it observes.
#pragma once

#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <systemc>

#include "fx1/sim_control.h"
#include "fx1_isp/fx1_isp_debug.h"
#include "traffic_counter.h"

namespace cdc::platforms::fx1_soc {

class BwRecorder final : public DmaEventSink, public cdc::components::fx1_isp::output_observer {
public:
    // Reads DDR at a global address without bus traffic (backdoor), for the NV12 dump.
    using ddr_reader = std::function<void(std::uint64_t address, std::uint8_t* data, std::size_t length)>;
    static constexpr const char* kSchema = "fx1-bw/1";
    static constexpr const char* kMasters[3] = {"MIPI_DMA", "ISP_IDMA", "ISP_ODMA"};

    BwRecorder(const std::string& path, std::string dump_dir, ddr_reader read)
        : out_(path), dump_dir_(std::move(dump_dir)), read_(std::move(read))
    {
        if (!out_) throw std::runtime_error("fx1_soc: --bw-report: cannot open " + path);
    }

    // ---- JSON helpers -------------------------------------------------------
    static std::string q(const std::string& s)
    {
        std::string r = "\"";
        for (char c : s) {
            if (c == '"' || c == '\\') r += '\\';
            if (static_cast<unsigned char>(c) < 0x20) {
                r += ' ';
                continue;
            }
            r += c;
        }
        return r + '"';
    }
    static std::string hex(std::uint64_t v)
    {
        std::ostringstream o;
        o << "\"0x" << std::hex << v << '"';
        return o.str();
    }
    static std::uint64_t ticks(const sc_core::sc_time& t) { return t.value(); }
    static std::uint64_t now() { return ticks(sc_core::sc_time_stamp()); }

    // Header (once, before the simulation). `fields` are written as given:
    // the caller passes JSON values (q() for strings).
    void header(const std::vector<std::pair<std::string, std::string>>& fields)
    {
        const double res_s = sc_core::sc_get_time_resolution().to_seconds();
        std::ostringstream o;
        o << "{\"ev\":\"header\",\"schema\":" << q(kSchema)
          << ",\"time_resolution_fs\":" << static_cast<std::uint64_t>(std::llround(res_s * 1e15));
        for (const auto& [k, v] : fields) o << ',' << q(k) << ':' << v;
        o << '}';
        line(o.str());
    }

    // ---- window marker (SIM_CTRL BW_MARK) -----------------------------------
    void mark(const fx1::SimControl::BwMark& m)
    {
        using K = fx1::SimControl::BwMarkKind;
        std::ostringstream o;
        o << "{\"ev\":\"mark\",\"kind\":" << q(m.kind == K::start ? "start" : m.kind == K::stop ? "stop" : "error")
          << ",\"value\":" << hex(m.value) << ",\"window\":" << m.window << ",\"t\":" << ticks(m.when);
        if (m.kind == K::start) {
            o << ",\"mode\":" << q((m.value & 0xF0u) == 0x10u ? "steady" : "batch") << ",\"count\":" << (m.value >> 8);
        }
        o << '}';
        line(o.str());
        // A mark takes effect at m.when, which can lie after the current time
        // (an initiator's annotated delay): events are assigned to a window by
        // their own time, and a window is closed only once time has passed its
        // effective STOP (flush()), never at the callback. The data of the
        // current tick is still pending (commit()): a mark of this tick, before
        // or after the data's callback, applies to it the same way.
        commit_before(now());
        flush();
        if (m.kind == K::start) {
            window w;
            w.id = m.window;
            w.start = ticks(m.when);
            windows_.push_back(w);
        } else if (m.kind == K::stop) {
            for (auto& w : windows_)
                if (w.id == m.window && !w.stopped) {
                    w.stopped = true;
                    w.stop = ticks(m.when);
                }
        }
    }

    // ---- CSI-2 RX Packet Analyzer (received payload, before every gate) ------
    // cls: 0 image, 1 embedded, 2 null/blank, 3 unsupported DT, 4 unsupported VC.
    void rx_long_header(int cls, unsigned vc, unsigned dt, unsigned wc, bool forwarded)
    {
        pkt_cls_ = cls;
        pkt_vc_ = vc & 1u;
        pkt_fwd_ = forwarded;
        rx_frame* f = open_rx(pkt_vc_);
        if (f) {
            ++f->packets[cls];
            if (cls == 0) f->dt = dt;
            f->wc_sum += wc;
        } else {
            ++rx_outside_.packets[cls];
        }
        tick(now()).rx.push_back({-1, 0});  // a header, assigned to a window at commit()
    }
    void rx_payload_bytes(unsigned n)
    {
        rx_frame* f = open_rx(pkt_vc_);
        rx_frame& tgt = f ? *f : rx_outside_;
        tgt.bytes[pkt_cls_] += n;
        if (!pkt_fwd_) tgt.not_forwarded += n;
        if (f && f->t_first_byte == 0 && pkt_cls_ == 0) f->t_first_byte = now();
        if (f && pkt_cls_ == 0) f->t_last_byte = now();
        tick(now()).rx.push_back({pkt_cls_, n});
    }
    void rx_packet_end(bool truncated, bool crc_ok)
    {
        rx_frame* f = open_rx(pkt_vc_);
        rx_frame& tgt = f ? *f : rx_outside_;
        if (truncated) ++tgt.truncated;
        else if (!crc_ok) ++tgt.crc_bad;
    }
    void rx_unclassified()
    {
        bool any = false;
        for (auto& f : rx_open_)
            if (f.open) {
                ++f.unclassified;
                any = true;
            }
        if (!any) ++rx_outside_.unclassified;
    }
    void rx_frame_start(unsigned vc, unsigned fn)
    {
        rx_frame& f = rx_open_[vc & 1u];
        if (f.open) emit_rx(f, false);  // FS without FE
        f = rx_frame{};
        f.open = true;
        f.vc = vc;
        f.fn = fn;
        f.idx = ++rx_frames_;
        f.t_fs = now();
    }
    void rx_frame_end(unsigned vc, unsigned)
    {
        rx_frame& f = rx_open_[vc & 1u];
        if (!f.open) return;
        f.t_fe = now();
        emit_rx(f, true);
        f.open = false;
    }

    // ---- CSI capture and AXI4-Stream (CsiIspDirect) --------------------------
    void axis(bool tvalid, std::uint8_t tkeep, bool tlast, std::uint8_t tuser, std::uint64_t seq, bool tready)
    {
        if (!tvalid) return;
        const std::uint64_t t = now();
        axis_agg& c = axis_cap_[seq];
        count_axis(c, tkeep, tlast, tuser, tready, t);
        tick(t).axis.push_back({tkeep, tlast, tuser, tready});  // window membership at commit()
    }
    void capture_start(std::uint64_t seq, unsigned frame_id, std::uint64_t ts_ns, std::uint32_t epoch)
    {
        // The CSI frame the Packet Analyzer has open on VC0 is this capture's.
        rx_frame& f = rx_open_[0];
        if (f.open && f.seq == 0) f.seq = seq;
        std::ostringstream o;
        o << "{\"ev\":\"capture_start\",\"seq\":" << seq << ",\"frame_id\":" << frame_id << ",\"epoch\":" << epoch
          << ",\"fs_ts_ns\":" << ts_ns << ",\"t\":" << now() << '}';
        line(o.str());
    }
    void capture_end(std::uint64_t seq, const char* outcome)
    {
        std::ostringstream o;
        o << "{\"ev\":\"capture_end\",\"seq\":" << seq << ",\"outcome\":" << q(outcome) << ",\"t\":" << now() << '}';
        line(o.str());
    }
    void capture_stream_end(std::uint64_t seq)
    {
        std::ostringstream o;
        o << "{\"ev\":\"capture_stream_end\",\"seq\":" << seq << ",\"t\":" << now() << '}';
        line(o.str());
        const auto it = axis_cap_.find(seq);
        emit_axis("axis_capture", "seq", seq, it == axis_cap_.end() ? axis_agg{} : it->second);
        if (it != axis_cap_.end()) axis_cap_.erase(it);
    }
    void source_reset(std::uint32_t epoch)
    {
        std::ostringstream o;
        o << "{\"ev\":\"source_reset\",\"epoch\":" << epoch << ",\"t\":" << now() << '}';
        line(o.str());
    }

    // ---- DMA (TrafficCounter event sink) -------------------------------------
    void dma_issue(const issue& i) override
    {
        std::ostringstream o;
        o << "{\"ev\":\"dma\",\"ph\":\"issue\",\"m\":" << q(kMasters[i.master]) << ",\"id\":" << i.id
          << ",\"w\":" << (i.write ? 1 : 0) << ",\"a\":" << hex(i.address) << ",\"n\":" << i.bytes
          << ",\"be\":" << (i.byte_enable ? 1 : 0) << ",\"t\":" << ticks(i.when) << '}';
        line(o.str());
        ++dma_issued_;
    }
    void dma_response(unsigned master, std::uint64_t id, const sc_core::sc_time& when, bool ok) override
    {
        std::ostringstream o;
        o << "{\"ev\":\"dma\",\"ph\":\"resp\",\"m\":" << q(kMasters[master]) << ",\"id\":" << id
          << ",\"ok\":" << (ok ? 1 : 0) << ",\"t\":" << ticks(when) << '}';
        line(o.str());
        ++dma_responded_;
    }

    // ---- ISP output observer ---------------------------------------------------
    void ingress_admitted(std::uint64_t seq, std::uint32_t epoch, std::uint16_t frame_id, std::uint32_t tag) override
    {
        seq_of_tag_[tag] = seq;
        std::ostringstream o;
        o << "{\"ev\":\"isp_admit\",\"seq\":" << seq << ",\"epoch\":" << epoch << ",\"frame_id\":" << frame_id
          << ",\"tag\":" << tag << ",\"t\":" << now() << '}';
        line(o.str());
    }
    void ingress_not_admitted(std::uint64_t seq, unsigned reason) override
    {
        std::ostringstream o;
        o << "{\"ev\":\"isp_drop\",\"seq\":" << seq << ",\"reason\":" << reason << ",\"t\":" << now() << '}';
        line(o.str());
    }
    void output_ended(cdc::components::fx1_isp::output_end how, const cdc::components::fx1_isp::output_record& r) override
    {
        using E = cdc::components::fx1_isp::output_end;
        const auto it = seq_of_tag_.find(r.tag);
        const std::uint64_t seq = r.direct && it != seq_of_tag_.end() ? it->second : 0;
        ++outputs_;
        std::string dump;
        if (how == E::done && !dump_dir_.empty() && !r.rgb) dump = dump_nv12(r, seq);
        std::ostringstream o;
        o << "{\"ev\":\"output\",\"how\":" << q(how == E::done ? "done" : how == E::failed ? "failed" : "aborted")
          << ",\"n\":" << outputs_ << ",\"seq\":" << seq << ",\"direct\":" << (r.direct ? 1 : 0) << ",\"tag\":" << r.tag
          << ",\"ch\":" << r.channel << ",\"slot\":" << r.slot << ",\"frame_id\":" << r.frame_id
          << ",\"w\":" << r.width << ",\"h\":" << r.height << ",\"y_base\":" << hex(r.y_base)
          << ",\"uv_base\":" << hex(r.uv_base) << ",\"y_stride\":" << r.y_stride << ",\"uv_stride\":" << r.uv_stride
          << ",\"writes\":" << r.writes << ",\"write_bytes\":" << r.write_bytes;
        if (r.wrote) {
            o << ",\"t_first_write_issue\":" << ticks(r.first_write_issue)
              << ",\"t_last_write_response\":" << ticks(r.last_write_response);
        }
        o << ",\"t_end\":" << ticks(r.done);
        if (!dump.empty()) o << ",\"dump\":" << q(dump);
        if (dump_error_) o << ",\"dump_error\":1";
        o << '}';
        line(o.str());
        dump_error_ = false;
    }
    void frame_retired(bool direct, std::uint32_t tag, bool ok) override
    {
        const auto it = seq_of_tag_.find(tag);
        std::ostringstream o;
        o << "{\"ev\":\"retire\",\"direct\":" << (direct ? 1 : 0) << ",\"tag\":" << tag
          << ",\"seq\":" << (direct && it != seq_of_tag_.end() ? it->second : 0) << ",\"ok\":" << (ok ? 1 : 0)
          << ",\"t\":" << now() << '}';
        line(o.str());
    }

    // ---- end of run ------------------------------------------------------------
    void finish(const std::string& reason)
    {
        for (auto& f : rx_open_)
            if (f.open) {
                emit_rx(f, false);
                f.open = false;
            }
        for (const auto& [seq, a] : axis_cap_) emit_axis("axis_capture_open", "seq", seq, a);  // no boundary yet
        commit_before(~std::uint64_t{0});
        flush();
        bool open = false;
        for (const auto& w : windows_) {  // STOP not passed yet: written now, `drained` 0
            open |= !w.stopped;
            emit_window(w, false);
        }
        windows_.clear();
        std::ostringstream o;
        o << "{\"ev\":\"footer\",\"complete\":1,\"reason\":" << q(reason) << ",\"t\":" << now()
          << ",\"window_open\":" << (open ? 1 : 0) << ",\"dma_issued\":" << dma_issued_
          << ",\"dma_responded\":" << dma_responded_ << ",\"outputs\":" << outputs_ << ",\"dump_errors\":" << dump_errors_
          << ",\"rx_outside_bytes\":" << (rx_outside_.bytes[0] + rx_outside_.bytes[1] + rx_outside_.bytes[2] +
                                          rx_outside_.bytes[3] + rx_outside_.bytes[4])
          << ",\"rx_outside_unclassified\":" << rx_outside_.unclassified << '}';
        line(o.str());
        out_.flush();
    }
    bool ok() const { return static_cast<bool>(out_) && dump_errors_ == 0; }

private:
    struct axis_agg {
        std::uint64_t valid = 0, accepted = 0, bytes = 0, aborts = 0, stall = 0, max_stall_run = 0, run = 0;
        std::uint64_t err_beats = 0, last_beats = 0, sof_beats = 0;
        std::uint64_t t_first = 0, t_last = 0;
        bool any = false;
    };
    struct rx_frame {
        bool open = false;
        unsigned vc = 0, fn = 0, dt = 0;
        std::uint64_t idx = 0, seq = 0;
        std::uint64_t t_fs = 0, t_fe = 0, t_first_byte = 0, t_last_byte = 0;
        std::uint64_t bytes[5] = {0, 0, 0, 0, 0}, packets[5] = {0, 0, 0, 0, 0};
        std::uint64_t not_forwarded = 0, wc_sum = 0, crc_bad = 0, truncated = 0, unclassified = 0;
    };
    struct rx_win {
        std::uint64_t image = 0, other = 0, packets = 0;
    };
    struct window {
        std::uint32_t id = 0;
        std::uint64_t start = 0, stop = 0;  // effective times (ticks)
        bool stopped = false;               // STOP seen (its time may still lie ahead)
        axis_agg axis;
        rx_win rx;
        bool covers(std::uint64_t t) const { return t >= start && (!stopped || t < stop); }
    };
    // Window membership of AXIS / RX data does not depend on the order of the
    // callbacks within one simulation time (review BW1 R1): the data of the
    // current tick is kept apart and assigned to the windows with
    // start <= t < stop only once time has moved past it, when every mark
    // that takes effect at t is known (a mark's callback is never later than
    // its effective time). One tick holds at most a few calls (one AXIS cycle
    // per CSI clock).
    struct axis_call {
        std::uint8_t tkeep;
        bool tlast;
        std::uint8_t tuser;
        bool tready;
    };
    struct rx_call {
        int cls;  // -1: a long-packet header
        unsigned n;
    };
    struct tick_data {
        bool any = false;
        std::uint64_t t = 0;
        std::vector<axis_call> axis;
        std::vector<rx_call> rx;
    };
    tick_data& tick(std::uint64_t t)
    {
        commit_before(t);
        flush();
        if (!tick_.any) {
            tick_.any = true;
            tick_.t = t;
        }
        return tick_;
    }
    // Assigns the pending tick to its windows if it is earlier than `t`.
    void commit_before(std::uint64_t t)
    {
        if (!tick_.any || tick_.t >= t) return;
        for (auto& w : windows_) {
            if (!w.covers(tick_.t)) continue;
            for (const auto& a : tick_.axis) count_axis(w.axis, a.tkeep, a.tlast, a.tuser, a.tready, tick_.t);
            for (const auto& r : tick_.rx) {
                if (r.cls < 0) ++w.rx.packets;
                else (r.cls == 0 ? w.rx.image : w.rx.other) += r.n;
            }
        }
        tick_ = tick_data{};
    }
    // Writes the windows whose effective STOP time has been reached (their
    // data before STOP is committed: the pending tick is not earlier than now).
    void flush()
    {
        const std::uint64_t t = now();
        for (auto it = windows_.begin(); it != windows_.end();) {
            if (it->stopped && t >= it->stop) {
                emit_window(*it, true);
                it = windows_.erase(it);
            } else {
                ++it;
            }
        }
    }
    void emit_window(const window& w, bool drained)
    {
        emit_axis("axis_window", "window", w.id, w.axis);
        std::ostringstream r;
        r << "{\"ev\":\"rx_window\",\"window\":" << w.id << ",\"image_bytes\":" << w.rx.image
          << ",\"other_bytes\":" << w.rx.other << ",\"packets\":" << w.rx.packets << ",\"t_start\":" << w.start;
        if (w.stopped) r << ",\"t_stop\":" << w.stop;
        r << ",\"drained\":" << (drained ? 1 : 0) << '}';
        line(r.str());
    }

    void line(const std::string& s) { out_ << s << '\n'; }
    rx_frame* open_rx(unsigned vc) { return rx_open_[vc & 1u].open ? &rx_open_[vc & 1u] : nullptr; }

    static void count_axis(axis_agg& a, std::uint8_t tkeep, bool tlast, std::uint8_t tuser, bool tready, std::uint64_t t)
    {
        ++a.valid;
        if (!a.any) {
            a.t_first = t;
            a.any = true;
        }
        if (!tready) {
            ++a.stall;
            if (++a.run > a.max_stall_run) a.max_stall_run = a.run;
            return;
        }
        a.run = 0;
        ++a.accepted;
        a.t_last = t;
        unsigned n = 0;
        for (std::uint8_t k = tkeep; k; k &= static_cast<std::uint8_t>(k - 1)) ++n;
        a.bytes += n;  // popcount(TKEEP) at the handshake: the null abort beat carries 0
        if (tkeep == 0 && tlast && (tuser & 2u)) ++a.aborts;
        else if (tuser & 2u) ++a.err_beats;
        if (tlast) ++a.last_beats;
        if (tuser & 1u) ++a.sof_beats;
    }
    void emit_axis(const char* ev, const char* key, std::uint64_t id, const axis_agg& a)
    {
        std::ostringstream o;
        o << "{\"ev\":" << q(ev) << ',' << q(key) << ':' << id << ",\"valid_cycles\":" << a.valid
          << ",\"accepted_beats\":" << a.accepted << ",\"useful_bytes\":" << a.bytes << ",\"abort_beats\":" << a.aborts
          << ",\"error_beats\":" << a.err_beats << ",\"stall_cycles\":" << a.stall
          << ",\"max_stall_run\":" << a.max_stall_run << ",\"tlast_beats\":" << a.last_beats
          << ",\"sof_beats\":" << a.sof_beats;
        if (a.any) o << ",\"t_first\":" << a.t_first << ",\"t_last\":" << a.t_last;
        o << ",\"t\":" << now() << '}';
        line(o.str());
    }
    void emit_rx(const rx_frame& f, bool fe)
    {
        std::ostringstream o;
        o << "{\"ev\":\"rx_frame\",\"idx\":" << f.idx << ",\"vc\":" << f.vc << ",\"fn\":" << f.fn << ",\"seq\":" << f.seq
          << ",\"dt\":" << hex(f.dt) << ",\"image_bytes\":" << f.bytes[0] << ",\"embedded_bytes\":" << f.bytes[1]
          << ",\"null_bytes\":" << f.bytes[2] << ",\"unsupported_dt_bytes\":" << f.bytes[3]
          << ",\"unsupported_vc_bytes\":" << f.bytes[4] << ",\"image_packets\":" << f.packets[0]
          << ",\"not_forwarded_bytes\":" << f.not_forwarded << ",\"crc_bad\":" << f.crc_bad
          << ",\"truncated\":" << f.truncated << ",\"unclassified\":" << f.unclassified << ",\"t_fs\":" << f.t_fs;
        if (fe) o << ",\"t_fe\":" << f.t_fe;
        else o << ",\"fe\":0";
        if (f.t_first_byte) o << ",\"t_first_byte\":" << f.t_first_byte << ",\"t_last_byte\":" << f.t_last_byte;
        o << '}';
        line(o.str());
    }
    // Y (height rows of width bytes) then UV (height / 2 rows), active area only.
    std::string dump_nv12(const cdc::components::fx1_isp::output_record& r, std::uint64_t seq)
    {
        std::ostringstream name;
        name << "out" << outputs_ << "_seq" << seq << "_ch" << r.channel << "_slot" << r.slot << ".nv12";
        const std::string path = dump_dir_ + "/" + name.str();
        try {
            std::vector<std::uint8_t> img(std::size_t{r.width} * r.height * 3u / 2u);
            std::size_t off = 0;
            for (std::uint32_t y = 0; y < r.height; ++y, off += r.width)
                read_(r.y_base + std::uint64_t{y} * r.y_stride, img.data() + off, r.width);
            for (std::uint32_t y = 0; y < r.height / 2u; ++y, off += r.width)
                read_(r.uv_base + std::uint64_t{y} * r.uv_stride, img.data() + off, r.width);
            std::ofstream f(path, std::ios::binary);
            f.write(reinterpret_cast<const char*>(img.data()), static_cast<std::streamsize>(img.size()));
            if (!f) throw std::runtime_error("write");
        } catch (const std::exception&) {
            ++dump_errors_;
            dump_error_ = true;
            return {};
        }
        return name.str();
    }

    std::ofstream out_;
    std::string dump_dir_;
    ddr_reader read_;
    std::vector<window> windows_;  // started, not yet written
    tick_data tick_;               // AXIS / RX data of the latest tick, not yet assigned
    std::map<std::uint64_t, axis_agg> axis_cap_;
    rx_frame rx_open_[2], rx_outside_;
    std::uint64_t rx_frames_ = 0;
    int pkt_cls_ = 0;
    unsigned pkt_vc_ = 0;
    bool pkt_fwd_ = false;
    std::map<std::uint32_t, std::uint64_t> seq_of_tag_;
    std::uint64_t dma_issued_ = 0, dma_responded_ = 0, outputs_ = 0, dump_errors_ = 0;
    bool dump_error_ = false;
};

}  // namespace cdc::platforms::fx1_soc
