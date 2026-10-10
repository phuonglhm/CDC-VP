// h264_spec.h — Normative constants and address contracts from the HAS.
//
// Source: SISLAB_H264_AVC_HAS_Spec_Detailed_v2.1.11
//   §12.3 planar address, coded dimensions, reference slots  [X]
//   §12.2 external DDR region contract
//   §6.1  CMB planar addressing
//   §5.5  width/lane/alignment rules
//
// Every [X] item here is a spec requirement, not an implementation choice.
// Where the PDF is silent the item is marked [TBD] and the model picks a
// documented default — see kDefault* below.
#ifndef H264_SPEC_H
#define H264_SPEC_H

#include <cstdint>
#include <array>
#include <stdexcept>
#include <string>

namespace h264 {

// --- Coded dimensions (§12.3, §16) -------------------------------------
// 1080p presentation uses 1920x1088 for coded storage/plane stride.
// The 8 cropped rows are SPS information and MUST NOT make Hc=1080 when
// computing DDR. §12.3 "Điểm cẩn thận [X]".
struct CodedDims {
    uint32_t Wc;  // coded width  in pixels, multiple of 16
    uint32_t Hc;  // coded height in pixels, multiple of 16
};

inline void validate_dims(const CodedDims& d) {
    if (!d.Wc || !d.Hc || d.Wc % 16 || d.Hc % 16 ||
        d.Wc > 1920 || d.Hc > 1088)
        throw std::invalid_argument("h264: invalid coded dimensions");
}

struct MacroblockPixels {
    std::array<uint8_t, 256> y{};
    std::array<uint8_t, 64> u{}, v{};
};

inline uint32_t frame_bytes(const CodedDims& d) {
    // §12.3: Frame bytes F = 3*Wc*Hc/2
    return (3u * d.Wc * d.Hc) / 2u;
}

// --- Planar plane offsets (§12.3, §6.1) --------------------------------
// These are byte offsets from a region base, for a coded Wc x Hc frame.
inline uint32_t y_plane_offset(const CodedDims&) { return 0u; }

inline uint32_t u_plane_offset(const CodedDims& d) {
    return d.Wc * d.Hc;              // §12.3: U offset = Wc*Hc
}

inline uint32_t v_plane_offset(const CodedDims& d) {
    return (5u * d.Wc * d.Hc) / 4u;  // §12.3: V offset = 5*Wc*Hc/4
}

inline uint32_t y_stride(const CodedDims& d) { return d.Wc; }        // bytes/row
inline uint32_t uv_stride(const CodedDims& d) { return d.Wc / 2u; }  // bytes/row

// §6.1 planar address expansion. (x,y) are PIXEL coordinates inside the
// coded picture — NOT macroblock indices. For U/V the coordinates live in
// the half-resolution chroma grid (so x < Wc/2, y < Hc/2).
inline uint64_t planar_y_addr(uint64_t base, const CodedDims& d, uint32_t x, uint32_t y) {
    return base + static_cast<uint64_t>(y) * y_stride(d) + x;
}
inline uint64_t planar_u_addr(uint64_t base, const CodedDims& d, uint32_t x, uint32_t y) {
    return base + u_plane_offset(d) + static_cast<uint64_t>(y) * uv_stride(d) + x;
}
inline uint64_t planar_v_addr(uint64_t base, const CodedDims& d, uint32_t x, uint32_t y) {
    return base + v_plane_offset(d) + static_cast<uint64_t>(y) * uv_stride(d) + x;
}

// --- REFM slots (§12.3) ------------------------------------------------
// REFM slot k = REG_REFM + k*F, k = 0,1,2. Minimum 3xF bytes (§12.2).
inline constexpr int kRefmSlots = 3;
inline uint64_t refm_slot_base(uint64_t reg_refm, const CodedDims& d, int k) {
    if (k < 0 || k >= kRefmSlots) throw std::out_of_range("h264: invalid REFM slot");
    return reg_refm + static_cast<uint64_t>(k) * frame_bytes(d);
}

// --- AXI contract (§5.5) ----------------------------------------------
inline constexpr uint32_t kDdr4KiBPage   = 4096u;   // no burst crosses this
inline constexpr uint32_t kMaxPhysAddr32 = 0xFFFFFFFFull; // baseline: < 4 GiB
inline constexpr uint32_t kReadFifoDepth = 16u;     // §5.1: FIFO 16 entry x 32 bit
inline constexpr uint32_t kInternalWordBits = 32u;  // internal legacy word width

// §5.5: internal transfer sizes are 8, 16 or 32 bit.
enum class TransferSize : uint32_t { B1 = 1, B2 = 2, B4 = 4 };

// --- Arbiter clients (§5.4) --------------------------------------------
// Four client groups: CMB, SW, NAL, DF/reference.
enum class ClientId : int { CMB = 0, SW = 1, NAL = 2, DF = 3, NONE = 4, COUNT = 4 };

inline const char* client_name(ClientId c) {
    switch (c) {
        case ClientId::CMB: return "CMB";
        case ClientId::SW:  return "SW";
        case ClientId::NAL: return "NAL";
        case ClientId::DF:  return "DF";
        default:            return "NONE";
    }
}

// §5.4: "Thứ tự priority là implementation-specific ... [TBD] chốt từ
// h264_arb.vhd". The PDF does NOT publish a copyable priority order, so the
// model exposes it as a configurable policy rather than hard-coding a guess.
// Default reflects the spec's stated intent to protect the critical path
// (reconstruction/DF and reference traffic) without claiming to be the RTL
// order. Replace with the RTL order when h264_arb.vhd is audited.
inline const ClientId kDefaultPriority[4] = {
    ClientId::DF, ClientId::SW, ClientId::CMB, ClientId::NAL};

}  // namespace h264

#endif  // H264_SPEC_H
