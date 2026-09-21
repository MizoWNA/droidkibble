# How it works

## The pieces

A chroot is a folder that programs treat as the whole filesystem. Android and its kernel keep running as normal, and Arch lives in `/data/arch`. `arch.sh` mounts `/dev`, `/proc`, `/sys` and a few tmpfs directories into it, then runs whatever you ask inside. Nothing is emulated and there's no VM, so it runs as fast as the phone can.

At boot, Magisk runs everything in `/data/adb/service.d/`. That's where `boot-hook.sh` lives. It waits for Android to finish booting, mounts the chroot, starts an SSH server in it, and then calls `autostart.sh`, which decides whether to go headless.

I didn't use Termux:Boot for this. It only fires after the phone has been unlocked once, and on my phone it never fired at all.

## The headless problem

Stopping the Android UI is easy:

```sh
setprop ctl.stop zygote
```

That frees most of the RAM, because `system_server`, SystemUI, Google Play services and the rest all go with it. On my phone, though, everything froze and it rebooted a little under two minutes later. Every time.

### What the crash looked like

- The network dropped and adb went offline.
- Logs simply ended, with no error at the end. A hard hang, not a graceful shutdown.
- The time from stopping the UI to the crash was almost constant: 98, 95, 100 and 102 seconds in four recorded runs. It didn't depend on how long the phone had been up.

A fixed delay after an event smells like a timer, and the obvious timer on a phone is a watchdog.

### The cause

A hardware watchdog resets the chip if software doesn't "pet" it regularly. Listing who has watchdog devices open shows two:

```sh
for p in /proc/[0-9]*; do ls -l $p/fd 2>/dev/null | grep -q watchdog && echo "$p $(cat $p/cmdline | tr '\0' ' ')"; done
```

| device | held by |
|---|---|
| `/dev/watchdog` | `watchdogd` (standard Android, keeps running after the UI stops) |
| `/dev/watchdog1` | `system_server` |

So Samsung's `system_server` feeds a second watchdog. When the UI stops, `system_server` exits, nothing feeds `/dev/watchdog1` any more, and about 100 seconds later the hardware pulls the plug.

### The fix

After `system_server` exits, open `/dev/watchdog1` from a small process and keep feeding it. BusyBox already has one:

```sh
busybox watchdog -t 5 -T 60 /dev/watchdog1
```

That's what `ui.sh off` does: stop the framework, wait until `/dev/watchdog1` is free, start a feeder. `ui.sh on` stops the feeder first and then restarts the framework, and the new `system_server` takes the device back.

The feeder is now `phoneserverd` (see [daemon.md](daemon.md)), which also renews the DHCP lease and watches the connection. The BusyBox command above is still what `ui.sh` uses if the daemon binary isn't installed.

### Things that were tried first and didn't help

If you're chasing the same crash, you can skip these dead ends.

| tried | result |
|---|---|
| stop the boot animation (it loops forever after the UI dies) | crash came a bit later, or the same time |
| hold a kernel wakelock so the phone can't suspend | no effect (kept, it's harmless) |
| disable the deep CPU idle state | still crashed |
| stop the radio/telephony daemons before the UI | still crashed, and caused a storm of HAL retry errors |
| Linux Deploy's `stop zygote; sleep 2; stop bootanim; stop surfaceflinger` | solves a different problem (the UI respawning), not this one |

## Other traps

**A bootloop-protection Magisk module can fight you.** "Magisk Bootloop Protector" watches zygote's process ID for the first ~50 seconds after boot and reboots the phone if zygote disappears. `autostart.sh` waits until the phone has been up for 150 seconds before stopping the UI, so it stays clear of that window. If you run a similar module, check what it does.

**Don't `pkill sshd` inside the chroot.** The chroot and the phone share one process list, so it would also kill the phone's own SSH server, and with it your session. `chroot-ssh.sh` uses a pid file and kills only its own process.

**Without the framework, nobody renews the DHCP lease.** `wpa_supplicant` and the rest of the network stack keep running, so the connection stays up, but lease renewal is done by the framework. `phoneserverd` has a small DHCP client of its own for this (BusyBox `udhcpc` can't run here: Android's SELinux policy denies an ioctl it needs, even for root). A DHCP reservation in the router is still the safest setup. If the router can't be reached for about five minutes the daemon restores the UI, and Android reconnects on its own.

## Android quirks that show up inside the chroot

A chroot on Android shares the phone's kernel, and Android's kernel and mount options are not what a normal Linux distro expects. These are the ones that came up while making `pacman` and the AUR work.

| what breaks | why | what fixes it |
|---|---|---|
| Non-root users get "could not resolve host" (root is fine) | Android only lets processes in group 3003 (`inet`) open network sockets, and root is exempt | put the user in a group with gid 3003 (`setup-aur.sh` creates `aid_inet`) |
| `sudo`: "effective uid is not 0 ... nosuid" | `/data` is mounted `nosuid,nodev`, so setuid programs don't work | `arch.sh` bind-mounts the chroot onto itself and clears those flags on that mount only |
| `fakeroot`: "lack of SYSV IPC support" | stock fakeroot needs System V IPC, which Android kernels omit | build fakeroot from source with `--with-ipc=tcp` into `/usr/local` (`setup-aur.sh`) |
| `makepkg` refuses to run | it won't run as root | use an unprivileged user, with the two fixes above |
| a `systemd-sysusers` hook fails on every install: "Protocol driver not attached" | that tool needs newer kernel features than 4.4 | harmless for existing users; when a package needs to create its own system user, add the user by hand with `useradd` |
| `pacman` aborts with "Operation too slow" or timeouts | the default mirror geo-redirects to the other side of the world, and pacman gives up on any download that stalls for 10 s | pick mirrors by measured speed and set `DisableDownloadTimeout` (`setup-pacman.sh`) |
| `pacman` sandbox errors | the download sandbox needs kernel features 4.x doesn't have | `DisableSandbox` in `pacman.conf` |
| a full `pacman -Syu` wants ~400 MB | most of it is a kernel and PC hardware firmware that does nothing in a chroot | remove them (`setup-pacman.sh --drop-kernel`): upgrade shrinks to ~60 MB, and ~1.3 GB of disk comes back |

## Drawing on the screen

**Update:** there is a working route that doesn't need any of the display-driver work described further down. SurfaceFlinger and the hardware composer keep running in headless mode, so a small Java program run with `app_process` can ask SurfaceFlinger for a layer and draw on it with ordinary Canvas calls. It needs no `system_server`, and it draws through the same path Android's own UI uses, so none of the driver structures below have to be guessed. `display/` has the prototype (`Status.java`, `build.sh`, and `run.sh` for the phone). Checked so far: it starts, creates the layer, an early plain-text version was seen on the physical panel, and `screencap` screenshots of SurfaceFlinger's output show the finished ticket page. It uses about 100 MB of RAM while running. Not yet checked: battery cost and how it behaves over hours. The rest of this section is what I found before I knew about this route.


With the Android UI off the screen is dark, and it would be useful to show status on it. The findings so far, all from the Galaxy A30:

- The panel is on but the backlight sits at 0 (`/sys/class/backlight/panel/brightness`, range 0 to 365). Raising it shows whatever the display last held.
- `/dev/graphics/fb0` exists (1080x2340, 32-bit, red at bit 16, green at 8, blue at 0), but it is **only a control device**. `mmap` fails with `EINVAL` at both one screen and the full double-buffered length, and `write()` returns short. There is no pixel memory to draw into, and the kernel has no virtual-terminal or DRM support, so there is no text console either.
- Samsung's display driver (`CONFIG_EXYNOS_DECON_7885`, the `dpu_7885` directory in the kernel source) expects a buffer allocated through ION (`/dev/ion`, world-accessible) and submitted with its private `S3CFB_WIN_CONFIG` ioctl. That is how Android's own graphics composer draws.
- Stopping SurfaceFlinger and the hwcomposer (`ctl.stop surfaceflinger`, `ctl.stop vendor.hwcomposer-2-1`) is stable for at least 90 seconds and restarts cleanly, so the display can be taken over by something else.
- The decon ioctl set includes `S3CFB_FORCE_PANIC`, so the exact struct layouts must be taken from the kernel source and not guessed. Kernel source for this chip: the LineageOS mirror `android_kernel_samsung_universal7904`, `drivers/video/fbdev/exynos/dpu_7885/`.

One unexplained hard reset happened during these tests, right after writing to `fb0` while SurfaceFlinger still owned the display. Treat writing to the framebuffer with SurfaceFlinger running as unsafe.

`daemon/tools/fbtest.c` is the probe used for these findings. It runs one framebuffer operation per invocation with unbuffered output, so the last line survives a hang. Run it only with SurfaceFlinger and the hwcomposer stopped.

## How it was debugged

Some of this is worth reusing on other hard-to-see failures:

1. **Log commands as you run them** to a file you can `tail -f`, so "working" and "stuck" look different.
2. **Measure time from the action, not from boot.** A constant delay meant a timer.
3. **Stream the logs off the phone** while it's alive: `dmesg -w` and `logcat -b all` over SSH to your computer. When a phone hangs hard it can't write its last words anywhere, but your computer can.
4. **Look in `/sys/fs/pstore/` and `/proc/last_kmsg` after the reboot.** Some phones keep a crash dump there.
5. **Ask who owns the hardware.** Looking through `/proc/*/fd` for watchdog devices found the answer in one command.
