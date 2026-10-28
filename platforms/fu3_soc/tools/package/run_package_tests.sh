#!/bin/bash
# VP_FU3_V1.0 package checks: run from anywhere, uses only the package
# (vp/bin, vp/configs). Exit 0 when every check passes.
#   run_package_tests.sh            all checks (GDB check if riscv-none-elf-gdb is found)
#   GDB=/path/to/riscv-none-elf-gdb run_package_tests.sh
set -u
CFG=$(cd "$(dirname "$0")" && pwd)
VP=$(cd "$CFG/../../bin/VP_FU3_V1.0" && pwd)/vp_fu3_soc
FW=$CFG/fw
DATA=$CFG/data
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
pass=0 fail=0

# check NAME EXPECTED_RC REGEX [FORBIDDEN_REGEX] -- command...
check() {
    local name=$1 rc_want=$2 want=$3 forbid=$4
    shift 5
    "$@" > "$WORK/out.txt" 2>&1
    local rc=$?
    if [ "$rc" -ne "$rc_want" ]; then
        echo "FAIL $name: exit $rc, expected $rc_want"; tail -5 "$WORK/out.txt"; fail=$((fail + 1)); return
    fi
    if [ -n "$want" ] && ! tr '\n' ' ' < "$WORK/out.txt" | grep -Eq -- "$want"; then
        echo "FAIL $name: output does not match: $want"; tail -5 "$WORK/out.txt"; fail=$((fail + 1)); return
    fi
    if [ -n "$forbid" ] && grep -Eq -- "$forbid" "$WORK/out.txt"; then
        echo "FAIL $name: output matches the forbidden: $forbid"; fail=$((fail + 1)); return
    fi
    echo "ok   $name"; pass=$((pass + 1))
}

check "version" 0 "VP_FU3_V1.0 .*FU3_VP_ABI_1" "" -- "$VP" --version
check "self-test" 0 "self-test: [0-9]+ checks, 0 failures -> PASS" "" -- "$VP" --self-test
check "bus map" 0 "FU3 Common Spec v2.1" "" -- "$VP" --list-instances
check "no ROM image" 3 "no ROM image: Ibex resets at 0x0000_0080" "" -- "$VP"
check "debug partition refused" 3 "touches the Ibex debug partition" "" -- "$VP" --rom "$FW/fu3_t20_console.elf" \
    --load "0x60000:$DATA/t20_flash.bin"

# Console demo: straps, console input, NOR through XIP, DSRAM dump.
check "console demo" 0 "straps MODE=10 BOOT=1 DEBUG_EN=0 valid=1 .*rx: hello vp .*flash 0x44332211 .*vp_fu3_soc: PASS" \
    "Warning|Error" -- "$VP" --rom "$FW/fu3_t20_console.elf" --mode 10 --boot-pin 1 \
    --uart-rx-file "$DATA/t20_rx.txt" --uart-rx-delay-us 400 --flash "$DATA/t20_flash.bin" \
    --dump "0x0103f000:4:$WORK/signature.bin" --expect-uart PASS --fail-uart FAIL --timeout-ms 20
if [ "$(od -An -tx1 "$WORK/signature.bin" 2>/dev/null | tr -d ' \n')" = "20eeffc0" ]; then
    echo "ok   console demo dump"; pass=$((pass + 1))
else
    echo "FAIL console demo dump: $(od -An -tx1 "$WORK/signature.bin" 2>/dev/null)"; fail=$((fail + 1))
fi
check "fail verdict wins" 1 "vp_fu3_soc: FAIL \\(console printed \"FAIL\"\\)" "" -- "$VP" --rom "$FW/fu3_t20_console.elf" \
    --uart-rx-file "$DATA/t20_rx_pass_fail.txt" --uart-rx-delay-us 400 --expect-uart PASS --fail-uart FAIL --timeout-ms 20

# Both CPUs: WFI wakes, SWIRQ, watchdog reset during traffic.
check "wake and watchdog reset" 0 "cva6 wfi 0x80000007 .*T21 boot 2 .*reset cause 0x00000009 .*vp_fu3_soc: PASS" \
    "Warning|Error" -- "$VP" --rom "$FW/fu3_t21_ibex.elf" --elf "$FW/fu3_t21_cva6.elf" --expect-uart PASS \
    --fail-uart FAIL --timeout-ms 50
# Ibex software reset, CVA6 handoff and FP.
check "handoff and FP" 0 "T19 boot 2 .*cva6 result 0x40580000 .*vp_fu3_soc: PASS" "Warning|Error" -- \
    "$VP" --rom "$FW/fu3_t19_ibex.elf" --elf "$FW/fu3_t19_cva6.elf" --expect-uart PASS --timeout-ms 50

# GDB on CVA6 (needs riscv-none-elf-gdb): breakpoint, FP registers, a register write the firmware uses.
GDB=${GDB:-$(command -v riscv-none-elf-gdb || true)}
if [ -n "$GDB" ] && [ -x "$GDB" ]; then
    "$VP" --rom "$FW/fu3_t19_ibex.elf" --elf "$FW/fu3_t19_cva6.elf" --gdb-cva6 0 --expect-uart PASS \
        > "$WORK/vp_gdb.txt" 2>&1 &
    vp_pid=$!
    port=""
    for _ in $(seq 100); do
        port=$(sed -n 's/.*GDB server for CVA6 (hart 0) on 127.0.0.1:\([0-9]*\).*/\1/p' "$WORK/vp_gdb.txt")
        [ -n "$port" ] && break
        sleep 0.1
    done
    timeout 120 "$GDB" -nx -batch -ex "set confirm off" -ex "set remotetimeout 30" -ex "target remote 127.0.0.1:$port" \
        -ex "break t19_fp_stop" -ex "continue" -ex "info registers fa0 fa2" -ex "set \$fa2 = 10" -ex "detach" \
        "$FW/fu3_t19_cva6.elf" > "$WORK/gdb.txt" 2>&1
    wait "$vp_pid"
    vp_rc=$?
    if [ "$vp_rc" -eq 0 ] && grep -Eq "fa0 +1\.5" "$WORK/gdb.txt" && grep -Eq "fa2 +3\.375" "$WORK/gdb.txt" &&
       grep -q "cva6 result 0x41200000" "$WORK/vp_gdb.txt"; then
        echo "ok   GDB session on CVA6"; pass=$((pass + 1))
    else
        echo "FAIL GDB session on CVA6 (vp exit $vp_rc)"; tail -5 "$WORK/gdb.txt"; fail=$((fail + 1))
    fi
else
    echo "skip GDB session (riscv-none-elf-gdb not found; set GDB=...)"
fi

echo "VP_FU3_V1.0 package checks: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
