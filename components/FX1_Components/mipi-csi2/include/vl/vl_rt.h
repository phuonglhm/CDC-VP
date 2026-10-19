// Runtime support for the SystemC model: signal access, timing, system
// tasks, gate primitives, UDP evaluation and delayed assignment helpers.
#ifndef VL_RT_H
#define VL_RT_H

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "vl_logic.h"

namespace vl {

// ------------------------------------------------------------------ channel / port types
typedef sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> sig1_mw;
template <int N> using sigv_mw = sc_core::sc_signal<sc_dt::sc_lv<N>, sc_core::SC_UNCHECKED_WRITERS>;

// ------------------------------------------------------------------ value conversion
inline logic<1> cv(const sc_dt::sc_logic& v) { return logic<1>(v); }
template <int N> inline logic<N> cv(const sc_dt::sc_lv<N>& v) { return logic<N>(v); }
template <class S> inline auto rd(const S& s) -> decltype(cv(s.read())) { return cv(s.read()); }

template <class S> inline void wr(S& s, const logic<1>& v) { s.write(v.sl()); }
template <class S, int N> inline void wr(S& s, const logic<N>& v) { s.write(v.lv()); }

// Register signal that also accepts part-select writes.  A part write is
// applied to the value pending for the current delta cycle, so part writes
// from several processes (or several non-blocking assignments of one process)
// in the same time step all take effect, as in Verilog.
template <int N>
class vreg : public sc_core::sc_signal<sc_dt::sc_lv<N>, sc_core::SC_UNCHECKED_WRITERS> {
    typedef sc_core::sc_signal<sc_dt::sc_lv<N>, sc_core::SC_UNCHECKED_WRITERS> base_type;

public:
    vreg(const char* nm, const sc_dt::sc_lv<N>& init) : base_type(nm, init) {}
    template <int M>
    void write_part(int64_t lo, const logic<M>& v) {
        logic<N> cur(this->m_new_val);
        cur.set_slice(lo, v);
        this->write(cur.lv());
    }
    void write_bit(int64_t pos, const logic<1>& v) {
        logic<N> cur(this->m_new_val);
        cur.set_bit(pos, v);
        this->write(cur.lv());
    }
};

// creator for sc_vector of resolved bit nets (initial value Z)
inline sc_core::sc_signal_resolved* make_z1(const char* nm, size_t) {
    return new sc_core::sc_signal_resolved(nm, sc_dt::SC_LOGIC_Z);
}

// ------------------------------------------------------------------ drive strength
// Verilog drive strengths (one strength per driver for both 0 and 1).
enum strength_t { ST_HIGHZ = 0, ST_SMALL = 1, ST_MEDIUM = 2, ST_WEAK = 3, ST_LARGE = 4, ST_PULL = 5,
                  ST_STRONG = 6, ST_SUPPLY = 7 };

// Resolved 1-bit net that also resolves Verilog drive strengths.  A plain
// write() is a strong drive, as for Verilog gates and continuous assignments.
// Use it for the pad nets (datap/datan/clkp/clkn) so that the LP receivers can
// distinguish pull-strength (LP) from strong (HS) line driving.
class sres : public sc_core::sc_signal_resolved {
public:
    explicit sres(const char* nm, const sc_dt::sc_logic& init = sc_dt::SC_LOGIC_Z)
        : sc_core::sc_signal_resolved(nm, init) {}
    void write(const sc_dt::sc_logic& v) override { write_s(v, ST_STRONG); }
    void write_s(const sc_dt::sc_logic& v, int st);
    int strength() const { return str_; }
    const sc_core::sc_event& strength_changed_event() const { return str_ev_; }
    const char* kind() const override { return "vl::sres"; }

protected:
    void update() override;

private:
    std::vector<sc_core::sc_process_b*> procs_;
    std::vector<std::pair<sc_dt::sc_logic, int>> vals_;
    int str_ = ST_HIGHZ;
    sc_core::sc_event str_ev_;
};

// strength of the value currently on the channel bound to port/signal p
template <class P>
inline int strength_of(const P& p) {
    const sres* s = dynamic_cast<const sres*>(p.get_interface());
    if (s) return s->strength();
    return p.read() == sc_dt::SC_LOGIC_Z ? ST_HIGHZ : ST_STRONG;
}
template <class P>
inline const sres* sres_of(const P& p) { return dynamic_cast<const sres*>(p.get_interface()); }
// drive a port with a given strength (plain write if the channel has no strength)
template <class P>
inline void wr_s(P& p, const logic<1>& v, int st) {
    sres* s = dynamic_cast<sres*>(p.get_interface());
    if (s) s->write_s(v.sl(), st);
    else p.write(v.sl());
}
inline void wr_s(sres& s, const logic<1>& v, int st) { s.write_s(v.sl(), st); }
inline void wr_s(sc_core::sc_signal_resolved& s, const logic<1>& v, int st) {
    sres* r = dynamic_cast<sres*>(&s);
    if (r) r->write_s(v.sl(), st);
    else s.write(v.sl());
}

// posedge / negedge of a 1-bit value transition (Verilog semantics)
inline bool is_posedge(int from, int to) { return from != to && (from == 0 || to == 1); }
inline bool is_negedge(int from, int to) { return from != to && (from == 1 || to == 0); }

// ------------------------------------------------------------------ time
inline sc_core::sc_time dly(double v, double unit_fs, double prec_fs) {
    double t = std::floor(v * unit_fs / prec_fs + 0.5) * prec_fs;
    if (t < 0) t = 0;
    return sc_core::sc_time(t, sc_core::SC_FS);
}
template <int N>
inline sc_core::sc_time dly(const logic<N>& v, double unit_fs, double prec_fs) {
    return dly(v.known() ? (double)v.u64() : 0.0, unit_fs, prec_fs);
}
inline double now_fs() { return sc_core::sc_time_stamp().to_seconds() * 1e15; }
inline double realtime(double unit_fs) { return now_fs() / unit_fs; }
inline logic<64> time(double unit_fs) { return logic<64>((uint64_t)std::floor(now_fs() / unit_fs + 0.5)); }

// ------------------------------------------------------------------ $random
extern int32_t g_seed;
int32_t random32(int32_t& seed);
inline logic<32> random() { return logic<32>((uint64_t)(uint32_t)random32(g_seed)); }
template <int N> inline logic<32> random(logic<N>& seed) {
    int32_t s = (int32_t)seed.u64();
    int32_t r = random32(s);
    seed = logic<N>((uint64_t)(int64_t)s);
    return logic<32>((uint64_t)(uint32_t)r);
}

// ------------------------------------------------------------------ $display
struct farg {
    enum kind_t { LOGIC, REAL, STR } kind;
    int width;
    bool sgn;
    std::vector<uint64_t> a, b;
    double d;
    std::string s;
};
template <int N>
inline farg A(const logic<N>& v, bool sgn = false) {
    farg f;
    f.kind = farg::LOGIC; f.width = N; f.sgn = sgn; f.d = 0;
    f.a.assign(v.a, v.a + logic<N>::W);
    f.b.assign(v.b, v.b + logic<N>::W);
    return f;
}
inline farg A(double d) { farg f; f.kind = farg::REAL; f.width = 64; f.sgn = true; f.d = d; return f; }
inline farg A(const char* s) { farg f; f.kind = farg::STR; f.width = 0; f.sgn = false; f.d = 0; f.s = s; return f; }
std::string format(const std::string& scope, double unit_fs, const char* fmt, const std::vector<farg>& args);
void display(const sc_core::sc_object* scope, double unit_fs, bool newline, const char* fmt, std::vector<farg> args);
void finish();

// ------------------------------------------------------------------ gate primitives
inline logic<1> buf_(const logic<1>& x) { int v = x.get_bit_val(0); return v >= 2 ? LX() : x; }
inline logic<1> not_(const logic<1>& x) { return ~x; }
inline logic<1> and_(const logic<1>& x) { return buf_(x); }
template <typename... R> inline logic<1> and_(const logic<1>& x, const R&... r) { return x & and_(r...); }
inline logic<1> or_(const logic<1>& x) { return buf_(x); }
template <typename... R> inline logic<1> or_(const logic<1>& x, const R&... r) { return x | or_(r...); }
inline logic<1> xor_(const logic<1>& x) { return buf_(x); }
template <typename... R> inline logic<1> xor_(const logic<1>& x, const R&... r) { return x ^ xor_(r...); }
template <typename... R> inline logic<1> nand_(const R&... r) { return ~and_(r...); }
template <typename... R> inline logic<1> nor_(const R&... r) { return ~or_(r...); }
template <typename... R> inline logic<1> xnor_(const R&... r) { return ~xor_(r...); }
inline logic<1> bufif1_(const logic<1>& d, const logic<1>& c) {
    int cv_ = c.get_bit_val(0);
    if (cv_ == 1) return buf_(d);
    if (cv_ == 0) return LZ();
    return LX();
}
inline logic<1> bufif0_(const logic<1>& d, const logic<1>& c) { return bufif1_(d, ~c); }
inline logic<1> notif1_(const logic<1>& d, const logic<1>& c) { return bufif1_(~d, c); }
inline logic<1> notif0_(const logic<1>& d, const logic<1>& c) { return bufif1_(~d, ~c); }
inline logic<1> nmos_(const logic<1>& d, const logic<1>& g) {
    int gv = g.get_bit_val(0);
    if (gv == 1) return d;
    if (gv == 0) return LZ();
    return d.get_bit_val(0) == 2 ? LZ() : LX();
}
inline logic<1> pmos_(const logic<1>& d, const logic<1>& g) { return nmos_(d, ~g); }

// wired resolution of two drivers (no strength)
inline logic<1> resolve(const logic<1>& x, const logic<1>& y) {
    int a = x.get_bit_val(0), b = y.get_bit_val(0);
    if (a == 2) return y;
    if (b == 2) return x;
    if (a == b) return x;
    return LX();
}
template <int N>
inline logic<N> resolve(const logic<N>& x, const logic<N>& y) {
    logic<N> r(0);
    for (int i = 0; i < N; i++) r.set_bit(i, resolve(x.bit(i), y.bit(i)));
    return r;
}

// ------------------------------------------------------------------ UDP
class udp {
public:
    udp(const char* name, int nin, bool seq, const char* table);
    // combinational evaluation; inputs as 0/1/3(x)
    int eval_comb(const int* in) const;
    // sequential: input k changed from 'from' to in[k]; st = current state
    int eval_seq(const int* in, int k, int from, int st) const;
    int nin() const { return nin_; }
    bool seq() const { return seq_; }

private:
    struct row {
        std::vector<unsigned char> lvl;   // level mask per input (bit0=0,bit1=1,bit2=x)
        int edge_in;                      // -1 if no edge
        unsigned char ef, et;             // edge from/to masks
        unsigned char st;                 // state mask (seq)
        int out;                          // 0,1,3 or -1 for '-'
    };
    std::string name_;
    int nin_;
    bool seq_;
    std::vector<row> level_rows_, edge_rows_;
};
inline int v3(const logic<1>& x) { int v = x.get_bit_val(0); return v == 2 ? 3 : v; }
inline logic<1> from3(int v) { return v == 0 ? L0() : v == 1 ? L1() : LX(); }
template <typename... A>
inline logic<1> ucomb(const udp& u, A... a) {
    const int in[] = {a...};
    return from3(u.eval_comb(in));
}

// sequential UDP instance state inside a compiled cell
template <int NIN>
struct udp_state {
    int prev[NIN];
    int st;
    explicit udp_state(int init = 3) : st(init) { for (int i = 0; i < NIN; i++) prev[i] = 3; }
    // returns true if state changed
    bool update(const udp& u, const int* in) {
        int old = st;
        int cur[NIN];
        for (int i = 0; i < NIN; i++) cur[i] = prev[i];
        for (int k = 0; k < NIN; k++) {
            if (in[k] == prev[k]) continue;
            int from = cur[k];
            cur[k] = in[k];
            st = u.eval_seq(cur, k, from, st);
        }
        for (int i = 0; i < NIN; i++) prev[i] = in[i];
        return st != old;
    }
};

// ------------------------------------------------------------------ delayed updates
// Transport-delay action queue (intra-assignment delays on non-blocking assignments).
class action_queue : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(action_queue);
    explicit action_queue(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(run);
        sensitive << ev_;
        dont_initialize();
    }
    void after(const sc_core::sc_time& d, std::function<void()> f) {
        sc_core::sc_time at = sc_core::sc_time_stamp() + d;
        q_.emplace(at, std::move(f));
        ev_.notify(q_.begin()->first - sc_core::sc_time_stamp());
    }

private:
    void run() {
        sc_core::sc_time now = sc_core::sc_time_stamp();
        while (!q_.empty() && q_.begin()->first <= now) {
            auto f = std::move(q_.begin()->second);
            q_.erase(q_.begin());
            f();
        }
        if (!q_.empty()) ev_.notify(q_.begin()->first - now);
    }
    std::multimap<sc_core::sc_time, std::function<void()>> q_;
    sc_core::sc_event ev_;
};

// Inertial delay helper: pick the delay for a transition (rise, fall, turn-off).
inline sc_core::sc_time pick_delay(int to, const sc_core::sc_time& rise, const sc_core::sc_time& fall,
                                   const sc_core::sc_time& off) {
    if (to == 1) return rise;
    if (to == 0) return fall;
    if (to == 2) return off;
    sc_core::sc_time m = rise < fall ? rise : fall;
    return off < m ? off : m;
}

// Constant driver for tie nets.
class tie : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> o;
    SC_HAS_PROCESS(tie);
    tie(sc_core::sc_module_name nm, sc_dt::sc_logic v) : sc_core::sc_module(nm), o("o"), v_(v) { SC_METHOD(run); }

private:
    void run() { o.write(v_); }
    sc_dt::sc_logic v_;
};

}  // namespace vl

#endif
