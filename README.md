# droidkibble

*Feed the watchdog, free the RAM.*

Scripts for turning an old rooted Android phone into a small always-on Linux server. It boots, joins your Wi-Fi, and you can SSH into a proper Arch Linux ARM system running on it. If you want, it can also shut the Android UI off to free up RAM.

I built it to host a personal web app on a Galaxy A30 that was otherwise in a drawer. The phone had 4 GB of RAM and Android was eating more than half of it, so I wanted Android gone. Stopping the UI is one command. Keeping the phone alive afterwards took most of an evening, because it reset itself about 100 seconds later, every time. The reason is a second hardware watchdog that Samsung's system_server feeds (hence the name: the watchdog needs its kibble), and it's written up in [docs/how-it-works.md](docs/how-it-works.md) in case you hit the same thing.

|  | with Android UI | headless |
|---|---|---|
| RAM in use (Galaxy A30, 3.7 GB total) | about 2.2 GB | about 0.85 GB |

## Does it work on my phone?

I've only tried it on one phone: a Samsung Galaxy A30 (Exynos 7904, Android 11, kernel 4.4.177). So I can't promise anything about yours.

What you need in general:

- Root with Magisk. None of this works without it.
- A 64-bit ARM phone and Android 9 or newer (I've only run 11).
- About 2 GB of free storage.
- A computer with `adb` and `ssh`, on the same Wi-Fi.

The chroot and SSH parts shouldn't care much about the model. Headless mode is the risky part: it leans on a Samsung-specific watchdog, and other phones might not need the fix, or might need a different one. To find out what yours does, run the read-only check first:

```sh
ssh <phone-root> sh -s < scripts/phone/diagnose.sh
```

It prints the model, Android and kernel versions, which watchdog devices exist and which processes hold them. If you try this on another phone, I'd like to hear how it went, whether it worked or not. Open an issue and paste that output.

## Setting it up

The full walkthrough with explanations is [docs/setup.md](docs/setup.md). The short version:

```sh
# download the Arch Linux ARM rootfs once (about 830 MB) and keep a copy
scripts/pc/get-rootfs.sh

# copy it to the phone and unpack it into /data/arch
scripts/pc/push-rootfs.sh <adb-target>          # a USB serial, or 192.168.x.x:5555

# install the scripts and the boot hook (the Android UI stays on)
scripts/pc/install.sh <ssh-host-for-phone-root>

# then give the chroot its own SSH login on port 2222 (setup.md, step 4)
```

After a reboot, `ssh -p 2222 root@<phone-ip>` gets you into Arch.

Two optional scripts, run inside the chroot (setup.md steps 7 and 8):

- `setup-pacman.sh` picks fast mirrors so `pacman -Syu` stops timing out. With `--drop-kernel` it also removes the kernel and PC firmware, which do nothing in a chroot. That takes the first upgrade from about 400 MB to about 60 MB.
- `setup-aur.sh` gets `yay` and the real AUR working, which needs a few Android-specific workarounds.

## Headless mode

Only try this once the steps above work.

```sh
scripts/pc/build-daemon.sh <ssh-host>                    # build the supervisor on the phone
ssh <phone-root> touch /data/adb/phoneserver/headless    # switch it on for the next boot
```

On the next boot the phone waits until it's been up for 150 seconds and is on Wi-Fi, then stops the Android UI. The screen stays dark from then on.

Something has to replace the parts of Android that go away, so a small C program, `phoneserverd`, runs on the phone. It feeds the watchdog, renews the Wi-Fi DHCP lease, pings the router, and writes a status file and a log. If the router is unreachable for about five minutes it brings the UI back so Android can sort the network out itself. If two headless sessions in a row end early, headless mode turns itself off and the phone boots normally, so a bad build can't leave you with a screenless phone in a reset loop. More in [docs/daemon.md](docs/daemon.md).

To see how it's doing: `ssh <phone-root> /data/adb/phoneserver/phoneserverd --status`, or run `scripts/pc/kibble` for a terminal dashboard: a pixel dog that shows whether the watchdog is being fed, battery and network info, a shell on the phone and a headless on/off switch. It also finds the phone again when you change networks, and `kibble ssh-config --install` makes plain `ssh` do the same ([docs/companion.md](docs/companion.md)).

## Getting the normal phone back

```sh
ssh <phone-root> rm /data/adb/phoneserver/headless      # don't go headless at boot
ssh <phone-root> /data/adb/phoneserver/ui.sh on         # bring the UI back right now
```

Nothing here touches the Android system partition. Deleting `/data/adb/service.d/phoneserver.sh` removes the boot hook completely.

## What doesn't work yet

- **Long runs.** The longest continuous headless run so far is well under an hour, plus a couple of unattended reboots. I haven't run it for days.
- **Moving to another network** while headless works only through the fallback: after about five minutes without the router the Android UI comes back and you add the new Wi-Fi on the screen. I've watched it work once. There is no way to join a new Wi-Fi while headless.
- **DHCP renewal** works against my router. Others may behave differently, and a DHCP reservation is still the safest setup.
- **The screen stays dark.** There's a working status screen (a letterpress-style ticket, see [display/](display/)) that draws through SurfaceFlinger, but you start it by hand for now and the daemon doesn't manage it yet. My notes on how I got there are in [how-it-works.md](docs/how-it-works.md#drawing-on-the-screen).
- **No systemd** in the chroot. The 4.x kernel is too old, so services are plain scripts.
- **No `ping`** inside the chroot (no raw sockets). Everything else, `curl` included, works.
- **Mirrors time out at random** on a phone connection. `setup-pacman.sh` copes, but expect the odd ten-second pause.
- **Setup scripts.** `install.sh` has been run for real. `get-rootfs.sh` and `push-rootfs.sh` are the commands I ran by hand, put into scripts, and haven't been run as scripts yet. I haven't done the whole thing from scratch on a fresh phone, so expect to fix small things. [CHANGELOG.md](CHANGELOG.md) lists what's verified and what isn't.

If your phone is supported by postmarketOS or Droidian, use one of those. They're a better answer than this. [docs/alternatives.md](docs/alternatives.md) covers other options.

## What's in the repo

```
scripts/pc/       run on your computer: download, push, install, check for drift, kibble (dashboard)
scripts/phone/    run on the phone: chroot manager, boot hook, headless on/off, pacman and AUR setup, diagnose.sh
daemon/           phoneserverd, the C source
docs/             setup, how it works, the daemon, the dashboard, troubleshooting, alternatives
extras/           optional extras, like a matching shell for the phone
CHANGELOG.md      changes, and what's been verified on hardware
```

## License

MIT, see [LICENSE](LICENSE). Use it however you like; just keep the copyright notice.
