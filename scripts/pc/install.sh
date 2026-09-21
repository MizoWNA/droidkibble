#!/usr/bin/env bash
# Install the phone-side scripts over SSH. Safe to re-run.
# Headless mode stays OFF until you turn it on yourself (see the end of this script).
#
# usage: install.sh <ssh-host>
#   <ssh-host>  an ssh host that logs in as root on the phone (e.g. "phone-root" in ~/.ssh/config)
set -euo pipefail

HOST="${1:?usage: install.sh <ssh-host>}"
HERE="$(cd "$(dirname "$0")/../phone" && pwd)"
BASE=/data/adb/phoneserver

ssh "$HOST" "mkdir -p $BASE /data/adb/service.d"

for f in arch.sh ui.sh autostart.sh; do
    ssh "$HOST" "cat > $BASE/$f && chmod 755 $BASE/$f" < "$HERE/$f"
    echo "installed $BASE/$f"
done

# the on-screen display (built by display/build.sh); skipped with a hint if it hasn't been built
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
if [ -f "$REPO/display/build/status.jar" ]; then
    ssh "$HOST" "mkdir -p $BASE/display"
    ssh "$HOST" "cat > $BASE/display/status.jar" < "$REPO/display/build/status.jar"
    ssh "$HOST" "cat > $BASE/display/run.sh && chmod 755 $BASE/display/run.sh" < "$REPO/display/run.sh"
    echo "installed $BASE/display/{status.jar,run.sh}"
else
    echo "on-screen display not installed: run scripts/pc/get-display-tools.sh and display/build.sh, then this again"
fi

ssh "$HOST" "cat > /data/adb/service.d/phoneserver.sh && chmod 755 /data/adb/service.d/phoneserver.sh" < "$HERE/boot-hook.sh"
echo "installed boot hook /data/adb/service.d/phoneserver.sh"

cat <<EOF

Installed. Nothing changes until the next boot, and the Android UI stays on.

To also switch the UI off after boot (headless mode):
    ssh $HOST touch $BASE/headless
To undo it:
    ssh $HOST rm $BASE/headless
    ssh $HOST $BASE/ui.sh on
EOF
