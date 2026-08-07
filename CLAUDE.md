# CLAUDE.md — etk-rpcs3-gtk (sister repo of the ETK ecosystem)

**Mother repo: `~/etk`.** Bootstrap every session there first — read `~/etk/CLAUDE.md`, then
`~/etk/TRACK_MANUAL.md` (§8 deployment, §8.5 the build fleet) before working here. This repo is
one spoke of that system: the **RPCS3 GTK Edition fork** (anti-lock nets, #11912 road-flicker
fix, perfstat channel — default-ON with `=0` kill-switches) for the SM8250 ROCKNIX rig.

## Reading order (this repo)
`README.md` → `BUILDING.md` → `PORTABILITY.md` → `patches/` (the working branch, e.g.
`etk-base-19638`, carries the fork as patches over a pinned upstream base commit — note this
repo does NOT use `main` as its working branch) → `hwtest/` (the #11912 psl1ght lane).

## How artifacts flow (never deviate)
1. **Mint**: `~/etk/forge.sh rpcs3` conducts `~/etk/tools/forge/lane_rpcs3.sh` on **etk-cloud**
   (container `etk-rpcs3-jammy-aarch64:llvm22`, tree `~/rpcs3` on the node). The patch is
   resolved from THIS repo's `patches/*-dev.patch` (newest wins; `FORGE_RPCS3_PATCH` overrides),
   and the packager + gates come from THIS repo's `scripts/package-appimage.sh` +
   `scripts/verify-markers.sh` (MARKER `rpcs3_perf_stat` in the binary; VERIFY = AppImage loads
   with system ffmpeg hidden). Artifact: a named, sha256'd AppImage staged to `~/etk/emulators/`.
2. **Deploy**: only the operator, only via `~/etk/install.sh` (STEP 6.55 binds the AppImage,
   6.56 env flags; `RPCS3_APPIMAGE` knob in `~/etk/etk.conf`). Cold-boot gated. Claude NEVER
   contacts the rig from anywhere but the Air, and never reboots it.

## Non-negotiables inherited from the mother repo
- Always-reboot gate; verify the LIVE process (`/proc/PID/environ`, sha256 of the staged file —
  two builds once size-collided byte-for-byte; ENOSPC once kept the old emulator silently).
- Every fork feature is a runtime flag with a kill-switch so A/B needs no rebuild.
- Public artifacts under the **mercurious** pseudonym; docs stay development/tuning-focused.
- Upstream comments are posted by the operator, never directly.
