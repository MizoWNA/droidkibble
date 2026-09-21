#!/system/bin/sh
# Arch chroot manager. Usage: arch.sh mount|umount|status|run <cmd...>|enter
R=/data/arch
BB=/data/adb/magisk/busybox
mounted() { $BB grep -q " $R/proc " /proc/mounts; }
do_mount() {
  rm -f $R/etc/resolv.conf; echo "nameserver 1.1.1.1" > $R/etc/resolv.conf
  mounted && return 0
  # /data is mounted nosuid,nodev on Android, which breaks sudo/su for non-root users in the chroot.
  # Bind the chroot onto itself and clear those flags on that mount only.
  if [ "$($BB awk -v r="$R" '$2==r {print "y"}' /proc/mounts)" != "y" ]; then
    $BB mount --bind $R $R && $BB mount -o remount,bind,suid,dev,exec $R
  fi
  $BB mount --bind /dev $R/dev
  $BB mount -t devpts devpts $R/dev/pts 2>/dev/null || $BB mount --bind /dev/pts $R/dev/pts
  $BB mount -t proc proc $R/proc
  $BB mount -t sysfs sysfs $R/sys
  $BB mount -t tmpfs tmpfs $R/tmp
  $BB mount -t tmpfs tmpfs $R/run
}
do_umount() {
  for m in run tmp sys proc dev/pts dev; do $BB umount -l $R/$m 2>/dev/null; done
  $BB umount -l $R 2>/dev/null   # the self bind mount from do_mount
}
case "$1" in
  mount) do_mount ;;
  umount) do_umount ;;
  status) mounted && echo mounted || echo unmounted ;;
  run) shift; do_mount
       $BB chroot $R /usr/bin/env -i HOME=/root TERM=xterm PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin /bin/bash -c "$*" ;;
  enter) do_mount
       exec $BB chroot $R /usr/bin/env -i HOME=/root TERM=xterm PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin /bin/bash -l ;;
  *) echo "usage: $0 mount|umount|status|run <cmd>|enter" ;;
esac
