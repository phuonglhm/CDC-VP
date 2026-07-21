# FreeRTOS NPU model asset provenance

The CDC-VP rights holder has confirmed that the following generated model
asset is authorized for public distribution under the repository's Apache
License 2.0.

- Rights holder: Copyright 2026 The CDC-VP Authors
- License: Apache-2.0
- License text: repository root `LICENSE`
- Original FlatBuffer size: 4680 bytes
- Original FlatBuffer SHA-256:
  `aa467144bf1e697094dec0f399e4e644fd39acc63be7d4bfd54974a84fe1c6b5`
- Generated representation:
  `secda_simple_model_model_data.cc`

The C array is an exact, 16-byte-aligned representation of the reviewed
`secda_simple_model.tflite` FlatBuffer. Its runtime contract is:

| Property | Value |
|---|---|
| Input | INT8 `[1,4,4,1]`, deterministic ramp `0..15` |
| Output | INT8 `[1,2]` |
| Golden output | `{-113, 127}` |
| Operators | Conv2D, FullyConnected, Softmax, DepthwiseConv2D, Shape, TransposeConv, Add, Pad, Mean |

If the array is regenerated or replaced, extract its `.rodata` payload,
recompute the SHA-256, rerun the CPU/NPU bit-exact regression, and reconfirm
that the replacement is authorized for public distribution.
