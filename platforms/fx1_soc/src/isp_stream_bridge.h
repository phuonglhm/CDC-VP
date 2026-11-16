#pragma once
// VP-only host bridge of the ISP RAW-sequence stream (fx1_soc --isp-stream-spool DIR;
// platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md, ABI fx1/fx1_isp_stream.h).
//
// An SC_THREAD. When the DDR is released (before the CPUs run) it attaches the session
// it finds in DDR: the control block names it and carries the spool budget
// (--isp-stream-max-mib), which the firmware enforces as an admission limit (it
// submits only the frames the budget holds). Then, woken by the sim-control doorbell
// or a 1 ms simulated-time poll (fallback, and Ctrl+C), it
//   1. turns a pending user stop (request_user_stop(), async-signal-safe) into
//      control = STOP, USER (also between the frames of one drain);
//   2. drains the published records strictly by seq: checks the record (magic, session,
//      seq, ordinal, slot, addresses, geometry), copies the active Y and UV rows with the
//      DDR debug read (stored bytes, not a bus transaction), appends them to
//      DIR/frames.nv12 and one line to DIR/frames.jsonl, fsyncs both, and only then
//      writes ack_seq = seq (DDR debug write). A frame is never ACKed unless stored.
// Any protocol violation or host I/O failure is fatal for the bridge: it requests STOP
// (the firmware then fails on the missing ACK) and never ACKs again. At the end of the
// run, finish() writes DIR/status.json (the firmware's final status block and the
// bridge's counts). No firmware-visible timing changes: file I/O costs wall time only.

#include <cstdint>
#include <string>
#include <vector>

#include <systemc>

#include <fx1/fx1_isp_stream.h>

namespace fx1 {
class SimControl;
}

namespace cdc::platforms::fx1_soc {

class Ddr3Subsystem;

class IspStreamBridge : public sc_core::sc_module {
public:
    struct Options {
        std::string dir;                  // must be new or empty (checked by the constructor)
        std::uint64_t max_spool_bytes = 0;  // 0: no budget
        // Fault injection (--isp-stream-hold-us): every frame is held this long (simulated
        // time) after it was seen published before it is copied and acknowledged, as a
        // slow host would; the firmware must not reuse the slot meanwhile. 0: off.
        unsigned hold_us = 0;
        // Fault injection (--isp-stream-fault): foreign_ack: the first frame's ACK is
        // written into its record after the record's session was altered.
        enum class Fault { none, foreign_ack } fault = Fault::none;
    };

    SC_HAS_PROCESS(IspStreamBridge);
    IspStreamBridge(sc_core::sc_module_name name, Ddr3Subsystem& ddr, fx1::SimControl& sim, const Options& options);
    ~IspStreamBridge() override;

    // Async-signal-safe: counts a Ctrl+C. The bridge thread forwards the first one as a
    // STOP, USER request at its next wake.
    static void request_user_stop() noexcept;
    static unsigned user_stop_requests() noexcept;

    // After the simulation: reads the final status block, writes DIR/status.json.
    void finish();
    bool ok() const noexcept { return errors_.empty(); }
    const std::vector<std::string>& errors() const noexcept { return errors_; }
    std::uint64_t frames() const noexcept { return captured_; }
    std::string report() const;  // one line

private:
    void run();
    void forward_requests();
    void drain();
    bool capture(unsigned index, const fx1_isp_stream_record& r);
    bool read_desc();
    void write_control(std::uint32_t request, std::uint32_t reason);
    void attach();
    void request_stop(std::uint32_t reason, const char* why);
    void fail(const std::string& what);
    template <class T> T read_block(std::uint64_t address) const;
    void write_u32(std::uint64_t address, std::uint32_t value);
    bool append(int fd, const void* data, std::size_t size, const char* what);

    Ddr3Subsystem& ddr_;
    fx1::SimControl& sim_;
    Options options_;
    int nv12_fd_ = -1, jsonl_fd_ = -1;
    bool have_desc_ = false;
    fx1_isp_stream_desc desc_{};
    std::uint64_t session_ = 0;
    std::uint32_t next_seq_ = 1;
    std::uint64_t captured_ = 0, spooled_bytes_ = 0;
    std::uint64_t wakes_ = 0, stale_ = 0, max_batch_ = 0;  // stale_: observations
    std::uint32_t stop_sent_ = 0;  // FX1_ISP_STREAM_REASON_* of the request written, 0: none
    bool failed_ = false;          // a fatal bridge error: no further ACK
    bool attached_ = false;        // the control block names the session in DDR
    bool fault_done_ = false;
    std::vector<std::string> errors_;
    std::string final_state_;
};

}  // namespace cdc::platforms::fx1_soc
