#include "cabac.h"
#include "rec_packet.h"
#include <vector>
#include <iostream>
#include <cstdlib>
#include <utility>
#include <algorithm>

Cabac::Cabac(sc_core::sc_module_name name) :
    sc_module(name), mem_socket("mem_socket"), start_socket("start_socket"), out_socket("out_socket") {
    // register the incoming start/command socket
    start_socket.register_b_transport(this, &Cabac::b_transport);
}

void Cabac::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND && trans.get_data_length() > 0 && trans.get_data_ptr()) {
        RecPacket pkt = unpackRecPacket(trans);
        // update current QP for context initialization logic
        qp_ = static_cast<uint8_t>(pkt.qp);

        // STEP 1: read coefficients from memory (source of raw symbols)
        std::vector<uint8_t> membuf(16, 0);
        tlm::tlm_generic_payload mtrans;
        mtrans.set_command(tlm::TLM_READ_COMMAND);
        // simple example address mapping: base 0x10000000 + block_idx
        uint64_t addr = 0x10000000ULL | static_cast<uint64_t>(pkt.block_idx);
        mtrans.set_address(addr);
        mtrans.set_data_ptr(membuf.data());
        mtrans.set_data_length(membuf.size());
        mtrans.set_streaming_width(membuf.size());
        mtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time md = sc_core::SC_ZERO_TIME;
        try {
            mem_socket->b_transport(mtrans, md);
        } catch (...) {
            std::cerr << "Cabac: mem_socket b_transport threw exception\n";
        }
        if (mtrans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "Cabac: mem read failed for addr 0x" << std::hex << addr << std::dec << "\n";
        }

        // STEP 2: binarize coefficients -> packed binstream
        std::vector<uint8_t> binstream = binarize(membuf);

        // STEP 3: bin buffer selection (placeholder or full selector)
        std::vector<uint8_t> selected = bin_buffer_select(binstream);

        // STEP 4: assign contexts to bins
        std::vector<Bin> bins = assign_contexts(pkt, selected);

        // STEP 5: arithmetic encode bins -> produce compressed bytes and write them
        // emit_base_addr chosen arbitrarily for compressed output; may be adjusted
        uint64_t emit_base = 0x20000000ULL | static_cast<uint64_t>(pkt.block_idx);
        std::vector<uint8_t> coded = encode_bins(bins, emit_base);

        // STEP 6: forward compressed bytes via out_socket as a RecPacket
        RecPacket outpkt;
        outpkt.cmd = RecCmd::COEFF;
        outpkt.block_idx = pkt.block_idx;
        outpkt.data = coded;

        std::vector<uint8_t> outbuf = packRecPacket(outpkt);
        tlm::tlm_generic_payload otrans;
        otrans.set_command(tlm::TLM_WRITE_COMMAND);
        otrans.set_address(0);
        otrans.set_data_ptr(outbuf.data());
        otrans.set_data_length(outbuf.size());
        otrans.set_streaming_width(outbuf.size());
        otrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time od = sc_core::SC_ZERO_TIME;
        try {
            out_socket->b_transport(otrans, od);
        } catch (...) {
            std::cerr << "Cabac: out_socket b_transport threw exception\n";
        }
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// Simple placeholder binarization:
// Each input coefficient is encoded as 9 bits: [sign(1)][magnitude(8)]
// Bits are packed LSB-first into output bytes.
std::vector<uint8_t> Cabac::binarize(const std::vector<uint8_t>& coeffs) {
    std::vector<uint8_t> out;
    uint8_t bitbuf = 0;
    int bits_in_buf = 0;

    for (uint8_t raw : coeffs) {
        // Interpret byte as signed value for sign/magnitude
        int val = static_cast<int8_t>(raw);
        uint16_t mag = static_cast<uint16_t>(std::abs(val) & 0xFF);
        uint16_t code = (static_cast<uint16_t>((val < 0) ? 1 : 0) << 8) | mag; // 9-bit code

        for (int b = 0; b < 9; ++b) {
            int bit = (code >> b) & 1;
            bitbuf |= static_cast<uint8_t>(bit << bits_in_buf);
            bits_in_buf++;
            if (bits_in_buf == 8) {
                out.push_back(bitbuf);
                bitbuf = 0;
                bits_in_buf = 0;
            }
        }
    }

    if (bits_in_buf) out.push_back(bitbuf);
    return out;
}

// Expand a packed binstream into a sequence of Bins, assign contexts and
// mark bypass bins (e.g. sign bits). Currently we assume each symbol from
// `binarize()` is 9 bits (8 magnitude + 1 sign, LSB-first). Sign bits are
// marked as bypass so they will not use context/state updates.
std::vector<Cabac::Bin> Cabac::bin_buffer_select(const RecPacket &pkt, const std::vector<uint8_t>& binstream) {
    std::vector<Cabac::Bin> out;
    if (binstream.empty()) return out;

    // Start with a basic mapping (contexts assigned by assign_contexts).
    std::vector<Bin> assigned = assign_contexts(pkt, binstream);

    // Mark sign bits (every 9th bit) as bypass. If future binarizers use
    // different symbol lengths, this logic should be updated accordingly.
    for (size_t i = 0; i < assigned.size(); ++i) {
        Bin b = assigned[i];
        if ((i % 9) == 8) b.bypass = true; // sign bit for each coefficient
        out.push_back(b);
    }

    return out;
}

std::pair<uint8_t, uint8_t> Cabac::read_context(uint32_t ctx_idx) {
    uint8_t mps = 0;
    uint8_t state = 0;

    uint8_t raw = 0;
    tlm::tlm_generic_payload t;
    t.set_command(tlm::TLM_READ_COMMAND);
    uint64_t addr = 0x10000000ULL + static_cast<uint64_t>(ctx_idx);
    t.set_address(addr);
    t.set_data_ptr(&raw);
    t.set_data_length(1);
    t.set_streaming_width(1);
    t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    try {
        mem_socket->b_transport(t, d);
        if (t.get_response_status() != tlm::TLM_OK_RESPONSE) {
            // leave defaults (state=0,mps=0)
            return {state, mps};
        }
    } catch (...) {
        std::cerr << "Cabac: read_context mem_socket exception\n";
        return {state, mps};
    }

    // If the stored byte looks like a packed context ([6]=mps,[5:0]=state),
    // decode it directly. Otherwise treat it as an "inittable" entry and
    // compute the initial context using the same formula as the RTL.
    if (raw <= 0x7F) {
        mps = (raw >> 6) & 0x1;
        state = raw & 0x3F;
        return {state, mps};
    }

    // Treat `raw` as the inittable value and compute initstate/mps/state
    int init = static_cast<int>(raw & 0xFF);
    // slope = (init >> 4) * 5 - 45
    int slope = ((init >> 4) * 5) - 45;
    // offset = ((init & 15) << 3) - 16
    int offset = ((init & 0xF) << 3) - 16;
    int signed_qp = static_cast<int>(qp_);
    int product = slope * signed_qp;
    int product_slope_qp_temp = product >> 4;
    int tmp = product_slope_qp_temp + offset;
    int b = tmp < 1 ? 1 : tmp;
    if (b > 126) b = 126;
    int initstate = b;
    mps = (initstate >= 64) ? 1 : 0;
    state = static_cast<uint8_t>(mps ? (initstate - 64) : (63 - initstate));

    return {state, mps};
}


uint16_t Cabac::rlps_for_state(uint8_t state, uint16_t range) {
    // A small deterministic RLPS heuristic based on `state`.
    // This is not the HM table but gives stable behavior for tests.
    unsigned s = static_cast<unsigned>(state) & 0x3F;
    // derive a small divisor from state (0..7)
    unsigned div = 1 + ((s * 7) >> 6); // maps 0..63 -> 1..8
    uint16_t rlps = std::max<uint16_t>(1, static_cast<uint16_t>(range / div));
    if (rlps >= range) rlps = std::max<uint16_t>(1, range / 4);
    return rlps;
}

// Simplified range update: adjust `range`, `low`, and update context state/mps
// based on whether the observed binary `bin` matched the context's MPS.
void Cabac::range_update(bool bin, uint16_t &range, uint16_t &low, uint8_t &state, uint8_t &mps) {
    uint16_t rlps = rlps_for_state(state, range);

    if (bin == static_cast<bool>(mps)) {
        // MPS path: range shrinks by RLPS
        if (range > rlps) range -= rlps;
        // nudge state towards stronger MPS
        if (state < 62) state++;
    } else {
        // LPS path: range becomes RLPS, flip MPS occasionally
        range = rlps;
        mps = static_cast<uint8_t>(1 - mps);
        if (state > 0) state--;
    }

    // Renormalize: ensure range >= 128 by left-shifting and growing low.
    // In a full implementation this would also emit bits to the output
    // and manage carry; here we only adjust the internal variables.
    while (range < 128) {
        range <<= 1;
        low = static_cast<uint16_t>((low << 1) & 0xFFFF);
    }
}

// Write updated context (pack as [6]=mps, [5:0]=state) back to memory
void Cabac::write_context(uint32_t ctx_idx, uint8_t state, uint8_t mps) {
    uint8_t val = static_cast<uint8_t>(((mps & 0x1) << 6) | (state & 0x3F));
    tlm::tlm_generic_payload t;
    t.set_command(tlm::TLM_WRITE_COMMAND);
    uint64_t addr = 0x10000000ULL + static_cast<uint64_t>(ctx_idx);
    t.set_address(addr);
    t.set_data_ptr(&val);
    t.set_data_length(1);
    t.set_streaming_width(1);
    t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    try {
        mem_socket->b_transport(t, d);
        if (t.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "Cabac: write_context failed for ctx " << ctx_idx << "\n";
        }
    } catch (...) {
        std::cerr << "Cabac: write_context mem_socket exception\n";
    }
}

// Map raw packed binstream bytes into a sequence of `Bin` entries with
// deterministic context indices derived from the packet's block index.
std::vector<Cabac::Bin> Cabac::assign_contexts(const RecPacket &pkt, const std::vector<uint8_t>& binstream) {
    std::vector<Bin> out;
    if (binstream.empty()) return out;

    const uint32_t ctx_base = static_cast<uint32_t>(pkt.block_idx) * 16u;
    size_t bit_index = 0;
    for (size_t bidx = 0; bidx < binstream.size(); ++bidx) {
        uint8_t byte = binstream[bidx];
        for (int bit = 0; bit < 8; ++bit) {
            bool v = ((byte >> bit) & 1) != 0;
            uint32_t ctx = ctx_base + static_cast<uint32_t>(bit_index % 16u);
            out.push_back(Bin{v, ctx, false});
            ++bit_index;
        }
    }

    return out;
}

// Emit data into memory
void Cabac::emit_bytes_to_mem(uint64_t addr, const std::vector<uint8_t>& data) {
    if (data.empty()) return;
    tlm::tlm_generic_payload t;
    t.set_command(tlm::TLM_WRITE_COMMAND);
    t.set_address(addr);
    // tlm expects a non-const data ptr
    uint8_t *ptr = const_cast<uint8_t*>(data.data());
    t.set_data_ptr(ptr);
    t.set_data_length(data.size());
    t.set_streaming_width(data.size());
    t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    try {
        mem_socket->b_transport(t, d);
        if (t.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "Cabac: emit_bytes_to_mem failed for addr 0x" << std::hex << addr << std::dec << "\n";
        }
    } catch (...) {
        std::cerr << "Cabac: emit_bytes_to_mem mem_socket exception\n";
    }
}

// Encode a sequence of Bin entries. This function updates contexts via
// `read_context`/`write_context`, performs the placeholder range update,
// and returns a byte-aligned buffer containing packed output bits.
std::vector<uint8_t> Cabac::encode_bins(const std::vector<Bin>& bins, uint64_t emit_base_addr) {
    // We'll implement a simple arithmetic coder using floating-point
    // intervals. This is not the HM/JM CABAC byte-stream but it uses
    // the same context updates (via read_context/range_update) so
    // state evolution is realistic for testing.

    // coder interval
    double lowf = 0.0;
    double highf = 1.0;

    // internal integer range used by rlps_for_state/range_update
    uint16_t irange = 510;
    uint16_t ilow = 0;

    for (const Bin &b : bins) {
        bool bit = b.bit;

        if (!b.bypass) {
            auto ctx = read_context(b.ctx_idx);
            uint8_t state = ctx.first;
            uint8_t mps = ctx.second;

            // derive an approximation of LPS probability from rlps
            uint16_t rlps = rlps_for_state(state, irange);
            double p_lps = static_cast<double>(rlps) / 512.0; // scale approx
            if (p_lps < 1e-8) p_lps = 1e-8;
            if (p_lps > 1.0 - 1e-8) p_lps = 1.0 - 1e-8;
            double p_mps = 1.0 - p_lps;

            // arithmetic interval update: MPS mapped to lower subinterval
            if (bit == static_cast<bool>(mps)) {
                // MPS: shrink to lower subinterval
                highf = lowf + (highf - lowf) * p_mps;
            } else {
                // LPS: take upper subinterval
                double split = lowf + (highf - lowf) * p_mps;
                lowf = split;
            }

            // update context numeric state (keeps irange in sync)
            range_update(bit, irange, ilow, state, mps);
            write_context(b.ctx_idx, state, mps);
        } else {
            // bypass: encode directly as a bit appended to the interval
            double split = lowf + (highf - lowf) * 0.5;
            if (!b.bit) {
                highf = split;
            } else {
                lowf = split;
            }
        }
    }

    // choose a representative point inside interval and emit 4 bytes
    double mid = (lowf + highf) * 0.5;
    uint32_t code = static_cast<uint32_t>(mid * 4294967296.0);
    std::vector<uint8_t> out(4);
    out[0] = static_cast<uint8_t>((code >> 24) & 0xFF);
    out[1] = static_cast<uint8_t>((code >> 16) & 0xFF);
    out[2] = static_cast<uint8_t>((code >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>((code >> 0) & 0xFF);

    // write encoded bytes to memory
    emit_bytes_to_mem(emit_base_addr, out);
    return out;
}