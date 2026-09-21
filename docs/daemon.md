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

`status.json` is rewritten every 5 seconds. It has battery percentage, temperature and charging state, memory, the Wi-Fi address and gateway, whether the router answers, the DHCP lease (when it was last renewed, how long it lasts, when the next renewal is), whether both SSH servers are listening, and how long ago the watchdog was fed. `--status` warns if the file looks stale, which usually means the daemon isn't running.

Options (all optional; the boot scripts use the defaults): `--dir`, `--iface`, `--no-watchdog`, `--no-dhcp`, `--no-restore`, `--foreground`, `--once`, `--status`.

## How the DHCP renewal works

The daemon has its own tiny DHCP client. BusyBox's `udhcpc` can't be used: it needs an ioctl (`SIOCGIFHWADDR`) that Android's SELinux policy denies, even for root.

Once shortly after starting, and then at the renewal time, it broadcasts a DHCPREQUEST for the address the phone already holds. The router answers with an ACK and a fresh lease, and the daemon schedules the next renewal for half the lease (capped at 6 hours).

It **never changes the interface**. Android configured the address long ago, and all this does is keep the router's record fresh. If the router answers NAK three times in a row, the daemon restores the Android UI so Android can get a lease the normal way. If the router simply doesn't answer, that is logged and retried every two minutes; the connection check below is what decides whether it matters.

I've only tried this against my own router, which accepted the request and handed back a 24-hour lease. Other routers might want a client ID or act differently. If yours refuses you'll see `dhcp: ... refused our address` in the log and the fallback above kicks in. A DHCP reservation in the router is still the most reliable setup.

## What happens when something goes wrong

- **The daemon crashes.** `ui.sh` runs it inside a small loop that restarts it within a second, well inside the watchdog window.
- **Both die.** The watchdog resets the phone in about a minute. After a reset it boots normally and headless mode starts again.
- **The daemon is unstable.** A headless session only counts as healthy after 10 minutes. If two sessions in a row end before that, headless mode blocks itself (`/data/adb/phoneserver/headless.blocked`) and the phone boots with the normal Android UI, so a bad build can't leave you with a screenless phone stuck in a reset loop. Delete that file to try again.
- **The router goes away.** After 5 minutes the UI comes back on its own. That also happens if the DHCP server keeps refusing the address.
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
| `headless.pending` | exists from headless start until the session has been stable for 10 minutes |
| `headless.blocked` | headless mode has been disabled after two unstable sessions |

## Drawing on the screen

Not implemented. The daemon builds the same few status lines it logs (battery, memory, network, services) and hands them to an empty `display_update()`. Putting them on the panel needs Samsung's ION + display-driver path, described in [how-it-works.md](how-it-works.md#drawing-on-the-screen-investigation-not-working-yet).
