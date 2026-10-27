# RPCS3 GTK Edition — 0.10.2 provenance (GTK_AUDIO_CONCEAL)

`etk-rpcs3-gtk-edition-0.10.2-dev.patch` = `git diff 8290349e5..<tree>` — a cumulative diff on
**`ARMSX2/ARMSX3` @ `8290349e5`** (tag `1.0.4`), the same base as 0.9.1.x, 0.10.0 and 0.10.1. Apply exactly one.
Self-identifies as `GTK Edition v0.10.2 (armsx3-8290349e5)`.

| | base | ETK delta over the previous core |
|---|---|---|
| 0.10.1-dev | `8290349e5` | 0.10.0 + upstream RPCS3 #19760 (GT6 mirror) (PROVENANCE-0.10.1.md) |
| **0.10.2-dev** | **`8290349e5`** | 0.10.1 carried verbatim **+ GTK_AUDIO_CONCEAL** (`audio-conceal-untouched.patch`) |

## The sub-native-fps "crunch" (2026-10-27)

Measured on car12, an operator-driven Eiger lap on GT HD Concept (NPEA90002), 1 Hz log of the
fork's own `rpcs3_audio_stat` beside `rpcs3_perf_stat`: over 298 s at an average 51 fps on a
60 fps title, `sil` rose by 11,911 (~40/s), `skip` by 148 (0.5/s), `ur` 0, `drop` 0. Per 2 s
sample the `sil` delta tracked the frame deficit (~100 at 50 fps, 140–170 at 38 fps, ~0 at
60 fps). The operator's one clean corner was the one place the game held 60 fps; "crunch is
readable on the frametime graph".

Mechanism: the game fills its audio once per rendered frame, so every frame it misses below 60
leaves a 5.33 ms cellAudio period unwritten. Stock cellAudio (`untouched == active_ports` →
`enqueue_silence()`) ships each as silence — a hard gate tens of times a second, a buzz riding
the engine note. The silence also keeps the ring full, so the title's enabled time stretching
(threshold 100, 250 ms buffer) never engaged: `ratio` stayed 1.000 all lap. Not throughput,
not the sink, not skips, drops or underruns.

## The change (`rpcs3/Emu/Cell/Modules/cellAudio.{h,cpp}`, new `etk_audio_conceal.h`, `rpcs3_version.cpp`)

- `audio_ringbuffer::enqueue_untouched()` replaces `enqueue_silence()` in that one branch. While
  a run cap allows, the unwritten period is filled from the last block the game DID write
  (copied in `enqueue()` before `commit_data()` downmixes the mixer buffer in place), ping-pong:
  reversed first (its first sample equals the last sample already shipped), forward next, so
  every junction inside a run is sample-continuous and the block keeps the signal's spectrum.
  The last block under the cap fades linearly to zero; past the cap, and after any true silence
  (startup fill, no active ports), stock silence. When real audio resumes after a run, a 64-frame
  linear offset ramp (1.33 ms) lands its first frame on the last sample shipped — no dip, no gain
  change. The arithmetic is a dependency-free header so `tests/test_audio_conceal.cpp` pins it on
  the host (continuity, fade, cap, ramp, amplitude bound, degenerate geometry).
- Switches (read once per ringbuffer, like every GTK_* runtime switch; delivered through
  `RPCS3_ENV_FLAGS` in `etk.conf`): `GTK_AUDIO_CONCEAL=0` restores stock silence exactly;
  `GTK_AUDIO_CONCEAL_MAX=N` (1–64, default 8 blocks = 42.7 ms) sets the run cap. Default ON.
- Telemetry: `conc=` appended after `drop=` on the `rpcs3_audio_stat` line (concealed periods, a
  subset of `sil=`, which keeps its meaning: periods the game never wrote, shipped in its place).
  Every earlier key keeps its place; `tools/test_audio_stat.py` in ETK pins the new shape and
  that a cell without `conc=` is a pre-conceal build, never zero. A `cellAudio.notice` line names
  the switch state and cap at every audio (re)configuration.
- Markers: `GTK_AUDIO_CONCEAL`, `conc=%llu` (scripts/verify-markers.sh).

Validation target: car12, GT HD Concept Eiger, the operator's ear with headphones on, the same
1 Hz log. Pass = `sil` still ~40/s and `conc` ≈ `sil` while the crunch is gone or softened to a
frame-rate-correlated smear; the 60 fps corner unchanged. Falsified if the buzz is unchanged
with `conc` tracking `sil` (the gate is not the audible fault), or if `conc` stays 0 (the
periods arrive through another branch). Control: `GTK_AUDIO_CONCEAL=0` in `RPCS3_ENV_FLAGS`,
relaunch, same lap — stock sound on the same binary. Expected to carry up the series: GT5P /
GT6 ship the same silence at a worse deficit, with real skips and underruns on top that this
does not address.
