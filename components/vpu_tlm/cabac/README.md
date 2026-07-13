# vpu_tlm / cabac block

This repository contains a SystemC TLM 2.3.4 model of the CABAC (Context-Adaptive Binary Arithmetic Coding) block in VPU. It serves to
- Perform CABAC encoding (binarization, context management, renormalization) with HM/JM-style buffered byte emission aimed at bit-exactness vs RTL.
- Ship CABAC tables as a header-only resource so the model builds and runs without external table files.

# Note
- All modules in test is for testing and wiring up dangling sockets
- Uses custom payload struct custom_packet (in custom_packet.h) (will be pushed to external)

# Specs
- Memory map (addresses used)
	- **0x10000000**: CABAC context base
		- Embedded context tables (loaded by SimpleMemory from include/cabac_tables.h):
			- `cabac_ctx_islice` (186 bytes) at 0x10000000
			- `cabac_ctx_init2` (186 bytes) at 0x10000100
			- `cabac_ctx_init1` (186 bytes) at 0x10000200
		- Context read/write:
			- contexts are accessed at `0x10000000 + ctx_idx` (one byte per context; packed as [mps<<6 | state])
	- **0x11000000**: CABAC coefficient source base
		- coefficient bytes are addressed by an extended 4x4 block key `((block_idx << 16) | (y << 8) | x)`
	- **0x20000000**: CABAC emitted byte-stream base
		- encoded bytes are written using the same extended block key

# Run in module
- make testbench
- (to clean build) make clean
