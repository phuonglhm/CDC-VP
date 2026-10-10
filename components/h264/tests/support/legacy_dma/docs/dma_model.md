# DMA transaction model

Bridge nhận byte-range request, kiểm tra command, address 32-bit, data length và byte enables.
Tại mỗi segment:

```text
lane = address % bus_bytes
bytes = min(remaining, 4096 - address % 4096, max_beats * bus_bytes - lane)
beats = ceil((lane + bytes) / bus_bytes)
external_address = address - lane
```

Buffer external có beats*bus_bytes byte; các byte ngoài request bị disable. Với enables
tuần hoàn, index dùng offset của request gốc nên không lệch pha khi chia segment.
Đọc chỉ copy lại các byte enabled. Arbiter lock toàn request, kể cả khi bridge chia segment.

DDR trả response sau latency và commit; bridge cũng tiêu thụ annotated delay của target
thay thế. Lỗi dừng request, giữ các segment đã commit trước đó. Không retry tự động.

Đây là LT lane/burst abstraction; không mô hình AW/W/R từng chu kỳ, legacy beat scheduling
hay FIFO 16-word. Xem giới hạn chung trước khi đối chiếu throughput/protocol RTL.

Arbiter dùng queue priority có aging: rank nhỏ được chọn trước, cùng rank giữ thứ tự đến;
request bị vượt 8 lần sẽ được ưu tiên. Rank mặc định bằng nhau, adapter release có thể gọi
set_priority(binding_index,rank). Active transaction không bị preempt, kể cả khi response
bị trễ. Chính sách aging là lựa chọn VP để bảo đảm tiến triển, không phải bảng priority RTL
được suy đoán. PDF §5.3 không liệt kê thứ tự ưu tiên cụ thể.
