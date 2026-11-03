// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.csr_file

// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
#include "icsl_eth/csr_file.h"

#include <algorithm>

#include <cstring>

namespace icsl_eth {

uint32_t CsrReg::mask_of(CsrAccess a) const {
    uint32_t m = 0;
    for (const auto& f : fields)
        if (f.access == a) m |= f.mask();
    return m;
}

uint32_t CsrReg::defined() const {
    uint32_t m = 0;
    for (const auto& f : fields) m |= f.mask();
    return m;
}

CsrFile::CsrFile() {
    CsrReg* cur = nullptr;
#define CSR_REG(off, nm, rst) \
    cur = &regs_[off];        \
    cur->offset = off;        \
    cur->name = nm;           \
    cur->reset = rst;
#define CSR_FIELD(nm, lsb, width, acc) cur->fields.push_back(CsrField{nm, lsb, width, acc});
#include "generated/csr_table.inc"
#undef CSR_REG
#undef CSR_FIELD
    (void)cur;
    reset();
}

void CsrFile::reset() {
    val_.clear();
    edge_.clear();
    for (const auto& kv : regs_) val_[kv.first] = kv.second.reset & kv.second.defined();
    for (const auto& kv : reset_ovr_) {
        uint32_t& v = val_[kv.first];
        v = (v & ~kv.second.first) | (kv.second.second & kv.second.first);
    }
}

const CsrReg* CsrFile::reg(uint32_t offset) const {
    auto it = regs_.find(offset);
    return it == regs_.end() ? nullptr : &it->second;
}

uint32_t CsrFile::offset_of(const char* name) const {
    for (const auto& kv : regs_)
        if (std::strcmp(kv.second.name, name) == 0) return kv.first;
    return 0xFFFFFFFFu;
}

uint32_t CsrFile::value(uint32_t offset) const {
    auto it = val_.find(offset);
    return it == val_.end() ? 0 : it->second;
}

uint32_t CsrFile::field(uint32_t offset, uint32_t mask) const {
    uint32_t v = value(offset) & mask;
    while (mask && !(mask & 1)) {
        mask >>= 1;
        v >>= 1;
    }
    return v;
}

uint32_t CsrFile::read(uint32_t offset) const {
    const CsrReg* r = reg(offset);
    if (!r) return 0;
    const uint32_t ro = r->mask_of(CsrAccess::R);
    const uint32_t stored = value(offset);
    uint32_t v = stored & ~ro;
    v &= ~r->mask_of(CsrAccess::W1P);                   // pulses read 0
    if (ro) v |= (hw_read ? hw_read(offset, stored) : stored) & ro;
    return v & r->defined();
}

void CsrFile::write(uint32_t offset, uint32_t data, uint32_t be_mask) {
    const CsrReg* r = reg(offset);
    if (!r) return;
    if (accept_write && !accept_write(offset)) return;
    uint32_t& v = val_[offset];
    const uint32_t before = v;
    const uint32_t d = data & be_mask;
    // hardware updates already applied on this edge win (Table 6-47 row 11)
    const Edge* e = this_edge(offset);
    const uint32_t hw_set = e ? e->set : 0, hw_stored = e ? e->stored : 0, hw_added = e ? e->added : 0;
    uint32_t pulse = 0, cleared = 0;
    for (const auto& f : r->fields) {
        const uint32_t m = f.mask();
        switch (f.access) {
        case CsrAccess::RW: {
            const uint32_t w = m & be_mask & ~hw_stored;
            v = (v & ~w) | (d & w);
            break;
        }
        case CsrAccess::R: break;
        case CsrAccess::W1C: {
            const uint32_t c = d & m & ~hw_set;
            cleared |= v & c;
            v &= ~c;
            if (now && (d & m)) mark(offset).w1c |= d & m;
            break;
        }
        case CsrAccess::W1P: pulse |= d & m; break;     // never stored
        case CsrAccess::WC:
            if (be_mask) {                              // restarts at this edge's increments
                cleared |= v & m;
                v = (v & ~m) | (hw_added & m);
            }
            break;
        }
    }
    const auto lim = range_.find(offset);
    if (lim != range_.end()) {
        const uint32_t m = r->mask_of(CsrAccess::RW);
        unsigned sh = 0;
        while (m && !((m >> sh) & 1u)) ++sh;
        const uint32_t x = std::min(std::max((v & m) >> sh, lim->second.first), lim->second.second);
        v = (v & ~m) | ((x << sh) & m);
    }
    if (on_write) on_write(Write{offset, data, be_mask, before, v, pulse, cleared});
}

CsrFile::Edge& CsrFile::mark(uint32_t offset) {
    Edge& e = edge_[offset];
    const uint64_t t = now();
    if (e.at != t) e = Edge{t, 0, 0, 0, 0, 0};
    return e;
}

const CsrFile::Edge* CsrFile::this_edge(uint32_t offset) const {
    if (!now) return nullptr;
    auto it = edge_.find(offset);
    return it != edge_.end() && it->second.at == now() ? &it->second : nullptr;
}

void CsrFile::hw_set(uint32_t offset, uint32_t bits) {
    auto it = val_.find(offset);
    if (it == val_.end()) return;
    if (now) {
        Edge& e = mark(offset);
        const uint32_t first = bits & ~e.set;
        e.prior = (e.prior & ~first) | (it->second & first);
        e.set |= bits;
    }
    it->second |= bits;
}

void CsrFile::hw_retract(uint32_t offset, uint32_t bits) {
    auto it = val_.find(offset);
    const Edge* e = this_edge(offset);
    if (it == val_.end() || !e) return;
    const uint32_t b = bits & e->set;
    const uint32_t keep = e->prior & ~e->w1c & b;
    it->second = (it->second & ~b) | keep;
    edge_[offset].set &= ~b;
}

void CsrFile::hw_clear(uint32_t offset, uint32_t bits) {
    auto it = val_.find(offset);
    if (it == val_.end()) return;
    it->second &= ~bits;
}

void CsrFile::hw_store(uint32_t offset, uint32_t mask, uint32_t bits) {
    auto it = val_.find(offset);
    if (it == val_.end()) return;
    it->second = (it->second & ~mask) | (bits & mask);
    if (now) mark(offset).stored |= mask;
}

void CsrFile::hw_add(uint32_t offset, uint32_t delta) {
    auto it = val_.find(offset);
    if (it == val_.end()) return;
    it->second += delta;
    if (now) mark(offset).added += delta;
}

}  // namespace icsl_eth
