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

Game data is not bundled into the APK. It must be copied separately to the app's external files directory:
`Android/data/com.wmmt.port/files/wmmt`.

## Device note

The project is configured for GLES 2.0 and 32-bit ARM. Actual startup and rendering still need to be verified on the target Mali-400 device. Some optional GLES extensions and the game's memory use may require further device-specific adjustments.
