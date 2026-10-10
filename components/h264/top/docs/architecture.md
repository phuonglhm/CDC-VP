# Encoder integration ownership

- ControlRegs and EncoderController own CSR, activation, STATUS/LEN and IRQ.
- ResetDomain and EncoderVp own the shared reset lifecycle.
- Vinh owns DMA clients, H264Arb and AxiMasterBridge in dma_subsystems.
- DmaTransport is the SystemC adapter around those cores; it does not implement
  a second priority or AXI segmentation algorithm.
- Vinh's complete h264_dma_tlm component remains in dma_subsystems/tlm,
  with its original MMIO, reset, IRQ and workload API. It is built and installed
  even when tests are disabled. Its original shared-bus tests use this component.
- DmaTransport belongs to top (integration glue), not Vinh's DMA module.
  EncoderVp currently connects to Vinh's cores through this adapter; it does not
  instantiate h264_dma_tlm. These are separate integration entry points, not
  two MMIO controllers to bind to the same address.
- Only Huy's old DMA is test support, in tests/support/legacy_dma.

```text
Host -> ControlRegs -> EncoderController -> FrameExecutorIf
                                             |
                        DmaTransport (H264Arb -> AxiMasterBridge)
                                             |
                                      TLM system memory
```

Public ports: registers.socket, dma.clients, dma.memory, rstn, irq.
PipelineFactory now receives DmaTransport&. Producers bind dma.clients and
attach DmaClientExtension for CMB/SW/NAL/DF identity. Untagged test fixtures use
CMB. No bridge or arbiter member from the old top API remains.

Word acceptance progress remains a pipeline responsibility via
nal_words_accepted; successful memory responses govern final drain/completion.
NalDma::stm_len() is committed-word accounting and must not be substituted for
accepted-word progress. ControlRegs remains the single architectural LEN owner.

FramePipelineStub and Prediction_Test remain non-codec fixtures. A released
pipeline must implement FrameExecutorIf and retain the real team modules for
the full simulation lifetime.
