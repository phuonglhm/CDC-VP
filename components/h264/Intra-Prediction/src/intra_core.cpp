#include <h264/intra/intra_core.h>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace h264::intra {
namespace {
int half(int a, int b) { return (a + b + 1) / 2; }
int quarter(int a, int b, int c) { return (a + 2*b + c + 2) / 4; }
// Defined arithmetic shift for negative plane gradients (C++17 portable).
int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
uint8_t clip(int v) { return static_cast<uint8_t>(std::clamp(v, 0, 255)); }
int vertical_right(int x, int y, const References& r) {
    const auto top = [&](int i) { return i < 0 ? int(r.upper_left) : int(r.top[i]); };
    const auto left = [&](int i) { return i < 0 ? int(r.upper_left) : int(r.left[i]); };
    const int z = 2*x - y;
    if (z >= 0) {
        const int k = x - y/2;
        return z%2 == 0 ? half(top(k-1), top(k))
                        : quarter(top(k-2), top(k-1), top(k));
    }
    if (z == -1) return quarter(r.left[0], r.upper_left, r.top[0]);
    return quarter(left(y-2*x-3), left(y-2*x-2), left(y-2*x-1));
}
}
unsigned Block::size() const {
    switch (kind) {
    case Kind::Luma4x4: return 4;
    case Kind::Luma16x16: return 16;
    case Kind::Chroma8x8: return 8;
    }
    throw std::invalid_argument("invalid intra block kind");
}
std::vector<unsigned> legal_modes(Kind kind, const References& r) {
    const bool both = r.has_top && r.has_left && r.has_upper_left;
    std::vector<unsigned> modes;
    if (kind == Kind::Luma4x4) {
        if (r.has_top) modes.push_back(0);
        if (r.has_left) modes.push_back(1);
        modes.push_back(2);
        if (r.has_top) modes.push_back(3); // Missing E..H replicate D.
        if (both) { modes.push_back(4); modes.push_back(5); modes.push_back(6); }
        if (r.has_top) modes.push_back(7);
        if (r.has_left) modes.push_back(8);
    } else if (kind == Kind::Luma16x16) {
        if (r.has_top) modes.push_back(0);
        if (r.has_left) modes.push_back(1);
        modes.push_back(2);
        if (both) modes.push_back(3);
    } else if (kind == Kind::Chroma8x8) {
        modes.push_back(0);
        if (r.has_left) modes.push_back(1);
        if (r.has_top) modes.push_back(2);
        if (both) modes.push_back(3);
    } else throw std::invalid_argument("invalid intra block kind");
    return modes;
}
std::vector<uint8_t> predict(Kind kind, unsigned mode, const References& original) {
    const auto modes = legal_modes(kind, original);
    if (std::find(modes.begin(), modes.end(), mode) == modes.end())
        throw std::invalid_argument("unavailable intra mode");
    References r = original;
    if (kind == Kind::Luma4x4 && !r.has_upper_right)
        std::fill(r.top.begin()+4, r.top.begin()+8, r.top[3]);
    const unsigned n = kind == Kind::Luma4x4 ? 4 : kind == Kind::Luma16x16 ? 16 : 8;
    const unsigned dc_mode = kind == Kind::Chroma8x8 ? 0 : 2;
    const unsigned vertical = kind == Kind::Chroma8x8 ? 2 : 0;
    int dc = 128, h = 0, v = 0, a = 0, b = 0, c = 0;
    if (mode == dc_mode && kind != Kind::Chroma8x8) {
        int sum = 0, count = 0;
        for (unsigned i = 0; i < n; ++i) {
            if (r.has_top) { sum += r.top[i]; ++count; }
            if (r.has_left) { sum += r.left[i]; ++count; }
        }
        if (count) dc = (sum + count/2) / count;
    }
    if (mode == 3 && kind != Kind::Luma4x4) {
        const int mid = int(n/2)-1;
        for (int i = 1; i <= int(n/2); ++i) {
            h += i*(int(r.top[mid+i]) - (mid-i < 0 ? r.upper_left : r.top[mid-i]));
            v += i*(int(r.left[mid+i]) - (mid-i < 0 ? r.upper_left : r.left[mid-i]));
        }
        a = 16*(int(r.top[n-1]) + int(r.left[n-1]));
        b = n == 16 ? floor_div(5*h+32,64) : floor_div(17*h+16,32);
        c = n == 16 ? floor_div(5*v+32,64) : floor_div(17*v+16,32);
    }
    std::vector<uint8_t> out(n*n);
    for (unsigned y = 0; y < n; ++y) for (unsigned x = 0; x < n; ++x) {
        int value = dc;
        if (mode == vertical) value = r.top[x];
        else if (mode == 1) value = r.left[y];
        else if (mode == dc_mode && kind == Kind::Chroma8x8) {
            const unsigned bx = x/4, by = y/4;
            int sum = 0, count = 0;
            // H.264 chroma DC has four independent 4x4 averages.
            const bool use_top = r.has_top && (!r.has_left || bx == by || by == 0);
            const bool use_left = r.has_left && (!r.has_top || bx == by || bx == 0);
            for (unsigned i = 0; i < 4; ++i) {
                if (use_top) { sum += r.top[4*bx+i]; ++count; }
                if (use_left) { sum += r.left[4*by+i]; ++count; }
            }
            value = count ? (sum+count/2)/count : 128;
        } else if (mode == 3 && kind != Kind::Luma4x4) {
            value = floor_div(a+b*(int(x)-int(n/2)+1)+c*(int(y)-int(n/2)+1)+16,32);
        } else if (kind == Kind::Luma4x4) {
            const int xx = int(x), yy = int(y);
            if (mode == 3) {
                const unsigned k = x+y;
                value = quarter(r.top[k], r.top[k+1], r.top[std::min(k+2,7u)]);
            } else if (mode == 4) {
                // Signed index across left -> upper-left -> top.
                const auto edge = [&](int i) {
                    return i == 0 ? int(r.upper_left) : i > 0 ? int(r.top[i-1]) : int(r.left[-i-1]);
                };
                const int d = xx-yy;
                value = quarter(edge(d-1),edge(d),edge(d+1));
            } else if (mode == 5) value = vertical_right(xx,yy,r);
            else if (mode == 6) {
                References transposed = r;
                transposed.top = r.left; transposed.left = r.top;
                value = vertical_right(yy,xx,transposed);
            } else if (mode == 7) {
                const unsigned k = x+y/2;
                value = y%2 == 0 ? half(r.top[k],r.top[k+1])
                                 : quarter(r.top[k],r.top[k+1],r.top[k+2]);
            } else if (mode == 8) {
                const unsigned k = y+x/2;
                const auto l = [&](unsigned i) { return r.left[std::min(i,3u)]; };
                value = x%2 == 0 ? half(l(k),l(k+1)) : quarter(l(k),l(k+1),l(k+2));
            }
        }
        out[y*n+x] = clip(value);
    }
    return out;
}
uint64_t distortion(const std::vector<int16_t>& residual, unsigned n, Metric metric) {
    if (!n || n%4 || residual.size() != size_t(n)*n)
        throw std::invalid_argument("invalid residual dimensions");
    uint64_t total = 0;
    if (metric == Metric::Sad) {
        for (int v : residual) total += unsigned(std::abs(v));
        return total;
    }
    if (metric != Metric::Satd) throw std::invalid_argument("invalid cost metric");
    for (unsigned by = 0; by < n; by += 4) for (unsigned bx = 0; bx < n; bx += 4) {
        int tmp[4][4];
        for (unsigned y = 0; y < 4; ++y) {
            const int p = residual[(by+y)*n+bx], q = residual[(by+y)*n+bx+1];
            const int r = residual[(by+y)*n+bx+2], s = residual[(by+y)*n+bx+3];
            tmp[y][0]=p+q+r+s; tmp[y][1]=p+q-r-s;
            tmp[y][2]=p-q-r+s; tmp[y][3]=p-q+r-s;
        }
        uint64_t sum = 0;
        for (unsigned x = 0; x < 4; ++x) {
            const int p=tmp[0][x], q=tmp[1][x], r=tmp[2][x], s=tmp[3][x];
            sum += std::abs(p+q+r+s)+std::abs(p+q-r-s)+std::abs(p-q-r+s)+std::abs(p-q+r-s);
        }
        total += (sum+1)/2;
    }
    return total;
}
void Core::reset() {
    ++generation_; sequence_ = 0; active_ = false; pending_.reset();
    for (auto& m : memory_) m = Memory{};
}
void Core::start_frame(unsigned w, unsigned h) {
    if (!w || !h || w%16 || h%16 || w > 4096 || h > 4096)
        throw std::invalid_argument("coded dimensions must be MB aligned, 16..4096");
    reset();
    for (unsigned i = 0; i < 3; ++i) {
        auto& m = memory_[i];
        m.width = i == 0 ? w : w/2; m.height = i == 0 ? h : h/2;
        m.pixels.assign(size_t(m.width)*m.height,128);
        m.valid.assign(m.pixels.size(),0);
        m.modes.assign(m.pixels.size(),-1);
    }
    active_ = true;
}
const Core::Memory& Core::memory(const Block& block) const {
    const unsigned i = static_cast<unsigned>(block.plane);
    if (i >= memory_.size()) throw std::invalid_argument("invalid plane");
    return memory_[i];
}
Core::Memory& Core::memory(const Block& block) {
    return const_cast<Memory&>(static_cast<const Core&>(*this).memory(block));
}
void Core::validate(const Block& block) const {
    if (!active_) throw std::logic_error("no active intra frame");
    const auto& m = memory(block);
    const unsigned n = block.size();
    if ((block.kind == Kind::Chroma8x8) != (block.plane != Plane::Y))
        throw std::invalid_argument("block kind does not match plane");
    if (block.x%n || block.y%n || block.x > m.width-n || block.y > m.height-n)
        throw std::invalid_argument("block alignment or picture boundary");
}
References Core::resolve(const Block& block) const {
    validate(block);
    const auto& m = memory(block);
    const unsigned n = block.size();
    References r;
    r.top.fill(128); r.left.fill(128);
    const auto available = [&](int x, int y) {
        return x >= 0 && y >= 0 && unsigned(x) < m.width && unsigned(y) < m.height && m.valid[size_t(y)*m.width+x];
    };
    const auto pixel = [&](unsigned x, unsigned y) { return m.pixels[size_t(y)*m.width+x]; };
    r.has_top = block.y != 0; r.has_left = block.x != 0;
    for (unsigned i = 0; i < n; ++i) {
        r.has_top = r.has_top && available(int(block.x+i),int(block.y)-1);
        r.has_left = r.has_left && available(int(block.x)-1,int(block.y+i));
    }
    if (r.has_top) for (unsigned i=0;i<n;++i) r.top[i]=pixel(block.x+i,block.y-1);
    if (r.has_left) for (unsigned i=0;i<n;++i) r.left[i]=pixel(block.x-1,block.y+i);
    r.has_upper_left=available(int(block.x)-1,int(block.y)-1);
    if (r.has_upper_left) r.upper_left=pixel(block.x-1,block.y-1);
    if (block.kind == Kind::Luma4x4) {
        const unsigned bx=block.x%16/4, by=block.y%16/4;
        const unsigned scan=8*(by/2)+4*(bx/2)+2*(by%2)+bx%2;
        const bool order_allows = scan!=3 && scan!=7 && scan!=11 && scan!=13 && scan!=15;
        r.has_upper_right = r.has_top && order_allows;
        for (unsigned i=4;i<8;++i)
            r.has_upper_right = r.has_upper_right && available(int(block.x+i),int(block.y)-1);
        for (unsigned i=4;i<8;++i)
            r.top[i]=r.has_upper_right ? pixel(block.x+i,block.y-1) : r.top[3];
    }
    return r;
}
const Decision& Core::evaluate(const Block& block, const std::vector<uint8_t>& source,
                               Metric metric, const std::array<uint32_t,9>& penalty) {
    validate(block);
    if (pending_) throw std::logic_error("reconstruction pending");
    const unsigned n=block.size();
    if (source.size()!=size_t(n)*n) throw std::invalid_argument("source sample count");
    Decision result;
    result.block=block; result.references=resolve(block);
    result.token={generation_,++sequence_};
    result.cost=std::numeric_limits<uint64_t>::max();
    for (unsigned mode : legal_modes(block.kind,result.references)) {
        auto pixels=predict(block.kind,mode,result.references);
        std::vector<int16_t> residual(source.size());
        for (size_t i=0;i<source.size();++i) residual[i]=int(source[i])-int(pixels[i]);
        const uint64_t d=distortion(residual,n,metric), cost=d+penalty[mode];
        result.candidates.push_back({mode,d,cost});
        if (cost < result.cost) { // Strict: keep first candidate on ties.
            result.cost=cost; result.mode=mode;
            result.predictor=std::move(pixels); result.residual=std::move(residual);
        }
    }
    pending_=std::move(result);
    return *pending_;
}
const Decision& Core::replay(Token token) const {
    if (!pending_ || !(pending_->token==token)) throw std::logic_error("stale intra token");
    return *pending_;
}
void Core::store(const Block& block, const std::vector<uint8_t>& pixels, int mode) {
    validate(block);
    const unsigned n=block.size();
    if (pixels.size()!=size_t(n)*n) throw std::invalid_argument("decoded sample count");
    auto& m=memory(block);
    for (unsigned y=0;y<n;++y) for (unsigned x=0;x<n;++x) {
        const size_t i=size_t(block.y+y)*m.width+block.x+x;
        m.pixels[i]=pixels[y*n+x]; m.valid[i]=1;
        m.modes[i]=mode<0 ? -1 : mode+16*int(block.kind);
    }
}
void Core::reconstruct(Token token, const std::vector<uint8_t>& pixels) {
    const auto& d=replay(token);
    store(d.block,pixels,int(d.mode)); pending_.reset();
}
void Core::import_reconstructed(const Block& block, const std::vector<uint8_t>& pixels) {
    if (pending_) throw std::logic_error("cannot change replay references while pending");
    store(block,pixels,-1);
}
std::optional<unsigned> Core::committed_mode(const Block& block) const {
    validate(block);
    const auto& m=memory(block);
    const int mode=m.modes[size_t(block.y)*m.width+block.x];
    if (mode<0 || mode/16!=int(block.kind)) return std::nullopt;
    const unsigned n=block.size();
    for (unsigned y=0;y<n;++y) for (unsigned x=0;x<n;++x) {
        const size_t i=size_t(block.y+y)*m.width+block.x+x;
        if (!m.valid[i] || m.modes[i]!=mode) return std::nullopt;
    }
    return unsigned(mode%16);
}
}
