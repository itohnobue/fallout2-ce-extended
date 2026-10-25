# Knowledge Base
Last updated: 2026-10-25T05:31:06.001811

## [pat-20260704221900-debed7]
Category: pattern
Tags: fallout2-ce, hooks, pattern
Changed: 2026-07-05T01:11:27.626093

fallout2-ce hook registration pattern: define enum in sfall_script_hooks.h, implement register+fire functions in sfall_script_hooks.cc, call from engine code (object.cc/light.cc/interface.cc). ScriptHookCall constructor takes (hookId, maxReturnValues, {args}); maxReturnValues=0 = observe-only, >0 = script can override via ret values. register_hook_proc and register_hook_proc_spec have SEPARATE handlers (F-15); sfall 4.2+ semantics = register_hook_proc adds to the BEGINNING, register_hook_proc_spec to the END (CE matches sfall 'BackwardHooksRegistration'); mods depending on execution order break silently if the order differs.

## [dis-20260704221900-a1b096]
Category: discovery
Tags: fallout2-ce, opcodes, architecture
Changed: 2026-07-04T22:19:00.795634

fallout2-ce opcode dispatch architecture: map-based registration with opcode & 0x3FFF mask (14-bit, 16384 slots), dispatch reads via opcode & 0x3FF (10-bit, 1024 slots) — this inconsistency was fixed as F-05. Sfall extended opcodes registered in sfall_opcodes.cc, standard opcodes in interpreter.cc/interpreter_extra.cc/interpreter_lib.cc.

## [ent-20260704235834-2f23d4]
Category: entity
Tags: fallout2-ce, repository, forgejo, github
Changed: 2026-07-04T23:58:34.318315

fallout2-ce-extended repo: origin = Forgejo/Gitea at git.aoizora.ru/nobu/fallout2-ce-extended (ssh://git@git.aoizora.ru:2222/nobu/fallout2-ce-extended.git, public); upstream = github.com/fallout2-ce/fallout2-ce (the FO:CE org fork). Local checkout: ~/Git/fallout2-ce-extended. This is the fork's home for engine work (sfall, hooks, bug fixes, SpeedMulti, test framework, Emscripten preset); future work goes here, not the original fallout-ce repo. Releases are published via the update-latest-release skill (GitHub rolling release + committed release/ folder).

## [dis-20260705011122-e81b5f]
Category: discovery
Tags: sfall, compatibility, ception, corrected
Changed: 2026-07-05T01:11:22.081688

CRITICAL TABLE CORRECTION: get/set/reset_critical_table (0x81E1-0x81E3) ARE fully implemented in CE at sfall_opcodes.cc:459-501,2090-2095. test_criticals.cc passes. Research reports incorrectly flagged as NOT IMPLEMENTED. Et Tu's OverrideCriticalTable=3 IS supported.

## [arc-20260705163503-a8427b]
Category: architecture
Tags: metarules, sfall, f2ce
Changed: 2026-07-05T16:35:03.450020

sfall_metarules.cc: 76 active metarule entries including Rotators fork wrappers (r_get_ini_string, r_message_box). New state: gNpcEngineLevelUpEnabled, gSavedOriginalDude, gQuestFailureValues (map<int,int>), gScriptNameOverride, gWorldmapHealTime, gRestHealTime, gTerrainNameOverrides (map<pair<int,int>,string>), gFakePerksNpc/TraitsNpc/SelectablePerksNpc (unordered_map<Object*,unordered_set<string>>). All state reset in sfall_metarules_reset(). 18 new static handlers implemented.

## [got-20260705203414-3a5e42]
Category: gotcha
Tags: fallout2-ce, sfall, compilation
Changed: 2026-07-05T20:34:14.759904

fallout2-ce-extended: STAT_SNEAK is not a valid constant in the codebase. Use skillGetValue(gDude, SKILL_SNEAK) from skill_defs.h instead.

## [dis-20260705221054-6d4639]
Category: discovery
Tags: fallout2-ce, rpu, et-tu, sfall
Changed: 2026-07-05T22:10:54.268090

fallout2-ce-extended: RPU v34 has 1518 .int files, et tu v1.16.3771 has 1049 .int files. Engine has ~90%+ sfall opcode/metarule coverage. Two MUST_FIX opcodes were set_hero_style (0x8215) and set_hero_race (0x8214) — implemented via sfall_gl_vars_store. Three parity metarules (set_town_title, set_car_intface_art, set_rest_mode) implemented as store-only stubs. Engine reports sfall version 4.5.1.

## [got-20260705221100-205ec6]
Category: gotcha
Tags: fallout2-ce, sfall, stat, perk
Changed: 2026-07-07T21:55:18.911594

Engine description/limit tables are file-static (internal linkage) with no public getters: gStatDescriptions[] in stat.cc (only statSetMaxValue/statSetMinValue setters exist; a kDefaultStatLimits mirror table is used for sfall metarule limits) and gPerkDescriptions in perk.cc (use perkSetMinLevel/perkGetMinLevel from perk.h). To expose such data, add accessors in the .h or mirror the initializer values.

## [dis-20260706142125-67fd68]
Category: discovery
Tags: fallout2-ce, rpu, etu, compatibility
Changed: 2026-07-06T14:21:25.016809

RPU compatibility is complete (zero gaps). Et Tu requires the gFallout1Behavior flag for FO1-mode behavior gating across combat, encounters, rest, reactions, and worldmap (implemented across ~17 files). Scope decision 2026-08-17: goal is 100% RPU + et tu support, not 100% sfall support.

## [got-20260706142125-5f007b]
Category: gotcha
Tags: fallout2-ce, sfall, timer, sid
Changed: 2026-07-06T14:21:25.160790

fallout2-ce-extended: Program struct has no 'sid' member. To get SID from Program*, must traverse global scripts list. scriptAddTimerEvent stores param as data arg, not opcode. PendingTimerEvent needs timerId field for per-event removal.

## [ref-20260706183422-02b473]
Category: reference
Tags: et-tu, fo1in2, modding
Changed: 2026-07-06T18:34:22.428517

Et Tu mod GitHub: github.com/rotators/Fo1in2. Uses sfall 4.5 with Rotators fork extensions. Key files: ddraw.ini (947 lines), ddraw.fo1in2.ini, config/fo1_settings.ini. Global scripts: gl_fo1mechanics.ssl (11 hooks), gl_worldmap.ssl, gl_classic_wm.ssl, gl_partyarmor.ssl, gl_autodoors.ssl, gl_car.ssl, gl_destroy_armor.ssl

## [got-20260706200521-0f6794]
Category: gotcha
Tags: sfall, interface, save-load, dialog, stale-handle
Changed: 2026-07-06T20:05:25.215162

Domain F4 save/load landmines (sfall interface/message/dialog): (1) opSayMessage (0x8054) causes a fatal error -- dialogMessage() always returns -1, triggering programFatalError at interpreter_lib.cc:1047; vanilla dialog opcodes (say_message, say_end, say_option) are landmines on CE (RPU/Et Tu use sfall metarules instead; any mod script calling the vanilla opcodes crashes). (2) Interface overlay (gInterfaceOverlayState) saves/restores windowHandle across save/load, but windows are transient (destroyed on map exit) -- after load, active=true with a stale handle = phantom overlay that can't render (affects Et Tu fo1_interface). (3) message_box mf_message_box saves dialogShowCount>0 if saved mid-dialog; on load the counter is non-zero but scripts are enabled, the next call increments to 2 then decrements to 1, never reaches 0, so scriptsEnable() is never called -- scripts permanently disabled.

## [dis-20260707035800-0bb9e0]
Category: discovery
Tags: fallout2, security, path-handling, hardening, CWE-121
Changed: 2026-07-07T03:58:00.117822

Fallout2 CE path-handling security audit: compat_makepath() has pointer-arithmetic bugs at platform_compat.cc:168,189,196,206 -- path++ can escape the COMPAT_MAX_PATH (260-byte) buffer and strchr() reads OOB (CWE-121 stack overflow; F-01 CRITICAL). There are 115+ COMPAT_MAX_PATH stack buffers across ~50 src/ files; compat_makepath/splitpath are the path-assembly API, and 3 calls in xlistEnumerate() are the crash trigger. Also unbounded strcpy at xfile.cc:761 (F-03) and xfile.cc:655 (F-04). Release builds had NO -fstack-protector / _FORTIFY_SOURCE. Fix ranking: Phase1 compat_makepath, Phase2 compiler flags, Phase3 snprintf, Phase4 std::filesystem.

## [got-20260707090024-0928cf]
Category: gotcha
Tags: sfall, voodoo, et-tu, fallout2-ce
Changed: 2026-07-07T09:00:24.311563

VOODOO memory patches from sfall's ddraw are unconditional no-ops in CE (write_byte/write_int stubs at sfall_opcodes.cc:3038-3063) -- a different address space means raw memory writes cannot work, and AllowUnsafeScripting is intentionally unwired. Consequence: EtTu Fallout1Behavior=1 silently produces NO FO1 behavior changes on its own. CE must implement equivalent behavior natively via gFallout1Behavior (reaction thresholds 25/-25, rest string offset 320, hit-chance nerf revert, Dogmeat PID, encounter dialog disable).

## [got-20260707134231-c96f7a]
Category: gotcha
Tags: fallout2, sfall, opcodes
Changed: 2026-07-07T13:42:31.530717

fallout2-ce-extended: F-13 set_critter_skill_mod uses 3 args (critter, skill, mod), set_base_skill_mod uses 2 args (skill, mod). Per-skill maps gBaseSkillModMap and gGlobalCritterSkillModMap must be separate to avoid double-application in skillGetValue.

## [got-20260707134231-785910]
Category: gotcha
Tags: fallout2, stat, ub
Changed: 2026-07-07T13:42:31.683888

fallout2-ce-extended: std::clamp UB when min > max affects 200+ call sites in critterGetStat. Guard with 'if (min > max) return value;' before clamp, or cross-validate in setters.

## [got-20260707215518-67cbe9]
Category: gotcha
Tags: fallout2-ce, sfall, fo1, level-cap
Changed: 2026-07-07T21:55:18.835522

FO1 level cap in fallout2-ce-extended: PC_LEVEL_MAX was hardcoded to 99 unconditionally. Fix adds statGetLevelCap() that returns 21 when gFallout1Behavior=true. Without this, FO1 mode lets players level to 99 instead of 21.

## [pat-20260707215518-388610]
Category: pattern
Tags: fallout2-ce, doctest, testing
Changed: 2026-07-07T21:55:18.987118

doctest does not have SUCCEED macro — use CHECK(true) instead. Also forbids || inside CHECK — assign to bool first. CHECK(expr == false) fails on Result::operator== — use CHECK_FALSE(expr).

## [got-20260708082707-e77868]
Category: gotcha
Tags: fallout2-ce-extended, build
Changed: 2026-07-08T08:27:07.664767

fallout2-ce-extended: After adding -Wall -Wextra (F-M45), pre-existing warnings surface but are NOT errors unless -Werror is set. Build succeeds with warnings.

## [pat-20260708082720-16c1e7]
Category: pattern
Tags: fallout2-ce-extended, testing, cmake
Changed: 2026-07-09T09:33:13.105802

Test infrastructure (fallout2-ce-extended): doctest is header-only; test targets that #include production headers need target_include_directories(test_NAME PRIVATE src) in tests/CMakeLists.txt; TEST_ACCESSORS_ENABLED must be set on the test_sources target (sfall_ini/sfall_opcodes test accessors). test_sources links config.cc/sfall_global_vars.cc/sfall_arrays.cc/sfall_global_scripts.cc/sfall_ini.cc/sfall_lists.cc/sfall_kb_helpers.cc; test_stubs provides debugPrint/compat_* stubs. CMakeLists.txt registers each test with add_executable+target_link_libraries+add_test. Files with 40+ engine deps use the self-contained mirror pattern.

## [got-20260708150217-e61166]
Category: gotcha
Tags: sfall, arrays, reset, state-leak
Changed: 2026-07-08T15:02:17.865594

sfallArraysReset does NOT reset expressionArrayId or arrayExpressionStack — stale expression state persists across game reset cycles. SetArrayFromExpression can target freed arrays (silently caught by get_array_by_id null check). See sfall_arrays.cc:646-654.

## [got-20260708223517-3f9855]
Category: gotcha
Tags: fallout2-ce, critical, stack, review
Changed: 2026-07-08T22:35:17.491446

Fix agents can reverse stack pop order when adding validation — always have second-opinion reviewers check LIFO stack semantics in script VM code. CRITICAL F-EXT-01 was caught only by cpp-pro second opinion, not by the primary code-reviewer.

## [pat-20260708223526-160f26]
Category: pattern
Tags: fallout2-ce, saveload, bounds, pattern
Changed: 2026-07-08T22:35:26.055561

Unbounded save load loops: sfall_opcodes.cc has 12+ load-from-save paths using sfall_gl_vars_fetch + for loop. All need && count <= kMaxConstant guards. Pattern: hcCount, kcCount, cpmCount, savedCount, skCount, gcCount, crtCount, skCount2, fasCount, dasCount. Fix pattern is identical for all: add cap constant + guard condition.

## [got-20260709002729-9acdb9]
Category: gotcha
Tags: fallout2-ce, sfall, compatibility, false-positives
Changed: 2026-07-09T00:27:29.704240

sfall script macros: set_attack_explosion_radius, set_attack_is_explosion_fire, debug, debug_warning, debug_blue, debug_yellow are all SCRIPT-LEVEL MACROS (defined in sfall.h/debug.h) that expand to metarule2_explosions or debug_msg calls — NOT separate opcodes or metarules. CE already handles them. Do not add them to opcode/metarule dispatch tables.

## [dis-20260709002735-a704a1]
Category: discovery
Tags: fallout2-ce, sfall, hooks, compatibility
Changed: 2026-07-09T00:27:35.467047

HOOK_ENCOUNTER arg contract: CE's first 3 args (eventType, mapId, isSpecial) MATCH sfall's 3-arg contract. CE's original comment claiming sfall used (encounterType, tile, forcedFlag) was WRONG. Sfall docs (hookscripts.txt) confirm: arg0=0=random encounter, arg0=1=player enters from worldmap — same as CE's RandomEncounter=0, LocalMapEnter=1. CE extends with 2 extra args (tableId, entryId) for enhanced context.

## [got-20260709030926-df4172]
Category: gotcha
Tags: sfall, combat, integration
Changed: 2026-07-09T03:09:26.722731

SpraySettings.flags field is set via set_spray_settings metarule and saved/loaded but never consumed by combat spray code at combat.cc:3897-3924 — only pid, radius, count are read, flags is ignored

## [got-20260709030930-42c1df]
Category: gotcha
Tags: sfall, npc, display
Changed: 2026-07-09T03:09:30.764116

NPC fake perks/traits (gFakePerksNpc/gFakeTraitsNpc/gFakeSelectablePerksNpc in sfall_metarules.cc) are file-static with no public getter — scripts can query via metarules but engine UI cannot display them on NPC character sheet

## [got-20260709055808-76fa79]
Category: gotcha
Tags: fallout2-ce, sfall, save-format, backward-compat
Changed: 2026-07-09T06:38:52.363154

sfall_global_vars save format v2 adds a stringVars section (length-prefixed records) after the float section; the header grew 16->20 bytes (added stringCount field). Backward compatible: the version>=2 check reads strings, v1 saves load fine. Keys follow an 8-char convention for direct uint64_t packing. sfall_gl_vars_fetch_string() returns a new[]-allocated copy -- caller must delete[]. Test: test_global_vars.cc.

## [pat-20260709063857-4940ab]
Category: pattern
Tags: fallout2-ce, sfall, save-format, metarules
Changed: 2026-07-09T06:38:57.677207

fallout2-ce-extended: METARULES_SAVE_VERSION bumped 6→7 for gTalkingHeadMood persistence. Version-gated load: if (version >= 7) read gTalkingHeadMood. Old saves (v6) default to -1 from reset().

## [con-20260710052652-500f4e]
Category: config
Tags: items, compatibility, documentation
Changed: 2026-07-10T05:26:52.778269

SFALL_COMPATIBILITY.md: HOOK_ITEMDAMAGE has undocumented contract difference — CE arg[4] is hitMode (HIT_MODE_*) while sfall arg[4] is 'type' (attack type flag). No current mods use this hook, so zero impact. Explosion runtime settings (set_explosion_radius, set_attack_explosion_pattern, etc.) are not persisted in savegames — undocumented behavior that matches sfall but should be noted.

## [dis-20260710055607-942bd8]
Category: discovery
Tags: sfall, opcode, compatibility
Changed: 2026-07-10T05:56:07.474470

F-01: CE repurposes sfall opcode 0x1FD from fs_write_int (duplicate alias) to fs_write_float. Semantic mismatch for scripts compiled expecting int-write behavior.

## [dis-20260710055608-e52d19]
Category: discovery
Tags: sfall, compatibility, globals
Changed: 2026-07-10T05:56:08.709243

F-04: get_sfall_global_int returns INT_MIN sentinel for missing keys instead of sfall contract's 0. RPU scripts use game_time comparison (not 0), so unlikely to hit RPU but breaks pure-sfall mods.

## [got-20260802233748-f0cc36]
Category: gotcha
Tags: regression, stat, fallout2-ce
Changed: 2026-08-02T23:37:48.334877

fallout2-ce-extended: VALUE-SEMANTICS MISMATCH when copying guard patterns to sibling functions. Pass-13 7f58356 mirrored critterSetBaseStat's absolute-value min/max write-side clamp onto critterSetBonusStat, which stores SIGNED DELTAS (negative=radiation/addiction/drug-wear-off/armor-removal/level-down-HP, zero=reset, positive=drugs/perks/armor) — rejected all legit 0/negative deltas, breaking radiation death, drug expiry, addiction, editor reset, sfall scripts; 77fe02f protoMarkDirty persisted transient deltas into NPC .pro files (permanent corruption). C-05 CRITICAL + stat N-03 HIGH. Lesson: when copying a guard to a sibling function verify the VALUE SEMANTICS match (absolute vs signed-delta vs sentinel); validation belongs at the single point where semantics are known (read-side/effective value), not at storage; a function with no documented contract gets re-'hardened' into regression by every audit pass — write the contract comment (fix: delete clamp + remove protoMarkDirty + read-side clamp stays).

## [got-20260802233750-8b7803]
Category: gotcha
Tags: regression, memory, fallout2-ce
Changed: 2026-08-02T23:37:50.526040

fallout2-ce-extended: INCOMPLETE-FIX CLUSTERS — fix families, not sites. Pass-17 2fabd98 fixed only 1 of 5 _GNW_check_buttons UAF sites (H-13; +window N-04 group-callback, N-01 6th site, P-08 later); M-142 fixed tile roof but missed the floor twin; M-84 found ~32 unguarded protoGetProto derefs + H-25 null deref; H-16 PCX overflow's twin underflow (art NEW-01) and H-17 DAT dataSize's twin (P-16 tuple validation) and M-77 tile-load root covering map NEW-2 + P-17 all surfaced in later passes. Lesson: when fixing an OOB/UAF/bounds defect, grep the WHOLE family (sibling functions, twin paths, reverse polarity, same-root callers) in one pass — one-site fixes leave siblings the next audit re-finds; a root-cause fix at load (object.cc:438 tile validation) closes multiple sites.

## [got-20260802233757-cc4207]
Category: gotcha
Tags: save-format, regression, backward-compat, fallout2-ce
Changed: 2026-08-02T23:37:57.328671

Save-format discipline & architecture: (1) Architecture -- save = fixed 131B header + sequential handler chunks; saves serialize only runtime object state (hp, ammo, ...), NOT proto structs, so FO2 proto-size differences don't affect save compatibility; the config system is fully extensible (the f1_res.ini pattern is reusable for fo1_settings.ini/ddraw.ini). (2) Discipline -- ANY persistent-format change MUST bump the format version: handler count 27->28 (lightSave/lightLoad added, C-04) and header-CRC added ungated (C-03; pass-11 0e4b74e claimed F2-21 but had no version gate) together made every old save abort/reject as CORRUPT; M-62 mid-stream float broke 1.2R alignment. Fix shape: VERSION_MINOR 3->4, version-selected handler lists (legacy 27 vs 28), header CRC verified unconditionally. Corollary: NEVER gate a checksum/protection read on the SAME version field it protects (save NEW-3 -- a single versionMajor 3->2 byte flip disables all CRC). The original hardcoded LOAD_SAVE_HANDLER_COUNT=27 positional chain had a DECORATIVE version check (SLOT_STATE_UNSUPPORTED_VERSION was unreachable, no version enforcement) -- the root prediction here.

## [got-20260802233802-266820]
Category: gotcha
Tags: sfall-compat, regression, fallout2-ce
Changed: 2026-08-02T23:38:02.495724

fallout2-ce-extended: sfall-compat divergence — verify compat layers against the REFERENCE SOURCE (sfall C++ modules: FileSystem.cpp, Stats.cpp, MiscHs.cpp, hooks.yml), NOT docs or the fork's own comments. Pass-18 evidence: P-05/H-03 — fork comment claimed LocalMapEnter=2, sfall reference proves arg0=1 LocalMapEnter / special in arg2 / forced encounters never fire HOOK_ENCOUNTER; M-64 — fork treats WorldMapFPSPatch as FPS divisor (1000/1 = 1000ms → ~1 FPS world map with shipped Et Tu ddraw.ini) where sfall treats it as boolean enable + WorldMapDelay2; sfall NEW-2/P-06/P-07 — fork VFS writes real disk (fs_delete flips handle, fs_create truncates) where sfall is in-memory; sfall N-02 — HOOK_GAMEMODECHANGE arg0 rationale wrong; P-04 — mf_opcode_exists needs exact sfall range [0x8000,0x8300) not 0x3FF mask. Every divergence broke mod compatibility. Lesson: when a compat shim differs from reference behavior, the reference source wins — read it, don't reason from comments/docs.

## [got-20260802233804-2cfe0e]
Category: gotcha
Tags: regression, review, fallout2-ce
Changed: 2026-08-02T23:38:04.676961

fallout2-ce-extended: FIX AGENTS OVER-APPLY SCOPE — post-fix review of pass-1/2 fixes found the fixes themselves regressed: R-01 duplicate default argument on aiHaveAmmo (build-breaking, introduced BY the combat N-01 fix), R-02 walkability polarity over-flipped beyond the finding (map N-01 fix inverted the terminal return that was already correct), R-11 UAF moved 2115→2110 not closed, F1/F2 load re-registration clobber (R-14 fix introduced activePid=0 clobber + damage-lookup inversion), R-06 apply block landed as a silent no-op, F4 short-table XP runaway in the R-06 fix, F3 broken test SUBCASE in the R-15 fix. Lesson: touch ONLY the cited lines — do not 'fix forward' beyond the finding; polarity/scope over-application and moved-not-closed bugs are the #1 fix-introduced regression class; the adversarial post-fix review catches what the fix agent's self-check misses.

## [got-20260802233813-e1e53b]
Category: gotcha
Tags: regression, memory, fallout2-ce
Changed: 2026-08-02T23:38:13.129852

fallout2-ce-extended: DEFENSIVE GUARDS ADDED WITH INVERTED POLARITY convert latent UB into deterministic crashes — the 'fix' makes things worse. map N-01: f874424 added wmWorldPosInvalid guards that return false ('not invalid' → walk continues) for exactly the OOB coords they were meant to catch → deterministic null deref at worldmap.cc:4923 (the only unguarded site). Same class in one pass: M-55 (_check_ranged_miss final guard was ==0, roll==SUCCESS only for solid critters → always false), M-68 (wmDrawCursorStopped !=0 misclassified the -1 idle sentinel as walking), R-02 (follow-up over-flipped the terminal walkability return ==0/!=0, stopped the party on step 1; only byte-diff vs upstream CE caught it). Lesson: when adding a guard for invalid input, verify WHICH return value the caller treats as 'stop/bail' — a guard that returns 'valid' for invalid input is worse than no guard; check the callers' expectation, and diff against upstream semantics.

## [got-20260802233825-7c4b0a]
Category: gotcha
Tags: test, regression, fallout2-ce, coverage
Changed: 2026-08-02T23:38:25.892371

Test-quality findings (E-Test suite): CODIFYING MIRROR TESTS invert regression detection -- mirrors that duplicate production logic can diverge AND encode the bug as expected behavior, so CI validates the regression. Examples: M-088/test_map.cc:447 asserted the .edg EOF bug as correct; F-55/test_fixes_saveload.cc encoded the 0x3FFF opcode-mask bug; UF-H-020 asserted critterSetBonusStat bonus-0->-2 (C-05 clamp bug); test_stat blessed the XPTable no-op; M-99 windowWordWrap had guards production lacked. Lesson: rewrite the codifying mirror in the SAME pass as the fix (converts it to regression-detecting -- a future re-add FAILS CI); mirrors give false confidence -- prefer production-linked tests or periodic mirror-vs-production drift checks. Audit scale: 45 test files (~42K LOC) -- 2 CRITICAL (7 TODO placeholders with CHECK(true), knockback opcodes untested), 5 HIGH (mirror test fallacy, VFS fs_resize mode mismatch, 150 file-static opcodes untestable, worldmap subtile skip, broken-code test), 9 MEDIUM, 4 LOW; coverage gaps: skill opcodes 0%, combat advanced 0%, lock/event/utility opcodes 0%; 38/45 tests were self-contained mirrors.

## [got-20260817000651-b24b40]
Category: gotcha
Tags: sfall, config, et-tu
Changed: 2026-08-17T00:06:51.285614

fallout2-ce-extended: contentConfigTryMigrateFromSfall writes gSfallConfig preset '0' defaults into data/config/game#patch.cfg BEFORE contentConfigInit's configRead — any set-if-absent seeding of migrated keys is defeated by migration-written zeros; seed unconditionally (et tu gate keys use this pattern)

## [got-20260817040334-6608fe]
Category: gotcha
Tags: automap, savegame, format
Changed: 2026-08-17T04:03:34.840993

fallout2-ce-extended: AUTOMAP.DB is copied into save slots (loadsave.cc _GameMap2Slot/_SlotMap2Game) despite being a per-session cache — format changes need a version bump (AUTOMAP_DB_VERSION 1->2 for the 173-map layout) + automapEnsureCurrent() regeneration on stale/corrupt header. dataSize must be derived from AUTOMAP_MAP_COUNT, never hardcoded

## [got-20260817040334-283c10]
Category: gotcha
Tags: sfall, art, cache, config
Changed: 2026-08-17T04:03:34.917949

fallout2-ce-extended: sfall OverrideArtCacheSize=1 sets a FIXED 261 MB art cache (sfall 4.3.2 changelog: '261 instead of 256'; source SafeWrite32(0x418872, 256) pre-4.3.2). No ArtCacheSize key exists in sfall. CE honors it via [Misc] ArtCacheSize default 261 with 8..512 clamp. Beware: readme LINE numbers (e.g. 'readme:361') get misread as values — verify against changelog/source

## [got-20260817040334-1df53a]
Category: gotcha
Tags: sfall, dialog, female, rpu
Changed: 2026-08-17T04:03:34.990701

fallout2-ce-extended: sfall FemaleDialogMsgs selects text/<lang>/dialog_female (level>=1) and cuts_female (level>=2) for a female player with per-directory fallback to normal dirs when absent (English ships none — byte-identical). Non-English RPU dirs: french/italian/polish/portuguese/russian/spanish/vietnamese ship BOTH; czech/german/hungarian/swedish ship cuts_female ONLY

## [got-20260817195002-4bc8cb]
Category: gotcha
Tags: sfall, perk, et-tu, config
Changed: 2026-08-17T19:50:02.002430

sfall PerksTweak semantics (PerksFile): keys are FLAT replacements of the engine's hardcoded perk bonuses — value < minGate is NOT applied (FO2 default stays), value > maxClamp is clamped. Skill bonuses 0..125; NightVision 0..100 (% of max light, per rank, 65536*20/100==65536/5); CautiousNature -12..20; Stonewall 0..100; WeaponHandling 0..10; Demolition/Salesman/Healer 0..999; Educated/Lifegiver 0..125; MasterTrader >=0 no clamp; Comprehension value+100; VaultCityInoculations clamped both sides [-100,100] with NO gate. PerksTweak applies whenever the file exists (no Enable gate); [Perks] section is Enable-gated. Verified from sfall-team/sfall dllmain/Perks.cpp via grep.app.

## [got-20260817195003-0d5b07]
Category: gotcha
Tags: sfall, perk, gotcha, init-order
Changed: 2026-08-17T19:50:03.438292

Perks.ini [Perks] section values must NOT go into the sfallPerk*Overrides arrays (sfall_opcodes.cc) — those reset on game reset (sfallOpcodesReset). Apply file values directly to gPerkDescriptions at init (perksLoadCustomConfig in perk.cc); script set_perk_* opcodes still win because perkCanAdd checks the override arrays first. Ranks=-1 removes a perk via maxRank=-1 (perkCanAdd false) but perkAddForce skips its cap for maxRank==-1 and perkAddEffect's maxRank==-1 stat-loop activates — same as sfall's engine patch behavior (latent hazard, unreachable with shipped et tu scripts).

## [dec-20260817200421-bbcbdf]
Category: decision
Tags: scope, decision, rpu, et-tu, sfall
Changed: 2026-08-17T20:04:21.022304

Project scope (2026-08-17, owner clarification): goal is 100% support of RPU and et tu — NOT 100% sfall support. sfall-parity QoL features are out of scope unless a supported mod depends on them. Verified: no et tu/RPU script reads CombatControl, Highlighting, UseScrollingQuestsList, ItemCounterAutoCaps, DeathScreenFontPatch, EnableMusicInDialogue; et tu's sfall-mods.ini [CombatControl]/[Highlighting] sections are inert (the consuming sfall mod scripts are not shipped). RPU support complete; et tu remaining: [Traits] section (NoHardcode) + runtime verification.

## [got-20260817202241-ffa209]
Category: gotcha
Tags: sfall, perk, trait, et-tu, config
Changed: 2026-08-17T20:22:41.906807

Perks.ini [Traits] section: trait IDs are positional — et tu (FO1) numbering puts Night Person at index 13 where FO2/CE's enum has TRAIT_SEX_APPEAL; the index maps directly, display name comes from the mod's trait.msg. NoHardcode gates ONLY the trait_adjust_stat/skill contributions (trait.cc) — character-editor effects (Skilled +5 sp/level, perk progression) and combat-code effects (FINESSE DR, FAST_SHOT AP, JINXED, ONE_HANDER, BLOODY_MESS, CHEM_*) are outside trait_adjust_* and untouched, mirroring sfall. StatMod/SkillMod apply unconditionally — sfall's format has no day/night condition (et tu's 'at day' comment is descriptive); FO1 Night Person's night +1/+1 swing is NOT replicated (known fidelity note).

## [dis-20260817234034-1e8004]
Category: discovery
Tags: et-tu, fo1in2, sfall, compatibility, fallout2-ce
Changed: 2026-08-17T23:40:34.386785

Et Tu (Fo1in2) engine requirements — UPDATED 2026-08-17: ~100+ sfall opcodes, 90+ metarules, 40+ hooks. Rotators fork detection (read_byte(0x410003)==0xF4 + metarule_exist(rotators)) WORKS since 172c78b/89d82aa (sfall_opcodes.cc:158-162, sfall_metarules.cc:1989-2000). HOOK_ADJUSTRADS implemented (critter.cc:492). Deliberately absent by design: HOOK_REMOVEINVENOBJ, HOOK_BESTWEAPON, HOOK_ROLLCHECK (documented rationale, not gaps).

## [pat-20260817234040-49e5ec]
Category: pattern
Tags: sfall, fastshotfix, combat, et-tu, fallout2-ce
Changed: 2026-08-17T23:40:40.454462

sfall FastShotFix per-mode contract (verified against sfall source 2026-08-17): 0 = FO2 (-1 AP ranged only, aimed disabled); 1 = Haenlomal (same AP, aimed ENABLED for melee/unarmed/HtH); 2 = alt (-1 AP all weapons); 3 = FO1 (-1 AP all weapons, aimed disabled). sfall impl: item_w_mp_cost_sub — mode 3/2 require non-null weapon item (HtH never reduced), modes 0/1 gate type>MELEE && range>=2, floor cost>=1, reduction runs BEFORE HOOK_CALCAPCOST. CE+et tu: mode-2/3 melee-class reduction defers to the hook layer when HOOK_CALCAPCOST registered (gl_apcost.ssl supplies it once).

## [got-20260817234041-5d33ed]
Category: gotcha
Tags: config, overload, worldmap, perk, gotcha
Changed: 2026-08-23T16:16:35.059861

C++ overload pitfalls that silently change behavior at existing call sites: (1) configGetInt(&config, section, key, ptr, default) -- the 5-arg DEFAULT OVERLOAD WRITES the default into *ptr when the key is absent, clobbering a prior value; use the 4-arg present-semantics overload when a default must not override (hit in the F-072 worldmap start_pos bug, worldmap.cc:1352-1354, fixed 2026-08-17). (2) Adding a typed overload next to an int-overload (e.g. perkGetMaxRank(Perk) vs perkGetMaxRank(int)) changes ALL call sites passing a typed enum -- C++ prefers the exact enum match, bypassing the int overload's extra semantics (set_perk_ranks override, -1 invalid contract). When merging upstream code that adds an enum-typed overload, grep callers first; if semantics differ, drop the new overload (upstream #698 -> fork fix 6744652a).

## [got-20260817234044-9c74f2]
Category: gotcha
Tags: et-tu, sfall, opcode, gotcha, fallout2-ce
Changed: 2026-08-17T23:40:44.050120

et tu ce_enabled macro (fo1.h:62) = metarule_exist('opcode_exists') AND NOT opcode_exists(0x823B). 0x823B is sfall's modified_ini — CE MUST keep it unregistered: implementing it flips ce_enabled=false and disables ALL et tu CE-path script workarounds (gl_apcost.ssl AP handling etc.). Guarded by comment at sfall_opcodes.cc:8903.

## [got-20260818040918-4a9dc9]
Category: gotcha
Tags: fallout2, scripts, interpreter, wait, gotcha
Changed: 2026-08-18T04:09:18.310205

Fallout2-CE interpreter WAITING branch: upstream has 'if (!checkWaitFunc) continue'; our f40c961 commit inverted it to 'if (checkWaitFunc) continue' — any wait() from an event-proc -1 dispatch busy-spins the main thread (100% CPU, game-time frozen, input starved, right-click taps lost) until real-time wait elapses. Fixed via programYieldIfWaiting() helper (pending->yield break, elapsed->clear+false) at interpreter.cc:3175-3182 + events entry guard 3491-3493. M-46 skip at scripts.cc:1653 is LOAD-BEARING (programSetupCall clears WAITING at interpreter.cc:3223 before proc dispatch — without M-46 the scriptExecProc channel bypasses the wait).

## [pat-20260818040919-744af0]
Category: pattern
Tags: fallout2, scripts, pattern, interpreter
Changed: 2026-08-18T04:09:19.448669

Program-wait handling in FO2 CE: 'a WAITING program yields its dispatch slot' must be expressed at EVERY ungated -1 dispatch channel — interpret loop (WAITING branch), programProcessProcedureEvents entry guard, kGlobalScriptBusyFlags. Choke-point guard at programExecuteProcedure would freeze dialogs (game_dialog.cc:2466 reply site deliberately re-enters dialog-WAITING programs) — guard placement must be per-channel, not global. Residual ungated sites: sfall hooks :232, sfall_animation :116, sfall_opcodes :3883/:6336, sfall_arrays :1314/:1387.

## [got-20260818181617-c2b3de]
Category: gotcha
Tags: fallout2, scripts, interpreter, uaf, root-cause
Changed: 2026-08-18T18:16:17.688032

Script Program* lifecycle & UAF: (1) F-034 (fork 36414b6) set program->exited=true in opExitProgram (O_EXIT_PROGRAM 0x8010 -- the NORMAL termination opcode); _updatePrograms then freed every script's Program right after its start proc ran, leaving script->program dangling (LOADED flag still set) -> dead world + UAF SIGSEGV in scriptExecProc->programExecuteProcedure. Upstream only sets the flag, never exited=true. Reverted; added programListContains() UAF detector + DBGTRACE lifecycle logging. (2) F-03: a Null Program* can reach programExecuteProcedure in scripts.cc:1429 after HOOK_STDPROCEDURE fires hook scripts (crash address 0x143 = null Program* offset) -- re-read program from script->program after the hook or add a null guard. 92/92 tests pass.

## [got-20260818184036-aa3c8a]
Category: gotcha
Tags: fallout2, scripts, messages, sfall, config, false-positive
Changed: 2026-08-18T18:40:36.159557

BoostScriptDialogLimit (ddraw.ini [Misc]) is a BOOLEAN toggle in sfall ('1 = boost script names 1450→10000'), but fork commit 5222087 assigned the raw value as absolute capacity → BoostScriptDialogLimit=1 set capacity to 1 → every script message lookup (list ID>=2) failed → 'Error' fallback text in message log. Fixed 2026-08-18: nonzero → max capacity (10000). Also: interpreter.cc heap-walk diagnostics used ptr>=heapEnd but the 0x8000 sentinel sits exactly AT heapEnd → false 'heap corruption' spam on every well-formed heap; changed to > in programMarkHeap/programPushString/interpreterPrintStats.

## [got-20260818192654-712c3d]
Category: gotcha
Tags: vfs, fs_copy, path, sfall, rpu, et-tu
Changed: 2026-08-18T19:26:54.185188

fs_copy / VFS file I/O (sfall_opcodes.cc): (1) FILE* handles handed to scripts (fs_create/fs_copy/fs_resize) are stdio-buffered by default -- fs_write_short writes sit in user space and are invisible to the engine's independent fopen reads during the session (RPU fs_copy same-path FRM FPS patching was silently inert). Fix: setvbuf(f, nullptr, _IONBF, 0) at open via sfallVfsFopen (2026-08-17). (2) fs_copy was disk-only (compat_fopen), so sources inside .dat archives (RPU goris-derobing art\critters\*.frm) or directory mods (et tu classicWM from mods/fo1_base) failed with 'cannot open source'. Fix (2026-08-18): sfallVfsReadFile() reads via the engine VFS (fileOpen) with a plain-fopen fast path; same-path fs_copy materializes to master_patches (data/) so the engine art loader sees patched bytes BEFORE the archive; different-path dest also materializes to the patches dir. (3) 0x81a3 eax_available / 0x81a4 set_eax_environment stubs added (et tu mods call them). Note: compat_stricmp is pure ASCII case-fold -- no '/' vs '\' separator normalization; sfallVfsResolvePath doesn't normalize either (only traversal/absolute/drive-letter rejection + root prepend); fs_copy's same-path branch compares RESOLVED paths case-insensitively.

## [got-20260818192655-46f906]
Category: gotcha
Tags: fallout2, worldmap, fade, merge, gotcha
Changed: 2026-08-18T19:26:55.104365

Worldmap black screen (both stands): fork kept stray wmFadeOut() at wmWorldMapFunc entry (worldmap.cc:3905) while upstream 2b43501 'Remove world map transition fades (#672)' removed BOTH fade-out and fade-in — incomplete merge left palette black for the whole worldmap session (music plays, travel works). Fixed: removed the stray wmFadeOut(); entry now matches upstream.

## [ref-20260818203343-2254ed]
Category: reference
Tags: fallout2, debug, instrumentation, smoke, dlog
Changed: 2026-08-18T20:33:43.806821

Debug instrumentation inventory for smoke builds: silent unless [debug] mode=log in fallout2.cfg — writes debug.log to CWD; console_output_path captures in-game console (rpu-boot.log/ettu-boot.log). Markers: [DBGTRACE] programCreate/programFree (interpreter.cc:667/524), UAF-DETECTED (scripts.cc:1651/1756), _scr_remove_all[_force] (scripts.cc:2979/3034), EDG[%p] zone geometry (map_edge.cc:142), MAP LOAD rc=total=ms (map.cc:904), heap-corruption sentinel diagnostics (interpreter.cc:781/845/3733, sentinel AT heapEnd so > not >=), VOODOO write/read NOT SUPPORTED (sfall_opcodes.cc via programPrintError, benign), sfall accepted-but-inert notes (sfall_config.cc:170/173). Build hash in git_version.h. Full table in AGENTS.md 'Debugging instrumentation (smoke builds)' section.

## [dis-20260818235558-8577bc]
Category: discovery
Tags: fallout2, map-format, et-tu, parsing
Changed: 2026-08-19T22:41:59.403049

FO2/et tu MAP file format: script section per type = count(int32), then ceil(count/16) extents; EACH extent = 16 script records + length(int32, validated 0..16) + next(int32, read+discarded) -- scriptListExtentRead scripts.cc:2545. Record sizes by SID_TYPE: SYSTEM=16 ints, SPATIAL=18, TIMED=17, ITEM=16, CRITTER=16 (64/72/68/64/64 bytes). Objects with inventory->length != 0 carry NESTED item records after their base record -- each item = quantity(int32) + full objectRead (18 ints + objectDataRead, recursively); omitting the recursion desyncs the walk. Critter base record = 72 + inv(3) + reaction/flags(1) + combat(7) + hp/rad/poison(3) ints. Object record = 18 ints (id,tile,x,y,sx,sy,frame,rot,fid,flags,elev,pid,cid,lightDist,lightInt,outline,sid,scriptIdx) + objectDataRead (objectWrite object.cc:704). Map header: 236 bytes + gv*4 + lv*4 + squares (10000 ints per present elevation; flags 0x2/0x4/0x8 = elev present).

## [got-20260819022058-a59ed2]
Category: gotcha
Tags: save, loadsave, global-clobber, path-resolution
Changed: 2026-08-19T02:20:58.288702

loadsave.cc _gmpath is a GLOBAL clobbered by save handlers mid-save: _GameMap2Slot rewrites it to slot-dir/AUTOMAP.DB.SAV paths while copying map files. Never build a rename destination from _gmpath after the handler loop — snapshot the path into a LOCAL first (save game fix 1ef6861; the incomplete b9a290c fix failed exactly this way: garbage dest like data\data\SAVEGAME\SLOT01\AUTOMAP.DB.SAV -> ENOENT -> 'Error renaming temp save file'). Also: fileOpen resolves relative paths through the directory xbase (master_patches), so any compat_rename/compat_remove of a file opened via fileOpen must use _patches-prefixed paths, never bare CWD-relative ones.

## [got-20260819024248-abacc4]
Category: gotcha
Tags: fallout2, input, SDL, macos, trackpad, tap
Changed: 2026-08-19T02:42:48.357036

Fast mouse clicks (trackpad taps) are LOST by the per-frame button-state polling design: mouseDeviceGetData polls SDL_GetMouseState once per frame, so a press+release completing between two polls (macOS two-finger tap / tap-to-click) generates NO right/left button event — the SDL_MOUSEBUTTONDOWN/UP events are queued, drained by _GNW95_process_message, but the engine state comes only from the poll. Physical corner clicks hold long enough to be caught. Fix (dinput.cc): latch SDL_MOUSEBUTTONDOWN buttons in gSyntheticDownButtons via mouseDeviceNoteButtonDown (input.cc drain), OR them into the polled state in mouseDeviceGetData, clear after one poll — burst becomes clean down/up pair; reset on focus loss/gain. TRAP: SDL button-state bits use SDL_BUTTON(X)=1<<(X-1) encoding — right button event value is 3, state bit is 0x04, NOT 0x02.

## [got-20260819025641-fae749]
Category: gotcha
Tags: fallout2, input, keyboard, hook, keypress, regression
Changed: 2026-08-19T02:56:41.612446

HOOK_KEYPRESS contract: arg0 is the DIK keyCode (console), arg1 the vkey, arg2 the pressed state, arg3 the keyID/left-right distinction; ret0=1 (or 255) consumes/blocks the key. This fork's input.cc treats -1 from sfall_kb_handle_key_pressed as BLOCK and 0/SDL_SCANCODE_UNKNOWN as pass-through -- the EXACT OPPOSITE of upstream fallout2-ce (-1 = pass, 0 = swallow). The Aug 16 sync merge (16bc1568 via b405e59) flipped the two no-hook early returns to -1, swallowing EVERY key when !gGameLoaded (main menu/char creation) or when no HOOK_KEYPRESS is registered (save-name dialog): text fields dead, backspace dead, held keys intermittently work. Fixed by restoring SDL_SCANCODE_UNKNOWN at those two returns; only ret0==255-swallow returns -1. Prior related defect: CE's HOOK_KEYPRESS passed a hardcoded 0 for the VK arg (RPU Party Orders depends on the left/right distinction) -- verify arg wiring. HOOK_MOUSECLICK needs x,y as arg1,arg2 from the SDL event. Regression tests: test_sfall_kb_helpers.cpp PRODUCTION handle_key_pressed sections.

## [got-20260819200251-0b9c45]
Category: gotcha
Tags: engine, config, smoke, teststand
Changed: 2026-08-19T20:02:51.435125

Smoke test-stand setup & ops: (1) The engine reads fallout2.cfg + ddraw.ini from the argv[0] directory (app bundle Contents/MacOS), NOT cwd; the bundle copy is the LIVE config (engine reads/writes next to the executable), while the stand-root copy is only the deploy template (game-folder cfg copies only work when launched with bare argv[0]). smoke/deploy.sh overwrites the bundle copy, wiping in-game persisted keys; edit the bundle copy directly for live changes and keep the stand-root copy in sync (re-run deploy after editing it). macOS 26 codesign treats every file in Contents/MacOS as a code object -- sign injected configs individually before re-signing the bundle. (2) Layout: app bundle in game folder (cwd = bundle parent); data paths in cfg resolve against cwd; Et Tu uses '..\master.dat' relative paths; RPU defaults (master.dat, data, mods) match the game folder layout. (3) The deployed binary MD5 differs from the tree build after deploy.sh re-signs (codesign rewrites the signature blob) -- compare the __text segment (offset 25632) instead. ctest on the Xcode multi-config build needs -C RelWithDebInfo. The engine auto-erases data/MAPS/*.SAV at launch (gameInitWithOptions->_InitLoadSave->MapDirErase), so stale snapshots self-clean.

## [got-20260819213552-410fe1]
Category: gotcha
Tags: combat, fallout2, issue-e, gotcha
Changed: 2026-08-19T21:35:52.328350

Combat overrideAttackResults: ORIGINAL FO2 binary (0x422F3C) writes BOTH CombatStartData result fields to defenderFlags (attackerResults write is dead, second wins); attackerFlags NEVER touched. The fork's split-field version (attackerFlags=attackerResults) wiped DAM_HIT when FO1 scripts call attack() with equal flags (0,0) — WanRats.int attack(dude,0,1,0,0,30000,0,0) — dodge anim + missed message + damage still applied (Issue E). Fixed combat.cc: restore defenderFlags-only writes. Disassembly: tmp/rpu/release/fallout2.exe VA 0x422F3C fileoff 0x1333C.

## [pat-20260819213557-5edaf2]
Category: pattern
Tags: scripts, ssl, disassembly, fallout2, tools
Changed: 2026-08-19T21:35:57.509159

FO1/FO2 .int disassembly: header 42B; proc table at 42 (BE int32 count, 24B entries {nameOffset,flags,time,conditionOffset,bodyOffset,argCount}); identifiers then staticStrings tables; bytecode = 2B big-endian words; PUSH variants (0x9001/0x9801/0xA001/0xC001/0xE001, index &0x3FF==1) consume 4B operands; opcodes 0x8000|index; other ops take operands from the value stack. Tools: tmp/probe/int_disasm.py (walker bounds buggy — prefer raw byte-scan for opcode words), int_dump.py. FO2 opcode docs: fodev.net/files/fo2/opcodes/ (attack=0x80D0 attack_complex(who,called_shot,num_attacks,bonus,min_damage,max_damage,attacker_results,target_results)).

## [got-20260819213601-18e5a4]
Category: gotcha
Tags: combat, scripts, et-tu, gotcha
Changed: 2026-08-19T21:36:01.902352

_gcsd lifecycle: only non-null during the FIRST combatant's turn of a script-started combat (_combat() nulls it after each turn). Script-started combat comes ONLY from the attack script opcode (0x80D0/0x80DD) — opAttackComplex sets overrideAttackResults=1 when data[1]==data[0] (the last two attack() args = attacker_results/target_results — FO1 scripts pass (0,0) so override fires constantly). Also: _gcsd damageBonus/minDamage clamp applies on MISSES too (upstream quirk, benign for et tu: rats pass bonus=0,min=0).

## [got-20260819224159-c9dd38]
Category: gotcha
Tags: proto, lst, map-format
Changed: 2026-08-19T22:41:59.331969

FO2 proto lookup: _proto_list_str (proto.cc:205) matches 'pid & 0xFFFFFF' against a 1-BASED line counter in <type>.lst — reading lst[index] 0-based in an offline parser is off by one. Proto .pro files: type field at offset 32, material at 36 (after pid/msgId/fid/lightDistance/lightIntensity/flags/extendedFlags/sid).

## [dis-20260819224159-c93cf1]
Category: discovery
Tags: et-tu, rendering, walls, shady-sands
Changed: 2026-08-19T22:41:59.475548

Et Tu Shady Sands 'striped homes': FO1 adobe walls = 16px/32px-wide FRM pieces at every other tile with 1x1 block.frm markers, two interleaved rows. Engine placement math (identical FO1/FO2) leaves 8px vertical slits every 48px at scale 1 (16px at scale 2) — slits are IN THE DATA, rendered faithfully by any engine. All 94 wall FRMs byte-identical to FO1 MASTER.DAT, size==w*h (no RLE). (Closed as data-faithful; see git log c8f927ff.)

## [got-20260819224159-59695c]
Category: gotcha
Tags: dat, dfile
Changed: 2026-08-19T22:41:59.547812

Fallout DAT files (footer format): entries table ends 8 bytes before EOF (entriesDataSize, dbaseDataSize int32s); per entry: pathLength(int32), path, compressed(1 BYTE), uncompressedSize/dataSize/dataOffset(int32). Entry data base = fileSize - dbaseDataSize. 'compressed' is a single byte, not int32.

## [got-20260820002435-d427db]
Category: gotcha
Tags: et-tu, install, walls, offsets
Changed: 2026-08-20T00:24:35.105623

et tu wall art: FO2's master.dat bundles legacy FO1 wall FRMs with DIFFERENT placement offsets than the FO1 GOG originals (e.g. ADB0989 xOff -12 vs -8, yOff 6 vs 11; deltas vary per file). The et tu map's wall layout needs the FO1 originals — the FO1 extraction MUST land in master_patches (data/), like the official undat tools do (outputPath + '\\data'), or the engine silently uses FO2's copies and walls render striped.

## [got-20260820163729-e38e94]
Category: gotcha
Tags: pipboy, rest, fo1, ettu
Changed: 2026-08-20T16:37:29.387310

Pip-Boy rest options in FO1 mode: base msg id is 320 (FO1 pipboy.msg 'Fo1 resting times' block: 320=ten min ... 333=party healed), NOT 321. Label for option N = base+N-1; a base of 321 shifted every label by one ('until noon' label rested until morning 6:00). Et Tu gl_0_settings.ssl calls rest_option_msgs(320) + set_rest_option(MORNING,6) when not fo1in2_0800_resting_enabled; old .int versions lack the call, so the engine default must be right. FO2 base = 302.

## [got-20260823002523-473fd9]
Category: gotcha
Tags: fallout2, ettu, rpu, sfall, metarule, save-corruption
Changed: 2026-08-23T00:25:23.666581

set_scr_name override poisoning (Issue B, 2026-08-23): the RPU/EtTu compat fork let mf_set_scr_name() write a GLOBAL script-file-name override (gScriptNameOverride) honored by scriptsGetFileName() BEFORE scripts.lst. One call of set_scr_name('Mike') from et-tu Mike.int made EVERY script (map script, doors, NPCs, party) load as Mike.int — log signature: programCreate flood of scripts\Mike.int + 'You see: Mike, the Old Town guard.' for every NPC (critter name = msg 101+scriptIndex, idx 820=Mike). Worse: the override was serialized into sfallgv.sav (metarule stream) so the poison survived save/load permanently (SLOT01 carried 'Mike'). Fix: scriptsGetFileName never applies the override; mf_set_scr_name is per-sid (sfallObjectNameSet) only; metarule load reads-and-discards the legacy slot (format kept). Test: tests/test_fixes_metarules.cc Issue B cases; 92/92 pass.

## [got-20260823014259-8e5eb4]
Category: gotcha
Tags: barter, et-tu, weight, pattern
Changed: 2026-08-23T01:43:07.726974

FO1 box traders (et tu) are intentionally overweight: get_barter_inven/move_obj_inven_to_obj force-loads full stock at talk start (itemMoveAll=force). ANY non-force itemMove/itemAttemptAdd targeting the trader fails rc=-6 (max weight) -- including returning the trader's own items. Rule: ALL barter moves must use itemMoveForce in BOTH directions (setup inventory->table AND undo table->inventory); a non-force itemMove in any barter path is a regression trap. Only the player-side weight check in barterAttemptTransaction is meaningful. Upstream CE uses itemMoveForce in both branches of barterMoveFromTable. Grep non-force itemMove inside barter* functions when reviewing barter fixes (prior fix e17707f2 fixed only barterAttemptTransaction and missed barterMoveFromTable -- completed in adbcc2f9, M-101).

## [dis-20260823161640-cc3da9]
Category: discovery
Tags: upstream, sync
Changed: 2026-08-23T16:16:40.586689

Upstream sync pass 3 (2026-08-23): 47 upstream commits since 1cce144 analyzed. Before cherry-picking ANY upstream commit into this fork, run 'git rev-list --cherry-pick --right-only main...upstream/main' (patch-id-aware) + grep the tree for marker symbols — several commits (FPS counter #689, mod_skill_points_per_level #695, perk-carryover #367) were ALREADY integrated in adapted form. Rejected: 6f05d3d8 sound-list dynamic (#577) — et tu ships a tagged SNDLIST.LST (mods/fo1_base/sound/sfx/SNDLIST.LST) and upstream's directory-scan-only change would reorder FO1 sound tags; 436d5554 encounter-intros removal — fork has a working implementation; 68479c44 perk carryover — fork's gSfallPerkOwed + sfall_gl_vars persistence replaces upstream's per-level vector + ce.sav sidecar (functionally equivalent, deliberately different architecture). Integrated 13 commits adapted (enum-based Map*/Rotation* upstream APIs adapted to fork's int-based worldmap).

## [got-20260823205234-bc713f]
Category: gotcha
Tags: elevation, caravan, combat, fo1ce
Changed: 2026-08-23T20:52:34.477690

Elevation-coherence invariant (FO1-CE contract, verified against alexbatalov/fallout1-ce): the dude may NEVER sit on a different elevation than the map/combat elevation. FO1-CE: override_map_start never touches map elevation; combat list is per-elevation (objectListCreate(-1,gElevation,CRITTER)); worldmap→encounter lands at enteringElevation=0. The et tu caravan self-attack bug = the FO1-in-2 caravan flow + a stray same-map/cross-map transition displacing the party to elev 1 while the encounter scripts commit elev 0. Engine-side repair committed bb7fa09b: _combat_enroll_csd_combatant (CSD att/def force-enrolled into the combat list); _combat_turn CSD attack gated on _gcsd->attacker==gDude; opOverrideMapStart now commits mapSetElevation + records the committed start; mapHandleTransition one-shot map-entry window (same-map suppression + settle, elevation-only); _combat re-asserts elevation BEFORE the list build.

## [got-20260823205240-eead0a]
Category: gotcha
Tags: window, lifecycle, combat, elevation, fix-design
Changed: 2026-08-23T20:52:40.447878

One-shot window vs long-lived record (design lesson from the caravan fix 3-cycle convergence): protections scoped to 'the map-entry moment' MUST be gated by a one-shot WINDOW flag (set at the trigger, consumed on the first consuming pass), NOT by a long-lived record cleared at some distant point (next map load). We shipped the long-lived version twice: (1) F-01 — _combat re-assert + settle restored the tile+elevation whenever the record was valid → a FO2/RPU player walking away then fighting would teleport back to spawn (tile over-reach); (2) N-01 — settle ran after the map==0 early-return so it never fired when no transition was pending. Correct final design: window flag for gating (suppression/settle/re-assert all peek/consume the same one-shot), long-lived record only for the stored values; elevation-only re-assert (tile ownership belongs to the player).

## [dis-20260823205246-796309]
Category: discovery
Tags: lineage, fo1ce, fallout1, reference
Changed: 2026-08-23T20:52:46.774433

Repo lineage + canonical reference (verified via web + source fetches): alexbatalov/fallout2-re (decompile) -> alexbatalov/fallout2-ce (ORIGINAL CE) -> fallout2-ce/fallout2-ce org = this repo's  remote; alexbatalov/fallout1-ce (NOT 'alexbatalov/fallout-ce' — that name does not exist) is the separate vanilla-FO1 CE. The attack-combat flaw class (ungated _combat_attack_this(_gcsd->defender)) is IDENTICAL in the original FO2-CE and FO1-CE — it is original-batalov code, not a fork regression. When fixing FO1-in-2 problems on this fork, consult FO1-CE first for the canonical FO1 semantics (op_attack intextra.cc:1466, combat_attack_this combat.cc:2269, override_map_start intextra.cc:273, elevation filter object.cc:2419/2431).

## [got-20260824035219-989e41]
Category: gotcha
Tags: game-time, gotcha, et-tu, engine
Changed: 2026-08-24T03:52:19.271975

Game-time units (FO2 engines): ONE_GAME_SECOND=10 ticks, ONE_GAME_MINUTE=600, ONE_GAME_HOUR=36000, ONE_GAME_DAY=864000 (define.h:801-804, scripts.h:22 GAME_TIME_TICKS_PER_DAY). game_time_advance(x) passes RAW TICKS to gameTimeAddTicks (interpreter_extra.cc:3074-3088) — NOT minutes/hours. GAME_TIME_IN_HOURS = game_time/36000. Trap: reading 60*(60*10) as '10 hours' by assuming 60 ticks/minute — it is exactly 1 game hour. Always check define.h ONE_GAME_* + scripts.h before asserting game-time magnitudes.

## [got-20260824035221-bf0117]
Category: gotcha
Tags: sfall, rest-mode, metarules, gotcha, repeat-regression
Changed: 2026-08-24T03:52:21.577821

sfall set_rest_mode contract (real sfall.h: RESTMODE_DISABLED=1, STRICT=2, NO_HEALING=4; NO UNTIL_HEALED constant exists). set_rest_mode(0) = RESET (→ engine default -1), NOT disabled. Engine translator sfall_metarules.cc translateSfallRestMode: map 0→default(-1), 1→disabled(0), 2→strict(1), 4→noHealing(2), combos(1|2,2|4,1|4,1|2|4)→disabled(0), unknown→default. Prior 'fix' 7f583564 (UF-H-035) authored the WRONG table (invented UNTIL_HEALED=4; 0→Disabled) causing global rest ban via 99map/LARIPPER.ssl set_rest_mode(0) (enter + map_exit) — repeat regression, third attempt corrected it. CE persists rest mode in saves; sfall resets on reload (documented divergence).

## [got-20260824035226-28de87]
Category: gotcha
Tags: healing, fo1, rest, worldmap, gotcha
Changed: 2026-08-24T03:52:26.127700

FO1-CE rest/travel heal is GRADUAL, not instant: partyMemberRestingHeal=(hours/3)×healingRate (tmp/fo1-ce/src/game/party.cc:438-459); pipboy rest _Check4Health fires after 180 rest-min; worldmap heals per game-day of travel. The fork's gFallout1Behavior instant-full-heal block (party_member.cc:878-895, from f40c9611 '50 verified fixes') deviated from real FO1 — DELETED. Post-fix worldmap heal math: loop fires every >1000 REAL ms while worldmap open (worldmap.cc:4041, not walking-gated, fires on open since partyHealTime=0) with healHours=3 → per-game-hour rate = R/30 HP (game time advances 0.5h/frame at ~60fps). Any new FO1-related gFallout1Behavior consumer must be checked against FO1-CE behavior, not assumptions.

## [got-20260824222708-331c08]
Category: gotcha
Tags: fid, armor, party, inventory, sfall, ce
Changed: 2026-08-24T22:27:08.066519

Non-dude critter armor FID: vanilla/FO1-CE contract = ONLY gDude re-derives armor frames (armorGetMaleFid as critter art). NPC armor change = stats only; NPC armor appearance is script-owned (et tu gl_partyarmor via HOOK_INVENWIELD + metarule3 ART_SET_BASE_FID_NUM=107 art_change_fid_num). NEVER buildFid(CRITTER, armor.maleFid, ...) for non-dudes - maleFid is an art index in critter-art space = unrelated figure (white man). IS-01 9fc66a87 violated this (white-man regression, fixed 2026-08-24). Real stale-sprite root: re-wear paths (critterRestoreEquipped, barter close) never re-fired HOOK_INVENWIELD (wield) after strip fired unwield - script art lost until reload. Hook re-fire (isWield=1) on re-wear = fire-and-continue (restore of pre-vetted gear; -1=engine default=proceed). Check FO1-CE inventry.cc:2839-2844 before touching.

## [got-20260824232228-6e9012]
Category: gotcha
Tags: fid, art, sfall, metarule, party, mod
Changed: 2026-08-24T23:22:28.617708

art_change_fid_num (metarule3 METARULE3_ART_SET_BASE_FID_NUM=107; sfall define_lite macro) = SET FULL FID VERBATIM (semantics of reg_anim_change_fid/register_object_change_fid). CE must NOT rebuild bits from the object's current anim/weapon/rotation: stale weapon bits (e.g. 6) glued onto a new art make artGetFrame file lookup fail (HMMET2IA.FRM missing) -> critter silently invisible/'disappeared'. In CE's interpreter_extra.cc metarule: after setting, reconcile critter weapon bits from critterGetItem2 via weaponGetAnimationCode when artExists(withWeapon) — otherwise reuse bits. Note: art columns are per-art pack: HMMET2 (Default) has NO weapon-6 ('I') column, HMBLTR (Leather) does; et-tu 14mm pid 0x16=22 = anim code 6 = SMG column by author convention; Vasques WeaponAnims=5 (pistol only) by design -> mod vetoes his stock 14mm (owner decision: keep as mod ground truth).

## [got-20260901201916-6d2a58]
Category: gotcha
Tags: github, release
Changed: 2026-09-01T20:19:16.035163

GitHub asset upload quirks: on itohnobue account the uploads host sanitizes SPACES to DOTS in release asset names (rename via PATCH keeps dots; hyphenated names avoid it). Asset uploads must go to uploads.github.com with a browser User-Agent (api.github.com host returns 404).

## [got-20260901201916-f3bcb8]
Category: gotcha
Tags: macos, signing, release
Changed: 2026-09-01T20:19:16.117284

macOS release pipeline: (1) Signing -- a Developer ID Application .cer from the portal has NO private key (usable for nothing alone); pair it with the CSR keypair used at creation (openssl pkcs12 -export then security import -T /usr/bin/codesign). Re-sign the ENTIRE bundle including nested executables with Developer ID before notarization. notarytool submit --wait --apple-id --team-id --password works from CLI; staple after. (2) Packaging -- CPack's 'cmake --build --target package' (DragNDrop) with the Xcode generator RE-SIGNS the .app ad-hoc ('Sign to Run Locally'), destroying the prior codesign signature; use manual 'hdiutil create -srcfolder <staging_with_signed_app> -format UDZO' instead (cp -R preserves the signed bundle). (3) ditto realpath errors were transient/flaky in this env (retry works).

## [pat-20261025053032-17e4ce]
Category: pattern
Tags: git, merge, upstream, sync, ledger, port
Changed: 2026-10-25T05:30:32.251805

Selective upstream sync via an ancestry-only merge: `git merge -s ours <upstream-sha>` makes upstream an ancestor and leaves the fork tree byte-identical; content is then adopted in one reviewable commit per port wave, with every upstream commit carrying exactly one recorded disposition (ADOPT / SKIP / ALREADY-PORTED / CONFLICT-DISCARDED, plus ANCESTRY-ONLY for merge commits). Use it when the range is dominated by an architectural refactor the fork deliberately does not follow (the typed FrmId/TypedFrmId/ProtoId series): a textual merge yields a tree that neither compiles nor keeps fork semantics, and buries the real fixes inside thousands of rename hunks. PERMANENT trade-off, accept it knowingly: after the merge the next `git merge upstream/main` uses the merged upstream sha as its base, so upstream changes this sync deliberately discarded never reappear as pending deltas. The committed disposition ledger (docs/upstream-sync-2026-10.md + tmp/ledger/ledger.tsv) is then the ONLY memory of what was discarded and why, and a future sync must revisit a decision by reading its entry rather than re-deriving it from a merge conflict. Warrant: merge 88ddbbdf (parents e3e2433b fork + f6254ed5 upstream) over a 317-commit range, 317-row ledger (286 content + 31 ancestry merges); docs/upstream-sync-2026-10.md:25-44. Prevention: not mechanically preventable - accept recurrence and budget for it in every sync; the committed ledger is the enforcement, so its commit is mandatory.

## [pat-20261025053032-1c8594]
Category: pattern
Tags: port, merge, atomic, contract, producer-consumer
Changed: 2026-10-25T05:30:32.334643

The dominant failure mode of a selective port is taking HALF of a producer/consumer pair, not taking a wrong commit. Two-sided contracts must be adopted in ONE commit or not at all: a hook's arg0 contract plus its fire sites; a clamp plus the unit its consumer reads; a fix plus the upstream commit that removes its pre-image. Two mechanical checks before porting a hunk pair: (a) `git apply --check` every hunk of the pair against the fork (a fix whose pre-image never existed here cannot be ported as authored - it must be re-expressed); (b) read the upstream commit IMMEDIATELY AFTER the one being ported - a following commit that deletes what the first adds means the pair is (fix, revert) and neither half is portable alone (1cb875b8 adds a `dataSize > payloadSize` rejection that the very next upstream commit removes; 1,614 loose .FR0-.FR5 files plus 13,602 DAT split entries trip it in the two mods the Et Tu stand ships). 13 of this sync's 50 confirmed findings were port-order/partial-adoption defects. Warrant: src/animation.cc `_check_move` reserves AP while `sad->length` is consumed as a hex count (:2496/:2504-2506); src/combat_ai.cc:1417 `dist <= a3` against the producer post-image `dist - radius - 1` at src/combat.cc:2517. Prevention: mechanically preventable - a wave-gate post-condition per adopted hunk (grep the post-port diff for the pair's distinguishing token, or `git apply --check` both halves) fails the wave when a member is missing.

## [got-20261025053032-c64af5]
Category: gotcha
Tags: port, merge, guard, invariant, prior-fix
Changed: 2026-10-25T05:30:32.417283

A faithful upstream rewrite of a body the fork has hardened silently deletes the fork's guard or loop invariant, because transcription replaces the whole body. Before applying an upstream hunk: diff the fork's same body at the merge base and list every fork-only line; each fork-only line must be RE-EXPRESSED into the new shape, or the hunk is CONFLICT-DISCARDED - never transcribed. Six of this sync's 50 confirmed findings were this class, and every one was a fork hardening or loop invariant disappearing inside an upstream rewrite of the same function: the fork's three `art N-1` length clamps absent from all 38 upstream revisions of art.cc; upstream's `#850` per-turn AP grant landing ABOVE the fork's rewind and its three AP gates (src/combat.cc:3311, src/combat_ai.cc:2198, :3560) so a spent critter gets a fresh pool; `#800` deleting the fork's fade machinery still referenced at src/worldmap.cc:4423. Prevention: review-only - the pre-port step `git diff <merge-base>..HEAD -- <file>` per hunk, plus a wave-gate grep for each fork-only token in the post-port body.

## [pat-20261025053032-c18aff]
Category: pattern
Tags: ledger, port, verification, counts, artifact
Changed: 2026-10-25T05:30:32.499286

A port ledger, plan, or committed sync record is a set of factual CLAIMS, not notes: every row's stated ground must re-derive from the tree or the git object, and every number must ship with the command that produces it plus the set it measures. Two measured warrants: (a) re-adjudicating the 19 most-scrutinised ledger rows found 9 (47%) carrying a reason or status their own evidence did not support, including false ALREADY-PORTED claims resting on surface presence of a symbol (a SKIP row whose reason said value was taken; a 39-row 'the FrmId family is absent' boilerplate contradicted by src/art.h:85-107); (b) a published assertion count that no available metric reproduces (four variants bracket it - 10,441 / 10,460 / 11,277 / 11,359 - while every one agrees on the +63 delta) cannot serve as the next sync's baseline. Label the measured set too: '17 of 19 adopted shas' was a correct number with the wrong label, since those 19 were the DISPUTED set and 16 of them were never adopted. Structural remedy that fits the failure: one mechanical reason-evidence challenger over every row before the ledger is committed - re-derive each stated ground and fail the row when it does not reproduce. Prevention: mechanically preventable - script the challenger as a pre-commit gate over the ledger.

## [got-20261025053032-b32867]
Category: gotcha
Tags: verification, grep, false-positive, inventory, port
Changed: 2026-10-25T05:30:32.582781

grep is evidence only with the right token and the right shape - three traps, each of which produced a wrong verdict: (1) MASK-CLEARS: a flag cleared as `flags &= ~OBJECT_EQUIPPED` (src/item.cc:512, from itemRemoveInternal) does not match a grep for a write of the flag, so a claimed duplicate side effect at a delegation boundary is a false positive until the callee's precondition flag state is read at the call site (the inventory window strips the worn armour and clears the bit before the drop block runs, so AC/DR/DT are subtracted once). (2) BARE-TOKEN INFLATION: `grep -c 'case '` on src/sfall_metarules.cc gives 228 because it counts comments and strings, against 134 real kMetarules[] rows (:1111) - anchor the pattern to code shape (`^[[:space:]]*\{[[:space:]]*"[a-z0-9_]+",[[:space:]]*mf_`). (3) TYPE-EXISTS is not MEMBER-EXISTS: `grep ProtoId -> 0` does not prove the fork has no frame-id type when a minimal FrmId wrapper exists (src/art.h:85-107), and 39 ledger rows inherited a false 'family absent' claim from that shape of check. Proving absence requires the exact symbol AND the exact member, searched across every spelling (mask clear, helper, wrapper). Prevention: review-only - no automated check decides intent; mitigation is anchored patterns plus reading the callee at every claimed boundary.

## [got-20261025053032-d363af]
Category: gotcha
Tags: loadsave, save, scan, state-machine, port
Changed: 2026-10-25T05:30:32.668062

Narrowing a whole-collection scan to a lazy per-page scan moves a freshness guarantee: consumers that read a per-item status now read a value scanned only for the DISPLAYED page. The pre-port `_GetSlotList` swept every slot, so the fork's auto-quicksave ring moving `_slot_cursor` off the displayed page (src/loadsave.cc:588-596) was harmless; after the per-page scan, `_LSstatus[_slot_cursor]` was stale at the overwrite gate (:878-892), so the confirmation was skipped and the save landed in an undisplayed ring slot. Rule: when you change a scan's granularity, enumerate every writer of the selector - not only the UI's own paging - and have the scan refresh the selector's slice as well as the displayed slice (`loadSaveLoadSlotPage(_slot_cursor / slotsPerPage)`, :2862). The page loader no-ops on an already-scanned page (:2873-2875), so the cost is one probe and the invariant becomes 'the cursor's slot is scanned' for every present and future writer. Fixing only the known writer closes today's instance and leaves the class open. Prevention: mechanically preventable in part - pin the arithmetic and grep-assert the cursor-page call is present (no test target links loadsave.cc, so a mirror pin cannot fail).

## [got-20261025053032-2803c1]
Category: gotcha
Tags: port, shipped-data, measurement, art, regression
Changed: 2026-10-25T05:30:32.750884

An adopted upstream fix can be a numerical no-op in every shipped configuration, and a class that pre-dates the port is widened, not introduced - measure the shipped asset and the merge base before recording a non-delivery as a defect or a delta as a regression. The endgame pan rewrite derives panDistance/fadeDistance/frameDelay from the art's width; for the shipped `art\intrface\DP.FRM` (1280x480) the old fork formula and upstream's rewrite compute identical values, so the genuine non-delivery cost the user nothing and the obligation was a ledger line plus a stale test rewrite, not code. Likewise a use-after-free window around `critterKill` that is already reachable through the pre-port HOOK_ONDEATH path, and only through third-party mod code, is a widened pre-existing class, not a port regression. Rule: compute both formulas at the shipped asset's actual dimensions, and diff the class against the merge base, before rating impact. Prevention: review-only - a measurement step before filing, no mechanical guard.

## [pat-20261025053032-99e337]
Category: pattern
Tags: test, pin, port, verification
Changed: 2026-10-25T05:30:32.835124

A regression pin can only fail on a production regression if the production translation unit is IN the harness's compiled set. This suite compiles 16 engine TUs (`grep -oE 'src/[a-z_0-9]+\.cc' tests/CMakeLists.txt | sort -u`), no test includes a `.cc` (0 matches), and no target links the game, so a pin for any TU outside that set is a self-contained mirror: it is green in both the correct and the incorrect production state. 7 of 8 promised port pins were structurally mirror-bound, and a mirror pin of production page arithmetic passed in both states. Rule: for each adopted behaviour change, decide linkability from tests/CMakeLists.txt FIRST; when the TU does not link, the only discriminating guard is a wave-gate grep for the hunk's distinguishing token (fails the wave when the token is absent from the post-port diff), stated as a manual gate check and never as a test. Keep the accounting honest: a compile-only OBJECT library buys compile visibility, not a pin - it executes nothing. Prevention: mechanically preventable - a gate script derives linkability per adopted commit and requires either a production-linked pin or a named gate grep.

## [got-20261025053032-603044]
Category: gotcha
Tags: build, ctest, gate, verification, allowlist
Changed: 2026-10-25T05:31:06.001806

`BUILD SUCCEEDED` does not prove the build covered the tests you are about to trust. Two ways it lies in an Xcode multi-config tree: (a) INCREMENTAL NO-OP - exit 0 with 0 `CompileC` (only CodeSign actions) proves the tree agrees with its dependency files, not that the checked-in sources compile, so it is not acceptance evidence; (b) STALE PLAN - the build plan can be computed before ZERO_CHECK regenerates the project, so a newly added test target is never built while the 92 existing ones relink, and `ctest` then passes on the stale binary set with the new target absent. Gate rule: after every build, count test binaries on disk and compare against `grep -c 'add_test(' tests/CMakeLists.txt`, and confirm the new target's name appears in the build log; re-invoke the build after any configure; use `--clean-first` when the build is itself the evidence. The same family, one level up: A CHECK THAT CANNOT FAIL - an allow-list gate whose allowed set is derived from the artifact it audits (`comm -23` against a list that itself contains the forbidden path) suppresses the violation by construction, and a remedy step that never returns a failure status reports success for every input; derive the allowed set from the authority, and make the failure path exit non-zero. And a baseline verifier that asserts tree equality with the merge base exits non-zero forever after the first post-baseline commit - read its individual lines, never its exit code alone. Prevention: mechanically preventable - the gate script asserts target-count == add_test-count, greps the log for every newly registered target, and its own failure path is exercised once against a known-bad input.

## [got-20261025053105-b0f97e]
Category: gotcha
Tags: port, contract, semantics, prior-fix, gotcha
Changed: 2026-10-25T05:31:05.914111

Same name, different semantics between fork and upstream: a function or field with an identical name can carry a different CONTRACT, so an upstream 'fix' that repairs upstream's contract can narrow or break the fork's deliberate one. Six of this sync's 50 confirmed findings were this class and every one was a fork-authored body: `sneak_success` re-rolls `randomBetween(1,100) <= skillGetValue` (src/sfall_opcodes.cc:6013-6021) instead of reporting the engine's persisted `_sneak_working`; `get_sfall_global_float` stores IEEE bits upstream but a parallel float map here (a test-pinned fork fix, tests/test_global_vars.cc:2002-2050); `rest_heal_time` is hours-per-event here vs interval-in-minutes upstream; `Rotation` is 0..5 while the FID field is 1..6, so upstream's `rotationIsValid` guard maps a legitimate NW critter to `kEmptyFid` at 57 `buildFid` call sites. Rule: before porting a fix to a body the fork owns, `git blame` the site and read the tests that pin it - a fork-authored body with a pin is deliberate until its own contract proves otherwise, and the right disposition is SKIP with the divergence named, not a spec alignment. Prevention: review-only - the pre-port blame/pin read is the check.

