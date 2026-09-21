#!/bin/sh
# Runs INSIDE the chroot. Makes pacman usable on a phone connection.
#
#   - measures a list of mirrors and keeps the fastest five (mirrors here fail at random,
#     so a long fallback list matters more than a perfect ranking) (the default geo-redirect
#     can send you across an ocean, and pacman aborts downloads that stall for 10 s)
#   - turns off pacman's download-stall timeout and lowers parallel downloads
#   - enables the small [aur] repo that ships with Arch Linux ARM
#   - optionally removes the kernel and PC firmware (about 1.3 GB, useless in a chroot)
#
# usage: setup-pacman.sh [--drop-kernel]
set -e

CONF=/etc/pacman.conf
LIST=/etc/pacman.d/mirrorlist
BASES="
http://dk.mirror.archlinuxarm.org
http://de.mirror.archlinuxarm.org
http://de4.mirror.archlinuxarm.org
http://gr.mirror.archlinuxarm.org
http://tw.mirror.archlinuxarm.org
http://mirrors.dotsrc.org/archlinuxarm
https://mirrors.bfsu.edu.cn/archlinuxarm
https://mirrors.ustc.edu.cn/archlinuxarm
http://mirror.archlinuxarm.org
"

cp -n "$LIST" "$LIST.orig" 2>/dev/null || true
cp -n "$CONF" "$CONF.orig" 2>/dev/null || true

echo "measuring mirrors (two small downloads each; a single sample is too noisy)..."
tmp=$(mktemp)
for m in $BASES; do
    total=0; ok=1
    for try in 1 2; do
        r=$(curl -sL -m 12 -o /dev/null -w '%{http_code} %{speed_download}' "$m/aarch64/core/core.db" 2>/dev/null) || r="000 0"
        code=${r% *}; speed=${r#* }; speed=${speed%.*}
        [ "$code" = "200" ] || ok=0
        total=$((total + speed))
    done
    avg=$((total / 2))
    printf '  %-48s %s %s B/s\n' "$m" "$([ $ok = 1 ] && echo ok || echo FAIL)" "$avg"
    [ $ok = 1 ] && echo "$avg $m" >> "$tmp"
done

best=$(sort -rn "$tmp" | head -5 | cut -d' ' -f2)
[ -n "$best" ] || { echo "no mirror answered, leaving the mirror list alone" >&2; exit 1; }
DEFAULT=http://mirror.archlinuxarm.org
{
    echo "# Fastest first, measured by setup-pacman.sh. Pacman falls through to the next on failure."
    for m in $best; do echo "Server = $m/\$arch/\$repo"; done
    echo "$best" | grep -qxF "$DEFAULT" || echo "Server = $DEFAULT/\$arch/\$repo"
} > "$LIST"
rm -f "$tmp"

grep -q '^DisableSandbox' "$CONF"          || sed -i '/^\[options\]/a DisableSandbox' "$CONF"
grep -q '^DisableDownloadTimeout' "$CONF"  || sed -i '/^\[options\]/a DisableDownloadTimeout' "$CONF"
if grep -q '^ParallelDownloads' "$CONF"; then
    sed -i 's/^ParallelDownloads.*/ParallelDownloads = 3/' "$CONF"
else
    sed -i '/^\[options\]/a ParallelDownloads = 3' "$CONF"
fi
# enable the [aur] repo header and the Include line right after it
if grep -q '^#\[aur\]' "$CONF"; then
    awk '/^#\[aur\]/{sub(/^#/,""); print; getline; sub(/^#/,""); print; next} {print}' "$CONF" > "$CONF.new" && cat "$CONF.new" > "$CONF" && rm "$CONF.new"
fi

echo "selected mirrors:"; grep '^Server' "$LIST"

if [ "${1:-}" = "--drop-kernel" ]; then
    echo "removing the kernel and PC firmware packages..."
    pacman -Rns --noconfirm $(pacman -Qq | grep -E '^linux-(aarch64|firmware)') || true
fi

pacman -Syy --noconfirm
echo "done. 'pacman -Syu' should work now."
