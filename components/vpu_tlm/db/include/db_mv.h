#ifndef DB_MV_H
#define DB_MV_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "custom_packet.h"
#include <bitset>
#include <array>
#include <cstdint>

class MotionVector : sc_core::sc_module {
    public:
    MotionVector(sc_core::sc_module_name name);
    tlm_utils::simple_initiator_socket<MotionVector> filter_socket;
    tlm_utils::simple_target_socket<MotionVector> start_socket;

    uint32_t last_mv_p{0};
    uint32_t last_mv_q{0};

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // Select motion vectors for the current edge/micro-phase. Mirrors the
    // selection logic in db_mv.v. Produces packed 20-bit MVs in the same
    // format used by the RTL: [19:10]=x (signed), [9:0]=y (signed).
    void select_mv(const CustomPacket &pkt,
                   unsigned sys_ctu_x, unsigned sys_ctu_y,
                   uint16_t cnt, uint8_t state,
                   uint32_t &mv_p, uint32_t &mv_q);

    // Compute addresses / control signals used to read/write the current and
    // top MV RAMs. Provided to help unit-test address timing if needed.
    void compute_mv_addresses(unsigned sys_ctu_x, uint16_t cnt,
                              unsigned &cur_rd_addr, unsigned &cur_wr_addr,
                              bool &use_top_read, unsigned &top_addr,
                              bool &top_write);

    void decode_mv(uint32_t packed_mv, int16_t &mx, int16_t &my);

    void write_cur_mv(unsigned addr, uint32_t data);
    void read_cur_mv(unsigned addr, uint32_t &data) const;
    void write_top_mv(unsigned addr, uint32_t data);
    void read_top_mv(unsigned addr, uint32_t &data) const;

    std::array<uint32_t, 64> cur_mv_r{};   // 64 entries, 20-bit valid
    std::array<uint32_t, 512> top_mv_r{};  // 512 entries, 20-bit valid
    std::array<uint32_t, 8> left_mv{};     // left_mv[0..7]
    uint32_t tl_mv{0};

    uint32_t mv_pre_up{0};
    uint32_t mv_pre_dn{0};
    uint32_t mv_cur_up{0};
    uint32_t mv_cur_dn{0};
};

#endif 