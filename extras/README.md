# Extras

Optional things that aren't needed to run the phone as a server.

## Moving between networks

`phonessh` uses the ssh hosts `arch` and `phone-root`, which normally have a fixed address. If the phone changes network, run `scripts/pc/kibble ssh-config --install` once and both hosts follow it automatically. See [docs/companion.md](../docs/companion.md).

## phonessh

A small script that opens a shell on the phone that looks like your own terminal, recolored so you can tell the two apart at a glance. It assumes you use zsh with oh-my-zsh on your computer.

```sh
phonessh            # open zsh on the phone (the Arch chroot)
phonessh <cmd>      # run one command there
phonessh --root     # the Android root shell instead
phonessh --sync     # (re)install zsh and copy your oh-my-zsh + generated theme to the phone
```

What `--sync` does:

- installs `zsh` in the chroot if it isn't there (one small package)
- copies your `~/.oh-my-zsh` to the phone over your LAN, without `.git` (about 13 MB, no internet use)
- builds a recolored copy of your theme: the green or red `user@host` becomes magenta, blue becomes cyan, and the hostname reads `phone`
- writes a small `~/.zshrc` and a greeting that shows a `PHONE` banner with uptime, RAM, battery and temperature. The banner is rendered on your computer with `figlet`, so the phone needs nothing extra

It relies on two SSH hosts in `~/.ssh/config`: `arch` (the chroot, port 2222) and `phone-root` (Android, port 22). Change them with environment variables: `PHONESSH_HOST`, `PHONESSH_ROOT_HOST`. To derive from a theme other than `bira`, set `PHONESSH_THEME`.

The theme recoloring only knows how to handle `bira`'s color definitions. Other themes will be copied but may not change color. Needs `figlet` on your computer for the banner (it falls back to plain text without it).

To use it, put it on your `PATH` or add an alias:

```sh
alias phonessh="$HOME/droidkibble/extras/phonessh"
```

Details worth knowing:

- It sets `TERM=xterm-256color` for the session, because terminals like kitty use a terminfo entry the phone doesn't have.
- oh-my-zsh files are extracted as root on purpose. Your user id on the computer can match a different user in the chroot, which makes oh-my-zsh refuse to load completions.
- `~/.zshrc` and the theme on the phone are generated. Change the script and re-run `--sync` instead of editing them there.
