# Building the GTK Edition

Two targets share this patch set: **ROCKNIX aarch64/glibc** (the rig) and **macOS/Apple Silicon**
(a fast, no-hardware harness for field-testing and cross-platform validation). Pick the section
below for your target.

# Target: ROCKNIX aarch64/glibc

The build replicates **RPCS3's own official ARM64 Linux CI** — same Docker image recipe, same
build script, same AppImage packaging — so the output matches the format ROCKNIX itself ships
(`rpcs3-sa` is a dwarfs/uruntime AppImage). Built and validated on an Apple-silicon Mac via a
native arm64 Docker runtime (colima); any arm64 Linux/Docker host works the same.

## 1. Toolchain image (one-time, multi-hour)

Build the CI image from RPCS3's public Dockerfile — `github.com/RPCS3/rpcs3-docker`, path
`jammy-aarch64/Dockerfile` (Ubuntu 22.04 base; compiles GCC 13 + LLVM 22, a static LLVM for the
JIT, CMake, Ninja, Qt 6, FFmpeg, SDL3, OpenCV from source at pinned versions):

```sh
git clone https://github.com/RPCS3/rpcs3-docker
docker build -t etk-rpcs3-jammy-aarch64:local rpcs3-docker/jammy-aarch64
```

Jammy's glibc 2.35 is older than ROCKNIX's (2.41), so the binary runs on the rig
(backward-compat is one-directional: older-build → newer-runtime is safe).

## 2. Source + patch

```sh
git clone https://github.com/RPCS3/rpcs3 && cd rpcs3
git checkout 60c9705a          # v0.0.41-19544 — the patch base
git apply /path/to/patches/etk-rpcs3-gtk-edition-0.6.0.patch
```

## 3. Build (clang variant — the flavor ROCKNIX's package downloads)

The CI contract mounts the repo root at container path `/rpcs3`:

```sh
docker run --rm -v "$PWD":/rpcs3 -e COMPILER=clang \
    etk-rpcs3-jammy-aarch64:local /rpcs3/.ci/build-linux-aarch64.sh
```

**Iterating on a patch:** keep the build tree warm and rebuild incrementally — copy only the
changed file(s) in and run `ninja` in `/rpcs3/build` (a one-file change is ~2 min instead of a
full multi-hour build). Gate on ninja's real exit code; never pipe it to `tail`.

## 4. Package the AppImage

```sh
docker run --rm -v "$PWD":/rpcs3 -e DEPLOY_APPIMAGE=true \
    etk-rpcs3-jammy-aarch64:local sh -c \
    'git config --global --add safe.directory "*"; rm -f /rpcs3/build/AppDir/.DirIcon; cd /rpcs3 && sh .ci/deploy-linux.sh aarch64'
```

Known packaging quirks (both benign, both hit in practice):
- `deploy-linux.sh` re-runs fail on an existing `AppDir/.DirIcon` (its `ln -sr` has no `-f`) —
  delete it first, as above.
- The script's **last** line writes a CI-only `$RELEASE_MESSAGE` file and exits non-zero outside
  CI — the AppImage is already fully built and renamed at that point; ignore that exit code and
  check for `build/rpcs3-v*.AppImage` instead.
- If your Docker host bind-mounts through **virtiofs** (colima default): `linuxdeploy`'s
  dependency-vendoring copy silently corrupts files on virtiofs. Do the build/packaging inside a
  **native docker volume** and copy the artifact out at the end.

## 5. Verify

The binary self-reports the stock upstream version string (the patch set doesn't bump it) —
identify builds by **sha256**, not the About dialog. The 0.6.0 release asset (flicker fix
default-on):

```
rpcs3-etk_gtk-edition-0.6.0_v0.0.41-19544-60c9705a_linux_aarch64.AppImage
sha256 1d0b490da981c3e05783fa621dcb4f5cdc7a5f48e380dafe8f111bbeb2ed80e8
```

Deploy to a ROCKNIX rig with ETK: nothing to configure — **ETK ≥0.6.0 fetches and deploys this
exact build automatically** (`install.sh` STEP 6.55, sha256-verified against the pin above) and
bind-mounts it over the read-only stock `rpcs3-sa` every boot (stock is never modified;
`RPCS3_APPIMAGE="stock"` in etk.conf opts out). Point `RPCS3_APPIMAGE` at a local file only to
stage your own dev build. No env flags needed: the road-flicker fix is on by default
(`GTK_REMAP0_ONE=0` is the diagnostic kill-switch).

# Target: macOS / Apple Silicon

A native macOS build via MoltenVK (Vulkan-over-Metal) — no rig, no Docker, useful as a fast
iteration harness and as its own field-test target (a genuinely different Vulkan implementation
and silicon vendor from every other confirmation of the road-flicker fix to date). Scoped to the
**0.6.0** patch (road-flicker fix + bounded fence waits + audio telemetry, inert on macOS): the
0.6.1-dev tguard/ffs device-loss hardening targets a6xx/Adreno kernel-hang recovery specific to
ROCKNIX and doesn't apply to Apple's GPU stack.

## 1. Toolchain (one-time)

Homebrew (Apple Silicon paths assumed):

```sh
brew install ccache llvm@21 googletest opencv@4 sdl3 vulkan-headers vulkan-loader molten-vk cmake ninja qt6
```

`qt6` installs as ~38 separate Homebrew kegs (not one prefix like the official Qt installer) —
this drives Trap 1 below.

## 2. Source + patch

```sh
git clone https://github.com/RPCS3/rpcs3 && cd rpcs3
git checkout 60c9705a          # v0.0.41-19544 — the patch base
git apply /path/to/patches/etk-rpcs3-gtk-edition-0.6.0.patch
```

## 3. Configure + build

```sh
unset Qt6_DIR
export SDL3_DIR=/opt/homebrew/opt/sdl3/lib/cmake/SDL3
export PATH="/opt/homebrew/opt/llvm@21/bin:$PATH"
export LDFLAGS="-L/opt/homebrew/opt/llvm@21/lib/c++ -L/opt/homebrew/opt/llvm@21/lib/unwind -lunwind"
export VULKAN_SDK=/opt/homebrew/opt/molten-vk   # needs libvulkan.dylib symlinked in from vulkan-loader once
export LLVM_DIR=/opt/homebrew/opt/llvm@21
export CMAKE_PREFIX_PATH=$(ls -d /opt/homebrew/opt/qt* | paste -sd ';' -)

mkdir build && cd build
cmake .. -DBUILD_RPCS3_TESTS=OFF -DRUN_RPCS3_TESTS=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=14.4 \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)" -DSTATIC_LINK_LLVM=ON -DUSE_SDL=ON \
  -DUSE_DISCORD_RPC=ON -DUSE_AUDIOUNIT=ON -DUSE_SYSTEM_FFMPEG=OFF -DUSE_NATIVE_INSTRUCTIONS=OFF \
  -DUSE_PRECOMPILED_HEADERS=OFF -DUSE_SYSTEM_MVK=ON -DUSE_SYSTEM_SDL=ON -DUSE_SYSTEM_OPENCV=ON \
  -DUSE_SYSTEM_PROTOBUF=ON -G Ninja
ninja rpcs3
```

Known build traps (all hit in practice, all with a one-line fix):
- **Trap 1 — do not set `Qt6_DIR` explicitly.** RPCS3's `find_package(Qt6 ...)` looks for sibling
  component configs *relative to* `Qt6_DIR` when it's set manually, but Homebrew splits
  qtmultimedia/qtsvg/etc. into separate kegs, so the lookup fails. Leave it unset; use
  `CMAKE_PREFIX_PATH` (above) instead. A stale `CMakeCache` entry from a prior failed attempt does
  NOT get re-searched on a plain re-run — `rm -rf build` and reconfigure clean if you hit this.
- **Trap 2 — protobuf codegen/runtime mismatch.** `rpcs3/Emu/NP/generated/np2_structs.pb.h` is a
  git-committed pre-generated file that can drift from the vendored protobuf submodule's version.
  `-DUSE_SYSTEM_PROTOBUF=ON` regenerates it fresh from whatever `protoc` is on `PATH` at configure
  time, self-healing the mismatch (Homebrew's `qt6`→`qtgrpc` chain installs its own protobuf/protoc
  as a side effect, which is what this flag targets).
- **Trap 3 (runtime) — codesign seal mismatch → silent SIGKILL, no output.** After building, running
  `build/bin/rpcs3.app/Contents/MacOS/rpcs3` directly dies with exit 137 and zero output — the
  post-link deploy step signs each embedded dylib separately, so the outer bundle's sealed-resources
  manifest doesn't match. Fix: `codesign --deep --force -s - build/bin/rpcs3.app` (one atomic
  re-sign of the whole tree).
- **Trap 4 (runtime) — OpenMP double-init → SIGABRT.** LLVM's `libomp` and OpenCV's bundled copy
  both link into the process (a known Homebrew multi-package conflict). Fix: run with
  `KMP_DUPLICATE_LIB_OK=TRUE` — see packaging step 4 for baking this into the app bundle so it
  isn't needed at launch time.

## 4. Package as a double-clickable app

The raw build output (`build/bin/rpcs3.app`) already launches from Terminal once traps 3–4 are
handled. To make it a normal double-click app (no Terminal, no env vars) — needed for field-testing
by anyone who isn't a developer:

```sh
APP=build/bin/rpcs3.app
PLIST="$APP/Contents/Info.plist"

# Bake the OpenMP env fix into the bundle (Trap 4) — Finder launches read LSEnvironment.
/usr/libexec/PlistBuddy -c "Add :LSEnvironment dict" "$PLIST"
/usr/libexec/PlistBuddy -c "Add :LSEnvironment:KMP_DUPLICATE_LIB_OK string TRUE" "$PLIST"

# Distinct identity so it doesn't collide with a stock RPCS3.app in Launch Services.
/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string 'RPCS3 GTK Edition'" "$PLIST"
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier net.rpcs3.rpcs3.gtk" "$PLIST"

# Re-sign after editing Info.plist (Trap 3 — any plist edit invalidates the seal).
codesign --deep --force -s - "$APP"
codesign --verify --deep --strict "$APP" && echo OK

# Install (copy, don't move, if you're keeping the build tree for iteration).
ditto "$APP" "/Applications/RPCS3 GTK Edition.app"
```

Config, games, and saves are **shared automatically** with any other RPCS3 install on the same
Mac — `fs::get_config_dir()` resolves to `~/Library/Application Support/rpcs3/` per-user, not
per-app-bundle (no `portable/` directory check succeeds for a fresh build), so a stock
`/Applications/RPCS3.app` and this GTK Edition build coexist and share one game library. Logs land
at `~/Library/Caches/rpcs3/RPCS3.log` — a different path from the config directory, easy to miss.

## 5. Verify

Same as the Linux build: the binary self-reports the stock upstream version string, so identify by
**sha256**, not the About dialog:

```
rpcs3-gtk-edition-0.6.0_v0.0.41-19544-60c9705a_macos_arm64.app.zip
sha256 29c63a3c93c4a7eaec6a441f47fd39e17dbd141da056343a8da82ccc9a101fb1
```

To confirm the fix is engaged, watch `~/Library/Caches/rpcs3/RPCS3.log` while playing GT5P for
`GTK-REMAP0: force-ONE override engaged (TIU...)` — this line fires with **no env vars set**, since
the fix is on by default. Field-validated 2026-07-09: GT5P road-flicker gone across Eiger, Daytona,
HSL, and Fuji on Apple M1 via MoltenVK.
