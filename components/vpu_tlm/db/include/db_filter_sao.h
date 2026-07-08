#ifndef DB_FILTER_SAO_H
#define DB_FILTER_SAO_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "custom_packet.h"
#include <array>
#include <cstdint>

class Filter_SAO : sc_core::sc_module {
public:
    Filter_SAO(sc_core::sc_module_name name);
    tlm_utils::simple_initiator_socket<Filter_SAO> out_socket;
    tlm_utils::simple_target_socket<Filter_SAO> bs_socket;
    tlm_utils::simple_target_socket<Filter_SAO> mv_socket;

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    void unpack_blocks(const DbCustomPacket &pkt, std::array<uint8_t,16> &p_blk, std::array<uint8_t,16> &q_blk) const;
    void pack_blocks(DbCustomPacket &pkt, const std::array<uint8_t,16> &p_blk, const std::array<uint8_t,16> &q_blk) const;

    // Parameters that control filtering for a single edge/block pair.
    struct FilterParams {
        bool is_luma{true};
        bool tu_edge{false};
        bool pu_edge{false};
        bool cbf_p{false};
        bool cbf_q{false};
        uint8_t qp_p{0};
        uint8_t qp_q{0};
        uint32_t mv_p{0};
        uint32_t mv_q{0};
        uint8_t bs{0}; // border strength
        bool is_ver{true};
    };

    // Lookup helpers (replicate db_lut_beta / db_lut_tc behavior)
    uint8_t lookup_beta(uint8_t qp) const;
    uint8_t lookup_tc(uint8_t qp, bool intra) const;

    // Deblocking kernels. Each operates on two 4x4 blocks (p = left/top,
    // q = right/bottom) and writes filtered outputs.
    void apply_normal_filter(const std::array<uint8_t,16> &p_in,
                             const std::array<uint8_t,16> &q_in,
                             std::array<uint8_t,16> &p_out,
                             std::array<uint8_t,16> &q_out,
                             const FilterParams &params) const;

    void apply_strong_filter(const std::array<uint8_t,16> &p_in,
                             const std::array<uint8_t,16> &q_in,
                             std::array<uint8_t,16> &p_out,
                             std::array<uint8_t,16> &q_out,
                             const FilterParams &params) const;

    void apply_chroma_filter(const std::array<uint8_t,16> &p_in,
                             const std::array<uint8_t,16> &q_in,
                             std::array<uint8_t,16> &p_out,
                             std::array<uint8_t,16> &q_out,
                             const FilterParams &params) const;

    // Decide whether to use the strong filter based on RTL dsam/norm_str logic
    bool want_strong_filter(const std::array<uint8_t,16> &p_in,
                            const std::array<uint8_t,16> &q_in,
                            const FilterParams &params) const;

    static inline uint8_t clamp8(int v) { if (v < 0) return 0; if (v > 255) return 255; return static_cast<uint8_t>(v); }

    // Cached state when bs_socket and mv_socket invoke the filter separately
    FilterParams last_params_{};
    std::array<uint8_t,16> last_p_in_{};
    std::array<uint8_t,16> last_q_in_{};
    bool have_bs_{false};
    bool have_mv_{false};
};

#endif
