# Upstream synchronization audit

Audit date: 2026-11-03

- Upstream repository: `Chinzx1217/Re3DXPlus_Original`
- Audited upstream revision: `b93b6b1681acb8e3b917a9e2c08b8c4b0d6322a3`
- Port repository branch: `repair/android6-story-audio-20261103`
- Port baseline before this audit: `0b547894f169c9213157d10d163950a74293c7a6`

## Result

The upstream tree is not a drop-in update for this branch. The upstream root contains platform choices that conflict with the requested targets, while its `ports/android/` and `ports/ipad/` directories are separate development snapshots rather than a tested release overlay.

The shared root files were compared. The port-side changes below are intentional compatibility or crash-hardening changes and must not be replaced by the upstream versions:

- `src/main_android.c`, `android/AndroidManifest.xml`: retain OpenSL ES and GLES2 for Android 6 / API 23 / ARMv7. Upstream root uses AAudio and GLES3, which are not suitable defaults for this target.
- `android/java/com/wmmt/port/MainActivity.java`: retain external-storage discovery for the user-managed `wmmt/` data folder, including Android 6 storage permission handling.
- `src/app.c`, `src/car.c`: retain story-movie missing-resource fallbacks and realtime movie directory checks.
- `src/main.m`: retain the guarded movie completion callback.
- `src/render.c`: retain the GLES2 missing-VAO fallback.
- `src/sound.c`: retain the more defensive PCM WAV reader, including stereo downmix handling.
- `src/phys/rc_runtime.c`: retain heap bounds checks and explicit physics-image allocation/read failures.
- `build.sh`, `build_ios12_arm64.sh`: retain the ARM64 iOS packaging target and iOS 9.3 deployment minimum.
- `README.md`, `README.en.md`, `build_android.sh`: retain the target-specific build and data-layout documentation.

## Upstream port snapshots not merged wholesale

- `ports/android/` contains a large independent Android port snapshot with additional networking, story, debug, HUD, and audio work. Its own `SELECTIVE_SYNC.md` labels some paths experimental and documents known interoperability and generated-physics limitations. Its source and tests need feature-by-feature review before they can be used by the Android 6 build.
- `ports/ipad/` is a separate iPad snapshot. Its own README says it is not a ready-to-build package, lists omitted generated headers/assets, and says the complete build is unverified. Its helper targets iOS 16.0, not the requested iOS 9.3 minimum.

Therefore this synchronization keeps the compatible target implementation instead of copying incompatible snapshots. No claim is made that the experimental port-only features have been merged or device-verified.

## Packaging correction in this branch

The iOS IPA packager now validates required generated and original runtime assets before packaging, copies the complete generated `data/` tree while preserving relative paths, and explicitly includes the original realtime movie and cut-in resource trees. This prevents optional-file checks from silently creating a partial IPA.

For a clean local regeneration, use:

```sh
REBUILD_IOS_ASSETS=1 sh ./build_ios12_arm64.sh
```

The previous generated `data/` directory is preserved under `build/data-backup-*` if generation fails or completes. The original arcade data directory is not modified.
