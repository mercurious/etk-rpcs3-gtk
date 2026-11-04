# RPCS3 GTK Edition — 0.11.0 provenance (back on upstream RPCS3)

`etk-rpcs3-gtk-edition-0.11.0-dev.patch` = `git diff b19cedaaf..<tree>` — a cumulative diff on
**upstream `RPCS3/rpcs3` @ `b19cedaaff84abc12afc6a27b58a16d370d55156`** (2026-10-29). Apply exactly one.
Self-identifies as `GTK Edition v0.11.0 (rpcs3-b19cedaaf)`.

| | base | lineage |
|---|---|---|
| 0.6–0.8.x | `a1deb2921` (v0.0.41-19638) | upstream RPCS3 |
| 0.9.0–0.10.2 | `a74a0f3e0` → `8290349e5` (ARMSX3 1.0.4) | ARMSX3 fork |
| **0.11.0** | **`b19cedaaf`** | **upstream RPCS3** |

## Why the lineage moved back (operator decision 2026-11-03)

Measured 2026-11-03 on a full-history clone of both trees:

- ARMSX3 has shipped 1.1 → 1.6.2 since our base (2026-10-10 … 11-03), 433 commits of its own. By the
  files they touch, ~76 % are the Android app (RPCN, remote play, toy portals, SAF saves). The core
  work is an OpenGL ES renderer for Adreno's Android blob (SPIR-V → GLSL ES, ANGLE), Vulkan
  robustness, and per-game fixes. None of the GL ES work reaches a Turnip/Vulkan rig.
- ARMSX3's last real merge of upstream is `961c86fbb` (2026-09-11). Since then it cherry-picks: of
  577 upstream commits, ~380 are carried (some adapted), ~197 are not, ~50 of those in the core
  (Elad's SPU accuracy series, RSX FIFO GET ordering, LV2 PPU thread-start sequencing, Malcolm's
  ARM64 CASPAL atomics). Only 2 ARMSX3 commits have gone back upstream.
- Nobody desktop-builds ARMSX3 but us: every base bump cost a Linux build-fix series (runbook A.1
  item 4). Upstream builds on Linux natively.
- Upstream's own ARM64 attention is rising (NEON upload/compaction paths, LSE2 runtime detection,
  CASPAL 16-byte RMWs, Adreno/Turnip driver recognition).

The 0.9.0 gate result that justified ARMSX3 (GT5P block-survive 25 → 6 → 1) compared it with a
v0.0.41-era upstream that predates that ARM64 work. **0.11.0 exists to re-run that comparison
against today's upstream.**

## Why this base and not the tip

`b19cedaaf` is the last upstream commit before kd-11's RSX re-architecture landed on 2026-10-29
(`17ca01610` … `baa7025a2` … `6050bd2a5`: the renderer backend pulled out of `rsx::thread`,
multi-context RSX, dynamic renderer swap with its own device-loss recovery — 240 files,
+11.6k/−5.9k). Every GTK RSX/Vulkan hook (flip, present, fence, FIFO nets, Pitlink EYES/CLOCK)
sits on code that refactor moved, and it was five days old. Porting onto it would make the
comparison test "upstream vs ARMSX3" and "a brand-new RSX core" at the same time. Crossing the
refactor is the NEXT bump, on its own; upstream's device-loss recovery will then need fitting to
tguard (the latch must clear when a replacement device is created, and quit-on-lost must move to the
unrecoverable branch).

The base still carries 419 upstream commits that ARMSX3 1.0.4 does not have (by patch-id, from
`961c86fbb`), among them: SPU LLVM ARM64 float-to-int saturation, the USER_COMMAND fixes
(`ff40118b9`, `99a926d27`, `56237b685`), LV2 PPU-thread start sequencing (`c82e59895` … `8e4659a6c`),
the RSX FIFO GET ordering series (`982337435`, `a3dc7e6af`, `acb249f05`, `0e0cdc7c2`, `43a42d1a8`),
Elad's SPU series (atomic 128-byte GET/PUT, MFC shuffling, SPU_EVENT_TM), CASPAL atomics
(`cb2179061`), and upstream's FMV-audio rework of the untouched-period logic (`7390f3dd9`).

## Method

1. Worktree at `b19cedaaf` in a clone holding BOTH histories (ARMSX3 + RPCS3), so `git apply --3way`
   could use the ARMSX3 base blobs: the 0.10.2 cumulative merged with 26 conflicted files.
2. Each component then judged on its own: **carry** (a GTK feature), **adapt** (a GTK feature whose
   0.10.2 form was fitted to ARMSX3 code: use its upstream-native form from the 0.8.4 cumulative,
   applied 3-way against the 19638 blobs), or **drop** (it only existed to adapt to ARMSX3).
3. Two upstream files are CRLF (`nv406e.cpp`, `vkutils/swapchain.cpp`) where ARMSX3 is LF: merged
   LF-normalized, written back CRLF so the diff carries only real changes.
4. Identifier census: every `etk_*`, `GTK_*`, `g_etk*`, `pitlink*`, `mark/is_device_lost` and
   `rpcs3_*_stat` token in the 0.8.4 and 0.10.2 cumulatives is present (the only absences are locals
   renamed by the upstream-native semapark and fence forms).

## Component verdicts

| component | verdict | note |
|---|---|---|
| #11912 remap fix + TIU probe (`RSXTexture.cpp`) | carry | clean |
| tguard latch (`mark/is_device_lost`, shared.cpp/.h, commands.cpp, swapchain destroy guard, stuck-join `_Exit` in Thread.cpp, fast teardown + quit-on-lost) | carry / adapt | the present-path latch returns to its 0.8.x form (upstream sends `VK_ERROR_DEVICE_LOST` from `vkQueuePresentKHR` to the default branch); the ARMSX3 `request_device_lost_shutdown` bridge is gone with that function |
| fence force-signal + rescue overlay (+ overlay-coalesce-notice) | adapt | 0.8.4 upstream-native `wait_for_fence` + the coalesce fix; `poke()` uses upstream's `vkGetFenceStatus` (ARMSX3's `vkWaitForFences(t=0)` poll is not carried) |
| RSX watchdog, FIFO resync, flip force-retire, AV readahead | carry | the watchdog's state variables re-seated where upstream's run loop has no ARMSX3 profiler scope |
| perfstat SHM mirror (`overlay_perf_metrics.cpp`) | carry | clean |
| semaphore park (`nv406e.cpp`) | adapt | upstream still has the plain `spin_on_cacheline_once` loop, so the 0.8.4 form returns: `GTK_SEMA_SPIN_US` (150 µs) is live again; ARMSX3's 500-lap WFE tier is not carried |
| audio stat `rpcs3_audio_stat`, `drop=`, GTK_AUDIO_CONCEAL `conc=` | carry / adapt | counters re-seated into upstream's reworked untouched logic (`expect_untouched`, partial-mix path) |
| **ARMSX3 `14e740513`: 60 ms ring headroom + whole-block drop counter** | **carried from ARMSX3** | `drop=` reads `m_dropped_blocks`, which only ARMSX3 had; ported with its headroom |
| Skip revert (`1d657c4e6`, SPU reduced-loop) | carry | bisect-proven GT5P Spec II boot fatal on aarch64 (CHANGELOG 0.8.1); upstream still carries "Skip" |
| CWX `#undef` (SPUOpcodes.h, SPUAnalyser.h) | carry | harmless; X11 macro clash in desktop TUs |
| Pitlink (`Emu/pitlink/`, pad_thread, RSXThread flip, VKPresent EYES, VKGSRender release) | carry | the tap-slot reset in `reinitialize_swapchain` kept without ARMSX3's framegen lines |
| PINE unlink-race fix | adapt | upstream moved bind/listen into `bind_and_listen()`; the inode is recorded on success |
| `.ci/deploy-linux.sh` `ln -srf` | carry | |
| GT6 sysutil startup mapping | **drop** | upstream never reverted #19467: the file ends identical to upstream |
| GT6 SPU DMA fault path | **drop** | upstream has no unbacked-DMA guard (ARMSX3 `a875e771f`) to exempt |
| GT6 mirror (#19760 backport) | **drop** | upstream commit `222754bfd` is in the base |
| `ETK_CONSTRAINED_HOST` (PPU compile RAM budget, descriptor pool, query-wait backoff) | **drop** | all three gated ARMSX3-only Android code; option and CI flag removed |
| FrameGen gating, Oboe source guard, frame-gen config members, include-case fix | **drop** | ARMSX3 Linux build fixes |
| 32-bit gfx byteswap teardown (`g_gfx_shuffle_32`) | **drop** | the pass does not exist upstream; `verify-markers.sh` now marks it `armsx3`-only |
| savestate version bumps (`lv2_prx_overlay`, `lv2_memory`) | **drop** | upstream's own versions already cover them |

## Candidates for a later cherry-pick from ARMSX3 (not carried; each needs its own evidence)

- `vkWaitForFences(t=0)` instead of `vkGetFenceStatus` in `poke()` — ARMSX3 measured 19.7 ms per
  `vkGetFenceStatus` call "on Adreno" (blob or Turnip not stated). Worth a dyno check on Turnip.
- PPU compile-worker RAM budget / JIT group sizing — only if an 8 GB unit OOMs during PPU precompile.
- ARMSX3's 1.1–1.6 Vulkan robustness (frame-context guards, device-loss renderer recovery) — compare
  against upstream's own recovery at the refactor bump instead.

## Gates

- `scripts/verify-markers.sh` on this patch: PASS (lineage `rpcs3`; `g_gfx_shuffle_32` n/a). The
  pre-change script FAILS this patch (proves the lineage field is what admits it); a copy with a
  marker removed FAILS on both lineages.
- Not compiled on the host (no builds on the M1); the first compile is the forge lane. Pre-mint: an
  independent read-only identifier/semantics review against `b19cedaaf`'s headers (findings folded
  in before the patch was cut).

## Mint notes (operator)

- The node tree's remote is `armsx3`; the lane does no fetch, so `b19cedaaf` must be fetched into
  `~/rpcs3` from upstream first (`provision_node.sh` now adds an `rpcs3` remote for this).
- 11 submodule pointers differ between `8290349e5` and `b19cedaaf` (7zip, FAudio, VMA, openal-soft,
  curl, discord-rpc, fusion, libpng, SDL, rtmidi, wolfssl). The lane's `reset --hard` does not move
  submodules, but `.ci/build-linux-aarch64.sh` runs `git submodule update --init` itself (all but
  llvm, SDL, curl, zlib, which the build takes from the image), so a fetched base is enough.
- The lane bundles `verify-markers.sh` from the fork checkout's CURRENT branch: mint from
  `upstream-0.11.0` (or after it merges), or the old gate fails the build on `g_gfx_shuffle_32`.
- Environment: `FORGE_RPCS3_BASE=b19cedaaf`,
  `FORGE_RPCS3_PATCH=etk-rpcs3-gtk-edition-0.11.0-dev.patch`,
  `FORGE_RPCS3_ARTIFACT=rpcs3-etk_gtk-edition-0.11.0_rpcs3-b19cedaaf_linux_aarch64.AppImage`.

## The test this core exists for

GT5P Spec III block (N≥3, cold boot) on car12: 0.11.0 against the certified 0.10.1, same Turnip,
same config. If 0.11.0 holds, the ARMSX3 lineage retires. If not, the gap is a list of specific
commits to carry, not a base. First boot recompiles every PPU/SPU cache: never count first-boot rows.
