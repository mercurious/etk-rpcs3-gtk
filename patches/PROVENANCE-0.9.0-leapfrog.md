# RPCS3 GTK Edition — 0.9.0 leap-frog lane provenance

Base is NOT upstream RPCS3: it is the ARMSX3 fork, public `ARMSX2/ARMSX3` @
`f707458b0` (2026-08-20), whose own upstream ancestor is `3c15df4e4`-era
RPCS3 (Aug 2026). Our previous base `a1deb2921` (v0.0.41-19638) is an
ancestor of the pin, 484 commits back. GPL-2.0 throughout; ARMSX3 fork
commits © jpolo1224.

| patch | applies to | composition | gate |
|---|---|---|---|
| `etk-rpcs3-gtk-edition-0.9.0-mint1.patch` | f707458b0 | version literal + Linux build fix (Oboe source guard) — declared baseline (`GTK-Markers-Waiver:` in-patch) | GTK Edition only, WAIVED loudly |
| `etk-rpcs3-gtk-edition-0.9.0-leapfrog-v1.patch` | f707458b0 | 0001 restore upstream ISO hardening (undo the pin's HEAD revert) + 0002 ETK_CONSTRAINED_HOST (widen 6 mobile-profile gates; -D flag rides .ci) + 0003 ETK identity rebased per-hunk (58 hunks: 41 KEEP / 16 ADAPT / 1 DROP=CRLF) + 0004 Linux build fix (Oboe backend source was unconditional; oboe headers are Android-only — found by forge run 20260820-145013, the tree's first Linux build, which otherwise configured and compiled clean to ~400 files) | 13/13 markers, no waiver |
| `etk-rpcs3-gtk-edition-0.9.0.patch` | f707458b0 | **GA cut** of leapfrog-v1 + Release literal `GTK Edition v0.9.0` (0007) | 13/13 markers, no waiver |

0003 reversals-by-validation worth knowing: fence force-signal KEPT (base
still ends in vkWaitForFences(UINT64_MAX)); semapark KEPT as an adaptation
riding the base's 500-lap escalation budget. perfstat emitter verbatim
(rsx_profiler does not overlap: no /dev/shm mirror). Version literal
"v0.9.0-leapfrog-dev" is a placeholder pending the operator's naming.

Full per-hunk verdict table and open questions: dossiers repo,
`LeapfrogTriage_20260820_{VERDICTS,OPEN-QUESTIONS}.md`. Neither cumulative
patch is compiled yet — the forge lane build is the compile check.

Identify patches by their embedded `GTK Edition v` literal, never by
filename or branch (see PROVENANCE-0.8.x.md for why). The leap-frog names
carry no `-dev` suffix ON PURPOSE: preflight's auto-pick (`*-dev.patch`)
must never select them for the GA lane.

## 0.9.0 GA (2026-08-20)
Gate evidence: N=3 GT5P block SURVIVED(1)+CLEAN+CLEAN — first ledger-CLEAN rows
on this title; block survives 25 (stock 0.8.5) -> 6 (raw base) -> 1 (identity).
Operator gate ruling and full analytics: dossiers LeapfrogGate_N3_20260820,
LeapfrogMasterMigration_20260820 (de-tuned-block claim falsified; block was
fully tuned). mint1/leapfrog-v1 patches remain as the experiment record.
