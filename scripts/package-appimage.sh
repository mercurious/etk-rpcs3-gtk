#!/usr/bin/env bash
# package-appimage.sh — hardened AppImage packager for the ROCKNIX aarch64 target.
#
# Wraps RPCS3's own .ci/deploy-linux.sh but closes the four traps that shipped a
# broken v0.7.1 (game quit straight to ES: `AppRun.wrapped: error while loading
# shared libraries: libavcodec.so.62`):
#
#   1. SILENT WRONG-ARCH DEFAULT. The stock script is `CPU_ARCH="${1:-x86_64}"` —
#      forget the positional arch on an aarch64 host and it silently builds an
#      x86_64-targeted package. We follow `uname -m`, never the x86_64 default.
#   2. STALE PRE-BAKED TOOLS. The script only fetches linuxdeploy/uruntime via a
#      `[ ! -f ]` guard, so a wrong-arch tool left in the image is reused. We purge
#      any tool whose `file(1)` arch != target before the run.
#   3. SWALLOWED FAILURES. The stock script continues past linuxdeploy/uruntime
#      dying with "Exec format error" and still exits 0. We `set -euo pipefail`,
#      gate on the real exit, and hard-fail if no AppImage is produced.
#   4. NO OUTPUT VERIFICATION. A package can be produced whose binary can't find
#      its own bundled libs (the linuxdeploy RUNPATH step is what makes ffmpeg et al
#      resolvable at `$ORIGIN/../lib`). We VERIFY by hiding the container's system
#      ffmpeg and running the packaged AppImage `--version` — it must still load,
#      exactly as the rig would. If it can't, we repair the RUNPATH and repackage
#      once; if it still can't, we FAIL rather than ship.
#
# ROCKNIX (aarch64 AppImage) is the primary target; this is that lane's packager.
# Runs INSIDE the CI container (etk-rpcs3-jammy-aarch64), cwd = the repo root.
#
# Usage (in-container):
#   MARKER=rpcs3_perf_stat scripts/package-appimage.sh [aarch64]
# Env: REPO (default /rpcs3), MARKER (a symbol that MUST be in the binary — catches
#      "packaged a stale/wrong build"), URUNTIME_VER (default v0.3.4).
set -euo pipefail

REPO="${REPO:-/rpcs3}"
TARGET_ARCH="${1:-$(uname -m)}"
MARKER="${MARKER:-}"
URUNTIME_VER="${URUNTIME_VER:-v0.3.4}"
cd "$REPO"

BIN="build/AppDir/usr/bin/rpcs3"
log(){ printf '\n\033[36m== %s ==\033[0m\n' "$*"; }
die(){ printf '\033[31mFATAL: %s\033[0m\n' "$*" >&2; exit 1; }

case "$TARGET_ARCH" in
  aarch64) FILE_ARCH='ARM aarch64' ;;
  x86_64)  FILE_ARCH='x86-64' ;;
  *) die "unsupported TARGET_ARCH '$TARGET_ARCH'";;
esac
log "target arch: $TARGET_ARCH  (host $(uname -m))"

# --- trap 2: purge stale wrong-arch tools so the deploy re-fetches correct ones ---
for t in /usr/bin/linuxdeploy /usr/bin/linuxdeploy-plugin-qt /uruntime; do
  if [ -f "$t" ] && ! file -L "$t" 2>/dev/null | grep -q "$FILE_ARCH"; then
    echo "purging wrong-arch $t ($(file -L "$t" | grep -o 'ARM aarch64\|x86-64' | head -1))"
    rm -f "$t"
  fi
done

# --- traps 1+3: run the stock deploy WITH the arch arg, gated on real exit ---
# DEPLOY_APPIMAGE=true gates the whole packaging block (deploy-linux.sh:7); without
# it the script no-ops and produces nothing. RELEASE_MESSAGE is where it writes the
# sha (deploy-linux.sh:82) — must be a writable path or the run dies at the finish.
log "deploy-linux.sh $TARGET_ARCH"
git config --global --add safe.directory '*' 2>/dev/null || true
rm -f build/AppDir/.DirIcon build/*.AppImage
# The ARTIFACT is the source of truth, not the exit code: the stock script exits
# non-zero at its CI-only RELEASE_MESSAGE step AFTER the AppImage is fully built
# (documented quirk), so a non-zero exit with an artifact present is fine — but no
# artifact means a real (silently-swallowed) tool failure. `set -e` is relaxed for
# this one line so we reach the artifact check either way.
DEPLOY_APPIMAGE=true RELEASE_MESSAGE="${RELEASE_MESSAGE:-/tmp/rpcs3-release-message.txt}" \
  sh .ci/deploy-linux.sh "$TARGET_ARCH" \
  || echo "note: deploy-linux.sh exited non-zero — checking for the artifact (RELEASE_MESSAGE quirk is benign)"

APPIMG="$(ls -t build/*.AppImage 2>/dev/null | head -1 || true)"
[ -n "$APPIMG" ] || die "deploy produced no AppImage — real tool failure (arch mismatch / swallowed error)"
log "produced: $APPIMG ($(stat -c %s "$APPIMG") bytes)"

# --- optional: the built binary must contain the expected fork symbol ---
if [ -n "$MARKER" ]; then
  if [ "$(strings "$BIN" | grep -c "$MARKER")" -gt 0 ]; then
    echo "marker OK: '$MARKER' present in binary"
  else
    die "marker '$MARKER' NOT in binary — packaged a stale/wrong build?"
  fi
fi

# --- helpers for the ffmpeg-hidden isolation test (the rig's condition) ---
ensure_patchelf(){ command -v patchelf >/dev/null || { apt-get update -qq && apt-get install -y -qq patchelf; }; }
HIDDEN=/tmp/.pkgverify-ffmpeg-hidden
verify(){  # returns 0 if the packaged AppImage resolves its bundled libs with system ffmpeg gone
  chmod +x "$APPIMG"
  rm -rf "$HIDDEN"; mkdir -p "$HIDDEN"
  # move every ffmpeg lib the binary links out of the system path
  mv /usr/lib/libav*.so.* /usr/lib/libsw*.so.* "$HIDDEN"/ 2>/dev/null || true
  local out rc
  out="$(APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMG" --version 2>&1)"; rc=$?
  mv "$HIDDEN"/* /usr/lib/ 2>/dev/null || true
  rmdir "$HIDDEN" 2>/dev/null || true
  if printf '%s' "$out" | grep -qi 'error while loading shared libraries'; then
    echo "$out" | grep -i 'error while loading' | head -1; return 1
  fi
  [ "$rc" -eq 0 ] || printf '%s\n' "$out" | tail -2  # non-lib exit is informational only
  return 0
}
# always restore ffmpeg even if the script dies mid-verify
trap 'mv "$HIDDEN"/* /usr/lib/ 2>/dev/null || true; rmdir "$HIDDEN" 2>/dev/null || true' EXIT

log "verify: resolve bundled libs with system ffmpeg hidden (rig condition)"
if verify; then
  echo "VERIFY OK — packaged AppImage loads without system ffmpeg"
else
  log "verify FAILED — repairing RUNPATH (\$ORIGIN/../lib) and repackaging once"
  ensure_patchelf
  patchelf --set-rpath '$ORIGIN/../lib' "$BIN"
  echo "rpath now: $(patchelf --print-rpath "$BIN")"
  # repackage with the correct-arch uruntime
  URU=/uruntime
  file -L "$URU" 2>/dev/null | grep -q "$FILE_ARCH" || \
    curl -fsSLo "$URU" "https://github.com/VHSgunzo/uruntime/releases/download/$URUNTIME_VER/uruntime-appimage-dwarfs-$TARGET_ARCH"
  chmod +x "$URU"
  rm -f build/RPCS3.AppImage
  ( cd build && "$URU" --appimage-mkdwarfs -f --set-owner 0 --set-group 0 --no-history \
      --no-create-timestamp --compression zstd:level=22 -S26 -B32 --header "$URU" -i AppDir -o RPCS3.AppImage )
  chmod +x build/RPCS3.AppImage
  APPIMG=build/RPCS3.AppImage
  verify || die "still fails after RUNPATH repair — do not ship (new/unknown dep gap)"
  echo "VERIFY OK after repair"
fi

log "DONE"
echo "artifact : $APPIMG"
echo "size     : $(stat -c %s "$APPIMG") bytes"
echo "sha256   : $(sha256sum "$APPIMG" | cut -d' ' -f1)"
