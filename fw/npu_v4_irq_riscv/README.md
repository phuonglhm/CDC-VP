# SAURIA NPU v4 full-SoC firmware test

This firmware validates the complete software-visible path:

`RV32 CPU -> NPU0 MMIO -> NPU RAM master -> SAURIA core -> RAM0 -> PLIC IRQ17`

It writes a deterministic INT8 `64x64` activation matrix and `64x64` weight
matrix into the reserved NPU pipeline buffers, starts one GEMM, handles the
level-sensitive done/error interrupt, and checks all 4096 INT32 outputs.

Expected UART markers:

```text
NPU v4 platform start
NPU IRQ17
NPU PASS
```
