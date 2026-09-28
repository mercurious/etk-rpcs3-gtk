# RPCS3 GTK Edition — 0.9.1 provenance (base bump onto ARMSX3 Release 1.0.4)

`etk-rpcs3-gtk-edition-0.9.1-dev.patch` = `git diff 8290349e5..<tree>` — a cumulative diff
on **`ARMSX2/ARMSX3` @ `8290349e5`** (tag `1.0.4`, "Release 1.0.4", 2026-09-26). Apply exactly
one. Self-identifies as `GTK Edition v0.9.1 (armsx3-8290349e5)`.
`etk-rpcs3-gtk-edition-0.9.1.1-dev.patch` is the same base plus the audio drop counter
(addendum below). It self-identifies as `GTK Edition v0.9.1.1 (armsx3-8290349e5)`.
`etk-rpcs3-gtk-edition-0.9.1.2-dev.patch` is 0.9.1.1 plus the exit-teardown fix (second
addendum). It self-identifies as `GTK Edition v0.9.1.2 (armsx3-8290349e5)`.

| | base | commits over previous base | ETK delta |
|---|---|---|---|
| 0.9.0.3-dev | `a74a0f3e0` (2026-08-24) | — | 35 files, 1324+/50− |
| 0.9.1-dev | `8290349e5` (2026-09-26) | **692** | 35 files, 1326+/53− |
| 0.9.1.1-dev | `8290349e5` | 0 (same base) | 35 files, 1335+/53− |
| **0.9.1.2-dev** | **`8290349e5`** | 0 (same base) | 36 files, 1349+/53− |

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
- **Minted clean — forge run `20260926-161125` (etk-cloud, 2026-09-26).** `8290349e5` +
  this patch compiled and linked with no build fixes; the packaged AppImage passes VERIFY
  without system ffmpeg; `verify-markers` 13/13 in the patch and 12/12 in the built ELF
  (`is_device_lost` is a symbol, patch-only by design); the ELF carries
  `GTK Edition v0.9.1 (armsx3-8290349e5)`; LANE OK, release_sanity PASS. Artifact
  `rpcs3-etk_gtk-edition-0.9.1_armsx3-8290349e5_linux_aarch64.AppImage`, 80,446,399 B,
  sha256 `98966af6a1ebe6ad98434f0757851e1154e5a07650e7bbb227dea2ea19e6a7d9` (local copy in
  `~/etk/emulators/` matches the node). Staged for testing only; the rig gate is still ahead.
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
   **Resolved by 0.9.1.1-dev** (2026-09-26) — see the addendum below.
2. **VK data-heap cap.** Widen the Android-only 256 MB ring ceiling to
   `ETK_CONSTRAINED_HOST`? Only on evidence of heap-growth failure on the rig.
3. **`-nofbl` shader-cache tag** is Android-only; DRIVER-tab Turnip swaps that toggle
   feedback-loop support would share a cache on the rig. Watch, don't pre-empt.

## Addendum — `audio-drop-counter.patch` (2026-09-26, open question 1)

Appends one key to the aud1 line: `... enq_ms=%.1f buf_ms=%u drop=%llu`. Every earlier key
keeps its name, position and meaning; the new key goes last so positional and `k=v` readers
alike are unaffected.

`drop` counts the same event as the base's `m_dropped_blocks` (the `push() != want` branch
in `audio_ringbuffer::commit_data`), but from ETK's own `s_etk_aud.dropped_blocks`, and not
by reading the member. `m_dropped_blocks` lives on the ringbuffer, which
`cell_audio_thread::update_config()` destroys and rebuilds mid-session (config update,
default-device change, and the not-operational backend retry every 128 loops), so reading
it would silently zero the count. The ETK counter resets only at cellAudio thread start,
like `ur`/`skip`/`sil`, so a cell stays "per guest boot". It is a plain `u64` because all
`commit_data` paths (`enqueue`, `advance` → `process_resampled_data`) run on the cellAudio
thread, as the dump does. The base's every-64 warning is untouched.

Increments over `0.9.1-dev`: 1 file, 9+/2− (cumulative 1326+ → 1333+); **0 marker
changes** (13/13, hit counts identical). The worst-case line, with every integer field at
`u64` max, is 218 bytes against the 256-byte buffer, so it cannot truncate. Verified
2026-09-26: base + `0.9.1-dev` + this diff equals a direct cumulative byte for byte; that
cumulative passes `git apply --check` and `-R --check` on a clean `8290349e5`;
`clang++ 22.1.8 -std=c++23 -fsyntax-only -Wformat=2` on `cellAudio.cpp` is clean, and a
mutant missing the new argument is flagged.

ETK side (mercurious/etk): `tools/etk_dyno.py --audio` gains DROP/min p50 + DROP N (a cell
without `drop=` is unknown, never 0); `tools/test_audio_stat.py` runs both line formats
through the real postmortem aud block and the dyno. `ETK_AUDIO_PRODUCER=<a patch or
cellAudio.cpp>` pins its fixture to the producer. Landed first, as mercurious/etk `9b67632`:
it is host-side only and reads old ledgers unchanged.

**Cut as `etk-rpcs3-gtk-edition-0.9.1.1-dev.patch` (2026-09-26)**, self-identifying as
`GTK Edition v0.9.1.1 (armsx3-8290349e5)`. It was not folded into `0.9.1-dev`: 0.9.1 was
minted from the patch at sha256 `ddfb163ba9f1cebb…` (run `20260926-161125`; see
Verification), and a fold would leave that binary disagreeing with its published patch, the
same identity rot as the 0.9.0.3 v1 incident. `0.9.1-dev` stays final at `92b809a`.

0.9.1.1-dev differs from 0.9.1-dev in exactly two files. `cellAudio.cpp` carries this
addendum's diff. `rpcs3_version.cpp` gets the literal bump plus two comment lines above it.
The cumulative was generated with `git diff --abbrev=9 8290349e5`, never hand-edited.
`scripts/verify-markers.sh` gains `drop=%llu` (since 0.9.1.1). Verified 2026-09-26:
- A fresh clean `8290349e5` clone passes `git apply --check`; after applying, `git diff`
  regenerates the patch byte for byte, and `git apply -R --check` passes.
- verify-markers passes 14/14 on 0.9.1.1-dev.
- `clang++ 22.1.8 -O2 -c` compiles `cellAudio.cpp` and `rpcs3_version.cpp` (the latter with
  a stub `git-version.h`). `strings -a` on the objects finds `drop=%llu` and
  `GTK Edition v0.9.1.1 (armsx3-8290349e5)`, so the lane's binary gate will see both.
- The ETK producer pin passes against the new patch.

Under the new marker list, **0.9.1-dev MISSes `drop=%llu`**, as the list's "every later
patch" rule intends. So re-minting 0.9.1 would need `verify-markers.sh` from `92b809a`.
`audio-drop-counter.patch` stays as the reviewable delta, as `overlay-coalesce-notice.patch`
did; apply exactly one cumulative.

**Minting 0.9.1.1 is a separate, operator-gated step.** `~/etk/emulators/` already holds the
two-core cap (0.9.0.3 CERT + 0.9.1), so one core must be retired first (0.9.1, never the
CERT) or release_sanity fails. `FORGE_RPCS3_*` in `etk.conf` also needs repointing to the
0.9.1.1 patch and artifact name.

## Addendum 2 — `gfx-shuffle32-teardown.patch` (2026-09-27, the 0.9.1 exit hang)

**Symptom (rig, ETK ledger).** On 0.9.1 the emulator wrote its complete graceful-shutdown log
(last line `gui_application: Deleting old game window`) and then the process stayed: 5 of 6
archived exits sat 9 s, 35 s, 79 s, 82 s and 74 min before an L1+R3 or a reboot, across GT5P
Spec III (.pkg), the GT5P disc (.iso) and GT HD. On 0.9.0.x the process was gone 0.5–4.6 s
after that line (N=30 archived exits).

**Mechanism (captured live on BCUS98158 by the ETK spotter's EXIT-HANG class).** One thread
left, uninterruptible, kernel stack `get_signal → vfs_coredump → elf_core_dump →
dump_user_range → ext4 write → balance_dirty_pages`: the process had taken SIGABRT and the
kernel was writing its core (`AppRun.wrapped.<pid>.6.core`, 1.3 GB when R3 cut it; the 13:55
GT5P session left a `.6` core too) to the SD card. The emulator's last stderr line:
`[Vulkan Loader] ERROR: vkDestroyBuffer: Invalid device [VUID-vkDestroyBuffer-device-parameter]`.

**Cause.** `85b7495b9` ("vk: run the 32-bit byteswap on the graphics pipe", 2026-09-11) added
a second global pass, `g_gfx_shuffle_32`, beside `g_gfx_shuffle`, but `clear_resolve_helpers()`
tears down only the first. The pass is Qualcomm-gated (`b36ab66d5`), so it exists only on
Adreno/Turnip and only in sessions that ran a 32-bit byteswap. It then outlives the device:
teardown logs `RSX: Leaking memory allocations!` for exactly its two heaps, the overlays UBO
(`8 * 0x100000`) and VAO (`1 * 0x100000`) in `VMM_ALLOCATION_POOL_SYSTEM` (4 of the 5 hung
exits carry that line; 0 of 31 clean exits across both cores do), and at process exit its static
destructor frees a buffer on the dead device, which the loader answers with `abort()`.

**Fix.** Destroy and reset `g_gfx_shuffle_32` in `clear_resolve_helpers()` exactly as its
twin is. `clear_resolve_helpers()` runs in `destroy_global_resources()` while the device is
alive; `reset()` runs the pass's destructor there, freeing both heaps in time. No runtime
flag: this is a teardown that was missing, not a behaviour to A/B.

**Verification (host, 2026-09-27).**
- Fresh `8290349e5` clone: `git apply --check` OK → apply → `git diff --abbrev=9 8290349e5`
  regenerates the patch **byte-identical** → `git apply -R --check` OK. The 0.9.1.1 cumulative
  round-trips byte-identical on the same base; the two cumulatives differ in exactly
  `VKResolveHelper.cpp` (this fix) and `rpcs3_version.cpp` (literal + two comment lines).
- `clang++ 22.1.8 -std=c++23 -fsyntax-only` (`ETK_CONSTRAINED_HOST`, X11+Wayland) on
  `VKResolveHelper.cpp`: clean. A mutant calling `destroyy()` fails the same check, so it is
  not vacuous.
- `scripts/verify-markers.sh`: 15/15 on 0.9.1.2-dev; 0.9.1.1-dev now MISSes
  `g_gfx_shuffle_32->destroy()` as the "every later patch" rule intends. The marker is
  patch-only (a call, not a string): `BINARY_EXEMPT` became a space-separated list, because
  a `|` coming out of a variable expansion is literal in a `case` pattern. Binary half
  re-checked against the 0.9.1.1 AppImage: both patch-only markers skipped, 13/13 found.
- Not compiled or linked end to end: that is the forge lane.

**What it does not claim.** The 74-minute case (session 1790536279) logged no leak line and
no RSX teardown lines at all, so its abort may have a second owner; the rig's next EXIT-CRASH
catch on 0.9.1.2 answers that. GT6/GT5 not reaching their menus on 0.9.1 is a separate,
deterministic guest-level stall (loader thread in `_sys_lwmutex_lock`, RSX healthy) and is
not addressed here.
