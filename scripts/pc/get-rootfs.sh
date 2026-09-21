#!/usr/bin/env bash
# Download the Arch Linux ARM (aarch64) root filesystem once and cache it.
# It's about 830 MB, so this avoids re-downloading it if you ever redo the setup.
#
# usage: get-rootfs.sh [cache-dir]     (default: ~/.cache/droidkibble)
set -euo pipefail

DEST="${1:-$HOME/.cache/droidkibble}"
URL="http://os.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz"
FILE="$DEST/rootfs.tar.gz"

mkdir -p "$DEST"
if [ -s "$FILE" ] && gzip -t "$FILE" 2>/dev/null; then
    echo "already cached: $FILE"
    exit 0
fi

echo "downloading to $FILE ..."
curl -fL --progress-bar -o "$FILE.part" "$URL"
gzip -t "$FILE.part"
mv "$FILE.part" "$FILE"
echo "done: $FILE"
