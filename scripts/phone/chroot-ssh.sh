#!/bin/sh
# Runs INSIDE the chroot: authorizes one SSH key and starts sshd on port 2222 (key-only).
#
# usage (from the phone's root shell):
#   /data/adb/phoneserver/arch.sh run "sh /root/chroot-ssh.sh /root/mykey.pub"
#
# sshd is tracked with a pid file on purpose. The chroot shares the phone's process
# list, so `pkill sshd` here would also kill the phone's own sshd (and your session).
set -e

PUBKEY="${1:?usage: chroot-ssh.sh <public-key-file>}"

mkdir -p /root/.ssh /run/sshd
chmod 700 /root/.ssh
cat "$PUBKEY" >> /root/.ssh/authorized_keys
chmod 600 /root/.ssh/authorized_keys

ssh-keygen -A > /dev/null

cat > /etc/ssh/sshd_config.d/10-phoneserver.conf <<CONF
Port 2222
PasswordAuthentication no
KbdInteractiveAuthentication no
UsePAM no
PermitRootLogin prohibit-password
CONF

[ -f /run/sshd-chroot.pid ] && kill "$(cat /run/sshd-chroot.pid)" 2>/dev/null || true
/usr/bin/sshd -o PidFile=/run/sshd-chroot.pid
echo "sshd is listening on port 2222"
