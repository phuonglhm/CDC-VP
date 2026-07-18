#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Static public-release compliance gate. This checks repository evidence and
# known blockers; it is not a legal opinion.

set -uo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"

allow_dirty=0
if [[ "${1:-}" == "--allow-dirty" ]]; then
    allow_dirty=1
elif [[ $# -ne 0 ]]; then
    echo "usage: $0 [--allow-dirty]" >&2
    exit 2
fi

errors=0
pass() { echo "[PASS] $1"; }
fail() { echo "[FAIL] $1" >&2; errors=$((errors + 1)); }

required_files=(
    LICENSE
    NOTICE
    THIRD_PARTY.md
    CONTRIBUTING.md
    docs/PUBLIC_RELEASE_CHECKLIST.md
    licenses/SAURIA.SHL-2.1
    licenses/SAURIA.PROVENANCE.md
    licenses/CMAKE.BSD-3-Clause
    licenses/CMAKE.PROVENANCE.md
    components/isp_tlm/ASSET_PROVENANCE.md
    components/vpu_tlm3.0/LICENSE
    components/vpu_tlm3.0/PROVENANCE.md
)

missing=0
for file in "${required_files[@]}"; do
    if [[ ! -s "$file" ]]; then
        fail "missing required release file: $file"
        missing=1
    fi
done
[[ "$missing" -eq 0 ]] && pass "required license/provenance files exist"

sauria_hash="$(sha256sum licenses/SAURIA.SHL-2.1 2>/dev/null | awk '{print $1}')"
if [[ "$sauria_hash" == \
      "f725a7d2ff028400f9f43618232222b6fe9a68563eecbce2e9666a9898153a07" ]]; then
    pass "SAURIA upstream license hash"
else
    fail "SAURIA license hash mismatch: ${sauria_hash:-missing}"
fi

cmake_hash="$(sha256sum cmake-3.21.7-linux-x86_64.sh 2>/dev/null | awk '{print $1}')"
if [[ "$cmake_hash" == \
      "47bfc0d1c81051c83429240195d0b747107db99c4153ba89582ca370986cce9d" ]]; then
    pass "CMake installer provenance hash"
else
    fail "CMake installer hash mismatch: ${cmake_hash:-missing}"
fi

if rg -Uq \
    'option\(CDC_ENABLE_SAURIA_NPU_V4[[:space:]]+"[^"]+"[[:space:]]+OFF\)' \
    CMakeLists.txt; then
    pass "public NPU option defaults to OFF"
else
    fail "CDC_ENABLE_SAURIA_NPU_V4 is not demonstrably default-OFF"
fi

if git ls-files | rg -q \
    '(^|/)(npu_top\.h|sauria_systolic\.h|sauria_sram\.h)$'; then
    fail "private SAURIA implementation header is tracked"
else
    pass "no known private SAURIA implementation headers tracked"
fi

private_hits="$(rg -n \
    '/home/[^/]+/(Documents|Desktop)/|MP1_V1\.1|test002/sauria' \
    . \
    --glob '!build*/**' --glob '!out/**' --glob '!third_party/**' \
    --glob '!.git/**' --glob '!tools/check_public_release.sh' \
    2>/dev/null || true)"
if [[ -n "$private_hits" ]]; then
    fail "private/local path marker found"
    printf '%s\n' "$private_hits" >&2
else
    pass "no known private/local path markers"
fi

if rg -q '\*\*BLOCKED\*\*' components/isp_tlm/ASSET_PROVENANCE.md; then
    fail "ISP media assets still have unresolved redistribution rights"
else
    pass "ISP media asset provenance resolved"
fi

if rg -qi 'pending|must confirm' components/vpu_tlm3.0/PROVENANCE.md; then
    fail "VPU TLM 3.0 rights holder is not confirmed"
else
    pass "VPU TLM 3.0 rights holder confirmed"
fi

if [[ "$allow_dirty" -eq 0 ]] &&
   [[ -n "$(git status --porcelain --untracked-files=normal)" ]]; then
    fail "working tree is dirty; commit intended release content first"
elif [[ "$allow_dirty" -eq 1 ]]; then
    echo "[INFO] dirty-tree gate skipped by --allow-dirty"
else
    pass "working tree is clean"
fi

if [[ "$errors" -ne 0 ]]; then
    echo "public release gate: $errors blocker(s)" >&2
    exit 1
fi

echo "public release gate: PASS"
