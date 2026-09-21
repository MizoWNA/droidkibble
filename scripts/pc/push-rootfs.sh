#!/usr/bin/env bash
# Copy the cached rootfs to the phone over adb and unpack it into /data/arch (as root).
# Needs about 2 GB free on the phone. Over Wi-Fi adb the push takes a few minutes.
#
# usage: push-rootfs.sh <adb-target> [tarball]
#   <adb-target>  what `adb devices` shows: a USB serial, or ip:5555
set -euo pipefail

TARGET="${1:?usage: push-rootfs.sh <adb-target> [tarball]}"
TARBALL="${2:-$HOME/.cache/droidkibble/rootfs.tar.gz}"
BB=/data/adb/magisk/busybox

[ -s "$TARBALL" ] || { echo "no tarball at $TARBALL, run get-rootfs.sh first" >&2; exit 1; }

echo "pushing $(du -h "$TARBALL" | cut -f1) ..."
adb -s "$TARGET" push "$TARBALL" /data/local/tmp/arch.tar.gz

echo "unpacking (this takes a minute or two) ..."
adb -s "$TARGET" shell "su -c 'mkdir -p /data/arch && $BB tar -xzpf /data/local/tmp/arch.tar.gz -C /data/arch && rm /data/local/tmp/arch.tar.gz && du -sh /data/arch'"
echo "done. rootfs is in /data/arch on the phone."
