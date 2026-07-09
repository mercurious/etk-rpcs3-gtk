# ETK RPCS3 — GTK Edition (downstream patch set)

A small, field-tested downstream patch set on top of [RPCS3](https://github.com/RPCS3/rpcs3)
(the open-source PlayStation 3 emulator), built for **glibc/ROCKNIX aarch64** on the
Snapdragon 865 / Adreno 650 class (Retroid Pocket Flip 2) and tuned against Gran Turismo
workloads. It is the emulator half of the **[ETK](https://github.com/mercurious/etk)** (Emulation
Tuning Kit) stack; the driver half is **[etk-turnip-gtk](https://github.com/mercurious/etk-turnip-gtk)**.

This repository exists to **publish the source and reproduce the build** — it is a development
and tuning record. The ROCKNIX AppImage ships as an asset of the matching
[ETK release](https://github.com/mercurious/etk/releases) for use with the kit; everything needed
to build the identical binary yourself is in [`BUILDING.md`](BUILDING.md).

**macOS build available** — a native Apple Silicon / MoltenVK build, distributed directly from
this repo's [Releases](../../releases) as a ready-to-run `.app` (unlike the ROCKNIX AppImage,
which ships via the main ETK release). Useful as a rig-free field-test harness and as its own
cross-platform validation of the road-flicker fix — see [`BUILDING.md`](BUILDING.md#target-macos--apple-silicon)
to build it yourself.

**Windows x64 build available** — a native MSVC build against AMD's Vulkan driver, distributed from
this repo's [Releases](../../releases) as a ready-to-run `.zip`. See
[`BUILDING.md`](BUILDING.md#target-windows-x64-msvc) to build it yourself (four toolchain traps are
documented there; none are in RPCS3 or the patch).

Both desktop builds are a **stopgap distribution channel** pending the upstream RPCS3 PR for
[#11912](https://github.com/RPCS3/rpcs3/issues/11912); once that lands, the fix reaches everyone
through mainline RPCS3 and this channel is no longer the recommended path for it.

## Road-flicker fix — cross-platform validation

The same root-cause fix (`GTK_REMAP0_ONE`: decode a parked shadow-TIU's fully-zeroed `CONTROL1` as
ONE×4 rather than FORCE-ZERO×4) is confirmed clean on **three distinct GPU vendors and three
different Vulkan implementations** — about as strong a "this isn't a driver quirk" signal as the
project can offer. On each, the fix's log line fires on the identical parked-TIU signature
(`CONTROL1 raw = 0x00000000`, 1×1 texture):

| Platform | GPU / API | Result |
|---|---|---|
| ROCKNIX (aarch64) | Adreno 650 / Turnip (native Vulkan) | road clean — Eiger, Daytona, HSL, Fuji |
| macOS | Apple M1 / MoltenVK (Vulkan-over-Metal) | road clean — same tracks |
| Windows x64 | AMD Radeon RDNA2 / AMD native Vulkan | road clean — Daytona, `TIU15 raw=0x00000000` |

The fix has additionally been ported unchanged into two independently-maintained RPCS3-derived
Android codebases ([aPS3e](https://github.com/mercurious/aps3e), RPCSX "Clanker"), both on the same
Adreno 650 hardware — evidence the patch is *portable and correct across trees*, though not new
hardware data points.

---

## Lineage (downstream fork)

> **Downstream patch series over RPCS3.**
> Base: commit **`60c9705a`** (github.com/RPCS3/rpcs3 `master`, build `v0.0.41-19544`).
> Upstream is the canonical source; this repository carries the delta as a single reviewed
> cumulative patch per release:
> - `patches/etk-rpcs3-gtk-edition-0.6.0.patch` — 0.6.0 GA (5 files / ~214 insertions)
> - `patches/etk-rpcs3-gtk-edition-0.6.1-dev-tguard-v6.patch` — tguard dev cumulative
>   (14 files / ~388 insertions): everything in 0.6.0 **plus** the tguard device-loss
>   crash-net (v1–v6), trigger top-end calibration, and the audio timeline logger.
> - `patches/etk-rpcs3-gtk-edition-0.6.1-dev-ffs-v4.patch` — **current** dev cumulative
>   (16 files / ~718 insertions): ffs-v3 **plus** anti-lock stage 3b, the FIFO-desync
>   resync net (`GTK_FIFO_RESYNC=1`, budget `GTK_FIFO_RESYNC_TRIES` default 10). A burst
>   of kernel hang-rescues drops many submits' guest-visible effects; the resulting RSX
>   command-FIFO desync *storm* trips RPCS3's own "hopeless" give-up in `recover_fifo()`
>   (20 recoveries in <2 s), which throws and kills `rsx::thread` — leaving the guest
>   deadlocked *alive* (SPUs parked in `get_ch_value` awaiting reports that never land;
>   captured live with a full gdb bundle, distinct from the stage-2 park: the FIFO goes
>   desynced-dead, not idle-empty, so the stage-2 watchdog is blind AND dies with the
>   thread it rides). Stage 3b intercepts that give-up: force-sync ZCULL (unpark the
>   polling SPUs; composes with the driver's query-survive) and run RPCS3's existing
>   queue-reset recovery instead of throwing, bounded to N consecutive give-ups; only on
>   true exhaustion does it stop cleanly (ledger row, no R3). Never touches the config
>   `as_ps3` accurate-death path. Default-off.
>   **Corrected 2026-07-09:** this file was published missing `RSXTexture.cpp` entirely —
>   the #11912 road-flicker fix (present in `0.6.0` and `tguard-v6`) had been dropped
>   silently when the ffs series was generated. Restored by diffing the correct `tguard-v6`
>   tree against the as-published `ffs-v4` tree and reconstructing the merge (verified: the
>   flicker fix's insertion point in `RSXThread.cpp` doesn't overlap any ffs-specific hunk).
>   The `v0.7.0` ETK release binary already had both fixes correctly combined — only this
>   *published patch file* was out of sync with what was actually shipped.
> - `patches/etk-rpcs3-gtk-edition-0.6.1-dev-ffs-v3.patch` — prior dev cumulative
>   (14 files / ~548 insertions): ffs-v2 **plus** anti-lock stage 2, the RSX progress
>   watchdog (`GTK_RSX_WATCHDOG=1`, threshold `GTK_RSX_WATCHDOG_S` default 10 s): after a
>   kernel hang-rescue drops a guest-visible completion, the game can park polling for it
>   with the FIFO permanently empty (proven live twice, cross-title, identical gdb
>   signatures — no VkFence stale, so the fence net is blind). The watchdog detects
>   in-game FIFO starvation, force-syncs ZCULL (pending reports write their sinks; dead
>   queries forge zero via the GTK driver's query-survive instead of blocking), and if the
>   FIFO still doesn't move by 2x threshold, stops emulation cleanly — a ledger row and an
>   auto-relaunch instead of a frozen handheld. Default-off.
> - `patches/etk-rpcs3-gtk-edition-0.6.1-dev-ffs-v2.patch` — prior dev cumulative
>   (~459 insertions): ffs-v1 **plus** the player-facing rescue notice via RPCS3's
>   native overlay — "GTK RESCUE: RECOVERING FROM CRASH - STANDBY, KEEP AT THE
>   CONTROLS" queues the moment a fence stall crosses 1.5 s and physically appears
>   at the first frame after the rescue (which is exactly when control returns),
>   followed by "CRASH ABSORBED - DRIVE ON" whether the kernel recovered the fence
>   alone or the force-signal fired.
> - `patches/etk-rpcs3-gtk-edition-0.6.1-dev-ffs-v1.patch` — prior dev cumulative
>   (~424 insertions): everything above **plus** the fence force-signal
>   "drive-through" (`GTK_FENCE_FORCE_SIGNAL=1`, threshold `GTK_FENCE_FORCE_SIGNAL_MS`,
>   default 5000 ms): under a kernel that keeps the GPU context alive across a hang
>   recovery (rocknix-gtk `msm.context_keepalive=1`), a fence stuck NOT_READY past
>   the threshold is force-completed — one garbage frame and the race continues,
>   instead of a frozen game or a teardown. Covers both the bounded tguard waits
>   (inside `vk::wait_for_fence`) and the hard-sync present-queue drain (`poke()`),
>   the live-proven GT HD freeze site. Default-off: flag unset = tguard-v6
>   fast-exit behavior, so the two crash policies A/B without a rebuild.

Kept as a thin patches repo (matching the etk-turnip-gtk convention) rather than a full
source-tree fork: the delta is small, reviewable in one sitting, and applies cleanly with
`git apply`.

## What this adds (all changes, one table)

| Area | Files | What it does | Runtime gate |
|---|---|---|---|
| **GT5P road-shadow flicker FIX** | `RSXTexture.cpp`, `RSXThread.cpp` | Decodes a parked shadow TIU's zeroed `CONTROL1` remap as `ONE×4` (hardware-matching), curing upstream bug [#11912](https://github.com/RPCS3/rpcs3/issues/11912) on GT tracks; plus the diagnostic TIU-transition probe used to localize it | **ON by default** (0.6.0 GA); `GTK_REMAP0_ONE=0` kill-switch, `GTK_REMAP0_IDENTITY=1` A/B variant, `GTK_PROBE_11912=1` dev log |
| **Bounded fence waits** | `VKPresent.cpp`, `vkutils/commands.cpp` | Converts two unbounded `wait_for_fence()` spins into bounded timeouts so a GPU device-loss can become a clean stop instead of a silent wedge | always on |
| **Audio telemetry** | `cellAudio.cpp` | Counters for backend underruns, skipped/silent audio periods and time-stretch depth (otherwise fully silent in RPCS3 — no log at any level), exported to `/dev/shm/rpcs3_audio_stat` every ~2 s plus a wall-clock-stamped timeline log; zero behavior change | always on (Linux only) |
| **tguard: device-loss crash-net (v1–v6)** | `vkutils/shared.{h,cpp}`, `vkutils/swapchain.cpp`, `VKGSRenderTypes.hpp`, `rpcs3.cpp`, `Utilities/Thread.cpp`, `Emu/System.cpp` | On an a6xx GPU hang the kernel recovers the GPU but userspace historically spun forever (silent freeze, manual kill required). The net: a sticky device-lost latch set at first sight of `VK_ERROR_DEVICE_LOST`; the fence-status poke routes error results into the standard handler instead of reading them as "pending" (v5); the fatal-log escalation kills instead of pausing (v2); swapchain teardown skips driver calls on a dead device (v1); and `Emu::Kill` on a lost device exits the process immediately (v6) — a wedge becomes a ~1 s detected stop with the frontend back in seconds. Pairs with the driver-side detection in [etk-turnip-gtk](https://github.com/mercurious/etk-turnip-gtk) | always on; inert unless the device is actually lost |
| **Trigger top-end calibration** | `PadHandler.cpp`, `pad_config.h` | Per-pad analog-trigger max calibration so hardware that saturates below 255 still reaches full throttle | config-gated |

## Validation status (honest, per patch)

- **#11912 remap fix** — field-validated: Eiger and Daytona 100% flicker-clean over multiple
  sessions, with a small perf gain; the flicker had been open upstream for 5 years. Independently
  re-confirmed clean across Eiger, Daytona, HSL, and Fuji on **macOS / Apple M1 via MoltenVK**
  (2026-07-09) — a different Vulkan implementation and silicon vendor from the ROCKNIX/Adreno
  result, and about as strong a "this isn't a driver quirk" signal as this project can offer.
- **Bounded fence waits** — deployed and swap-tested; the target a6xx wedge class has so far
  presented via a *different* spin site (occlusion-query wait), so these timeouts have not yet
  converted a real hang. Carried as harm-free hardening while the correct site is chased.
- **tguard v1–v6** — field-validated end-to-end (2026-07-05, multiple live a6xx wedges on
  hardware): with the paired driver-side detection, real GPU hangs now end in a detected stop
  within ~1 s, an immediate process exit, and an attributed session-ledger entry — no manual
  intervention. The chase that got here (three falsified detector designs, five unguarded
  poll sites found by live-process register reads) is documented in the patch comments.
- **Audio telemetry** — validated live: counters proved the GT5P audio stutter is a
  production-side deficit (~15–27% of periods missed under load) with a clean delivery layer
  (zero backend underruns), redirecting the whole tuning campaign.

## AI usage

Developed and maintained with **Anthropic Claude** (Claude Code) as the engineering side of the
ETK garage, with all changes field-validated on hardware by the human operator. Stated here in
the spirit of upstream projects' AI-disclosure practices.

## License

RPCS3 is licensed **GPL-2.0** — see [`LICENSE.md`](LICENSE.md). This patch set is published
under the same license; applying it produces a derivative of RPCS3 and inherits upstream's terms.
