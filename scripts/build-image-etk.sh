#!/usr/bin/env bash
# build-image-etk.sh — build the jammy-aarch64 CI toolchain image with the
# ARMSX3 AArch64 GHC emergency-spill fix baked into the image's static LLVM.
#
# Why this exists: the leap-frog base (ARMSX3 @ f707458b0) REQUIRES
# 3rdparty/llvm/armsx3-aarch64-ghc-emergency-spill.patch on AArch64, but nothing
# in that tree applies it — the LLVM submodule is pinned at stock llvmorg-22.1.8
# and .ci/build-linux-aarch64.sh links the container's prebuilt /opt/llvm.
# Baking the fix into /opt/llvm fixes it once, at zero per-build cost.
# Details: rpcs3-docker-overlay/README.md.
#
# Runs on any arm64 Docker host (etk-cloud primary; colima fallback).
# Idempotent: re-running reuses the checkout and skips an already-injected Dockerfile.
set -euo pipefail
cd "$(dirname "$0")/.."

DOCKER_REPO=${DOCKER_REPO:-https://github.com/RPCS3/rpcs3-docker}
WORK=${WORK:-build-image}
TAG=${TAG:-etk-rpcs3-jammy-aarch64:llvmspill-22.1.8}
PATCH=rpcs3-docker-overlay/armsx3-aarch64-ghc-emergency-spill.patch

[ -f "$PATCH" ] || { echo "FATAL: missing $PATCH" >&2; exit 1; }

if [ -d "$WORK/rpcs3-docker/.git" ]; then
    git -C "$WORK/rpcs3-docker" pull --ff-only
else
    mkdir -p "$WORK"
    git -C "$WORK" clone --depth 1 "$DOCKER_REPO"
fi

CTX="$WORK/rpcs3-docker/jammy-aarch64"
DF="$CTX/Dockerfile"
cp "$PATCH" "$CTX/ghc-emergency-spill.patch"

if grep -q 'ghc-emergency-spill' "$DF"; then
    echo "Dockerfile already carries the injection -- skipping edit."
else
    # Anchor on upstream's exact LLVM-extract line; refuse to guess if it moved.
    python3 - "$DF" <<'EOF'
import sys
path = sys.argv[1]
s = open(path).read()

anchor = "tar -xf llvm-*.tar.xz && \\\n"
n = s.count(anchor)
if n != 1:
    sys.exit(f"FATAL: expected exactly one LLVM tar-extract anchor, found {n} -- "
             "upstream Dockerfile changed shape; update build-image-etk.sh deliberately.")

inject = (
    "\t(command -v patch >/dev/null 2>&1 || (apt-get update && "
    "apt-get install -y --no-install-recommends patch)) && \\\n"
    "\tpatch --fuzz=0 -p1 -d llvm-project-${STATIC_LLVM_VER}.src "
    "< /tmp/ghc-emergency-spill.patch && \\\n"
)
s = s.replace(anchor, anchor + inject)

env_line = "ENV STATIC_LLVM_VER="
if s.count(env_line) != 1:
    sys.exit("FATAL: STATIC_LLVM_VER ENV line not found exactly once.")
i = s.index(env_line)
s = s[:i] + "COPY ghc-emergency-spill.patch /tmp/ghc-emergency-spill.patch\n" + s[i:]

open(path, "w").write(s)
print("Dockerfile injected: patch applied to llvm-project-${STATIC_LLVM_VER}.src "
      "before the LLVM build; a non-applying patch fails the image build loudly.")
EOF
fi

docker build -t "$TAG" "$CTX"
# Keep existing BUILDING.md / mint invocations working unchanged:
docker tag "$TAG" etk-rpcs3-jammy-aarch64:local

echo "OK: built $TAG (and re-tagged etk-rpcs3-jammy-aarch64:local)"
echo "    LLVM 22.1.8 + AArch64 GHC emergency-spill fix in /opt/llvm"
echo "    Sentinel (must NEVER appear in RPCS3.log): 'Retrying module .* allocator-friendly codegen'"
