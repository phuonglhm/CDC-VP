#include "vl_rt.h"
#include <climits>
#include <cstring>
#include <iostream>
#include <sstream>

namespace vl {

int32_t g_seed = 0;

// Uniform generator of IEEE 1364 (rtl_dist_uniform with full 32-bit range).
static double uniform(int32_t* seed, int32_t start, int32_t end) {
    union { float s; uint32_t stemp; } u;
    double d = 0.00000011920928955078125;
    double a, b, c;
    if (*seed == 0) *seed = 259341593;
    if (start >= end) { a = 0.0; b = 2147483647.0; }
    else { a = (double)start; b = (double)end; }
    *seed = (int32_t)(69069u * (uint32_t)(*seed) + 1u);
    u.stemp = (uint32_t)*seed;
    u.stemp = (u.stemp >> 9) | 0x3f800000;
    c = (double)u.s;
    c = c + (c * d);
    c = ((b - a) * (c - 1.0)) + a;
    return c;
}

int32_t random32(int32_t& seed) {
    double r = (uniform(&seed, INT32_MIN, INT32_MAX) + 2147483648.0) / 4294967295.0;
    r = r * 4294967296.0 - 2147483648.0;
    int64_t i = r >= 0 ? (int64_t)r : (int64_t)(r - 1);
    return (int32_t)i;
}

// ------------------------------------------------------------------ formatting
static int bitval(const farg& f, int i) {
    return (int)(((f.a[i >> 6] >> (i & 63)) & 1) | (((f.b[i >> 6] >> (i & 63)) & 1) << 1));
}

static std::string fmt_radix(const farg& f, int bits_per_digit, bool pad) {
    int nd = (f.width + bits_per_digit - 1) / bits_per_digit;
    std::string s;
    for (int d = nd - 1; d >= 0; d--) {
        int val = 0, nx = 0, nz = 0, nb = 0;
        for (int k = bits_per_digit - 1; k >= 0; k--) {
            int i = d * bits_per_digit + k;
            if (i >= f.width) continue;
            nb++;
            int v = bitval(f, i);
            if (v == 3) nx++;
            else if (v == 2) nz++;
            else val = val * 2 + v;
            if (v >= 2) val = val * 2;
        }
        char c;
        if (nx == nb) c = 'x';
        else if (nz == nb) c = 'z';
        else if (nx) c = 'X';
        else if (nz) c = 'Z';
        else c = "0123456789abcdef"[val];
        s += c;
    }
    if (!pad) {
        size_t p = s.find_first_not_of('0');
        s = p == std::string::npos ? "0" : s.substr(p);
    }
    return s;
}

static std::string fmt_dec(const farg& f, bool pad) {
    bool anyx = false, allx = true, anyz = false, allz = true;
    for (int i = 0; i < f.width; i++) {
        int v = bitval(f, i);
        if (v == 3) anyx = true; else allx = false;
        if (v == 2) anyz = true; else allz = false;
    }
    std::string s;
    if (anyx || anyz) {
        s = allx ? "x" : allz ? "z" : anyx ? "X" : "Z";
    } else {
        // arbitrary width unsigned/signed decimal
        std::vector<uint64_t> w(f.a);
        bool neg = false;
        if (f.sgn && f.width > 0 && bitval(f, f.width - 1) == 1) {
            neg = true;
            // two's complement negate within width
            uint64_t carry = 1;
            for (size_t k = 0; k < w.size(); k++) {
                uint64_t x = ~w[k];
                uint64_t y = x + carry;
                carry = (carry && y == 0) ? 1 : 0;
                w[k] = y;
            }
            int top = f.width - 64 * ((int)w.size() - 1);
            if (top < 64) w.back() &= (1ULL << top) - 1;
        }
        bool zero = true;
        for (uint64_t x : w) if (x) zero = false;
        if (zero) s = "0";
        while (!zero) {
            unsigned __int128 rem = 0;
            for (int k = (int)w.size() - 1; k >= 0; k--) {
                unsigned __int128 cur = (rem << 64) | w[k];
                w[k] = (uint64_t)(cur / 10);
                rem = cur % 10;
            }
            s.insert(s.begin(), (char)('0' + (int)rem));
            zero = true;
            for (uint64_t x : w) if (x) zero = false;
        }
        if (neg) s.insert(s.begin(), '-');
    }
    if (pad) {
        // width of the largest value representable
        double digits = f.width * 0.30102999566398;
        size_t nd = (size_t)std::ceil(digits) + (f.sgn ? 1 : 0);
        if (f.width <= 1) nd = 1;
        while (s.size() < nd) s.insert(s.begin(), ' ');
    }
    return s;
}

static double arg_real(const farg& f) {
    if (f.kind == farg::REAL) return f.d;
    if (f.kind == farg::STR) return 0.0;
    double r = 0;
    for (int i = f.width - 1; i >= 0; i--) {
        int v = bitval(f, i);
        r = r * 2 + (v == 1 ? 1 : 0);
    }
    if (f.sgn && f.width > 0 && bitval(f, f.width - 1) == 1) r -= std::ldexp(1.0, f.width);
    return r;
}

static farg to_logic_arg(const farg& f) {
    if (f.kind == farg::LOGIC) return f;
    farg g;
    g.kind = farg::LOGIC; g.width = 64; g.sgn = true; g.d = 0;
    double r = f.kind == farg::REAL ? f.d : 0.0;
    int64_t v = (int64_t)(r >= 0 ? std::floor(r + 0.5) : -std::floor(-r + 0.5));
    g.a.assign(1, (uint64_t)v);
    g.b.assign(1, 0);
    return g;
}

std::string format(const std::string& scope, double unit_fs, const char* fmt, const std::vector<farg>& args) {
    std::ostringstream os;
    size_t arg_idx = 0;
    const char* p = fmt;
    while (*p) {
        if (*p != '%') { os << *p++; continue; }
        p++;
        if (*p == '%') { os << '%'; p++; continue; }
        std::string flags;
        while (*p && (isdigit((unsigned char)*p) || *p == '.' || *p == '-')) flags += *p++;
        char c = *p ? *p++ : 0;
        bool pad = flags.empty();
        if (c == 'm' || c == 'M') { os << scope; continue; }
        if (arg_idx >= args.size()) { os << '%' << flags << c; continue; }
        const farg& f = args[arg_idx++];
        switch (c) {
            case 'd': case 'D': os << fmt_dec(to_logic_arg(f), pad); break;
            case 'h': case 'H': case 'x': case 'X': os << fmt_radix(to_logic_arg(f), 4, pad); break;
            case 'b': case 'B': os << fmt_radix(to_logic_arg(f), 1, pad); break;
            case 'o': case 'O': os << fmt_radix(to_logic_arg(f), 3, pad); break;
            case 's': case 'S':
                if (f.kind == farg::STR) os << f.s;
                else {
                    std::string s;
                    farg g = to_logic_arg(f);
                    for (int i = (g.width / 8) * 8 - 8; i >= 0; i -= 8) {
                        int ch = 0;
                        for (int k = 7; k >= 0; k--) ch = ch * 2 + (bitval(g, i + k) == 1);
                        if (ch) s += (char)ch;
                    }
                    os << s;
                }
                break;
            case 'c': case 'C': {
                farg g = to_logic_arg(f);
                int ch = 0;
                for (int k = 7; k >= 0; k--) ch = ch * 2 + (k < g.width && bitval(g, k) == 1);
                os << (char)ch;
                break;
            }
            case 't': case 'T': {
                double v = arg_real(f) * unit_fs;  // in fs
                char buf[64];
                std::snprintf(buf, sizeof buf, "%20.0f", v);
                os << buf;
                break;
            }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
                std::string spec = "%" + flags + c;
                char buf[128];
                std::snprintf(buf, sizeof buf, spec.c_str(), arg_real(f));
                os << buf;
                break;
            }
            default:
                os << '%' << flags << c;
        }
    }
    return os.str();
}

void display(const sc_core::sc_object* scope, double unit_fs, bool newline, const char* fmt, std::vector<farg> args) {
    std::string s = format(scope ? scope->name() : "", unit_fs, fmt, args);
    std::cout << s;
    if (newline) std::cout << std::endl;
}

void finish() { sc_core::sc_stop(); }

// ------------------------------------------------------------------ strength-resolved net
void sres::write_s(const sc_dt::sc_logic& v, int st) {
    sc_core::sc_process_b* p = sc_core::sc_get_current_process_b();
    for (size_t i = 0; i < procs_.size(); i++) {
        if (procs_[i] == p) {
            if (!(vals_[i].first == v) || vals_[i].second != st) {
                vals_[i] = std::make_pair(v, st);
                request_update();
            }
            return;
        }
    }
    procs_.push_back(p);
    vals_.push_back(std::make_pair(v, st));
    request_update();
}

void sres::update() {
    int smax = ST_HIGHZ;
    sc_dt::sc_logic val = sc_dt::SC_LOGIC_Z;
    for (const auto& d : vals_) {
        if (d.first == sc_dt::SC_LOGIC_Z || d.second == ST_HIGHZ) continue;
        if (d.second > smax) {
            smax = d.second;
            val = d.first;
        } else if (d.second == smax && !(d.first == val)) {
            val = sc_dt::SC_LOGIC_X;
        }
    }
    m_new_val = val;
    if (smax != str_) {
        str_ = smax;
        str_ev_.notify(sc_core::SC_ZERO_TIME);
    }
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_MANY_WRITERS>::update();
}

// ------------------------------------------------------------------ UDP
static unsigned char lvl_mask(char c) {
    switch (c) {
        case '0': return 1;
        case '1': return 2;
        case 'x': case 'X': return 4;
        case '?': return 7;
        case 'b': case 'B': return 3;
    }
    return 0;
}

udp::udp(const char* name, int nin, bool seq, const char* table) : name_(name), nin_(nin), seq_(seq) {
    // parse rows: tokens separated by ';'
    std::string t(table);
    size_t pos = 0;
    while (pos < t.size()) {
        size_t e = t.find(';', pos);
        if (e == std::string::npos) break;
        std::string r = t.substr(pos, e - pos);
        pos = e + 1;
        // collect symbols
        std::vector<std::string> sym;
        for (size_t i = 0; i < r.size(); i++) {
            char c = r[i];
            if (isspace((unsigned char)c)) continue;
            if (c == '(') {
                size_t k = r.find(')', i);
                std::string s;
                for (size_t j = i + 1; j < k; j++) if (!isspace((unsigned char)r[j])) s += r[j];
                sym.push_back("(" + s);
                i = k;
                continue;
            }
            sym.push_back(std::string(1, c));
        }
        if (sym.empty()) continue;
        row rw;
        rw.edge_in = -1; rw.ef = rw.et = 0; rw.st = 7;
        int idx = 0;
        size_t si = 0;
        for (; si < sym.size() && idx < nin_; si++) {
            const std::string& s = sym[si];
            if (s == ":") continue;
            unsigned char m = 0;
            if (s[0] == '(') {
                rw.edge_in = idx;
                rw.ef = lvl_mask(s[1]);
                rw.et = lvl_mask(s[2]);
                m = 7;
            } else {
                char c = s[0];
                switch (c) {
                    case 'r': case 'R': rw.edge_in = idx; rw.ef = 1; rw.et = 2; m = 7; break;
                    case 'f': case 'F': rw.edge_in = idx; rw.ef = 2; rw.et = 1; m = 7; break;
                    case 'p': case 'P': rw.edge_in = idx; rw.ef = 1 | 4; rw.et = 2 | 4; m = 7; break;
                    case 'n': case 'N': rw.edge_in = idx; rw.ef = 2 | 4; rw.et = 1 | 4; m = 7; break;
                    case '*': rw.edge_in = idx; rw.ef = 7; rw.et = 7; m = 7; break;
                    default: m = lvl_mask(c);
                }
            }
            rw.lvl.push_back(m);
            idx++;
        }
        // remaining: [':' state] ':' out
        std::vector<std::string> rest;
        for (; si < sym.size(); si++) if (sym[si] != ":") rest.push_back(sym[si]);
        if (seq_ && rest.size() >= 2) {
            rw.st = lvl_mask(rest[0][0]);
            rest.erase(rest.begin());
        }
        char o = rest.empty() ? 'x' : rest[0][0];
        rw.out = o == '0' ? 0 : o == '1' ? 1 : o == '-' ? -1 : 3;
        if (rw.edge_in >= 0) edge_rows_.push_back(rw);
        else level_rows_.push_back(rw);
    }
}

static inline unsigned char vmask(int v) { return v == 0 ? 1 : v == 1 ? 2 : 4; }

int udp::eval_comb(const int* in) const {
    for (const row& r : level_rows_) {
        bool ok = true;
        for (int i = 0; i < nin_ && ok; i++) ok = (r.lvl[i] & vmask(in[i])) != 0;
        if (ok) return r.out < 0 ? 3 : r.out;
    }
    return 3;
}

int udp::eval_seq(const int* in, int k, int from, int st) const {
    unsigned char sm = vmask(st);
    for (const row& r : level_rows_) {
        if (!(r.st & sm)) continue;
        bool ok = true;
        for (int i = 0; i < nin_ && ok; i++) ok = (r.lvl[i] & vmask(in[i])) != 0;
        if (ok) return r.out < 0 ? st : r.out;
    }
    unsigned char fm = vmask(from), tm = vmask(in[k]);
    for (const row& r : edge_rows_) {
        if (r.edge_in != k) continue;
        if (!(r.st & sm)) continue;
        if (!(r.ef & fm) || !(r.et & tm)) continue;
        bool ok = true;
        for (int i = 0; i < nin_ && ok; i++) if (i != k) ok = (r.lvl[i] & vmask(in[i])) != 0;
        if (ok) return r.out < 0 ? st : r.out;
    }
    return 3;
}

}  // namespace vl
