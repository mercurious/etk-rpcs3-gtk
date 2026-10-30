# RPCS3 GTK Edition — 0.10.2 provenance (cellAudio concealment)

`etk-rpcs3-gtk-edition-0.10.2-dev.patch` = a cumulative diff on **`ARMSX2/ARMSX3` @ `8290349e5`**
(tag `1.0.4`), the same base as 0.9.1.x, 0.10.0 and 0.10.1. Apply exactly one.
Self-identifies as `GTK Edition v0.10.2 (armsx3-8290349e5)`.

| | base | ETK delta over the previous core |
|---|---|---|
| 0.10.1-dev | `8290349e5` | 0.10.0 + upstream RPCS3 #19760 backported (PROVENANCE-0.10.1.md) |
| **0.10.2-dev** | **`8290349e5`** | 0.10.1 carried verbatim **+ `GTK_AUDIO_CONCEAL`** (`cellAudio.cpp`, `cellAudio.h`, new `etk_audio_conceal.h`) |

## GTK_AUDIO_CONCEAL (2026-10-27)

A game that fills its audio once per rendered frame leaves a 5.33 ms period unwritten for every frame
it misses below its native rate, and stock cellAudio ships each one as silence: a hard gate, tens of
times a second on a slow title (GT HD Concept on the SM8250, car12: `sil` ~40/s at ~50 fps on a 60 fps
game, zero underruns, zero drops), heard as "crunch". The silence also keeps the ring full, so time
stretching never engages.

The core fills an unwritten period from the last block the game did write, ping-pong (reversed, then
forward, so every junction inside a run is sample-continuous), capped at 8 blocks (42.7 ms); the last
block fades to silence and the first real block after a run ramps from the last sample shipped.

- On by default. `GTK_AUDIO_CONCEAL=0` restores stock silence exactly; `GTK_AUDIO_CONCEAL_MAX=N` (1..64)
  sets the cap.
- The telemetry stat line gains `conc=` at its end. A cell without it is a pre-conceal build, never 0.
- Everything else in the delta is the version string; no other 0.10.1 line changes.

## Recovery of this file (2026-10-30)

This patch was minted on 2026-10-27 and never committed here. It was recovered from etk-cloud's run
bundle `~/forge-runs/20261027-101334/`. That run applied it to `8290349e5` and produced
`rpcs3-etk_gtk-edition-0.10.2_armsx3-8290349e5_linux_aarch64.AppImage`, 80,615,499 B, sha256
`7f551fcc80a6ececd94b4e4b2beef2bec2ee340d535a21040074493d98f041b5`, marker gates green. The patch sha
(`a456b67b97266c2a…`) matches all three October 27 run bundles that carry it, it applies cleanly to a
clean tree at base, and it reverse-applies to etk-cloud's resting tree.

**Known gap:** the header names a host test, `hwtest/test_audio_conceal.cpp`. That test was not
recovered: it is not in this repo or on etk-cloud.

0.10.2 is not the certified core: the kit's pin is still 0.10.1.
