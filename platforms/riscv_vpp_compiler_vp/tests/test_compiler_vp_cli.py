#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Phase 4.5 platform gate for `riscv_vpp_compiler_vp`.

Covers the parts of the gate that live above the C++ unit level: the two
required demonstrations, what `--print-config` is obliged to prove, and the
refusals — because a handoff tool's most important property is that it fails
rather than quietly running something else.

Most negative cases are built here rather than committed:

  * the *malformed* images are byte-patched copies of a good one, so each
    differs from a working image in exactly the field under test. A committed
    bad ELF drifts from the good one it is supposed to be a variant of, and then
    a refusal can pass for the wrong reason;
  * the *behavioural* probes (a guest that fails, one that traps, one that never
    ends, one that declares traffic it never makes) are compiled from source
    here with the shipped SDK. Writing them by hand as binaries would hide
    whether the SDK can still build them.

Anything needing the cross toolchain skips with 77 when it is absent: the
toolchain is a host prerequisite, not a defect in this repository.

usage: test_compiler_vp_cli.py <binary> <configs> <fw-build-dir> <scratch>
                              <tooldir> <map-include>
"""

import os
import re
import shutil
import struct
import subprocess
import sys

SKIP = 77

EXIT_PASS = 0
EXIT_GUEST_FAILED = 1
EXIT_USAGE = 2
EXIT_IMAGE_REJECTED = 3
EXIT_WATCHDOG = 4
EXIT_TRAFFIC_MISSING = 6

failures = []
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        failures.append(message)
        print(f"FAIL: {message}", file=sys.stderr)


def run(binary, *args, timeout=600):
    result = subprocess.run(
        [binary, *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )
    return result


def expect(result, code, label):
    check(
        result.returncode == code,
        f"{label}: exit code is {result.returncode}, expected {code}\n"
        f"--- stdout ---\n{result.stdout}\n--- stderr ---\n{result.stderr}",
    )


def expect_mentions(result, needle, label):
    haystack = result.stdout + result.stderr
    check(
        needle.lower() in haystack.lower(),
        f"{label}: the diagnostic does not mention {needle!r}\n{haystack}",
    )


# ── ELF byte patching ────────────────────────────────────────────────────────
#
# ELF32 little-endian header offsets, and the program-header entry layout. Named
# rather than written as magic numbers at the call sites: a patch to the wrong
# offset produces a differently-broken image that is still refused, and the test
# would pass while checking nothing.

E_TYPE = 16
E_MACHINE = 18
E_FLAGS = 36
E_PHOFF = 28
E_PHENTSIZE = 42
E_PHNUM = 44
EI_CLASS = 4

PH_TYPE = 0
PH_PADDR = 12
PT_LOAD = 1


def patch(source, destination, edits):
    """Copy `source` to `destination`, applying (offset, format, value) edits."""
    data = bytearray(open(source, "rb").read())
    for offset, fmt, value in edits:
        struct.pack_into(fmt, data, offset, value)
    open(destination, "wb").write(bytes(data))
    return destination


def first_load_segment_offset(path):
    data = open(path, "rb").read()
    phoff = struct.unpack_from("<I", data, E_PHOFF)[0]
    phentsize = struct.unpack_from("<H", data, E_PHENTSIZE)[0]
    phnum = struct.unpack_from("<H", data, E_PHNUM)[0]
    for i in range(phnum):
        entry = phoff + i * phentsize
        if struct.unpack_from("<I", data, entry + PH_TYPE)[0] == PT_LOAD:
            return entry
    raise AssertionError(f"{path} has no PT_LOAD segment")


# ── building probe images with the shipped SDK ───────────────────────────────

PROBE_FAIL = """
#include "host_io.h"
int main(void) { hio_putline("probe: about to fail"); return 42; }
"""

PROBE_TRAP = """
#include "host_io.h"
int main(void)
{
    /* 0x4000_0000 is unmapped: below RAM, above the host-I/O window. The
     * decoder refuses it, the hart takes a load access fault, and crt0's trap
     * entry reports it. */
    volatile unsigned int *nowhere = (volatile unsigned int *)0x40000000u;
    hio_putline("probe: about to touch unmapped memory");
    return (int)*nowhere;
}
"""

PROBE_SPIN = """
#include "host_io.h"
int main(void)
{
    hio_putline("probe: spinning");
    /* Retires instructions for ever *and* advances simulated time, so which
     * watchdog fires is decided by which limit the run was given. That is what
     * makes it usable for both cases. */
    for (;;) {
    }
}
"""

PROBE_WINDOW = """
#include "host_io.h"
int main(void)
{
    /* Declares traffic and then makes none. The measurement window must catch
     * it: this is the same failure shape as a model servicing vector accesses
     * from a direct pointer, with the model half held correct. */
    hio_mark_begin(1);
    hio_expect_accesses(100000u);
    hio_mark_end();
    hio_putline("probe: window closed");
    return 0;
}
"""

PROBE_SIGNATURE = """
#include "host_io.h"

/* A signature block located by linker symbols, the riscv-arch-test convention
 * that `--dump-signature` implements. */
extern unsigned int begin_signature[];
extern unsigned int end_signature[];
unsigned int signature_area[4]
    __attribute__((section(".vdata"), aligned(64), used));
__asm__(".global begin_signature\\n"
        ".set begin_signature, signature_area\\n"
        ".global end_signature\\n"
        ".set end_signature, signature_area + 16\\n");

int main(void)
{
    signature_area[0] = 0xdeadbeefu;
    signature_area[1] = 0x00c0ffeeu;
    signature_area[2] = 0x12345678u;
    signature_area[3] = 0xa5a5a5a5u;
    hio_putline("probe: signature written");
    return 0;
}
"""


def build_probe(name, source, scratch, fw_dir, tooldir, include,
                extra_flags=()):
    directory = os.path.join(scratch, name)
    os.makedirs(directory, exist_ok=True)
    main_c = os.path.join(directory, "main.c")
    with open(main_c, "w", encoding="utf-8") as handle:
        handle.write(source)

    elf = os.path.join(directory, f"{name}.elf")
    common = os.path.join(fw_dir, "common")

    command = [
        os.path.join(tooldir, "riscv-none-elf-gcc"),
        "-march=rv32gcv_zvl512b",
        "-mabi=ilp32d",
        "-O2",
        "-std=c11",
        "-ffreestanding",
        "-nostdlib",
        "-nostartfiles",
        "-fno-builtin",
        "-fno-common",
        f"-I{common}",
        f"-I{include}",
        *extra_flags,
        "-static",
        "-T",
        os.path.join(common, "link.ld"),
        "-Wl,--build-id=none",
        "-Wl,--gc-sections",
        "-o",
        elf,
        os.path.join(common, "crt0.S"),
        os.path.join(common, "host_io.c"),
        main_c,
    ]
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    if result.returncode != 0:
        raise AssertionError(f"building probe {name} failed:\n{result.stdout}")
    return elf


# ── the checks ───────────────────────────────────────────────────────────────


def check_identity(binary):
    version = run(binary, "--version")
    expect(version, EXIT_PASS, "--version")
    text = version.stdout

    # Phase 4.5's packaging gate: --print-config must prove rv32gcv_zvl512b,
    # ilp32d, RVV 1.0, XLEN 32, VLEN 512, ELEN 64, vlenb=64, one hart, no NoC.
    for needle in [
        "rv32gcv_zvl512b",
        "ilp32d",
        "XLEN            : 32",
        "RVV             : 1.0",
        "VLEN            : 512 bits",
        "ELEN            : 64 bits",
        "vlenb           : 64",
        "harts           : 1",
        "No NoC",
    ]:
        check(needle in text, f"--version does not report {needle!r}:\n{text}")

    # ...and the provenance a result can be traced with.
    check("VP++ base       :" in text, "--version does not name the VP++ base")
    check(
        re.search(r"VP\+\+ base       : [0-9a-f]{40}", text) is not None,
        f"--version does not report a full VP++ base SHA:\n{text}",
    )
    check(
        "upstream-backport" in text and "downstream-conformance" in text,
        f"--version does not distinguish the two kinds of VP++ patch:\n{text}",
    )
    check(
        len(re.findall(r"[0-9a-f]{64}", text)) >= 3,
        f"--version does not report a hash for every patch:\n{text}",
    )
    check("CDC-VP revision :" in text, "--version does not name the CDC-VP revision")
    check("SystemC         :" in text, "--version does not name SystemC")
    check(
        "not pipeline- or cycle-accurate" in text.lower(),
        f"--version does not disclaim cycle accuracy:\n{text}",
    )

    printed = run(binary, "--print-config")
    expect(printed, EXIT_PASS, "--print-config")
    check(
        "rv32gcv_zvl512b" in printed.stdout and "this invocation" in printed.stdout,
        f"--print-config does not report both the machine and the run:\n"
        f"{printed.stdout}",
    )
    # An override must be visible in what the tool says it would do, or
    # `--print-config` is describing a different run from the one that follows.
    overridden = run(binary, "--print-config", "--hart-id", "5",
                     "--ram-size", "8MiB")
    check(
        "hart id         : 5" in overridden.stdout
        and "RAM size        : 8388608" in overridden.stdout,
        f"--print-config ignores command-line overrides:\n{overridden.stdout}",
    )

    helped = run(binary, "--help")
    expect(helped, EXIT_PASS, "--help")
    check("--dump-signature" in helped.stdout, "--help omits --dump-signature")
    check("Exit codes:" in helped.stdout, "--help omits the exit codes")


def check_demonstrations(binary, scalar_elf, vector_elf, configs):
    scalar = run(binary, "--elf", scalar_elf)
    expect(scalar, EXIT_PASS, "scalar_hello")
    for line in [
        "Hello from RISC-V VP++ RV32GCV",
        "XLEN=32",
        "hart_id=0",
        "SCALAR HELLO: PASS",
    ]:
        check(line in scalar.stdout,
              f"scalar_hello did not print {line!r}:\n{scalar.stdout}")

    vector = run(binary, "--elf", vector_elf)
    expect(vector, EXIT_PASS, "rvv_vector_add")
    for line in ["RVV=1.0", "VLEN=512", "vlenb=64", "VECTOR ADD: PASS"]:
        check(line in vector.stdout,
              f"rvv_vector_add did not print {line!r}:\n{vector.stdout}")

    # Traffic invariants, checked on the vector run because it is the one with
    # enough of everything to be conclusive.
    report = vector.stdout
    fetches = int(re.search(r"executable-segment R\s+(\d+)", report).group(1))
    data = int(re.search(r"RAM data\s+(\d+)", report).group(1))
    host_io = int(re.search(r"host I/O\s+(\d+)", report).group(1))
    dmi = int(re.search(r"DMI requests \(refused\)\s+(\d+)", report).group(1))
    retired = int(re.search(r"instructions  : (\d+)", report).group(1))

    check(dmi == 0, f"the run made {dmi} DMI requests; the backend must make none")
    check(host_io > 0, "no host-I/O MMIO reached the bus")
    # Two loads and a store per element, 1024 elements, and that is only the
    # measured loop.
    check(data >= 3 * 1024,
          f"only {data} RAM data accesses; the vector loop alone needs 3072")
    # With no decode cache and no DMI, every retired instruction costs a fetch.
    # `>=` rather than `==` leaves room for an instruction split across a fetch;
    # in practice the two are equal and a large gap means something is
    # servicing instruction reads off the bus.
    check(fetches >= retired,
          f"{fetches} instruction fetches for {retired} retired instructions; "
          "instruction fetch is not reaching the bus")

    # A configuration file must be able to run the same image.
    tight = run(binary, "--config", os.path.join(configs, "tight_watchdog.yaml"),
                "--elf", vector_elf)
    expect(tight, EXIT_PASS, "rvv_vector_add under tight_watchdog.yaml")
    check("VECTOR ADD: PASS" in tight.stdout,
          "the tight watchdog configuration cannot run the shipped vector example")

    # `--hart-id` has to reach the ISS, not just the banner: scalar_hello
    # compares `mhartid` against the platform's identity register and fails if
    # they differ.
    other = run(binary, "--elf", scalar_elf, "--hart-id", "3")
    expect(other, EXIT_PASS, "scalar_hello with --hart-id 3")
    check("hart_id=3" in other.stdout,
          f"--hart-id 3 did not reach mhartid:\n{other.stdout}")

    traced = run(binary, "--elf", scalar_elf, "--trace", "20")
    expect(traced, EXIT_PASS, "scalar_hello --trace")
    check(traced.stderr.count("[trace]") == 20,
          f"--trace 20 produced {traced.stderr.count('[trace]')} lines")
    check("host I/O" in traced.stderr or "executable segment" in traced.stderr,
          f"--trace does not classify transactions:\n{traced.stderr[:2000]}")


def check_usage_refusals(binary, scalar_elf, configs, scratch):
    expect(run(binary, "--nonsense"), EXIT_USAGE, "unknown option")
    expect_mentions(run(binary, "--nonsense"), "--nonsense", "unknown option")

    expect(run(binary), EXIT_USAGE, "no --elf")
    expect(run(binary, "--elf"), EXIT_USAGE, "--elf with no value")
    expect(run(binary, "--hart-id", "abc", "--elf", scalar_elf), EXIT_USAGE,
           "--hart-id with a non-number")
    expect(run(binary, "--ram-size", "1K", "--elf", scalar_elf), EXIT_USAGE,
           "--ram-size below the minimum")
    # 4 GiB, not 4 TiB: `TiB` is not an accepted suffix, so that spelling would
    # be refused for the wrong reason and the address-space bound below it would
    # never run.
    result = run(binary, "--ram-size", "4GiB", "--elf", scalar_elf)
    expect(result, EXIT_USAGE, "--ram-size beyond the address space")
    expect_mentions(result, "4 GiB RV32 address space",
                    "--ram-size beyond the address space")
    expect(run(binary, "--ram-size", "64MiX", "--elf", scalar_elf), EXIT_USAGE,
           "--ram-size with an unknown suffix")
    expect(run(binary, "--max-instructions", "0", "--elf", scalar_elf), EXIT_USAGE,
           "--max-instructions 0")
    expect(run(binary, "--timeout", "0", "--elf", scalar_elf), EXIT_USAGE,
           "--timeout 0")
    expect(run(binary, "--timeout", "5fortnights", "--elf", scalar_elf), EXIT_USAGE,
           "--timeout with an unknown unit")
    expect(run(binary, "--config", os.path.join(scratch, "absent.yaml"),
               "--elf", scalar_elf), EXIT_USAGE, "a missing configuration file")

    bad_key = os.path.join(scratch, "bad_key.yaml")
    with open(bad_key, "w", encoding="utf-8") as handle:
        handle.write("platform: riscv_vpp_compiler_vp\nram_sixe_bytes: 8MiB\n")
    result = run(binary, "--config", bad_key, "--elf", scalar_elf)
    expect(result, EXIT_USAGE, "a typo in a configuration key")
    expect_mentions(result, "ram_sixe_bytes", "a typo in a configuration key")

    wrong_platform = os.path.join(scratch, "wrong_platform.yaml")
    with open(wrong_platform, "w", encoding="utf-8") as handle:
        handle.write("platform: tpu_v3_soc\n")
    expect(run(binary, "--config", wrong_platform, "--elf", scalar_elf),
           EXIT_USAGE, "a configuration for another platform")

    # `--timeout 2ms` and `--timeout 2000000` must mean the same thing.
    for spelling in ["2000000", "2000us", "2ms"]:
        result = run(binary, "--print-config", "--timeout", spelling)
        check("timeout         : 2e+06 ns" in result.stdout
              or "timeout         : 2000000 ns" in result.stdout,
              f"--timeout {spelling} was not read as 2 ms:\n{result.stdout}")

    # NaN and infinity. `std::stod` accepts both, and every comparison against a
    # NaN is false — so `timeout <= 0` did not reject it, `elapsed >= limit`
    # never became true, and the run proceeded with the simulated-time watchdog
    # silently disarmed while `--print-config` still reported it armed. The
    # spellings below are the ones that actually reached that state.
    for spelling in ["nan", "NaN", "inf", "-inf", "1e400"]:
        result = run(binary, "--timeout", spelling, "--elf", scalar_elf)
        expect(result, EXIT_USAGE, f"--timeout {spelling}")
    # ...and once the unit is applied, because `1e308 s` is finite on its own.
    expect(run(binary, "--timeout", "1e308s", "--elf", scalar_elf), EXIT_USAGE,
           "--timeout 1e308s")

    # `std::stoull` accepts a leading minus and wraps it, so these used to
    # become enormous limits rather than errors.
    for option in ["--max-instructions", "--ram-size", "--hart-id",
                   "--wall-timeout"]:
        result = run(binary, option, "-1", "--elf", scalar_elf)
        expect(result, EXIT_USAGE, f"{option} -1")

    # The configuration file must apply the same range check as the command
    # line. `hart_id: 4294967296` was truncated to hart 0, so the run used a
    # different machine than the file described and said nothing.
    overflow = os.path.join(scratch, "hart_overflow.yaml")
    with open(overflow, "w", encoding="utf-8") as handle:
        handle.write("platform: riscv_vpp_compiler_vp\nhart_id: 4294967296\n")
    result = run(binary, "--config", overflow, "--elf", scalar_elf)
    expect(result, EXIT_USAGE, "hart_id beyond 32 bits in a configuration file")
    expect_mentions(result, "mhartid",
                    "hart_id beyond 32 bits in a configuration file")
    # The command-line path already refused it; check it still does, so the two
    # cannot drift apart again.
    expect(run(binary, "--hart-id", "4294967296", "--elf", scalar_elf),
           EXIT_USAGE, "--hart-id beyond 32 bits")

    # A signature request an image cannot satisfy must fail before the run, not
    # after it: finding out afterwards throws away a run that succeeded.
    result = run(binary, "--elf", scalar_elf, "--dump-signature",
                 os.path.join(scratch, "unused.sig"))
    expect(result, EXIT_USAGE, "--dump-signature on an image without the symbols")
    expect_mentions(result, "begin_signature",
                    "--dump-signature on an image without the symbols")
    check(not os.path.exists(os.path.join(scratch, "unused.sig")),
          "the refused signature request still created a file")
    check("SCALAR HELLO: PASS" not in result.stdout,
          "the image ran before the signature request was refused")


def check_image_refusals(binary, scalar_elf, scratch):
    missing = run(binary, "--elf", os.path.join(scratch, "no_such.elf"))
    expect(missing, EXIT_IMAGE_REJECTED, "a missing image")

    not_elf = os.path.join(scratch, "not_an_elf.bin")
    with open(not_elf, "wb") as handle:
        handle.write(b"#!/bin/sh\necho definitely not an ELF\n" * 8)
    result = run(binary, "--elf", not_elf)
    expect(result, EXIT_IMAGE_REJECTED, "a file that is not an ELF")
    expect_mentions(result, "not an ELF", "a file that is not an ELF")

    truncated = os.path.join(scratch, "truncated.elf")
    with open(truncated, "wb") as handle:
        handle.write(open(scalar_elf, "rb").read()[:40])
    expect(run(binary, "--elf", truncated), EXIT_IMAGE_REJECTED,
           "a truncated ELF")

    elf64 = patch(scalar_elf, os.path.join(scratch, "elf64.elf"),
                  [(EI_CLASS, "<B", 2)])
    result = run(binary, "--elf", elf64)
    expect(result, EXIT_IMAGE_REJECTED, "an ELF64 image")
    expect_mentions(result, "ELF64", "an ELF64 image")
    expect_mentions(result, "rv32gcv_zvl512b", "an ELF64 image")

    other_machine = patch(scalar_elf, os.path.join(scratch, "x86.elf"),
                          [(E_MACHINE, "<H", 62)])
    result = run(binary, "--elf", other_machine)
    expect(result, EXIT_IMAGE_REJECTED, "an image for another machine")
    expect_mentions(result, "RISC-V", "an image for another machine")

    shared = patch(scalar_elf, os.path.join(scratch, "dyn.elf"),
                   [(E_TYPE, "<H", 3)])
    result = run(binary, "--elf", shared)
    expect(result, EXIT_IMAGE_REJECTED, "a position-independent image")
    expect_mentions(result, "ET_EXEC", "a position-independent image")

    # EF_RISCV_FLOAT_ABI_SOFT: the image would run and produce wrong numbers,
    # which is exactly why it is refused rather than warned about.
    flags = struct.unpack_from("<I", open(scalar_elf, "rb").read(), E_FLAGS)[0]
    soft = patch(scalar_elf, os.path.join(scratch, "soft_float.elf"),
                 [(E_FLAGS, "<I", flags & ~0x6)])
    result = run(binary, "--elf", soft)
    expect(result, EXIT_IMAGE_REJECTED, "a soft-float image")
    expect_mentions(result, "ilp32d", "a soft-float image")

    load = first_load_segment_offset(scalar_elf)

    outside = patch(scalar_elf, os.path.join(scratch, "outside_ram.elf"),
                    [(load + PH_PADDR, "<I", 0x10000000)])
    result = run(binary, "--elf", outside)
    expect(result, EXIT_IMAGE_REJECTED, "a segment outside RAM")
    expect_mentions(result, "outside the RAM window", "a segment outside RAM")

    over_mmio = patch(scalar_elf, os.path.join(scratch, "over_mmio.elf"),
                      [(load + PH_PADDR, "<I", 0x000F0000)])
    result = run(binary, "--elf", over_mmio)
    expect(result, EXIT_IMAGE_REJECTED, "a segment over the host-I/O window")
    expect_mentions(result, "host-I/O", "a segment over the host-I/O window")

    # ...and a segment placed elsewhere *inside* the 64 MiB window is accepted,
    # so the refusals above are about the map and not about the platform
    # refusing everything it is handed.
    #
    # It is also the fault-loop case, and the reason that detector exists. The
    # entry point still says 0x8000_0000, which is now memory the image never
    # wrote, so the hart executes a zero word, traps, and vectors to `mtvec` —
    # still 0, because the startup code that would have set it never ran.
    # Fetching from 0 faults too. The hart then retires nothing (so the
    # instruction watchdog cannot advance) and never reaches a quantum boundary
    # (so simulated time cannot advance, and `sc_start` cannot return). Before
    # the decoder learned to recognise this, the run hung for as long as anyone
    # was willing to wait, with both watchdogs armed and neither able to fire.
    high = patch(scalar_elf, os.path.join(scratch, "high_but_inside.elf"),
                 [(load + PH_PADDR, "<I", 0x81000000)])
    result = run(binary, "--elf", high, "--max-instructions", "10000",
                 "--timeout", "1ms", "--wall-timeout", "120", timeout=180)
    check(result.returncode != EXIT_IMAGE_REJECTED,
          "a segment inside a 64 MiB RAM was refused as out of map:\n"
          + result.stdout + result.stderr)
    expect(result, EXIT_WATCHDOG, "a guest in an unrecoverable fault loop")
    expect_mentions(result, "fault loop", "a guest in an unrecoverable fault loop")


def check_behaviour_probes(binary, scratch, fw_dir, tooldir, include):
    fail_elf = build_probe("fail_probe", PROBE_FAIL, scratch, fw_dir,
                           tooldir, include)
    result = run(binary, "--elf", fail_elf)
    expect(result, EXIT_GUEST_FAILED, "an image that reports failure")
    check("reported check 42" in result.stdout,
          f"the failing image's status was not reported:\n{result.stdout}")

    trap_elf = build_probe("trap_probe", PROBE_TRAP, scratch, fw_dir,
                           tooldir, include)
    result = run(binary, "--elf", trap_elf)
    expect(result, EXIT_GUEST_FAILED, "an image that traps")
    expect_mentions(result, "trapped", "an image that traps")
    # cause 5 is a load access fault: the decoder refused an unmapped read and
    # VP++ chose the cause from the access origin (decision record D13).
    check("mcause 0x5" in result.stdout,
          f"an unmapped load did not become a load access fault:\n{result.stdout}")
    check(re.search(r"unmapped, refused\s+[1-9]", result.stdout) is not None,
          f"the refused access was not counted:\n{result.stdout}")

    spin_elf = build_probe("spin_probe", PROBE_SPIN, scratch, fw_dir,
                           tooldir, include)

    result = run(binary, "--elf", spin_elf, "--max-instructions", "200000",
                 "--timeout", "10s")
    expect(result, EXIT_WATCHDOG, "the instruction watchdog")
    expect_mentions(result, "instruction limit", "the instruction watchdog")

    result = run(binary, "--elf", spin_elf, "--timeout", "200us",
                 "--max-instructions", "100000000")
    expect(result, EXIT_WATCHDOG, "the simulated-time watchdog")
    expect_mentions(result, "simulated-time limit", "the simulated-time watchdog")
    check("simulated time: 200 us" in result.stdout,
          f"the simulated-time watchdog overshot its limit:\n{result.stdout}")

    # The wall-clock backstop. The spin probe *does* yield to the kernel, so
    # with both simulated bounds set out of reach the only thing that can end
    # the run is the backstop — which is the situation it exists for, and the
    # only way to exercise it without a guest that hangs the kernel outright.
    result = run(binary, "--elf", spin_elf, "--max-instructions", "100000000000",
                 "--timeout", "1000s", "--wall-timeout", "3", timeout=120)
    expect(result, EXIT_WATCHDOG, "the wall-clock backstop")
    expect_mentions(result, "wall-clock backstop", "the wall-clock backstop")

    window_elf = build_probe("window_probe", PROBE_WINDOW, scratch, fw_dir,
                             tooldir, include)
    result = run(binary, "--elf", window_elf)
    expect(result, EXIT_TRAFFIC_MISSING, "declared traffic that never happened")
    expect_mentions(result, "measurement window",
                    "declared traffic that never happened")

    signature_elf = build_probe("signature_probe", PROBE_SIGNATURE, scratch,
                                fw_dir, tooldir, include)
    signature_path = os.path.join(scratch, "probe.sig")
    result = run(binary, "--elf", signature_elf, "--dump-signature",
                 signature_path)
    expect(result, EXIT_PASS, "--dump-signature")
    words = [line.strip() for line in open(signature_path, encoding="utf-8")]
    check(words == ["deadbeef", "00c0ffee", "12345678", "a5a5a5a5"],
          f"the dumped signature is {words}")


def main():
    if len(sys.argv) != 7:
        print(__doc__, file=sys.stderr)
        return EXIT_USAGE

    binary, configs, fw_dir, scratch, tooldir, include = sys.argv[1:7]

    scalar_elf = os.path.join(fw_dir, "examples", "scalar_hello",
                              "scalar_hello.elf")
    vector_elf = os.path.join(fw_dir, "examples", "rvv_vector_add",
                              "rvv_vector_add.elf")

    if not os.path.exists(scalar_elf) or not os.path.exists(vector_elf):
        print("SKIP: the demonstration images have not been built "
              "(no cross toolchain?)", file=sys.stderr)
        return SKIP
    if not os.path.exists(os.path.join(tooldir, "riscv-none-elf-gcc")):
        print(f"SKIP: no cross toolchain at {tooldir}", file=sys.stderr)
        return SKIP

    shutil.rmtree(scratch, ignore_errors=True)
    os.makedirs(scratch, exist_ok=True)

    check_identity(binary)
    check_demonstrations(binary, scalar_elf, vector_elf, configs)
    check_usage_refusals(binary, scalar_elf, configs, scratch)
    check_image_refusals(binary, scalar_elf, scratch)
    check_behaviour_probes(binary, scratch, fw_dir, tooldir, include)

    if failures:
        print(f"\n{len(failures)} of {checks} checks failed", file=sys.stderr)
        return 1
    print(f"riscv_vpp_compiler_vp CLI: {checks} checks PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
