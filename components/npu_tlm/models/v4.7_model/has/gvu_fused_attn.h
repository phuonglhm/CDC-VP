// gvu_fused_attn.h -- GVU FUSED_ATTN (vector-unit specification section 5.5, figure 7), one head, three phases:
//   phase 1  acc_ij = Q_i . K_j - Zk*Qrow_i - Zq*Kcol_j + d*Zq*Zk ; x_ij = Requant(acc, Mqk, TS_qk, Zqk)  (1/sqrt(d) inside Mqk, H7)
//            masked (mask == -128, knob) -> x_ij = -128 ; row max runs while the OC tiles stream (SOFTMAX stage 1)
//   phase 2  SOFTMAX stages 2-3 (has/gvu_softmax.h, log-scale LUT_recip), e forced to 0 on masked positions only with
//            mask_e_zero (H22 proposal; the drawing does not carry the mask to stage 2)
//   phase 3  acc_in = A_i . V_n - Za*Vcol_n [ - Zv*Arow_i + L*Za*Zv only with av_zv_corr, H23 ] ; O = Requant(acc, Mav, TS_av, Zav)
// Qrow / Kcol / Vcol are v32int16 accumulators in the drawing: overflow is counted (H24), not modelled.
// Bit-exact with tools/fe/fe_ref_gvu.py fused_attn at its default knobs (tools/has/tb_gvu_fused_attn.cpp). Functional model;
// the systolic-array part (Q.K^T, A.V) is what the core computes -- timing comes from the core when this is wired into the DFC.
#ifndef HAS_FUSED_ATTN_H
#define HAS_FUSED_ATTN_H

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>
#include "has/gvu_softmax.h"

namespace has
{
    struct AttnParams
    {
        int Zq{0}, Zk{0}, Zv{0}, Zqk{0}, Zav{0};
        int64_t Mqk{1}, Mav{1};
        int TSqk{0}, TSav{0};
        bool asym_zp{true}, mask_e_zero{false}, av_zv_corr{false};
    };

    struct AttnResult
    {
        std::vector<std::vector<int>> X, A, O;
        std::vector<SoftmaxRow> stats;
        uint64_t ovf16{0};
    };

    typedef std::vector<std::vector<int>> Mat;
    typedef std::vector<std::vector<int64_t>> Mat64;
    // dot(X, Y)[i][j] = sum_c X[i][c] * Y[j][c] -- the two systolic-array products (Q.K^T, A.V as A.(V^T)^T). Default = plain loops;
    // the DFC passes one that runs them on the core (HasNpuTop::rce_core). Integer sums, so both give the same numbers.
    typedef std::function<Mat64(const Mat &, const Mat &)> DotFn;
    inline Mat64 dot_cpu(const Mat &X, const Mat &Y)
    {
        Mat64 r(X.size(), std::vector<int64_t>(Y.size(), 0));
        for (size_t i = 0; i < X.size(); i++)
            for (size_t j = 0; j < Y.size(); j++)
            {
                int64_t acc = 0;
                for (size_t c = 0; c < X[i].size(); c++) acc += int64_t(X[i][c]) * Y[j][c];
                r[i][j] = acc;
            }
        return r;
    }

    inline AttnResult fused_attn(const Mat &Q, const Mat &K, const Mat &V, const Mat &mask, const AttnParams &p,
                                 const int32_t *exp_table, const LutIndirect &recip, const Knobs &k, const DotFn &dot = dot_cpu)
    {
        const size_t L = K.size(), D = Q[0].size(), NQ = Q.size();
        const int Zq = p.asym_zp ? p.Zq : 0, Zk = p.asym_zp ? p.Zk : 0, Zv = p.asym_zp ? p.Zv : 0;
        AttnResult r;
        std::vector<int64_t> qrow(NQ, 0), kcol(L, 0);
        for (size_t i = 0; i < NQ; i++) for (size_t c = 0; c < D; c++) qrow[i] += Q[i][c];
        for (size_t j = 0; j < L; j++) for (size_t c = 0; c < D; c++) kcol[j] += K[j][c];
        for (auto v : qrow) r.ovf16 += std::llabs(v) > 32767;
        for (auto v : kcol) r.ovf16 += std::llabs(v) > 32767;
        r.X.assign(NQ, std::vector<int>(L));
        const Mat64 qk = dot(Q, K);
        for (size_t i = 0; i < NQ; i++)
            for (size_t j = 0; j < L; j++)
            {
                int64_t acc = qk[i][j];
                acc += -int64_t(Zk) * qrow[i] - int64_t(Zq) * kcol[j] + int64_t(D) * Zq * Zk;
                const int x = requant(acc, p.Mqk, p.TSqk, p.Zqk, k);
                r.X[i][j] = (mask[i][j] == -128) ? -128 : x;
            }
        SoftmaxCfg sc;
        sc.exp = exp_table;
        sc.log_recip = &recip;
        r.A.resize(NQ);
        for (size_t i = 0; i < NQ; i++)
        {
            std::vector<uint8_t> z(L);
            for (size_t j = 0; j < L; j++) z[j] = mask[i][j] == -128;
            SoftmaxRow row = softmax_row(r.X[i], sc, k, p.mask_e_zero ? &z : nullptr);
            r.A[i].assign(row.a.begin(), row.a.end());
            r.stats.push_back(row);
        }
        const int Za = sc.zp;
        std::vector<int64_t> vcol(D, 0), arow(NQ, 0);
        for (size_t n = 0; n < D; n++) for (size_t j = 0; j < L; j++) vcol[n] += V[j][n];
        for (size_t i = 0; i < NQ; i++) for (size_t j = 0; j < L; j++) arow[i] += r.A[i][j];
        for (auto v : vcol) r.ovf16 += std::llabs(v) > 32767;
        r.O.assign(NQ, std::vector<int>(D));
        Mat Vt(D, std::vector<int>(L));
        for (size_t j = 0; j < L; j++) for (size_t n = 0; n < D; n++) Vt[n][j] = V[j][n];
        const Mat64 av = dot(r.A, Vt);
        for (size_t i = 0; i < NQ; i++)
            for (size_t n = 0; n < D; n++)
            {
                int64_t acc = av[i][n];
                acc -= int64_t(Za) * vcol[n];
                if (p.av_zv_corr) acc += -int64_t(Zv) * arow[i] + int64_t(L) * Za * Zv;
                r.O[i][n] = requant(acc, p.Mav, p.TSav, p.Zav, k);
            }
        return r;
    }
} // namespace has

#endif // HAS_FUSED_ATTN_H
