# rpcs3-docker overlay — LLVM AArch64 GHC emergency-spill fix

## What

`armsx3-aarch64-ghc-emergency-spill.patch` is a 46-line, one-file LLVM patch
(`llvm/lib/Target/AArch64/AArch64FrameLowering.cpp`) that gives GHC-calling-convention
functions an emergency spill slot when they actually have a stack frame. Without it the
register scavenger aborts an entire module with "Cannot scavenge register without an
emergency spill slot" — RPCS3's PPU recompiler emits ghccc for every guest function, so one
unlucky function costs a whole module, which then falls back to the interpreter.

The patch's own header documents the repro: needs ghccc AND a pressure-heavy scheduling
model AND -O2. It reproduces on cortex-x1/x2/x3 **and cortex-a55** — the SM8250 has four
A55s, so the ETK rig is squarely in scope.

## Why it lives here

Provenance: `ARMSX2/ARMSX3` @ `f707458b0`, path
`3rdparty/llvm/armsx3-aarch64-ghc-emergency-spill.patch` (copied verbatim; target tag
llvmorg-22.1.8 == the CI image's `STATIC_LLVM_VER`). In that tree the patch is applied by
**nothing**: the LLVM submodule is pinned at stock `llvmorg-22.1.8`, no build script
references the file (the author patched a local checkout by hand), and our
`.ci/build-linux-aarch64.sh` lane links the container's prebuilt `/opt/llvm` while
explicitly excluding the llvm submodule from init. A fresh clone builds WITHOUT the fix.

So the fix is baked into the toolchain image instead: `scripts/build-image-etk.sh` clones
upstream `RPCS3/rpcs3-docker`, injects a `patch -p1` step into the jammy-aarch64
Dockerfile's LLVM build (anchored on the exact extract line; refuses to guess if upstream
reshapes it; the image build fails loudly if the patch stops applying), builds
`etk-rpcs3-jammy-aarch64:llvmspill-22.1.8`, and re-tags `:local` so existing mint
invocations pick it up unchanged.

## Sentinel

`rpcs3/Emu/Cell/PPUThread.cpp:5641` (ARMSX3 tree) logs
`LLVM: Retrying module %s with allocator-friendly codegen` when the in-code fallback fires —
its comment states that if this ever appears, the LLVM patch has been lost. Every deploy's
RPCS3.log must stay silent on that string; `config/crash_signatures.json` classifies it as
TOOLCHAIN_REGRESSION (page the operator).
