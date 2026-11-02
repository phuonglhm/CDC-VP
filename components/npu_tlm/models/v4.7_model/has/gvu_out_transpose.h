// gvu_out_transpose.h -- corner turn on the int8 output of the GEMM_FUSED epilogue (HAS_OUT_TRANSPOSE=1; proposal of the model, NOT
// in the hardware RTL drop). The epilogue gives the results of one context as one beat per output channel, the lanes being the
// positions of the context ([channel][position], the order the PSM scans the array). The corner turn keeps two banks of W x W int8
// (ping-pong): one bank takes the beats of a context, the other gives back one word per position, the lanes being the channels
// ([position][channel]). The write-back of a token-major channel range (GEMM_FUSED X_TMAJ_C0) then stores rows of channels.
// Timing, one edge per call of edge(): a beat enters the fill bank at the edge it is offered; the last beat of a context turns the
// bank over when the drain bank is empty, else the beat waits (back-pressure, edge() returns false); the drain bank gives one word
// per edge, starting at the edge after the turn. A context of n channels and p positions takes n edges in and p edges out.
#ifndef HAS_GVU_OUT_TRANSPOSE_H
#define HAS_GVU_OUT_TRANSPOSE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace has
{
    template <int W = 32>
    class GvuOutTranspose
    {
    public:
        struct Beat { uint32_t ch{0}; uint32_t npos{0}; bool last{false}; std::array<int8_t, W> lane{}; };   // npos valid lanes
        struct Word { uint32_t ctx{0}; uint32_t pos{0}; uint32_t nch{0}; std::array<int8_t, W> lane{}; };      // nch valid lanes

        // One edge. `in` (may be null) is the beat offered at this edge; returns false when it was not taken (offer it again at
        // the next edge). A word given out at this edge is appended to `out`.
        bool edge(const Beat *in, std::vector<Word> &out)
        {
            edges_++;
            if (drain_left_ > 0)
            {
                const Bank &d = bank_[1 - fill_];
                Word w;
                w.ctx = drain_ctx_;
                w.pos = d.npos - drain_left_;
                w.nch = d.nch;
                for (uint32_t c = 0; c < d.nch; c++) w.lane[c] = d.v[c][w.pos];
                out.push_back(w);
                drain_left_--;
            }
            if (!in) return true;
            if (in->last && drain_left_ > 0) { stalls_++; return false; }
            Bank &f = bank_[fill_];
            if (in->ch < uint32_t(W)) f.v[in->ch] = in->lane;
            f.nch = std::max(f.nch, std::min<uint32_t>(in->ch + 1, W));
            f.npos = std::min<uint32_t>(in->npos, W);
            if (in->last)
            {
                drain_ctx_ = fill_ctx_++;
                drain_left_ = f.npos;
                fill_ = 1 - fill_;
                bank_[fill_].nch = 0;
            }
            return true;
        }
        // Edges after the last beat of a tile of npos positions in contexts of per_ctx positions = the words of its last context.
        static uint32_t tail_edges(uint32_t npos, uint32_t per_ctx)
        {
            const uint32_t pc = std::max<uint32_t>(1, std::min<uint32_t>(per_ctx, W));
            return npos ? (npos - 1) % pc + 1 : 0;
        }
        bool idle() const { return drain_left_ == 0; }
        uint64_t edges() const { return edges_; }
        uint64_t stalls() const { return stalls_; }

        // A whole tile [nch][npos] int8 (channel-major, as the epilogue writes it) through the corner turn, contexts of `per_ctx`
        // positions, one beat offered per edge. Returns [npos][nch]; sets the edges from the first beat to the last word and the
        // edges after the last beat was taken (what a write-back behind the epilogue waits for).
        static std::vector<int8_t> run(const std::vector<int8_t> &cm, uint32_t nch, uint32_t npos, uint32_t per_ctx,
                                       uint64_t *edges = nullptr, uint64_t *tail = nullptr)
        {
            GvuOutTranspose t;
            std::vector<int8_t> tm(size_t(nch) * npos, 0);
            std::vector<Word> out;
            uint64_t last_in = 0;
            const uint32_t pc = std::max<uint32_t>(1, std::min<uint32_t>(per_ctx, W));
            for (uint32_t p0 = 0; p0 < npos; p0 += pc)
                for (uint32_t c = 0; c < nch; c++)
                {
                    Beat b;
                    b.ch = c;
                    b.npos = std::min(pc, npos - p0);
                    b.last = c + 1 == nch;
                    for (uint32_t k = 0; k < b.npos; k++) b.lane[k] = cm[size_t(c) * npos + p0 + k];
                    while (!t.edge(&b, out)) {}
                    last_in = t.edges();
                }
            while (!t.idle()) t.edge(nullptr, out);
            for (const Word &w : out)
                for (uint32_t c = 0; c < w.nch && c < nch; c++) tm[(size_t(w.ctx) * pc + w.pos) * nch + c] = w.lane[c];
            if (edges) *edges = t.edges();
            if (tail) *tail = t.edges() - last_in;
            return tm;
        }

    private:
        struct Bank { std::array<std::array<int8_t, W>, W> v{}; uint32_t nch{0}, npos{0}; };
        Bank bank_[2];
        int fill_{0};
        uint32_t drain_left_{0}, fill_ctx_{0}, drain_ctx_{0};
        uint64_t edges_{0}, stalls_{0};
    };
} // namespace has

#endif // HAS_GVU_OUT_TRANSPOSE_H
