#!/bin/sh
# Runs INSIDE the chroot. Sets up the AUR (via yay) on a phone, which needs three workarounds:
#
#   1. Android only lets processes in group 3003 open network sockets. Root is exempt, but the
#      unprivileged user that makepkg requires is not, so it is added to that group.
#   2. The stock fakeroot talks to its daemon over System V IPC, which Android kernels don't have.
#      It is rebuilt from source with TCP IPC (installed to /usr/local, ahead of /usr/bin).
#   3. sudo needs setuid to work, which needs the chroot mounted without nosuid (arch.sh does that).
#
# Downloads about 50 MB (mostly gcc). Safe to re-run.
#
# usage: setup-aur.sh [unprivileged-user]      (default: alarm, the Arch Linux ARM default user)
set -e

USER_NAME="${1:-alarm}"
id "$USER_NAME" >/dev/null 2>&1 || { echo "no such user: $USER_NAME" >&2; exit 1; }

echo "== toolchain and helpers"
pacman -S --noconfirm --needed git sudo debugedit gcc make autoconf automake libtool m4 patch pkgconf binutils

echo "== network groups for $USER_NAME"
getent group aid_inet    >/dev/null || groupadd -g 3003 aid_inet
getent group aid_net_raw >/dev/null || groupadd -g 3004 aid_net_raw
usermod -aG aid_inet,aid_net_raw "$USER_NAME"

echo "== sudo for $USER_NAME (passwordless); locking its password"
echo "$USER_NAME ALL=(ALL) NOPASSWD: ALL" > "/etc/sudoers.d/10-$USER_NAME"
chmod 440 "/etc/sudoers.d/10-$USER_NAME"
passwd -l "$USER_NAME" >/dev/null

echo "== fakeroot with TCP IPC"
if su "$USER_NAME" -c 'fakeroot -- true' >/dev/null 2>&1; then
    echo "   already working"
else
    work=$(mktemp -d); cd "$work"
    tarball=$(curl -s https://deb.debian.org/debian/pool/main/f/fakeroot/ | grep -o 'fakeroot_[0-9.]*\.orig\.tar\.gz' | sort -uV | tail -1)
    [ -n "$tarball" ] || { echo "couldn't find the fakeroot source" >&2; exit 1; }
    curl -sSfLO "https://deb.debian.org/debian/pool/main/f/fakeroot/$tarball"
    tar xf "$tarball"; cd fakeroot-*/
    ./configure --prefix=/usr/local --with-ipc=tcp >/dev/null
    make -j4 >/dev/null && make install >/dev/null
    ldconfig 2>/dev/null || true
    cd /; rm -rf "$work"
    su "$USER_NAME" -c 'fakeroot -- id -u' | grep -qx 0 || { echo "fakeroot still not working" >&2; exit 1; }
fi

echo "== yay"
if command -v yay >/dev/null 2>&1; then
    echo "   already installed: $(yay --version)"
else
    su "$USER_NAME" -c 'cd ~ && rm -rf yay-bin && git clone --depth 1 https://aur.archlinux.org/yay-bin.git && cd yay-bin && makepkg --noconfirm'
    pacman -U --noconfirm "$(eval echo ~"$USER_NAME")"/yay-bin/yay-bin-*.pkg.tar.*
fi

echo "done. As $USER_NAME:  yay -S <package>"
