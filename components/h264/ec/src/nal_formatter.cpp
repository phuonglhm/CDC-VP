#include "nal_formatter.h"

namespace h264::ec {

void NalFormatter::wrap_nal_unit(EcResult& res) {

    // Dịch data để nhét Start Code (0x00000001) vào đầu mảng NAL
    for (int i = res.stream_length - 1; i >= 0; --i) {
        res.nal_stream[i + 4] = res.nal_stream[i];
    }

    res.nal_stream[0] = 0x00;
    res.nal_stream[1] = 0x00;
    res.nal_stream[2] = 0x00;
    res.nal_stream[3] = 0x01;
    res.stream_length += 4;
}

} // namespace h264::ec
