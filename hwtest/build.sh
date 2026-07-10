#!/bin/sh
# Deterministic container build for rsx-zfunc-hwtest.
#
# Toolchain image (classic psl1ght-era ps3dev, linux/amd64 — runs under
# emulation on arm64 hosts):
#   ps3dev/ps3dev:submodules
#   digest sha256:990a4dec9150b89508134271964827ece5483c6a5e16e96f34e4283cf0791a32
#
# psl1ght's cgcomp dlopens NVIDIA's proprietary x86-only libCg.so, which is
# NOT redistributed here. Fetch it once from the NVIDIA Cg 3.1 (April 2012)
# toolkit and place libCg.so in $CG_DIR (default ~/etk-cg):
#   https://developer.download.nvidia.com/cg/Cg_3.1/Cg-3.1_April2012_x86_64.tgz
#   sha256 e8ff01e6cc38d1b3fd56a083f5860737dbd2f319a39037528fb1a74a89ae9878
#   tar xzf Cg-3.1_April2012_x86_64.tgz ./usr/lib64/libCg.so
#
# Outputs (all in this directory):
#   rsx-zfunc-hwtest.self       CEX self — boot this in RPCS3, or on a real
#                               PS3 with HEN/CFW
#   rsx-zfunc-hwtest.fake.self  fself variant, for loaders that want one
#   rsx-zfunc-hwtest.elf        stripped + sprxlinker-fixed ELF (ps3load etc.)
#
# NOTE: the raw linker output (before sprxlinker) has an empty PRX stub table
# and dies at a null call in crt — always ship/boot the artifacts above, which
# the Makefile produces from the fixed copy under build/.

set -e

HWTEST_DIR=$(cd "$(dirname "$0")" && pwd)
CG_DIR=${CG_DIR:-$HOME/etk-cg}
IMAGE=${IMAGE:-ps3dev/ps3dev:submodules}

if [ ! -f "$CG_DIR/libCg.so" ]; then
    echo "error: $CG_DIR/libCg.so not found — extract it from the NVIDIA Cg 3.1 tarball (see header)" >&2
    exit 1
fi

docker run --rm --platform linux/amd64 \
    -v "$HWTEST_DIR":/work -v "$CG_DIR":/opt/cg -w /work \
    "$IMAGE" bash -c '
        set -e
        cp /opt/cg/libCg.so /usr/lib/x86_64-linux-gnu/ && ldconfig
        make clean && make
    '

# ship the sprxlinker-fixed ELF, not the raw link output
cp "$HWTEST_DIR/build/rsx-zfunc-hwtest.elf" "$HWTEST_DIR/rsx-zfunc-hwtest.elf"

echo "== artifacts =="
ls -la "$HWTEST_DIR"/rsx-zfunc-hwtest.elf "$HWTEST_DIR"/rsx-zfunc-hwtest.self "$HWTEST_DIR"/rsx-zfunc-hwtest.fake.self
shasum -a 256 "$HWTEST_DIR"/rsx-zfunc-hwtest.elf "$HWTEST_DIR"/rsx-zfunc-hwtest.self "$HWTEST_DIR"/rsx-zfunc-hwtest.fake.self 2>/dev/null || \
    sha256sum "$HWTEST_DIR"/rsx-zfunc-hwtest.elf "$HWTEST_DIR"/rsx-zfunc-hwtest.self "$HWTEST_DIR"/rsx-zfunc-hwtest.fake.self
