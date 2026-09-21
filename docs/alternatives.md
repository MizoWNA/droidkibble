# Alternatives and prior art

A quick survey of what already exists for turning an old phone into a server, and where this project overlaps or differs. It was done with web searches and by reading each project's README (September 2026), so treat it as a starting point, not a full review. If something here is wrong or out of date, please say so.

## What's out there

| approach | examples | notes |
|---|---|---|
| Linux in a chroot or proot on Android | [Linux Deploy](https://github.com/meefik/linuxdeploy), the [lhroot](https://github.com/Magisk-Modules-Alt-Repo/lhroot) and [chroot-distro](https://github.com/Magisk-Modules-Alt-Repo/chroot-distro) Magisk modules, Termux `proot-distro`, [android-chroot](https://github.com/ThomasBaruzier/android-chroot) (Arch Linux ARM) | Mature and widely used for getting a distro running. They mount the chroot and give you a shell or SSH. They don't try to stop the Android framework and keep the phone alive without it. android-chroot documents the `fakeroot` TCP build and manual DNS that this project also needed. |
| "Phone as a server" projects that keep Android running | [project-zeus](https://github.com/abxn4r/project-zeus), [android-home-server](https://github.com/Egebrktn/android-home-server), [android-home-nas](https://github.com/phuche2004/android-home-nas) | Small projects (a handful of commits and stars when checked). They leave Android running with the screen off and a wakelock held, add boot scripts, a supervisor or dashboard, and a tunnel. That is simpler and safer, but it costs the RAM the framework uses (about 1.5 GB on the test phone). project-zeus was tested on a Snapdragon phone, and its README doesn't mention hardware watchdogs. |
| A real Linux instead of Android | [postmarketOS](https://postmarketos.org/), [Droidian](https://droidian.org/), [Halium](https://halium.org/) | The best result when your phone is supported: no Android at all. The supported list is limited, and the Galaxy A30 used here isn't on it. |

## Where this project differs

- **It stops the Android framework and keeps the phone stable.** Most others don't try. On the test phone that needs feeding a second hardware watchdog (`/dev/watchdog1`) that Samsung's `system_server` normally feeds. I didn't find that documented elsewhere, though absence from search results isn't proof. The general method (list which processes hold `/dev/watchdog*` and reproduce what they do) should apply to other phones, but the specific device is Samsung- and kernel-dependent.
- **A supervisor for the headless state.** `phoneserverd` feeds the watchdog, renews the DHCP lease with its own client (BusyBox `udhcpc` is blocked by Android's SELinux policy), checks the router, restores the UI if the network stays down, and has a boot-loop breaker.
- **The write-up.** A consolidated list of the Android-specific traps hit along the way (`nosuid` on `/data`, the network group for non-root users, `fakeroot` without System V IPC, mirror timeouts) and what didn't work.

## Where it doesn't

- It has only been tested on **one phone and one router**, and not yet over days.
- Much of the chroot setup (suid remount, DNS, `fakeroot` with TCP IPC) is known ground; the value there is having it in one place, not novelty.
- It needs a rooted phone with Magisk and a separate SSH module. There is no app or one-command installer.
- The on-screen display isn't built.
- If your phone is supported by postmarketOS or Droidian, use that instead.
