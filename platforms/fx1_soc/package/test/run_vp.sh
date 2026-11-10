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
  ./run_vp.sh isp ... --dump-demosaic  # also the ISP image after Demosaic, before CCM (VP debug tap,
                                       # not an ISP output): demosaic.rgb16 (RGB12 in LE16), .png, .json
                                       # of the job's final frame, checked against its golden
  ./run_vp.sh isp FILE.raw [auto] [interactive [port]]
                                       # your own frame, same format as the samples (2688x1520 BGGR,
                                       # RAW10 in bits [15:6] of LE uint16, 8171520 bytes); d65's job
                                       # and profile; NV12 + PNG, no golden (nothing to compare with)
  ./run_vp.sh version                  # VP build identity
  ./run_vp.sh ddr3                     # DDR3 firmware-init examples (BootROM/SRAM, --ddr-boot firmware)
  ./run_vp.sh camera [NAME...]         # camera examples (fx1_soc --camera; minutes each: detailed D-PHY)
  ./run_vp.sh camera-isp [NAME...]     # camera -> ISP examples (camera manager; NV12 = ISP model golden)
  ./run_vp.sh camera-cli               # FreeRTOS CLI with --camera, scripted camera session (cli_camera.txt)
  ./run_vp.sh eth [PEER...]            # bare-metal Ethernet example on each line peer (default: loopback partner none)
  ./run_vp.sh eth-cli [PEER]           # FreeRTOS CLI, scripted eth session (cli_eth.txt), PEER default loopback
  ./run_vp.sh eth-load                 # FreeRTOS CLI, Ethernet traffic with SYS_DMA and the ISP (cli_eth_load.txt)

FX1_FW=<image.elf> runs another firmware image instead of freertos_cli.elf.
Build the firmware first: make -C sw/bootloader/sources/VP_FX1_V3.0
DDR3 examples: make -C sw/bootloader/sources/VP_FX1_V3.0 ddr3 (see ddr3/README.md there)
Camera examples: make -C sw/bootloader/sources/VP_FX1_V3.0 camera (see vp/doc/VP_FX1_V3.0/camera/README.md)
Ethernet example: make -C sw/bootloader/sources/VP_FX1_V3.0 eth (see vp/doc/VP_FX1_V3.0/eth/README.md)
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
    eth)
        # sw/bootloader/sources/VP_FX1_V3.0/eth/eth_wiring.c on each line peer:
        # CSR and MDIO, DMA through DDR3, interrupts, a DMA error and its
        # recovery, LR/SC against the DMA, stop/restart.
        ethout="${build}/eth"
        [[ -f "${ethout}/eth_wiring.elf" ]] \
            || die "missing ${ethout}/eth_wiring.elf; build with: make -C sw/bootloader/sources/${version} eth"
        peers=("$@")
        [[ ${#peers[@]} -gt 0 ]] || peers=(loopback partner none)
        overall=0
        echo "VP_FX1_V3.0 Ethernet example (eth_wiring)"
        for peer in "${peers[@]}"; do
            case "${peer}" in loopback|partner|none) ;; *) die "unknown line peer ${peer} (loopback, partner, none)" ;; esac
            set +e
            "${vp}" --fw "${ethout}/eth_wiring.elf" --eth-peer "${peer}" --timeout-ms 200 \
                > "${ethout}/eth_wiring_${peer}.log" 2>&1
            rc=$?
            set -e
            if [[ ${rc} -eq 0 ]]; then
                echo "  PASS eth_wiring --eth-peer ${peer}"
            else
                echo "  FAIL eth_wiring --eth-peer ${peer}, exit ${rc} (log ${ethout}/eth_wiring_${peer}.log)"
                overall=1
            fi
        done
        [[ ${overall} -eq 0 ]] && echo "ETH EXAMPLES PASS" || echo "ETH EXAMPLES FAIL"
        exit ${overall}
        ;;
    eth-cli|eth-load)
        need_fw
        mkdir -p "${build}"
        script="${here}/cli_eth.txt" peer="${1:-loopback}" log="${build}/${mode}.log"
        [[ "${mode}" == eth-load ]] && script="${here}/cli_eth_load.txt"
        case "${peer}" in loopback|partner|none) ;; *) die "unknown line peer ${peer} (loopback, partner, none)" ;; esac
        echo "VP_FX1_V3.0 ${mode}: scripted CLI session (${script}), --eth-peer ${peer}"
        set +e
        "${vp}" --fw "${fw}" --eth-peer "${peer}" --uart0-rx-file "${script}" "${rx_pacing[@]}" \
            --timeout-ms 3000 > "${log}" 2>&1
        rc=$?
        set -e
        grep -a -E "^eth (probe|start|send|stop|recover|selftest|cycle|load)|  eth .*: (PASS|FAIL)|poweroff:|RESULT" "${log}" \
            | tr -d '\r' || true
        if [[ ${rc} -eq 0 ]]; then
            echo "ETH CLI PASS (log ${log})"
        else
            echo "ETH CLI FAIL, exit ${rc} (log ${log})"
        fi
        exit ${rc}
        ;;
    camera-cli)
        # The FreeRTOS CLI with the camera: probe, 64x32 RAW12 and RAW10 captures
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
        # --dump-demosaic may come anywhere after "isp": taken out before the
        # positional words (sample, RAW path with spaces, auto, interactive [port]).
        dump_demosaic=0
        rest=()
        for word in "$@"; do
            if [[ "${word}" == "--dump-demosaic" ]]; then dump_demosaic=1; else rest+=("${word}"); fi
        done
        set -- "${rest[@]+"${rest[@]}"}"
        sample="d65"
        if [[ $# -gt 0 && "$1" == "list" ]]; then
            for d in "${samples}"/*/; do
                echo "$(basename "${d}"): $(head -n 1 "${d}SAMPLE.txt")"
            done
            exit 0
        fi
        raw_file=""
        if [[ $# -gt 0 && -d "${samples}/$1" ]]; then
            sample="$1"
            shift
        elif [[ $# -gt 0 && ( "$1" == */* || "$1" == *.raw ) ]]; then
            # A RAW file of the user's. An unquoted path with spaces arrives
            # as several words: join them until they name a file.
            raw_file="$1"
            shift
            while [[ ! -f "${raw_file}" && $# -gt 0 ]]; do
                raw_file="${raw_file} $1"
                shift
            done
            [[ -f "${raw_file}" ]] || die "RAW file not found: ${raw_file}"
        fi
        adaptive=0
        if [[ "${1:-}" == "auto" ]]; then adaptive=1; shift; fi
        sub="${1:-smoke}"
        isp_in="${samples}/${sample}"
        isp_out="${build}/isp/${sample}"
        input="${isp_in}/input.isp16"
        if [[ -n "${raw_file}" ]]; then
            # d65's job (geometry, buffers, frames) and profile, another frame.
            name="$(basename "${raw_file}")"
            name="${name%.*}"
            name="$(printf '%s' "${name}" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9_' '_')"
            isp_out="${build}/isp/raw_${name}"
            input="${isp_out}/input.isp16"
        fi
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
        if [[ -n "${raw_file}" ]]; then
            # The samples' conversion (le16_raw10_msb): RAW10 in bits [15:6] to
            # the ISP's 12-bit scale, i.e. >> 4; the 6 low bits must be zero.
            python3 - "${raw_file}" "${input}" <<'PY' || die "cannot use ${raw_file} (see above)"
import array, sys
w, h = 2688, 1520
src, out = sys.argv[1], sys.argv[2]
a = array.array("H")
with open(src, "rb") as f:
    data = f.read()
if len(data) != 2 * w * h:
    sys.exit("%s: %d bytes; expected %d (%dx%d, 16-bit samples, stride %d)" % (src, len(data), 2 * w * h, w, h, 2 * w))
a.frombytes(data)
if sys.byteorder != "little":
    a.byteswap()
bad = sum(1 for v in a if v & 0x3F)
if bad:
    sys.exit("%s: %d samples have bits [5:0] set: not RAW10 in bits [15:6] (le16_raw10_msb)" % (src, bad))
o = array.array("H", (v >> 4 for v in a))
if sys.byteorder != "little":
    o.byteswap()
with open(out, "wb") as f:
    f.write(o.tobytes())
PY
        fi
        replay="${here}/isp_run.txt"
        command="isp run"
        if [[ ${adaptive} -eq 1 ]]; then
            python3 "${here}/isp_auto.py" prepare "${isp_out}/job.bin" "${isp_out}/job.bin"
            command="isp run auto"
            replay="${isp_out}/auto_run.txt"
            printf 'isp run auto\risp stats\rpoweroff\r' > "${replay}"
        fi
        io=(--load "${RAW_ADDR}:${input}" --load "${JOB_ADDR}:${isp_out}/job.bin"
            --load "${PROFILE_ADDR}:${profile_bin}"
            --dump "${RESULT_ADDR}:${RESULT_LEN}:${isp_out}/result.bin"
            --dump "${Y_ADDR}:${Y_LEN}:${isp_out}/y.bin" --dump "${UV_ADDR}:${UV_LEN}:${isp_out}/uv.bin")
        demosaic_dir="${isp_out}/demosaic.d"
        [[ ${dump_demosaic} -eq 0 ]] || io+=(--dump-isp-demosaic "${demosaic_dir}")
        case "${sub}" in
            smoke)
                if [[ -n "${raw_file}" ]]; then
                    echo "VP_FX1_V3.0 isp ${raw_file}: ${sample}'s job and profile (${PROFILE_ID})"
                else
                    echo "VP_FX1_V3.0 isp ${sample}: $(head -n 1 "${isp_in}/SAMPLE.txt")"
                fi
                set +e
                "${vp}" --fw "${fw}" "${io[@]}" --uart0-rx-file "${replay}" "${rx_pacing[@]}" \
                    --timeout-ms 5000 > "${isp_out}/platform.log" 2>&1
                rc=$?
                set -e
                grep -a -E "isp run|ISP_AUTO|AEC|AWB|AF |poweroff:|RESULT" "${isp_out}/platform.log" | tr -d '\r' || true
                [[ ${rc} -ne 5 ]] || { echo "ISP FAIL: the run passed but the Demosaic capture failed (host I/O), see ${isp_out}/platform.log"; exit 1; }
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
                [[ ${rc} -ne 5 ]] || { echo "ISP FAIL: the run passed but the Demosaic capture failed (host I/O), see ${isp_out}/platform.log"; exit 1; }
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
        [[ -z "${raw_file}" ]] || expected=""  # a user frame has no golden
        expect_args=()
        if [[ -n "${expected}" && -f "${expected}" ]]; then
            golden="$(tr -d ' \r\n' < "${expected}")"
            [[ "${golden}" =~ ^[0-9a-fA-F]{64}$ ]] || die "golden ${expected} is not a SHA-256 (64 hex digits)"
            expect_args=(--expect-sha256 "${golden}")
        fi
        [[ ${adaptive} -eq 1 || -n "${raw_file}" || -f "${expected}" ]] || die "fixed sample golden missing: ${expected}"
        set +e
        python3 "${here}/isp_pack.py" "${isp_out}" "${expect_args[@]}"
        rc=$?
        set -e
        case ${rc} in
            0) if [[ ${#expect_args[@]} -gt 0 ]]; then
                   echo "ISP PASS: NV12 equals the stand-alone ISP model's output; preview ${isp_out}/platform.png"
               elif [[ -n "${raw_file}" ]]; then
                   echo "ISP DONE: ${raw_file} processed (no golden, not compared); NV12 ${isp_out}/platform.nv12, preview ${isp_out}/platform.png"
               else
                   echo "ISP AUTO COMPLETE: output valid, no adaptive golden; preview ${isp_out}/platform.png"
               fi ;;
            1) echo "ISP FAIL: NV12 differs from the expected output" ;;
            3) echo "ISP FAIL: no preview, the result block has no output colorimetry (give --std and --range to isp_pack.py); NV12 kept: ${isp_out}/platform.nv12" ;;
            *) echo "ISP FAIL: the dumps were rejected" ;;
        esac
        if [[ ${dump_demosaic} -eq 1 && "${sub}" == "interactive" ]]; then
            # No job boundary is known here: every frame of the session, keyed
            # by ordinal/epoch/sequence (the firmware gives no job ID).
            echo "DEMOSAIC: per-frame captures of the session in ${demosaic_dir}/frames, index ${demosaic_dir}/index.jsonl"
            echo "          (interactive: no frame is selected; post-Demosaic, pre-CCM VP debug tap, RGB12 in LE16)"
        elif [[ ${dump_demosaic} -eq 1 && ${rc} -eq 0 ]]; then
            # The job's final frame: the last of its frames, all completed; for
            # auto the last render frame, with the WB the policy applied.
            # -B: no __pycache__ in the shipped tree (SHA256SUMS lists every file)
            frames="$(python3 -B -c 'import sys; sys.path.insert(0, sys.argv[1]); import isp_pack; print(isp_pack.read_job(sys.argv[2])["frames"])' "${here}" "${isp_out}/job.bin")"
            select_args=(--frames "${frames}" --out "${isp_out}/demosaic" --summary "${isp_out}/summary.txt")
            dgolden_file="${isp_in}/expected_demosaic.sha256"
            if [[ ${adaptive} -eq 1 ]]; then
                dgolden_file="${isp_in}/expected_auto_demosaic.sha256"
                wb="$(python3 -B -c 'import json, sys; c = json.load(open(sys.argv[1]))["csr"]; print("%d,%d,%d" % (c["WB_GAIN_R"], c["WB_GAIN_G"], c["WB_GAIN_B"]))' "${isp_out}/auto.json")"
                select_args+=(--expect-wb "${wb}")
            fi
            [[ -z "${raw_file}" ]] || dgolden_file=""
            if [[ -n "${dgolden_file}" ]]; then
                [[ -f "${dgolden_file}" ]] || die "Demosaic golden missing: ${dgolden_file}"
                dgolden="$(tr -d ' \r\n' < "${dgolden_file}")"
                [[ "${dgolden}" =~ ^[0-9a-fA-F]{64}$ ]] || die "golden ${dgolden_file} is not a SHA-256 (64 hex digits)"
                select_args+=(--expect-sha256 "${dgolden}")
            fi
            set +e
            python3 -B "${here}/demosaic_select.py" "${demosaic_dir}" "${select_args[@]}"
            drc=$?
            set -e
            if [[ ${drc} -ne 0 ]]; then
                echo "DEMOSAIC FAIL: no valid capture of the job's final frame (see above, ${demosaic_dir}/index.jsonl)"
                rc=1
            elif [[ -n "${dgolden_file}" ]]; then
                echo "DEMOSAIC PASS: ${isp_out}/demosaic.rgb16 equals the stand-alone ISP model's capture; preview ${isp_out}/demosaic.png"
            else
                echo "DEMOSAIC DONE: ${isp_out}/demosaic.rgb16 (no golden, not compared); preview ${isp_out}/demosaic.png"
            fi
        fi
        exit ${rc}
        ;;
    *)
        usage
        exit 2
        ;;
esac
