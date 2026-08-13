# TPU_V3 SoC — portable package

This directory is a self-contained bundle. It does not need the CDC-VP source
tree, the build tree, or a system SystemC installation.

```text
tpu_v3_soc            the executable ($ORIGIN rpath)
libsystemc.so*        the SystemC runtime it loads
configs/              platform configurations
firmware/             firmware images (populated from Phase 10)
licenses/             licence texts and third-party provenance
BUILD_MANIFEST.json   what this was built from
README.md             this file
```

## Running it

```bash
./tpu_v3_soc --config configs/single_chip.yaml
./tpu_v3_soc --config configs/mesh_4x4.yaml --print-address-map
./tpu_v3_soc --help
./tpu_v3_soc --version
```

Command-line options override the configuration file whatever order they
appear in.

## What this build actually does

**Phase 3 of the implementation plan.** It reads a configuration, validates it
against the frozen architecture, elaborates a SystemC top level with one
sparsely page-backed core SRAM per NEO-CORE and the global RAM store, and
reports the machine it is configured for.

It does **not** yet instantiate a hart, the matrix engine, the DMA, the
transform engine or the NoC, and it does not compose the NEO-CORE fabrics —
those exist as tested components in the source tree and are wired into a core
in Phase 7. The binary says so in its own output. Nothing it prints is a
simulation result, and no timing number can be quoted from it, because it does
not produce any.

The phases that add those components are listed in
`components/TPU_V3/docs/TPU_V3_IMPLEMENTATION_PLAN.md` in the source
repository.

## Configuration format

A restricted `key: value` subset — `#` comments, one pair per line, no nesting.
It is not YAML, despite the file extension. An unknown key is an error rather
than a warning, so a typo cannot leave a run silently using a default.

Accepted keys: `platform`, `name`, `mesh_x`, `mesh_y`, `chips`,
`core_sram_size_bytes`, `global_ram_size_bytes`, `sa_geometry`, `sa_datatype`,
`sa_source_revision`, `dma_max_burst_bytes`, `local_sram_data_width_bits`,
`local_sram_banks`, `local_sram_bank_mapping`, `local_sram_pipeline_stages`,
`local_sram_arbitration`, `noc_timing`. Sizes accept `K`/`KiB`, `M`/`MiB`,
`G`/`GiB` suffixes.

## Limits this build enforces

* at most **8 chips / 16 TPU cores** — the Revision 1 backend limit. The frozen
  FlooNoC chimney manager id is three bits and each chip presents one
  aggregated NoC manager;
* mesh sizes **2x2, 3x3, 4x4, 4x2, 2x4** — the ones `noc_interconnect`
  instantiates;
* one mesh node must be left free for the global targets, because the NoC
  refuses a target on a node that hosts an initiator;
* one matrix engine, one DMA and one transform engine per NEO-CORE, two cores
  per chip. A configuration saying otherwise is rejected, not adapted;
* the matrix geometry is the verified **64x64** bring-up array. 128x128 is the
  architectural destination and is refused until the NPU team's promotion gate
  passes; no build, manifest or report may call the bring-up array 128x128;
* the **geometry is chosen when this binary is compiled**. A configuration
  selecting a different one is refused, so `BUILD_MANIFEST.json` cannot name a
  geometry that did not run;
* the local-SRAM datapath width, bank count and pipeline depth have **no
  default**. They are physical values still pending the SRAM macro, clock
  target and PD constraints, so the configuration must state them and the
  report prints them labelled "provisional".

## Memory windows versus capacity

Core SRAM has a 16 MiB window per NEO-CORE and global RAM a 1 GiB window. The
instantiated capacity may be smaller — `--print-address-map` annotates any
region whose window is larger than its storage.

The whole window always decodes. An access above the capacity is an error from
the target, never an alias into valid storage, so the decoded map does not
change with the memory size.

The reference core SRAM capacity is the full 16 MiB. A smaller value is a
bring-up configuration and the report labels it as one.

Behind the capacity there is a third quantity: **host memory**. Storage is
backed sparsely in deterministic 4 KiB pages, so the largest configuration
describes 1.25 GiB of logical memory and commits only the pages something has
actually written. The report prints logical memory and allocated backing side
by side; they are different numbers and neither may be quoted as the other.

## Licences

`licenses/` contains the applicable texts. `BUILD_MANIFEST.json` records which
upstream components are actually linked into this binary and at which pinned
revision. It records two revisions — the one compiled into the binary and the
one the working tree was at when the package was assembled — because they are
captured at different moments and can differ. Either may be `null` when the
package was built outside a Git checkout.
