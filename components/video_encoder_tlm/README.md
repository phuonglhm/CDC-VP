# video_encoder_tlm

Minimal TLM-2.0 video encoder skeleton (`cdc::components::video_encoder_tlm`).

This component provides:

- A target socket for simple register-style control.
- A placeholder encode pipeline composed of PREI, POSI, IME, FME, mode decision,
  reconstruction, and CABAC stages.
- A host-side `load_input_frame()` API to seed an input frame for tests and
  platform bring-up.

## Register map

- `0x00` `CONTROL` (W): write `1` to trigger a placeholder encode.
- `0x04` `STATUS` (R): bit `0` set when the last encode completed.
- `0x08` `OUTPUT_SIZE` (R): encoded payload size in bytes.

## Notes

- This is a compile-safe scaffold intended for incremental implementation.
- The internal pipeline currently uses deterministic placeholder algorithms.
