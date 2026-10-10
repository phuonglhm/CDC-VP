# Full-pipeline harness vectors

Vector tổng hợp tự tạo trong project, không phải dữ liệu H.264 được encode bởi encoder tham chiếu.
source.hex có 768 byte: hai frame YUV420 16x16, byte i = i mod 256.

stub_expected.hex: FNV-1a 32-bit little-endian của từng tile 384 byte, sau đó EOS fixture
00 00 01 0B. reference.hex là frame thứ hai, dùng offset 768 (slot 2).
contract_expected.hex: 20 byte liên tiếp 01..14, thuộc adapter thử nghiệm variable length.
bad_expected.hex cố ý đổi byte đầu; dùng negative test, không phải golden hợp lệ.

Mỗi case chạy hai activation để kiểm tra counters/output pointer được reset đúng.
Không dùng các vector fixture này làm golden cho backend real. Case real cần nguồn/golden
độc lập và ghi rõ cấu hình/revision của release theo hướng dẫn testbench.
