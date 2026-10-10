#include "nal_formatter.h"

#include <vector>
#include <algorithm>
#include <iostream>
namespace h264::ec {

void NalFormatter::wrap_nal_unit(EcResult& res, 
                                 std::uint8_t nal_ref_idc, 
                                 std::uint8_t nal_unit_type) {
    std::vector<std::uint8_t> out;
    out.reserve(res.stream_length + 10);

    // Chèn NAL Start Code Prefix (00 00 00 01)
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x00);
    out.push_back(0x01);

    // Chèn NAL Header lấy TRỰC TIẾP từ đầu vào 
    std::uint8_t nal_header = ((nal_ref_idc & 0x03) << 5) | (nal_unit_type & 0x1F);
    out.push_back(nal_header);
    
    // Quét RBSP và chèn Emulation Prevention Byte (0x03)
    int zero_count = 0;
    for (std::uint32_t i = 0; i < res.stream_length; ++i) {
        std::uint8_t b = res.nal_stream[i];
        if (zero_count == 2 && b <= 0x03) {
            out.push_back(0x03); 
            zero_count = 0;
        }
        out.push_back(b);
        if (b == 0x00) {
            zero_count++;
        } else {
            zero_count = 0;
        }
    }

    // Bổ sung RBSP Trailing bit căn lề 32-bit (Tương thích bus AXI 32-bit)
    while (out.size() % 4 != 0) {
        out.push_back(0x00);
    }
    
    if (out.size() > res.nal_stream.size()) {
        std::cout << "NAL Stream (" << out.size() 
                  << " bytes) vượt quá dung lượng buffer!\n";
    }
    
    res.stream_length = std::min(static_cast<std::uint32_t>(out.size()), 
                                 static_cast<std::uint32_t>(res.nal_stream.size()));
                                 
    for (std::uint32_t i = 0; i < res.stream_length; ++i) {
        res.nal_stream[i] = out[i];
    }
}

} // namespace h264::ec