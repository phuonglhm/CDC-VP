#!/bin/bash
# Native v4.5 regression with a FRESH build.
# The Makefile only depends on the .cpp files, so `make <target>` reports "up to date" after header edits and re-runs an old
# binary. This script compiles the 4 regression targets with the exact Makefile flags into tools/has/regress/ (the binaries in
# the repo root are NOT touched), runs the fresh AND the old binary, and prints the verdict lines of both.
# Run from the repository root:  bash tools/has/regress_native.sh > tools/has/regress/regress.log 2>&1
# Optional SINCE="<date time>": also list native sources modified after that time (outside has/ and tools/).
set -u
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}
OUT=tools/has/regress
mkdir -p "$OUT"
FLAGS="-std=c++17 -O3 -Wall -I. -I/usr/include -DEVAL_X=32 -DEVAL_Y=32 -DA_REGION_BYTES=16384 -DB_REGION_BYTES=16384 -DC_REGION_BYTES=16384"
LIBS="-L/usr/lib -L/usr/lib/x86_64-linux-gnu -Wl,-rpath,/usr/lib -lsystemc -lm -pthread"
declare -A SRC=([tb_obp]=tb_obp.cpp [test_vit]=tools/test_vit.cpp [test_yolo]=tools/test_yolo.cpp [test_onnx_model]=tools/test_onnx_model.cpp)
if [ -n "${SINCE:-}" ]; then
  echo "native sources newer than $SINCE (outside has/, fe_work/, tools/):"
  find . \( -path ./fe_work -o -path ./has -o -path ./tools -o -path ./.backups \) -prune \
       -o -type f \( -name "*.h" -o -name "*.cpp" -o -name Makefile \) -newermt "$SINCE" -print
fi
for t in tb_obp test_vit test_yolo test_onnx_model; do
  echo "=================== $t"
  EXTRA=""; [ "$t" != tb_obp ] && EXTRA="-DEVAL_X=32 -DEVAL_Y=32"
  if nice -n 19 g++ $FLAGS $EXTRA "${SRC[$t]}" $LIBS -o "$OUT/$t" 2> "$OUT/$t.build.log"; then
    echo "build: OK ($(sha256sum "$OUT/$t" | cut -c1-12))"
  else
    echo "build: FAILED"; tail -5 "$OUT/$t.build.log"; continue
  fi
  for b in "$OUT/$t" "./$t"; do
    s=$(date +%s)
    nice -n 19 "$b" > "$OUT/$t.$(basename $(dirname $b)).run.log" 2>&1; rc=$?
    echo "run $b: rc=$rc, $(( $(date +%s) - s )) s"
    grep -iE "pass|fail|result|mismatch" "$OUT/$t.$(basename $(dirname $b)).run.log" | tail -3
  done
done
echo "done"
