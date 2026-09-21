# kibble, the terminal dashboard

`scripts/pc/kibble` is a small terminal dashboard you run on your computer. It shows what the phone is doing and lets you run commands on it, so you don't have to keep typing `ssh ... cat status.json`.

```
   __
 o(__)\____
  /     __)~        mode      headless  (headless flag set)
 (_|_|--|_|         battery   [###########---] 80%  Discharging  27.8C
  good boy          memory    [#####---------] 1371/3746 MB
                    network   192.168.3.11 via 192.168.3.1  router ok
                    watchdog  /dev/watchdog1 fed 0s ago
```

(That's a mock-up of the layout; the real thing updates every few seconds and the dog wags.)

## Using it

```sh
scripts/pc/kibble <ssh-host>            # the dashboard
scripts/pc/kibble <ssh-host> --once     # one snapshot as text, then exit
```

`<ssh-host>` is the same root-on-the-phone host from setup step 2 (`phone-root` in the examples). It's plain `ssh` underneath, so it uses your existing key login. It stores nothing and adds no server or port to the phone. Python 3 is all it needs, no extra packages.

Type at the bottom line and press Enter to run a command on the phone. Output shows above it.

| you type | what runs |
|---|---|
| `uptime` | in the phone's Android root shell |
| `@arch pacman -Q \| wc -l` | inside the Arch chroot |
| `:q`, Ctrl-C or Ctrl-D | quits |

Up and Down step through history, PgUp and PgDn scroll the output, Ctrl-U clears the line. Programs that need a real terminal (`top`, `vim`) don't work here; use plain `ssh` for those. Commands time out after two minutes.

## The dog

| dog | meaning |
|---|---|
| good boy (wagging) | headless, and the daemon fed the watchdog in the last 15 seconds |
| feed me! | headless, but the watchdog isn't being fed, or the daemon isn't running. The phone will reset soon. Run `ui.sh on`. |
| sleeping | the Android UI is on, so Android feeds the watchdog itself and the daemon isn't needed |
| where is it | the phone can't be reached over ssh |

## Where the data comes from

Every few seconds it makes one ssh call that reads `status.json` (written by `phoneserverd`, see [daemon.md](daemon.md)). If the daemon isn't running, for example with the UI on, it falls back to `phoneserverd --once`, which shows battery, memory and network but not DHCP or watchdog details.

## Status

Tested on Linux, against one phone, in headless mode. Checked: the dashboard draws, the numbers match `status.json`, commands run in the Android shell and in the chroot, failures show the exit code, and it quits cleanly. **Not yet checked:** the sleeping, feed-me and where-is-it dogs (I've only seen good boy), a phone that goes away while it's open, macOS, and very small terminal windows. It expects the GNU `timeout` command, which macOS doesn't ship.
