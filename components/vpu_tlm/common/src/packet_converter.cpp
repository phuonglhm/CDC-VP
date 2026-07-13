#include "../include/packet_converter.h"

#include "../../fetch/include/fetch_packet.h"
#include "../../rec/include/rec_packet.h"

bool cdc::components::packet_converter::custom_to_fetch_buf(const CustomPacket &in, std::vector<uint8_t> &outbuf) {
    if (in.cmd != CustomCmd::READ_REQ) return false;

    FetchPacket fp;
    fp.cmd = FetchCmd::GET_EDGE;
    uint8_t plane = in.sel;
    if (plane > 2u) plane = 0u;
    fp.plane = plane;
    fp.x_px = static_cast<uint32_t>(in.x) * 4u;
    fp.y_px = static_cast<uint32_t>(in.y) * 4u;
    uint32_t N = recSizeToPixels(in.size);
    fp.width = static_cast<uint16_t>(N);
    fp.height = static_cast<uint16_t>(N);
    fp.margin = 0;
    fp.downsample = 1;
    fp.align_px = 1;
    fp.req_id = static_cast<uint32_t>(in.block_idx);
    fp.data.clear();

    outbuf = packFetchPacket(fp);
    return true;
}

bool cdc::components::packet_converter::rec_to_fetch_buf(const RecPacket &in, std::vector<uint8_t> &outbuf) {
    if (in.cmd != RecCmd::READ_REQ) return false;

    FetchPacket fp;
    fp.cmd = FetchCmd::GET_EDGE;
    uint8_t plane = in.sel;
    if (plane > 2u) plane = 0u;
    fp.plane = plane;
    fp.x_px = static_cast<uint32_t>(in.x) * 4u;
    fp.y_px = static_cast<uint32_t>(in.y) * 4u;
    uint32_t N = recSizeToPixels(in.size);
    fp.width = static_cast<uint16_t>(N);
    fp.height = static_cast<uint16_t>(N);
    fp.margin = 0;
    fp.downsample = 1;
    fp.align_px = 1;
    fp.req_id = static_cast<uint32_t>(in.block_idx);
    fp.data.clear();

    outbuf = packFetchPacket(fp);
    return true;
}
