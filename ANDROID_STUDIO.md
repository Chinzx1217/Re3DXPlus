# Re3DXPlus Android build

Open this repository root in Android Studio (the folder containing `settings.gradle`).

## Target configuration

- Minimum Android version: Android 6.0 (API 23)
- ABI: `armeabi-v7a` (32-bit ARM only)
- Graphics: OpenGL ES 2.0 / EGL
- Audio output: OpenSL ES
- Native build: CMake

## SDK components

Install these components from Android Studio > SDK Manager:

- Android SDK Platform 35
- Android SDK Build-Tools (latest 35.x)
- NDK (Side by side) 27.2.12479018
- CMake 3.22.1
- Android SDK Platform-Tools

## Build

After Gradle sync completes, choose the `android` run configuration or use:

- Build > Make Project
- Build > Build Bundle(s) / APK(s) > Build APK(s)

The APK is generated under `android/build/outputs/apk/`.

Game data is **not bundled into the APK**. Install the small APK separately, then copy the complete generated game-data contents into a folder named `wmmt` at either location:

- Removable SD-card root: `/storage/XXXX-XXXX/wmmt/`
- Internal shared-storage root: `/storage/emulated/0/wmmt/`

Put `menu.bin` directly inside `wmmt/` (not inside another nested `data/` folder). The folder also contains items such as `cars/`, `sound/`, `traffic/`, `rtmovie/`, and other generated game assets. Do not rename the individual files or subfolders.

For backwards compatibility, the launcher also accepts the older folder name `data/` at either storage root. It checks removable SD cards first, then internal shared storage. The app-specific `Android/data/com.wmmt.port/files` directory is used for saves only, not game data.

On Android 6, grant the app the requested storage permission so it can read the folder. The app does not need to copy the game data into its private directory.

## Prepare sound effects and BGM

The native OpenSL ES mixer reads WAV files from `<game-data>/sound/` and `<game-data>/sound/engine/`. The Java BGM player separately reads AAC files from `<game-data>/sound/bgm/`. The original arcade folder's nested `race_se` and `system_se` WAV layout is not read directly by the runtime.

After placing the original game folder beside this repository, run these from the repository root:

```sh
sh ./prepare_sound.sh
sh ./prepare_bgm.sh
```

Both scripts create files under the local `data/sound/` directory. Copy the **complete generated `data/sound/` folder** into the device's selected game-data folder, preserving the `engine/` and `bgm/` subfolders. In particular, BGM can work while effects remain silent if `sound/*.wav` or `sound/engine/*.wav` were not copied. Native startup logs report missing or unsupported WAV files.

## Device note

The project is configured for GLES 2.0 and 32-bit ARM. Actual startup, rendering, and audio still need to be verified on the target Mali-400 device. Some optional GLES extensions and the game's memory use may require further device-specific adjustments.
