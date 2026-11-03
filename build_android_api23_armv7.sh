#!/bin/sh
# Build the Android 6.0 / API 23 32-bit ARM variant.
# This uses the Gradle/CMake configuration in android/, not the legacy arm64 build_android.sh.
set -eu
cd "$(dirname "$0")"
ROOT="$(pwd)"

SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}"
if [ -z "${JAVA_HOME:-}" ] && [ -x "/Applications/Android Studio.app/Contents/jbr/Contents/Home/bin/java" ]; then
  export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
fi

if [ -n "${JAVA_HOME:-}" ] && [ -x "$JAVA_HOME/bin/java" ]; then
  JAVA="$JAVA_HOME/bin/java"
elif command -v java >/dev/null 2>&1; then
  JAVA="$(command -v java)"
else
  echo "Java 17+ not found. Install/use Android Studio's JBR." >&2
  exit 1
fi

JAVA_MAJOR="$("$JAVA" -version 2>&1 | sed -n '1s/.*version "\([0-9][0-9]*\).*/\1/p')"
case "$JAVA_MAJOR" in
  ''|*[!0-9]*) echo "Could not determine Java version." >&2; exit 1 ;;
esac
[ "$JAVA_MAJOR" -ge 17 ] || {
  echo "Gradle plugin 8.13.2 requires JDK 17+. Found Java $JAVA_MAJOR." >&2
  exit 1
}

[ -d "$SDK/platforms/android-35" ] || {
  echo "Missing Android SDK Platform 35: $SDK/platforms/android-35" >&2
  exit 1
}
[ -d "$SDK/ndk/27.2.12479018" ] || {
  echo "Missing Android NDK 27.2.12479018 under $SDK/ndk." >&2
  exit 1
}
[ -d "$SDK/cmake/3.22.1" ] || {
  echo "Missing CMake 3.22.1 under $SDK/cmake." >&2
  exit 1
}

export ANDROID_HOME="$SDK"
echo "Target: Android 6.0 (minSdk 23), armeabi-v7a (32-bit), OpenGL ES 2.0, OpenSL ES"
echo "Using SDK: $SDK"
echo "Using Java: $JAVA"
sh ./gradlew --no-daemon :android:assembleDebug

APK="$(find "$ROOT/android/build/outputs/apk" -type f -name '*.apk' -print | head -n 1)"
[ -n "$APK" ] && [ -f "$APK" ] || {
  echo "Gradle completed but no APK was found under android/build/outputs/apk." >&2
  exit 1
}
echo "Android 6 / ARMv7 APK: $APK"

# Optional device actions. Game assets stay outside the APK in /sdcard/wmmt/.
ACTION="${1:-build}"
if [ "$ACTION" = "install" ] || [ "$ACTION" = "data" ]; then
  ADB="$SDK/platform-tools/adb"
  [ -x "$ADB" ] || { echo "adb not found: $ADB" >&2; exit 1; }
  "$ADB" install -r "$APK"
fi

if [ "$ACTION" = "data" ]; then
  ORIG="$ROOT/../Wangan Midnight Maximum Tune 3DX+ (Japan)"
  [ -d "$ORIG/data/rtmovie/table" ] && [ -d "$ORIG/data/rtmovie/trail" ] || {
    echo "Original realtime movie data is missing under: $ORIG/data/rtmovie" >&2
    exit 1
  }
  [ -d "$ORIG/data/cutin/jp" ] || {
    echo "Original story cut-in text is missing: $ORIG/data/cutin/jp" >&2
    exit 1
  }
  [ -f "$ROOT/data/menu.bin" ] || {
    echo "Generated game data is missing: $ROOT/data/menu.bin. Generate/pack the local data first." >&2
    exit 1
  }

  D="/sdcard/wmmt"
  "$ADB" shell "mkdir -p '$D'"
  # Copy the generated data folder's contents, not the data/ parent directory.
  "$ADB" push "$ROOT/data/." "$D/"
  "$ADB" shell "mkdir -p '$D/rtmovie' '$D/cutin/jp' '$D/carconfig'"
  "$ADB" push "$ORIG/data/rtmovie/table" "$ORIG/data/rtmovie/trail" "$D/rtmovie/"
  "$ADB" push "$ORIG/data/cutin/jp/." "$D/cutin/jp/"
  for f in "$ORIG"/data/carconfig/ytConfig*.txt; do
    [ -f "$f" ] || continue
    "$ADB" push "$f" "$D/carconfig/"
  done
  echo "Android 6 game data copied separately to $D (not bundled in the APK)."
elif [ "$ACTION" != "build" ] && [ "$ACTION" != "install" ]; then
  echo "Usage: sh ./build_android_api23_armv7.sh [build|install|data]" >&2
  exit 2
fi
