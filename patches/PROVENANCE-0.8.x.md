# RPCS3 GTK Edition — 0.8.x provenance (recovered 2026-08-06)

Every patch here is `git diff a1deb2921..<commit>` — a cumulative diff on the published
upstream base **`a1deb2921`** (`v0.0.41-19638`). Apply exactly one.

| version | commit | patch | files | lines | probe | optnone |
|---|---|---|---|---|---|---|
| 0.8.0-dev | `16a900d85` | `etk-rpcs3-gtk-edition-0.8.0-dev.patch` | 22 | 1685 | — | — |
| **0.8.1-dev** | `884836af9` | `etk-rpcs3-gtk-edition-0.8.1-dev.patch` | 24 | 1723 | — | — |
| 0.8.2-dev | `f7a5d6eb0` | `etk-rpcs3-gtk-edition-0.8.2-dev.patch` | 24 | 1723 | — | — |
| 0.8.3-dev | `4d37a7cf6` | `etk-rpcs3-gtk-edition-0.8.3-dev.patch` | 25 | 1743 | — | ✔ |
| 0.8.4-dev | `324558fc2` | `etk-rpcs3-gtk-edition-0.8.4-dev.patch` | 25 | 1785 | ✔ | ✔ |

**0.8.1-dev is the core ETK ships** (`install.sh` `CERT_RPCS3`,
`rpcs3-etk_gtk-edition-0.8.1_v0.0.41-19638-a1deb2921_linux_aarch64.AppImage`).

Self-verifying: each patch contains its own `GTK Edition v<x>` literal, so a patch can always
be identified from its contents alone — no reliance on filenames or branch names. 0.8.1 and
0.8.2 are byte-identical in size because 0.8.2 is *the same source rebuilt on the LLVM-22
toolchain* (see `etk.conf`), which this table independently confirms.

## Addendum — `overlay-coalesce-notice.patch` (2026-08-31, Asahi surface)

A base-independent fix to the fence rescue-notice code added in the 0.6.1-ffs series (present
in every 0.8.x and 0.9.x patch). The **"GTK Crash Recovery"** overlay was queued from two fence
paths per stall — the warn in `etk_fence_note_not_ready` and the resolution in
`etk_fence_note_signaled` — with no coalescing, so a single GPU stall stacked two overlapping
overlays. First observed on **Honeykrisp (Apple M1 / Asahi)**, 2026-08-31; see
[rpcs3-asahi-M1](https://github.com/mercurious/rpcs3-asahi-M1).

It touches only `etk_queue_rescue_notice` in `vkutils/sync.cpp` (a 13-line insertion, **0 marker
changes**), so it applies cleanly on top of any cumulative carrying the fence-force net —
verified atop `0.8.4-dev` on upstream `c6e96729c` (19895). **Not yet a numbered cumulative:** it
carries no commit hash or literal bump. Fold it into the next cumulative at the next core
revision (bump the `GTK Edition v` literal + cut the patch, per the runbook below); until then,
apply it *after* the chosen `0.8.x-dev` patch. The 0.9.x / ARMSX3 line wants the same fix.

## Why this had to be recovered — two naming failures

**1. Branch-name drift.** The branch `etk/0.8.0-19638` has tip `884836af9`, which is
**0.8.1-dev** — the branch was cut for 0.8.0, 0.8.1 was committed onto it, and it was never
renamed. Anyone reading branch names would have shipped the wrong patch. Commit
`aed8540d7` is a further trap: it applies the Skip revert but still declares v0.8.0-dev; the
version bump only lands in `884836af9`.

**2. Stale git version stamp.** The rig reports
`v0.0.41-19642-f7a5d6eb | etk/0.8.2-dev | GTK Edition v0.8.4-dev` — three fields, two wrong.
`f7a5d6eb` is commit `f7a5d6eb0` = **0.8.2-dev**, and `etk/0.8.2-dev` is the branch name at
that moment. Both froze because incremental `ninja` does not regenerate the version stamp
(only a fresh CMake configure does), while the hardcoded literal in
`rpcs3/rpcs3_version.cpp` was hand-bumped. Cloud builds from a reset tree report
`v0.0.41-19638-a1deb292 | HEAD` — correct — so building on a clean tree fixes this
structurally rather than by remembering.

## Runbook additions this implies
- Name the branch for the version it will *end* at, or don't trust branch names — identify a
  build by the `GTK Edition v` literal in `rpcs3_version.cpp`, which is the only self-consistent
  source of truth.
- Bump that literal **and** cut a patch into `patches/` as part of every core revision, the way
  `etk-turnip-gtk` (0001–0008) and `rocknix-gtk` (0001–0002) already do.
- Release artifacts get version-only names; probe/experiment builds get a distinct prefix
  (`EXP-`, `ASAN-probe`) and are pruned from the catalog at every cut.
- At cut time, assert each shipped asset's producing repo contains a patch/recipe naming that
  version — and that the pinned asset actually resolves on the release (the `gtk_0.6`/`gtk_0.7`
  404 slipped through because `release_sanity.sh` only checks internal consistency).
