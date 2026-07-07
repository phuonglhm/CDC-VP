#!/usr/bin/env bash
# run_vp.sh - launch the VP_FX1 virtual SoC with an optional firmware ELF.
#
#   ./run_vp.sh                      # run default uart_hello driver ELF
#   ./run_vp.sh path/to/app.elf      # load ELF at its entry
#   ./run_vp.sh app.elf --sim-ms 20  # extra args pass straight through
#   ./run_vp.sh --no-fw              # elaboration smoke run (no firmware)
#
# ROM-code boot flow (see vp/doc/VP_FX1_SOC/AI_CONTEXT.md):
#   ./run_vp.sh bootrom.elf --int-flash app.bin --boot-pin low
#   ./run_vp.sh bootrom.elf --boot-pin high --uart0-socket 5577 --uart0-wait --sim-ms 60000
#   ./run_vp.sh bootrom.elf --boot-pin high --spi-flash image.bin --sim-ms 300
#
# The binary is self-contained (libsystemc.so* sits next to it, rpath=$ORIGIN);
# no SystemC install is required on the firmware team's host.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../../../.." && pwd)"
bin="$root/vp/bin/VP_FX1_SOC/vp_fx1_full_soc"
cfg="$root/vp/configs/VP_FX1_SOC/default.yaml"
default_fw="$root/sw/drivers/build/VP_FX1_SOC/uart_hello.elf"

fw="$default_fw"
if [[ $# -gt 0 && "$1" == "--no-fw" ]]; then
    fw=""
    shift
elif [[ $# -gt 0 && "$1" != --* ]]; then
    fw="$1"; shift
fi

if [[ ! -x "$bin" ]]; then
    echo "error: VP binary not found or not executable: $bin" >&2
    exit 1
fi

if [[ ! -f "$cfg" ]]; then
    echo "error: VP config not found: $cfg" >&2
    exit 1
fi

if [[ -n "$fw" ]]; then
    if [[ ! -f "$fw" ]]; then
        echo "error: firmware ELF not found: $fw" >&2
        echo "hint: build it with: make -C $root/sw/drivers/sources/VP_FX1_SOC/uart_hello" >&2
        exit 1
    fi
    exec "$bin" -c "$cfg" --fw "$fw" "$@"
else
    exec "$bin" -c "$cfg" --sim-ms 0 "$@"
fi
