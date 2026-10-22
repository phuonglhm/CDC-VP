#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Run the packaged VP_FX1_V3.0 binary (FX1 SoC VP) with its FreeRTOS CLI firmware.
set -euo pipefail
# Scripted console input: one byte per 115200 8N1 character time, starting after
# the FreeRTOS boot (the harts run from the DDR3 controller; a faster replay
# overruns the 16-byte UART RX FIFO while the CLI prints).
rx_pacing=(--uart0-rx-delay-us 60000 --uart0-rx-byte-us 87)
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../../../.." && pwd)"
version="VP_FX1_V3.0"
vp="${root}/vp/bin/${version}/fx1_soc"
build="${root}/sw/bootloader/build/${version}"
fw="${FX1_FW:-${build}/freertos_cli.elf}"
samples="${here}/isp"

usage() {
    cat <<'EOF'
Usage:
  ./run_vp.sh smoke                    # default: scripted CLI session, exit status = verdict
  ./run_vp.sh run [VP arguments]       # boot and print the console (default --timeout-ms 2000)
  ./run_vp.sh interactive [port]       # CLI over TCP; connect with ./fx1_console.sh [port]
  ./run_vp.sh isp [SAMPLE] auto        # FW-selected WB/tone; one probe + >=2 render frames
  ./run_vp.sh isp [SAMPLE]             # RAW frame through `isp run`, NV12 + PNG, golden check
  ./run_vp.sh isp [SAMPLE] interactive [port]   # same job, console in this terminal: isp run, poweroff
  ./run_vp.sh isp list                 # the samples (default d65)
  ./run_vp.sh version                  # VP build identity
  ./run_vp.sh ddr3                     # DDR3 firmware-init examples (BootROM/SRAM, --ddr-boot firmware)
  ./run_vp.sh camera [NAME...]         # camera examples (fx1_soc --camera; minutes each: detailed D-PHY)
  ./run_vp.sh camera-isp [NAME...]     # camera -> ISP examples (camera manager; NV12 = ISP model golden)
  ./run_vp.sh camera-cli               # FreeRTOS CLI with --camera, scripted camera session (cli_camera.txt)

FX1_FW=<image.elf> runs another firmware image instead of freertos_cli.elf.
Build the firmware first: make -C sw/bootloader/sources/VP_FX1_V3.0
DDR3 examples: make -C sw/bootloader/sources/VP_FX1_V3.0 ddr3 (see ddr3/README.md there)
Camera examples: make -C sw/bootloader/sources/VP_FX1_V3.0 camera (see vp/doc/VP_FX1_V3.0/camera/README.md)
EOF
}

die() { echo "ERROR: $*" >&2; exit 2; }
[[ -x "${vp}" ]] || die "VP is missing or not executable: ${vp}"
need_fw() {
    [[ -f "${fw}" ]] || die "missing ${fw}; build it with: make -C sw/bootloader/sources/${version}"
}
# A socket in LISTEN state on the port (no connect: --uart0-wait would take a
# probe as its client). /proc/net/tcp, so neither ss nor netstat is needed.
listening() {
    local hex
    hex="$(printf '%04X' "$1")"
    awk -v p=":${hex}" 'NR > 1 && $4 == "0A" && substr($2, length($2) - 4) == p { found = 1 } END { exit !found }' \
        /proc/net/tcp /proc/net/tcp6 2>/dev/null
}

mode="${1:-smoke}"
[[ $# -eq 0 ]] || shift
case "${mode}" in
    -h|--help|help)
        usage
        ;;
    version)
        exec "${vp}" --version
        ;;
    ddr3)
        # The examples of sw/bootloader/sources/VP_FX1_V3.0/ddr3: they start from
        # BootROM and initialize the DDR3 themselves (--ddr-boot firmware). The
        # fault schedule must equal the FAULT_* defines of the Makefile.
        d3="${build}/ddr3"
        [[ -f "${d3}/ddr3_fw_init.elf" ]] \
            || die "missing ${d3}/ddr3_fw_init.elf; build with: make -C sw/bootloader/sources/${version} ddr3"
        overall=0
        example() {
            local name="$1"
            shift
            set +e
            "${vp}" --fw "${d3}/${name}.elf" --ddr-boot firmware "$@" > "${d3}/${name}.log" 2>&1
            local rc=$?
            set -e
            if [[ ${rc} -eq 0 ]]; then
                echo "  PASS ${name}"
            else
                echo "  FAIL ${name}, exit ${rc} (log ${d3}/${name}.log)"
                overall=1
            fi
        }
        echo "VP_FX1_V3.0 DDR3 firmware-init examples"
        example ddr3_fw_init --timeout-ms 3000
        example ddr3_fault_recovery --timeout-ms 4000 --ddr3-fault 1000000:ce:0x80100000 \
            --ddr3-fault 1010000:ue:0x80100040 --ddr3-fault 1020000:read-timeout --ddr3-fault 2000000:fatal:dfi
        example ddr3_power_recovery --timeout-ms 3000
        example boot_freertos --timeout-ms 5000 --uart0-rx-file "${here}/cli_smoke.txt" \
            --uart0-rx-delay-us 1000000 --uart0-rx-byte-us 87
        [[ ${overall} -eq 0 ]] && echo "DDR3 EXAMPLES PASS" || echo "DDR3 EXAMPLES FAIL"
        exit ${overall}
        ;;
    camera)
        # The examples of sw/bootloader/sources/VP_FX1_V3.0/camera. camera_slverr's
        # fault time belongs to its image (comment at the top of camera/slverr.c).
        cam="${build}/camera"
        [[ -f "${cam}/camera_raw10.elf" ]] \
            || die "missing ${cam}/camera_raw10.elf; build with: make -C sw/bootloader/sources/${version} camera"
        names=("$@")
        [[ ${#names[@]} -gt 0 ]] || names=(camera_raw10 camera_raw12 camera_recover camera_decerr camera_slverr camera_i2c_irq camera_i2c_timeout)
        overall=0
        echo "VP_FX1_V3.0 camera examples (fx1_soc --camera)"
        for name in "${names[@]}"; do
            extra=(--timeout-ms 40)
            case "${name}" in
                camera_recover) extra=(--timeout-ms 80) ;;
                camera_i2c_timeout) extra=(--timeout-ms 60) ;;
                camera_slverr) extra=(--timeout-ms 40 --ddr3-ecc off --ddr3-fault 6200:write-result:1) ;;
            esac
            [[ -f "${cam}/${name}.elf" ]] || die "no camera example ${name}"
            set +e
            "${vp}" --fw "${cam}/${name}.elf" --camera "${extra[@]}" > "${cam}/${name}.log" 2>&1
            rc=$?
            set -e
            if [[ ${rc} -eq 0 ]]; then
                echo "  PASS ${name}"
            else
                echo "  FAIL ${name}, exit ${rc} (log ${cam}/${name}.log)"
                overall=1
            fi
        done
        [[ ${overall} -eq 0 ]] && echo "CAMERA EXAMPLES PASS" || echo "CAMERA EXAMPLES FAIL"
        exit ${overall}
        ;;
    camera-isp)
        # sw/bootloader/sources/VP_FX1_V3.0/camera_isp: sensor -> CSI-2 RX ->
        # camera manager -> ISP -> NV12, eight frames, each NV12 checked by the
        # firmware against the stand-alone ISP model's CRC.
        cam="${build}/camera"
        names=("$@")
        [[ ${#names[@]} -gt 0 ]] || names=(camera_isp_raw12 camera_isp_raw10)
        overall=0
        echo "VP_FX1_V3.0 camera -> ISP examples (fx1_soc --camera)"
        for name in "${names[@]}"; do
            [[ -f "${cam}/${name}.elf" ]] \
                || die "missing ${cam}/${name}.elf; build with: make -C sw/bootloader/sources/${version} camera"
            set +e
            "${vp}" --fw "${cam}/${name}.elf" --camera --timeout-ms 80 > "${cam}/${name}.log" 2>&1
            rc=$?
            set -e
            if [[ ${rc} -eq 0 ]]; then
                echo "  PASS ${name}"
            else
                echo "  FAIL ${name}, exit ${rc} (log ${cam}/${name}.log)"
                overall=1
            fi
        done
        [[ ${overall} -eq 0 ]] && echo "CAMERA ISP EXAMPLES PASS" || echo "CAMERA ISP EXAMPLES FAIL"
        exit ${overall}
        ;;
    camera-cli)
        # The FreeRTOS CLI with the camera: probe, 64x16 RAW12 and RAW10 captures
        # checked against the golden, status, stop (cli_camera.txt).
        "${vp}" --fw "${build}/freertos_cli.elf" --camera --timeout-ms 3000 --uart0-rx-file "${here}/cli_camera.txt" \
            --uart0-rx-delay-us 60000 --uart0-rx-byte-us 87
        ;;
    smoke)
        need_fw
        mkdir -p "${build}"
        echo "VP_FX1_V3.0 smoke: scripted CLI session (${here}/cli_smoke.txt)"
        set +e
        "${vp}" --fw "${fw}" --uart0-rx-file "${here}/cli_smoke.txt" "${rx_pacing[@]}" \
            --timeout-ms 3000 --uart-log "${build}/smoke_uart.log" > "${build}/last_run.log" 2>&1
        rc=$?
        set -e
        grep -a -E "hwcheck|total:|: PASS|: FAIL|poweroff:|RESULT" "${build}/last_run.log" | tr -d '\r' || true
        if [[ ${rc} -eq 0 ]]; then
            echo "SMOKE PASS (log ${build}/last_run.log)"
        else
            echo "SMOKE FAIL, exit ${rc} (log ${build}/last_run.log)"
        fi
        exit ${rc}
        ;;
    run)
        need_fw
        [[ $# -gt 0 ]] || set -- --timeout-ms 2000
        exec "${vp}" --fw "${fw}" "$@"
        ;;
    interactive)
        need_fw
        port="${1:-5000}"
        listening "${port}" && die "TCP port ${port} is already in use (another fx1_soc running?)"
        echo "VP waits for the UART console on 127.0.0.1:${port}"
        echo "Open another terminal: ${here}/fx1_console.sh ${port}   (end with 'poweroff')"
        exec "${vp}" --fw "${fw}" --uart0-socket "${port}" --uart0-wait --timeout-ms 0 --realtime
        ;;
    isp)
        need_fw
        command -v python3 >/dev/null 2>&1 || die "python3 is needed to pack the NV12 (dnf install -y python3)"
        sample="d65"
        if [[ $# -gt 0 && "$1" == "list" ]]; then
            for d in "${samples}"/*/; do
                echo "$(basename "${d}"): $(head -n 1 "${d}SAMPLE.txt")"
            done
            exit 0
        fi
        if [[ $# -gt 0 && -d "${samples}/$1" ]]; then
            sample="$1"
            shift
        fi
        adaptive=0
        if [[ "${1:-}" == "auto" ]]; then adaptive=1; shift; fi
        sub="${1:-smoke}"
        isp_in="${samples}/${sample}"
        isp_out="${build}/isp/${sample}"
        [[ -f "${isp_in}/job.env" ]] || die "no ISP sample '${sample}' (./run_vp.sh isp list)"
        # shellcheck source=/dev/null
        unset PROFILE_ID
        . "${isp_in}/job.env"
        [[ "${PROFILE_ID:-}" =~ ^[a-z0-9_]+$ ]] || die "sample '${sample}': invalid or missing PROFILE_ID"
        profile_bin="${here}/profiles/${PROFILE_ID}/profile.bin"
        [[ -s "${profile_bin}" ]] || die "missing shared profile: ${profile_bin}"
        rm -rf "${isp_out}"
        mkdir -p "${isp_out}"
        cp "${isp_in}/job.bin" "${isp_out}/job.bin"
        replay="${here}/isp_run.txt"
        command="isp run"
        if [[ ${adaptive} -eq 1 ]]; then
            python3 "${here}/isp_auto.py" prepare "${isp_out}/job.bin" "${isp_out}/job.bin"
            command="isp run auto"
            replay="${isp_out}/auto_run.txt"
            printf 'isp run auto\risp stats\rpoweroff\r' > "${replay}"
        fi
        io=(--load "${RAW_ADDR}:${isp_in}/input.isp16" --load "${JOB_ADDR}:${isp_out}/job.bin"
            --load "${PROFILE_ADDR}:${profile_bin}"
            --dump "${RESULT_ADDR}:${RESULT_LEN}:${isp_out}/result.bin"
            --dump "${Y_ADDR}:${Y_LEN}:${isp_out}/y.bin" --dump "${UV_ADDR}:${UV_LEN}:${isp_out}/uv.bin")
        case "${sub}" in
            smoke)
                echo "VP_FX1_V3.0 isp ${sample}: $(head -n 1 "${isp_in}/SAMPLE.txt")"
                set +e
                "${vp}" --fw "${fw}" "${io[@]}" --uart0-rx-file "${replay}" "${rx_pacing[@]}" \
                    --timeout-ms 5000 > "${isp_out}/platform.log" 2>&1
                rc=$?
                set -e
                grep -a -E "isp run|ISP_AUTO|AEC|AWB|AF |poweroff:|RESULT" "${isp_out}/platform.log" | tr -d '\r' || true
                [[ ${rc} -eq 0 ]] || { echo "ISP FAIL: the VP run failed, exit ${rc} (${isp_out}/platform.log)"; exit 1; }
                ;;
            interactive)
                port="${2:-5000}"
                listening "${port}" && die "TCP port ${port} is already in use (another fx1_soc running?)"
                "${vp}" --fw "${fw}" "${io[@]}" --uart0-socket "${port}" --uart0-wait --timeout-ms 0 --realtime \
                    > "${isp_out}/platform.log" 2>&1 &
                vp_pid=$!
                trap 'kill ${vp_pid} 2>/dev/null || true' EXIT
                for _ in $(seq 300); do
                    listening "${port}" && break
                    kill -0 ${vp_pid} 2>/dev/null || die "the VP exited before listening, see ${isp_out}/platform.log"
                    sleep 0.1
                done
                echo
                echo "Type '${command}', 'isp stats', then 'poweroff' (the dumps are written at poweroff)."
                echo
                "${here}/fx1_console.sh" "${port}" || true
                for _ in $(seq 50); do
                    kill -0 ${vp_pid} 2>/dev/null || break
                    sleep 0.1
                done
                if kill -0 ${vp_pid} 2>/dev/null; then
                    echo
                    die "console closed before 'poweroff': VP stopped, nothing to pack"
                fi
                set +e
                wait ${vp_pid}
                rc=$?
                set -e
                trap - EXIT
                echo
                [[ ${rc} -eq 0 ]] || { echo "ISP FAIL: the VP run failed, exit ${rc} (${isp_out}/platform.log)"; exit 1; }
                ;;
            *)
                usage
                exit 2
                ;;
        esac
        expected="${isp_in}/expected_nv12.sha256"
        if [[ ${adaptive} -eq 1 ]]; then
            python3 "${here}/isp_auto.py" capture "${isp_out}" "${here}/profiles/${PROFILE_ID}/profile.csrw"
            expected="${isp_in}/expected_auto_nv12.sha256"
        fi
        expect_args=()
        if [[ -f "${expected}" ]]; then expect_args=(--expect-sha256 "$(cat "${expected}")"); fi
        [[ ${adaptive} -eq 1 || -f "${expected}" ]] || die "fixed sample golden missing: ${expected}"
        set +e
        python3 "${here}/isp_pack.py" "${isp_out}" "${expect_args[@]}"
        rc=$?
        set -e
        case ${rc} in
            0) if [[ -f "${expected}" ]]; then
                   echo "ISP PASS: NV12 equals the stand-alone ISP model's output; preview ${isp_out}/platform.png"
               else
                   echo "ISP AUTO COMPLETE: output valid, no adaptive golden; preview ${isp_out}/platform.png"
               fi ;;
            1) echo "ISP FAIL: NV12 differs from the expected output" ;;
            *) echo "ISP FAIL: the dumps were rejected" ;;
        esac
        exit ${rc}
        ;;
    *)
        usage
        exit 2
        ;;
esac
