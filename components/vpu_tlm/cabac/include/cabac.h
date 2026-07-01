#ifndef CABAC_H
#define CABAC_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>

// forward-declare RecPacket to avoid including the full header in this file
struct RecPacket;

class Cabac : sc_core::sc_module {
    public:
    Cabac(sc_core::sc_module_name name);
    SC_HAS_PROCESS(Cabac);
    tlm_utils::simple_initiator_socket<Cabac> mem_socket;//write to mem
    tlm_utils::simple_initiator_socket<Cabac> out_socket;
    tlm_utils::simple_target_socket<Cabac> start_socket;


    private:
     struct Bin {
        bool bit;         
        uint32_t ctx_idx;
        bool bypass;
    };
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    std::vector<uint8_t> binarize(const std::vector<uint8_t>& coeffs);
    // Given a packed binstream (LSB-first bytes from `binarize`), expand
    // it into a vector of `Bin` entries, assigning contexts and marking
    // bypass bins (e.g. sign bits). The function uses `pkt` metadata to
    // derive a deterministic context mapping.
    std::vector<Bin> bin_buffer_select(const RecPacket &pkt, const std::vector<uint8_t>& binstream);
    std::pair<uint8_t, uint8_t> read_context(uint32_t ctx_idx);
    void write_context(uint32_t ctx_idx, uint8_t state, uint8_t mps);
    void emit_bytes_to_mem(uint64_t addr, const std::vector<uint8_t>& data);

    std::vector<Bin> assign_contexts(const RecPacket &pkt, const std::vector<uint8_t>& binstream);
    std::vector<uint8_t> encode_bins(const std::vector<Bin>& bins, uint64_t emit_base_addr = 0x20000000ULL);
    uint16_t rlps_for_state(uint8_t state, uint16_t range);
    void range_update(bool bin, uint16_t &range, uint16_t &low, uint8_t &state, uint8_t &mps);    

    // Current QP
    uint8_t qp_ = 22;
};
#endif