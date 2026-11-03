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
