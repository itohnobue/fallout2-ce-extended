# Upstream sync — October 2026 (`fallout2-ce` `1cce1449..f6254ed5`)

The committed record of the fork's October 2026 upstream sync: what was merged, what was
discarded, and why. It exists because the merge strategy chosen (ancestry-only `-s ours`) makes
the discarded material invisible to a future `git merge` — this document is the only place that
remembers it.

| Item | Value |
| --- | --- |
| Upstream target | `fallout2-ce/fallout2-ce` `f6254ed5d25ec74f1de166bbba3611e5d6e303a3` |
| Merge base | `1cce1449` |
| Upstream commits in range | **317** (`git rev-list --count 1cce1449..f6254ed5`) |
| Merge commit | `88ddbbdf` — parents `e3e2433b` (fork) and `f6254ed5` (upstream), `git merge -s ours` |
| Content | 10 port waves, one fork commit per wave, plus one regression-fix commit |
| Ledger | **317** rows, one disposition each (60 ADOPT · 191 SKIP · 29 ALREADY-PORTED · 6 CONFLICT-DISCARDED · 31 ANCESTRY-ONLY) |
| Fork state after the sync | `0c39f728`; `git rev-list --count HEAD..f6254ed5` → **0** (not behind) |
| Tree delta vs pre-merge | 79 files, +2332 / −618 (`git diff --stat 88ddbbdf..HEAD`) |

## 1. The shape of this sync

**The merge records ancestry, not content.** `88ddbbdf` is `git merge -s ours f6254ed5`: it takes
upstream as an ancestor and leaves the fork's tree byte-identical to its pre-merge state. Content
was then adopted selectively — one reviewable fork commit per port wave — and every adopted
commit was adapted to the fork's idioms rather than transcribed.

**Why a content merge was rejected.** The range is dominated by upstream's typed-value refactor
series (`FrmId`/`TypedFrmId`/`ProtoId`, scoped frame-ID enums, typed `Color`, typed `Map` APIs). This
fork is int-based by design: it carries a minimal `FrmId` wrapper (`src/art.h:86-108`) and none of
the subclass family or the scoped enums (`grep -rn "TypedFrmId\|ObjectFrameId\|ProtoId" src/` → 0).
A textual merge would have produced a tree that neither compiles nor preserves the fork's
semantics, and would have buried ~60 real fixes inside thousands of mechanical rename hunks. Per-
commit adoption with an explicit disposition was the cheaper correct path.

**The trade-off, stated because it is permanent.** After this merge, the next
`git merge upstream/main` uses `f6254ed5` as its base. Upstream changes this run deliberately
discarded will therefore **not** reappear as pending deltas — they stay discarded by design. This
document is the record of *what was discarded and why*. A future sync must revisit a decision
deliberately (read the entry, re-evaluate it against the then-current code) instead of re-deriving
it from a merge conflict. That is the price of getting GitHub's "behind" count to 0 without
adopting all 221 upstream-changed files wholesale.

## 2. Disposition of the 317 rows

| Disposition | Rows | Meaning |
| --- | --- | --- |
| ADOPT | 60 | Ported — 51 whole commits and 9 partial carries (each listed under the table and in §4/§5) |
| SKIP | 191 | Deliberately not ported |
| ALREADY-PORTED | 29 | The fork already has the behaviour, implemented its own way |
| CONFLICT-DISCARDED | 6 | Wanted, but unsafe to port onto this fork's architecture |
| **content commits** | **286** | |
| ANCESTRY-ONLY | 31 | Merge commits — no content of their own; their commits are dispositioned individually |
| **ledger rows** | **317** | |

Sum check: `60 + 191 + 29 + 6 = 286` content commits, `+ 31` merges = **317**.

**Whole and partial carries.** 51 of the 60 ADOPT rows carried the commit's whole change set (a hunk the
fork already had counts as carried — that is why the ledger's `4ae5c41b` `[2 of 3 hunks]` and
`6fcdfc42` `[residual]` rows are whole: their remainder was already present). Nine are partial
carries, each recorded in §4/§5: `1cb875b8` (§4 #1 — the strict `dataSize > payloadSize` rejection is
not taken), `a035c0fa` (§4 #2 — two hunks; the constexpr page model is not ported), `14d95ab5`
(§4 #7 — the commit's `files/fallout2.cfg` hunk is excluded by the standing rule),
`5a2f3c19` (§4 #8, §5 #1 — the panning rewrite is not adopted), `afee24e6` (§4 #9 — the 13
hard-coded-effect sites only; `trait_tweak.*` is protected), `fe35a604` (§4 #11 — two of three hunks),
`106fb3a6` (§4 #13 — 12 of its 17 files), `e3fc7467` (§4 #14 — the rest limb only; the holodisk half is
SKIP) and `09677395` (§5 #5 — the `game_dialog.cc` half only). The split re-derives from the ledger's
60 ADOPT rows against those entries.

**The rule used.** Each commit got a value class — **V** behavioural fix the player can see,
**M** RPU/Et Tu mod compatibility, **S** safety/hardening — or `none` (aesthetics, typing, naming,
chores). The disposition rule was **default-deny**: `none` → SKIP unless the behaviour is already
present here (then ALREADY-PORTED); a real fix with no carrier in this fork → SKIP; a wanted fix
whose port would change the fork's architecture away from a load-bearing invariant →
CONFLICT-DISCARDED. Measured tally over the ledger: 69 V, 16 M, 23 S, 209 `none`.

**What the SKIPs are.** The largest group is the typed-value refactor series: measured on the
ledger's own reason text, 69 of the 191 SKIP rows name a retype/rename/typed-family ground (a
typing-only change is never worth the churn in an int-based fork). The rest are formatting and
comment churn, upstream documentation and CI, upstream-only features and QoL defaults, and fixes
whose subject code this fork does not have (e.g. a ctor of a type that exists only upstream).

**The six CONFLICT-DISCARDED commits — wanted, and why each is unsafe here:**

| Commit | What it does | Why it must not be ported |
| --- | --- | --- |
| `44950f9c` | `buildFid` rotation guard | The guard's value space is 0..5 while the FID field is 1..6: it returns `kEmptyFid` for a legitimate north-west critter at **57** `buildFid` call sites that pass `rotation + 1` (`src/obj_types.h:48`). |
| `6b4984a2` | frame-id **range** bail-out in `FrmId::buildFid` | The fork's free `buildFid` deliberately masks instead of bailing (`src/art.cc:1135-1159`); adding the bail-out turns mod-supplied frame ids into invisible art (`buildFid(OBJ_TYPE_CRITTER, customModelNum, …)`, `src/sfall_opcodes.cc:1369-1373`). Upstream reverts the bail-out four commits later (`cf72de75`, itself SKIP) — the two must not be adopted together. |
| `2254fc64` | per-critter permanent withdrawal (Jet) model | The fork's addiction is a dude-global GVAR (`src/item.cc:3257-3262`, `:3615-3626`); the commit adds a per-critter queue entry. The save format is *not* the blocker — the state model is. Its UI limb is separately SKIP on value. |
| `3e570309` | per-turn AP grant | Voids the invariant the fork's AP rewind is built on: a per-turn grant hands a spent critter a fresh AP pool, which the rewind logic assumes cannot happen. |
| `f00495c0` | corrected door-open predicate | Its pre-image never existed on this branch, so it *replaces* rather than repairs `#731` (`src/animation.cc:2729-2739`). The corrected predicate is carried instead by the re-expressed W7 port (see §4, narrowing 5). |
| `61820700` | `HOOK_USEANIMOBJ` contract | Et Tu ships a `UseAnimObj_handler`, so the value is real — but a faithful port rebinds an in-use hook id: arg0 is the literal `gDude` (`src/sfall_script_hooks.cc:1722`), a mod's `-1` return is ignored (`:1730-1736`), and the fire sites are opcode-only (`src/interpreter_extra.cc:1415`, `:1442`). |

## 3. The ten port waves

Each wave is one fork commit naming the upstream shas it carries. **The agents *inside* a wave are
file-disjoint by construction** — a wave's shas are grouped by the connected components of the wave's
commit↔file graph, so two upstream commits that share a file always land in the same agent's range.
**The waves themselves overlap in files by design** (`src/combat.cc` in W1, W2, W3 and W6;
`src/worldmap.cc` in W3 and W10), which is why they run one after another: with overlapping file sets
a concurrent wave could not be attributed to a single gate, whereas a serial wave closes with its own
gate (build + full `ctest` + invariant probe + allow-list audit + protected-file diff) and its own
lead commit, so a red gate names exactly one wave's commit set.

| Wave | Fork commit | Upstream commits | What it delivered |
| --- | --- | --- | --- |
| W1 | `918a09ed` | 18 | Combat engine and combat AI, sfall lineage: `damage_p_proc` on misses, `HOOK_INVENWIELD`/`drop_obj` worn-armour stats, Bonus Move walk distances, zero-damage instadeath criticals, the critical-hit table, NPC pickup in combat, multihex placement/overlap, `ai_retrieve_object`, the "Unlimited Ammo" removal reason `-2`, Bonus Move save/load stash, `ai_move_away` sign pair, party-member corpse preservation, friendly-fire burst reduction, `destroy_p_proc` before `critterKill` for explosions, `use_obj_on_obj` on self, multihex party placement, the `fe35a604` combat-AI trio (2 of 3 hunks), XP on elevator-ended combat, and message id 1000 in all eight `ce.msg` locales |
| W2 | `682feb24` | 8 | Barter: the overweight short-circuit, the barter-table offset after ammo collapse; flares: take from the owner's stack, light one not the stack, restore the holder's light, `UseObj` reason; trait overrides at the 13 hard-coded-effect sites; corpse aging and party placement parity |
| W3 | `d7f43dfb` | 5 | Saved-map aging (`0a403248`: a failed blood-stain frame no longer makes the map reload as new), `whoHitMe` clearing for hidden critters, `dead_bodies_age=No` healing/fleeing, `mark_area_known` (#720), worldmap teleport centring guard |
| W4 | `a0302c18` | 7 | Save/load: per-slot tolerance + lazy page load, ESC-rest original duration; character-editor card strip artifact; repaint after window resize; literal log format strings; main menu behind submenus (three menu-flow fixes); dialog parameter-name fix |
| W5 | `74d9e33c` | 10 | Speech routing and audio: barter reply speech, out-of-dialogue voiced lines, dialogue-window-state routing, lip-sync only with a talking head, per-NPC loose-dir speech resolution (2 commits), 44.1 kHz channel count, case-insensitive gapless music, sound-effect cache size error check, and the rest-mode speech guard (`e3fc7467` rest limb) |
| W6 | `ad1d2710` | 6 | Memory/heap/art-I/O safety sweeps (`1cb875b8`, `0e5d1c7d`, `3b4ed9e4`, `701e465d`, `ac19e1ab`), with the fork's existing guards preserved; AI distance-overflow bound keeping the script-visible `9999` sentinel |
| W7 | `ec60445f` | 1 | Et Tu scripted doors: re-check door blocking after confirming door use (Killian's storeroom), re-expressed onto the fork's `else` branch |
| W8 | `75fafc08` | 1 | Fidelity sweep, per-hunk (12 of 17 files): save short-writes, automap overflow, OOM drift, item charges, hover clear, font-null guard, movie-effect guard, sfx negative-handle guard, svga null guard, sound error code/callback reset |
| W9 | `068d134f` | 3 | Et Tu purple `float_msg`, hires `obj_on_screen`/`tile_is_visible`, endgame/death-slide sizing |
| W10 | `de6e1127` | 1 | Endgame movie flags + focus-gained MVE sync, and the worldmap fade removal including the dead fork-local `HOOK_ENCOUNTER` block |

Upstream shas carried, by wave:

```
W1  13fbd382 2510bf8e 2b148cbe 3180c707 3dd29af8 523aced8 685122ba 73417d71
    87da37f3 96f76b64 97b7285c af75deb4 d8cbdfca e7f2eec3 f1569075 fee3c3d3
    fe35a604 926e4ed4
W2  386dd45a e4dba296 3ead395c e63445df 6200ac27 6f2e3fa3 afee24e6 4ae5c41b
W3  0a403248 28079722 b623d427 ad91699f 4963b847
W4  09677395 a035c0fa a53cbe3c 2c4b66b4 9ffd7aee 6fcdfc42 14d95ab5
W5  61ee7802 76d435fa 7c5cec83 896b5c44 8c134431 97d3b8e5 a3830a74 cd5ca495
    d1af9af6 e3fc7467 (rest limb only)
W6  1cb875b8 0e5d1c7d 3b4ed9e4 701e465d ac19e1ab 64c9eeef
W7  af86e37c
W8  106fb3a6
W9  5511a0ec 2cbaa5ac 5a2f3c19
W10 79bdb28a
```

## 4. Mandatory narrowings

No ADOPT commit was ported verbatim. Each carries a binding narrowing, because upstream's post-
image assumes upstream's architecture. These are the ones that change behaviour, are easy to get
wrong on a future re-read, or explain why this tree differs from `upstream/main` on purpose.

| # | Commit | Narrowing as applied | Why |
| --- | --- | --- | --- |
| 1 | `1cb875b8` | **The strict `dataSize > payloadSize` rejection is NOT taken**; only a negative `dataSize` is rejected, and the payload is authoritative (`src/art.cc:1224-1245`). The frame-capacity threading is taken. | Split directional art (`.FR0`–`.FR5`, ~15,000 shipped files in the two stands) repeats the aggregate size of all six files in every header, so the strict check rejects virtually all of it. Upstream reversed the same check in `a1193da3` (#790). |
| 2 | `a035c0fa` | Two hunks only (`_GetSlotList` per-slot tolerance + lazy page load). **`saveLoadPages` stays a runtime value**: `int saveLoadPages = 10` (`src/loadsave.cc:269`) and `saveLoadPages = gExtraSaveSlots ? 100 : 1` (`:543`). `saveLoadTotalSlots` is never `constexpr`. | The fork's 10-slot-per-page paging (and the 1-page model when `ExtraSaveSlots` is off) is the shipped behaviour; making the page model compile-time deletes it. |
| 3 | `ad91699f` | **State 3 is treated as "known, reveal nothing"** and only state 3: the value is folded to `Known`, the radius mark is skipped, and the subtile is upgraded at the circle's centre without downgrading a subtile the player has already seen (`src/worldmap.cc:7248-7300`). The fork's `int` state type is kept — no `enum class VisitedState`. | Et Tu's shipped scripts call `mark_area_known(0, area, 3)` (59 call sites, measured during triage); passed through as an ordinary state it revealed a 3×3 (5×5 with Scout) fog block for an area the player had only heard of. Keeping `int` avoids carrying upstream's typed-enum series. |
| 4 | `4963b847` | Only the `wmTeleportToArea` guard is new; the destination math duplicates code the fork already has, and the terrain hunk is an alternative implementation. Ordered after `ad91699f`. | Porting the whole commit would have replaced the fork's worldmap position math with a second copy of itself. |
| 5 | `af86e37c` | **Re-expressed, not transcribed**: upstream's `openFlags & 0x01` predicate inserted in the fork's own branch using the literal `0x01` (`src/animation.cc:2729-2739`); `f00495c0` itself stays CONFLICT-DISCARDED. | The fork has no `DOOR_FLAG_OPEN` symbol, and `f00495c0`'s pre-image never existed here — a transcription would have replaced, not repaired, `#731`. |
| 6 | `3180c707` | The zero-damage clamp goes **inside** `attackComputeDamage` after `scriptHooks_ComputeDamage(...)`, at its end (`src/combat.cc:5685-5705`), not inside the hook wrapper. | Appended inside the wrapper it is inert for unmodded play — the hook gate is not entered. |
| 7 | `14d95ab5` | Widened allow-list (mainmenu.*, main.cc, settings.*, preferences.cc, character_selector.cc); `sfallOnAfterNewGame()` preserved **before** `sfall_gl_scr_exec_start_proc()`; the start-map override, `mainHandleDevEndgameRequests()` and the unconditional `main_reset_system()` preserved; `files/fallout2.cfg` excluded. | Those four are fork flow elements upstream does not have; upstream's reversed hook order would re-break the new-game sequence (F-009/F-51). |
| 8 | `5a2f3c19` | **The panning rewrite was NOT adopted.** The fork keeps its `panDistance = width - 640` arithmetic and its `panDistance <= 0` early return (`src/endgame.cc:505-513`). | See §5 entry 1 — the wave brief forbade removing the fork's guard, and for the shipped `art\intrface\DP.FRM` (1280×480) upstream's rewrite computes numerically identical values. |
| 9 | `afee24e6` | The 13 hard-coded-effect sites only; `trait_tweak.*` untouched (protected module); `has_trait` (`src/interpreter_extra.cc:2938-2942`) kept ungated. | The AP claim behind the full port was false: the 13 sites never consult `NoHardcode`, and Et Tu ships `[t14] NoHardcode=1` — the sites are the live defect; the protected module is not ours to rewrite. |
| 10 | `523aced8` | `gCeMessageList` and message id 1000 added to **all eight** `ce.msg` locales. | Upstream's diff carries six; the fork ships eight (italian and spanish are fork-only locales). |
| 11 | `fe35a604` | **Two of three hunks.** The third (`_combat_should_end`) is staged behind an in-game check + regression test, and the `97b7285c` consumer must not import `aiFinishMovement`. | That hunk changes *when combat ends*; it is not fork-owned code. See §5 entry 4. |
| 12 | `79bdb28a` | All three units adopted **and** the now-dead fork-local `HOOK_ENCOUNTER` block deleted in the same commit (`src/worldmap.cc`; `grep -rn wmFade src/` → 0). | Compile-required after the fade removal, behaviour-neutral, and §5 entry 3 records the interpretation conflict this settled. |
| 13 | `106fb3a6` | Per-hunk: 12 of 17 files. The `src/dbox.cc` hunk **is** taken (it adds the loop's only `pch++`). | The filed synthesis called the `dbox.cc` hunk cosmetic; it is one of the twelve applicable hunks and dropping it would have kept the filename-truncation defect (F-94). |
| 14 | `e3fc7467` | **Rest limb only**: a `gPipboyRestDepth` counter around `pipboyRest`, `bool pipboyIsResting()` (`src/pipboy.h:20`, `src/pipboy.cc:1056`) and the `&& !pipboyIsResting()` conjunct at `src/scripts.cc:3484`. The holodisk half is SKIP. | `pipboySoundStop()` is absent here and introduced only by the SKIPped pool commit `fb1a0fd1`, and no holodisk narration plays in either stand. The limb is mandatory once `76d435fa` voices every out-of-dialogue audio-tagged line — resting runs queued script events, so floats would be voiced. |
| 15 | `97b7285c` | Producer (`src/combat.cc:2511-2518`) and consumer (`src/combat_ai.cc:1417-1471`) changed in **one** commit; the consumer keeps the fork's `aiGetPacket` null guard. | The sign contract is two-sided; the producer alone makes the attacker throw from inside the blast. |

## 5. Recorded deviations (AC-12)

AC-12 requires every deviation from the plan's letter to be recorded, naming the fork behaviour
that blocks it. Five were unrecorded at review time; they are recorded here, in the commit that
delivers this document.

| # | Row | Deviation and its one-line reason |
| --- | --- | --- |
| 1 | `5a2f3c19` | **The panning rewrite was not adopted**: the W9 wave brief forbade removing the fork's `panDistance <= 0` guard and forbade test edits, which conflicts with the plan's narrowing 9 — a plan-vs-brief conflict; the fork guard is preserved (`src/endgame.cc:505-513`) and `tests/test_endgame.cc` still pins the pre-existing behaviour. |
| 2 | `2cbaa5ac` | **Height-semantics narrowing**: the port uses `screenGetHeight() - 1` where upstream uses `screenGetVisibleHeight() - 1` (`src/interpreter_extra.cc:423`, `:5251`), so the 100-px interface-bar strip counts as visible. |
| 3 | W10 `wmFade*` removal | **Dead-block deletion, not a fade-port deviation**: the plan's §B row reads the fade deletion as conflicting with the fork's `HOOK_ENCOUNTER` work, while §D.5 #13 orders all three units adopted plus the now-dead block deleted; the port followed §D.5, the dead block is gone, and `grep -rn wmFade src/` → 0. |
| 4 | `fe35a604` third hunk / `97b7285c` consumer | **Partial carry recorded**: the plan authorises withholding the `aiFinishMovement` hunk at `_ai_move_away`, but no ledger row said a hunk was withheld; this entry is that record — the hunk is staged, not landed, and its in-game check + regression test are open items. |
| 5 | `09677395` | **Partial carry recorded**: only the `game_dialog.cc` half landed (the OK path now stores the chosen preference instead of `NO_PREF`); the `settings.cc` `clamp(8→16)` hunk was **not** carried, so `art_cache_size` keeps its fork floor of 8 (`src/settings.cc:155`) — the RPU stand ships `art_cache_size=8` and upstream's floor would raise it and log a clamp. |

## 6. Three SKIPs decided before triage

These were decided in the previous sync pass and are carried unchanged. Each was re-verified
against the code during this sync.

- **`6f05d3d8` — #577 dynamic sound list.** Et Tu ships a tagged `SNDLIST.LST`; upstream's
  directory-scan change reorders FO1 sound tags. It is not a tag reorder only: it also drops the
  fork's count cap (`src/sound_effects_list.cc:99-108`, M-195), which is what keeps a crafted list
  from making the load loop dereference a failed allocation.
- **`436d5554` — #694 remove `encounter-intros`.** The fork has a working, FO1-gated
  implementation (`src/worldmap.cc:4805-4813`) plus the `encounter_intros` metarule
  (`src/sfall_metarules.cc:1246`); adopting the removal deletes a metarule mods call.
- **`68479c44` — #367 perk carryover.** The fork's `gSfallPerkOwed` + sfall-globals persistence
  (`src/sfall_opcodes.cc:6399-6403`, `:6557`) is a deliberate, functionally equivalent
  architecture; upstream's per-level vector + `ce.sav` sidecar would replace it rather than fix it.

## 7. Verification record

Every wave closed with the same gate: a build (all targets), the full `ctest` suite, a six-count
invariant probe, an allow-list diff audit (changed paths ⊆ the wave's allow-list, with
`files/fallout2.cfg` a hard exclusion) and a protected-file diff. The final acceptance stage
re-ran all of it on the finished tree:

- **Build:** `cmake --build out/build/macos-arm64-debug --config Debug --clean-first` → `BUILD
  SUCCEEDED`, exit 0, **600** `CompileC`, **0** errors (the prescribed incremental form compiled
  nothing, so it was not accepted as evidence).
- **Tests:** `ctest --test-dir out/build/macos-arm64-debug -C Debug --output-on-failure` →
  **93/93 passed, 0 failed, 0 skipped**, exit 0. The 93rd target is this sync's new
  `test_global_scripts_timers`.
- **Six invariant probes**, all at or above the pre-sync floor:
  opcode handlers **285** (≥285), hook enumerators **56** (≥56), implemented hooks **44** (≥44),
  `kMetarules[]` rows **134** (≥134), ctest targets **93** (≥92), Et Tu overlay detections **4**
  (≥4). The probe measures *shapes*; it cannot see behaviour and is not offered as behavioural
  evidence.
- **Protected files:** all **38** byte-identical to the pre-merge tree (four independent checks:
  name-set intersection 0, `git diff --stat` empty, per-file `git diff --quiet` 38/38 identical,
  no commit in the range names one). `files/fallout2.cfg` untouched in the range, the working tree
  and the log.
- **Allow-list:** 79 changed paths (71 `src/` plus the 8 `ce.msg` locales), **0** outside the
  allow-list, no test assertion weakened (`CHECK`/`REQUIRE` call-start lines **10,441 → 10,504**,
  delta **+63**; no deleted `CHECK`/`REQUIRE` line; one `add_test` added, none removed). Both
  numbers are reproducible: baseline
  `git grep -hE '^[[:space:]]*(CHECK|REQUIRE)(_MESSAGE|_THROWS)?\(' 88ddbbdf -- tests/ | wc -l` and
  final `grep -hE '^[[:space:]]*(CHECK|REQUIRE)(_MESSAGE|_THROWS)?\(' tests/*.cc tests/*.cpp | wc -l`
  (call-start lines only, optional `_MESSAGE`/`_THROWS` suffix; the metric is stated because the
  counts are §10 item 4's baseline).
- **One regression found, one fixed.** The post-implementation review's adversarial stage
  confirmed exactly one regression against the pre-merge tree: **N2**, the save-slot
  cursor↔page invariant. The ported lazy `_GetSlotList` scanned only `_currentSlotPage`, while the
  fork's auto-quicksave ring advances `_slot_cursor` without moving the page, so
  `_LSstatus[_slot_cursor]` read stale `EMPTY` and the "overwrite?" gate was skipped (and the load
  gate refused a real save). Fixed in `0c39f728` by scanning the cursor's page as well
  (`src/loadsave.cc:2862`); severity corrected HIGH → MEDIUM on blast radius (the destination is
  always a page-0 ring slot the quicksave already overwrites silently). Pinned by an arithmetic
  mirror test plus a gate grep. Everything else filed at MEDIUM or above either did not manifest
  or is a latent hazard with no shipped-configuration trigger.

**Honest limits of this record.** The harness compiles 16 engine translation units; on the
iteration-2 disputed set (19 shas over 18 slots — 12 SKIP, 4 ALREADY-PORTED, 2 CONFLICT-DISCARDED
and one ADOPT limb, `e3fc7467`), 17 of the 19 touch none of them, so most pins are mirror tests
paired with gate greps rather than executable proof. The fixes that no test can execute are on the
operator checklist below. The release build and both smoke stands were rebuilt and redeployed at the
end of the sync, but the game was not launched — in-game behaviour is the operator's check, not this
document's claim.

**Operator in-game checklist** (the condensed form of the Stage-11 list, kept here so the committed
record is self-contained — `tmp/` does not survive the session):

1. **F-08** — the Best-Weapon row applies the chosen preference on the OK path (double-click already
   worked).
2. **F-44** — combat ends through the third `fe35a604` hunk's condition once that hunk is enabled.
3. **F-27** — a party member no longer shoots the dude in the back after `ai_move_away`.
4. **F-49** — ESC during rest returns the *original* rest duration, and `HOOK_RESTTIMER` CANCEL
   receives the right arg0.
5. **F-46** — `mark_area_known(area,3)` marks the 3×3 area (5×5 with Scout) and the fog radius
   matches.
6. **F-84** — lighting one flare lights one, not the stack.
7. **F-80** — saving after a full disk / short write fails loudly instead of renaming a truncated
   `.tmp` over `SAVE.DAT`.
8. **F-53** — the Killian-storeroom door can no longer be walked through.
9. **F-50 / N3** — the endgame and death slide scale instead of early-returning, while endgame
   panning is unchanged with the shipped 1280×480 `DP.FRM`.
10. **W5 speech / N25** — out-of-dialogue voiced lines are audible, resting floats are *not* voiced,
    barter reply speech no longer plays over the barter UI, and 44.1 kHz voice plays at the right
    pitch and speed.
11. **W10 worldmap** — the map opens without a black frame.
12. **N2** (this sync's fix) — with `ExtraSaveSlots=1` and `auto_quick_save>=1`: quicksave, open the
    Save Game window, press ENTER/DONE — the "overwrite?" confirmation must now appear.
13. **N22** — the worldmap destination marker sits on the centre of the town circle (it may be 1–3 px
    off the shipped art).
14. **N7** — the overlay feature stays invisible on every shipped config (confirm nothing regressed).
15. **A7-1 / R-10** — a de-targeted enemy re-engages.
16. **Smoke sanity** — both stands reach their main menu (RPU → Arroyo, Et Tu → Vault 13) and
    `debug.log` shows `Game config loaded from …/Contents/MacOS/fallout2.cfg` plus the expected mod
    list, with no new error lines.

## 8. Known residuals — fork defects recorded, not fixed

These are pre-existing fork defects, or port-induced deltas that no shipped configuration reaches.
None is introduced by the merge as a reachable defect; none is fixed in this sync. They are listed
so a future session does not rediscover them from scratch.

| Defect | Site | Risk |
| --- | --- | --- |
| **Global-script timer API is inert** | `src/scripts.cc:678-694` (`scriptGetSid` scans only `gScriptLists`) → `src/sfall_metarules.cc:4371-4377` returns −1 | An sfall-compat mod registering a global timer silently receives −1. Its only carrier was SKIP (no shipped caller, ~153 lines to re-host). Re-open only if a supported RPU/Et Tu mod needs it. |
| **Script extent writer/reader asymmetry** | reader `src/scripts.cc:2555-2588`, writer `:2409-2501` | LATENT (no shipped producer): a save whose `#NO_SAVE` exceeds the tail extent count would be refused on load. The writer is upstream's and changing it alters the on-disk layout — report upstream rather than patch locally. |
| **`xlistEnumerate` abort not propagated** | `src/xfile.cc:687-689` | LATENT (OOM): a handler-requested abort exits only the inner loop, so the handler can be re-entered on an already-failed list. |
| **`HOOK_REMOVEINVENOBJ` handler destroying the item** | `src/item.cc:483-489` | The hook fires before the item is used; a handler that destroys it leaves the caller on a freed object. Pre-existing in the callee, not a port repair. |
| **NPC loot-window armour drop keeps its stats** | `src/inventory.cc:4713` (inside the `&gInventoryArmor` branch at `:4712-4714`) | Dropping an NPC's worn armour from the loot window does not re-run the stat adjustment (only the player's `&gInventoryArmor` slot takes that branch), so the critter keeps the dropped armour's AC/DR/DT. Pre-existing. |
| **Protected-module default divergence** | `src/trait_tweak.cc:78` (`[Traits] Enable` defaults 0, upstream 1) | Et Tu ships `Enable=1`, so shipped content is unaffected; `trait_tweak.*` is protected and the divergence is not this sync's to change. |
| **`sad->length = actionPoints` without an `ANIM_COMPLETE` guard** | `src/animation.cc:2293-2294`, `:2496-2497` | Upstream-identical; all call sites pass a non-zero AP budget, so no reachable zero-length path is known. |
| **`soundGetDuration` conflates error and duration** | `src/sound.cc:988-1007` | Returns the `SoundError` code as a duration: endgame speech with an unusable handle gets a 21–22 "second" duration. LOW, gated by speech being loaded. |
| **`map_edge` elevation index unchecked** | `src/map_edge.cc:372-375` | `edgeData[elevation]` on a 3-element array; mapper-only, no reachable invalid elevation in game mode. |
| **`.LIP` marker count vs phoneme count** | `src/lips.cc:93` | A malformed `.LIP` whose marker count exceeds its phoneme count indexes `phonemes[]` out of bounds. |
| **Unbounded `strcpy` of save-/MSG-derived text** | e.g. `src/loadsave.cc:2938` (`_LSData[index].description`) | A description or message line longer than the destination overflows. Triage measured the longest shipped message at 72/117 bytes against 260-byte-and-up buffers, so it is dormant. |
| **`Config` leak on a malformed worldmap file** | `src/worldmap.cc:2128`, `:2134` return −1 before the single `configFree` at `:2193` (`worldmap.txt`); the city half is another function — `city.txt` is read at `:3241`, whose twelve early returns at `:3293`-`:3361` precede their `configFree` at `:3371` | A malformed `worldmap.txt` or `city.txt` leaks one dictionary per failed load. |
| **`strParseKeyValue` caller omits `keySize`** | `src/worldmap.cc:3496` → `src/string_parsers.cc:255` | The unbounded copy branch is live, blocked only by the caller's pre-check at `src/worldmap.cc:3491`. LATENT by convention, not by type. |
| **`hs_*` name→id map missing two hooks** | `src/sfall_global_scripts.cc:194-238` | `hs_adjustrads.int` / `hs_removeinvenobj.int` never auto-register (both hooks have ids and fire sites); numeric registration is unaffected. |
| **Object-count clamp** | `src/interpreter_extra.cc:4114-4116` | Still clamps at 99,999 where sfall 4.4.2 removed the cap — a mod-compat gap, not an engine defect. |

Five defects from the earlier list are **no longer residuals** and must not be re-filed: the
negative font index (`src/text_font.cc:308` now rejects it), save-driven inventory counts
(`src/proto.cc:563-580` now validates them), `mapAllocLocalVars` state rollback
(`src/map.cc:540-556` now restores the length), the numeric field's mid-string `-`
(`src/window_manager_private.cc:1705` now tests `cursorPos == 0`) and the unchecked
`internal_malloc` in `src/window_manager_private.cc:1634` (now null-checked). All five were closed
by this sync's own ports.

## 9. Rejected fixes — what must NOT be applied

Each of these was proposed during review and rejected with its reason. They are recorded because
each one *looks* like a fix and would make things worse.

1. **Do not delete the UI armour strip at `src/inventory.cc:4713`** (sfall's preferred shape: let
   `objectDrop` own the stat adjustment). On the UI path the callee's `OBJECT_WORN`
   precondition is false — the inventory window's setup strips the worn armour and clears the flag
   before the drop block runs — so deleting the caller's strip leaves **no** strip at all and the
   exact sfall bug the port fixes reappears on the character screen.
2. **Do not add a static `_obj_dropStatsAdjusted` handshake across `src/inventory.cc` and
   `src/proto_instance.cc`** (sfall's second preferred shape, and the proposed fix for N1). It adds
   cross-translation-unit mutable state to suppress a call that cannot occur — the N1 "false
   positive" does not manifest — so the flag is inert state whose only effect is to couple two
   modules.
3. **Do not move the two `destroy_p_proc` loops after `_report_explosion`** (`src/actions.cc:1768-1794`).
   Killing the extras *before* the report is what makes the extras damage loop skip them: that loop
   (`src/combat.cc:5779-5780`) applies damage only to extras whose `DAM_DEAD` is unset. Reordering
   applies `extrasDamage` to live extras and changes damage and XP bookkeeping. If that window is
   ever closed, compact the surviving extras; do not reorder.
4. **Do not adopt the full `5a2f3c19` viewport rewrite.** For the shipped 1280×480 `DP.FRM` it
   computes numerically identical `panDistance`/`fadeDistance`/`frameDelay`, it forces a
   same-commit rewrite of `tests/test_endgame.cc`, and it removes the fork's guard. The real
   obligation was the ledger line (§5 entry 1), not the code.
5. **Do not alter the `_combat_should_end` predicate.** It is byte-identical to `upstream/main` and
   plan-mandated; reverting it restores the pre-sync defect (combat never ending while a distant
   critter sits in the list). The open item is its regression test, not the code.
6. **Do not fix the N2 class at the producer alone** (`src/loadsave.cc:590-593`). That closes
   today's quicksave producer and leaves the invariant open to any future off-page cursor writer;
   the scan belongs in `_GetSlotList` (`:2862`), where all three callers meet it.
7. **Do not silently carry the `clamp(8→16)` hunk** of `09677395`. Either record the omission (as
   §5 entry 5 does) or carry it deliberately — the RPU stand ships `art_cache_size=8`.
8. **Do not touch the save-slot page model** while working near N2: `saveLoadPages` stays runtime
   (§4 narrowing 2), `gLoadSaveSlotPageLoaded` keeps its 100 entries (the page-bounds proof
   depends on that shape), and no `_LSstatus` memset may be added.

## 10. Using this document in a future sync

1. Read this document **before** the next `git merge upstream/main`. `f6254ed5` is the new base, so
   anything here that was SKIPped or CONFLICT-DISCARDED will not surface as a conflict.
2. Re-evaluate a discarded commit only against the *current* code: the reasons above name the
   fork behaviour that blocked the port, and that behaviour may since have changed.
3. Keep the rule: a value class (V/M/S) or no adoption; the fork wins every conflict; no typed-
   refactor scaffolding; `files/fallout2.cfg` is never ported.
4. Re-run the same gate (all-target build, full `ctest`, the six probes, allow-list and
   protected-file audits) and record the counts here or in the next sync's document, so a
   regression has a baseline to fail against.
