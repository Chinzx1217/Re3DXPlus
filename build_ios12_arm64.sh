#!/bin/sh
# Build an ARM64 iPad IPA locally, without the Xcode GUI.
# The original arcade data stays beside this repository and is never uploaded.
set -eu
cd "$(dirname "$0")"
ROOT="$(pwd)"
ORIG="$ROOT/../Wangan Midnight Maximum Tune 3DX+ (Japan)"
ORIG="$(cd "$ORIG" 2>/dev/null && pwd)" || {
  echo "Original game folder not found: $ORIG" >&2
  echo "Place 'Wangan Midnight Maximum Tune 3DX+ (Japan)' beside Re3DXPlus." >&2
  exit 1
}
[ -f "$ORIG/main" ] || { echo "Missing original executable: $ORIG/main" >&2; exit 1; }
[ -d "$ORIG/data" ] || { echo "Missing original data folder: $ORIG/data" >&2; exit 1; }
[ -d "$ORIG/data/rtmovie/table" ] && [ -d "$ORIG/data/rtmovie/trail" ] || {
  echo "Original data is missing data/rtmovie/table or data/rtmovie/trail." >&2; exit 1;
}
[ -d "$ORIG/data/cutin/jp" ] || { echo "Original data is missing data/cutin/jp." >&2; exit 1; }

CLANG="${IOS_CLANG:-}"
SDK="${IOS_SDK:-}"
THEOS_ROOT="${THEOS:-$HOME/theos}"

# Prefer the manually installed Xcode selected by xcode-select or DEVELOPER_DIR.
# xcrun returns SDK/tool paths with spaces intact; keep every path quoted.
if command -v xcrun >/dev/null 2>&1; then
  if [ -z "$SDK" ]; then
    XCODE_SDK="$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null || true)"
    if [ -n "$XCODE_SDK" ] && [ -d "$XCODE_SDK" ]; then SDK="$XCODE_SDK"; fi
  fi
  if [ -z "$CLANG" ]; then
    XCODE_CLANG="$(xcrun --sdk iphoneos --find clang 2>/dev/null || true)"
    if [ -n "$XCODE_CLANG" ] && [ -x "$XCODE_CLANG" ]; then CLANG="$XCODE_CLANG"; fi
  fi
fi

# Keep the previously supported Theos SDKs as an explicit fallback.
if [ -z "$SDK" ]; then
  for candidate in \
    "$THEOS_ROOT/sdks/iPhoneOS12.4.sdk" \
    "$THEOS_ROOT/sdks/iPhoneOS9.3.sdk" \
    "$THEOS_ROOT/sdks/iPhoneOS.sdk"; do
    if [ -d "$candidate" ]; then SDK="$candidate"; break; fi
  done
fi
[ -n "$SDK" ] || {
  echo "iPhoneOS SDK not found. Install Xcode manually and select it with xcode-select/DEVELOPER_DIR, or set IOS_SDK. Automatic SDK downloads are disabled." >&2
  exit 1
}
CLANG="${CLANG:-clang}"
command -v "$CLANG" >/dev/null 2>&1 || { echo "clang not found: $CLANG" >&2; exit 1; }
command -v ldid >/dev/null 2>&1 || {
  echo "ldid is required for the jailbroken-device IPA. Install it (for example: brew install ldid)." >&2; exit 1;
}
command -v ffmpeg >/dev/null 2>&1 || {
  echo "ffmpeg is required for the arcade Bink movies. Install it (for example: brew install ffmpeg)." >&2; exit 1;
}
command -v afconvert >/dev/null 2>&1 || { echo "afconvert is missing; this script must run on macOS." >&2; exit 1; }
ffmpeg -hide_banner -decoders 2>/dev/null | grep -qi bink || {
  echo "Your ffmpeg build has no Bink decoder. Install a full Homebrew ffmpeg build." >&2; exit 1;
}
[ -d "$SDK" ] || {
  echo "iOS SDK not found: $SDK" >&2
  exit 1;
}
[ -f "$SDK/System/Library/Frameworks/UIKit.framework/Headers/UIKit.h" ] || {
  echo "The SDK does not contain UIKit headers: $SDK" >&2; exit 1;
}
export IOS_SDK="$SDK"
export IOS_ARCH=arm64
export IOS_MIN_VERSION="${IOS_MIN_VERSION:-9.3}"
export IOS_CLANG="$CLANG"

# The source generators need Python packages. Keep them isolated from macOS system Python.
PYTHON="${PYTHON:-python3}"
command -v "$PYTHON" >/dev/null 2>&1 || { echo "python3 not found." >&2; exit 1; }
if [ ! -x .venv/bin/python ]; then "$PYTHON" -m venv .venv; fi
. .venv/bin/activate
python -m pip install --disable-pip-version-check capstone unicorn pillow numpy
if ! command -v c++filt >/dev/null 2>&1 && command -v brew >/dev/null 2>&1; then
  BINUTILS="$(brew --prefix binutils 2>/dev/null || true)"
  if [ -x "$BINUTILS/bin/c++filt" ]; then PATH="$BINUTILS/bin:$PATH"; export PATH; fi
fi
command -v c++filt >/dev/null 2>&1 || {
  echo "c++filt not found. Install GNU binutils (for example: brew install binutils)." >&2; exit 1;
}

mkdir -p build

# A requested clean rebuild keeps the previous generated data as a recoverable backup.
# Original arcade files are never moved or modified.
DATA_BACKUP=""
if [ "${REBUILD_IOS_ASSETS:-0}" = "1" ]; then
  DATA_BACKUP="build/data-backup-$(date +%Y%m%d-%H%M%S)"
  if [ -e "$DATA_BACKUP" ]; then
    echo "Backup path already exists: $DATA_BACKUP" >&2
    exit 1
  fi
  if [ -d data ]; then mv data "$DATA_BACKUP"; fi
  mkdir -p data
  rm -f build/.ios_assets_ready
  echo "Previous generated data preserved at $DATA_BACKUP"
fi
mkdir -p data

restore_data_on_failure() {
  status=$?
  if [ "$status" -ne 0 ] && [ -n "$DATA_BACKUP" ] && [ -d "$DATA_BACKUP" ]; then
    rm -rf data
    mv "$DATA_BACKUP" data
    echo "Restored previous generated data after failure." >&2
  fi
  exit "$status"
}
trap restore_data_on_failure EXIT HUP INT TERM

if [ ! -f build/.ios_assets_ready ]; then
  echo "== Generating code tables and recompiled arcade physics =="
  sh tools/gen_from_original.sh "$ORIG" --phys

  echo "== Packing maps, lights and car assets (first build can take a while) =="
  sh tools/stage/pack_tokyo.sh daytime
  sh tools/stage/pack_tokyo.sh night
  for city in oosaka hakone nagoya fukuoka; do
    for time in daytime night; do
      python3 tools/stage/pack_city.py "$ORIG/data" "$city" "$time" data
    done
  done
  python3 tools/stage/pack_lights.py "$ORIG/data" data
  python3 tools/stage/pack_neon.py "$ORIG/data" data/neon.wpk

  python3 tools/stage/pack_cars.py "$ORIG" data/cars
  for aero in a b c; do
    python3 tools/stage/pack_cars.py "$ORIG" data/cars --aero "$aero"
  done
  python3 tools/stage/pack_cars.py "$ORIG" data/wheels --wheels
  python3 tools/stage/pack_cars.py "$ORIG" data/chara --chara
  python3 tools/stage/pack_vinyl.py "$ORIG" data/vinyl
  sh tools/stage/pack_traffic.sh
  sh tools/stage/pack_car.sh

  echo "== Packing HUD, minimap and menu =="
  python3 tools/hudpack.py "$ORIG/data/sprite" "$ORIG/main" data
  python3 tools/navipack.py "$ORIG/data" "$ORIG/main" data
  FONT="/System/Library/Fonts/Supplemental/Arial.ttf"
  [ -f "$FONT" ] || FONT="/System/Library/Fonts/Supplemental/Arial Unicode.ttf"
  [ -f "$FONT" ] || { echo "No usable system font found for the versus menu." >&2; exit 1; }
  python3 tools/vsdiscs.py "$ORIG/data/sprite" "$FONT"
  python3 tools/menupack.py "$ORIG" data/menu.bin

  echo "== Converting audio and movies =="
  sh prepare_sound.sh
  sh prepare_bgm.sh
  sh prepare_movie.sh

  echo "== Creating app icons =="
  mkdir -p data/icon
  if [ ! -f data/icon/logo_allan2.jpg ]; then cp docs/logo.png data/icon/logo_allan2.jpg; fi
  python3 tools/make_icon.py

  touch build/.ios_assets_ready
else
  echo "Using previously generated local assets in data/."
fi

echo "== Compiling ARM64 iOS app and packaging IPA =="
sh ./build.sh ipa
echo "IPA ready: $ROOT/build/WMMT3DX-1.0.ipa"\ntrap - EXIT HUP INT TERM
