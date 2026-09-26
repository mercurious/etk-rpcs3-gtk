# RPCS3 GTK Edition — 0.9.1 provenance (base bump onto ARMSX3 Release 1.0.4)

`etk-rpcs3-gtk-edition-0.9.1-dev.patch` = `git diff 8290349e5..<tree>` — a cumulative diff
on **`ARMSX2/ARMSX3` @ `8290349e5`** (tag `1.0.4`, "Release 1.0.4", 2026-09-26). Apply exactly
one. Self-identifies as `GTK Edition v0.9.1 (armsx3-8290349e5)`.

| | base | commits over previous base | ETK delta |
|---|---|---|---|
| 0.9.0.3-dev | `a74a0f3e0` (2026-08-24) | — | 35 files, 1324+/50− |
| **0.9.1-dev** | **`8290349e5`** (2026-09-26) | **692** | 35 files, 1326+/53− |

## What the base moved

692 commits `a74a0f3e0..8290349e5`, linear (the old base is an ancestor):

- **530 fork commits** (468 by jpolo1224); 221 of them touch only `android/`.
  Core areas: `rpcs3/Emu/RSX` (164 commits), `rpcs3/Emu/Cell` (131), `Utilities` (22).
- **4 upstream merges** absorbing **158 RPCS3 commits**; ARMSX3's last upstream sync point
  is `961c86fbb` (2026-09-11). It also cherry-picks selectively (≈14 later upstream
  commits carried in adapted form).
- No submodule pointer changed; no new desktop build dependency.

Base changes the rig gate should watch (mechanism first):

- **Device-loss handling of its own** (`0cd345ce5`, `6849836f9`, `dca8ec2a7`, …): a new
  `rsx::request_device_lost_shutdown()` latch replaces `die_with_error` at six Vulkan
  sites. See "the tguard bridge" below — this is the one merge that was clean and wrong.
- **Interruptible fence waits** (`36c4820b8`): the unbounded fence wait is now 1 s slices,
  abandoned on stop.
- **PPU ARM64 FP correctness** — XER SO/CA transposition, fnmadd/fnmsub sign of zero,
  NaN→int, single-store mantissa truncation, OE overflow flag — each with a PPU
  codegen/cache-key bump. **Every title recompiles its PPU cache on first boot: do not
  count first-boot ledger rows.**
- **SPU reservation / PUTLLC work** (`7abf45dc6` reservation-snapshot fences on weakly
  ordered hosts, `be3f419de`, PUTLLC16 whitelist) — the SPURS-hot path GT5P lives on.
  ARM64 SPU object caching stays OFF, as at the old base (`457a932c7`).
- **kd-11's blit-engine / surface-cache refactor** (`f8b12a209`…`d4b287944`, 28 commits):
  the texture-cache neighbourhood of the #11912 fix. `decoded_remap()` itself is
  untouched and the fix's payload carried byte-identical — but re-confirm the road
  at Eiger/Daytona.
- **cellAudio ring headroom 20→60 ms + dropped-block counter** (`14e740513`): whole
  5.33 ms blocks were silently discarded on post-stall bursts with underruns=0 — the
  same "clean delivery layer" signature our telemetry recorded on GT5P. Not exported to
  `/dev/shm/rpcs3_audio_stat` (only a warning every 64 drops) — see open questions.
- Qualcomm-only VK paths (graphics-pipe byteswap/D24S8 conversions, `b36ab66d5`) gate on
  driver vendor `ADRENO` or `TURNIP` — they apply on the rig. `ARMSX3_GFX_CONVERT=0`
  forces them off for an A/B without a rebuild.
- SPU loop detection default-off migration is Android-app-only; ETK configs already
  carry `false`.

## Method

0.9.0.3-dev committed on `a74a0f3e0`, cherry-picked onto `8290349e5` (3-way, zdiff3).
33 of 35 files auto-merged; **every ETK payload line carried byte-identical** except the
edits listed here.

**Conflicts (2):**
1. `NV47/HW/nv406e.cpp` — the base added `g_stuck_sema_addr` diagnostics where semapark's
   knob block sits. Both kept. Semapark still plugs into the base's post-500-lap tier,
   unchanged.
2. `VK/vkutils/sync.cpp` `wait_for_fence()` unbounded path — the base replaced
   `UINT64_MAX` with 1 s interruptible slices. Merged: `GTK_FENCE_FORCE_SIGNAL=0` is the
   base's stock loop exactly; ON (default) = 250 ms slices + fence aging + force-complete
   at threshold, **plus** the base's abandon-on-stop and 3 s log.

**The tguard bridge (clean merge, semantic regression — fixed).** The base's six new
device-lost sites (fence hot-poll, `wait_for_event`, present, swapchain rebuild, acquire,
occlusion-query poll) call `rsx::request_device_lost_shutdown()` instead of
`die_with_error`, which is where `vk::mark_device_lost()` — the tguard latch — was set.
Unbridged, a GPU hang on those paths would take the base's GUI policy (clean stop, app
stays up) and headless RPCS3 would idle in `exec()` with the frontend never regaining
control: tguard v2/v4/v6 silently disarmed. Fix: `request_device_lost_shutdown()` calls
`vk::mark_device_lost()` first (idempotent CAS). The now-unreachable `DEVICE_LOST` arm in
`present()`'s `default:` was removed.

**Linux build fix.** `vkutils/sync.cpp` gained `#include "Emu/system.h"` (`36c4820b8`);
the file is `System.h`. Resolves on the case-insensitive filesystems ARMSX3 builds on,
fails on Linux. Corrected.

## Pre-mint sweeps (runbook A.1, mint loop item 4)

| sweep | result |
|---|---|
| quoted `#include` resolution, 277 changed desktop sources + whole-tree case check | 1 defect (above), fixed; rest external/generated |
| Android-gated declarations referenced by desktop TUs | clean (3 hits, all declared on both sides) |
| `#else` desktop stub signatures vs headers | clean (FrameGen stubs match, incl. 0.9.0.1's 0010 fix) |
| new mobile-profile gates since base | one candidate NOT widened (VK data-heap 256 MB cap, `9bd3410b6`) — open question |

The sweeps prove resolution and declaration, not semantics; lld's undefined-symbol list
in the forge lane is the exhaustive check.

## Verification (host, 2026-09-26)

- Clean `8290349e5` worktree: `git apply --check` OK → apply → `git diff` **byte-identical**
  to the patch → `git apply -R --check` OK.
- `scripts/verify-markers.sh`: 13/13; hit counts equal 0.9.0.3 except
  `GTK_FENCE_FORCE_SIGNAL` 5→6 (the merged fence-wait comment).
- `clang++ 22.1.8 -std=c++23 -fsyntax-only` (`ETK_CONSTRAINED_HOST`, X11+Wayland):
  `nv406e.cpp`, `vkutils/sync.cpp`, `RSXThread.cpp` clean. `VKPresent.cpp` not checked
  here (needs ffmpeg headers); its change is a deletion of an unreachable branch.
- Not compiled or linked end-to-end: that is the forge lane.
- Build node `~/rpcs3` resting state = 0.9.0.3 v2 content exactly (the published
  0.9.0.3 patch carries a stale `rpcs3_version.cpp` index hash from the hand-edited
  literal bump in `e1398af`; content identical). The preflight reset is safe.

## Upstream RPCS3 not in 1.0.4 (81 commits, to `77cb9423d`, 2026-09-26)

None required for the rig; none carried. Take them through ARMSX3's next upstream merge
rather than into this patch. Checked: Adreno/Turnip detection (`fdffb7d5f`) — ARMSX3 has
its own; ARM64 CFLTS saturation (`e13ee1579`) — ARMSX3's original is already in the old
base; Linux affinity reset (`6e6ffc505`) — no-op under the rig's OS scheduler. Notable
for a later bump: kd-11 USER_COMMAND fixes (`ff40118b9`, `99a926d27`, `56237b685`); Elad's
LV2 PPU-thread start sequencing (`c82e59895`…`77cb9423d`, timing-sensitive, days old).

## Open questions (operator)

1. **Audio drop counter → telemetry.** Export `m_dropped_blocks` into
   `rpcs3_audio_stat` so the ledger can see the class of click the 60 ms headroom
   targets. New field = a parser change on the ETK side; not folded into this rebase.
2. **VK data-heap cap.** Widen the Android-only 256 MB ring ceiling to
   `ETK_CONSTRAINED_HOST`? Only on evidence of heap-growth failure on the rig.
3. **`-nofbl` shader-cache tag** is Android-only; DRIVER-tab Turnip swaps that toggle
   feedback-loop support would share a cache on the rig. Watch, don't pre-empt.
