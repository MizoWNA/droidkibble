# kibble, the terminal dashboard

`scripts/pc/kibble` is a small terminal dashboard you run on your computer. It shows what the phone is doing and lets you run commands on it, so you don't have to keep typing `ssh ... cat status.json`.

The window has three parts: a pixel-art dog in the middle, battery and memory on its left, network, DHCP and watchdog on its right, and a shell on the phone in the bottom third. It wants a terminal with 256 colors and about 100 columns by 30 rows. It shrinks gracefully: below about 96 columns the info stacks under the dog, on a terminal without 256 colors it shows a small ASCII dog, and under 60x18 it just asks for a bigger window.

## Using it

```sh
scripts/pc/kibble                       # finds the phone by itself (see "Finding the phone")
scripts/pc/kibble <ssh-host>            # or name an ssh host that logs in as root on the phone
scripts/pc/kibble <ssh-host> --once     # one snapshot as text, then exit
```

It's plain `ssh` underneath, and it needs Python 3 and nothing else.

### The bottom shell

Type on the bottom line and press Enter. It's a real, long-lived shell session, so `cd` and variables carry over from one command to the next, and the prompt shows the current directory. There are two shells, the Android root shell and the Arch chroot, and each keeps its own directory. **Tab** (or Ctrl-T) switches between them; the prompt and title show which one is active. `@arch <cmd>` runs a single command in the chroot without switching.

| key | does |
|---|---|
| Enter | run the line in the current shell |
| Tab / Ctrl-T | switch between the Android shell and the Arch chroot |
| Left, Right, Home, End, Delete | move around and edit the line (Ctrl-A/E, Ctrl-W, Ctrl-K, Ctrl-U also work) |
| Up, Down | command history |
| PgUp, PgDn | scroll the output |
| Ctrl-L, or `clear` | clear the output |
| Ctrl-C | stop a running command, or quit when nothing is running |
| :q, Ctrl-D | quit |
| :help | list these keys |

Programs that need a real terminal (`top`, `vim`) don't work here; use plain `ssh` for those. A command that runs for more than two minutes, or has an unbalanced quote and waits for the rest of it, is given up on. After that (and after Ctrl-C) the shell session is dropped and the next command starts a new one in the default directory. The dropped command may keep running on the phone, because the shell has no terminal to signal it through.

### Headless on and off

| key | command | does |
|---|---|---|
| F5 | `:headless` | go headless now, or bring the Android UI back, depending on the current state |
| F6 | `:boot` | turn "go headless at every boot" on or off (the `headless` flag file) |

Both ask `[y/N]` first, since going headless turns the screen off. Under the hood this runs `ui.sh off` or `ui.sh on` on the phone, detached, and prints the script's log about 25 seconds later. The dog shows the result: it falls asleep when Android comes back and wakes up fed when the daemon takes over.

## Finding the phone

The usual annoyance: you move to another network, the phone gets a different address, and every `ssh` alias with the old address stops working. kibble fixes that by recognising the phone by its **SSH host key** and not its address.

- **First run with no phone remembered.** kibble scans your computer's own subnet for SSH servers (port 22 only, it just reads their host keys and never tries to log in), lists them, and lets you pick one. Then it asks for a user, and how to log in: one of the keys in `~/.ssh`, your ssh agent, or a password (held in memory only, never written to disk). It tests the login, and if it works it remembers the phone in `~/.config/droidkibble/phone.json` (address, user, key path, and the host keys).
- **Every run after that.** It tries the last address, and if the phone isn't there, it scans and picks out the host whose key matches. The other machines on the network are never logged in to.
- **`:connect`** inside the dashboard opens the same screen to pick a different phone or log in again.
- If you already have working ssh aliases, `kibble learn <ssh-host>` remembers that phone without the wizard.

### Making every ssh command follow the phone

```sh
scripts/pc/kibble ssh-config --install          # for the hosts "phone-root" and "arch"
scripts/pc/kibble ssh-config --install phone-root arch mything    # or name your own aliases
```

This puts a small block at the top of `~/.ssh/config` (and saves a backup as `~/.ssh/config.bak-droidkibble` the first time). It adds only a `ProxyCommand` that runs `kibble connect`, which finds the phone and relays the connection. Everything else in your config for those hosts is untouched. After that `ssh arch`, `scp`, `phonessh` and the dashboard all work from any network with no editing. Host-key checking still applies, so a different machine at the remembered address can't pose as the phone. `kibble ssh-config --remove` undoes it, and `kibble ssh-config` alone prints the block without changing anything.

If the phone isn't on the same network at all (it's off, or you're somewhere else), there is nothing to find, and ssh says so.

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

Tested on Linux, on one phone, through a pseudo-terminal. **Checked on the real phone:** the dashboard at several window sizes; both shells keep their directory and switch with Tab; line editing with the arrow keys; `clear`; the F5 and F6 prompts, including answering no; going from headless to Android and back again through F5, and the dog changing to match; Ctrl-C dropping a stuck command; the first-run scan and login wizard against a real network (it listed the phone and one other SSH server, and logged in with a key); finding the phone again after a wrong address was cached, in about six seconds, from a different address than before; and `ssh`, `phonessh` and the dashboard following it through the ProxyCommand.

**Not checked:** password login (my phone only accepts keys), so that path is written but never run; the F6 toggle actually being confirmed with `y`; how it looks on real terminals other than the one I can't see; networks larger than a /22 (only a /22 around your address is scanned); IPv6; macOS (the status poll uses the GNU `timeout` command, which macOS doesn't ship); and phones with more than one Wi-Fi address.
