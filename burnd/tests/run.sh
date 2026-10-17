#!/usr/bin/env bash
# Exercise burnd against a fake remoteproc sysfs directory and a pty pair.
# Usage: tests/run.sh path/to/burnd   (needs socat and python3)
# Set BURND_RUNNER (e.g. qemu-riscv64-static) to run a cross-built binary.
set -euo pipefail

BURND=$(realpath "${1:?usage: $0 path/to/burnd}")
RUNNER=${BURND_RUNNER:-}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; wait 2>/dev/null; rm -rf "$WORK"' EXIT

mkdir -p "$WORK/rproc" "$WORK/fw"
echo offline > "$WORK/rproc/state"
echo cvirtos.elf > "$WORK/rproc/firmware"

# Fake kernel: "start" becomes "running", "stop" becomes "offline".
(
    while true; do
        case "$(cat "$WORK/rproc/state" 2>/dev/null)" in
        start*) echo running > "$WORK/rproc/state" ;;
        stop*) echo offline > "$WORK/rproc/state" ;;
        esac
        sleep 0.05
    done
) &

socat -d -d pty,raw,echo=0,link="$WORK/board" pty,raw,echo=0,link="$WORK/host" 2>/dev/null &
for _ in $(seq 50); do [ -e "$WORK/board" ] && [ -e "$WORK/host" ] && break; sleep 0.1; done

$RUNNER "$BURND" -v -d "$WORK/board" -r "$WORK/rproc" -D "$WORK/fw" -f test.elf 2> "$WORK/burnd.log" &
sleep 0.3

fail() { echo "FAIL: $*"; echo "--- burnd log"; cat "$WORK/burnd.log"; exit 1; }
client() { python3 "$HERE/client.py" "$WORK/host" "$@"; }

head -c 300000 /dev/urandom > "$WORK/a.bin"
head -c 70000 /dev/urandom > "$WORK/b.bin"

echo "== upload, tail right behind the data"
client upload "$WORK/a.bin" --no-delay
cmp "$WORK/a.bin" "$WORK/fw/test.elf" || fail "image differs"
[ "$(cat "$WORK/rproc/state")" = running ] || fail "core not running"
[ "$(head -c 8 "$WORK/rproc/firmware")" = test.elf ] || fail "firmware name not set"

echo "== second upload keeps a backup"
client upload "$WORK/b.bin"
cmp "$WORK/b.bin" "$WORK/fw/test.elf" || fail "second image differs"
cmp "$WORK/a.bin" "$WORK/fw/test_old.elf" || fail "backup differs"

echo "== bad crc is rejected and the image untouched"
client upload "$WORK/a.bin" --bad-crc
cmp "$WORK/b.bin" "$WORK/fw/test.elf" || fail "image clobbered by bad transfer"
[ ! -e "$WORK/fw/test.elf.tmp" ] || fail "temp file left behind"

echo "== stop / start / unknown command / noise before magic"
client cmd STOP
[ "$(cat "$WORK/rproc/state")" = offline ] || fail "stop did not stop"
client cmd START
[ "$(cat "$WORK/rproc/state")" = running ] || fail "start did not start"
client raw aa5509000000000000 | grep -q 'reply (5, 0, 0)' || fail "unknown command not answered with ERROR"
client raw 0011aa5500000000000000 | grep -q 'reply (0, 0, 0)' || fail "noise before magic not skipped"

echo "== burnd log"
cat "$WORK/burnd.log"
echo "PASS"
