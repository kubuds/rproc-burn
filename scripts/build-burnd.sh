#!/usr/bin/env bash
# Build the board-side burnd daemon for both supported architectures.
#
# Usage: scripts/build-burnd.sh <version> [out-dir]
#
# Needs riscv64-linux-gnu-gcc and aarch64-linux-gnu-gcc (Debian packages
# gcc-riscv64-linux-gnu and gcc-aarch64-linux-gnu). The result is static,
# so it runs on the buildroot image regardless of its libc version.
set -euo pipefail

VERSION=${1:?usage: $0 <version> [out-dir]}
OUT=${2:-dist}

SRC=$(cd "$(dirname "$0")/../burnd" && pwd)
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

for arch in riscv64 aarch64; do
    echo "building burnd for $arch"
    make -C "$SRC" clean >/dev/null 2>&1 || true
    make -C "$SRC" ARCH=$arch >/dev/null
    cp "$SRC/burnd" "$OUT/burnd-$VERSION-$arch"
done
make -C "$SRC" clean >/dev/null 2>&1 || true

echo "wrote $OUT/burnd-$VERSION-{riscv64,aarch64}"
