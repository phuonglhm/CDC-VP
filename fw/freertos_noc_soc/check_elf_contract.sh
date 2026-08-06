#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

elf="${1:?usage: check_elf_contract.sh ELF [READELF]}"
readelf_tool="${2:-riscv-none-elf-readelf}"

ram_first=$((0x80000000))
ram_last=$((0x80fff000))

[[ -f "${elf}" ]] || {
    echo "ELF contract FAIL: file not found: ${elf}" >&2
    exit 1
}
command -v "${readelf_tool}" >/dev/null || {
    echo "ELF contract FAIL: readelf not found: ${readelf_tool}" >&2
    exit 1
}

header="$("${readelf_tool}" -h "${elf}")"
grep -Eq 'Class:[[:space:]]+ELF32' <<<"${header}" || {
    echo "ELF contract FAIL: image is not ELF32" >&2
    exit 1
}
grep -Eq 'Data:[[:space:]]+2.s complement, little endian' <<<"${header}" || {
    echo "ELF contract FAIL: image is not little-endian" >&2
    exit 1
}
grep -Eq 'Machine:[[:space:]]+RISC-V' <<<"${header}" || {
    echo "ELF contract FAIL: image is not RISC-V" >&2
    exit 1
}

entry_text="$(awk '/Entry point address:/ { print $4 }' <<<"${header}")"
[[ -n "${entry_text}" ]] || {
    echo "ELF contract FAIL: entry point is missing" >&2
    exit 1
}
entry=$((entry_text))
if (( entry != ram_first )); then
    printf 'ELF contract FAIL: entry is 0x%08x, expected 0x80000000\n' \
        "${entry}" >&2
    exit 1
fi

loads=0
entry_covered=0
while read -r address_text size_text; do
    [[ -n "${address_text}" && -n "${size_text}" ]] || continue
    address=$((address_text))
    size=$((size_text))
    end=$((address + size))
    if (( end < address || address < ram_first || end > ram_last )); then
        printf 'ELF contract FAIL: PT_LOAD [0x%08x, 0x%08x) is outside '\
'[0x80000000, 0x80fff000)\n' "${address}" "${end}" >&2
        exit 1
    fi
    if (( entry >= address && entry < end )); then
        entry_covered=1
    fi
    printf 'ELF PT_LOAD [0x%08x, 0x%08x) OK\n' "${address}" "${end}"
    loads=$((loads + 1))
done < <(
    "${readelf_tool}" -lW "${elf}" |
        awk '$1 == "LOAD" { print $3, $6 }'
)

if (( loads == 0 )); then
    echo "ELF contract FAIL: image has no PT_LOAD segment" >&2
    exit 1
fi
if (( entry_covered == 0 )); then
    echo "ELF contract FAIL: entry is not covered by a PT_LOAD segment" >&2
    exit 1
fi

symbols="$("${readelf_tool}" -sW "${elf}")"
stack_text="$(
    awk '$8 == "_stack_top" { print "0x" $2; exit }' <<<"${symbols}"
)"
ram_end_text="$(
    awk '$8 == "__firmware_ram_end" { print "0x" $2; exit }' <<<"${symbols}"
)"
[[ -n "${stack_text}" && -n "${ram_end_text}" ]] || {
    echo "ELF contract FAIL: linker boundary symbols are missing" >&2
    exit 1
}
stack=$((stack_text))
linked_ram_end=$((ram_end_text))
if (( stack != ram_last || linked_ram_end != ram_last )); then
    printf 'ELF contract FAIL: stack/end 0x%08x/0x%08x, expected 0x80fff000\n' \
        "${stack}" "${linked_ram_end}" >&2
    exit 1
fi

printf 'ELF contract PASS: entry 0x%08x, %d PT_LOAD segment(s), '\
'stack 0x%08x, reserved page untouched\n' "${entry}" "${loads}" "${stack}"
