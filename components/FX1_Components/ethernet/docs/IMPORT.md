# Import audit — 2026-11-02

All 243 delivery files are retained or mapped in source-map.json. The source
in Downloads is unchanged. No model/driver/test/firmware logic was changed.
Root/component CMake files and release documents are archived under upstream;
originals of adapted build helpers are under upstream/relocated.

Intentional edits to mapped files:
- `examples/cdc_vp/CMakeLists.txt`
- `examples/cdc_vp/run_in_docker.sh`
- `examples/eth_soc/CMakeLists.txt`
- `scripts/run_regression.sh`
- `tests/CMakeLists.txt`

New root CMakeLists.txt, Makefile and README.md provide the relocated layout.
Parent CDC-VP CMake and fx1_soc wiring have not been changed. Original hashes
refer to the original directory layout; do not run that manifest at this root.
The complete source inventory was checked against the delivered manifest.
The staged build passed all 7 host CTest cases with GCC 11.5.0 / SystemC 2.3.4.
