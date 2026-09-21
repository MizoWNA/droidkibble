# Changelog

Newest first. Each entry says what changed and, where it matters, why. Anything that isn't
verified on real hardware is marked as such.

## 1.0.0 (2026-09-21)

The whole thing now works end to end on the test phone: boot, headless mode with the supervisor, a dashboard on your computer, and a status screen on the phone itself. "1.0" means that, on one phone. It does not mean it has been run for weeks; see the limits in the README.

### Added
- The on-screen display is part of `phoneserverd` now (the daemon reports 1.0.0). In headless mode the screen starts dark. Press the power button and it lights up with the status ticket; press again, or wait for the auto-off timer (10 minutes by default), and it goes dark. The daemon finds the power key by capability instead of by device number, owns the backlight, and stops the display if the display program dies.
- `ui.sh` reads an optional `display.conf` (`DISPLAY_THEME`, `DISPLAY_BRIGHTNESS`, `DISPLAY_TIMEOUT`), `ui.sh restart` restarts just the daemon to apply it, and `ui.sh on` removes any leftover display so its layer never sits on top of Android.
- `status.json` has a `display` block. `install.sh` and `check-sync.sh` handle the display files. kibble shows the screen state.
- Details are in `docs/daemon.md` and `display/README.md`.

### Verified on hardware (Galaxy A30)
- The physical power button turns the screen on and off.
- With key events written into the input device: on and off, the auto-off timer, a killed display program turning the screen off, `ui.sh on` with the screen lit leaving nothing behind, and `ui.sh off` picking up `display.conf`.

### Not verified
- Battery cost of the screen, and runs of many hours or days.
- Any phone other than the Galaxy A30.

## 0.6.0 (2026-09-21)

### Added
- A proper look for the display: a letterpress-style status ticket with a ruled grid, a perforation, and a stub whose big word says FED, HUNGRY or LOST, plus a round stamp with a dog in it. Two themes: paper (cream, blue ink, gold) and night (cream on navy over pure black, for the OLED).
- A small framework under it (`display/`): pages, a data bus that reads `status.json` and any other JSON file dropped into a directory, themes, and a drawing toolkit. The picture shifts a few pixels every few minutes to avoid burn-in. See `display/README.md`.

### Verified on hardware
- Both themes render correctly in `screencap` screenshots of SurfaceFlinger's output. The first plain-text prototype (0.5.0) had been seen on the physical panel. About 100 MB of RAM while running.

## 0.5.0 (2026-09-21)

### Added
- `display/`: the first working way to draw on the screen while the Android UI is off. A small Java program run with `app_process` asks SurfaceFlinger for a layer and paints on it, with no framework and no kernel display code. `scripts/pc/get-display-tools.sh` downloads the JDK, d8 and android.jar to build it (about 260 MB, into `~/.local/droidkibble-sdk`). Started by hand with `display/run.sh`.

### Verified on hardware
- Seen on the physical panel, and in a `screencap` of SurfaceFlinger's output.

## 0.4.1 (2026-09-21)

### Fixed
- kibble's shell hid prompts that have no trailing newline, and gave commands no input, so `pacman -S` waited forever at "Proceed with installation? [Y/n]" and could not be answered. Output without a newline now shows immediately, and what you type while a command runs goes to it.
- Ctrl-C used to drop the whole shell session, which could leave pacman's lock file behind. It now interrupts the command (SIGINT, then SIGTERM) and keeps the shell and its directory. The fixed two-minute command timeout is gone.
- Rewrote `docs/companion.md`.

### Verified on hardware
- Answering `pacman -S neovim` with `n` in the chroot, and pressing Ctrl-C at its prompt: pacman exited and its lock was released both times.

## 0.4.0 (2026-09-21)

### Added
- kibble finds the phone on any network by its SSH host key, with a scan-and-login screen when it can't (`kibble`, `:connect`), plus `kibble learn`, `find` and `connect`. `kibble ssh-config --install` adds a `ProxyCommand` to `~/.ssh/config` so plain `ssh`, `scp` and `phonessh` follow the phone from network to network. Password login is written but untested.
- Tab switches between the Android shell and the Arch chroot, the command line has real cursor editing, `clear` and Ctrl-L clear the output, and F5 and F6 switch headless mode on and off, now and at boot.

### Fixed
- The watchdog age in the dashboard always read 0, because the status file is rewritten right after each feed. It now counts up between updates.

### Verified on hardware
- The scan and login screen on a real network, finding the phone again after a wrong address was cached (about six seconds), `ssh` and `phonessh` following it through the ProxyCommand, and going from headless to Android and back with F5.

## 0.3.0 (2026-09-21)

### Changed
- kibble's shell is a real session now, so `cd` and variables carry over from one command to the next (before, every command was a separate `ssh`). The Android shell and the Arch chroot each have their own.
- kibble got its arcade look: a pixel-art dog in the middle, battery and memory on its left, network, DHCP and watchdog on its right, colors throughout, and the terminal limited to the bottom third of the window.

## 0.2.0 (2026-09-21)

### Added
- `scripts/pc/kibble`, a terminal dashboard for the phone (Python, standard library only): battery, memory, network, DHCP and watchdog state, an ASCII dog that shows whether the watchdog is being fed, and a command line that runs commands on the phone. See `docs/companion.md`.

## 0.1.1 (2026-09-21)

### Fixed
- `scripts/phone/diagnose.sh` took minutes with the UI running, because it started a `readlink` for every file descriptor of every process. It now uses one `ls` and takes about 2 seconds. I only found this by running it with the normal UI up; the headless run had been fast.

### Verified on hardware (Galaxy A30)
- Carrying the phone out of range of the router while headless: the daemon logged `network down` and, 5 min 8 s later, restored the Android UI (`router unreachable for too long`). The phone did not reset. Seen once.
- Going headless on a second network (a cafe router on another subnet): the daemon found the new gateway and the router renewed the lease for 24 hours.
- `diagnose.sh` with the UI running shows `system_server` holding `/dev/watchdog1` and `watchdogd` holding `/dev/watchdog`.

## 0.1.0 (2026-09-21, first public version)

### Added
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
- README and docs reworded to read less like a spec sheet. README is now written in the first person and lists what doesn't work up front.
- `scripts/phone/arch.sh` now bind-mounts the chroot onto itself with `suid` and `dev` enabled.
  `/data` is mounted `nosuid` on Android, which broke `sudo` for non-root users.

### Verified on hardware (Galaxy A30, Android 11, kernel 4.4)
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

### What the very first version did
- Arch Linux ARM chroot with key-only SSH and a Magisk `service.d` boot hook.
- Optional headless mode: stops the Android UI and feeds `/dev/watchdog1` itself. Without that,
  the phone resets about 100 seconds after the UI stops. See `docs/how-it-works.md`.
