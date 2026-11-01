#!/bin/sh
# Install the patched iPhoneOS 12.4 SDK used by the local ARM64 iPad build.
# The SDK is downloaded from the official theos/sdks GitHub release.
set -eu

SDK_NAME="iPhoneOS12.4.sdk"
THEOS_ROOT="${THEOS:-$HOME/theos}"
SDK_ROOT="$THEOS_ROOT/sdks"
DEST="$SDK_ROOT/$SDK_NAME"
URL="https://github.com/theos/sdks/releases/download/master-146e41f/iPhoneOS12.4.sdk.tar.xz"

if [ -f "$DEST/System/Library/Frameworks/UIKit.framework/Headers/UIKit.h" ]; then
  echo "Using existing SDK: $DEST"
  exit 0
fi

mkdir -p "$SDK_ROOT"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/re3dx-sdk.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

echo "Downloading patched $SDK_NAME from theos/sdks..."
curl --fail --location --retry 3 "$URL" -o "$TMP/$SDK_NAME.tar.xz"
tar -xJf "$TMP/$SDK_NAME.tar.xz" -C "$TMP"

if [ -d "$TMP/$SDK_NAME" ]; then
  SOURCE="$TMP/$SDK_NAME"
elif [ -d "$TMP/sdks-master/$SDK_NAME" ]; then
  SOURCE="$TMP/sdks-master/$SDK_NAME"
else
  echo "SDK archive did not contain $SDK_NAME in the expected location." >&2
  exit 1
fi

[ -f "$SOURCE/System/Library/Frameworks/UIKit.framework/Headers/UIKit.h" ] || {
  echo "Downloaded SDK is incomplete: UIKit headers are missing." >&2
  exit 1
}

rm -rf "$DEST"
mv "$SOURCE" "$DEST"
echo "Installed SDK: $DEST"
