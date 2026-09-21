#!/system/bin/sh
# ui.sh off|on|status
#
# off: stop the Android framework to free RAM, then start phoneserverd, which takes over the jobs the
#      framework did (feeding the second hardware watchdog, renewing the DHCP lease, watching the
#      connection). Without a feeder on /dev/watchdog1 the phone resets ~100 s after the UI stops.
#      If the phoneserverd binary isn't installed, a plain BusyBox watchdog feeder is used instead.
# on:  stop the daemon (or the fallback feeder), then restart the framework. system_server opens the
#      watchdog again by itself.
#
# Optional settings for the on-screen display go in /data/adb/phoneserver/display.conf (shell syntax):
#   DISPLAY_THEME=paper       paper or night
#   DISPLAY_BRIGHTNESS=120    backlight level while it is on (the panel's maximum is 365)
#   DISPLAY_TIMEOUT=600       seconds until the screen turns itself off; 0 = stay on until the button is pressed
BB=/data/adb/magisk/busybox
D=/data/adb/phoneserver
DAEMON=$D/phoneserverd
STOPFILE=$D/daemon.stop
SUPPID=$D/supervisor.pid
FEEDPID=$D/wdt1.pid          # the fallback feeder

daemon_pid() { [ -s $D/phoneserverd.lock ] && cat $D/phoneserverd.lock; }
# The lock file can be stale after a reset, and pids get reused, so check the process really is the daemon.
daemon_alive() {
  p=$(daemon_pid)
  [ -n "$p" ] && kill -0 "$p" 2>/dev/null && $BB grep -aq phoneserverd /proc/$p/cmdline 2>/dev/null
}

start_daemon() {
  daemon_alive && { echo "daemon already running (pid $(daemon_pid))"; return; }
  rm -f $STOPFILE
  ARGS=""
  if [ -f $D/display.conf ]; then
    . $D/display.conf
    [ -n "$DISPLAY_THEME" ] && ARGS="$ARGS --display-theme $DISPLAY_THEME"
    [ -n "$DISPLAY_BRIGHTNESS" ] && ARGS="$ARGS --display-brightness $DISPLAY_BRIGHTNESS"
    [ -n "$DISPLAY_TIMEOUT" ] && ARGS="$ARGS --display-timeout $DISPLAY_TIMEOUT"
  fi
  # respawn loop: if the daemon ever crashes it is back within a second, well inside the watchdog window
  $BB setsid $BB sh -c "while [ ! -f $STOPFILE ]; do $DAEMON $ARGS >/dev/null 2>&1; [ -f $STOPFILE ] && break; sleep 1; done" >/dev/null 2>&1 &
  echo $! > $SUPPID
  sleep 2
  echo "headless: daemon pid $(daemon_pid)"
}

stop_daemon() {
  touch $STOPFILE                                  # tells the respawn loop not to restart it
  daemon_alive && kill "$(daemon_pid)" 2>/dev/null   # SIGTERM: it releases the watchdog cleanly
  n=0; while daemon_alive && [ $n -lt 20 ]; do n=$((n+1)); sleep 0.5; done
  daemon_alive && kill -9 "$(daemon_pid)" 2>/dev/null
  [ -s $SUPPID ] && kill "$(cat $SUPPID)" 2>/dev/null; rm -f $SUPPID
}

start_fallback_feeder() {
  if [ ! -f $FEEDPID ] || ! kill -0 "$(cat $FEEDPID)" 2>/dev/null; then
    $BB setsid $BB watchdog -t 5 -T 60 /dev/watchdog1
    sleep 1; $BB pgrep -f "watchdog -t 5 -T 60 /dev/watchdog1" | head -1 > $FEEDPID
  fi
  echo "headless: fallback feeder pid $(cat $FEEDPID 2>/dev/null) (phoneserverd is not installed)"
}

case "$1" in
  off)
    echo phoneserver > /sys/power/wake_lock
    setprop ctl.stop zygote; setprop ctl.stop zygote_secondary
    n=0
    while ls -l /proc/[0-9]*/fd 2>/dev/null | grep -q "/dev/watchdog1"; do
      n=$((n+1)); [ $n -ge 40 ] && break; sleep 0.5
    done
    if [ -x $DAEMON ]; then start_daemon; else start_fallback_feeder; fi
    setprop service.bootanim.exit 1; setprop ctl.stop bootanim
    ;;
  on)
    rm -f $D/headless.pending      # a deliberate stop is not an unstable session (see autostart.sh)
    stop_daemon
    # the status screen is a layer above everything; make sure none is left over Android's UI
    for p in $($BB pgrep -f "dk\.Status"); do kill -9 $p 2>/dev/null; done
    if [ -f $FEEDPID ]; then kill "$(cat $FEEDPID)" 2>/dev/null; rm -f $FEEDPID; fi
    echo phoneserver > /sys/power/wake_unlock 2>/dev/null
    setprop ctl.start zygote; setprop ctl.start zygote_secondary
    ;;
  status)
    echo "zygote=$(getprop init.svc.zygote) daemon=$(daemon_alive && echo "running pid $(daemon_pid)" || echo stopped) fallback_feeder=$(cat $FEEDPID 2>/dev/null)"
    ;;
  *) echo "usage: $0 off|on|status" ;;
esac
