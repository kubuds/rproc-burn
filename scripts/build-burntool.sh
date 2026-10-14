#!/usr/bin/env bash
# Cross-compile burntool for every supported host, archive each binary and
# emit checksums plus an Arduino Board Manager "systems" JSON fragment.
#
# Usage: scripts/build-burntool.sh <version> [out-dir]
#
# Environment:
#   DOWNLOAD_URL  base URL the archives will be served from; defaults to the
#                 GitHub release page of this repository for tag v<version>.
#   GO            go binary to use (default: go from PATH).
set -euo pipefail

VERSION=${1:?usage: $0 <version> [out-dir]}
OUT=${2:-dist}
GO=${GO:-go}
REPO=${GITHUB_REPOSITORY:-kubuds/sg200x-burn}
DOWNLOAD_URL=${DOWNLOAD_URL:-https://github.com/$REPO/releases/download/v$VERSION}

# Arduino host triplet -> GOOS/GOARCH. The triplet is what arduino-cli
# reports for the machine running the IDE; windows-arm64 and apple-arm64
# users with an index lacking their host fall back to the x64 entry.
HOSTS=(
    x86_64-mingw32:windows/amd64
    i686-mingw32:windows/386
    arm64-mingw32:windows/arm64
    x86_64-linux-gnu:linux/amd64
    aarch64-linux-gnu:linux/arm64
    riscv64-linux-gnu:linux/riscv64
    x86_64-apple-darwin:darwin/amd64
    arm64-apple-darwin:darwin/arm64
)

SRC=$(cd "$(dirname "$0")/../burntool" && pwd)
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

systems=()
for entry in "${HOSTS[@]}"; do
    host=${entry%%:*}
    target=${entry#*:}
    goos=${target%/*}
    goarch=${target#*/}

    exe=burntool
    ext=tar.gz
    if [ "$goos" = windows ]; then
        exe=burntool.exe
        ext=zip
    fi

    # Arduino strips exactly one top-level directory when extracting, so
    # every archive contains a single burntool/ directory.
    dir="$STAGE/$host/burntool"
    mkdir -p "$dir"
    echo "building $host ($goos/$goarch)"
    (cd "$SRC" && CGO_ENABLED=0 GOOS=$goos GOARCH=$goarch \
        "$GO" build -trimpath -ldflags "-s -w -X main.version=$VERSION" \
        -o "$dir/$exe" .)

    name="burntool-$VERSION-$host.$ext"
    archive="$OUT/$name"
    rm -f "$archive"
    if [ "$ext" = zip ]; then
        (cd "$STAGE/$host" && zip -qrX "$archive" burntool)
    else
        tar -C "$STAGE/$host" --owner=0 --group=0 --numeric-owner -czf "$archive" burntool
    fi

    sha=$(sha256sum "$archive" | cut -d' ' -f1)
    size=$(stat -c %s "$archive")
    systems+=("$(printf '    {\n      "host": "%s",\n      "url": "%s/%s",\n      "archiveFileName": "%s",\n      "checksum": "SHA-256:%s",\n      "size": "%s"\n    }' \
        "$host" "$DOWNLOAD_URL" "$name" "$name" "$sha" "$size")")
done

# Fragment in the format of package_*_index.json tools[].systems, so the
# Arduino core release script can splice it in without recomputing hashes.
{
    echo '{'
    echo "  \"name\": \"burntool\","
    echo "  \"version\": \"$VERSION\","
    echo '  "systems": ['
    for i in "${!systems[@]}"; do
        sep=,
        [ "$i" -eq $((${#systems[@]} - 1)) ] && sep=
        printf '%s%s\n' "${systems[$i]}" "$sep"
    done
    echo '  ]'
    echo '}'
} > "$OUT/burntool-$VERSION-systems.json"

echo "wrote $OUT/burntool-$VERSION-systems.json"
