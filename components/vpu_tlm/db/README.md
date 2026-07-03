# vpu_tlm / rec block

This is a SystemC TLM 2.3.4 model of the Deblocking block in a VPU. It serves to:
- Compute and smooth border mismatch caused by processing by small block in the process.
- Filter it so it can be used as reference frames.

# Note
- All modules in test is for testing and wiring up dangling sockets
- Uses custom payload struct custom_packet (in custom_packet.h)

# Run
- make testbench
- (to clean build) make clean