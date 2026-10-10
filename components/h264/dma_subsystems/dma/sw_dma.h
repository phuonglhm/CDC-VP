// Search-window reference cache, HAS §6.2 / §12.3.
// Geometry policy: window starts at the requested MB and clips at picture edges.
// This configurable functional view is not an assertion about RTL search origin.
#ifndef H264_SW_DMA_H
#define H264_SW_DMA_H
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>
#include "h264_arb.h"
namespace h264 {
enum class RefList : int { List0 = 0, List1 = 1 };
inline constexpr int kRefListCount = 2;
struct SwWindow { uint32_t width = 0, height = 0; };
struct SwPlane {
    uint32_t x = 0, y = 0, width = 0, height = 0;
    std::vector<uint8_t> pixels, valid;
};
class SwDma : public CompletionSink {
public:
    SwDma(H264Arb& arb, const CodedDims& dims, SwWindow win)
        : arb_(arb), dims_(dims), win_(win) {
        validate_dims(dims);
        if (!win.width || !win.height || win.width % 16 || win.height % 16)
            throw std::invalid_argument("sw: window dimensions must be positive multiples of 16");
    }
    void invalidate(RefList l) {
        auto& c = cache(l);
        ++c.generation;
        c.resident = c.failed = false;
        c.pending = 0;
        c.ref_slot = -1;
        c.planes = {};
    }
    void invalidate_all() { invalidate(RefList::List0); invalidate(RefList::List1); }
    bool ready(RefList l) const { return cache(l).resident; }
    void set_ref_slot(RefList l, int slot, uint64_t picture_tag = 0) {
        (void)refm_slot_base(refm_base_, dims_, slot);
        auto& c = cache(l);
        if (c.ref_slot != slot || c.picture_tag != picture_tag) invalidate(l);
        c.ref_slot = slot;
        c.picture_tag = picture_tag;
    }
    void set_refm_base(uint64_t base) {
        if (!base || base >= (1ull << 32)) throw std::invalid_argument("sw: invalid reference base");
        if (base != refm_base_) invalidate_all();
        refm_base_ = base;
    }
    void fill_window(RefList l, uint32_t mb_x, uint32_t mb_y) {
        if (mb_x >= dims_.Wc / 16 || mb_y >= dims_.Hc / 16)
            throw std::out_of_range("sw: MB coordinate");
        auto& c = cache(l);
        if (!refm_base_ || c.ref_slot < 0) throw std::logic_error("sw: no reference region/slot");
        const uint32_t x = mb_x * 16, y = mb_y * 16;
        const uint32_t w = std::min(win_.width, dims_.Wc - x);
        const uint32_t h = std::min(win_.height, dims_.Hc - y);
        if (c.pending && c.planes[0].x == x && c.planes[0].y == y) return;
        ++c.generation; // old queued responses cannot satisfy this refill
        c.resident = c.failed = false;
        c.pending = 0;
        for (uint32_t p = 0; p < 3; ++p) {
            const uint32_t divisor = p ? 2 : 1;
            SwPlane next{x / divisor, y / divisor, w / divisor, h / divisor, {}, {}};
            next.pixels.resize(static_cast<size_t>(next.width) * next.height);
            next.valid.resize(next.pixels.size());
            const auto old = std::move(c.planes[p]);
            // Copy only successfully received overlap, in absolute picture coordinates.
            for (uint32_t row = 0; row < next.height; ++row)
                for (uint32_t col = 0; col < next.width; ++col) {
                    const uint32_t ax = next.x + col, ay = next.y + row;
                    if (ax >= old.x && ay >= old.y && ax - old.x < old.width &&
                        ay - old.y < old.height) {
                        const size_t src = static_cast<size_t>(ay - old.y) * old.width + ax - old.x;
                        const size_t dst = static_cast<size_t>(row) * next.width + col;
                        if (old.valid[src]) {
                            next.valid[dst] = 1;
                            next.pixels[dst] = old.pixels[src];
                        }
                    }
                }
            c.planes[p] = std::move(next);
            auto& plane = c.planes[p];
            for (uint32_t row = 0; row < plane.height; ++row) {
                uint32_t col = 0;
                while (col < plane.width) {
                    const size_t start_row = static_cast<size_t>(row) * plane.width;
                    if (plane.valid[start_row + col]) { ++col; continue; }
                    const uint32_t first = col;
                    while (col < plane.width && !plane.valid[start_row + col]) ++col;
                    issue(l, p, row, first, col - first);
                }
            }
        }
        update_ready(c);
    }
    bool fme_mc_may_start(bool is_b) const {
        return ready(RefList::List0) && (!is_b || ready(RefList::List1));
    }
    bool selected_list_resident(RefList l) const { return ready(l); }
    struct HoldState {
        bool held = false;
        RefList selected = RefList::List0;
        uint32_t phase = 0;
        uint64_t sw_addr = 0;
        uint32_t mb_x = 0, mb_y = 0;
    };
    bool advance_phase(RefList l, uint64_t addr) {
        if (!ready(l)) {
            if (!hold_.held) { hold_.selected = l; hold_.sw_addr = addr; }
            hold_.held = true;
            return false;
        }
        if (hold_.held && (l != hold_.selected || addr != hold_.sw_addr))
            throw std::logic_error("sw: held request metadata changed");
        hold_.held = false;
        ++hold_.phase;
        return true;
    }
    uint32_t phase() const { return hold_.phase; }
    const HoldState& hold_state() const { return hold_; }
    bool failed(RefList l) const { return cache(l).failed; }
    const SwPlane& plane(RefList l, uint32_t p) const {
        if (!ready(l)) throw std::logic_error("sw: pixels not resident");
        return cache(l).planes.at(p);
    }
    void on_done(ClientId client, const DmaResponse& rsp) override {
        auto it = reads_.find(rsp.tag);
        if (client != ClientId::SW || it == reads_.end()) return;
        const Read r = it->second;
        reads_.erase(it);
        auto& c = cache(r.list);
        if (r.generation != c.generation) return;
        if (!rsp.ok || rsp.data.size() != r.bytes) c.failed = true;
        else {
            auto& p = c.planes[r.plane];
            std::copy(rsp.data.begin(), rsp.data.end(), p.pixels.begin() + r.offset);
            std::fill(p.valid.begin() + r.offset, p.valid.begin() + r.offset + r.bytes, 1);
        }
        --c.pending;
        update_ready(c);
    }
private:
    struct Cache {
        bool resident = false, failed = false;
        int ref_slot = -1;
        uint64_t generation = 0, picture_tag = 0;
        uint32_t pending = 0;
        std::array<SwPlane, 3> planes;
    };
    struct Read {
        RefList list; uint64_t generation;
        uint32_t plane; size_t offset; uint32_t bytes;
    };
    static size_t idx(RefList l) {
        const int i = static_cast<int>(l);
        if (i < 0 || i >= 2) throw std::out_of_range("sw: invalid reference list");
        return static_cast<size_t>(i);
    }
    Cache& cache(RefList l) { return cache_[idx(l)]; }
    const Cache& cache(RefList l) const { return cache_[idx(l)]; }
    void update_ready(Cache& c) {
        c.resident = c.pending == 0 && !c.failed;
        for (const auto& p : c.planes)
            c.resident = c.resident && !p.valid.empty() &&
                std::all_of(p.valid.begin(), p.valid.end(), [](uint8_t v) { return v != 0; });
    }
    void issue(RefList l, uint32_t p, uint32_t row, uint32_t col, uint32_t bytes) {
        auto& c = cache(l);
        const auto& plane = c.planes[p];
        const uint64_t base = refm_slot_base(refm_base_, dims_, c.ref_slot);
        const uint32_t x = plane.x + col, y = plane.y + row;
        DmaRequest r;
        r.client = ClientId::SW;
        r.addr = p == 0 ? planar_y_addr(base, dims_, x, y) :
                 p == 1 ? planar_u_addr(base, dims_, x, y) : planar_v_addr(base, dims_, x, y);
        // Byte count avoids assumptions about an overlapping strip's word alignment.
        r.size = TransferSize::B1; r.beats = bytes; r.tag = next_tag_++;
        reads_.emplace(r.tag, Read{l, c.generation, p,
                                  static_cast<size_t>(row) * plane.width + col, bytes});
        ++c.pending;
        arb_.request(r, this);
    }
    H264Arb& arb_;
    CodedDims dims_;
    SwWindow win_;
    std::array<Cache, 2> cache_;
    HoldState hold_;
    uint64_t refm_base_ = 0;
    uint32_t next_tag_ = 0;
    std::map<uint32_t, Read> reads_;
};
}
#endif
