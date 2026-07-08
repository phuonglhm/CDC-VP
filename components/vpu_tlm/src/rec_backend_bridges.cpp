#include "rec_backend_bridges.h"
#include "block_coord_codec.h"

namespace cdc::components {

namespace {

constexpr std::uint64_t kCabacCoeffBaseAddr = 0x11000000ULL;

std::uint32_t make_block_addr(const RecPacket& packet)
{
    return make_extended_block_address(packet.block_idx, packet.x, packet.y);
}

template <typename PacketT, typename CmdT>
PacketT copy_common_fields(const RecPacket& packet, CmdT cmd)
{
    PacketT out;
    out.cmd = cmd;
    out.block_idx = packet.block_idx;
    out.x = packet.x;
    out.y = packet.y;
    out.size = packet.size;
    out.sel = packet.sel;
    out.qp = packet.qp;
    out.pred_type = packet.pred_type;
    out.mode = packet.mode;
    out.pre_sel = packet.pre_sel;
    out.i4x4_x = packet.i4x4_x;
    out.i4x4_y = packet.i4x4_y;
    out.cbf_mask = packet.cbf_mask;
    out.data = packet.data;
    return out;
}

} // namespace

rec_to_db_bridge::rec_to_db_bridge(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      start_socket("start_socket"),
      bs_socket("bs_socket"),
      mv_socket("mv_socket")
{
    start_socket.register_b_transport(this, &rec_to_db_bridge::b_transport);
}

DbCustomPacket rec_to_db_bridge::convert_packet(const RecPacket& packet)
{
    DbCustomPacket out = copy_common_fields<DbCustomPacket>(packet, static_cast<DbCustomCmd>(packet.cmd));
    out.cnt = 0;
    out.state = 0;
    return out;
}

void rec_to_db_bridge::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    const RecPacket packet = unpackRecPacket(trans);
    const DbCustomPacket db_packet = convert_packet(packet);
    std::vector<std::uint8_t> bs_storage = packDbCustomPacket(db_packet);
    std::vector<std::uint8_t> mv_storage = bs_storage;

    tlm::tlm_generic_payload bs_trans;
    bs_trans.set_command(tlm::TLM_WRITE_COMMAND);
    bs_trans.set_address(trans.get_address());
    bs_trans.set_data_ptr(bs_storage.data());
    bs_trans.set_data_length(bs_storage.size());
    bs_trans.set_streaming_width(bs_storage.size());
    bs_trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    bs_socket->b_transport(bs_trans, delay);

    tlm::tlm_generic_payload mv_trans;
    mv_trans.set_command(tlm::TLM_WRITE_COMMAND);
    mv_trans.set_address(trans.get_address());
    mv_trans.set_data_ptr(mv_storage.data());
    mv_trans.set_data_length(mv_storage.size());
    mv_trans.set_streaming_width(mv_storage.size());
    mv_trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    mv_socket->b_transport(mv_trans, delay);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

rec_to_cabac_bridge::rec_to_cabac_bridge(sc_core::sc_module_name name)
    : sc_core::sc_module(name),
      start_socket("start_socket"),
      cabac_socket("cabac_socket")
{
    start_socket.register_b_transport(this, &rec_to_cabac_bridge::b_transport);
}

void rec_to_cabac_bridge::bind_memory(CabacSimpleMemory& memory)
{
    memory_ = &memory;
}

CabacCustomPacket rec_to_cabac_bridge::convert_packet(const RecPacket& packet)
{
    return copy_common_fields<CabacCustomPacket>(packet, static_cast<CabacCustomCmd>(packet.cmd));
}

void rec_to_cabac_bridge::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    const RecPacket packet = unpackRecPacket(trans);
    if (memory_ != nullptr) {
        const std::uint64_t coeff_addr =
            kCabacCoeffBaseAddr | static_cast<std::uint64_t>(make_block_addr(packet));
        memory_->load_data(coeff_addr, packet.data);
    }

    const CabacCustomPacket cabac_packet = convert_packet(packet);
    std::vector<std::uint8_t> storage = packCabacCustomPacket(cabac_packet);

    tlm::tlm_generic_payload cabac_trans;
    cabac_trans.set_command(tlm::TLM_WRITE_COMMAND);
    cabac_trans.set_address(trans.get_address());
    cabac_trans.set_data_ptr(storage.data());
    cabac_trans.set_data_length(storage.size());
    cabac_trans.set_streaming_width(storage.size());
    cabac_trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    cabac_socket->b_transport(cabac_trans, delay);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

} // namespace cdc::components
