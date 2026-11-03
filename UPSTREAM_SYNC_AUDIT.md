# Upstream synchronization audit

Audit date: 2026-11-03

Repositories reviewed:

- Working port: `Chinzx1217/Re3DXPlus`, branch `repair/android6-story-audio-20261103`
- Upstream reference: `Chinzx1217/Re3DXPlus_Original`, HEAD `b93b6b1681acb8e3b917a9e2c08b8c4b0d6322a3`

## Decision: selective sync, not wholesale replacement

The latest upstream repository contains two different generations of port material:

1. Its root source/build files are an older baseline in several critical areas.
2. Its `ports/android` and `ports/ipad` directories are newer 1.31 development snapshots, but they are isolated snapshots, not drop-in replacements for this repository's Android 6 / iOS 9.3 target.

Replacing the working tree with either source set would regress known fixes or change the platform target.

## Compatibility findings

| Area | Upstream snapshot | Working target |
|---|---|---|
| Android graphics | Upstream root manifest requests GLES 3.0 | Android 6 target requests GLES 2.0 |
| Android ABI | Newer isolated port build flow | Dedicated API 23 / `armeabi-v7a` build |
| iPad deployment | `ports/ipad/tools/build_ios.sh` sets minimum iOS 16.0 and only links an executable | ARM64 iOS 9.3 minimum plus IPA packager |
| iOS packaging | Upstream iPad helper does not package or sign an IPA | Local `build.sh ipa` packages app resources |
| Generated assets | Upstream port documentation says generated/private inputs are omitted and a clean build is not established | Local generation still requires user-owned original files and missing generator tools |

## Root-source comparisons

The following current-target protections are absent or weaker in upstream root source and must be preserved:

- `src/car.c`: rejects non-finite or too-short Story Movie lengths before using the movie system.
- `src/app.c`: continues the story race when an optional story movie is unavailable, instead of leaving the state machine idle.
- `src/main.m`: checks the Objective-C callback target and selector before calling `performSelector`.
- `src/main_android.c`: includes the OpenSL ES `audio_pause` implementation.
- `src/render.c`: provides a GLES2 fallback when `GL_OES_vertex_array_object` is unavailable.
- `src/sound.c`: validates RIFF chunks and supports mono/stereo 8-bit/16-bit PCM rather than assuming the minimal mono format.
- `src/menu.c`: byte-for-byte identical to the upstream root version; the scoped `MENU_INFO` handling remains intact.

The upstream root Android manifest requests GLES 3.0, while the Android 6 target must stay on GLES 2.0. The upstream iPad compile helper targets iOS 16.0. Neither should be copied into the target build configuration.

## Changes made in this synchronization pass

- Kept the working target's Story Movie, Objective-C callback, Android audio, GLES2 fallback, and WAV parser fixes.
- Set the ARM64 iPad wrapper's default minimum deployment target to iOS 9.3.
- Changed IPA packaging to copy the complete `data/` tree into the app bundle rather than silently skipping individual missing files.
- Added IPA preflight checks and ZIP-content checks for `tokyo.wpk` and `menu.bin`.
- Added a data-only Mac build mode and an explicit clean-regeneration mode that preserves the previous generated data as a backup.
- Added a Mac build guide and linked it from both READMEs.

## Remaining blockers / verification

The clean-checkout data generator calls four tools that are not present in this repository snapshot:

- `tools/weartexts.py`
- `tools/titledb.py`
- `tools/storycut.py`
- `tools/storybgm.py`

The build wrapper now checks for these before touching existing generated data. Restore compatible local copies from the user's own lawful development source tree before attempting a full data rebuild.

The original arcade files and generated data are not available in this GitHub workspace. Therefore no clean asset generation, APK/IPA build, or real-device test is claimed here. The iPad mini 2 target is ARM64 with iOS 9.3 minimum; actual launch, graphics, movie playback, and performance still require testing on the device.
