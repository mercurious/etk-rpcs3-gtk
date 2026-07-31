# Downstream portability — the #11912 road-flicker fix

The **GT5P road-shadow flicker fix** ([`#11912`](https://github.com/RPCS3/rpcs3/issues/11912),
`RSXTexture.cpp`) is core RSX code, not GTK/ROCKNIX-specific. It is therefore portable to **other
emulators that vendor RPCS3's PS3 backend** — notably [RPCSX](https://github.com/RPCSX/rpcsx) and
its Android forks, which carry a restructured copy of the rpcs3 tree and reach `decoded_remap()`
along the same path. This note is for maintainers of those downstreams.

## What the fix is
In GT5P (and GT5/GT6), the track shaders' shadow-map texture unit is intermittently parked on a
1×1 fallback texture whose `NV4097_SET_TEXTURE_CONTROL1` register is fully zeroed. Stock RPCS3
decodes control `0` as `CELL_GCM_TEXTURE_REMAP_ZERO`, building an image-view swizzle of
`(ZERO,ZERO,ZERO,ZERO)` — so every track material samples a constant `0.0` shadow-visibility term
and the whole track renders shadowed. On real hardware the fully-unconfigured register is neutral
(the console renders lit; the guest texel at that address is never written). Decoding the parked
state as **ONE×4** matches hardware and cures the flicker. Full root-cause analysis is in the
upstream issue draft; the runtime controls this repo ships (`GTK_REMAP0_ONE`, `GTK_REMAP0_IDENTITY`,
`GTK_PROBE_11912`) are documented in the [README](README.md).

## The portable change
`decoded_remap()` reads the control word, then extracts the high half:

```cpp
u32 remap_ctl = registers[NV4097_SET_TEXTURE_CONTROL1 + (m_index * 8)];
u32 remap_override = (remap_ctl >> 16) & 0xFFFF;
```

The fix is a single insert **between** those two lines. Minimal form:

```cpp
// A fully-unconfigured CONTROL1 (crossbar + control fields all zero) behaves as neutral on real
// hardware, not FORCE-ZERO×4. GT5P/GT5/GT6 park the shadow TIU in this state with no shadow map.
if ((remap_ctl & 0xFFFF) == 0)
    remap_ctl |= 0x55E4; // ONE×4 control, identity crossbar
```

This repo ships the **conservative** variant — scoped to the exact confirmed signature
(`… == 0 && width() <= 1 && height() <= 1`), so it only ever touches the 1×1 parked placeholder and
cannot affect a legitimately-configured texture. That scoping is deliberate: an earlier blanket
override produced brief flicker blips by sampling cache-garbage from the never-written texel. Ship
the scoped form unless a hardware test confirms the unscoped decode is safe in general.

## Applying it
Do **not** `git apply` this repo's raw `.patch` against a restructured or newer rpcs3 base — the
surrounding context will not match. Re-express the insert above by hand (or via a 3-way merge
against your base), then game-test: a clean build does not prove correct emulation. Validate on your
own target (drive Arcade → Daytona → bumper cam, no cars ahead; the flicker is view-dependent).

Rebase data point (2026-07-31): a 3-way merge of the full patch set from base v0.0.41-19544
(`60c9705a`) to v0.0.41-19638 (`a1deb2921`) was conflict-free — `fragment_texture::decoded_remap()`
remains the single remap decode point through upstream's RSXTexture `attributes()` refactor, and
upstream's new wrapper paths route through it, so the override applies uniformly.

## Canonical channel
The durable home for this fix is **upstream RPCS3** — [issue #11912](https://github.com/RPCS3/rpcs3/issues/11912).
If it lands there, every rpcs3-derived project (including RPCSX forks) picks it up through normal
upstream tracking, with no per-fork port. This repo carries it downstream only because the upstream
bug remains open.
