#!/usr/bin/env bash
# Build display/status.jar (a dex jar for app_process) from display/src.
#
# Needs a JDK 17+, d8 (the r8 jar) and an API 30 android.jar. By default they are looked for in
# ~/.local/droidkibble-sdk (jdk/, r8.jar, android.jar); override with SDK=/some/dir.
# ../scripts/pc/get-display-tools.sh downloads them there (about 260 MB, once).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="${SDK:-$HOME/.local/droidkibble-sdk}"
JAVA_HOME="${JAVA_HOME:-$SDK/jdk}"
for f in "$JAVA_HOME/bin/javac" "$SDK/r8.jar" "$SDK/android.jar"; do
    [ -e "$f" ] || { echo "missing $f (run scripts/pc/get-display-tools.sh)" >&2; exit 1; }
done
OUT="$HERE/build"; rm -rf "$OUT"; mkdir -p "$OUT/classes"
"$JAVA_HOME/bin/javac" -Xlint:-options --release 8 -cp "$SDK/android.jar" -d "$OUT/classes" $(find "$HERE/src" -name '*.java')
"$JAVA_HOME/bin/java" -cp "$SDK/r8.jar" com.android.tools.r8.D8 --release --min-api 30 --lib "$SDK/android.jar" \
    --output "$OUT/status.jar" $(find "$OUT/classes" -name '*.class')
ls -l "$OUT/status.jar" | awk '{print "built", $9, "(" $5 " bytes)"}'
