# Troubleshooting

| what you see | what it usually means |
|---|---|
| You're not sure what's going on with the phone | Run `ssh <phone-root> sh -s < scripts/phone/diagnose.sh`. It's read-only and prints the model, versions, watchdog holders and what's installed. Paste it in any issue you open. |
| `adb devices` shows `unauthorized` | The phone hasn't approved this computer. Unplug, replug, and accept the popup. If there's no popup, revoke USB debugging authorizations in Developer options. Also check the USB mode isn't "charging only". |
| adb shows the phone as `offline` (Wi-Fi adb) | The phone rebooted or is off the network. `adb connect <ip>:5555` again after it's back. |
| `ssh phone-root` times out after a reboot | Boot takes a minute or two on old phones. Wait, then check it's reachable with `ping`. |
| `ssh phone-root` works, `ssh arch` doesn't | The chroot's SSH server isn't running. Look at `/data/adb/phoneserver/boot.log` on the phone, and try `/data/adb/phoneserver/arch.sh run "pgrep -a sshd"`. |
| Everything inside the chroot fails with DNS errors | `/etc/resolv.conf` in the chroot is broken. `arch.sh` rewrites it each time it mounts; run `arch.sh run true` once. |
| `pacman` says "failed to retrieve" or times out | A mirror is slow. Comment out repos you don't need in `/etc/pacman.conf`. Also make sure `DisableSandbox` is set. |
| `pacman -Syu` fails with "Operation too slow" | Run `setup-pacman.sh` (see setup step 7): it picks fast mirrors and disables the stall timeout. |
| Everything works as root but a normal user says "could not resolve host" | The user isn't in group 3003. See the quirks table in [how-it-works.md](how-it-works.md). |
| `sudo` says "effective uid is not 0" | The chroot is mounted with `nosuid`. Use the current `arch.sh` (it clears that on the chroot mount) and reboot, or `arch.sh umount` then remount. |
| `makepkg`/`yay`: "fakeroot ... lack of SYSV IPC support" | Run `setup-aur.sh`; it builds fakeroot with TCP IPC. |
| Every package install ends with "command failed to execute correctly" from `systemd-sysusers` | Harmless on 4.x kernels; it can only create system users, so add them by hand if a package needs one. |
| `pacman` can't do anything about the sandbox | The 4.x kernel doesn't support it. Add `DisableSandbox` to the `[options]` section. |
| Phone reboots about two minutes after going headless | The watchdog feeder isn't running. Check `ls -l /proc/*/fd 2>/dev/null \| grep watchdog1` before turning the UI off, and make sure `ui.sh` is the version from this repo. |
| Phone reboots about a minute after every boot | A bootloop-protection module is reacting to the UI being stopped. See "Other traps" in [how-it-works.md](how-it-works.md). |
| Phone drops off Wi-Fi hours after going headless | Check `phoneserverd --status` and the log for `dhcp:` lines. If the router refuses the renewal, set a DHCP reservation in it. |
| The phone boots with the normal UI even though the `headless` flag exists | `headless.blocked` exists: two headless sessions in a row ended before 10 minutes. Read `phoneserverd.log` to see why, fix it, then delete `/data/adb/phoneserver/headless.blocked`. |
| `phoneserverd --status` says the status is stale or missing | The daemon isn't running. `ui.sh status` shows it; `ui.sh off` starts it. Check that the binary exists (`scripts/pc/check-sync.sh`). |
| Screen is dark and you want the normal phone back | `ssh phone-root /data/adb/phoneserver/ui.sh on`. Then `rm /data/adb/phoneserver/headless` so it doesn't happen again next boot. |
| The phone is somewhere new and you can't get in over Wi-Fi | While headless it can't join a new network. If the daemon is running, the Android UI comes back about five minutes after the router disappears; then add the Wi-Fi on the screen, or over a USB cable: `adb shell su -c '/data/adb/phoneserver/ui.sh on'`, then on Android 11 `adb shell cmd wifi connect-network <ssid> wpa2 <password>` (I haven't tried that command on this phone). Once you can SSH in, `ui.sh off` goes headless again without a reboot. |
| You can't reach the phone at all | Plug in the cable. `adb` over USB works even when Wi-Fi doesn't. Failing that, hold power and volume-down for about ten seconds to force a reboot. |
