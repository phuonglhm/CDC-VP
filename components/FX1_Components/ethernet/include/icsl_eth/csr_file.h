// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.csr_file

// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// Register behavior is defined by generated/csr_table.inc. Byte enables
// mask each access. Supported access types are R, RW, W1C, W1P and WC.
// Reserved bits read zero. Range limits apply after byte merging.
// The owner supplies side effects and hardware values through hooks.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace icsl_eth {

enum class CsrAccess { R, RW, W1C, W1P, WC };

struct CsrField {
    const char* name;
    unsigned lsb, width;
    CsrAccess access;
    uint32_t mask() const { return (width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u)) << lsb; }
};

struct CsrReg {
    uint32_t offset;
    const char* name;
    uint32_t reset;
    std::vector<CsrField> fields;
    uint32_t mask_of(CsrAccess a) const;
    uint32_t defined() const;               // union of the field masks
};

class CsrFile {
public:
    struct Write {
        uint32_t offset;
        uint32_t data;                      // bus write data
        uint32_t mask;                      // per-bit byte-enable mask
        uint32_t before, after;             // stored value around the write
        uint32_t pulse;                     // W1P bits written as one
        uint32_t cleared;                   // W1C / WC bits the write cleared
    };
    // Called after every accepted write to a mapped register.
    std::function<void(const Write&)> on_write;
    // Value of the R fields of a register: gets the stored value, returns the
    // full hardware value; only the R field bits are used.
    std::function<uint32_t(uint32_t offset, uint32_t stored)> hw_read;
    // Return false to drop a write (write windows); the owner answers it with
    // an error response.
    std::function<bool(uint32_t offset)> accept_write;
    // Current clock edge (any monotonic stamp, e.g. the simulation time).  When
    // set, a hardware update and a bus write on the same edge resolve as MAS
    // Table 6-47 row 11 in either call order: a sticky bit set by hardware
    // stays 1 against W1C, a WC clear keeps the increments of that edge, and a
    // field stored by hardware keeps the hardware value against an RW write.
    std::function<uint64_t()> now;

    CsrFile();
    void reset();
    // Reset value of `mask` bits taken from the owner's parameters instead of
    // the generated table (parameter-dependent reset values).
    void set_reset(uint32_t offset, uint32_t mask, uint32_t bits) { reset_ovr_[offset] = {mask, bits}; }
    // Limit the RW field of a register to [lo, hi] on every bus write.
    void set_range(uint32_t offset, uint32_t lo, uint32_t hi) { range_[offset] = {lo, hi}; }

    bool mapped(uint32_t offset) const { return regs_.count(offset) != 0; }
    const CsrReg* reg(uint32_t offset) const;
    const std::map<uint32_t, CsrReg>& regs() const { return regs_; }
    uint32_t offset_of(const char* name) const;         // 0xFFFFFFFF if unknown

    // Bus access.  `be_mask` is the per-bit mask from the byte enables.
    uint32_t read(uint32_t offset) const;
    void write(uint32_t offset, uint32_t data, uint32_t be_mask);

    // Hardware side.
    uint32_t value(uint32_t offset) const;              // stored bits
    uint32_t field(uint32_t offset, uint32_t mask) const;
    void hw_set(uint32_t offset, uint32_t bits);        // sticky status set
    void hw_clear(uint32_t offset, uint32_t bits);
    void hw_store(uint32_t offset, uint32_t mask, uint32_t bits);
    void hw_add(uint32_t offset, uint32_t delta);       // counter increment
    // Take back a hw_set of `bits` made on this edge, as if it never happened:
    // the bits return to their value before that set, less any W1C software
    // wrote on this edge (before or after the set).
    void hw_retract(uint32_t offset, uint32_t bits);

private:
    struct Edge {                                       // updates of one edge
        uint64_t at = ~uint64_t(0);
        uint32_t set = 0, stored = 0, added = 0;        // by hardware
        uint32_t prior = 0;                             // `set` bits before their first set
        uint32_t w1c = 0;                               // W1C bits software wrote
    };
    Edge& mark(uint32_t offset);
    const Edge* this_edge(uint32_t offset) const;
    std::map<uint32_t, CsrReg> regs_;
    std::map<uint32_t, uint32_t> val_;
    std::map<uint32_t, Edge> edge_;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> range_;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> reset_ovr_;   // mask, bits
};

}  // namespace icsl_eth
