#include "rec_intra.h"
#include <algorithm>
#include <iostream>

RecIntra::RecIntra(sc_core::sc_module_name name) : 
    sc_module(name), buffer_socket("buffer_socket"), start_socket("start_socket") {
    start_socket.register_b_transport(this, &RecIntra::b_transport);  
}

void RecIntra::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    // Parse incoming transaction into a typed RecPacket (backward-compatible)
    auto len = trans.get_data_length();
    auto ptr = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    if (len >= 8 && ptr) {
        RecPacket in = unpackRecPacket(ptr, len);
        if (in.cmd == RecCmd::READ_REQ && mem_if) {
            // Convert 4x4 block coords to pixel coords (caller may already be pixels)
            uint32_t px = static_cast<uint32_t>(in.x) * 4u;
            uint32_t py = static_cast<uint32_t>(in.y) * 4u;
            RecPlane plane = (in.sel <= 2) ? static_cast<RecPlane>(in.sel) : RecPlane::Y;

            // Anchor at (px-1,py-1) so the returned RefBlock contains ref_tl,
            // top[0..N-1] at row 0 cols 1..N and left[0..N-1] at col 0 rows 1..N.
            uint32_t ax = px - 1u;
            uint32_t ay = py - 1u;
            RefBlock win;
            if (getRefBlock(plane, ax, ay, in.size, PaddingMode::EDGE, win)) {
                std::vector<uint8_t> pred;
                // Dispatch by pred_type/mode
                if (static_cast<PredType>(in.pred_type) == PredType::INTRA) {
                    if (in.mode == 1) {
                        std::cout << "Using DC Mode" << std::endl;
                        DC_mode(win, in.size, in.pre_sel, static_cast<uint32_t>(in.x), static_cast<uint32_t>(in.y), pred);
                    } else if (in.mode == 0) {
                        std::cout << "Using Planar Mode" << std::endl;
                        planar_mode(win, in.size, static_cast<uint32_t>(in.x), static_cast<uint32_t>(in.y), pred);
                    } else {
                        // default fallback to DC
                        DC_mode(win, in.size, in.pre_sel, static_cast<uint32_t>(in.x), static_cast<uint32_t>(in.y), pred);
                    }
                } else {
                    // Non-INTRA engines not implemented yet; return empty PRE
                    pred.assign(16, 0);
                }

                RecPacket pkt;
                pkt.cmd = RecCmd::PRE;
                pkt.block_idx = in.block_idx;
                pkt.x = in.x;
                pkt.y = in.y;
                pkt.size = in.size;
                pkt.sel = in.sel;
                pkt.qp = in.qp;
                pkt.type = in.type;
                pkt.pred_type = in.pred_type;
                pkt.mode = in.mode;
                pkt.pre_sel = in.pre_sel;
                pkt.i4x4_x = in.i4x4_x;
                pkt.i4x4_y = in.i4x4_y;
                pkt.data = std::move(pred);

                std::vector<uint8_t> outbuf = packRecPacket(pkt);

                std::cout << "-------------RecIntra Packet (internal)--------------" << std::endl;
                std::cout << pkt << std::endl;

                // Create local TLM transaction and forward synchronously
                tlm::tlm_generic_payload new_trans;
                new_trans.set_command(tlm::TLM_WRITE_COMMAND);
                new_trans.set_address(0);
                new_trans.set_data_ptr(outbuf.data());
                new_trans.set_data_length(outbuf.size());
                sc_core::sc_time d = sc_core::SC_ZERO_TIME;

                buffer_socket->b_transport(new_trans, d);
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }
            // else fall through and forward original transaction
        }
    }
    // Default behaviour: forward the incoming transaction unchanged
    buffer_socket->b_transport(trans, delay);
}

void RecIntra::bindMemory(RecMemoryIf &mem) {
    mem_if = &mem;
}

bool RecIntra::getRefBlock(RecPlane plane, uint32_t x, uint32_t y, uint8_t size4x4, PaddingMode pad, RefBlock &out) {
    if (mem_if) return mem_if->getRefBlock(plane, x, y, size4x4, pad, out);
    return false;
}

bool RecIntra::fetchRefAsVector(RecPlane plane,
                                uint32_t x,
                                uint32_t y,
                                uint8_t size4x4,
                                PaddingMode pad,
                                std::vector<uint8_t> &out)
{
    RefBlock blk;
    if (!getRefBlock(plane, x, y, size4x4, pad, blk)) return false;
    out = std::move(blk.data);
    return true;
}

void RecIntra::DC_mode(const RefBlock& win,
                       uint8_t size4x4,
                       uint8_t pre_sel,
                       uint32_t i4x4_x,
                       uint32_t i4x4_y,
                       std::vector<uint8_t>& out)
{
    int N = recSizeToPixels(size4x4);
    auto idx = [&](int r, int c) { return static_cast<size_t>(r) * win.stride + static_cast<size_t>(c); };

    int32_t sum_top = 0;
    int32_t sum_left = 0;
    for (int k = 0; k < N; ++k) {
        sum_top += static_cast<int32_t>(win.data[idx(0, 1 + k)]);
        sum_left += static_cast<int32_t>(win.data[idx(1 + k, 0)]);
    }

    int shift = (N == 4 ? 3 : (N == 8 ? 4 : (N == 16 ? 5 : 6)));
    int32_t rounding = 1 << (shift - 1);
    int32_t dc_value = (sum_top + sum_left + rounding) >> shift;
    dc_value = std::clamp(dc_value, 0, 255);

    // Determine the 4x4 sub-block offset inside the N-size block
    uint32_t blocks = static_cast<uint32_t>(N / 4); // 1,2,4,8
    uint32_t subX = (blocks > 1) ? (i4x4_x & (blocks - 1)) : 0u;
    uint32_t subY = (blocks > 1) ? (i4x4_y & (blocks - 1)) : 0u;
    uint32_t startX = subX * 4;
    uint32_t startY = subY * 4;

    // gather top0..top3 and left0..left3 used by RTL DC special cases
    auto read_u8 = [&](int r, int c) -> int32_t {
        size_t pos = idx(r, c);
        if (pos < win.data.size()) return static_cast<int32_t>(win.data[pos]);
        return 0;
    };

    int32_t top0 = read_u8(0, 1 + static_cast<int>(startX + 0));
    int32_t top1 = read_u8(0, 1 + static_cast<int>(startX + 1));
    int32_t top2 = read_u8(0, 1 + static_cast<int>(startX + 2));
    int32_t top3 = read_u8(0, 1 + static_cast<int>(startX + 3));

    int32_t left0 = read_u8(1 + static_cast<int>(startY + 0), 0);
    int32_t left1 = read_u8(1 + static_cast<int>(startY + 1), 0);
    int32_t left2 = read_u8(1 + static_cast<int>(startY + 2), 0);
    int32_t left3 = read_u8(1 + static_cast<int>(startY + 3), 0);

    auto sat8 = [](int32_t v)->uint8_t { if (v < 0) return 0; if (v > 255) return 255; return static_cast<uint8_t>(v); };

    bool is_left_edge = (startX == 0);
    bool is_top_edge = (startY == 0);
    bool size_is_32 = (size4x4 == 3);

    int32_t DC_00, DC_01, DC_02, DC_03;
    int32_t DC_10, DC_20, DC_30;

    // Default DC_* values
    DC_01 = DC_02 = DC_03 = dc_value;
    DC_10 = DC_20 = DC_30 = dc_value;

    // DC_00 special-cases
    if (pre_sel == 0) {
        if (is_left_edge && is_top_edge && !size_is_32) {
            DC_00 = (top0 + left0 + (dc_value << 1) + 2) >> 2;
        } else if (is_left_edge && !size_is_32) {
            DC_00 = (left0 + (dc_value * 3) + 2) >> 2;
        } else if (is_top_edge && !size_is_32) {
            DC_00 = (top0 + (dc_value * 3) + 2) >> 2;
        } else {
            DC_00 = dc_value;
        }
    } else {
        DC_00 = dc_value;
    }

    // First column special-cases (rows 1..3)
    if (is_left_edge && !size_is_32 && pre_sel == 0) {
        DC_10 = (left1 + (dc_value * 3) + 2) >> 2;
        DC_20 = (left2 + (dc_value * 3) + 2) >> 2;
        DC_30 = (left3 + (dc_value * 3) + 2) >> 2;
    }

    // First row special-cases (cols 1..3)
    if (is_top_edge && !size_is_32 && pre_sel == 0) {
        DC_01 = (top1 + (dc_value * 3) + 2) >> 2;
        DC_02 = (top2 + (dc_value * 3) + 2) >> 2;
        DC_03 = (top3 + (dc_value * 3) + 2) >> 2;
    }

    // Clamp and write out 4x4 in row-major: row0..row3
    out.resize(16);
    out[0] = sat8(DC_00);
    out[1] = sat8(DC_01);
    out[2] = sat8(DC_02);
    out[3] = sat8(DC_03);

    out[4] = sat8(DC_10);
    out[5] = sat8(dc_value);
    out[6] = sat8(dc_value);
    out[7] = sat8(dc_value);

    out[8] = sat8(DC_20);
    out[9] = sat8(dc_value);
    out[10] = sat8(dc_value);
    out[11] = sat8(dc_value);

    out[12] = sat8(DC_30);
    out[13] = sat8(dc_value);
    out[14] = sat8(dc_value);
    out[15] = sat8(dc_value);
}

void RecIntra::planar_mode(const RefBlock& win, uint8_t size4x4, uint32_t i4x4_x, uint32_t i4x4_y, std::vector<uint8_t>& out) {
    int N = recSizeToPixels(size4x4);
    auto idx = [&](int r, int c) { return static_cast<size_t>(r) * win.stride + static_cast<size_t>(c); };

    // Read top[0..N-1] (row 0, cols 1..N) and left[0..N-1] (rows 1..N, col 0)
    std::vector<int32_t> top(N), left(N);
    for (int c = 0; c < N; ++c) top[c] = static_cast<int32_t>(win.data[idx(0, 1 + c)]);
    for (int r = 0; r < N; ++r) left[r] = static_cast<int32_t>(win.data[idx(1 + r, 0)]);

    // top-right and bottom-left extras are at (0, N+1) and (N+1, 0)
    int32_t TR = static_cast<int32_t>(win.data[idx(0, N + 1)]);
    int32_t BL = static_cast<int32_t>(win.data[idx(N + 1, 0)]);

    // planar_x[c] = (c+1)*TR, planar_y[r] = (r+1)*BL
    std::vector<int32_t> planar_x(N), planar_y(N);
    for (int c = 0; c < N; ++c) planar_x[c] = (c + 1) * TR;
    for (int r = 0; r < N; ++r) planar_y[r] = (r + 1) * BL;

    int shift = (N == 4 ? 3 : (N == 8 ? 4 : (N == 16 ? 5 : 6)));

    // compute full N x N planar predictions (integer math, clamp 0..255)
    std::vector<int32_t> pred(static_cast<size_t>(N) * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int32_t v = (( (N - 1 - c) * left[r]) + ((N - 1 - r) * top[c]) + planar_x[c] + planar_y[r] + N) >> shift;
            if (v < 0) v = 0;
            else if (v > 255) v = 255;
            pred[static_cast<size_t>(r) * N + c] = v;
        }
    }

    // extract the 4x4 sub-block identified by i4x4_x,i4x4_y inside the N block
    uint32_t blocks = static_cast<uint32_t>(N / 4); // 1,2,4,8
    uint32_t subX = (blocks > 1) ? (i4x4_x & (blocks - 1)) : 0u;
    uint32_t subY = (blocks > 1) ? (i4x4_y & (blocks - 1)) : 0u;
    uint32_t startX = subX * 4;
    uint32_t startY = subY * 4;

    out.resize(16);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[static_cast<size_t>(r) * 4 + c] = static_cast<uint8_t>(pred[static_cast<size_t>(startY + r) * N + (startX + c)]);
        }
    }
}

void RecIntra::angular_mode(const RefBlock& win, uint8_t size4x4, uint8_t mode, uint32_t i4x4_x, uint32_t i4x4_y, std::vector<uint8_t>& out) {
    // Ported logic: pred_angle lookup, fact/idx/ifact, reference indexing and
    // interpolation. Produces a 4x4 block written to `out` (clamped 0..255).

    int N = recSizeToPixels(size4x4);
    auto at = [&](int r, int c) -> size_t { return static_cast<size_t>(r) * win.stride + static_cast<size_t>(c); };

    // compute 4x4 sub-block origin inside N x N
    uint32_t blocks = static_cast<uint32_t>(N / 4);
    uint32_t subX = (blocks > 1) ? (i4x4_x & (blocks - 1)) : 0u;
    uint32_t subY = (blocks > 1) ? (i4x4_y & (blocks - 1)) : 0u;
    int startX = static_cast<int>(subX * 4);
    int startY = static_cast<int>(subY * 4);

    int x_pos[4], y_pos[4];
    for (int i = 0; i < 4; ++i) { x_pos[i] = startX + i; y_pos[i] = startY + i; }

    auto pred_angle_from_mode = [](uint8_t m) -> int {
        switch (m) {
            case 2: case 34: return 32;
            case 3: case 33: return 26;
            case 4: case 32: return 21;
            case 5: case 31: return 17;
            case 6: case 30: return 13;
            case 7: case 29: return 9;
            case 8: case 28: return 5;
            case 9: case 27: return 2;
            case 10: case 26: return 0;
            case 11: case 25: return -2;
            case 12: case 24: return -5;
            case 13: case 23: return -9;
            case 14: case 22: return -13;
            case 15: case 21: return -17;
            case 16: case 20: return -21;
            case 17: case 19: return -26;
            case 18: return -32;
            default: return 0;
        }
    };

    int pred_angle = pred_angle_from_mode(mode);

    // RTL arithmetic right shift (preserve sign)
    auto arith_shr = [](int v, int s) -> int {
        if (v >= 0) return v >> s;
        return - ( ((-v) + ((1 << s) - 1)) >> s );
    };

    bool horiz = (static_cast<int>(mode) >= 18);

    int fact[4], idxA[4]; unsigned ifact[4];
    for (int i = 0; i < 4; ++i) {
        int pos = horiz ? y_pos[i] : x_pos[i];
        fact[i] = (pos + 1) * pred_angle;
        idxA[i] = arith_shr(fact[i], 5);
        ifact[i] = static_cast<unsigned>(fact[i] & 0x1Fu);
    }

    int delta = horiz ? startX : startY;

    auto read_ref = [&](int k) -> int32_t {
        // k == -1 -> ref_tl @ (0,0)
        if (k == -1) return static_cast<int32_t>(win.data[at(0,0)]);
        if (horiz) {
            if (k <= -2) {
                int top_idx = (-k) - 2;
                if (top_idx < N) return static_cast<int32_t>(win.data[at(0, 1 + top_idx)]);
                return static_cast<int32_t>(win.data[at(0, N + 1)]);
            }
            if (k <= N - 1) return static_cast<int32_t>(win.data[at(1 + k, 0)]);
            return static_cast<int32_t>(win.data[at(N + 1, 0)]);
        } else {
            if (k <= -2) {
                int left_idx = (-k) - 2;
                if (left_idx < N) return static_cast<int32_t>(win.data[at(1 + left_idx, 0)]);
                return static_cast<int32_t>(win.data[at(N + 1, 0)]);
            }
            if (k <= N - 1) return static_cast<int32_t>(win.data[at(0, 1 + k)]);
            return static_cast<int32_t>(win.data[at(0, N + 1)]);
        }
    };

    auto sat8 = [](int32_t v)->uint8_t { if (v < 0) return 0; if (v > 255) return 255; return static_cast<uint8_t>(v); };

    out.resize(16);
    for (int r = 0; r < 4; ++r) {
        int base_idx = delta + idxA[r];
        for (int c = 0; c < 4; ++c) {
            int k = base_idx + c;
            int32_t A = read_ref(k);
            int32_t B = read_ref(k + 1);
            int32_t v = ((static_cast<int32_t>(32 - ifact[r]) * A) + static_cast<int32_t>(ifact[r]) * B + 16) >> 5;
            out[static_cast<size_t>(r) * 4 + c] = sat8(v);
        }
    }
}


