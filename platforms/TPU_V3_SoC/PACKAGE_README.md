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

**Phase 1 of the implementation plan.** It reads a configuration, validates it
against the frozen architecture, elaborates a SystemC top level, and reports
the machine it is configured for.

It does **not** yet instantiate cores, MXUs, SVM or the NoC. The binary says so
in its own output. Nothing it prints is a simulation result, and no timing
number can be quoted from it, because it does not produce any.

The phases that add those components are listed in
`components/TPU_V3/docs/TPU_V3_IMPLEMENTATION_PLAN.md` in the source
repository.

## Configuration format

A restricted `key: value` subset — `#` comments, one pair per line, no nesting.
It is not YAML, despite the file extension. An unknown key is an error rather
than a warning, so a typo cannot leave a run silently using a default.

Accepted keys: `platform`, `name`, `mesh_x`, `mesh_y`, `chips`,
`svm_size_bytes`, `global_ram_size_bytes`, `mxu_backend`, `mxu_arithmetic`,
`noc_timing`. Sizes accept `K`/`KiB`, `M`/`MiB`, `G`/`GiB` suffixes.

## Limits this build enforces

* at most **8 chips / 16 TPU cores** — the Revision 1 backend limit. The frozen
  FlooNoC chimney manager id is three bits and each chip presents one
  aggregated NoC manager;
* mesh sizes **2x2, 3x3, 4x4, 4x2, 2x4** — the ones `noc_interconnect`
  instantiates;
* one mesh node must be left free for the global targets, because the NoC
  refuses a target on a node that hosts an initiator;
* MXU geometry is fixed at 128x128, two per core, two cores per chip. A
  configuration saying otherwise is rejected, not adapted;
* the **MXU backend is chosen when this binary is compiled**. A configuration
  selecting a different one is refused, so `BUILD_MANIFEST.json` cannot name a
  backend that did not run.

## Memory windows versus capacity

SVM has a 16 MiB window per core and global RAM a 1 GiB window. The instantiated
capacity may be smaller — `--print-address-map` annotates any region whose
window is larger than its storage.

The whole window always decodes. An access above the capacity is an error from
the target, never an alias into valid storage, so the decoded map does not
change with the memory size.

The reference SVM capacity is the full 16 MiB. A smaller value is a bring-up
configuration and the report labels it as one.

## Licences

`licenses/` contains the applicable texts. `BUILD_MANIFEST.json` records which
upstream components are actually linked into this binary and at which pinned
revision. It records two revisions — the one compiled into the binary and the
one the working tree was at when the package was assembled — because they are
captured at different moments and can differ. Either may be `null` when the
package was built outside a Git checkout.
