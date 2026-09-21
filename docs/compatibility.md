# Compatibility

What has been tried, and on what. This started with one phone, and it grows from reports. If you try
droidkibble on another phone, please tell me how it went, whether it worked or not.

| phone | Android / kernel | result | notes | reported by |
|---|---|---|---|---|
| Samsung Galaxy A30 (SM-A305F) | 11 / 4.4.177 | works | Magisk 26.1. `/dev/watchdog1` is fed by `system_server`. Chroot, headless mode, `phoneserverd` and the status screen all tested. | the author |

"Works" means the parts listed in the notes, on that phone. Nothing here has been run for days yet.

## How to report

1. Run the read-only check on the phone. It changes nothing and prints no IP addresses or serial numbers:
   ```sh
   ssh <phone-root> sh -s < scripts/phone/diagnose.sh
   ```
   (or with only adb: `adb push scripts/phone/diagnose.sh /data/local/tmp/ && adb shell su -c 'sh /data/local/tmp/diagnose.sh'`)
2. Open an issue with the [device report form](https://github.com/MizoWNA/droidkibble/issues/new/choose) and paste the output.
   Say how far you got: only the check, the chroot, headless mode.

The most useful single fact is which process holds which watchdog device (`system_server` and `/dev/watchdog1` on the A30).
If `system_server` holds one on your phone, headless mode will probably need feeding that device, and it may not be number 1.
`ui.sh` currently feeds `/dev/watchdog1` only.
