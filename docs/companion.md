# kibble, the terminal dashboard

`scripts/pc/kibble` is a small terminal dashboard you run on your computer. It shows what the phone is doing and lets you run commands on it, so you don't have to keep typing `ssh ... cat status.json`.

The window has three parts: a pixel-art dog in the middle, battery and memory on its left, network, DHCP and watchdog on its right, and a shell on the phone in the bottom third. It wants a terminal with 256 colors and about 100 columns by 30 rows. It shrinks gracefully: below about 96 columns the info stacks under the dog, on a terminal without 256 colors it shows a small ASCII dog, and under 60x18 it just asks for a bigger window.

## Using it

```sh
scripts/pc/kibble <ssh-host>            # the dashboard
scripts/pc/kibble <ssh-host> --once     # one snapshot as text, then exit
```

`<ssh-host>` is the same root-on-the-phone host from setup step 2 (`phone-root` in the examples). It's plain `ssh` underneath, so it uses your existing key login. It stores nothing and adds no server or port to the phone. Python 3 is all it needs, no extra packages.

Type on the bottom line and press Enter. It's a real, long-lived shell session, so `cd` and variables carry over from one command to the next. The prompt shows the current directory.

| you type | what runs |
|---|---|
| `uptime` | in the phone's Android root shell |
| `@arch pacman -Q \| wc -l` | in the Arch chroot, a second session that keeps its own directory |
| `:q` or Ctrl-D | quits |
| Ctrl-C | stops a running command (see below), or quits when nothing is running |

Up and Down step through history, PgUp and PgDn scroll the output, Ctrl-U clears the line. Programs that need a real terminal (`top`, `vim`) don't work here; use plain `ssh` for those.

Two limits to know about. A command with an unbalanced quote will wait for the rest of it, and a command that runs for more than two minutes is given up on. In both cases, and after Ctrl-C, that shell session is dropped and the next command starts a new one in the default directory. The dropped command may keep running on the phone, because the shell has no terminal to send it a signal.

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

Tested on Linux, against one phone, in headless mode, driven through a pseudo-terminal at several window sizes. Checked: it draws without crashing at 100x30, 70x20, 60x18, 200x40 and shows the "too small" message at 50x12; `cd` sticks between commands in both shells and the two keep separate directories; failures show their exit code; Ctrl-C drops a stuck command and the next one works; it quits cleanly. **Not checked:** how the colors and pixel dog look on real terminals other than the one I can't see (please tell me if it looks off), the sleeping, feed-me and lost dogs on a live phone, a phone that disappears while it's open, macOS (it uses the GNU `timeout` command for the status poll, which macOS doesn't ship).
