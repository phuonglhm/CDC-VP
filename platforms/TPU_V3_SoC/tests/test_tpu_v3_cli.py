#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Platform-level tests for the tpu_v3_soc executable.

The component unit tests cover the configuration object and the address map.
What they cannot cover is what this file does: the parser, the command-line
overrides, and — most importantly — that a bad configuration is *refused* at
the platform boundary rather than silently normalised.

Roughly half of these are negative controls. A validator that accepts a
128x128 matrix geometry the NPU team has not delivered is worse than no
validator, because the run it permits produces numbers that look exactly like
real ones and carry a name no source backs.

Usage: test_tpu_v3_cli.py <tpu_v3_soc binary> <configs dir>
"""

from __future__ import annotations

import os
import pathlib
import resource
import subprocess
import sys
import tempfile

TIMEOUT_S = 60

failures: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)
        print(f"CHECK failed: {message}", file=sys.stderr)


def run(binary: str, *args: str) -> subprocess.CompletedProcess:
    # LD_LIBRARY_PATH is dropped so the binary must find libsystemc through its
    # own RPATH. A test that passes only because the developer's environment
    # points at /opt would say nothing about the packaged bundle.
    env = dict(os.environ)
    env.pop("LD_LIBRARY_PATH", None)
    return subprocess.run(
        [binary, *args],
        capture_output=True,
        text=True,
        timeout=TIMEOUT_S,
        env=env,
        check=False,
    )


def expect_ok(binary: str, *args: str) -> str:
    result = run(binary, *args)
    check(
        result.returncode == 0,
        f"{' '.join(args)} exited {result.returncode}\n"
        f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
    )
    return result.stdout


def expect_rejected(binary: str, needle: str, *args: str) -> None:
    """The run must fail *and* say why in terms the user can act on."""
    result = run(binary, *args)
    check(
        result.returncode != 0,
        f"{' '.join(args)} should have been rejected but exited 0\n"
        f"stdout:\n{result.stdout}",
    )
    combined = result.stdout + result.stderr
    check(
        needle in combined,
        f"{' '.join(args)} was rejected, but the message does not mention "
        f"{needle!r}:\n{combined}",
    )


def test_help_and_version(binary: str) -> None:
    text = expect_ok(binary, "--help")
    for key in (
        "--config",
        "--chips",
        "--sa-geometry",
        "--core-sram-size",
        "--local-sram-banks",
        "--noc-timing",
        "--print-address-map",
    ):
        check(key in text, f"--help does not document {key}")

    version = expect_ok(binary, "--version")
    for key in (
        "build type",
        "CDC-VP revision",
        "compiler",
        "SA geometry",
        "SystemC",
    ):
        check(key in version, f"--version does not report {key!r}")


def test_shipped_configs_all_run(binary: str, configs: pathlib.Path) -> None:
    found = sorted(configs.glob("*.yaml"))
    check(len(found) == 4, f"expected 4 shipped configs, found {len(found)}")

    for config in found:
        text = expect_ok(binary, "--config", str(config))
        check(config.stem in text, f"{config.name}: report does not name it")
        # Every report must name its timing backend: a latency number quoted
        # without one is unusable.
        check(
            "NoC timing backend" in text,
            f"{config.name}: report does not name the timing backend",
        )
        check("status         : OK" in text, f"{config.name}: no OK status")
        # The geometry must be named, and must be the verified bring-up array.
        # Decision record D14: no build, manifest or report may call it
        # 128x128.
        check(
            "64x64 bring-up" in text,
            f"{config.name}: SA geometry is not reported",
        )
        # A numeric result is not interpretable without its arithmetic.
        check(
            "matrix datatype      : bf16_fp32" in text,
            f"{config.name}: matrix datatype is not reported",
        )
        check(
            "reference capacity" in text,
            f"{config.name}: shipped configs must use the D6 reference core "
            f"SRAM capacity",
        )
        # D15 leaves the fabric's physical values open; wherever they appear
        # they must appear labelled.
        check(
            "provisional" in text,
            f"{config.name}: the open D15 fabric values are not labelled "
            f"provisional",
        )
        # The legacy Phase 1 vocabulary must be gone from generated output.
        for legacy in ("svm", "SVM", "mxu", "MXU"):
            check(
                legacy not in text,
                f"{config.name}: the report still says {legacy!r}",
            )

    names = {c.stem for c in found}
    check(
        names == {"single_core", "single_chip", "mesh_2x2", "mesh_4x4"},
        f"unexpected shipped config set: {sorted(names)}",
    )


def test_largest_config_is_the_documented_maximum(
    binary: str, configs: pathlib.Path
) -> None:
    text = expect_ok(binary, "--config", str(configs / "mesh_4x4.yaml"))
    check("chips                : 8 of 8" in text, "mesh_4x4 is not 8 chips")
    check("hart 15" in text, "mesh_4x4 does not reach hart 15")
    check(
        "matrix engines       : 16" in text,
        "mesh_4x4 is not 16 matrix engines",
    )
    check("DMA engines          : 16" in text, "mesh_4x4 is not 16 DMAs")


def test_address_map_output(binary: str, configs: pathlib.Path) -> None:
    text = expect_ok(
        binary, "--config", str(configs / "mesh_4x4.yaml"), "--print-address-map"
    )
    for needle in (
        "0x00000000",  # boot ROM
        "0x80000000",  # global RAM
        "0xc0000000",  # chip 0 aperture
        "chip7.core1.sa_control",
        "chip7.core1.dma_control",
        "chip7.core1.transform_control",
        "chip0.core0.sram",
        "global.ram",
    ):
        check(needle in text, f"address map output is missing {needle!r}")

    # The Phase 3 migration gate: the legacy names are absent from generated,
    # firmware-visible address output. Historical audits may still quote them.
    for legacy in ("svm", "mxu"):
        check(legacy not in text, f"address map still emits {legacy!r}")

    # 3 global + 8 chips * (2 chip-level + 2 cores * 6 core-level)
    check("(115 regions)" in text, f"unexpected region count:\n{text[:200]}")


def test_window_and_capacity_are_reported_separately(
    binary: str, configs: pathlib.Path
) -> None:
    """Decision record D6: the full window decodes; capacity is separate.

    The failure this guards against is subtle. If the map shrank to the
    instantiated capacity, an address just above the core SRAM would be
    *unmapped* in a bring-up configuration and *valid* in the reference one, so
    the same firmware pointer bug would report a decode error on one run and
    corrupt data on another.
    """
    config = str(configs / "single_chip.yaml")

    # Reference: the window is fully backed, so nothing is annotated.
    reference = expect_ok(binary, "--config", config, "--print-address-map")
    sram_lines = [ln for ln in reference.splitlines() if "core0.sram" in ln]
    check(len(sram_lines) == 1, f"expected one core0.sram line, got {sram_lines}")
    if sram_lines:
        check("0x01000000" in sram_lines[0], "core SRAM window is not 16 MiB")
        check(
            "backed" not in sram_lines[0],
            f"the reference core SRAM should be fully backed: {sram_lines[0]}",
        )

    # Bring-up: same window, smaller storage, and the difference is stated.
    bringup = expect_ok(
        binary,
        "--config",
        config,
        "--core-sram-size",
        "1MiB",
        "--print-address-map",
    )
    sram_lines = [ln for ln in bringup.splitlines() if "core0.sram" in ln]
    check(len(sram_lines) == 1, f"expected one core0.sram line, got {sram_lines}")
    if sram_lines:
        check(
            "0x01000000" in sram_lines[0],
            f"the window must not shrink with the capacity: {sram_lines[0]}",
        )
        check(
            "[backed 1 MiB of 16 MiB" in sram_lines[0],
            f"the backed capacity is not reported: {sram_lines[0]}",
        )
        check("never an alias" in sram_lines[0], "alias policy is not stated")

    # The core control register file must sit immediately above the window in
    # both, which is what proves the window did not move.
    for text in (reference, bringup):
        control = [ln for ln in text.splitlines() if "core0.control" in ln]
        check(len(control) == 1, "expected one core0.control line")
        if control:
            check("0xc1000000" in control[0], f"core control moved: {control[0]}")

    # Global RAM at the 256 MiB default is a partially backed 1 GiB window.
    ram = [ln for ln in reference.splitlines() if "global.ram" in ln]
    check(len(ram) == 1, "expected one global.ram line")
    if ram:
        check("[backed 256 MiB of 1 GiB" in ram[0], f"global RAM: {ram[0]}")


def test_overrides_apply_in_any_order(binary: str, configs: pathlib.Path) -> None:
    config = str(configs / "single_chip.yaml")

    after = expect_ok(binary, "--config", config, "--chips", "3")
    check("chips                : 3 of 8" in after, "--chips after --config")

    # The override must survive being written before --config: parsing in
    # argv order would silently discard it when the file is read.
    before = expect_ok(binary, "--chips", "3", "--config", config)
    check("chips                : 3 of 8" in before, "--chips before --config")

    sized = expect_ok(binary, "--config", config, "--core-sram-size", "8MiB")
    check(
        "core SRAM per core   : 8 MiB" in sized, "--core-sram-size 8MiB"
    )

    hexed = expect_ok(binary, "--config", config, "--core-sram-size", "0x100000")
    check("core SRAM per core   : 1 MiB" in hexed, "--core-sram-size hex")

    banks = expect_ok(binary, "--config", config, "--local-sram-banks", "8")
    check(
        "128-bit x 8 banks" in banks,
        "--local-sram-banks is not reflected in the report",
    )

    named = expect_ok(binary, "--config", config, "--name", "experiment_7")
    check("'experiment_7'" in named, "--name override")

    timing = expect_ok(binary, "--config", config, "--noc-timing", "detailed")
    check(
        "NoC timing backend   : detailed" in timing, "--noc-timing detailed"
    )


def test_frozen_architecture_is_refused(binary: str, configs: pathlib.Path) -> None:
    config = str(configs / "single_chip.yaml")

    # Nine chips: the frozen NoC manager id is 3 bits.
    expect_rejected(binary, "chips", "--config", config, "--chips", "9")

    # A mesh size make_noc() does not instantiate.
    expect_rejected(
        binary, "make_noc", "--config", config, "--mesh-x", "5", "--mesh-y", "5"
    )

    # Four chips on 2x2 leaves no node for the global targets.
    expect_rejected(binary, "NoLoopback", "--config", config, "--chips", "4")

    # The matrix geometry is a *build-time* choice. A configuration asking for
    # a different one must fail naming the CMake variable, because the package
    # manifest records the compiled value: honouring the file silently would
    # make the manifest describe a run it did not describe, and decision
    # record D14 forbids any build, manifest or report calling the 64x64
    # bring-up array 128x128.
    #
    # (The component-level "the promotion gate has not passed" refusal is a
    # separate rule with its own unit test; this platform rule fires first
    # because it is more specific about what to change.)
    expect_rejected(
        binary,
        "TPU_V3_SA_GEOMETRY=64x64",
        "--config",
        config,
        "--sa-geometry",
        "128x128",
    )

    # An unnamed geometry has neither a source nor golden tests.
    expect_rejected(
        binary,
        "TPU_V3_SA_GEOMETRY=64x64",
        "--config",
        config,
        "--sa-geometry",
        "32x32",
    )

    # The datatype is a numeric contract: an unimplemented one must not quietly
    # produce BF16/FP32 results under another name.
    expect_rejected(
        binary,
        "sa.datatype",
        "--config",
        config,
        "--sa-datatype",
        "int8_int32",
    )

    # Core SRAM above its 16 MiB window, and a non-power-of-two capacity.
    expect_rejected(binary, "sram_size_bytes", "--config", config,
                    "--core-sram-size", "32MiB")
    expect_rejected(binary, "power of two", "--config", config,
                    "--core-sram-size", "3145728")

    # D15's open physical values are refused when not stated, and when stated
    # as something no bank decoder would be built from.
    expect_rejected(binary, "power of two", "--config", config,
                    "--local-sram-banks", "6")
    expect_rejected(binary, "no architectural default", "--config", config,
                    "--local-sram-banks", "0")


def test_bad_command_lines(binary: str, configs: pathlib.Path) -> None:
    config = str(configs / "single_chip.yaml")

    expect_rejected(binary, "unknown option", "--not-an-option")
    expect_rejected(binary, "requires a value", "--chips")
    expect_rejected(binary, "cannot open", "--config", "/nonexistent/x.yaml")
    # A typo in an enum must not silently select the default.
    expect_rejected(binary, "detaild", "--config", config,
                    "--noc-timing", "detaild")
    expect_rejected(binary, "suffix", "--config", config,
                    "--core-sram-size", "4Mib!")
    expect_rejected(binary, "ROWSxCOLUMNS", "--config", config,
                    "--sa-geometry", "64by64")


def test_config_file_syntax(binary: str) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)

        good = root / "good.yaml"
        good.write_text(
            "# a comment\n"
            "\n"
            "platform: tpu_v3_soc   # trailing comment\n"
            "name: from_file\n"
            "chips: 2\n"
            "core_sram_size_bytes: 8MiB\n"
        )
        text = expect_ok(binary, "--config", str(good))
        check("'from_file'" in text, "name from file")
        check("chips                : 2 of 8" in text, "chips from file")
        check(
            "core SRAM per core   : 8 MiB" in text,
            "core SRAM size from file",
        )

        unknown = root / "unknown.yaml"
        unknown.write_text("platform: tpu_v3_soc\nsa_rows: 64\n")
        expect_rejected(binary, "unknown configuration key",
                        "--config", str(unknown))

        wrong_platform = root / "wrong.yaml"
        wrong_platform.write_text("platform: noc_soc\n")
        expect_rejected(binary, "another platform",
                        "--config", str(wrong_platform))

        no_colon = root / "nocolon.yaml"
        no_colon.write_text("platform: tpu_v3_soc\nchips 2\n")
        expect_rejected(binary, "expected 'key: value'",
                        "--config", str(no_colon))

        # The realistic nesting mistake: a parent key with indented children.
        # It is caught on the parent line, and the message must point at the
        # structure rather than at a missing number.
        nested = root / "nested.yaml"
        nested.write_text("platform: tpu_v3_soc\nchip:\n  cores: 2\n")
        expect_rejected(binary, "no nesting", "--config", str(nested))

        # An indented `key: value` with no parent, which reaches the other
        # branch of the same rule.
        indented = root / "indented.yaml"
        indented.write_text("platform: tpu_v3_soc\n  chips: 2\n")
        expect_rejected(binary, "indented line", "--config", str(indented))

        empty_value = root / "empty.yaml"
        empty_value.write_text("platform: tpu_v3_soc\nchips:\n")
        expect_rejected(binary, "no value", "--config", str(empty_value))

        # A file with only comments is valid and uses the defaults; its name
        # falls back to the file stem so the run stays traceable.
        comments = root / "comments_only.yaml"
        comments.write_text("# nothing but a comment\n")
        text = expect_ok(binary, "--config", str(comments))
        check("'comments_only'" in text, "name falls back to the file stem")


def test_report_is_honest_about_phase_3(binary: str, configs: pathlib.Path) -> None:
    """A partial platform must not read as a working simulator.

    A platform that printed a full hierarchy while instantiating memories and
    nothing else is exactly how a Phase 3 tree gets mistaken for a Phase 7
    model.
    """
    text = expect_ok(binary, "--config", str(configs / "single_chip.yaml"))
    check("This is the Phase 3 platform" in text, "report does not say which phase")
    check("core SRAM            : yes" in text, "report denies the SRAM exists")
    check("RV32GCV hart         : no" in text, "report claims a hart exists")
    check("Sauria matrix engine : no" in text, "report claims an SA exists")
    check("NEO DMA              : no" in text, "report claims a DMA exists")
    check("NoC                  : no" in text, "report claims a NoC exists")
    check(
        "SystemC elaboration  : yes" in text,
        "report does not confirm the SystemC elaboration ran",
    )


def test_largest_memory_config_costs_almost_nothing(
    binary: str, configs: pathlib.Path
) -> None:
    """The Phase 3 gate: ``mesh_4x4`` elaborates without eager host commitment.

    ``mesh_4x4`` describes 1.25 GiB of logical memory — sixteen 16 MiB core
    SRAMs plus a 1 GiB global RAM. Decision record D6 requires that to cost a
    few megabytes of host memory until firmware writes to it, and a
    ``std::vector`` per target would commit all of it before a single
    instruction ran.

    Both halves are checked. The report says how much backing is allocated,
    which catches a target that quietly pre-touched its pages; the child's peak
    RSS catches the case the report cannot see, where the memory is committed
    somewhere the counters do not know about.
    """
    before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    text = expect_ok(binary, "--config", str(configs / "mesh_4x4.yaml"))
    after = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

    check("logical total        : 1280 MiB" in text,
          f"mesh_4x4 does not describe 1.25 GiB of logical memory:\n{text[-800:]}")
    check("host backing in use  : 0 B" in text,
          "an untouched configuration must commit no backing at all")
    check("sparse pages" in text, "the backing policy is not stated")

    # ru_maxrss is in kilobytes on Linux and is a high-water mark across every
    # child this process has reaped, so it can only be read as an upper bound —
    # which is exactly what is wanted here. 256 MiB is generous: the observed
    # figure is around 10 MiB, and anything approaching the logical size would
    # mean the storage was allocated eagerly.
    peak_kib = max(before, after)
    check(
        peak_kib < 256 * 1024,
        f"a child peaked at {peak_kib // 1024} MiB of RSS; mesh_4x4 describes "
        f"1.25 GiB of logical memory and must not commit it (decision record "
        f"D6)",
    )


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2

    binary = argv[1]
    configs = pathlib.Path(argv[2])

    if not os.access(binary, os.X_OK):
        print(f"not executable: {binary}", file=sys.stderr)
        return 1
    if not configs.is_dir():
        print(f"not a directory: {configs}", file=sys.stderr)
        return 1

    test_help_and_version(binary)
    test_shipped_configs_all_run(binary, configs)
    test_largest_config_is_the_documented_maximum(binary, configs)
    test_address_map_output(binary, configs)
    test_window_and_capacity_are_reported_separately(binary, configs)
    test_overrides_apply_in_any_order(binary, configs)
    test_frozen_architecture_is_refused(binary, configs)
    test_bad_command_lines(binary, configs)
    test_config_file_syntax(binary)
    test_report_is_honest_about_phase_3(binary, configs)
    test_largest_memory_config_costs_almost_nothing(binary, configs)

    if failures:
        print(f"\n{len(failures)} check(s) failed", file=sys.stderr)
        return 1
    print("tpu_v3_soc CLI regression PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
