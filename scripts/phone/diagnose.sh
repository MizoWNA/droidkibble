#!/system/bin/sh
# Read-only check of a rooted phone: what is it, and who feeds its hardware watchdogs?
# Changes nothing. Prints no IP addresses, serial numbers or account names, so the output
# is safe to paste into an issue.
#
# Run it as root on the phone:
#   ssh <phone-root> sh -s < scripts/phone/diagnose.sh
# or, with only adb:
#   adb push scripts/phone/diagnose.sh /data/local/tmp/ && adb shell su -c 'sh /data/local/tmp/diagnose.sh'

echo "== phone"
echo "brand/model : $(getprop ro.product.brand) $(getprop ro.product.model)"
echo "android     : $(getprop ro.build.version.release) (sdk $(getprop ro.build.version.sdk))"
echo "soc         : $(getprop ro.board.platform) / $(getprop ro.hardware)"
echo "kernel      : $(uname -r) $(uname -m)"
echo "selinux     : $(getenforce 2>/dev/null)"
echo "magisk      : $(magisk -v 2>/dev/null || echo 'not found')"
echo "user        : $(id)"
[ "$(id -u)" = 0 ] || echo "!! not root: the watchdog check below will see nothing"
echo "memory      : $(grep -E '^(MemTotal|MemAvailable)' /proc/meminfo | tr -s ' ' | tr '\n' ' ')"
echo "uptime      : $(cut -d' ' -f1 /proc/uptime) s"

echo
echo "== android framework"
echo "zygote      : $(getprop init.svc.zygote)"
echo "system_server pid: $(pidof system_server || echo 'not running')"

echo
echo "== watchdog devices"
devs=$(ls /dev/watchdog* 2>/dev/null)
if [ -z "$devs" ]; then
    echo "none found in /dev"
else
    echo "$devs" | sed 's/^/present     : /'
fi

echo
echo "who has them open (needs root):"
held=""
for p in /proc/[0-9]*; do
    pid=${p#/proc/}
    for fd in "$p"/fd/*; do
        t=$(readlink "$fd" 2>/dev/null) || continue
        case "$t" in
        /dev/watchdog*)
            name=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null | cut -c1-60)
            [ -n "$name" ] || name=$(cat "$p/comm" 2>/dev/null)
            echo "  $t  <-  pid $pid  $name"
            held="$held $t:$name"
            ;;
        esac
    done
done
[ -n "$held" ] || echo "  (nobody, or not root)"

echo
echo "== what this means"
sysheld=""
for h in $held; do
    case "$h" in *system_server*) sysheld="$sysheld ${h%%:*}" ;; esac
done
if [ -n "$sysheld" ]; then
    echo "system_server holds:$sysheld"
    echo "If you stop the Android framework, nothing feeds those devices and the phone will"
    echo "probably reset a minute or two later. droidkibble's ui.sh feeds /dev/watchdog1 for"
    echo "this reason. If your list shows a different device, that is the one to feed instead."
elif [ -z "$(pidof system_server)" ]; then
    echo "The framework is stopped, so system_server can't be checked. Anything in the list above"
    echo "that is not watchdogd is what replaced it. To see the stock picture, run this again with"
    echo "the normal UI on (ui.sh on)."
elif [ "$(id -u)" != 0 ]; then
    echo "Not root, so nothing could be checked."
else
    echo "system_server does not hold a watchdog device on this phone, so headless mode may not"
    echo "need a feeder here. Test it with a cable attached and a way to reboot by hand."
fi

echo
echo "== droidkibble install"
B=/data/adb/phoneserver
for f in arch.sh ui.sh autostart.sh phoneserverd; do
    [ -e "$B/$f" ] && echo "have        : $B/$f" || echo "missing     : $B/$f"
done
[ -e /data/adb/service.d/phoneserver.sh ] && echo "have        : boot hook" || echo "missing     : boot hook"
[ -d /data/arch/etc ] && echo "chroot      : /data/arch present" || echo "chroot      : /data/arch not found"
for f in headless headless.pending headless.blocked; do
    [ -e "$B/$f" ] && echo "flag        : $f exists"
done
