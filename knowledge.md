# Knowledge Base
Last updated: 2026-08-24T03:52:26.130422

## [dis-20260704144725-9b3649]
Category: discovery
Tags: f1et, save-format, config, proto
Changed: 2026-07-04T14:47:25.848405

Config & Save audit: save format uses fixed 131B header + 27 sequential handler chunks, no version enforcement (only signature check). Save files do NOT serialize proto structs — only runtime object state (hp, ammo, etc.), so FO2 proto size differences don't affect save compatibility. Config system is fully extensible; existing f1_res.ini pattern can be replicated for fo1_settings.ini and ddraw.ini. Critical issues: LOAD_SAVE_HANDLER_COUNT=27 is a hardcoded positional chain, adding handlers breaks old-load-new or new-load-old. Version check is decorative — SLOT_STATE_UNSUPPORTED_VERSION is unreachable. See tmp/s2i1-d1b-config-report.md for full analysis.

## [con-20260704193654-b49a87]
Category: context
Tags: fallout2-ce, plan, completed
Changed: 2026-07-04T19:36:54.836646

Fallout 2 CE fork plan completed: tmp/fallout2-ce-fork-plan.md (1,856 lines). Base fork = fallout2-ce/fallout2-ce org fork. Et Tu + RPU compatible. 3 convergence iterations, 71 findings, 67 fixed. 6 unique review specialists used (code-reviewer, cpp-pro, backend-architect, build-engineer, research-analyst, security-reviewer).

## [got-20260704221900-f633b8]
Category: gotcha
Tags: fallout2-ce, interpreter, bugs
Changed: 2026-07-04T22:19:00.645953

fallout2-ce interpreter.cc is the bug hotspot — 12 confirmed MEDIUM+ findings in 3,461 LOC. Key patterns: missing bounds checks on bytecode/procedure indices, programFatalError called without setjmp context (UB), opModulo/opSubtract/opMultiply/opDivide all share silent stack imbalance for non-numeric types.

## [pat-20260704221900-debed7]
Category: pattern
Tags: fallout2-ce, hooks, pattern
Changed: 2026-07-04T22:19:00.720561

fallout2-ce hook registration pattern: define enum in sfall_script_hooks.h, implement register+fire functions in sfall_script_hooks.cc, call from engine code (object.cc/light.cc/interface.cc). ScriptHookCall constructor takes (hookId, maxReturnValues, {args}). maxReturnValues=0 = observe-only, >0 = script can override via ret values.

## [dis-20260704221900-a1b096]
Category: discovery
Tags: fallout2-ce, opcodes, architecture
Changed: 2026-07-04T22:19:00.795634

fallout2-ce opcode dispatch architecture: map-based registration with opcode & 0x3FFF mask (14-bit, 16384 slots), dispatch reads via opcode & 0x3FF (10-bit, 1024 slots) — this inconsistency was fixed as F-05. Sfall extended opcodes registered in sfall_opcodes.cc, standard opcodes in interpreter.cc/interpreter_extra.cc/interpreter_lib.cc.

## [ent-20260704235834-2f23d4]
Category: entity
Tags: fallout2-ce, repository, forgejo
Changed: 2026-07-04T23:58:34.318315

fallout2-ce-extended repo: new Forgejo repo at git.aoizora.ru/nobu/fallout2-ce-extended (public). Local: ~/Git/fallout2-ce-extended. Remote: ssh://git@git.aoizora.ru:2222/nobu/fallout2-ce-extended.git. Contains all Session 1+2 work (sfall 4.4.9, 10 hooks, 32+ bug fixes, SpeedMulti, test framework, Emscripten preset). Future work goes here, not the original fallout2-ce repo.

## [dis-20260705011122-e81b5f]
Category: discovery
Tags: sfall, compatibility, ception, corrected
Changed: 2026-07-05T01:11:22.081688

CRITICAL TABLE CORRECTION: get/set/reset_critical_table (0x81E1-0x81E3) ARE fully implemented in CE at sfall_opcodes.cc:459-501,2090-2095. test_criticals.cc passes. Research reports incorrectly flagged as NOT IMPLEMENTED. Et Tu's OverrideCriticalTable=3 IS supported.

## [got-20260705011127-cf46c9]
Category: gotcha
Tags: sfall, hooks, compatibility, bug
Changed: 2026-07-05T01:11:27.626093

register_hook_proc vs register_hook_proc_spec: CE maps BOTH to same handler at sfall_opcodes.cc:2341, both add to END via scriptHooksRegister at sfall_script_hooks.cc:146 (emplace at begin, reverse iterate = end). sfall 4.2+: register_hook_proc adds to BEGINNING, register_hook_proc_spec to END. CE behavior matches sfall 'BackwardHooksRegistration' mode. Mods depending on execution order break silently.

## [pat-20260705011129-fd1230]
Category: pattern
Tags: sfall, hooks, bug, keypress
Changed: 2026-07-05T01:11:29.136175

HOOK_KEYPRESS at sfall_kb_helpers.cc:371 passes hardcoded 0 for arg3 (VK code). sfall uses this for distinguishing left/right shift. TODO comment says 'not sure any mod actually used it' but RPU Party Orders depends on keyboard shortcut distinction.

## [dis-20260705064730-b61619]
Category: discovery
Tags: sfall_opcodes, rpu, completed
Changed: 2026-07-05T06:47:30.595064

F-01 VFS: Full implementation of 18 file ops (fs_create/copy/find/read/write/seek/size/pos/delete/resize) using FILE* handle table in sfall_opcodes.cc. F-02 NPC/Hero: 5 stubs (inc/get_npc_level, set_dm/df_model, hero_select_win). F-08 available_global_script_types returns 0xF bitmask. F-14 get_sfall_global_float int-backed cast. F-11 reg_anim_callback stores callback (not invoked without sfall_animation.cc changes). F-15 register_hook_proc_spec separate handler. F-16 ITEM_TYPE_AMMO guard on set_weapon_ammo_pid. F-34 version bumped to 4.5.1. F-68 negative offset guard in proto_data ops. F-71 fallback push(0) in op_get_array else branch. F-79 critter type checks on 4 stat handlers. F-29 all Object* sites verified consistent.

## [arc-20260705075444-fd6f6a]
Category: architecture
Tags: fallout2-ce, sfall, production-check, rpu, ettu
Changed: 2026-07-05T07:54:44.366085

fallout2-ce-extended production check: 78 original findings + 19 review findings fixed across 27 files. VFS (fs_create/fs_copy w+b mode), interpreter bounds checks, script dialog capacity, global script early-return paths, INI snprintf guards, config fprintf checks, hook DescriptionObj string resolution, inventory explosive check. Build: macOS ARM Debug passes, 5/5 tests pass.

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

to sfallOpcodesReset() for any dynamically allocated state.

## [dis-20260705221054-6d4639]
Category: discovery
Tags: fallout2-ce, rpu, et-tu, sfall
Changed: 2026-07-05T22:10:54.268090

fallout2-ce-extended: RPU v34 has 1518 .int files, et tu v1.16.3771 has 1049 .int files. Engine has ~90%+ sfall opcode/metarule coverage. Two MUST_FIX opcodes were set_hero_style (0x8215) and set_hero_race (0x8214) — implemented via sfall_gl_vars_store. Three parity metarules (set_town_title, set_car_intface_art, set_rest_mode) implemented as store-only stubs. Engine reports sfall version 4.5.1.

## [got-20260705221100-205ec6]
Category: gotcha
Tags: fallout2-ce, stat, sfall
Changed: 2026-07-05T22:11:00.176451

fallout2-ce-extended: gStatDescriptions[] in stat.cc is file-static — no getter functions (statGetMaxValue/statGetMinValue) exist. Only setters (statSetMaxValue/statSetMinValue) are in stat.h. To return correct stat limits from sfall metarules, a kDefaultStatLimits lookup table mirroring the initializer values is used as a workaround.

## [dis-20260706093444-0cdecc]
Category: discovery
Tags: tests, coverage, quality, sfall, ce-extended
Changed: 2026-07-06T09:34:44.371430

E-Test Suite Quality Audit completed: 45 test files (~42K LOC) analyzed. 2 CRITICAL (7 TODO placeholders with CHECK(true), knockback opcodes untested), 5 HIGH (mirror test fallacy, VFS fs_resize mode mismatch, 150 file-static opcodes untestable, worldmap subtile skip, broken code test), 9 MEDIUM, 4 LOW findings. Key gaps: skill opcodes 0% coverage, combat advanced 0%, lock/event/utility opcodes 0%. 38/45 tests are self-contained mirrors that don't test production code.

## [dis-20260706142125-67fd68]
Category: discovery
Tags: fallout2-ce, rpu, etu, compatibility
Changed: 2026-07-06T14:21:25.016809

fallout2-ce-extended: RPU is 100% compatible (zero gaps). Et Tu requires gFallout1Behavior flag for FO1-mode behavior gating across combat, encounters, rest, reactions, worldmap. 28 fixes implemented across 17 files. Build: cmake -S . -B out/build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64. Tests: 43/43 pass.

## [pat-20260706142125-99c5dc]
Category: pattern
Tags: fallout2-ce, voodoo, sfall, architecture
Changed: 2026-07-06T14:21:25.090015

fallout2-ce-extended: VOODOO memory patches from sfall can't use raw memory writes in CE (different address space). Must implement as native CE config-driven behavior using gFallout1Behavior flag.

## [got-20260706142125-5f007b]
Category: gotcha
Tags: fallout2-ce, sfall, timer, sid
Changed: 2026-07-06T14:21:25.160790

fallout2-ce-extended: Program struct has no 'sid' member. To get SID from Program*, must traverse global scripts list. scriptAddTimerEvent stores param as data arg, not opcode. PendingTimerEvent needs timerId field for per-event removal.

## [dis-20260706153805-923a05]
Category: discovery
Tags: fallout2, sfall, rpu, etu, compat, research
Changed: 2026-07-06T15:38:05.760661

fallout2-ce-extended RPU/etu compat: research identified ~50 gaps. CRITICAL: (1) RPU gl_k_modini.ssl BLOCKS on WorldMapSlots=21 + BoostScriptDialogLimit=1 INI validation — CE status unknown, must return expected values; (2) gEnableHeroAppearanceMod/gExtraSaveSlots/gAllowUnsafeScripting/gUseFileSystemOverride are PARSED but UNWIRED in sfall_config.h (et tu needs them); (3) HOOK_COMBATTURN claimed ✅ but must verify it fires (et tu autodoors + destroy_armor depend on it); (4) add_trait engine display integration partial; (5) ~18 storage-only sfall opcodes (knockback, drugs, spray, skill mods, pickpocket mods, hp_per_level, pipboy_available, car_intface_art) values stored but engine never reads them; (6) FO1-specific config (StartingMap, StartXPos/YPos, WorldMapDelay2, XPTable, Movie1-17, DamageFormula, SkillsFile, PerksFile) not parsed by CE; (7) SFALL_COMPATIBILITY.md has 8 outdated entries (set_perk_level done, unjam_lock done, add_g_timer_event/remove_timer_event/create_spatial done, set_quest_failure_value done). RPU uses hooks 5/8/18/30 (all ✅). et tu uses 22 hooks (mostly ✅, 1 needs verify). Full reports: tmp/s1-research-{rpu,etu,sfall}-report.md

## [ref-20260706183422-02b473]
Category: reference
Tags: et-tu, fo1in2, modding
Changed: 2026-07-06T18:34:22.428517

Et Tu mod GitHub: github.com/rotators/Fo1in2. Uses sfall 4.5 with Rotators fork extensions. Key files: ddraw.ini (947 lines), ddraw.fo1in2.ini, config/fo1_settings.ini. Global scripts: gl_fo1mechanics.ssl (11 hooks), gl_worldmap.ssl, gl_classic_wm.ssl, gl_partyarmor.ssl, gl_autodoors.ssl, gl_car.ssl, gl_destroy_armor.ssl

## [got-20260706200521-0f6794]
Category: gotcha
Tags: sfall, dialog, compatibility, crash
Changed: 2026-07-06T20:05:21.011332

Domain F4: opSayMessage (0x8054) causes fatal error — dialogMessage() always returns -1, triggering programFatalError at interpreter_lib.cc:1047. Vanilla dialog opcodes (say_message, say_end, say_option) are landmines on CE. RPU/Et Tu use sfall metarules instead but any mod script calling vanilla opcodes will crash.

## [got-20260706200522-f03187]
Category: gotcha
Tags: sfall, interface, save-load, stale-handle
Changed: 2026-07-06T20:05:22.887952

Domain F4: Interface overlay (gInterfaceOverlayState) saves/restores windowHandle across save/load. Windows are transient (destroyed on map exit). After load, active=true with stale handle — phantom overlay that can't render. Affects Et Tu fo1_interface mod.

## [got-20260706200525-e95408]
Category: gotcha
Tags: sfall, message-box, save-load, scripts-disable
Changed: 2026-07-06T20:05:25.215162

Domain F4: message_box mf_message_box saves dialogShowCount > 0 if saved while dialog is showing. On load, counter is non-zero but scripts are (or will be) enabled. Next message_box call increments to 2, decrements to 1, never reaches 0, so scriptsEnable() never called — scripts permanently disabled.

## [got-20260706235949-33ba1a]
Category: gotcha
Tags: fallout2-ce, macos, codesign, dmg, cmake
Changed: 2026-07-06T23:59:49.288031

fallout2-ce-extended macOS DMG signing: CPack 'cmake --build --target package' (DragNDrop generator) with Xcode generator RE-SIGNS the .app ad-hoc ('Sign to Run Locally') during packaging, DESTROYING any prior codesign signature. Fix: use manual 'hdiutil create -srcfolder <staging_with_signed_app> -format UDZO' instead — cp -R preserves the signed bundle, hdiutil packages it without re-signing. Discovered at TEST stage; CPack approach looked sufficient in research but fails in practice with Xcode generator.

## [got-20260707035756-0ff8ef]
Category: gotcha
Tags: buffer-overflow, COMPAT_MAX_PATH, fallout2
Changed: 2026-07-07T03:57:56.799778

Fallout2 CE: compat_makepath() pointer-arithmetic bugs at platform_compat.cc:168,189,196,206 — path++ can escape COMPAT_MAX_PATH buffer boundary, strchr() reads OOB. remaining calc is defeated.

## [arc-20260707035758-6c7f04]
Category: architecture
Tags: fallout2, path-handling, hardening
Changed: 2026-07-07T03:57:58.159931

Fallout2 CE: 115+ COMPAT_MAX_PATH (260-byte) stack buffers across ~50 src/ files. compat_makepath/splitpath are the path assembly API. 3 compat_makepath calls in xlistEnumerate() are the crash trigger. compiler hardening: NO -fstack-protector, NO _FORTIFY_SOURCE in release.

## [dis-20260707035800-0bb9e0]
Category: discovery
Tags: fallout2, security-audit, CWE-121
Changed: 2026-07-07T03:58:00.117822

Fallout2 CE security audit: 1 CRITICAL (F-01: compat_makepath stack overflow, CWE-121), 3 HIGH (F-02: path past buffer via path++, F-03: unbounded strcpy xfile.cc:761, F-04: unbounded strcpy dfile path xfile.cc:655), 3 MEDIUM, 2 LOW. Fix ranked: Phase1=fixed compat_makepath, Phase2=compiler flags, Phase3=snprintf, Phase4=std::filesystem

## [got-20260707090024-0928cf]
Category: gotcha
Tags: et-tu, fo1, voodoo, sfall
Changed: 2026-07-07T09:00:24.311563

VOODOO memory patches (write_byte/write_int at 0x4244ED,0x4A29F5,0x499746,0x444D10,0x4C0B75) are unconditional no-ops in sfall_opcodes.cc:3038-3063. AllowUnsafeScripting is intentionally unwired. EtTu Fallout1Behavior=1 silently produces NO FO1 behavior changes. CE MUST provide equivalent high-level APIs for reaction thresholds (25/-25), rest string offset (320), hit-chance nerf revert, Dogmeat PID, encounter dialog disable.

## [got-20260707134231-c96f7a]
Category: gotcha
Tags: fallout2, sfall, opcodes
Changed: 2026-07-07T13:42:31.530717

fallout2-ce-extended: F-13 set_critter_skill_mod uses 3 args (critter, skill, mod), set_base_skill_mod uses 2 args (skill, mod). Per-skill maps gBaseSkillModMap and gGlobalCritterSkillModMap must be separate to avoid double-application in skillGetValue.

## [got-20260707134231-b1d629]
Category: gotcha
Tags: fallout2, sfall, hooks
Changed: 2026-07-07T13:42:31.608261

fallout2-ce-extended: HOOK_KEYPRESS arg0 must be DIK keyCode (not pressed state). ret0=1 means consume/block the key. HOOK_MOUSECLICK needs x,y as arg1,arg2 from SDL event.

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

## [got-20260707215518-1ce35e]
Category: gotcha
Tags: fallout2-ce, sfall, perk, compilation
Changed: 2026-07-07T21:55:18.911594

fallout2-ce-extended perk.cc: gPerkDescriptions is file-static (internal linkage). Cannot access from sfall_opcodes.cc — use perkSetMinLevel/perkGetMinLevel accessors from perk.h instead.

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
Changed: 2026-07-08T08:27:20.866112

fallout2-ce-extended test infrastructure: test targets that #include production headers need target_include_directories(test_NAME PRIVATE src) in tests/CMakeLists.txt. TEST_ACCESSORS_ENABLED compile definition must be set on test_sources target for sfall_ini/sfall_opcodes test accessors.

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
Tags: sfall, persistence, strings, save-format
Changed: 2026-07-09T05:58:08.060541

sfall_global_vars save format: version 2 adds string section (length-prefixed records) after float section. Backward compatible — old code ignores trailing data, new code checks version>=2. Keys follow 8-char convention for direct uint64_t packing. sfall_gl_vars_fetch_string() returns new[]-allocated copy — caller must delete[].

## [got-20260709063852-1f4205]
Category: gotcha
Tags: fallout2-ce, sfall, save-format, backward-compat
Changed: 2026-07-09T06:38:52.363154

fallout2-ce-extended: sfall_global_vars save format version 2 adds stringVars section after float section. Version 1 saves load fine (version >= 2 check skips strings). Header size changed from 16 to 20 bytes (added stringCount field). Test in test_global_vars.cc updated.

## [pat-20260709063857-4940ab]
Category: pattern
Tags: fallout2-ce, sfall, save-format, metarules
Changed: 2026-07-09T06:38:57.677207

fallout2-ce-extended: METARULES_SAVE_VERSION bumped 6→7 for gTalkingHeadMood persistence. Version-gated load: if (version >= 7) read gTalkingHeadMood. Old saves (v6) default to -1 from reset().

## [pat-20260709093313-70c3f0]
Category: pattern
Tags: fallout2-ce, test-infrastructure
Changed: 2026-07-09T09:33:13.105802

fallout2-ce-extended test infrastructure: doctest header-only, self-contained mirror pattern for files with 40+ engine deps, test_sources library links config.cc/sfall_global_vars.cc/sfall_arrays.cc/sfall_global_scripts.cc/sfall_ini.cc/sfall_lists.cc/sfall_kb_helpers.cc, test_stubs provides debugPrint/compat_* stubs, CMakeLists.txt registers each test with add_executable+target_link_libraries+add_test

## [dis-20260709235136-4354d2]
Category: discovery
Tags: fallout2, audit, pass-13
Changed: 2026-07-09T23:51:36.053056

fallout2-ce-extended audit pass 13 complete: 39-domain 78-agent discovery found 33 confirmed MEDIUM+ findings. Key bugs: audio.cc:395 wrong-direction seek heap overflow (CRITICAL), scan_unimplemented_opcodes.h hooks 100% false positives (CRITICAL), 11 missing window.cc -1 guards, rest mode bitmask mismatch (sfall 1,2,4 vs CE 0,1,2), mapGetTimeMultiplier unwired, sound_decoder off-by-one, draw div-by-zero. 84/84 tests pass. Commit 7f58356.

## [dis-20260710023753-cebba3]
Category: discovery
Tags: fallout2-ce, audit, pass-14
Changed: 2026-07-10T02:37:53.479303

fallout2-ce-extended audit pass 14 complete: 86 agents, 2 discovery iterations (CONVERGE=ONCE), 17 unique MEDIUM fixes applied. Files modified: combat.cc, loadsave.cc, scan_unimplemented_opcodes.h, scan_unimplemented_sfall.h, sfall_arrays.cc, sfall_kb_helpers.cc, sfall_metarules.cc, sfall_opcodes.cc, sfall_script_hooks.cc. Build: 0 warnings. Tests: 84/84 pass. Commit d649bd9 pushed to main.

## [con-20260710052652-500f4e]
Category: config
Tags: items, compatibility, documentation
Changed: 2026-07-10T05:26:52.778269

SFALL_COMPATIBILITY.md: HOOK_ITEMDAMAGE has undocumented contract difference — CE arg[4] is hitMode (HIT_MODE_*) while sfall arg[4] is 'type' (attack type flag). No current mods use this hook, so zero impact. Explosion runtime settings (set_explosion_radius, set_attack_explosion_pattern, etc.) are not persisted in savegames — undocumented behavior that matches sfall but should be noted.

## [dis-20260710055606-0abbaa]
Category: discovery
Tags: crash, sfall, interpreter
Changed: 2026-07-10T05:56:06.577407

F-03: Null Program* can reach programExecuteProcedure in scripts.cc:1429 after HOOK_STDPROCEDURE fires hook scripts. Crash address 0x143 = null Program* offset. Fix: re-read program from script->program after hook, or add null guard.

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

## [dis-20260711000705-858d7c]
Category: discovery
Tags: fallout2-ce, audit, production
Changed: 2026-07-11T00:07:05.541138

fallout2-ce-extended audit pass 17 complete: 106 verified findings fixed across 300-agent workflow. RPU crash root cause was interpreter.cc:3218 missing procedure index bounds check in opProcCall. Build: cmake --preset macos-arm64-debug. Tests: ctest --test-dir out/build/macos-arm64-debug — 84 tests. Commit: 2fabd98

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

fallout2-ce-extended: ANY persistent-format change MUST bump the format version — handler count 27→28 (lightSave/lightLoad added, C-04), header-CRC added ungated (C-03, pass-11 0e4b74e claimed F2-21 but had no version gate), save CRC read-back on wb stream (C-02) — together made every old save abort or reject CORRUPT; M-62 mid-stream float broke 1.2R alignment. Fix shape (confirmed): VERSION_MINOR 3→4, version-selected handler lists (legacy 27 vs 28), header CRC verified unconditionally in the CRC era. Corollary: NEVER gate a checksum/protection read on the SAME version field it protects (save NEW-3 — a single versionMajor 3→2 byte flip disables all CRC). This confirms the earlier dis-20260704144725-9b3649 prediction that LOAD_SAVE_HANDLER_COUNT=27 was a hardcoded positional chain with decorative version check.

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
Tags: test, regression, fallout2-ce
Changed: 2026-08-02T23:38:25.892371

fallout2-ce-extended: CODIFYING MIRROR TESTS invert regression detection — test mirrors that duplicate production logic can diverge AND can encode the bug as expected behavior, making CI validate the regression. Examples: M-088/test_map.cc:447 asserted the .edg EOF bug as correct (P-15, PRIOR_FIX 78a8373); F-55/test_fixes_saveload.cc encoded the 0x3FFF opcode-mask bug (R-09: 0xC001 & 0x3FFF = 1 < 768 CHECK fails); UF-H-020/test_misc_ui_config_fixes.cc asserted critterSetBonusStat bonus-0→-2 (the C-05 clamp bug); test_stat blessed the XPTable no-op (R-06); M-99 windowWordWrap test had guards production lacked. Lesson: rewrite the codifying mirror in the SAME pass as the fix (converts it to regression-detecting — a future re-add FAILS CI); mirrors give false confidence — prefer production-linked tests or periodic mirror-vs-production drift checks.

## [got-20260817000651-b24b40]
Category: gotcha
Tags: sfall, config, et-tu
Changed: 2026-08-17T00:06:51.285614

fallout2-ce-extended: contentConfigTryMigrateFromSfall writes gSfallConfig preset '0' defaults into data/config/game#patch.cfg BEFORE contentConfigInit's configRead — any set-if-absent seeding of migrated keys is defeated by migration-written zeros; seed unconditionally (et tu gate keys use this pattern)

## [got-20260817000651-e58a8e]
Category: gotcha
Tags: vfs, fs_copy, path
Changed: 2026-08-17T00:06:51.362963

fallout2-ce-extended: compat_stricmp is pure ASCII case-fold — no '/' vs '\' separator normalization; sfallVfsResolvePath doesn't normalize either (only traversal/absolute/drive-letter rejection + root prepend). fs_copy same-path branch compares RESOLVED paths case-insensitively

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
Tags: config, gotcha, worldmap, fallout2-ce
Changed: 2026-08-17T23:40:41.927062

configGetInt(&config, section, key, ptr, default) with the 5-arg DEFAULT OVERLOAD WRITES the default into *ptr when the key is absent — using it after a prior value was set silently clobbers it. Use the 4-arg present-semantics overload (returns false, leaves *ptr untouched) when a default should not override. Hit in F-072 worldmap start_pos bug (worldmap.cc:1352-1354) where the fork override wrote 173/122 over the upstream [start] worldmap_x/y value; fixed 2026-08-17.

## [got-20260817234043-3adc8f]
Category: gotcha
Tags: fs, stdio, vfs, rpu, sfall, gotcha, fallout2-ce
Changed: 2026-08-17T23:40:43.109649

FILE* handles handed to scripts (fs_create/fs_copy/fs_resize in sfall_opcodes.cc) are stdio-buffered by default — fs_write_short writes sit in user space and are invisible to the engine's independent fopen reads during the session (RPU fs_copy same-path FRM FPS patching was silently inert). Fix: setvbuf(f, nullptr, _IONBF, 0) at open (sfallVfsFopen helper, 2026-08-17). Also: fs_copy cannot open files inside .dat archives (plain fopen) — DAT-resident patching requires the mod's pre-patched DATs.

## [got-20260817234044-9c74f2]
Category: gotcha
Tags: et-tu, sfall, opcode, gotcha, fallout2-ce
Changed: 2026-08-17T23:40:44.050120

et tu ce_enabled macro (fo1.h:62) = metarule_exist('opcode_exists') AND NOT opcode_exists(0x823B). 0x823B is sfall's modified_ini — CE MUST keep it unregistered: implementing it flips ce_enabled=false and disables ALL et tu CE-path script workarounds (gl_apcost.ssl AP handling etc.). Guarded by comment at sfall_opcodes.cc:8903.

## [got-20260818002659-354454]
Category: gotcha
Tags: engine, config, smoke, teststand
Changed: 2026-08-18T00:26:59.283439

CE engine reads fallout2.cfg + ddraw.ini from argv[0] dir (app bundle Contents/MacOS), NOT from cwd/game folder — game folder copies only work when launched with bare argv[0]. Test stand fix: inject per-stand cfg into bundle via smoke/deploy.sh; macos 26 codesign treats every file in Contents/MacOS as code object — sign injected configs individually before re-signing bundle

## [pat-20260818002659-20d078]
Category: pattern
Tags: fallout2, ceu, teststand, setup
Changed: 2026-08-18T00:26:59.362408

CE mac test stand: app bundle goes in game folder (cwd=bundle parent), data paths in cfg resolve against cwd; Et Tu uses '..\master.dat' relative paths; RPU defaults (master.dat, data, mods) match game folder layout so it works even without cfg read

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

## [dis-20260818040923-55654c]
Category: discovery
Tags: fallout2, smoke, deploy, ctest
Changed: 2026-08-18T04:09:23.238747

Smoke stand ops: deployed binary MD5 differs from tree build EXPECTED after deploy.sh re-signs (codesign rewrites the signature blob) — compare __text segment (offset 25632) instead. ctest on the Xcode multi-config build requires -C RelWithDebInfo. Engine auto-erases data/MAPS/*.SAV at launch (gameInitWithOptions→_InitLoadSave→MapDirErase) — stale snapshot shadowing self-cleans on standard flows.

## [got-20260818181617-c2b3de]
Category: gotcha
Tags: fallout2, scripts, interpreter, uaf, root-cause
Changed: 2026-08-18T18:16:17.688032

F-034 (fork 36414b6) set program->exited=true in opExitProgram (O_EXIT_PROGRAM 0x8010 — the NORMAL script-termination opcode). _updatePrograms then freed every script's Program right after its start proc ran, leaving script->program dangling (LOADED flag still set) → dead world (M-46 silently skipped freed memory) + UAF SIGSEGV in scriptExecProc→programExecuteProcedure (null/garbage Program*). Upstream only sets the flag, never exited=true. Reverted in opExitProgram; added programListContains() UAF detector + DBGTRACE lifecycle logging. 92/92 tests pass.

## [got-20260818184036-aa3c8a]
Category: gotcha
Tags: fallout2, scripts, messages, sfall, config, false-positive
Changed: 2026-08-18T18:40:36.159557

BoostScriptDialogLimit (ddraw.ini [Misc]) is a BOOLEAN toggle in sfall ('1 = boost script names 1450→10000'), but fork commit 5222087 assigned the raw value as absolute capacity → BoostScriptDialogLimit=1 set capacity to 1 → every script message lookup (list ID>=2) failed → 'Error' fallback text in message log. Fixed 2026-08-18: nonzero → max capacity (10000). Also: interpreter.cc heap-walk diagnostics used ptr>=heapEnd but the 0x8000 sentinel sits exactly AT heapEnd → false 'heap corruption' spam on every well-formed heap; changed to > in programMarkHeap/programPushString/interpreterPrintStats.

## [got-20260818192654-712c3d]
Category: gotcha
Tags: fallout2, sfall, vfs, fscopy, opcodes, et-tu
Changed: 2026-08-18T19:26:54.185188

fs_copy (sfall_opcodes.cc) was disk-only (compat_fopen): sources inside .dat archives (RPU goris-derobing art\critters\*.frm) or directory mods (et tu classicWM art\INTRFACE\classicWM\*.frm from mods/fo1_base) failed with 'cannot open source'. Fixed 2026-08-18: sfallVfsReadFile() helper reads via engine VFS (fileOpen) with plain-fopen fast path; same-path fs_copy materializes to master_patches dir (data/) so the engine art loader sees patched bytes BEFORE the archive; different-path dest also materialized to patches dir. Also: 0x81a3 eax_available/0x81a4 set_eax_environment stubs added (et tu mods call them; were 'undefined opcode').

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
Changed: 2026-08-18T23:55:58.372942

FO2/et tu MAP file script section format: per type — count(int32), then ceil(count/16) extents; EACH extent = 16 script records + length(int32, validated 0..16) + next(int32, read+discarded) — scriptListExtentRead scripts.cc:2545. Record sizes by SID_TYPE: SYSTEM=16 ints, SPATIAL=18, TIMED=17, ITEM=16, CRITTER=16 (64/72/68/64/64 bytes). et tu SHADYW.MAP: type3 ITEM count=8, type4 CRITTER count=33; object section starts after scripts. Object record = 18 ints (id,tile,x,y,sx,sy,frame,rot,fid,flags,elev,pid,cid,lightDist,lightInt,outline,sid,scriptIdx) + objectDataRead (objectWrite object.cc:704). Map header: 236 bytes + gv*4 + lv*4 + squares (10000 ints per present elevation; flags 0x2/0x4/0x8 = elev present).

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

HOOK_KEYPRESS sentinel contract (input.cc vs sfall_kb_helpers.cc): this fork's input.cc treats -1 from sfall_kb_handle_key_pressed as BLOCK the key (F-27 keyBlocked), 0/SDL_SCANCODE_UNKNOWN as pass-through. Upstream fallout2-ce treats -1 as 'no override/pass' and 0 as swallow — EXACT OPPOSITE. The Aug 16 sync merge (16bc1568 via b405e59) flipped the two no-hook early returns in sfall_kb_helpers.cc to -1, swallowing EVERY key when !gGameLoaded (main menu, character creation — gGameLoaded only set after char-selector at main.cc:160) or when no script registered HOOK_KEYPRESS (save-name dialog). Symptom: text fields dead, backspace dead, held keys 'occasionally work' (SDL repeat events skip the hook at input.cc:1034). Fixed: those two returns are SDL_SCANCODE_UNKNOWN again; only ret0==255-swallow returns -1. Regression tests: test_sfall_kb_helpers.cpp PRODUCTION: handle_key_pressed sections.

## [got-20260819200251-0b9c45]
Category: gotcha
Tags: fallout2, smoke, config
Changed: 2026-08-19T20:02:51.435125

Smoke-stand fallout2.cfg: the bundle copy (Contents/MacOS/fallout2.cfg) is the LIVE config — engine reads/writes next to executable. Stand-root cfg is only the deploy template. deploy.sh overwrites bundle copy, wiping in-game persisted keys (RPU bundle has extra [qol]/[ui]/mouse_lock). Edit bundle copy directly for live changes; keep stand-root in sync for future deploys. et tu bundle copy had CRLF endings.

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

## [got-20260819224159-49fb8d]
Category: gotcha
Tags: map-format, loadsave
Changed: 2026-08-19T22:41:59.403049

FO2 MAP object section: objects with inventory->length != 0 carry nested item records after their base record — each item = quantity(int32) + full objectRead (18 ints + objectDataRead, recursively). Critter base record = 72 + inv(3) + reaction/flags(1) + combat(7) + hp/rad/poison(3) ints. omitting inventory recursion desyncs the walk.

## [dis-20260819224159-c93cf1]
Category: discovery
Tags: et-tu, rendering, walls, shady-sands
Changed: 2026-08-19T22:41:59.475548

Et Tu Shady Sands 'striped homes': FO1 adobe walls = 16px/32px-wide FRM pieces at every other tile with 1x1 block.frm markers, two interleaved rows. Engine placement math (identical FO1/FO2) leaves 8px vertical slits every 48px at scale 1 (16px at scale 2) — slits are IN THE DATA, rendered faithfully by any engine. All 94 wall FRMs byte-identical to FO1 MASTER.DAT, size==w*h (no RLE). See KNOWN_ISSUES.md Issue A.

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
Tags: barter, ettu, weight, gotcha
Changed: 2026-08-23T01:42:59.816250

FO1 box traders (et tu) are intentionally overweight: get_barter_inven/move_obj_inven_to_obj force-loads full stock at talk start (itemMoveAll = force). ANY non-force itemMove/itemAttemptAdd targeting the trader fails with rc=-6 (max weight) — including returning the trader's own items from the barter table. All barter moves must use itemMoveForce (barterMoveToTable, barterMoveFromTable both directions, barterAttemptTransaction M-90/M-101); weight gates only make sense on the player side (barterAttemptTransaction check).

## [pat-20260823014307-d4e3fd]
Category: pattern
Tags: barter, pattern
Changed: 2026-08-23T01:43:07.726974

Barter move functions: the undo direction (table -> critter inventory) must use itemMoveForce like the setup direction (inventory -> table). Upstream CE uses itemMoveForce in BOTH branches of barterMoveFromTable. A non-force itemMove in any barter path is a regression trap for FO1 box traders; grep for non-force itemMove (not itemMoveForce/itemMoveAll) inside barter* functions when reviewing barter fixes. Prior fix e17707f2 fixed only barterAttemptTransaction and missed barterMoveFromTable — completed in adbcc2f9 (M-101).

## [got-20260823161635-5f96bd]
Category: gotcha
Tags: overload, perk, merge, gotcha
Changed: 2026-08-23T16:16:35.059861

Adding a typed overload (e.g. perkGetMaxRank(Perk)) next to an existing int-overload (perkGetMaxRank(int)) silently changes behavior at ALL existing call sites that pass a typed enum arg — C++ overload resolution prefers the exact enum match, bypassing the int overload's extra semantics (here: set_perk_ranks override + -1 invalid contract). When merging upstream code that adds an enum-typed overload, grep existing callers first and check whether the new overload's semantics differ from the int overload; if they do, drop the new overload and let enum->int conversion bind to the canonical one (upstream #698 -> fork fix 6744652a). Same class as configGetInt default-overload gotcha (got-20260817234041).

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

## [got-20260824035223-e2d5ba]
Category: gotcha
Tags: fid, armor, inventory, party, gotcha
Changed: 2026-08-24T03:52:23.971026

Non-dude critter armor FID staleness: inventoryEquipFunc (inventory.cc:3885) refreshes critter->fid ONLY for gDude (animationRegisterSetFid); the non-dude else on armor calls only adjustCritterStatsOnArmorChange → sprite stale until object recreated. Same asymmetry in canonical FO1-CE inventry.cc:2839-2844 (invisible there: no party-equip UI). Fix pattern (verified): after stats call, buildFid(OBJ_TYPE_CRITTER, armorBaseFrmId|protoBaseFrmId, ANIM_STAND, weaponAnimationFromFid(critter->fid), critter->rotation+1) + objectSetFid(critter,fid,&rect) + tileWindowRefreshRect(&rect,0) — set the POST-REMOVAL base from the critter proto base (proto->fid & 0xFFF), never from the removed armor's fid. Missing sites fixed: equip else, unequipLootArmorFunc, barter strip/close, correctFidForRemovedItem (unwield), _obj_remove_from_inven armor case.

## [got-20260824035226-28de87]
Category: gotcha
Tags: healing, fo1, rest, worldmap, gotcha
Changed: 2026-08-24T03:52:26.127700

FO1-CE rest/travel heal is GRADUAL, not instant: partyMemberRestingHeal=(hours/3)×healingRate (tmp/fo1-ce/src/game/party.cc:438-459); pipboy rest _Check4Health fires after 180 rest-min; worldmap heals per game-day of travel. The fork's gFallout1Behavior instant-full-heal block (party_member.cc:878-895, from f40c9611 '50 verified fixes') deviated from real FO1 — DELETED. Post-fix worldmap heal math: loop fires every >1000 REAL ms while worldmap open (worldmap.cc:4041, not walking-gated, fires on open since partyHealTime=0) with healHours=3 → per-game-hour rate = R/30 HP (game time advances 0.5h/frame at ~60fps). Any new FO1-related gFallout1Behavior consumer must be checked against FO1-CE behavior, not assumptions.

