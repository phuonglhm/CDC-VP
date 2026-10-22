#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write BUILD_MANIFEST.json of the VP_FX1_V3.0 delivery (package_sdk.sh).

Records what the package was built from and what it contains, from the
sources themselves: the CDC-VP revision now and the one compiled into the
binary (`fx1_soc --version`), the tree state, compilers, the VP++ pin and
patch series, the FreeRTOS pin, the memory and IRQ map parsed from
fx1_memory_map.h, the samples with their golden hashes, and the SHA-256 of
every binary artefact (including the firmware outputs the SDK repository does
not track). Python stdlib only.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess

VERSION = "VP_FX1_V3.0"


def sh(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode:
        raise SystemExit("write_manifest: %s failed:\n%s" % (" ".join(cmd), r.stdout))
    return r.stdout


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def memory_map(header):
    text = open(header).read()
    val = lambda name: int(re.search(r"#define\s+%s\s+FX1_U32\((0x[0-9A-Fa-f]+|\d+)\)" % name, text).group(1), 0)
    regions = []
    for name, size in (("BOOTROM", "FX1_BOOTROM_SIZE"), ("SRAM", "FX1_SRAM_SIZE"), ("CLINT", "FX1_CLINT_SIZE"),
                       ("PLIC", "FX1_PLIC_SIZE"), ("UART", "FX1_APB_SLOT_SIZE"), ("SYS_DMA_CSR", "FX1_APB_SLOT_SIZE"),
                       ("SIM_CTRL", "FX1_APB_SLOT_SIZE"), ("I2C0", "FX1_APB_SLOT_SIZE"), ("ISP_CSR", "FX1_APB_SLOT_SIZE"),
                       ("MIPI_CSR", "FX1_APB_SLOT_SIZE"),
                       ("DDR3_CSR", "FX1_DDR3_CSR_APERTURE_SIZE"), ("DDR_SYSCTRL", "FX1_DDR_SYSCTRL_SIZE"),
                       ("DDR", "FX1_DDR_SIZE")):
        regions.append({"name": name, "base": "0x%08x" % val("FX1_%s_BASE" % name), "size": "0x%08x" % val(size)})
    irqs = {m.group(1): int(m.group(2)) for m in re.finditer(r"#define FX1_IRQ_(\w+)\s+FX1_U32\((\d+)\)", text)}
    return {
        "status": "VP placeholder values, pending the HAS; firmware must use fx1_memory_map.h, not literals",
        "regions": regions,
        "plic_sources": irqs,
        "harts": val("FX1_NUM_HARTS"),
        "mtime_hz": val("FX1_MTIME_HZ"),
        "firmware_image": {"base": "0x%08x" % val("FX1_FW_IMAGE_BASE"), "size": "0x%08x" % val("FX1_FW_IMAGE_SIZE")},
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True, help="package root (vp/, sw/)")
    ap.add_argument("--source", required=True, help="CDC-VP source tree")
    ap.add_argument("--build-dir", required=True)
    ap.add_argument("--tooldir", required=True)
    ap.add_argument("--samples", required=True, help="JSON list written by package_sdk.sh")
    ap.add_argument("--manifest", required=True)
    a = ap.parse_args()

    rel = lambda p: os.path.relpath(p, a.out)
    binp = os.path.join(a.out, "vp", "bin", VERSION, "fx1_soc")
    ver = sh([binp, "--version"])
    m = re.search(r"CDC-VP revision (\S+) \((\w+) at configure\)", ver)
    bt = re.search(r"build type +([^,]+), (.*)", ver)
    revision = sh(["git", "-C", a.source, "rev-parse", "HEAD"]).strip()
    status = sh(["git", "-C", a.source, "status", "--porcelain"]).strip()
    kernel = os.path.join(a.source, "third_party", "FreeRTOS-Kernel")
    cache = open(os.path.join(a.build_dir, "CMakeCache.txt")).read()
    cache_val = lambda k: (re.search(r"^%s:\w+=(.*)$" % k, cache, re.M) or [None, ""])[1]
    patches_dir = os.path.join(a.source, "cpu_models", "riscv_vp_plusplus", "patches")
    vpp_rev = re.search(r'set\(RISCV_VP_PLUSPLUS_REVISION "([0-9a-f]+)"',
                        open(os.path.join(a.source, "cpu_models", "riscv_vp_plusplus", "CMakeLists.txt")).read())

    artefacts = {}
    for d in ("vp/bin/%s" % VERSION, "sw/bootloader/build/%s" % VERSION):
        for name in sorted(os.listdir(os.path.join(a.out, d))):
            p = os.path.join(a.out, d, name)
            if os.path.isfile(p):
                artefacts[rel(p)] = {"sha256": sha256(p), "bytes": os.path.getsize(p),
                                     "tracked_by_sdk_repo": not d.startswith("sw/bootloader/build")}

    manifest = {
        "schema": "cdc-vp/fx1-sdk-manifest/1",
        "package": VERSION,
        "platform": "fx1_soc",
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "summary": "FX1 SoC VP: 2 x RV32IMAFC (VP++) harts, FX1 bus, CLINT, PLIC, PL011-style UART, SYS_DMA, "
                   "FX1 ISP, DDR3 controller (ECC, preinitialized by the VP before the harts start). "
                   "FreeRTOS V11.2.0 CLI on hart 0, boot IP check on hart 1. "
                   "Not a superset of VP_FX1_V2.0: different CPU, ABI and memory map; see README.md.",
        "source": {
            "repository": "CDC-VP",
            "package_revision": revision,
            "dirty_at_package_time": bool(status),
            "binary_revision": m.group(1) if m else "unknown",
            "binary_tree_state_at_configure": m.group(2) if m else "unknown",
            "revision_matches_binary": bool(m) and m.group(1) == revision,
        },
        "build": {
            "vp_build_type": bt.group(1) if bt else "unknown",
            "host_cxx": bt.group(2).strip() if bt else "unknown",
            "cmake_build_type": cache_val("CMAKE_BUILD_TYPE"),
            "systemc_home": cache_val("SYSTEMC_HOME"),
            "cross_gcc": sh([os.path.join(a.tooldir, "riscv-none-elf-gcc"), "--version"]).splitlines()[0],
            "firmware_abi": "-march=rv32imafc_zicsr_zifencei -mabi=ilp32f -mcmodel=medany",
        },
        "upstream": {
            "riscv_vp_plusplus": {
                "base_revision": vpp_rev.group(1) if vpp_rev else "unknown",
                "patches": {f: sha256(os.path.join(patches_dir, f)) for f in sorted(os.listdir(patches_dir))},
            },
            "freertos_kernel": {"version": "V11.2.0",
                                "commit": sh(["git", "-C", kernel, "rev-parse", "HEAD"]).strip()},
            "systemc": "2.3.4 (Accellera), shipped as %s" % next(
                (n for n in os.listdir(os.path.join(a.out, "vp", "bin", VERSION)) if n.startswith("libsystemc")), "?"),
        },
        "memory_map": memory_map(os.path.join(a.source, "components", "FX1_Components", "fx1_map", "include", "fx1",
                                              "fx1_memory_map.h")),
        "modeled": ["2 harts RV32IMAFC M-mode (VP++)", "FX1 bus with exclusive monitor", "CLINT", "PLIC",
                    "UART (PL011-style) + host TCP bridge", "SYS_DMA", "FX1 ISP",
                    "DDR3 controller (NATIVE_R2, DDR3-1600, 2 ranks 4 Gb x8, ECC; CSR, init, training, scrub, "
                    "power, error injection from the host)", "DDR_SYSCTRL sideband glue (VP only)", "boot SRAM",
                    "sim-control finisher (VP only)", "I2C0 host (functional)",
                    "camera with --camera: CSI-2 RX controller + DMA (MIPI_DMA), detailed D-PHY RX model, "
                    "CCS-subset camera sensor model on I2C0"],
        "absent": ["NPU", "H.264/H.265", "Ethernet", "SPI/QSPI", "timers/WDT/PWM", "GPIO", "RTC",
                   "ADC", "TRNG", "OTP", "PMU/CMU/clock-reset", "caches", "MMU"],
        "camera": {
            "built": "CSI-2 RX" in ver,
            "option": "--camera [--camera-raw FILE]",
            "dphy_model": "mipirxphy, converted to SystemC from RTL/netlists/analog models",
            "dphy_model_provenance": "pending: supplier, owner and original licence not confirmed",
            "firmware": "camera manager (CSI-2 -> ISP -> NV12), FreeRTOS CLI `camera` commands, "
                        "camera and camera -> ISP examples",
            "open_items": ["RAW12 zero-copy writes the ISP's IDMA_BUF_ADDR outside the ISP driver: "
                           "to be confirmed by the ISP owner"],
        },
        "isp_samples": json.load(open(a.samples)),
        "artefacts": artefacts,
    }
    with open(a.manifest, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")


if __name__ == "__main__":
    main()
