#include "cabac.h"

// Binarization types
enum BinaType : uint8_t { BINA_FL = 0, BINA_TU = 1, BINA_EG1 = 2, BINA_CREG = 4, BINA_SP = 5 };
Cabac::Cabac(sc_core::sc_module_name name) :
    sc_module(name), mem_socket("mem_socket"), start_socket("start_socket"), out_socket("out_socket") {
    start_socket.register_b_transport(this, &Cabac::b_transport);
}

void Cabac::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND && trans.get_data_length() > 0 && trans.get_data_ptr()) {
        CustomPacket pkt = unpackCustomPacket(trans);
        // update current QP
        qp_ = static_cast<uint8_t>(pkt.qp);

        //read coefficients from memory (source of raw symbols)
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

        std::vector<uint8_t> binstream = binarize(membuf);
        std::vector<Bin> bins = bin_buffer_select(pkt, binstream);

        uint64_t emit_base = 0x20000000ULL | static_cast<uint64_t>(pkt.block_idx);
        std::vector<uint8_t> coded = encode_bins(bins, emit_base);

        CustomPacket outpkt;
        outpkt.cmd = CustomCmd::COEFF;
        outpkt.block_idx = pkt.block_idx;
        outpkt.data = coded;

        std::vector<uint8_t> outbuf = packCustomPacket(outpkt);
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
std::vector<Cabac::Bin> Cabac::bin_buffer_select(const CustomPacket &pkt, const std::vector<uint8_t>& binstream) {
    std::vector<Cabac::Bin> out;
    if (binstream.empty()) return out;

    // Start with a basic mapping
    std::vector<Bin> assigned = assign_contexts(pkt, binstream);

    // Mark sign bits (every 9th bit) as bypass
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

    // If the stored byte looks like a packed context ([6]=mps,[5:0]=state), decode it diCustomtly. Otherwise treat it as an "inittable" entry and compute the initial context.
    if (raw <= 0x7F) {
        mps = (raw >> 6) & 0x1;
        state = raw & 0x3F;
        return {state, mps};
    }

    // Treat `raw` as the inittable value and compute initstate/mps/state
    int init = static_cast<int>(raw & 0xFF);
    int slope = ((init >> 4) * 5) - 45;
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
        if (range > rlps) range -= rlps;
        if (state < 62) state++;
    } else {
        // LPS path: range becomes RLPS, flip MPS occasionally
        range = rlps;
        mps = static_cast<uint8_t>(1 - mps);
        if (state > 0) state--;
    }
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

// Map raw packed binstream bytes into a sequence of `Bin` entries with deterministic context indices derived from the packet's block index
std::vector<Cabac::Bin> Cabac::assign_contexts(const CustomPacket &pkt, const std::vector<uint8_t>& binstream) {
    std::vector<Bin> out;
    if (binstream.empty()) return out;

    const uint32_t ctx_base = static_cast<uint32_t>(pkt.block_idx) * 16u;
    size_t total_bits = binstream.size() * 8u;
    size_t sym_count = (total_bits + 8) / 9; // number of 9-bit symbols (ceil)

    // pCustomompute binarization type and cMax for each symbol using the LUT
    std::vector<std::pair<uint8_t,uint8_t>> sym_meta;
    sym_meta.reserve(sym_count);
    for (size_t s = 0; s < sym_count; ++s) {
        uint32_t ctx = ctx_base + static_cast<uint32_t>(s % 16u);
        uint8_t out_type = 255; uint8_t out_cmax = 0;
        if (ctx < 192) {
            out_type = cabac_bina_type[ctx];
            out_cmax = cabac_bina_cmax[ctx];
            if (out_type == 255) out_type = BINA_FL;
        }
        sym_meta.emplace_back(out_type, out_cmax);
    }

    size_t bit_index = 0;
    for (size_t bidx = 0; bidx < binstream.size(); ++bidx) {
        uint8_t byte = binstream[bidx];
        for (int bit = 0; bit < 8; ++bit) {
            bool v = ((byte >> bit) & 1) != 0;
            size_t sym = bit_index / 9;
            uint32_t ctx = ctx_base + static_cast<uint32_t>(sym % 16u);
            uint8_t type = 0, cmax = 1;
            if (sym < sym_meta.size()) {
                type = sym_meta[sym].first;
                cmax = sym_meta[sym].second;
            }
            out.push_back(Bin{v, ctx, false, type, cmax});
            ++bit_index;
        }
    }

    return out;
}

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

// Encode a sequence of Bin entries using the embedded CABAC tables
std::vector<uint8_t> Cabac::encode_bins(const std::vector<Bin>& bins, uint64_t emit_base_addr) {
    // Use an integer, table-driven CABAC encoder with carry-aware byte emission
    const uint64_t SCALE = (1ULL << 32);
    uint64_t low = 0;
    uint64_t high = SCALE;

    std::vector<uint8_t> stream;

    // buffered emission state
    int buffered_byte = -1; // -1 == empty
    int pending_ff = 0;

    auto emit_pending_byte = [&](uint8_t b) {
        // store/flush using a small carry
        if (buffered_byte < 0) {
            buffered_byte = static_cast<int>(b);
            return;
        }
        if (b == 0xFF) {
            // delay emission of 0xFF bytes (they may be affected by a later carry)
            pending_ff++;
            return;
        }
        int carry = 0;
        if (buffered_byte == 0xFF) {
            carry = 0;
        }
        if (carry) {
            int idx = static_cast<int>(stream.size()) - 1;
            int c = 1;
            while (idx >= 0 && c) {
                uint16_t v = static_cast<uint16_t>(stream[idx]) + static_cast<uint16_t>(c);
                stream[idx] = static_cast<uint8_t>(v & 0xFF);
                c = (v >> 8) & 0x1;
                --idx;
            }
            if (c) {
                // all previous bytes carried out, insert 0x01 at front
                stream.insert(stream.begin(), 1);
            }
        }

        stream.push_back(static_cast<uint8_t>(buffered_byte & 0xFF));

        // flush pending 0xFFs: if a carry occurred they become 0x00, else 0xFF
        for (int i = 0; i < pending_ff; ++i) stream.push_back(carry ? 0x00 : 0xFF);

        buffered_byte = static_cast<int>(b);
        pending_ff = 0;
    };

    // process bins
    for (const Bin &b : bins) {
        bool bit = b.bit;

        if (!b.bypass) {
            auto ctx = read_context(b.ctx_idx);
            uint8_t pstate = ctx.first & 0x3F;
            uint8_t mps = ctx.second & 0x1;

            // compute a 9-bit-ish range value from current interval
            uint64_t interval = high - low;
            uint32_t range9 = static_cast<uint32_t>((interval * 512ULL) >> 32);
            if (range9 >= 511) range9 = 510; // clamp to 9-bit-like max

            uint8_t sel = static_cast<uint8_t>(((range9 & 0xFF) >> 6) & 0x3);
            uint16_t rlps = static_cast<uint16_t>(cabac_rlps[pstate][sel]);

            // compute split point (use same 512 denom as earlier)
            uint64_t split = low + (interval * static_cast<uint64_t>(512 - rlps)) / 512ULL;

            if (bit == static_cast<bool>(mps)) {
                // MPS: keep lower interval
                high = split;
                uint8_t next_state = cabac_next_state_mps[pstate];
                uint8_t next_mps = mps;
                write_context(b.ctx_idx, next_state, next_mps);
            } else {
                // LPS: move low up to split
                low = split;
                bool next_mps_bool = (pstate == 0) ? static_cast<bool>(!mps) : static_cast<bool>(mps);
                uint8_t next_state = cabac_next_state_lps[pstate];
                write_context(b.ctx_idx, next_state, static_cast<uint8_t>(next_mps_bool));
            }

            // emit stable top bytes while possible
            while (((low ^ (high - 1)) >> 24) == 0) {
                uint8_t outb = static_cast<uint8_t>(low >> 24);
                emit_pending_byte(outb);
                low = (low << 8) & 0xFFFFFFFFFFFFFFFFULL;
                high = ((high << 8) | 0xFF) & 0xFFFFFFFFFFFFFFFFULL;
            }
        } else {
            // bypass (equal-probability split)
            uint64_t half = low + ((high - low) >> 1);
            if (!bit) high = half; else low = half;
        }
    }

    if (buffered_byte >= 0) {
        stream.push_back(static_cast<uint8_t>(buffered_byte & 0xFF));
        for (int i = 0; i < pending_ff; ++i) stream.push_back(0xFF);
        buffered_byte = -1;
        pending_ff = 0;
    }

    for (int i = 0; i < 4; ++i) {
        uint8_t outb = static_cast<uint8_t>(low >> 24);
        stream.push_back(outb);
        low = (low << 8) & 0xFFFFFFFFFFFFFFFFULL;
    }

    if (!stream.empty()) emit_bytes_to_mem(emit_base_addr, stream);
    return stream;
}