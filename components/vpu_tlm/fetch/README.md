# vpu_tlm / fetch block

This block contains the functional TLM helpers used to load frame data into the encoder model.

Main responsibilities:
- provide a small TLM wrapper around frame loading
- support testbench-side loaders and registries
- model frame-memory access at functional level

Notes:
- this is an integration/helper block, not a full encoder stage by itself
- unit-style coverage lives under `fetch/test`
