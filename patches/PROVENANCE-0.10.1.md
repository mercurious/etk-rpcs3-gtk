# RPCS3 GTK Edition — 0.10.1 provenance (GT6 rear-view mirror)

`etk-rpcs3-gtk-edition-0.10.1-dev.patch` = `git diff 8290349e5..<tree>` — a cumulative diff on
**`ARMSX2/ARMSX3` @ `8290349e5`** (tag `1.0.4`), the same base as 0.9.1.x and 0.10.0. Apply exactly one.
Self-identifies as `GTK Edition v0.10.1 (armsx3-8290349e5)`.

| | base | ETK delta over the previous core |
|---|---|---|
| 0.10.0-dev | `8290349e5` | 0.9.1.2 + GTK Pitlink + PINE unlink fix + two GT6 boot fixes (PROVENANCE-0.10.0.md) |
| **0.10.1-dev** | **`8290349e5`** | 0.10.0 carried verbatim **+ upstream RPCS3 #19760 backported** (`gt6-mirror-fb-aliasing-tiebreak.patch`) |

## The GT6 mirror (2026-10-24)

Seen on every GTK core since GT6 first booted (`etk_gametest_status_sheet.md`: "rear-view mirror does not
render"; the 0.10.0 hunts: "renders black"). Upstream found and fixed it while our base sat still:

- **RPCS3 #19730** (closed, superseded) named the mechanism from RRC captures: GT6 renders the 2D HUD
  rear-view mirror into a colour target whose memory the *depth* target's oversized clip runs into. Both
  surfaces are written by the same draws, so they carry the same `last_use_tag`, and
  `get_merged_texture_memory_region` broke the tie by insertion order — the depth surface won, and the
  mirror sampled stale depth memory (black, with the previous frame's minimap bleeding through).
- **RPCS3 #19760** (kd-11, merged 2026-10-08, upstream commit `222754bfd`, `rpcs3/Emu/RSX/Common/surface_store.h`,
  +32/−7): the sort in `get_merged_texture_memory_region` keeps the fast path (different tags → older
  first) and, on a tie between a colour and a depth surface, honours the existing *Framebuffer Aliasing
  Heuristic Bias* setting when it is not `Auto`; on `Auto` it prefers the surface whose base address
  IS the requested texture address and is not clipped, then the unclipped one, then the larger area.
  kd-11: "a complex issue. FB slices are overlapping in memory for different aspects … clearly a
  game-specific scenario." Upstream testers: GT6 1.04 corrected with bias `Color` before the `Auto`
  rule landed; 1.06–1.22 were never affected.

The backport is upstream's merged diff, cherry-picked onto `8290349e5` + the 0.10.0 patch with no
conflicts: the base already carries `g_cfg.video.fb_aliasing_bias` and the `framebuffer_aliasing_bias`
enum (Auto / Prefer Color / Prefer Depth), so no config or UI change travels with it. The ETK delta is
untouched (Pitlink, nets, GT6 boot fixes), so 0.10.1 is a pure rendering-correctness revision of 0.10.0.

Validation target: car8 (headless, lift access), GT6 BCUS98296 v01.00, Arcade race, default bias `Auto`,
default HUD mirror renders the view behind the car (Pitlink frames). If `Auto` still shows the defect,
the fallback is `Framebuffer Aliasing Heuristic Bias: Prefer Color` in `config_BCUS98296.yml`.
