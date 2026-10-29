# EXWORLD

GTA-style open-world game on [EXGINE](https://github.com/benjaminchume-droid/exgine).

**100% procedural.** No authored glTF characters, no high-res texture packs, no sampled audio banks required.

Read the honest engineering audit: [`docs/VISION_AND_GAP.md`](docs/VISION_AND_GAP.md) —
what we wanted, what actually shipped, and what got fixed in the visual-rescue pass.

## Status (v0.4 — visual rescue pass)

| System | State |
|--------|--------|
| Vehicle enter/exit (ExAnimation + ExSound) | Done |
| VehicleDynamicsController possession | Done |
| Character controller + 3rd person camera | Done |
| Building doors + interior room nav | Done |
| Wanted levels + police AI chase | Done (police = 4 straight-line walkers; escalation is next) |
| Real input (keyboard / gamepad / touch) | Done |
| Save / load (player + wanted) | Implemented (no trigger wired yet) |
| Streaming terrain/water/vegetation | Done (city blocks + roads are the next system) |
| **Per-seed materials: biome terrain, building facades, car paint, skin/clothing** | **Done** |
| **Vehicles rotate with physics yaw; characters face movement** | **Done** |
| **Day/night cycle + distance fog wired into lighting** | **Done** |
| **Android NativeActivity + debug APK workflow** | **Done** |

Still missing (ranked in the audit): roads/city blocks, sun shadow map, character
animation (skeleton exists, driver disabled), readable HUD overlay, police escalation,
save trigger.

## Android debug APK

### CI (automatic)

Push to `main` or run the workflow manually:

**Actions → EXWORLD Android Debug APK → Run workflow**

Artifact name: **`exworld-android-debug`**  
Contains: `app-debug.apk` (installable debug build)

### Local

```bash
# Sibling layout required
git clone https://github.com/benjaminchume-droid/exgine.git
git clone https://github.com/benjaminchume-droid/exworld.git

cd exworld/platform/android
# SDK 35, NDK 27.2.12479018, CMake 3.22.1, JDK 17
export EXGINE_ROOT=../../exgine   # or absolute path
gradle :app:assembleDebug

# APK:
# app/build/outputs/apk/debug/app-debug.apk
```

Install:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Expected size (procedural, arm64 + armv7): roughly **18–40 MB** debug.

## Desktop validation

```bash
cmake -S . -B build -DEXGINE_ROOT=../exgine
cmake --build build -j
./build/exworld
```

## License

Apache 2.0
