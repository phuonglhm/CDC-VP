#!/bin/bash
# Rebuild and extract the data package carried in data/ of this repository.
#
# The data package (programs, golden images, quantization parameters, unit-test vectors, reference run logs) is stored
# as data/sauria_npu_data.tar.gz.partNN, each part below 100 MB so that the repository can be pushed to any git host
# without large-file support. This script joins the parts, checks the sha256 of the archive against data/SHA256SUMS,
# extracts it and checks every extracted file against its package_manifest.txt.
#
# usage: bash tools/unpack_data.sh [destination directory]     (default: the repository root)
# result: <destination>/sauria_npu_data/ ; then: export FE_WORK=<destination>/sauria_npu_data
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=${1:-$ROOT}
PARTS=$(ls "$ROOT"/data/sauria_npu_data.tar.gz.part* 2>/dev/null | sort)
[ -n "$PARTS" ] || { echo "error: no data/sauria_npu_data.tar.gz.part* in $ROOT"; exit 1; }
WANT=$(awk '$2 ~ /sauria_npu_data.tar.gz$/ {print $1}' "$ROOT/data/SHA256SUMS")
[ -n "$WANT" ] || { echo "error: data/SHA256SUMS has no line for sauria_npu_data.tar.gz"; exit 1; }
mkdir -p "$DEST"
if [ -e "$DEST/sauria_npu_data" ]; then
  echo "error: $DEST/sauria_npu_data already exists; remove it or give another destination"; exit 1
fi
echo "[unpack_data] checking the archive ($(echo "$PARTS" | wc -l) parts)"
GOT=$(cat $PARTS | sha256sum | awk '{print $1}')
if [ "$GOT" != "$WANT" ]; then
  echo "error: sha256 of the joined parts is $GOT, expected $WANT"
  echo "       (a text conversion of the parts by git would cause this: keep .gitattributes in place)"; exit 1
fi
echo "[unpack_data] extracting to $DEST/sauria_npu_data"
cat $PARTS | tar -xzf - -C "$DEST"
( cd "$DEST/sauria_npu_data" && sha256sum --quiet -c package_manifest.txt )
echo "[unpack_data] OK: archive and every extracted file match their checksums"
echo "export FE_WORK=$DEST/sauria_npu_data"
