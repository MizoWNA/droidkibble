# Setup, step by step

This assumes a rooted phone with Magisk and a computer on the same Wi-Fi. Commands that start with `ssh phone-root` mean "SSH into the phone as root"; set that up as a host in `~/.ssh/config` (step 2) so the examples stay short.

## 0. Check the phone first (optional)

This is read-only and changes nothing:

```sh
adb push scripts/phone/diagnose.sh /data/local/tmp/
adb shell su -c 'sh /data/local/tmp/diagnose.sh'
```

(Once SSH works in step 2 you can also use `ssh phone-root sh -s < scripts/phone/diagnose.sh`.) It reports the model, Android and kernel versions, and which processes hold the hardware watchdog devices. If you plan to try headless mode on something other than a Galaxy A30, this shows whether it has the same watchdog setup. The output has no IP addresses or serial numbers, so it's fine to paste into an issue.

## 1. Get adb working

1. On the phone: Settings, About phone, tap "Build number" seven times. Then Developer options, turn on USB debugging.
2. Plug in the phone and run `adb devices`. Approve the prompt on the phone (tick "always allow").
   - If it says `unauthorized`: unplug, replug, and look for the prompt. If none appears, use "Revoke USB debugging authorizations" in Developer options and try again. Make sure the USB mode isn't "charging only".
3. Switch to Wi-Fi so you don't need the cable:
   ```sh
   adb tcpip 5555
   adb connect <phone-ip>:5555
   adb shell su -c 'setprop persist.adb.tcp.port 5555'   # keeps it after reboot
   ```
   Find the phone's IP in its Wi-Fi settings, or with `adb shell ip addr show wlan0`.

## 2. Root SSH at boot

Install a Magisk module that runs an SSH server as root at boot. "SSH for Magisk" does this and listens on port 22. Add your public key to `/data/ssh/root/.ssh/authorized_keys` on the phone, and confirm the module's `sshd_config` has `PasswordAuthentication no`.

Then add this to `~/.ssh/config` on your computer:

```
Host phone-root
    HostName <phone-ip>
    Port 22
    User root
    IdentityFile ~/.ssh/<your-key>
```

Check it with `ssh phone-root id`. You should see `uid=0(root)`.

Give the phone a fixed address (a DHCP reservation in your router) while you're at it. It matters later.

## 3. Put Arch on the phone

```sh
scripts/pc/get-rootfs.sh
scripts/pc/push-rootfs.sh <phone-ip>:5555
```

The download is around 830 MB and the unpacked system is about 2 GB. The first script keeps the download in `~/.cache/droidkibble` so you only fetch it once.

## 4. Give the chroot an SSH login

Install the scripts first (this doesn't change the Android UI or start anything new at boot until you reboot):

```sh
scripts/pc/install.sh phone-root
```

Copy your public key into the chroot, then run the setup script inside it:

```sh
scp ~/.ssh/<your-key>.pub phone-root:/data/arch/root/mykey.pub
scp scripts/phone/chroot-ssh.sh phone-root:/data/arch/root/chroot-ssh.sh
ssh phone-root '/data/adb/phoneserver/arch.sh run "sh /root/chroot-ssh.sh /root/mykey.pub"'
```

Add a second host to `~/.ssh/config`:

```
Host arch
    HostName <phone-ip>
    Port 2222
    User root
    IdentityFile ~/.ssh/<your-key>
```

`ssh arch` should now drop you into Arch Linux ARM.

## 5. Check the boot hook

Reboot the phone (`ssh phone-root reboot`), leave it locked, and after a minute or two run `ssh arch uname -a`. If that works, the boot hook is doing its job: it mounts the chroot and starts its SSH server without anyone touching the phone.

## 6. Headless mode (optional)

Only do this once step 5 works. First build the supervisor that runs while the UI is off (it needs `gcc` and `make` in the chroot, which `setup-aur.sh` in step 8 installs; or `pacman -S gcc make`):

```sh
scripts/pc/build-daemon.sh phone-root
```

Then see the README for the commands, [daemon.md](daemon.md) for what the supervisor does, and [how-it-works.md](how-it-works.md) for why the phone needs it.

A few things to know before you turn it on:

- The screen goes dark and stays that way.
- Keep USB debugging on. If something goes badly wrong, `adb` over a cable is your way back in.
- If the phone gets stuck, holding power and volume-down for about ten seconds forces a reboot. With the flag file removed it will come up with the normal Android UI.

## 7. Make pacman work properly

Out of the box, `pacman -Syu` on the phone tends to time out: the default mirror picks a server that can be far away, and pacman drops any download that stalls for ten seconds. Run this inside the chroot:

```sh
scp scripts/phone/setup-pacman.sh arch:/root/
ssh arch 'sh /root/setup-pacman.sh --drop-kernel'
```

It measures a list of mirrors and keeps the fastest five, turns off pacman's download timeout, and enables the small `[aur]` repo. With `--drop-kernel` it also removes the kernel and PC firmware packages that came in the rootfs. They do nothing in a chroot, and dropping them turns a ~400 MB first upgrade into ~60 MB and frees about 1.3 GB. Skip that flag if you'd rather keep them.

Then run `pacman -Syu` as usual. Run it inside `nohup` or `screen` if your SSH connection is flaky, since a dropped connection in the middle of a package transaction is the one thing worth avoiding.

## 8. The AUR (optional)

`[aur]` from step 7 is only a dozen prebuilt packages. For the real AUR you need an AUR helper, and that takes some Android-specific fixes, which the script applies:

```sh
scp scripts/phone/setup-aur.sh arch:/root/
ssh arch 'sh /root/setup-aur.sh'
```

It downloads about 50 MB (mostly a compiler), sets up an unprivileged build user with sudo, builds a working `fakeroot`, and installs `yay`. The reasons for each step are in the table in [how-it-works.md](how-it-works.md#android-quirks-that-show-up-inside-the-chroot). Afterwards, as the build user:

```sh
su alarm -c 'yay -S <package>'
```

Two notes. Many AUR packages only list `x86_64` in their PKGBUILD; if a build refuses, `yay --mflags --ignorearch` tries anyway (it may or may not compile). And building big packages on a phone is slow.

## Other chroot notes

- `/etc/resolv.conf` is rewritten by `arch.sh` every time it mounts the chroot.
- Non-root users need to be in a group with gid 3003 to reach the network. See the table in [how-it-works.md](how-it-works.md#android-quirks-that-show-up-inside-the-chroot).

## Optional: the dashboard

`scripts/pc/kibble` is a terminal dashboard for the phone, with a shell and a headless switch, and it can find the phone again after you change networks. It needs nothing beyond Python 3. If you move the phone between networks, run `scripts/pc/kibble ssh-config --install` once so plain `ssh` follows it. See [companion.md](companion.md).

## Optional: a matching shell

If you use zsh with oh-my-zsh, [extras/phonessh](../extras/README.md) gives you a shell on the phone that looks like your own terminal, in shifted colors so you can tell them apart.

## Keeping the phone and this repo in sync

The scripts on the phone are copies of the ones in `scripts/phone/`. After you change one, deploy it with `scripts/pc/install.sh <ssh-host>`, and check nothing has drifted with:

```sh
scripts/pc/check-sync.sh <ssh-host>
```

It compares every deployed script with the repo and lists anything that is different or missing.

