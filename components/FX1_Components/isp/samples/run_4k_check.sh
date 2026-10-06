#!/bin/sh
# Bit-exact check of the installed model with no dataset and no Python:
# runs the 4K cases (tools/gen_4k_vectors.py, DEC-37) through fx1_isp_run_raw
# and compares the NV12 SHA-256 and every statistics read with the values the
# Python reference produced. Usage: run_4k_check.sh [install prefix] [out dir]
here=$(cd "$(dirname "$0")" && pwd)
prefix=${1:-$(cd "$here/../../.." && pwd)}
out=${2:-${TMPDIR:-/tmp}/fx1_isp_4k_check}
mkdir -p "$out"
status=0
for case in "$here"/4k/*/; do
    name=$(basename "$case")
    read -r w h frames < "$case/meta.txt"
    read -r sha ow oh < "$case/expected_nv12.sha256"
    if "$prefix/bin/fx1_isp_run_raw" --synthetic --width "$w" --height "$h" --frames "$frames" \
           --profile "$case/profile.csrw" --stats-script "$case/expected_stats.txt" --check-stats \
           --expect-sha256 "$sha" --out "$out/$name" > "$out/$name.log" 2>&1; then
        echo "PASS $name (${ow}x${oh})"
    else
        echo "FAIL $name, see $out/$name.log"
        status=1
    fi
done
exit $status
