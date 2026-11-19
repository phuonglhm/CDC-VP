# SAURIA v4.7 drop notice: what this SDK delivery contains

`SAURIA-V4.7-DROP.THIRD_PARTY_NOTICES.md`, next to this file, is the notice of
the SAURIA v4.7 model drop, kept unchanged. Its "Shipped" and "Linked" labels
describe the drop as its authors delivered it, not this SDK. The table maps each
item of that notice to VP_FX1_V3.0.

| Item of the drop notice | In VP_FX1_V3.0 |
|---|---|
| SAURIA accelerator design, ported to SystemC (RTL-ref core, driver headers, `tb_utilization_optimization.cpp`) | **Compiled into `vp/bin/VP_FX1_V3.0/fx1_soc` only.** No source file of the drop is delivered. Licence: Solderpad Hardware License v2.1 (`Apache-2.0 WITH SHL-2.1`), `SAURIA.SHL-2.1.txt`; upstream and provenance: `SAURIA.PROVENANCE.md` |
| Quantized YOLOv8m weights and biases, network structure (`has/insts_pe*/`, `has/net_has/`, `step2/`, `step3/`) | **Not delivered** |
| Quantized ViT-B/16 weights and biases, attention heads (`has/vit_full/`, `has/vectors/gvu/`) | **Not delivered** |
| Preprocessed COCO input image inside each program's `dram_init.bin` / `dram_golden.bin` | **Not delivered** (no program of the drop is delivered) |
| `sample_vit_block.onnx` | **Not delivered** |
| SystemC 2.3.3 (drop's testbenches), GNU C++ runtime | The delivery uses its own SystemC 2.3.4 (`SYSTEMC.Apache-2.0.txt`) |
| Frontend tools and inputs (Ultralytics, timm, PyTorch, ONNX, COCO subsets, ...) | **Not delivered, not needed** |

What the delivery adds for the NPU, none of it from the drop's data:

- the FX1 integration of the model (compiled into `fx1_soc`): CDC-VP code,
  Apache-2.0;
- the firmware driver, job format, C oracle and fixture generator in
  `sw/bootloader/sources/VP_FX1_V3.0/`: CDC-VP code, Apache-2.0;
- the job fixtures in `sw/bootloader/test/VP_FX1_V3.0/npu/`: synthetic data from
  fixed seeds, with goldens from the C oracle (no network, weights or images).

Distribution: `SAURIA.PROVENANCE.md` requires that the rights owner of the
SystemC implementation separately approve the distribution of an NPU-enabled
binary and confirm its licence before it is published. This file does not
replace that approval.
