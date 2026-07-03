#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "custom_packet.h"
#include <bitset>
#include <array>
#include <cstdint>

class BorderStrength : sc_core::sc_module {
    public:
    BorderStrength(sc_core::sc_module_name name);
    tlm_utils::simple_initiator_socket<BorderStrength> filter_socket;
    tlm_utils::simple_target_socket<BorderStrength> start_socket;


    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    struct EdgeMasks {
        std::array<uint8_t, 16> v;   // v0..v15 (7-bit fields packed into bytes)
        std::array<uint16_t, 8> h;   // h0..h7
    };

    void edge_detect(const CustomPacket &pkt,
                     unsigned sys_ctu_x, unsigned sys_ctu_y,
                     uint16_t cnt, uint8_t state,
                     bool &tu_edge, bool &pu_edge);

    void cbf_select(const CustomPacket &pkt,
                    unsigned sys_ctu_x, unsigned sys_ctu_y,
                    uint16_t cnt, uint8_t state,
                    bool &cbf_p, bool &cbf_q);

    void select_qp(const CustomPacket &pkt,
                   unsigned sys_ctu_x, unsigned sys_ctu_y,
                   uint16_t cnt, uint8_t state,
                   uint8_t &qp_p, uint8_t &qp_q);

    void compute_tu_masks(const std::bitset<21> &mb_partition, EdgeMasks &out);
    void compute_pu_masks(const std::bitset<21> &mb_partition, const std::bitset<42> &mb_p_pu_mode, EdgeMasks &out);
    void compute_qp_flags(const CustomPacket &pkt, std::array<bool, 64> &qp_flags);

    std::array<uint16_t, 64> cbf_top_r{};
    std::array<uint8_t, 16> cbf_left{};
    std::array<uint8_t, 8>  qp_left_flag{};
    uint8_t cbf_tl{};
    // qp RAM and left/top registers emulation
    std::array<uint32_t, 64> qp_top_r{}; // 20-bit entries: [19:12]=top_flag[7:0], [11:6]=qp_left, [5:0]=qp_i
    uint8_t qp_left{0};
    uint8_t qp_left_modified{0};

};