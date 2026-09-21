#!/system/bin/sh
# Magisk service.d hook: bring up the Arch chroot + its sshd at boot.
# Nothing of ours survives a reboot, so any pid, lock or status files left behind are stale (pids get reused,
# and an old status.json would look like a live daemon).
rm -f /data/adb/phoneserver/phoneserverd.lock /data/adb/phoneserver/supervisor.pid /data/adb/phoneserver/wdt1.pid /data/adb/phoneserver/daemon.stop /data/adb/phoneserver/status.json
until [ "$(getprop sys.boot_completed)" = "1" ]; do sleep 2; done
sleep 10   # let Wi-Fi settle
[ -f /data/adb/phoneserver/no-arch ] && exit 0
/data/adb/phoneserver/arch.sh run "mkdir -p /run/sshd; [ -f /run/sshd-chroot.pid ] && kill \$(cat /run/sshd-chroot.pid) 2>/dev/null; /usr/bin/sshd -o PidFile=/run/sshd-chroot.pid" >> /data/adb/phoneserver/boot.log 2>&1
echo "$(date) arch up" >> /data/adb/phoneserver/boot.log
[ -x /data/adb/phoneserver/autostart.sh ] && /data/adb/phoneserver/autostart.sh >> /data/adb/phoneserver/boot.log 2>&1
