#include "fx1_csi2_phy/dphy_adapter.h"

#include <algorithm>
#include <cstdio>

namespace fx1::csi2 {

namespace {
template <class T>
T* find_in(const sc_core::sc_object& root, const char* rel) {
    const std::string full = std::string(root.name()) + "." + rel;
    auto* o = dynamic_cast<T*>(sc_core::sc_find_object(full.c_str()));
    if (!o) SC_REPORT_FATAL("fx1/csi2/dphy", ("observation signal not found: " + full).c_str());
    return o;
}

unsigned to_state(const sc_dt::sc_lv<4>& v) { return v.is_01() ? v.to_uint() : 0xFF; }

sc_dt::sc_logic lg(bool b) { return b ? sc_dt::SC_LOGIC_1 : sc_dt::SC_LOGIC_0; }
}  // namespace

DphyAdapter::DphyAdapter(sc_core::sc_module_name nm, DphyRxWrapper& phy, DphyAdapterConfig cfg)
    : sc_core::sc_module(nm), w_(phy), cfg_(std::move(cfg)) {
    state_main_ = find_in<sc_core::sc_signal_rv<4>>(w_, kStateMain);
    enable_lanes_ = find_in<sc_core::sc_signal_resolved>(w_, kEnableLanes);
    SC_METHOD(clock_gen);
    SC_THREAD(control_thread);
    SC_METHOD(observe);
    sensitive << *state_main_ << *enable_lanes_ << progress_ev_;
    dont_initialize();
}

void DphyAdapter::set_control(bool phy_enable, bool cl_enable, uint8_t dl_mask, uint8_t forcerx_mask) {
    req_phy_enable_ = phy_enable;
    req_cl_ = cl_enable;
    req_dl_ = dl_mask & 0xF;
    req_forcerx_ = forcerx_mask & 0xF;
    ctrl_ev_.notify(sc_core::SC_ZERO_TIME);
}

void DphyAdapter::request_reset() {
    reset_req_ = true;
    pulse_active_ = true;  // visible to reset_in_progress from the request on
    ctrl_ev_.notify(sc_core::SC_ZERO_TIME);
}

bool DphyAdapter::init_done() const {
    return running_ && to_state(state_main_->read()) == 7 && enable_lanes_->read() == sc_dt::SC_LOGIC_1;
}

void DphyAdapter::clock_gen() {
    if (!clk_on_) {
        clk_level_ = false;
        w_.cfg_clk.write(sc_dt::SC_LOGIC_0);
        next_trigger(clk_start_ev_);
        return;
    }
    w_.cfg_clk.write(lg(clk_level_));
    clk_level_ = !clk_level_;
    next_trigger(cfg_.cfg_clk_period / 2);
}

void DphyAdapter::observe() {
    const unsigned s = to_state(state_main_->read());
    if (trace_.empty() || trace_.back().second != s) trace_.emplace_back(sc_core::sc_time_stamp(), s);
    if (!running_ || done_time_ > release_time_ || timed_out_) return;
    if (init_done()) {
        done_time_ = sc_core::sc_time_stamp();
        status_ev_.notify(sc_core::SC_ZERO_TIME);
    } else if (sc_core::sc_time_stamp() >= release_time_ + cfg_.startup_timeout) {
        timed_out_ = true;
        status_ev_.notify(sc_core::SC_ZERO_TIME);
    }
}

void DphyAdapter::power_down() {
    running_ = false;
    w_.shutdownz.write(sc_dt::SC_LOGIC_0);
    w_.rstz.write(sc_dt::SC_LOGIC_0);
    w_.enableclk.write(sc_dt::SC_LOGIC_0);
    w_.enable.write(sc_dt::sc_lv<4>(0));
    for (auto& f : w_.forcerxmode) f.write(sc_dt::SC_LOGIC_0);
    applied_cl_ = false;
    applied_dl_ = applied_forcerx_ = 0;
    clk_on_ = false;
}

void DphyAdapter::startup() {
    ++startups_;
    timed_out_ = false;
    clk_on_ = true;
    clk_start_ev_.notify(sc_core::SC_ZERO_TIME);
    applied_cl_ = req_cl_;
    applied_dl_ = req_dl_;
    applied_forcerx_ = 0;
    if (applied_dl_ && !(applied_dl_ & 1))
        SC_REPORT_WARNING("fx1/csi2/dphy", "data lane 0 disabled: rxbyteclkhs is lane 0's word clock, no lane will receive");
    // Configuration is static and valid before reset release (contract §3.2).
    w_.cfgclkfreqrange.write(sc_dt::sc_lv<8>(cfg_.cfgclkfreqrange));
    w_.hsfreqrange.write(sc_dt::sc_lv<7>(cfg_.hsfreqrange));
    w_.enable.write(sc_dt::sc_lv<4>(applied_dl_));
    w_.enableclk.write(lg(applied_cl_));
    w_.basedir.write(lg(cfg_.basedir));
    for (auto& f : w_.forcerxmode) f.write(sc_dt::SC_LOGIC_0);
    w_.shutdownz.write(sc_dt::SC_LOGIC_0);
    w_.rstz.write(sc_dt::SC_LOGIC_0);
    w_.testclr.write(sc_dt::SC_LOGIC_0);
    cycles(2);
    // Tester registers power up as X; only testclr loads their defaults.
    w_.testclr.write(sc_dt::SC_LOGIC_1);
    cycles(cfg_.testclr_cycles);
    w_.testclr.write(sc_dt::SC_LOGIC_0);
    cycles(2);
    for (const auto& [addr, value] : cfg_.test_regs) {
        const uint8_t rb = write_test_reg(addr, value);
        if (rb != value) {
            ++test_reg_mismatches_;
            char msg[96];
            std::snprintf(msg, sizeof msg, "test register 0x%02X: wrote 0x%02X, read back 0x%02X", addr, value, rb);
            SC_REPORT_WARNING("fx1/csi2/dphy", msg);
        }
    }
    w_.shutdownz.write(sc_dt::SC_LOGIC_1);
    w_.rstz.write(sc_dt::SC_LOGIC_1);
    release_time_ = sc_core::sc_time_stamp();
    done_time_ = sc_core::SC_ZERO_TIME;
    running_ = true;
    progress_ev_.cancel();  // a pending earlier notification would swallow this one
    progress_ev_.notify(cfg_.startup_timeout);  // timeout check in observe()
}

void DphyAdapter::apply_running_changes() {
    if (applied_cl_ && !req_cl_) {
        applied_cl_ = false;
        w_.enableclk.write(sc_dt::SC_LOGIC_0);
    } else if (!applied_cl_ && req_cl_) {
        SC_REPORT_WARNING("fx1/csi2/dphy", "clock lane enabled while the PHY runs: effective after the next PHY reset");
    }
    const uint8_t removed = applied_dl_ & ~req_dl_;
    if (removed) {
        applied_dl_ &= static_cast<uint8_t>(~removed);
        w_.enable.write(sc_dt::sc_lv<4>(applied_dl_));
    }
    if (req_dl_ & ~applied_dl_)
        SC_REPORT_WARNING("fx1/csi2/dphy", "data lane enabled while the PHY runs: not calibrated, effective after the next PHY reset");
    if (req_forcerx_ != applied_forcerx_) {
        applied_forcerx_ = req_forcerx_;
        for (int i = 0; i < 4; ++i) w_.forcerxmode[i].write(lg(applied_forcerx_ >> i & 1));
    }
}

void DphyAdapter::control_thread() {
    power_down();
    for (;;) {
        if (reset_req_) {
            reset_req_ = false;
            power_down();
            wait(std::max(cfg_.min_reset_pulse, cfg_.cfg_clk_period * 4));
            if (!reset_req_) {
                pulse_active_ = false;
                status_ev_.notify(sc_core::SC_ZERO_TIME);
            }
            continue;
        }
        if (!running_ && req_phy_enable_) {
            startup();
            continue;
        }
        if (running_ && !req_phy_enable_) {
            power_down();
            continue;
        }
        if (running_) apply_running_changes();
        wait(ctrl_ev_);
    }
}

uint8_t DphyAdapter::write_test_reg(uint8_t addr, uint8_t value) {
    using sc_dt::SC_LOGIC_0;
    using sc_dt::SC_LOGIC_1;
    const sc_core::sc_time h = cfg_.testclk_half_period;
    // Test interface (TST:13642-13656, 14155+): address latched on the falling
    // testclk edge with testen=1, data on the rising edge with testen=0.
    w_.testclk.write(SC_LOGIC_1);
    wait(h);
    w_.testen.write(SC_LOGIC_1);
    w_.testdin.write(sc_dt::sc_lv<8>(addr));
    wait(h);
    w_.testclk.write(SC_LOGIC_0);  // falling edge: testcode <= addr
    wait(h);
    w_.testen.write(SC_LOGIC_0);
    w_.testdin.write(sc_dt::sc_lv<8>(value));
    wait(h);
    w_.testclk.write(SC_LOGIC_1);  // rising edge: reg[addr] <= value
    wait(h);
    const sc_dt::sc_lv<8> rb = w_.testdout.read();
    w_.testclk.write(SC_LOGIC_0);
    w_.testdin.write(sc_dt::sc_lv<8>(0));
    wait(h);
    return rb.is_01() ? static_cast<uint8_t>(rb.to_uint()) : 0xFF;
}

std::string DphyAdapter::fsm_trace_text() const {
    std::string s;
    char buf[96];
    for (const auto& [t, st] : trace_) {
        std::snprintf(buf, sizeof buf, "  t=%12.3f ns  state_main=%s\n", t.to_seconds() * 1e9,
                      st == 0xFF ? "X" : std::to_string(st).c_str());
        s += buf;
    }
    return s;
}

}  // namespace fx1::csi2
