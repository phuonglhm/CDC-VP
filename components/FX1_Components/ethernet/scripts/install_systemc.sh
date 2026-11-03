#!/usr/bin/env bash
# =============================================================================
# Integrated Circuits and Systems laboratory
# Author: Hung Nguyen <ngthung@hcmut.edu.vn>
# Module: virtual_platform.scripts.install_systemc
# Description: Build and install Accellera SystemC 2.3.4 (with TLM-2.0).
# Changelog:
#   2026-09-29: Initial version.
# =============================================================================
#
#   virtual_platform/scripts/install_systemc.sh [PREFIX]        default PREFIX: $HOME/opt/systemc-2.3.4
#
# The version CDC-VP pins.  Works on Linux and on Windows under Git Bash with
# MinGW-w64 g++ (static library there, so executables need no DLL on PATH).
# Needs git, cmake >= 3.21 and ninja or make.  CMake 4 dropped compatibility
# with the cmake_minimum_required of SystemC 2.3.4's build, hence
# CMAKE_POLICY_VERSION_MINIMUM.
set -euo pipefail

PREFIX="${1:-$HOME/opt/systemc-2.3.4}"
TAG=2.3.4
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

git clone --quiet --depth 1 --branch "$TAG" \
    https://github.com/accellera-official/systemc.git "$WORK/src"

GEN=()
if command -v ninja >/dev/null 2>&1; then GEN=(-G Ninja); fi
SHARED=ON
THREADS=(-DENABLE_PTHREADS=ON)
# Windows: a static library, and SystemC's own WinFiber coroutines (its build
# refuses pthreads there)
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) SHARED=OFF; THREADS=() ;; esac
# SystemC's C++ sources already select WinFiber coroutines under MinGW, but its
# CMake still adds the QuickThreads x86_64 assembly, which is ELF-only and does
# not assemble for Windows.  Treat MinGW like MSVC there.
sed -i -e 's/^elseif (MSVC)$/elseif (MSVC OR MINGW)/' \
       -e 's/^  if (NOT MSVC)$/  if (NOT (MSVC OR MINGW))/' "$WORK/src/CMakeLists.txt"

cmake -S "$WORK/src" -B "$WORK/build" "${GEN[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_SHARED_LIBS="$SHARED" \
    "${THREADS[@]}" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$WORK/build" -j"$(nproc 2>/dev/null || echo 4)"
cmake --install "$WORK/build"
echo "SystemC $TAG installed in $PREFIX -- export SYSTEMC_HOME=$PREFIX"
