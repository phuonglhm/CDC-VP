# FlooNoC SystemC model provenance

## Scope

This component is a C++17/SystemC behavioral model and CDC-VP integration
adapter. It does not contain or compile upstream SystemVerilog into the
installed library. The RTL is used as the behavioral source of truth and by
the separate verification harnesses.

## Frozen upstream sources

| Dependency | Upstream | Frozen revision | Use |
|---|---|---|---|
| FlooNoC | `https://github.com/pulp-platform/FlooNoC.git` | `9a6972a5f9b8117506d1df8a6505ce1da2bc9084` (`v0.8.4-10-g9a6972a`) | Router, routing, AXI chimney, metadata and NoRoB behavior |
| common_cells | `https://github.com/pulp-platform/common_cells.git` | `9ca8a7655f741e7dd5736669a20a301325194c28` (1.39.0) | FIFO, spill-register, round-robin tree and leading-zero behavior selected by FlooNoC |
| axi | `https://github.com/pulp-platform/axi.git` | `a256a3b86394fedf19e361047fccfdd7f6ef83e4` (0.39.9) | AXI configuration and payload-width arithmetic selected by FlooNoC |

The revisions are taken from the frozen FlooNoC `Bender.lock`. Changing any
one is a model scope change and requires the relevant RTL cross-checks to be
rerun.

## Derived and original files

The installed headers carrying `SPDX-License-Identifier: SHL-0.51` are
SystemC representations derived from the named FlooNoC/common_cells/axi
behavior. They are modified, language-translated works rather than verbatim RTL
copies. The mapping from each header to its source module is maintained in
`docs/RTL_MAPPING.md`.

The following installed integration/reference files are original CDC-VP code
under Apache-2.0 and have no direct RTL counterpart:

- `include/floo_noc_model/noc_interconnect.h`;
- `include/floo_noc_model/axi_lanes.hpp`;
- `include/floo_noc_model/reference_model.hpp`;
- `src/noc_interconnect.cpp`.

Test and trace-harness files retain their own SPDX identifiers. They are not
installed as part of the `cdc-components` binary/development package.

## Verification and redistribution

The frozen model is checked against unmodified RTL with twelve independent
SystemC-to-RTL trace runners. The complete evidence and known boundaries are in
`docs/STATUS.md` and `docs/AI_HANDOFF_CONTEXT.md`.

Redistributions of the installed component must retain:

- `Apache-2.0.txt`;
- `SHL-0.51.txt`;
- this provenance record;
- the source-file SPDX notices and the CDC-VP `NOTICE`.

External SystemC and CPU runtime licences belong to the platform bundle that
ships those binaries, not to `libnoc_interconnect.a` itself.
