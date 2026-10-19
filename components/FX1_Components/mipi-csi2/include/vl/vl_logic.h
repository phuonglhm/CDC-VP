// 4-state fixed-width logic value used by the SystemC model.
// Encoding per bit (a,b): 0=(0,0) 1=(1,0) Z=(0,1) X=(1,1).
#ifndef VL_LOGIC_H
#define VL_LOGIC_H

#include <cstdint>
#include <cstring>
#include <cmath>
#include <ostream>
#include <string>
#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif
#include <systemc.h>

namespace vl {

template <int N>
struct logic {
    static_assert(N > 0, "logic width must be positive");
    static constexpr int W = (N + 63) / 64;
    static constexpr int TOPBITS = N - 64 * (W - 1);
    static constexpr uint64_t TOPMASK = TOPBITS == 64 ? ~0ULL : ((1ULL << TOPBITS) - 1);
    uint64_t a[W];
    uint64_t b[W];

    // default: all X (uninitialised reg)
    logic() {
        for (int i = 0; i < W; i++) { a[i] = ~0ULL; b[i] = ~0ULL; }
        a[W - 1] &= TOPMASK; b[W - 1] &= TOPMASK;
    }
    explicit logic(uint64_t v) {
        for (int i = 0; i < W; i++) { a[i] = 0; b[i] = 0; }
        a[0] = v;
        a[W - 1] &= TOPMASK;
    }
    static logic xz(uint64_t av, uint64_t bv) {
        logic r(av);
        r.b[0] = bv;
        r.b[W - 1] &= TOPMASK;
        return r;
    }
    static logic words(std::initializer_list<uint64_t> av, std::initializer_list<uint64_t> bv) {
        logic r(0);
        int i = 0;
        for (uint64_t v : av) { if (i < W) r.a[i] = v; i++; }
        i = 0;
        for (uint64_t v : bv) { if (i < W) r.b[i] = v; i++; }
        r.a[W - 1] &= TOPMASK; r.b[W - 1] &= TOPMASK;
        return r;
    }
    static logic all_x() { return logic(); }
    static logic all_z() { logic r(0); for (int i = 0; i < W; i++) r.b[i] = ~0ULL; r.b[W - 1] &= TOPMASK; return r; }
    static logic ones() { logic r(0); for (int i = 0; i < W; i++) r.a[i] = ~0ULL; r.a[W - 1] &= TOPMASK; return r; }

    // conversions with SystemC types
    logic(const sc_dt::sc_logic& l) {
        static_assert(N == 1, "sc_logic converts to logic<1> only");
        unsigned v = (unsigned)l.value();
        a[0] = v & 1; b[0] = v >> 1;
    }
    template <int M>
    logic(const sc_dt::sc_lv<M>& lv) {
        static_assert(M == N, "sc_lv width mismatch");
        for (int i = 0; i < W; i++) { a[i] = 0; b[i] = 0; }
        const int nw = (N + 31) / 32;
        for (int i = 0; i < nw; i++) {
            a[i / 2] |= (uint64_t)lv.get_word(i) << (32 * (i & 1));
            b[i / 2] |= (uint64_t)lv.get_cword(i) << (32 * (i & 1));
        }
        a[W - 1] &= TOPMASK; b[W - 1] &= TOPMASK;
    }
    sc_dt::sc_logic sl() const {
        static_assert(N == 1, "sl() only for 1-bit values");
        return sc_dt::sc_logic((sc_dt::sc_logic_value_t)((a[0] & 1) | ((b[0] & 1) << 1)));
    }
    sc_dt::sc_lv<N> lv() const {
        sc_dt::sc_lv<N> r;
        const int nw = (N + 31) / 32;
        for (int i = 0; i < nw; i++) {
            r.set_word(i, (sc_dt::sc_digit)(a[i / 2] >> (32 * (i & 1))));
            r.set_cword(i, (sc_dt::sc_digit)(b[i / 2] >> (32 * (i & 1))));
        }
        return r;
    }

    // queries
    bool known() const { for (int i = 0; i < W; i++) if (b[i]) return false; return true; }
    uint64_t u64() const { return a[0]; }
    int64_t s64() const {
        if (N >= 64) return (int64_t)a[0];
        uint64_t v = a[0];
        if ((v >> (N - 1)) & 1) v |= ~0ULL << N;
        return (int64_t)v;
    }
    int get_bit_val(int i) const { return (int)(((a[i >> 6] >> (i & 63)) & 1) | (((b[i >> 6] >> (i & 63)) & 1) << 1)); }
    bool is_one_bit(int i) const { return get_bit_val(i) == 1; }

    // Verilog truth value: 1 if any known 1, else X if any unknown, else 0
    int truth_val() const {
        bool unk = false;
        for (int i = 0; i < W; i++) {
            if (a[i] & ~b[i]) return 1;
            if (b[i]) unk = true;
        }
        return unk ? 3 : 0;
    }
    bool test() const { return truth_val() == 1; }
    logic<1> truth() const;

    // selects
    logic<1> bit(int64_t i) const;
    template <int M> logic<M> slice(int64_t lo) const;
    void set_bit(int64_t i, const logic<1>& v);
    template <int M> void set_slice(int64_t lo, const logic<M>& v);
    template <int M> logic<M> ext() const;    // zero-extend / truncate
    template <int M> logic<M> sext() const;   // sign-extend / truncate

    bool operator==(const logic& o) const {
        for (int i = 0; i < W; i++) if (a[i] != o.a[i] || b[i] != o.b[i]) return false;
        return true;
    }
    bool operator!=(const logic& o) const { return !(*this == o); }

    std::string str() const {
        std::string s;
        s.reserve(N);
        for (int i = N - 1; i >= 0; i--) s += "01zx"[get_bit_val(i)];
        return s;
    }
};

template <int N>
inline std::ostream& operator<<(std::ostream& os, const logic<N>& v) { return os << v.str(); }

// ------------------------------------------------------------------ selects
template <int N>
inline logic<1> logic<N>::bit(int64_t i) const {
    if (i < 0 || i >= N) return logic<1>();
    return logic<1>::xz((a[i >> 6] >> (i & 63)) & 1, (b[i >> 6] >> (i & 63)) & 1);
}

template <int N>
inline logic<1> logic<N>::truth() const {
    int t = truth_val();
    return logic<1>::xz(t & 1, t >> 1);
}

namespace detail {
template <int W>
inline uint64_t getbits(const uint64_t* w, int64_t lo) {
    // 64 bits starting at lo, zero outside [0, 64*W)
    if (lo >= 64 * (int64_t)W || lo <= -64) return 0;
    if (lo < 0) return w[0] << (-lo);
    int64_t wi = lo >> 6, sh = lo & 63;
    uint64_t r = w[wi] >> sh;
    if (sh && wi + 1 < W) r |= w[wi + 1] << (64 - sh);
    return r;
}
}  // namespace detail

template <int N>
template <int M>
inline logic<M> logic<N>::slice(int64_t lo) const {
    logic<M> r(0);
    for (int i = 0; i < logic<M>::W; i++) {
        int64_t p = lo + 64 * i;
        r.a[i] = detail::getbits<W>(a, p);
        r.b[i] = detail::getbits<W>(b, p);
        // bits outside [0,N) are X
        int64_t vlo = p < 0 ? -p : 0;
        int64_t vhi = (int64_t)N - p;
        if (vhi > 64) vhi = 64;
        uint64_t valid = 0;
        if (vhi > vlo) {
            uint64_t hm = vhi >= 64 ? ~0ULL : ((1ULL << vhi) - 1);
            uint64_t lm = vlo >= 64 ? ~0ULL : ((1ULL << vlo) - 1);
            valid = hm & ~lm;
        }
        r.a[i] |= ~valid;
        r.b[i] |= ~valid;
    }
    r.a[logic<M>::W - 1] &= logic<M>::TOPMASK;
    r.b[logic<M>::W - 1] &= logic<M>::TOPMASK;
    return r;
}

template <int N>
inline void logic<N>::set_bit(int64_t i, const logic<1>& v) {
    if (i < 0 || i >= N) return;
    uint64_t m = 1ULL << (i & 63);
    a[i >> 6] = (a[i >> 6] & ~m) | ((v.a[0] & 1) ? m : 0);
    b[i >> 6] = (b[i >> 6] & ~m) | ((v.b[0] & 1) ? m : 0);
}

template <int N>
template <int M>
inline void logic<N>::set_slice(int64_t lo, const logic<M>& v) {
    for (int k = 0; k < M; k++) {
        int64_t q = lo + k;
        if (q < 0 || q >= N) continue;
        uint64_t m = 1ULL << (q & 63);
        uint64_t va = (v.a[k >> 6] >> (k & 63)) & 1, vb = (v.b[k >> 6] >> (k & 63)) & 1;
        a[q >> 6] = (a[q >> 6] & ~m) | (va ? m : 0);
        b[q >> 6] = (b[q >> 6] & ~m) | (vb ? m : 0);
    }
}

template <int N>
template <int M>
inline logic<M> logic<N>::ext() const {
    logic<M> r(0);
    for (int i = 0; i < logic<M>::W && i < W; i++) { r.a[i] = a[i]; r.b[i] = b[i]; }
    r.a[logic<M>::W - 1] &= logic<M>::TOPMASK;
    r.b[logic<M>::W - 1] &= logic<M>::TOPMASK;
    return r;
}

template <int N>
template <int M>
inline logic<M> logic<N>::sext() const {
    logic<M> r = ext<M>();
    if (M > N) {
        int sb = get_bit_val(N - 1);
        uint64_t sa = sb & 1, sbb = sb >> 1;
        for (int k = N; k < M; k++) {
            uint64_t m = 1ULL << (k & 63);
            if (sa) r.a[k >> 6] |= m;
            if (sbb) r.b[k >> 6] |= m;
        }
    }
    return r;
}

// ------------------------------------------------------------------ helpers
constexpr int64_t IDX_X = INT64_MIN / 2;   // unknown index marker (always out of range)

template <int N>
inline int64_t idx(const logic<N>& v, bool sgn) {
    if (!v.known()) return IDX_X;
    for (int i = 1; i < logic<N>::W; i++) if (v.a[i]) return IDX_X;
    return sgn ? v.s64() : (int64_t)(v.u64() & 0x3fffffffffffffffULL);
}
// bit position of Verilog index i in a vector declared [msb:lsb] (asc: msb >= lsb)
inline int64_t ipos(int64_t i, int64_t lsb, bool asc) {
    if (i == IDX_X) return IDX_X;
    return asc ? i - lsb : lsb - i;
}
// low bit position of an indexed part-select [start +: w] / [start -: w]
inline int64_t spos(int64_t start, int64_t w, bool up, int64_t lsb, bool asc) {
    if (start == IDX_X) return IDX_X;
    int64_t lo = up ? start : start - w + 1;
    return asc ? lo - lsb : lsb - (lo + w - 1);
}
template <int N>
inline int clog2(const logic<N>& v) {
    uint64_t x = v.u64();
    int r = 0;
    while ((1ULL << r) < x && r < 64) r++;
    return r;
}

template <int N>
inline logic<N> make_x() { return logic<N>(); }

template <int N>
inline bool any_unknown(const logic<N>& v) { return !v.known(); }

inline logic<1> L0() { return logic<1>(0); }
inline logic<1> L1() { return logic<1>(1); }
inline logic<1> LX() { return logic<1>(); }
inline logic<1> LZ() { return logic<1>::xz(0, 1); }
inline logic<1> lbool(bool v) { return logic<1>(v ? 1 : 0); }

// ------------------------------------------------------------------ bitwise
template <int N>
inline logic<N> operator~(const logic<N>& x) {
    logic<N> r(0);
    for (int i = 0; i < logic<N>::W; i++) { r.b[i] = x.b[i]; r.a[i] = ~x.a[i] | x.b[i]; }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return r;
}
template <int N>
inline logic<N> operator&(const logic<N>& x, const logic<N>& y) {
    logic<N> r(0);
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t x1 = x.a[i] & ~x.b[i], y1 = y.a[i] & ~y.b[i];
        uint64_t x0 = ~x.a[i] & ~x.b[i], y0 = ~y.a[i] & ~y.b[i];
        uint64_t one = x1 & y1, zero = x0 | y0;
        uint64_t unk = ~(one | zero);
        r.a[i] = one | unk; r.b[i] = unk;
    }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK; r.b[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return r;
}
template <int N>
inline logic<N> operator|(const logic<N>& x, const logic<N>& y) {
    logic<N> r(0);
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t x1 = x.a[i] & ~x.b[i], y1 = y.a[i] & ~y.b[i];
        uint64_t x0 = ~x.a[i] & ~x.b[i], y0 = ~y.a[i] & ~y.b[i];
        uint64_t one = x1 | y1, zero = x0 & y0;
        uint64_t unk = ~(one | zero);
        r.a[i] = one | unk; r.b[i] = unk;
    }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK; r.b[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return r;
}
template <int N>
inline logic<N> operator^(const logic<N>& x, const logic<N>& y) {
    logic<N> r(0);
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t unk = x.b[i] | y.b[i];
        r.a[i] = ((x.a[i] ^ y.a[i]) & ~unk) | unk; r.b[i] = unk;
    }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK; r.b[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return r;
}
template <int N>
inline logic<N> xnor(const logic<N>& x, const logic<N>& y) { return ~(x ^ y); }

// ------------------------------------------------------------------ reductions
template <int N>
inline logic<1> rand_(const logic<N>& x) {
    bool unk = false;
    for (int i = 0; i < N; i++) { int v = x.get_bit_val(i); if (v == 0) return L0(); if (v >= 2) unk = true; }
    return unk ? LX() : L1();
}
template <int N>
inline logic<1> ror_(const logic<N>& x) {
    bool unk = false;
    for (int i = 0; i < N; i++) { int v = x.get_bit_val(i); if (v == 1) return L1(); if (v >= 2) unk = true; }
    return unk ? LX() : L0();
}
template <int N>
inline logic<1> rxor_(const logic<N>& x) {
    if (!x.known()) return LX();
    int p = 0;
    for (int i = 0; i < logic<N>::W; i++) p ^= __builtin_parityll(x.a[i]);
    return logic<1>(p);
}
template <int N> inline logic<1> rnand_(const logic<N>& x) { return ~rand_(x); }
template <int N> inline logic<1> rnor_(const logic<N>& x) { return ~ror_(x); }
template <int N> inline logic<1> rxnor_(const logic<N>& x) { return ~rxor_(x); }

// ------------------------------------------------------------------ logical
template <int N>
inline logic<1> lnot(const logic<N>& x) { return ~x.truth(); }
inline logic<1> land(const logic<1>& x, const logic<1>& y) { return x & y; }
inline logic<1> lor(const logic<1>& x, const logic<1>& y) { return x | y; }

// ------------------------------------------------------------------ arithmetic
template <int N>
inline logic<N> operator+(const logic<N>& x, const logic<N>& y) {
    if (!x.known() || !y.known()) return logic<N>();
    logic<N> r(0);
    uint64_t c = 0;
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t s = x.a[i] + y.a[i];
        uint64_t c1 = s < x.a[i];
        uint64_t s2 = s + c;
        uint64_t c2 = s2 < s;
        r.a[i] = s2;
        c = c1 | c2;
    }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return r;
}
template <int N>
inline logic<N> operator-(const logic<N>& x) {
    if (!x.known()) return logic<N>();
    logic<N> r = ~x;
    return r + logic<N>(1);
}
template <int N>
inline logic<N> operator-(const logic<N>& x, const logic<N>& y) {
    if (!x.known() || !y.known()) return logic<N>();
    return x + (-y);
}
template <int N>
inline logic<N> operator*(const logic<N>& x, const logic<N>& y) {
    if (!x.known() || !y.known()) return logic<N>();
    if (N <= 64) return logic<N>(x.a[0] * y.a[0]);
    logic<N> r(0), sh = x;
    for (int i = 0; i < N; i++) {
        if (y.is_one_bit(i)) r = r + sh;
        sh = sh.template slice<N>(-1);
        sh.set_bit(0, L0());
    }
    return r;
}
template <int N>
inline logic<N> operator/(const logic<N>& x, const logic<N>& y) {
    static_assert(N <= 64, "wide division not supported");
    if (!x.known() || !y.known() || y.a[0] == 0) return logic<N>();
    return logic<N>(x.a[0] / y.a[0]);
}
template <int N>
inline logic<N> operator%(const logic<N>& x, const logic<N>& y) {
    static_assert(N <= 64, "wide modulus not supported");
    if (!x.known() || !y.known() || y.a[0] == 0) return logic<N>();
    return logic<N>(x.a[0] % y.a[0]);
}
template <int N>
inline logic<N> sdiv(const logic<N>& x, const logic<N>& y) {
    static_assert(N <= 64, "wide division not supported");
    if (!x.known() || !y.known() || y.a[0] == 0) return logic<N>();
    return logic<N>((uint64_t)(x.s64() / y.s64()));
}
template <int N>
inline logic<N> smod(const logic<N>& x, const logic<N>& y) {
    static_assert(N <= 64, "wide modulus not supported");
    if (!x.known() || !y.known() || y.a[0] == 0) return logic<N>();
    return logic<N>((uint64_t)(x.s64() % y.s64()));
}
template <int N>
inline logic<N> pow_u(const logic<N>& x, const logic<N>& y) {
    static_assert(N <= 64, "wide power not supported");
    if (!x.known() || !y.known()) return logic<N>();
    uint64_t r = 1, bs = x.a[0], e = y.a[0];
    while (e) { if (e & 1) r *= bs; bs *= bs; e >>= 1; }
    return logic<N>(r);
}
template <int N>
inline logic<N> pow_s(const logic<N>& x, const logic<N>& y) {
    static_assert(N <= 64, "wide power not supported");
    if (!x.known() || !y.known()) return logic<N>();
    int64_t bs = x.s64(), e = y.s64();
    if (e < 0) {
        if (bs == 1) return logic<N>(1);
        if (bs == -1) return logic<N>((uint64_t)((e & 1) ? -1 : 1));
        if (bs == 0) return logic<N>();
        return logic<N>(0);
    }
    int64_t r = 1;
    while (e) { if (e & 1) r *= bs; bs *= bs; e >>= 1; }
    return logic<N>((uint64_t)r);
}

// ------------------------------------------------------------------ shifts
template <int N>
inline logic<N> shl(const logic<N>& x, int64_t n) {
    if (n <= 0) return x;
    if (n >= N) return logic<N>(0);
    logic<N> r = x.template slice<N>(-n);
    for (int64_t k = 0; k < n; k++) r.set_bit(k, L0());
    return r;
}
template <int N>
inline logic<N> shr(const logic<N>& x, int64_t n) {
    if (n <= 0) return x;
    if (n >= N) return logic<N>(0);
    logic<N> r = x.template slice<N>(n);
    for (int64_t k = N - n; k < N; k++) r.set_bit(k, L0());
    return r;
}
template <int N>
inline logic<N> ashr(const logic<N>& x, int64_t n) {
    if (n <= 0) return x;
    logic<1> s = x.bit(N - 1);
    if (n >= N) { logic<N> r(0); for (int k = 0; k < N; k++) r.set_bit(k, s); return r; }
    logic<N> r = x.template slice<N>(n);
    for (int64_t k = N - n; k < N; k++) r.set_bit(k, s);
    return r;
}
template <int N, int M>
inline logic<N> shl(const logic<N>& x, const logic<M>& n) {
    if (!n.known()) return logic<N>();
    for (int i = 1; i < logic<M>::W; i++) if (n.a[i]) return logic<N>(0);
    return n.a[0] >= (uint64_t)N ? logic<N>(0) : shl(x, (int64_t)n.a[0]);
}
template <int N, int M>
inline logic<N> shr(const logic<N>& x, const logic<M>& n) {
    if (!n.known()) return logic<N>();
    for (int i = 1; i < logic<M>::W; i++) if (n.a[i]) return logic<N>(0);
    return n.a[0] >= (uint64_t)N ? logic<N>(0) : shr(x, (int64_t)n.a[0]);
}
template <int N, int M>
inline logic<N> ashr(const logic<N>& x, const logic<M>& n) {
    if (!n.known()) return logic<N>();
    for (int i = 1; i < logic<M>::W; i++) if (n.a[i]) return ashr(x, (int64_t)N);
    return ashr(x, n.a[0] >= (uint64_t)N ? (int64_t)N : (int64_t)n.a[0]);
}

// ------------------------------------------------------------------ comparisons
template <int N>
inline logic<1> eq(const logic<N>& x, const logic<N>& y) {
    bool unk = false;
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t kn = ~(x.b[i] | y.b[i]);
        if ((x.a[i] ^ y.a[i]) & kn) return L0();
        if (~kn & (i == logic<N>::W - 1 ? logic<N>::TOPMASK : ~0ULL)) unk = true;
    }
    return unk ? LX() : L1();
}
template <int N>
inline logic<1> ne(const logic<N>& x, const logic<N>& y) { return ~eq(x, y); }
template <int N>
inline logic<1> ceq(const logic<N>& x, const logic<N>& y) { return lbool(x == y); }
template <int N>
inline logic<1> cne(const logic<N>& x, const logic<N>& y) { return lbool(!(x == y)); }

template <int N>
inline int ucmp(const logic<N>& x, const logic<N>& y) {
    for (int i = logic<N>::W - 1; i >= 0; i--) {
        if (x.a[i] < y.a[i]) return -1;
        if (x.a[i] > y.a[i]) return 1;
    }
    return 0;
}
template <int N>
inline int scmp(const logic<N>& x, const logic<N>& y) {
    int sx = x.get_bit_val(N - 1) & 1, sy = y.get_bit_val(N - 1) & 1;
    if (sx != sy) return sx ? -1 : 1;
    return ucmp(x, y);
}
#define VL_REL(name, cmp, expr)                                                 \
    template <int N>                                                            \
    inline logic<1> name(const logic<N>& x, const logic<N>& y) {                \
        if (!x.known() || !y.known()) return LX();                              \
        int c = cmp(x, y);                                                      \
        return lbool(expr);                                                     \
    }
VL_REL(lt, ucmp, c < 0)
VL_REL(le, ucmp, c <= 0)
VL_REL(gt, ucmp, c > 0)
VL_REL(ge, ucmp, c >= 0)
VL_REL(slt, scmp, c < 0)
VL_REL(sle, scmp, c <= 0)
VL_REL(sgt, scmp, c > 0)
VL_REL(sge, scmp, c >= 0)
#undef VL_REL

// case-statement matching
template <int N>
inline bool case_eq(const logic<N>& x, const logic<N>& y) { return x == y; }
template <int N>
inline bool casez_eq(const logic<N>& x, const logic<N>& y) {
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t zx = x.b[i] & ~x.a[i], zy = y.b[i] & ~y.a[i];
        uint64_t care = ~(zx | zy);
        if (((x.a[i] ^ y.a[i]) | (x.b[i] ^ y.b[i])) & care) return false;
    }
    return true;
}
template <int N>
inline bool casex_eq(const logic<N>& x, const logic<N>& y) {
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t care = ~(x.b[i] | y.b[i]);
        if ((x.a[i] ^ y.a[i]) & care) return false;
    }
    return true;
}

// ------------------------------------------------------------------ conditional
template <int N>
inline logic<N> mux(const logic<1>& c, const logic<N>& t, const logic<N>& f) {
    int v = c.get_bit_val(0);
    if (v == 1) return t;
    if (v == 0) return f;
    logic<N> r(0);
    for (int i = 0; i < logic<N>::W; i++) {
        uint64_t same = ~(t.a[i] ^ f.a[i]) & ~t.b[i] & ~f.b[i];
        r.a[i] = (t.a[i] & same) | ~same;
        r.b[i] = ~same;
    }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK; r.b[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return r;
}
inline double mux(const logic<1>& c, double t, double f) {
    int v = c.get_bit_val(0);
    if (v == 1) return t;
    if (v == 0) return f;
    return t == f ? t : 0.0;
}

// ------------------------------------------------------------------ concatenation / replication
template <int N>
inline logic<N> cat(const logic<N>& x) { return x; }
template <int N, int M, typename... R>
inline auto cat(const logic<N>& x, const logic<M>& y, const R&... rest) {
    logic<N + M> r(0);
    r.set_slice(0, y);
    r.set_slice(M, x);
    return cat(r, rest...);
}
template <int K, int N>
inline logic<K * N> rep(const logic<N>& x) {
    logic<K * N> r(0);
    for (int i = 0; i < K; i++) r.set_slice(i * N, x);
    return r;
}

// ------------------------------------------------------------------ real conversion
template <int N>
inline double to_real(const logic<N>& x, bool is_signed) {
    if (!x.known()) return 0.0;
    if (N <= 64) return is_signed ? (double)x.s64() : (double)x.u64();
    double r = 0.0;
    bool neg = is_signed && x.is_one_bit(N - 1);
    logic<N> v = neg ? -x : x;
    for (int i = logic<N>::W - 1; i >= 0; i--) r = r * 18446744073709551616.0 + (double)v.a[i];
    return neg ? -r : r;
}
template <int N>
inline logic<N> from_real(double d) {
    double rr = d >= 0 ? std::floor(d + 0.5) : -std::floor(-d + 0.5);
    if (N <= 64) return logic<N>((uint64_t)(int64_t)rr);
    bool neg = rr < 0;
    double m = neg ? -rr : rr;
    logic<N> r(0);
    for (int i = 0; i < logic<N>::W; i++) {
        double q = std::floor(m / 18446744073709551616.0);
        r.a[i] = (uint64_t)(m - q * 18446744073709551616.0);
        m = q;
    }
    r.a[logic<N>::W - 1] &= logic<N>::TOPMASK;
    return neg ? -r : r;
}
inline double bitstoreal(const logic<64>& x) {
    uint64_t v = x.a[0];
    double d;
    std::memcpy(&d, &v, 8);
    return d;
}
inline logic<64> realtobits(double d) {
    uint64_t v;
    std::memcpy(&v, &d, 8);
    return logic<64>(v);
}

}  // namespace vl

#endif
