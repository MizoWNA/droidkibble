# kibble, the terminal dashboard

`scripts/pc/kibble` is a dashboard for your terminal. It shows what the phone is up to, gives you a shell on it, and can switch headless mode on and off. I wrote it because I was tired of typing `ssh phone-root cat status.json` every few minutes.

The window has a pixel dog in the middle, battery and memory on its left, network, DHCP and watchdog details on its right, and a shell on the phone in the bottom third. You want a terminal with 256 colors and roughly 100 by 30 characters. Smaller works too: under about 96 columns the info stacks below the dog, without 256 colors you get a small ASCII dog, and under 60 by 18 it just asks for a bigger window.

Python 3 is all it needs, and it talks to the phone with your normal `ssh`.

```sh
scripts/pc/kibble                      # finds the phone by itself (see "Finding the phone")
scripts/pc/kibble <ssh-host>           # or name an ssh host that logs in as root on the phone
scripts/pc/kibble <ssh-host> --once    # print one snapshot as text and exit
```

## The dog

| dog | what it means |
|---|---|
| wagging, "good boy" | headless, and the daemon fed the watchdog in the last 15 seconds |
| worried, "feed me!" | headless, but the watchdog isn't being fed or the daemon isn't running. The phone will reset soon. Run `ui.sh on`. |
| asleep | the Android UI is on, so Android feeds the watchdog itself |
| question marks | the phone can't be reached over ssh |

The "fed N s ago" number counts up between updates and drops back to zero each time the daemon feeds (every 5 seconds).

## The shell at the bottom

Type on the bottom line and press Enter. It's a real, long-lived session, so `cd` and variables carry over from one command to the next, and the prompt shows where you are. There are two shells, the Android root shell and the Arch chroot. Each keeps its own directory, and **Tab** switches between them. The title of the pane says which one you're in. `@arch <cmd>` runs a single command in the chroot without switching.

Anything a command prints without a newline at the end shows up straight away, and while a command is running, what you type goes to it. So `pacman -S something` shows its `Proceed with installation? [Y/n]` prompt, and typing `y` and Enter answers it.

| key | does |
|---|---|
| Enter | run the line (or, while something runs, send it to that command) |
| Tab, Ctrl-T | switch between the Android shell and the Arch chroot |
| Left, Right, Home, End, Delete | edit the line. Ctrl-A, E, W, K and U work too. |
| Up, Down | history |
| PgUp, PgDn | scroll the output |
| Ctrl-L, or `clear` | clear the output |
| Ctrl-C | ask the running command to stop, or quit when nothing is running |
| `:q`, Ctrl-D | quit |
| `:help` | list the keys |

**Ctrl-C** works like it does in a normal terminal: it sends the command an interrupt, and after a few seconds a terminate signal if that didn't do it. The shell and its directory survive. This matters for pacman, which only releases its lock (`/var/lib/pacman/db.lck`) when it's interrupted properly. If a command ignores both signals, kibble drops that shell and tells you; the command may still be running on the phone, so check with `ps`.

Programs that need a real terminal (`top`, `vim`, `less`) don't work in here, since there's no terminal behind the shell. Use plain `ssh` for those. Commands that wait for end-of-file on their input (`cat` with no arguments) will wait until you press Ctrl-C.

## Headless on and off

| key | command | what it does |
|---|---|---|
| F5 | `:headless` | goes headless now, or brings Android back, depending on the current state |
| F6 | `:boot` | turns "go headless at every boot" on or off (the `headless` flag file) |

Both ask `[y/N]` first, since going headless turns the screen off. Underneath it runs `ui.sh off` or `ui.sh on` on the phone, detached, and prints that script's log about 25 seconds later. The dog shows the result: asleep when Android is back, wagging when the daemon has taken over.

## Finding the phone

If you switch networks, the phone gets a new address, and any ssh host that points at the old one stops working. kibble handles that by recognising the phone by its **SSH host key** instead of its address.

- **First run.** With no phone remembered, kibble scans your computer's own subnet for SSH servers and lists them. It only checks port 22 and reads host keys, and never tries to log in to anything. You pick one, give a user name, and choose how to log in: a key from `~/.ssh`, your ssh agent, or a password (kept in memory, never written to disk). If the login works, the phone is remembered in `~/.config/droidkibble/phone.json`: its address, the user, the key file and its host keys.
- **After that.** kibble tries the last address, and if the phone isn't there it scans and picks out the host whose key matches.
- **`:connect`** in the dashboard opens the same screen again, to log in differently or pick another phone.
- If your ssh hosts already work, `kibble learn <ssh-host>` remembers the phone without the wizard.

### Making plain ssh follow the phone

```sh
scripts/pc/kibble ssh-config --install                      # for the hosts "phone-root" and "arch"
scripts/pc/kibble ssh-config --install phone-root arch foo  # or name your own
```

That adds a small block at the top of `~/.ssh/config` (and saves a backup as `~/.ssh/config.bak-droidkibble` the first time). All it sets is a `ProxyCommand` that runs `kibble connect`, which finds the phone and passes the connection through. The rest of your config for those hosts stays as it is. After that `ssh`, `scp`, `phonessh` and the dashboard all work from any network without editing anything. Host key checking still applies, so some other machine sitting at the remembered address can't pass as the phone.

`kibble ssh-config` alone prints the block without changing anything, and `--remove` takes it out again. If the phone isn't on your network at all, there's nothing to find and ssh tells you so.

## Where the data comes from

Every few seconds one ssh call reads `status.json`, which `phoneserverd` writes (see [daemon.md](daemon.md)). When the daemon isn't running, for example with the UI on, it falls back to `phoneserverd --once`. That still gives battery, memory and network, but not the DHCP or watchdog details.

## What's been tested

Everything below was run on Linux against my Galaxy A30, mostly by driving the dashboard through a pseudo-terminal, since I can't look at the screen myself.

- The dashboard draws at several window sizes without crashing.
- Both shells keep their directory and switch with Tab. Line editing, `clear`, and history work.
- Prompts without a trailing newline show up, and answers reach the running command. I checked this with `read` in the Android shell and with `pacman -S neovim` in the chroot, answering `n` so nothing was installed.
- Ctrl-C stops a command, keeps the shell and its directory, and lets pacman release its lock.
- F5 works in both directions and the dog changes to match. F6 shows its prompt and cancels.
- The scan and login screen against a real network: it listed the phone and one other SSH server, and a key login worked.
- Finding the phone again after I cached a wrong address, twice, about six seconds each time. `ssh`, `phonessh` and the dashboard all followed it through the ProxyCommand.

What hasn't been tested:

- Password login. My phone only takes keys, so that path is written but has never run.
- Confirming F6 with `y`.
- What it looks like on terminals other than mine. If the colors or the dog look wrong, tell me.
- The "feed me" and "question marks" dogs on a live phone.
- Networks bigger than a /22 (only the /22 around your address is scanned), IPv6, and phones with several Wi-Fi addresses.
- macOS. The status poll uses the GNU `timeout` command, which macOS doesn't ship.
