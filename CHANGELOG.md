# Changelog

Newest first. Each entry says what changed and, where it matters, why. Anything that isn't
verified on real hardware is marked as such.

## 0.1.0 (first public version)

### Added
- `scripts/pc/kibble`: an arcade-style terminal dashboard (Python, standard library only). A pixel dog in the middle shows whether the watchdog is being fed, with battery and memory on its left and network, DHCP and watchdog on its right. The bottom third is a persistent shell on the phone (Android shell, or `@arch` for the chroot, each keeping its own directory). See `docs/companion.md`. Tested on Linux against the Galaxy A30 in headless mode through a pseudo-terminal; the look on real terminals, the other dog moods, and macOS are untested.
- Project name is now droidkibble (was the working title phone-server). The on-phone paths (`/data/adb/phoneserver/`) are unchanged.
- Issue templates in `.github/ISSUE_TEMPLATE/`: a device report and a bug form, both asking for `diagnose.sh` output.
- `LICENSE` (MIT).
- `scripts/phone/diagnose.sh`: a read-only report of the phone model, versions, watchdog devices and who holds them, and what is installed. Meant for people trying this on other phones and for bug reports. Run on the Galaxy A30 in headless state; it has not been run on any other phone.
- `docs/alternatives.md`: a survey of existing tools and how this project differs.
- `daemon/phoneserverd.c`, a supervisor for headless mode. It feeds `/dev/watchdog1`, renews the DHCP lease
  with its own small DHCP client, pings the router and restores the Android UI if it stays unreachable for
  5 minutes, and writes `status.json` and a log. It replaces the BusyBox watchdog feeder, which `ui.sh` still
  uses as a fallback when the binary isn't installed. See `docs/daemon.md`.
- `scripts/pc/build-daemon.sh` builds it on the phone; `check-sync.sh` now also checks the binary is current.
- A boot-loop breaker: if two headless sessions in a row end before 10 minutes, headless mode blocks itself.
- `extras/phonessh`: an optional script that opens a zsh session on the phone with your own
  oh-my-zsh theme, recolored so you can tell it apart from your computer. See `extras/README.md`.
- `scripts/phone/setup-pacman.sh`: picks the fastest mirrors, turns off pacman's download-stall
  timeout, enables the `[aur]` repo, and can remove the kernel and PC firmware (`--drop-kernel`).
- `scripts/phone/setup-aur.sh`: gets `yay` and the real AUR working (network group, fakeroot
  built with TCP IPC, sudo, toolchain).
- `scripts/pc/check-sync.sh`: compares the scripts deployed on the phone with the repo and reports drift.
- Docs: a table of Android quirks that show up inside the chroot, more troubleshooting rows.

### Investigated
- On-screen status display: `/dev/graphics/fb0` cannot be drawn to directly on this Samsung phone. It needs
  ION buffers and the private decon ioctl. Findings and the probe tool (`daemon/tools/fbtest.c`) are in
  `docs/how-it-works.md`. Not implemented yet.

### Changed
- `scripts/phone/diagnose.sh` no longer takes minutes with the UI on: it forked `readlink` for every file descriptor of every process. It now uses a single `ls` (about 2 s on the test phone). Found by running it with the normal UI up; the headless run had been fast.
- README and docs reworded to read less like a spec sheet. README is now written in the first person and lists what doesn't work up front.
- `scripts/phone/arch.sh` now bind-mounts the chroot onto itself with `suid` and `dev` enabled.
  `/data` is mounted `nosuid` on Android, which broke `sudo` for non-root users.

### Verified on hardware (Galaxy A30, Android 11, kernel 4.4)
- Going headless on a second network (a cafe router, 192.168.3.x): the daemon picked up the new gateway and the router renewed the lease (86400 s). `diagnose.sh` with the UI running shows `system_server` holding `/dev/watchdog1` and `watchdogd` holding `/dev/watchdog`, matching the write-up.
- Leaving the router's range while headless: the daemon logged `network down`, and 5 min 8 s later restored the Android UI (`restoring the Android UI: router unreachable for too long`), released the watchdog and stopped. The phone did not reset, stayed up, and Android was usable on a different network afterwards. Seen once, on one phone.
- `scripts/pc/install.sh` deployed to the phone, then a reboot: the chroot, its sshd and
  headless mode all came back from the installed paths.
- `setup-pacman.sh` (run twice) and `setup-aur.sh` (run on an already configured chroot, so it
  only exercised the "already done" paths). A full `pacman -Syu` and a real AUR package build
  both worked.

### Not verified yet
- `scripts/pc/get-rootfs.sh` and `scripts/pc/push-rootfs.sh`: the rootfs was put on the phone
  by hand with the same commands.
- `setup-aur.sh` on a fresh chroot start to finish (each step was done by hand and then the
  script was re-run over the result).
- Anything over several days: uptime, DHCP lease renewal while headless, Wi-Fi drops.

## 0.1.0: first commit
- Arch Linux ARM chroot with key-only SSH and a Magisk `service.d` boot hook.
- Optional headless mode: stops the Android UI and feeds `/dev/watchdog1` itself. Without that,
  the phone resets about 100 seconds after the UI stops. See `docs/how-it-works.md`.
