# RPCS3 GTK Edition — 0.10.0 provenance (GTK Pitlink)

`etk-rpcs3-gtk-edition-0.10.0-dev.patch` = `git diff 8290349e5..<tree>` — a cumulative diff on
**`ARMSX2/ARMSX3` @ `8290349e5`** (tag `1.0.4`), the same base as 0.9.1.x. Apply exactly one.
Self-identifies as `GTK Edition v0.10.0 (armsx3-8290349e5)`. Numbered to match the **GTK 0.10.0**
October release it ships in (operator decision 2026-10-10), not as a 0.9.x patch level.

| | base | ETK delta over the previous core |
|---|---|---|
| 0.9.1.2-dev | `8290349e5` | exit-teardown fix (see PROVENANCE-0.9.1.md) |
| **0.10.0-dev** | **`8290349e5`** | 0.9.1.2 carried verbatim **+ GTK Pitlink + the PINE unlink-race fix** |

## GT6 boot fixes (2026-10-11, folded in)

`0.10.0-dev` now also carries the two Gran Turismo 6 (BCUS98296) boot fixes found by the first ETK hunts
(`etk-dossiers/Hunt_GT6Bisect_20261010.md`). Each also ships as its own file for review:
- **`gt6-sysutil-startup-mapping.patch`**: restores the sysutil startup shared-memory mapping from upstream #19467
  WITHOUT its cellAudio half. ARMSX3 `e24761512` reverted all of #19467 (for Mortal Kombat KE's WB-logo hang). That
  moved every `sys_mmapper` area 256 MB lower and deadlocked GT6 at boot (convicted by bisect, confirmed by m10).
- **`gt6-spu-dma-fault-path.patch`**: the unbacked-DMA guard (ARMSX3 `a875e771f`, for Killzone 3) dropped an SPU PUT
  to a page GT6 maps on demand, and GT6 read a null pointer 3 s later. Pages in an area armed for page-fault
  notification now go to the fault path. KZ3's bad addresses can't match such an area.
Validated: GT6 boots to My Home and races (car8 and car12, 2026-10-11). Not tested in the garage: Killzone 3
and MK KE (not owned; the 0.10.0 notes ask the community). Known issue: a rare gcm user-interrupt race (null +
0xc in GT6's handler at `0x00b7ba80`), see the hunt dossier.

## Why it exists

The 2026-10-10 session tried to give an agent eyes and hands on a running GT5P using only what
the OS and stock RPCS3 offer. RAM worked first time (`/proc/<pid>/mem` on `vm::g_sudo_addr`).
Everything else fought back: InputPlumber 0.79's `SendEvent` panics in its own runtime; a uinput
pad bound by `--input-config` reached RPCS3 (its home menu obeyed) but GT5P ignored every
pressure-class button; `grim` gives compositor frames with no frame identity; PINE lost its
socket file. Operator: *we own this chassis — never fight it, rewrite it.* Pitlink puts the
agent at the PS3 boundary instead. Design + wire protocol: ETK `docs/PITLINK_SPEC.md`.

## What it adds (all behind `GTK_PITLINK=1`; unset = one relaxed load per hook)

- **`rpcs3/Emu/pitlink/pitlink.{h,cpp}`** (new, in `rpcs3_emu`) — the server: abstract Unix
  socket `@etk-pitlink` (no path, so no second Init can unlink it) + optional TCP
  `GTK_PITLINK_TCP=addr:port | iface:port` (retried every 5 s until the interface is up). PLNK v1:
  HELLO/roles/token, PAD, VIDEO/SNAP, RAM_READ/RAM_WRITE/WATCH, PAUSE/RESUME/STEP/EXIT, STATUS,
  pushed FRAME and EVENT. Process singleton (`std::call_once` from `Emulator::Init`).
- **HANDS** — `Input/pad_thread.cpp`: `pitlink::apply_pad()` right after `apply_copilots()`,
  under `pad::g_pad_mutex`, writing digital bits **and pressure** into `m_buttons_external` /
  `m_sticks_external` — the copy `cellPadGetData` reads. Modes off/merge/exclusive; merge keeps
  the human pad live (an agent axis wins only off centre). `at_flip` scheduling; TTL dead-man
  (default 500 ms) returns the port to neutral.
- **CLOCK** — `Emu/RSX/RSXThread.cpp` `thread::flip`: `pitlink::on_flip()` (flip counter, fps
  EMA, STEP budget → `Emu.Pause(false, false)` from the main thread).
- **EYES** — `Emu/RSX/VK/VKPresent.cpp` / `VKGSRender.{h,cpp}`: at the flip, one downscale
  `vkCmdBlitImage` + `copy_image_to_buffer` into a 3-slot ring, recorded into the frame's own
  command buffer and **harvested in `frame_context_cleanup` after the swap wait the frame already
  pays** — no `flush_command_queue(true)` (the screenshot/recording path's per-frame hard sync),
  no fence poke (which blocks on this driver). Pre-overlay game image; WATCH bytes sampled at the
  same flip ride the frame. Tap resources released in `~VKGSRender` after `vkDeviceWaitIdle`.
- **PINE fix** — `3rdparty/pine/pine_server.h`: record the socket file's dev/inode at bind;
  `Cleanup()` unlinks only that file. Root cause (car8, 2026-10-10, core 0.9.1): `Emulator::Init`
  runs twice at a `--no-gui` boot; the first IPC server's teardown unlinked the second's live
  socket (`IPC: Starting server` twice, 18 ms apart; no `rpcs3.sock`).
- `rpcs3_version.cpp`: `GTK Edition v0.10.0`.

## Gates

- `scripts/verify-markers.sh` now also requires `GTK_PITLINK`, `etk-pitlink` (both in the
  binary), `pitlink_record_tap`, `m_socket_ino` (patch-only).
- Not compiled before the mint by policy (no builds on the M1); the first compile is the
  etk-cloud forge lane. Pre-mint review: an independent read-only compile/semantics review of
  every identifier against the base's headers.
- Acceptance (spec §7 P2/P3): flag-off behaviour identical to 0.9.1.2; with the flag on, GT5P
  obeys X / d-pad / stick / pressure from HANDS; EYES p95 flip→host ≤ 50 ms at 640×360@30 over
  USB-net; zero added hard syncs; dyno A/B tap on/off within noise (N≥3).
