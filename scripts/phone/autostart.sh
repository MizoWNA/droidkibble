#!/system/bin/sh
# Called by the boot hook after the Arch chroot is up. Enters headless mode if enabled and Wi-Fi is healthy.
#
# Boot-loop breaker: headless mode is only trusted once phoneserverd has run for ten minutes without
# trouble (it then deletes headless.pending). If two sessions in a row end before that, headless mode
# blocks itself and the phone boots with the normal Android UI. Remove headless.blocked to try again.
D=/data/adb/phoneserver
[ -f $D/headless ] || { echo "headless flag off; leaving Android UI running"; exit 0; }

if [ -x $D/phoneserverd ]; then
  if [ -f $D/headless.blocked ]; then
    echo "headless is blocked after repeated unstable sessions; remove $D/headless.blocked to retry"; exit 0
  fi
  n=1
  if [ -f $D/headless.pending ]; then
    n=$(( $(cat $D/headless.pending) + 1 ))
    if [ $n -gt 2 ]; then
      touch $D/headless.blocked; rm -f $D/headless.pending
      echo "the last headless sessions did not reach 10 minutes of stability; blocking headless mode"; exit 0
    fi
    echo "previous headless session ended early (attempt $n of 2)"
  fi
  echo $n > $D/headless.pending
fi

# Magisk Bootloop Protector samples zygote for ~50s after boot; stopping it inside that window triggers a reboot.
while [ "$(cut -d. -f1 /proc/uptime)" -lt 150 ]; do sleep 5; done
n=0
until ip -4 addr show wlan0 2>/dev/null | grep -q inet; do
  n=$((n+1)); [ $n -ge 45 ] && { echo "wifi not up after 90s; keeping UI on"; rm -f $D/headless.pending; exit 0; }
  sleep 2
done
echo "wifi up; stopping UI"
sleep 5
$D/ui.sh off

if [ ! -x $D/phoneserverd ]; then
  # legacy fallback: no daemon, so watch the connection from here. If the gateway is unreachable ~5 min
  # in a row, bring the UI back so Android can reconnect Wi-Fi.
  GW=$(ip route show table wlan0 2>/dev/null | awk '/default/{print $3; exit}')
  ( fails=0
    while [ "$(getprop init.svc.zygote)" = "stopped" ]; do
      sleep 60
      if ping -c1 -W3 "$GW" >/dev/null 2>&1; then fails=0; else fails=$((fails+1)); fi
      [ $fails -ge 5 ] && { echo "$(date) watchdog: gateway lost, restoring UI" >> $D/boot.log; $D/ui.sh on; break; }
    done ) >/dev/null 2>&1 &
fi
