# Platform models

`host/`: HostDriver phát register/memory transaction qua TLM. Caller chạy trong SC_THREAD.
`memory/`: DdrMemory lưu byte, latency cấu hình được, fault region và trace giao dịch.

Đây là môi trường ngoài encoder IP, không đặt trong task thuật toán. CMake target h264_platform.
Host và DMA bind hai initiator vào DDR multi-target. Memory serialize access, commit sau latency.
Diagnostic in_flight/active_address chỉ phục vụ quan sát test, không phải giao diện phần mềm IP.

Owner: Huy Nguyen Huynh Quoc (hạ tầng kiểm thử). Memory Architecture chương 12 thuộc
Nguyên ở `module Memory Architecture (chương 12) do nhóm bàn giao`: local memory, layout/stride,
reference slots và hợp đồng read latency/collision. DDR model này chỉ thực hiện giao dịch
bộ nhớ ngoài; không tự quyết định slot rotation hoặc thay thế các RAM nội bộ của Nguyên.
