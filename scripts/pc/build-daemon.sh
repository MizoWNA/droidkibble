#!/usr/bin/env bash
# Build phoneserverd on the phone (inside the Arch chroot, which needs gcc and make) and install
# the static binary. The result is a plain Linux binary that runs on Android outside the chroot.
#
# Needs: scripts/pc/install.sh already run, and a compiler in the chroot (setup-aur.sh installs
# one, or: pacman -S gcc make). Safe to re-run; a running daemon keeps using the old binary until
# it is restarted.
#
# usage: build-daemon.sh <ssh-host>     (a host that logs in as root on the phone)
set -euo pipefail

HOST="${1:?usage: build-daemon.sh <ssh-host>}"
SRC="$(cd "$(dirname "$0")/../../daemon" && pwd)"
BASE=/data/adb/phoneserver

echo "==> checking the source compiles here first"
make -C "$SRC" check

echo "==> copying the source into the chroot"
ssh "$HOST" 'rm -rf /data/arch/root/phoneserverd && mkdir -p /data/arch/root/phoneserverd'
tar -C "$SRC" -cf - phoneserverd.c Makefile | ssh "$HOST" 'tar -C /data/arch/root/phoneserverd -xf -'

echo "==> building on the phone"
ssh "$HOST" "$BASE/arch.sh run 'cd /root/phoneserverd && make clean && make'"

echo "==> installing $BASE/phoneserverd"
ssh "$HOST" "cp /data/arch/root/phoneserverd/phoneserverd $BASE/phoneserverd.new && chmod 755 $BASE/phoneserverd.new && mv $BASE/phoneserverd.new $BASE/phoneserverd"
ssh "$HOST" "$BASE/phoneserverd --version"
# remember which source this binary was built from, so check-sync.sh can spot a stale binary
sha256sum "$SRC/phoneserverd.c" | cut -d' ' -f1 | ssh "$HOST" "cat > $BASE/phoneserverd.src.sha256"
