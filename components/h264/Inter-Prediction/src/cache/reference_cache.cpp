#include <h264/inter/inter_core.h>
#include <algorithm>
#include <limits>

namespace h264::inter {
namespace {
unsigned list_index(List list) {
    if (list != List::L0 && list != List::L1) throw std::invalid_argument("invalid list");
    return list == List::L0 ? 0 : 1;
}
unsigned plane_index(Plane plane) {
    if (plane != Plane::Y && plane != Plane::U && plane != Plane::V)
        throw std::invalid_argument("invalid plane");
    return static_cast<unsigned>(plane);
}
}
void ReferenceCache::invalidate() {
    for (auto& item : entries_) { item = Entry{}; item.incarnation = ++sequence_; }
}
void ReferenceCache::retag(List list, Tag tag, unsigned w, unsigned h) {
    const auto i = list_index(list);
    if (tag.slot >= 3 || w < 16 || h < 16 || w > 4096 || h > 4096 || w % 16 || h % 16)
        throw std::invalid_argument("reference dimensions/slot invalid");
    Entry next;
    next.tag = tag; next.width = w; next.height = h;
    for (unsigned p = 0; p < 3; ++p) {
        const auto count = size_t(w) * h / (p == 0 ? 1 : 4);
        next.planes[p].pixels.resize(count);
        next.planes[p].resident.resize(count);
    }
    next.incarnation = ++sequence_;
    entries_[i] = std::move(next);
}
bool ReferenceCache::matches(Reference ref) const {
    const auto& item = entries_[list_index(ref.list)];
    return item.tag && *item.tag == ref.tag;
}
const ReferenceCache::Entry& ReferenceCache::entry(Reference ref) const {
    const auto& item = entries_[list_index(ref.list)];
    if (!item.tag || !(*item.tag == ref.tag)) throw NotResident("reference tag mismatch");
    return item;
}
uint64_t ReferenceCache::incarnation(Reference ref) const { return entry(ref).incarnation; }
unsigned ReferenceCache::width(Reference ref, Plane p) const {
    plane_index(p); return entry(ref).width / (p == Plane::Y ? 1 : 2);
}
unsigned ReferenceCache::height(Reference ref, Plane p) const {
    plane_index(p); return entry(ref).height / (p == Plane::Y ? 1 : 2);
}
void ReferenceCache::fill(Reference ref, Plane p, unsigned x, unsigned y, unsigned w,
                          unsigned h, const std::vector<uint8_t>& pixels) {
    const unsigned pi = plane_index(p);
    const unsigned stride = width(ref, p), rows = height(ref, p);
    if (!w || !h || x >= stride || y >= rows || w > stride - x || h > rows - y ||
        pixels.size() != size_t(w) * h) throw std::invalid_argument("invalid refill rectangle");
    auto& image = entries_[list_index(ref.list)].planes[pi];
    // Validate the entire transfer before publishing any residency.
    for (unsigned row = 0; row < h; ++row) for (unsigned col = 0; col < w; ++col) {
        const auto at = size_t(y + row) * stride + x + col;
        if (image.resident[at] && image.pixels[at] != pixels[size_t(row) * w + col])
            throw std::logic_error("resident reference pixels are immutable until retag");
    }
    for (unsigned row = 0; row < h; ++row) for (unsigned col = 0; col < w; ++col) {
        const auto at = size_t(y + row) * stride + x + col;
        image.pixels[at] = pixels[size_t(row) * w + col]; image.resident[at] = 1;
    }
}
uint8_t ReferenceCache::sample(Reference ref, Plane p, int x, int y) const {
    const auto& image = entry(ref).planes[plane_index(p)];
    const int w = static_cast<int>(width(ref, p)), h = static_cast<int>(height(ref, p));
    const auto at = size_t(std::clamp(y, 0, h - 1)) * w + std::clamp(x, 0, w - 1);
    if (!image.resident[at]) throw NotResident("support sample not resident");
    return image.pixels[at];
}
void validate_mv(MotionVector mv) {
    if (mv.x < -65536 || mv.x > 65536 || mv.y < -65536 || mv.y > 65536)
        throw std::invalid_argument("MV outside VP range (+/-65536 quarter samples)");
}
void validate_partition(const Partition& p) {
    const bool allowed = (p.width == 4 && (p.height == 4 || p.height == 8)) ||
        (p.width == 8 && (p.height == 4 || p.height == 8 || p.height == 16)) ||
        (p.width == 16 && (p.height == 8 || p.height == 16));
    if (!allowed || p.x >= 16 || p.y >= 16 || p.width > 16 - p.x || p.height > 16 - p.y ||
        p.x % p.width || p.y % p.height) throw std::invalid_argument("invalid partition");
}
uint64_t checked_cost(uint64_t sad, uint64_t rate) {
    if (rate > std::numeric_limits<uint64_t>::max() - sad)
        throw std::overflow_error("SAD plus rate overflows uint64");
    return sad + rate;
}
}
