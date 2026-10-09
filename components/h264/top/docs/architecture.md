# Top VP

HostDriver.registers -> ControlRegs -> EncoderController -> FramePipelineStub

FramePipelineStub.{cmb,reference,nal} -> DmaArbiter -> DmaBridge -> DdrMemory

HostDriver.memory -> DdrMemory (staging/drain bên ngoài IP)

ProcessingStub là điểm thay thế tính toán macroblock. FrameExecutorIf là điểm thay
toàn pipeline khi nhóm có entropy/ref completion thực. Không đổi trách nhiệm arbiter
để arbiter quyết định frame done. Top chỉ kết nối và quản lý reset.

Public port thực tế: `registers.socket`, `bridge.memory`, `rstn`, `irq`.
Các component có tên phân cấp để dùng trong diagnostic trace.

EncoderVp sở hữu unique_ptr<FrameExecutorIf>, tạo bằng PipelineFactory ở constructor.
Không còn chứa ProcessingStub hoặc FramePipelineStub bằng value. Library h264_top chỉ
chứa control/wiring; h264_pipeline_stub chứa factory mặc định và implementation giả lập.
Ứng dụng muốn dùng real factory truyền rõ vào constructor và link adapter tương ứng.
Factory gọi lúc elaboration và giữ các module của nhóm sống đến hết simulation.
