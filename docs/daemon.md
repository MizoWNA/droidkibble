# phoneserverd

A small program that runs on the phone while the Android UI is off (headless mode). It takes over the jobs Android's framework normally does and that the phone can't do without.

It's one static C binary (`daemon/phoneserverd.c`). It runs as root on the Android side, outside the chroot, and uses about a megabyte of RAM.

## What it does

| job | why it's needed |
|---|---|
| Feeds `/dev/watchdog1` every 5 seconds | Samsung's `system_server` feeds this second hardware watchdog. When the UI is off nobody does, and the phone resets about 100 seconds later. See [how-it-works.md](how-it-works.md). |
| Renews the Wi-Fi DHCP lease | Android's framework normally renews it. Without that, the router eventually hands your address to another device. |
| Pings the router every 10 seconds | If the router stays unreachable for 5 minutes, it brings the Android UI back so Android can reconnect on its own. |
| Writes `status.json` and a log | So you can see what's going on over SSH. |
| Runs the on-screen display and reads the power button | With the UI off the screen is dark. Pressing the power button lights it and shows the status ticket ([display/README.md](../display/README.md)); pressing again, or waiting for the auto-off timer, turns it off. |

## Installing it

```sh
scripts/pc/build-daemon.sh <ssh-host>
```

That copies the source into the chroot, compiles it there (the chroot needs `gcc` and `make`; `setup-aur.sh` installs them), and installs the binary as `/data/adb/phoneserver/phoneserverd`. `ui.sh off` starts it automatically. If the binary isn't installed, `ui.sh` falls back to a plain BusyBox watchdog feeder, which keeps the phone alive but does nothing else.

`scripts/pc/check-sync.sh` tells you whether the binary on the phone was built from the current source.

## Looking at it

```sh
ssh <phone-root> /data/adb/phoneserver/phoneserverd --status   # the live status file
ssh <phone-root> /data/adb/phoneserver/phoneserverd --once     # read the phone's state once, as plain lines
ssh <phone-root> tail /data/adb/phoneserver/phoneserverd.log   # times are UTC
ssh <phone-root> /data/adb/phoneserver/ui.sh status
```

`scripts/pc/kibble` shows the same information as a dashboard ([companion.md](companion.md)).

`status.json` is rewritten every 5 seconds. It has battery percentage, temperature and charging state, memory, the Wi-Fi address and gateway, whether the router answers, the DHCP lease (when it was last renewed, how long it lasts, when the next renewal is), whether both SSH servers are listening, and how long ago the watchdog was fed. `--status` warns if the file looks stale, which usually means the daemon isn't running.

Options (all optional; the boot scripts use the defaults): `--dir`, `--iface`, `--no-watchdog`, `--no-dhcp`, `--no-restore`, `--no-display`, `--display-theme`, `--display-brightness`, `--display-timeout`, `--backlight`, `--foreground`, `--once`, `--status`. The display options are normally set through `display.conf` (below).

## How the DHCP renewal works

The daemon has its own tiny DHCP client. BusyBox's `udhcpc` can't be used: it needs an ioctl (`SIOCGIFHWADDR`) that Android's SELinux policy denies, even for root.

Once shortly after starting, and then at the renewal time, it broadcasts a DHCPREQUEST for the address the phone already holds. The router answers with an ACK and a fresh lease, and the daemon schedules the next renewal for half the lease (capped at 6 hours).

It **never changes the interface**. Android configured the address long ago, and all this does is keep the router's record fresh. If the router answers NAK three times in a row, the daemon restores the Android UI so Android can get a lease the normal way. If the router simply doesn't answer, that is logged and retried every two minutes; the connection check below is what decides whether it matters.

I've only tried this against my own router, which accepted the request and handed back a 24-hour lease. Other routers might want a client ID or act differently. If yours refuses you'll see `dhcp: ... refused our address` in the log and the fallback above kicks in. A DHCP reservation in the router is still the most reliable setup.

## What happens when something goes wrong

- **The daemon crashes.** `ui.sh` runs it inside a small loop that restarts it within a second, well inside the watchdog window.
- **Both die.** The watchdog resets the phone in about a minute. After a reset it boots normally and headless mode starts again.
- **The daemon is unstable.** A headless session only counts as healthy after 10 minutes. If two sessions in a row end before that, headless mode blocks itself (`/data/adb/phoneserver/headless.blocked`) and the phone boots with the normal Android UI, so a bad build can't leave you with a screenless phone stuck in a reset loop. Delete that file to try again.
- **The router goes away.** After 5 minutes the UI comes back on its own. That also happens if the DHCP server keeps refusing the address. I've seen this work for real: I carried the phone out of range of my router while headless, and about five minutes later the UI came back and Android joined a different network. Android only auto-joins networks it already has saved, so at a new place you add the network on the screen (or with adb, see [troubleshooting.md](troubleshooting.md)).
- **You run `ui.sh on` yourself.** That's a deliberate stop; it doesn't count against the stability check.

## Files it uses (in `/data/adb/phoneserver/`)

| file | what |
|---|---|
| `phoneserverd` | the binary |
| `phoneserverd.src.sha256` | hash of the source it was built from, for `check-sync.sh` |
| `status.json` | live status |
| `phoneserverd.log` (and `.1`) | log, rotated at 256 KB |
| `phoneserverd.lock` | held while running; contains the pid; prevents a second copy |
| `daemon.stop` | tells the respawn loop to stop restarting it |
| `display/` | `status.jar` and `run.sh`, the on-screen display (installed by `install.sh`) |
| `display.conf` | optional display settings, in shell syntax (see below) |
| `display.log` | what the display program printed, for the current run |
| `headless.pending` | exists from headless start until the session has been stable for 10 minutes |
| `headless.blocked` | headless mode has been disabled after two unstable sessions |

## The screen and the power button

With the Android UI off nothing reads the power button, so the daemon does. It looks through `/dev/input` for every device that can send a power key (on the Galaxy A30 that's `event14`, `gpio_keys`, which also carries the volume keys) and logs which one it found.

- **The screen starts dark** when headless mode begins.
- **One press** raises the backlight and starts the display program (`display/run.sh`, which draws through SurfaceFlinger). **Another press** kills the program and puts the backlight back to 0.
- **It turns itself off** after 10 minutes (`DISPLAY_TIMEOUT`), to spare the OLED and the battery. Set it to 0 to keep the screen on until you press the button.
- **If the display program dies**, the daemon notices, logs it, and turns the backlight off.
- **If the display isn't installed** (no `display/status.jar`), the button does nothing and the log says so. `status.json` has a `display` block with `available`, `on`, `auto_off_s` and `theme`.
- **Turning the Android UI back on** (`ui.sh on`, or the automatic restore) shuts the display down first, because its layer would otherwise sit on top of Android.

To change the settings, create `/data/adb/phoneserver/display.conf`:

```sh
DISPLAY_THEME=night        # paper (default) or night
DISPLAY_BRIGHTNESS=120     # backlight level while it's on; the panel's maximum is 365
DISPLAY_TIMEOUT=600        # seconds until it turns itself off; 0 = stay on
```

It's read when the daemon starts (`ui.sh off`, a boot, or `ui.sh restart`), not while it's running. After editing it, run `ssh <phone-root> /data/adb/phoneserver/ui.sh restart` to apply the change without leaving headless mode. Deleting the file has no effect until the daemon restarts either.
