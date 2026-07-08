# `src/` Overview

This folder contains the top-level orchestration code for `vpu_tlm`.

Unlike the block-local implementations under `prei/`, `posi/`, `ime/`, `fme/`, `rec/`, `db/`, and `cabac/`, the files here are responsible for connecting those blocks into a usable encoder flow.

## Files

### `video_encoder_tlm.cpp`

Implements the functional prediction-stage encoder entry point.

Main responsibilities:

- validate the input frame and region
- run the intra path: `PREI -> POSI`
- run the inter path: `IME -> FME` when a valid reference exists
- perform final `mode_decision`
- convert the selected prediction into a REC request packet

This is the main top-level file for block-level encode decisions.

### `mode_decision.cpp`

Implements the final selection policy between the best intra and best inter candidates.

Main responsibilities:

- handle invalid/partial candidate combinations
- choose lower-cost prediction
- apply deterministic tie-break behavior

### `prediction_to_rec_packet.cpp`

Converts a selected `prediction_result` into a `RecPacket` request plus optional MV payload information.

Main responsibilities:

- translate prediction mode into REC packet fields
- encode block size and intra mode
- generate the extended block address used by REC/MV memory

This file defines the packet contract between prediction and reconstruction.

### `video_encoder_to_rec.cpp`

Provides the helper bridge from encoder decision output to REC-side transaction setup.

Main responsibilities:

- write MV data into MV memory when inter prediction is selected
- serialize the REC packet
- decide whether the request targets `rec_intra` or `rec_mc`
- build a TLM transaction for the REC path

### `rec_backend_bridges.cpp`

Bridges REC output packets into DB and CABAC backend packet formats.

Main responsibilities:

- translate `RecPacket` into `DbCustomPacket`
- translate `RecPacket` into `CabacCustomPacket`
- preload CABAC coefficient memory using the extended block address
- forward TLM traffic from REC to the backend modules

### `video_encoder_full_tlm.cpp`

Implements the fully integrated encoder path that combines:

- `video_encoder_tlm`
- `video_encoder_to_rec`
- `REC`
- `DB`
- `CABAC`

Main responsibilities:

- instantiate and wire the integrated top-level path
- run one region through the complete pipeline
- collect DB/CABAC outputs
- traverse a full frame CTU-by-CTU
- update the working reconstructed frame after each successful region

This is the highest-level functional entry point in `vpu_tlm`.

## Reading Order

If you are new to this folder, read in this order:

1. `video_encoder_tlm.cpp`
2. `prediction_to_rec_packet.cpp`
3. `video_encoder_to_rec.cpp`
4. `rec_backend_bridges.cpp`
5. `video_encoder_full_tlm.cpp`

That order follows the dataflow from prediction selection to integrated backend execution.
