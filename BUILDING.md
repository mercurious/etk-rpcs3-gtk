# Building the GTK Edition (ROCKNIX aarch64/glibc)

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
