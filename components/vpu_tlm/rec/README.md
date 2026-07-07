# vpu_tlm / rec block

This is a SystemC TLM 2.3.4 model of the Reconstruction block in a VPU. It serves to:
- Perform the predictions based on mode decision's mode selection. (rec_intra / rec_mc)
- Calculate the residual from those predictions
- Perform Transform / Quantize on the residual and forward to CABAC block
- In parallel, perform Inverse Transform / Quantize and forward to the Deblocking block

# Note
- All modules in /test folder is for testing and wiring up dangling sockets
- Uses custom payload struct rec_packet (in rec_packet.h)

# Run
- make testbench
- To run using package from fetch: make testbench USE_FETCH=1
- (to clean build) make clean