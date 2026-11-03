# Mac build guide — Android 6 / iOS 9.3 iPad

This guide describes the intended local workflow for the ARMv7 Android 6 build and the ARM64 iPad IPA. It does not claim that a clean checkout can currently regenerate every private game-derived input or that either package has been device-tested.

## 1. Keep the original game files local

Place these two folders side by side:

```text
Re3DXPlus/
Wangan Midnight Maximum Tune 3DX+ (Japan)/
```

The original folder must contain `main` and `data/`, including `data/rtmovie/table`, `data/rtmovie/trail`, and `data/cutin/jp`. Do not upload or commit original game files, generated game data, save files, or signing credentials.

## 2. Important source-snapshot limitation

A clean data rebuild currently stops before modifying `data/` if these generator tools are missing:

- `tools/weartexts.py`
- `tools/titledb.py`
- `tools/storycut.py`
- `tools/storybgm.py`

The latest public `Re3DXPlus_Original` snapshots intentionally omit some original-derived generators and tables. Do not substitute unrelated versions: their output must match this source revision. Restore compatible copies from your own lawful local development source tree, or provide the matching generated outputs and update the generation procedure deliberately. The repository does not fabricate these files.

## 3. Generate the shared game data on macOS

Install Python 3, GNU binutils (`c++filt`), FFmpeg with a Bink decoder, and the macOS `afconvert` utility. The script creates a project-local Python virtual environment and installs `capstone`, `unicorn`, `Pillow`, and `numpy`. Data-only mode does not require Xcode, an iPhoneOS SDK, or `ldid`; those are required only for the IPA build.

From Terminal:

```sh
cd /path/to/Re3DXPlus
sh ./build_ios12_arm64.sh data
```

This runs the code/physics generators, map and car packers, HUD/menu packers, and audio/movie conversion. The result is in `Re3DXPlus/data/`. The original arcade folder is read as input; it is not moved or modified.

To intentionally regenerate from scratch, preserving the previous generated data as a backup:

```sh
REBUILD_IOS_ASSETS=1 sh ./build_ios12_arm64.sh data
```

The backup is placed under `build/data-backup-YYYYMMDD-HHMMSS`. Do not delete it until the new data has been checked.

## 4. Build Android 6 — 32-bit ARM

Install Android Studio components matching the checked-in build configuration:

- Android SDK Platform 35
- Android NDK 27.2.12479018
- CMake 3.22.1
- JDK 17 or newer

Connect the Android 6 device with USB debugging enabled. From the repository root:

```sh
sh ./build_android_api23_armv7.sh data
```

This builds the `armeabi-v7a` APK, installs it, and copies the contents of `data/` plus the original realtime-movie and cut-in resources to:

```text
/sdcard/wmmt/
```

The files must be directly inside `wmmt/` (for example, `/sdcard/wmmt/menu.bin`), not nested under `wmmt/data/`. Game data is external; it is not embedded in the APK. The script prints the actual APK path after the build.

## 5. Build the iOS 9.3+ iPad IPA

The iPad build is ARM64, with `IOS_MIN_VERSION=9.3` by default. This is the correct architecture family for iPad mini 2; the app's `UIDeviceFamily` is iPad. The wrapper uses the selected Xcode iPhoneOS SDK by default, or an explicitly supplied `IOS_SDK`.

After step 3 has produced a complete data set:

```sh
sh ./build_ios12_arm64.sh ipa
```

The wrapper compiles the app and calls `build.sh ipa`. The IPA is written to:

```text
build/WMMT3DX-1.0.ipa
```

The IPA packager copies the entire contents of `data/` into the app bundle, then adds the original `rtmovie/table`, `rtmovie/trail`, and `cutin/jp` resources. It checks that the ZIP is valid and that at least `tokyo.wpk` and `menu.bin` are actually inside the bundle. This prevents the previous silent-omission behavior for optional per-file copies.

The current signing step uses `ldid` (ad-hoc/fake signing), not an App Store distribution certificate. Installation therefore depends on a compatible jailbreak/AppSync or other installation/signing arrangement. A successful build does not prove runtime compatibility; test on the actual iPad mini 2.

## 6. Rebuild sequence after data is ready

If the same regenerated data is intended for both platforms:

1. Run `sh ./build_ios12_arm64.sh data` once.
2. Run `sh ./build_android_api23_armv7.sh data` to build/install Android and transfer `wmmt/`.
3. Run `sh ./build_ios12_arm64.sh ipa` to package that same data into the iPad IPA.

Do not run the clean rebuild flag again between steps 2 and 3 unless you intentionally want to replace the shared data set.
