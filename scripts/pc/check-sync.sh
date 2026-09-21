#!/usr/bin/env bash
# Compare the scripts deployed on the phone with the ones in this repo.
# Prints one line per file and exits non-zero if anything differs or is missing.
#
# usage: check-sync.sh <ssh-host>     (a host that logs in as root on the phone)
set -uo pipefail

HOST="${1:?usage: check-sync.sh <ssh-host>}"
HERE="$(cd "$(dirname "$0")/../phone" && pwd)"
BASE=/data/adb/phoneserver
rc=0

check() {   # <repo-file> <path-on-phone>
    if ! remote=$(ssh -o BatchMode=yes -o ConnectTimeout=6 "$HOST" "cat $2" 2>/dev/null); then
        echo "MISSING  $2"; rc=1
    elif [ "$remote" = "$(cat "$1")" ]; then
        echo "same     $2"
    else
        echo "DIFFERS  $2   (run scripts/pc/install.sh $HOST, or copy the change back into the repo)"; rc=1
    fi
}

for f in arch.sh ui.sh autostart.sh; do check "$HERE/$f" "$BASE/$f"; done
check "$HERE/boot-hook.sh" /data/adb/service.d/phoneserver.sh

# the on-screen display
REPO="$HERE/../.."
[ -f "$REPO/display/run.sh" ] && check "$REPO/display/run.sh" "$BASE/display/run.sh"
if [ -f "$REPO/display/build/status.jar" ]; then
    want=$(sha256sum "$REPO/display/build/status.jar" | cut -d' ' -f1)
    have=$(ssh -o BatchMode=yes -o ConnectTimeout=6 "$HOST" "/data/adb/magisk/busybox sha256sum $BASE/display/status.jar" 2>/dev/null | cut -d' ' -f1 || true)
    if [ -z "$have" ]; then echo "MISSING  $BASE/display/status.jar   (run scripts/pc/install.sh $HOST)"; rc=1
    elif [ "$have" = "$want" ]; then echo "same     $BASE/display/status.jar"
    else echo "DIFFERS  $BASE/display/status.jar   (run scripts/pc/install.sh $HOST)"; rc=1; fi
fi

# the daemon binary: is the one on the phone built from the current source?
if [ -f "$HERE/../../daemon/phoneserverd.c" ]; then
    want=$(sha256sum "$HERE/../../daemon/phoneserverd.c" | cut -d' ' -f1)
    have=$(ssh -o BatchMode=yes -o ConnectTimeout=6 "$HOST" "cat $BASE/phoneserverd.src.sha256" 2>/dev/null || true)
    if [ -z "$have" ]; then echo "MISSING  $BASE/phoneserverd   (run scripts/pc/build-daemon.sh $HOST)"; rc=1
    elif [ "$have" = "$want" ]; then echo "same     $BASE/phoneserverd (built from the current source)"
    else echo "DIFFERS  $BASE/phoneserverd   (built from older source: run scripts/pc/build-daemon.sh $HOST)"; rc=1; fi
fi

exit $rc
