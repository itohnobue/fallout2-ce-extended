# Known Issues

Current known issues for the Fallout 2 CE Extended engine with **RPU** and **Et Tu**.
Open issues carry their investigation state (root cause hypotheses, ruled-out work,
next steps) so nobody re-does work already done. Sources: in-game user reports and
debug-log analysis, 2026-08-18/19.

Fixed items are removed from this file once verified and shipped (see git log + AGENTS.md
for the full investigation trail of each).

## Open issues

### Mod-side note — Vasques is pistol-only by design; his stock 14mm is vetoed

et tu `gl_partyarmor` (`config\party_armor.ini`, Vasques `WeaponAnims=5`) refuses
weapons whose anim code is not in the list. The 14mm pistol (pid 0x16=22) carries
anim code **6** (SMG column per the author's art convention — engine and mod read
the same proto field and agree). Under the mod's own rules his stock 14mm is
unusable → he runs empty-handed and "use best weapon" yields None. **Intentional
per owner decision (pistol-only); the mod's INI is the ground truth — not engine
code, not fixed.** Ian/Tycho/Katja lists include 6 → unaffected.

---

## Debugging notes

`Error during execution: VOODOO write_int/byte(0x00499xxx, ...) — NOT SUPPORTED in
CE engine` — gl_classic_wm.int's VOODOO_WriteNop procs attempt direct memory
patching (sfall-era engine addresses). The fork correctly rejects them. The
classicWM art swap now works via fs_copy, so these are benign-but-noisy. Mod-side
issue; engine behavior is correct.

---

## Debugging notes

- DBGTRACE instrumentation (program create/free/exit traces, UAF detectors, teardown
  markers, `[COMBAT]` lines) is silent unless `[debug] mode=log` is set in the cfg —
  then everything lands in `debug.log` in the game folder.
- `[debug] console_output_path=<file>` captures the in-game console.
