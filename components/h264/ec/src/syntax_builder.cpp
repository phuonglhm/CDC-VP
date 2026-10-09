#include "syntax_builder.h"

namespace h264::ec {

void SyntaxBuilder::build_slice_header(const EcRequest& req, BitWriter& bw) {

    // Đóng gói cờ giả lập Slice Header
    bw.write_bits(0b101, 3);
    bw.write_bits(req.qp, 6);
    bw.write_bits(req.entropy_coding_mode, 1);
}

} // namespace h264::ec
