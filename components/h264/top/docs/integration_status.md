# Integration status

| Thành phần | Đang nối | Trạng thái |
|---|---|---|
| ControlRegs / EncoderController | TLM register target + blocking frame executor | VP implemented/tested |
| DmaArbiter / DmaBridge | Multi-client target -> one memory initiator | VP implemented/tested |
| Reset | Epoch + delay release | VP implemented/tested |
| DDR / Host | Platform models | Implemented/tested |
| CMB / Reference / NAL clients | FramePipelineStub | Fixture của Huy; chờ Vinh task 05 |
| Intra / inter prediction | ProcessingStub | Chờ Vinh task 06/07 |
| TQ / reconstruction / entropy | ProcessingStub | Chờ Nguyên task 08/09/10 |
| Search-window / P/B list coordination | Chưa nối | Chờ Vinh task 05/07 |
| Memory architecture / local RAM | Chưa nối | Chờ Nguyên task 11; DDR platform chỉ là môi trường test |

Điểm bàn giao: `ProcessingIf` cho block fixture; `FrameExecutorIf` cho pipeline functional
hoàn chỉnh; `DmaArbiter::clients` cho DMA initiator mới. Mô hình functional cần golden mới
và test I/P/B, entropy variable length, reference completion riêng.

Chi tiết bàn giao: `integration_docs/cdc_vp_integration.md` từ root. Pipeline đã được inject qua factory;
controller nhận length từ adapter và không tự thêm EOS. Full-pipeline harness đã có;
real backend chỉ được bật khi có target adapter của nhóm và golden độc lập.
