#!/system/bin/sh
# Start the on-screen status (dk.Status) on the phone. It runs under app_process without the Android
# framework, using the class paths that SurfaceFlinger was started with.
#   run.sh [--seconds N]          (no argument: runs until stopped)
# Needs SurfaceFlinger running (it keeps running in headless mode) and status.jar next to this script.
D="$(cd "$(dirname "$0")" && pwd)"
P=$(pidof surfaceflinger) || { echo "surfaceflinger is not running" >&2; exit 1; }
eval "$(tr '\0' '\n' < /proc/$P/environ | grep -E '^(ANDROID_|BOOTCLASSPATH|DEX2OATBOOTCLASSPATH|SYSTEMSERVERCLASSPATH|ASEC_MOUNTPOINT)' | sed 's/^/export /')"
export CLASSPATH="$D/status.jar"
# phoneserverd sets NO_BACKLIGHT and looks after the backlight itself. Run by hand, we do it here.
if [ -n "$NO_BACKLIGHT" ]; then
  exec app_process /system/bin dk.Status "$@"
fi
B=/sys/class/backlight/panel/brightness
echo "${BRIGHTNESS:-120}" > $B
trap 'echo 0 > $B' EXIT
app_process /system/bin dk.Status "$@"
