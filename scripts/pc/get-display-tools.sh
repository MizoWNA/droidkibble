#!/usr/bin/env bash
# Download what display/build.sh needs, into ~/.local/droidkibble-sdk (no root needed, about 260 MB, once):
#   - a JDK 17 (Amazon Corretto, from corretto.aws)
#   - d8, the Android dex compiler (the r8 jar from Google's Maven repo)
#   - android.jar for API 30, taken from Google's platform-30 package
# Override the location with SDK=/some/dir.
set -euo pipefail
SDK="${SDK:-$HOME/.local/droidkibble-sdk}"
mkdir -p "$SDK"; cd "$SDK"
dl() { echo "downloading $2 ..."; curl -L --fail --retry 3 -sS -o "$2.part" "$1" && mv "$2.part" "$2"; echo "  got $2 ($(du -h "$2" | cut -f1))"; }
[ -d jdk ]         || { [ -f jdk.tar.gz ] || dl "https://corretto.aws/downloads/latest/amazon-corretto-17-x64-linux-jdk.tar.gz" jdk.tar.gz
                        mkdir jdk && tar -xzf jdk.tar.gz -C jdk --strip-components=1; }
[ -f r8.jar ]      || dl "https://dl.google.com/dl/android/maven2/com/android/tools/r8/8.1.72/r8-8.1.72.jar" r8.jar
[ -f android.jar ] || { [ -f platform.zip ] || dl "https://dl.google.com/android/repository/platform-30_r03.zip" platform.zip
                        unzip -q -o -j platform.zip 'android-11/android.jar' -d .; }
"$SDK/jdk/bin/java" -version 2>&1 | head -1
echo "ready in $SDK. Build with display/build.sh"
