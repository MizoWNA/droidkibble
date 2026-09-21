# Changelog

Newest first. Each entry says what changed and, where it matters, why. Anything that isn't
verified on real hardware is marked as such.

## Unreleased (after 0.1.0)

### Added
- `scripts/pc/kibble`, a terminal dashboard (Python, standard library only). A pixel dog shows whether the watchdog is being fed, with battery, memory, network, DHCP and watchdog details around it. The bottom third is a persistent shell on the phone, either the Android root shell or the Arch chroot (Tab switches, each keeps its own directory). While a command runs, what you type goes to it, so prompts like pacman's `[Y/n]` can be answered. F5 and F6 switch headless mode on and off, now and at boot. See `docs/companion.md`.
- kibble finds the phone on any network by its SSH host key, with a scan-and-login screen when it can't (`kibble`, `:connect`), plus `kibble learn`, `find` and `connect`. `kibble ssh-config --install` adds a `ProxyCommand` to `~/.ssh/config` so plain `ssh`, `scp` and `phonessh` follow the phone from network to network. Password login is written but untested.

### On-screen display
- `phoneserverd` 0.2 runs the display and reads the power button. The screen starts dark in headless mode; a press lights it and starts the display program, another press (or the auto-off timer, 10 minutes by default) turns it off. It finds the power key by capability, not by device number. `status.json` gained a `display` block, `ui.sh` reads an optional `display.conf` (theme, brightness, timeout), `ui.sh on` removes any leftover display, and `install.sh` and `check-sync.sh` handle the display files. Details in `docs/daemon.md`.
- `display/`: a small Java program that draws the phone's status on the screen through SurfaceFlinger while the UI is off. It has a tiny framework (pages, a data bus that also reads extra JSON files, themes, a drawing toolkit) and one page, a letterpress-style status ticket, in a paper theme and an OLED-friendly night theme. `scripts/pc/get-display-tools.sh` downloads the JDK, d8 and android.jar it is built with (about 260 MB, into `~/.local/droidkibble-sdk`). See `display/README.md`.
- Checked on the Galaxy A30: the first prototype was seen on the physical panel, and the ticket page is confirmed in `screencap` screenshots of SurfaceFlinger's output in both themes. It uses about 100 MB of RAM. With the daemon: a simulated power press (a key event written into the input device) turns the screen on and off, the auto-off timer works, a killed display program turns the screen off, `ui.sh on` with the screen lit leaves nothing behind, and `ui.sh off` picks up `display.conf`. Not checked: a real press of the physical button, battery cost, runs of many hours, other phones.

### Fixed
- `scripts/phone/diagnose.sh` took minutes with the UI running (it started a `readlink` for every file descriptor of every process). It now uses one `ls` and takes about 2 seconds. I only found this by running it with the normal UI up; the headless run had been fast.

### Verified on hardware (Galaxy A30)
- Carrying the phone out of range of the router while headless: the daemon logged `network down` and, 5 min 8 s later, restored the Android UI (`router unreachable for too long`). The phone did not reset. Seen once.
- Going headless on a second network (a cafe router on another subnet): the daemon found the new gateway and the router renewed the lease for 24 hours.
- `diagnose.sh` with the UI running shows `system_server` holding `/dev/watchdog1` and `watchdogd` holding `/dev/watchdog`.
- kibble: shells that keep their directory, answering `pacman -S` prompts from the chroot, Ctrl-C stopping commands and letting pacman release its lock, headless on and off from F5, and re-finding the phone after it changed address. Details and gaps are in `docs/companion.md`.

## 0.1.0 (first public version)

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
