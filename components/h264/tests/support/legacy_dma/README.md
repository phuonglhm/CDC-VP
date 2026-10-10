# Legacy DMA fixture

The previous Huy arbiter/bridge is retained only for isolated regression.
Target: h264_legacy_dma. Namespace: h264::legacy. Headers: h264/legacy_dma/.
It is neither installed nor linked into EncoderVp. Its aging policy is checked
by h264_legacy_priority_fairness; this policy is not imposed on Vinh's DMA.
Production DMA is in components/h264/dma_subsystems with its DmaTransport adapter.
