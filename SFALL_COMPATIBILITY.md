# Sfall Compatibility

This document tracks the sfall scripting surface implemented by the CE fork. It is for modders
who need to know which sfall features work, which have contract differences, and what to use
instead when a feature is deliberately absent.

The sfall-specific documentation at [sfall-team.github.io/sfall](https://sfall-team.github.io/sfall/)
is the authoritative reference for individual functions; this page only records the CE status
per feature group and any behavior differences from sfall.

## Overview

| Area | Status |
| --- | --- |
| **sfall version reported** | 4.5.1 (`sfall_ver_major=4`, `sfall_ver_minor=5`, `sfall_ver_build=1`) |
| **Scripting surface** | 134 metarules, 296 opcode registrations |
| **Hooks** | 45 of 62 hook slots implemented (40 sfall hooks + 5 CE-specific hooks); 10 sfall hooks deliberately absent (see Hooks) |
| **RPU** | Supported — full hook/opcode/metarule/ddraw.ini surface implemented |
| **Et Tu** | Supported — FO1-mode engine behavior + config overlays implemented |
| **Save compatibility** | Backward-compatible; sfall global-vars and override state serialized |
| **Tests** | 92 test executables (unit/mirror; not a substitute for in-game testing) |

## Settings (ddraw.ini → CE config files)

Settings previously read from `ddraw.ini` have been moved into standard CE config files.
Content-mod settings live in `<DAT>/config/game.cfg` (overridable by mods); engine-level
settings moved into `fallout2.cfg`.

| ddraw.ini section | ddraw.ini key | CE config file | CE config key |
| --- | --- | --- | --- |
| `Misc` | `SkipOpeningMovies` | fallout2.cfg `[ui]` | `skip_opening_movies` |
| `Misc` | `DisplayKarmaChanges` | fallout2.cfg `[ui]` | `display_karma_changes` |
| `Misc` | `DisplayBonusDamage` | fallout2.cfg `[ui]` | `display_bonus_damage` |
| `Misc` | `NumbersInDialogue` | fallout2.cfg `[ui]` | `numbers_in_dialogue` |
| `Misc` | `AutoQuickSave` | fallout2.cfg `[ui]` | `auto_quick_save` |
| `Main` | `EnableHighResolutionStencil` | fallout2.cfg `[ui]` | `enable_high_resolution_stencil` |
| `Misc` | `ConsoleOutputPath` | fallout2.cfg `[debug]` | `console_output_path` |
| `Misc` | `GaplessMusic` | fallout2.cfg `[sound]` | `gapless_music` |
| `Misc` | `ScreenshotsFormat` | fallout2.cfg `[system]` | `screenshots_format` |
| `Misc` | `UseWalkDistance` | fallout2.cfg `[qol]` | `use_walk_distance` |
| `Misc` | `AutoOpenDoors` | fallout2.cfg `[qol]` | `auto_open_doors` |
| `Misc` | `StartGDialogFix` | game.cfg `[dialog]` | `start_gdialog_fix` |
| `Misc` | `StartingMap` | game.cfg `[start]` | `map` |
| `Misc` | `StartYear` / `StartMonth` / `StartDay` / `StartTime` | game.cfg `[start]` | `year` / `month` / `day` / `time` |
| `Misc` | `StartXPos` / `StartYPos` | game.cfg `[start]` | `worldmap_x` / `worldmap_y` |
| `Misc` | `ViewXPos` / `ViewYPos` | game.cfg `[start]` | `worldmap_view_x` / `worldmap_view_y` |
| `Misc` | `XPTable` | game.cfg `[stats]` | `xp_table` |
| `Misc` | `DisableSpecialMapIDs` | game.cfg `[maps]` | `disable_special_map_ids` |
| `Misc` | `Movie1` – `Movie32` | game.cfg `[movies]` | `movie1` – `movie32` |
| `Misc` | `Fallout1Behavior` movie behavior | game.cfg `[movies]` | `endgame_play_after_slideshow`, `endgame_movie_male`, `endgame_movie_female` |
| `Sound` | `MainMenuMusic` | game.cfg `[sound]` | `main_menu_music` |
| `Sound` | `WorldMapMusic` | game.cfg `[sound]` | `worldmap_music` |
| `Sound` | `WorldMapCarMusic` | game.cfg `[sound]` | `worldmap_car_music` |
| `Sound` | `EndGameMovieMusic0` / `EndGameMovieMusic1` | game.cfg `[sound]` | `endgame_movie_music0` / `endgame_movie_music1` |
| `Sound` | `MapLoadingSound` | game.cfg `[sound]` | `map_loading_sound` |
| `Interface` | `WorldMapTerrainInfo` | game.cfg `[worldmap]` | `terrain_info` |
| `Misc` | `WorldMapFPSPatch` + `WorldMapDelay2` | game.cfg `[worldmap]` | `travel_delay` |

Notes:

- There is no single `Fallout1Behavior` switch. The movie portion is configured directly:
  `endgame_play_after_slideshow=0`, `endgame_movie_male=10`, `endgame_movie_female=11`
  for the sfall Fallout 1 movie sequence. Time-limit and item-weight behavior are separate.
- `travel_delay` throttles only world-map travel simulation; input, rendering, and world-map
  scripts run at the normal frame rate (unlike sfall).

### Speed control (`ddraw.ini [Speed]`)

| Key | Default | Description |
| --- | --- | --- |
| `SpeedMultiInitial` | 100 | Initial speed multiplier percentage. Preferred; falls back to `SpeedMulti`. |
| `SpeedMulti` | 100 | Fallback multiplier used when `SpeedMultiInitial` is absent. |

Applied to all animation types in `animationComputeTicksPerFrame()`. Scripts can change it at
runtime via `set_sfall_global(0, value)` / `get_sfall_global_int(0)`. Values ≤ 0 are clamped to
100. Independent of the FPS limiter (it controls animation speed, not rendering rate).

## Opcodes / Metarules

Status legend: ✅ implemented (group may include exceptions noted in the Notes column) ·
⛔ not implemented (deliberately absent) · ⚠️ partial.

| Group | Opcodes / metarules | Status | Notes |
| --- | --- | --- | --- |
| Direct memory access | `read_byte/short/int/string`, `write_byte/short/int/string`, `call_offset_v*` | ⚠️ | `read_byte` supports a small whitelist (0x56D38C combat highlight, 0x410003 Rotators fork detection → 0xF4). Other read_* return -1; write_* and all `call_offset_v*/r_*` are registered safe no-ops (require `AllowUnsafeScripting=1`). |
| Stats | `get/set_pc_base_stat`, `get/set_pc_extra_stat`, `get/set_critter_base_stat`, `get/set_critter_extra_stat` | ✅ | CE uses engine stat helpers; derived-stat update behavior can differ. |
| Stats / min-max | `get/set_stat_min/max`, `set_pc_stat_min/max`, `set_npc_stat_min/max` | ✅ | Implemented in sfall_metarules.cc:2095,2111 + sfall_opcodes.cc. |
| Skills | `get/set_critter_skill_points`, `get/set_available_skill_points`, `set_skill_max`, `set_critter_skill_mod`, `set_base_skill_mod`, `mod_skill_points_per_level` | ✅ | `mod_skill_points_per_level` clamps to [-100, 100] (consumed at `characterEditorUpdateLevel`). `set_critter_skill_mod`/`set_base_skill_mod` consumed in `skillGetValue()` via sfall accessors. |
| Maps / worldmap | `get_world_map_x/y_pos`, `set_world_map_pos` | ✅ | |
| Audio | `play_sfall_sound`, `stop_sfall_sound` | ✅ | `.acm`, `.wav`, `.ogg` (incl. from `.dat` archives). `.mp3` not supported. |
| Weapons and ammo | `get/set_weapon_ammo_pid`, `get/set_weapon_ammo_count` | ✅ | |
| Version | `sfall_ver_major/minor/build` | ✅ | CE reports 4.5.1. |
| Math | `log, exponent, round, sqrt, abs, sin, cos, tan, arctan, ceil, ^, floor2, div` | ✅ | |
| Keyboard/mouse | `key_pressed`, `tap_key`, `get_mouse_x/y`, `get_mouse_buttons` | ✅ | DIK codes or VK codes with the `0x80000000` flag (see key mapping section). |
| Lists | `list_begin/next/end`, `list_as_array`, `party_member_list` | ✅ | |
| Explosions | explosion pattern/art/radius/fire/damage/max-targets metarules, `item_make_explosive` | ✅ | |
| Animations | `reg_anim_combat_check/destroy/animate_and_hide/light/change_fid/take_out/turn_towards/callback/animate_and_move` | ✅ | |
| Art/appearance | `art_exists`, `art_frame_data`, `refresh_pc_art`, `art_cache_clear` | ✅ | |
| Tiles/paths | `get_tile_fid`, `tile_under_cursor`, `tile_light`, `tile_get_objs`, `tile_refresh_display`, `obj_blocking_tile/line`, `tile_by_position`, `get_tile_ground_fid`, `get_tile_roof_fid`, `path_find_to`, `objects_in_radius` | ✅ | `get_tile_ground_fid`/`get_tile_roof_fid` are wrappers around `get_tile_fid` (mode 0=ground, 1=roof). |
| Utility | `sprintf`, `typeof`, `atoi`, `atof` | ✅ | |
| Strings | `string_split`, `substr`, `strlen`, `charcode`, `get_string_pointer`, `string_find[_from]`, `string_format[_array]`, `string_replace`, `string_to_case`, `string_compare` | ✅ | `get_string_pointer` deprecated — intentionally omitted. |
| Interface / tags | `show_iface_tag`, `hide_iface_tag`, `is_iface_tag_active`, `set_iface_tag_text`, `add_iface_tag` | ⚠️ | `show/hide` do not work for tag values 1 (Poisoned) and 2 (Radiated). `set_iface_tag_text`/`add_iface_tag` only for custom tags ≥ 5 (generic box mechanism — cosmetic divergence from sfall's dedicated graphics). Legacy `BoxBarCount`/`BoxBarColours` settings not supported. |
| Global variables | `set_sfall_global`, `get_sfall_global_int`, `get_sfall_global_float` | ✅ | `set_sfall_global` stores ints; float getter falls back to int (`sfall_global_vars.cc`). |
| Hooks / hook functions | `init_hook`, `get_sfall_arg[s_at]`, `set_sfall_return`, `set_sfall_arg`, `register_hook`, `register_hook_proc[_spec]` | ✅ | `init_hook` deprecated — not implemented. `register_hook_proc` and `_spec` both add hooks to the *end* of the hook list (sfall has begin/end respectively). |
| Arrays | `create_array`, `temp_array`, `fix_array`, `get/set_array`, `resize_array`, `free_array`, `scan_array`, `len_array`, `save/load_array`, `array_key`, `arrayexpr` | ✅ | |
| Perks and traits / NPC perks | `set_fake_perk_npc`, `set_fake_trait_npc`, `set_selectable_perk_npc`, `has_fake_perk_npc`, `has_fake_trait_npc` | ⛔ | Not implemented (no CE NPC-perk layer). |
| Global scripts | `set_global_script_repeat`, `set_global_script_type`, `available_global_script_types` | ⚠️ | `available_global_script_types` not implemented. |
| Combat | `attack_is_aimed`, `block_combat`, `force_aimed_shots`, `disable_aimed_shots`, `get_attack_type`, `get/set_bodypart_hit_modifier`, `combat_data`, `get/set/reset_critical_table`, `get_last_target`, `get_last_attacker`, `set_critter_burst_disable`, `get/set_critter_current_ap`, `set_spray_settings`, `get/set_combat_free_move`, `set_fo1_hit_chance` | ⚠️ | `block_combat`, `get_last_target`, `get_last_attacker`, `set_spray_settings` not implemented. `force/disable_aimed_shots` integrate at `critterCanAim` (pid-keyed sets) — see Mutants Rising row. |
| Car | `set_car_current_town`, `car_gas_amount`, `set_car_intface_art` | ✅ | |
| Interface / outline | `outlined_object`, `get_outline`, `set_outline` | ✅ | |
| Interface / main | `intface_is_hidden/redraw/hide/show`, `set_quest_failure_value` | ✅ | `intface_redraw` supports 0-arg and 1-arg forms. |
| Interface / inventory | `display_stats`, `inventory_redraw`, `critter_inven_obj2`, `get_current_inven_size`, `item_weight` | ✅ | |
| Interface / cursor | `get/set_cursor_mode` | ✅ | |
| Locks | `lock_is_jammed`, `unjam_lock`, `set_unjam_locks_time` | ✅ | `set_unjam_locks_time` overrides the 24-hour unjam threshold. |
| INI settings | `get_ini_setting`, `get_ini_string`, `get_ini_section[s]`, `get_ini_config[_db]`, `set_ini_setting` | ✅ | `modified_ini` deprecated — omitted. |
| Objects and scripts | `set_self`, `set_dude_obj`, `real_dude_obj`, `remove_script`, `get/set_script`, `obj_is_carrying_obj`, `loot_obj`, `dialog_obj`, `obj_under_cursor`, `get/set_object_data`, `get/set_flags`, `set_unique_id`, `set_scr_name`, `obj_is_openable`, `get/set_proto_data`, `get_object_ai_data` | ✅ | `set_dude_obj`/`real_dude_obj`/`set_object_data`/`set_scr_name` are metarules. `get_object_ai_data` type 0 (AI packet) implemented; types 1-2 via `aiPacketGetFlags`/`aiPacketGetProcedure`. **`get/set_object_data` contract:** field-mapped through `ObjectDataField`/`AttackDataField` with sfall-matching `C_ATTACK_*` offsets (e.g. `C_ATTACK_FLAGS_TARGET` 0x30 → `attack->defenderFlags`). `set_object_data` accepts `ARG_ANY` for Object* fields; unsupported offsets return -1. **Compat delta:** raw-byte writes to Object position members (offsets 8/12/16/20) are **not** supported — scripts that repositioned objects through raw offsets must use `OBJ_DATA_TILENUM`/`OBJ_DATA_ELEVATION`. |
| Game management | `set_movie_path`, `stop/resume_game`, `mark_movie_played`, `game_loaded`, `get_game_mode`, `get_uptime`, `signal_close_game` | ✅ | `set_movie_path` runtime override + game.cfg `[movies]`. `stop/resume_game` safe no-ops. |
| Gameplay tweaks | `set_pickpocket_max`, `set_hit_chance_max`, `set_xp_mod`, `set_critter_hit_chance_mod`, `set_base_hit_chance_mod`, `set_hp_per_level_mod`, `gdialog_get_barter_mod`, `get/set_unspent_ap[ _perk]_bonus`, `set_base_pickpocket_mod`, `set_critter_pickpocket_mod`, `get/set_inven_ap_cost`, `set_drugs_data`, `get/mod_kill_counter`, `set_pipboy_available` | ✅ | Per-critter hit-chance mod additive with global base mod. Pickpocket cap uses `sfallGetPickpocketMax()` (95 fallback). |
| NPCs | `inc_npc_level`, `get_npc_level`, `npc_engine_level_up` | ✅ | `get_npc_level` delegates to `partyMemberGetLevel`. |
| Hero appearance | `set_dm/df_model`, `hero_select_win`, `set_hero_race`, `set_hero_style` | ✅ | Values stored in sfall globals. Always-on (no config flag needed). |
| Events | `add_g_timer_event`, `remove_timer_event`, `create_spatial`, `spatial_radius` | ✅ | |
| Other | `get_year`, `active_hand`, `toggle_active_hand`, `get/set_viewport_x/y`, `get_light_level`, `message_str_game`, `sneak_success`, `unwield_slot`, `add_extra_msg_file`, `get_metarule_table`, `metarule_exist` | ✅ | `get/set_viewport_x/y` safe stubs (SDL2 scroll is engine-managed). `input_funcs_available`, `nb_create_char` deprecated in sfall — absent. `add_extra_msg_file` supports the 2-arg form. |
| Perk level control | `get/set_perk_owed` (0x818E/0x818F), `get_perk_available` (0x8190), `set_perk_freq` | ✅ | Owed counter clamped [0,255]; persisted via sfall_gl_vars. |

Note: metarule groups consolidate many individual metarules; `metarule_exist()` probes against the
134-entry table. CE-only metarules are listed below.

### CE-only metarules

CE defines metarules not present in sfall. Include [ce.h](files/ce.h) for the `#defines`.

| Name | Definition |
| --- | --- |
| `encounter_intros(toggle)` | Enable/disable the display-monitor random-encounter intro message (`You encounter: ...`). Does not affect the separate encounter-detection dialog. |
| `set_reaction_thresholds(neutral, good)` | Thresholds for "neutral" / "good" reactions. Defaults: FO1 -25/25, FO2 -51/49. |
| `set_party_member_cc_msg_ids(pid, start_msg_id, end_msg_id)` | Override party-member combat-control messages for a pid; random pick from the inclusive range. Defaults 670-674 (humans), 677-678 (hardcoded dog pids). |
| `rest_option_msgs(base_msg_id)` | Pip-Boy rest option labels base id; reads `base_msg_id` through `base_msg_id + 13`. Default FO2 302-315, FO1 321-334. |
| `set_rest_option(rest_option, value)` | Wake hour for rest options 8-11 (morning/noon/evening/midnight). Defaults 8, 12, 18, 0 (FO1 morning: 6). |

## Hooks

45 of 62 hook slots implemented: 40 sfall hooks + 5 CE-specific (ids 49-53).

| Hook | ID | Status | Notes |
| --- | --- | --- | --- |
| ToHit | 0 | ✅ | |
| AfterHitRoll | 1 | ✅ | Overriding `defender` leaves attack state (distance, roundsHitMainTarget) stale. |
| CalcAPCost | 2 | ✅ | |
| DeathAnim1 | 3 | 🚫 | Use DeathAnim2 (same anim-override ability from the standard death path). |
| DeathAnim2 | 4 | ✅ | |
| CombatDamage | 5 | ✅ | |
| OnDeath | 6 | ✅ | **Notification-only:** fires after `DAM_DEAD` is set; `maxReturnValues=0` — scripts cannot alter death. Fires once at `critterKill()` for all death paths. |
| FindTarget | 7 | ⚠️ | **Contract difference:** 2-arg layout (attacker, target; 1 return). sfall uses 5 args (attacker, combat-group, current target, previous target, area-attack mode). Fires at 4 combat_ai.cc sites. Arg0/arg1 values differ from sfall expectations. |
| UseObjOn | 8 | ✅ | |
| RemoveInvenObj | 9 | ✅ | Reason constants + `*_HAND`/`*_ARMOR_SLOT`/`_CONTAINER`/`_GROUND`/`_PICKUP`/`_CHARACTER_PORTRAIT`/`_BARTER` sub-ids supported. |
| BarterPrice | 10 | ✅ | |
| MoveCost | 11 | ✅ | |
| HexMoveBlocking / HexAIBlocking / HexShootBlocking / HexSightBlocking | 12-15 | 🚫 | Deliberately absent — per-hex callbacks in pathfinding are impractical in the restructured CE pipeline. Use FindTarget / CanUseWeapon instead. |
| ItemDamage | 16 | ✅ | |
| AmmoCost | 17 | ✅ | Requires `check_weapon_ammo_cost=1` for pre-attack validation to respect per-shot/per-round overrides. |
| UseObj | 18 | ✅ | sfall-matching inconsistency around return code 2 between interface contexts. |
| KeyPress | 19 | ✅ | 3-arg layout (state, DIK, VK). ret0=255 swallows the key; ret0 1-263 remaps it. See key-mapping section. |
| MouseClick | 20 | ✅ | |
| UseSkill | 21 | ✅ | |
| Steal | 22 | ✅ | |
| WithinPerception | 23 | ✅ | |
| InventoryMove | 24 | ✅ | |
| InvenWield | 25 | ✅ | |
| AdjustFID | 26 | ✅ | Second arg currently mirrors the first (no internal FID modifiers in CE). |
| CombatTurn | 27 | ✅ | |
| CarTravel | 28 | ✅ | Ret0=steps (-1 keep), ret1=fuel (-1 keep); default fuel 100/tick. |
| SetGlobalVar | 29 | ✅ | Integer values only. ret0 overrides the stored value. |
| RestTimer | 30 | ✅ | Only ret0 == 1 interrupts. Ticks wrap every 6.8y. |
| GameModeChange | 31 | ✅ | |
| UseAnimObj | 32 | ✅ | Fires on `animate_stand_obj` and `animate_stand_reverse_obj`. |
| ExplosiveTimer | 33 | ✅ | |
| DescriptionObj | 34 | ✅ | Supports sfall 4.4.0+ direct string return. |
| UseSkillOn | 35 | ✅ | |
| OnExplosion | 36 | ✅ | |
| SubCombatDamage | 37 | 🚫 | |
| SetLighting | 38 | ✅ | |
| Sneak | 39 | ✅ | arg0=result, arg1=duration ticks, arg2=critter; ret0/ret1 override. |
| StdProcedure | 40 | ✅ | |
| StdProcedureEnd | 41 | ✅ | |
| TargetObject | 42 | ✅ | Fires at start of `_combat_attack`; args attacker/defender/hitMode/hitLocation. |
| Encounter | 43 | ✅ | 5-arg sfall layout (event type 0=random/1=local-map-enter, mapId, isSpecial, tableId, entryId). **Forced encounters do not fire the hook.** ret0 overrides mapId (-1 cancels event 0); ret1 (event 0 only) returns 1 to cancel + load ret0 map. |
| AdjustPoison | 44 | 🚫 | |
| AdjustRads | 45 | ✅ | Fires at `critterAdjustRadiation()`; used by et tu's rads-2000 failsafe. |
| RollCheck | 46 | 🚫 | Deliberately absent — randomRoll() has 30+ call sites without event context; per-roll hooks too expensive. |
| BestWeapon | 47 | 🚫 | Deliberately absent — `_ai_best_weapon()` has 10+ return points; pointer lifetime concerns. |
| CanUseWeapon | 48 | ✅ | |
| Dialog | 49 | ✅ [CE] | Fires start (speaker, headFid, reaction) and exit (speaker, -1, -1). |
| DialogReaction | 50 | ✅ [CE] | Fires on dialog reaction: (speaker, reaction -2/-1/0). |
| StatLevelUp | 51 | ✅ [CE] | Fire sites: `pcAddExperienceWithOptions()`, `characterEditorUpdateLevel()`. |
| Barter | 52 | ✅ [CE] | Fire site: `gameDialogBarter()`. |
| Message | 53 | ✅ [CE] | Fire site: `displayMonitorAddMessage()`. |

**Reserved slots 54-60** — registrations are accepted but never fire. **61 (BuildSfxWeapon)** —
deliberately absent: weapon sound names come from a static buffer with no script-string return
support; mods needing custom sounds should set proto data via `set_weapon_sound`.

Registering a 🚫 / reserved hook type emits a debugPrint warning — harmless, but a signal that
the hook will never fire. Reserved slots are available for future use; add a fire site before
claiming one.

### Key codes: DIK / VK → SDL

CE uses SDL2, not DirectInput. Script helpers accept **DIK** codes (0-255) or **VK** codes with
the `0x80000000` flag, converted internally via `get_scancode_from_key()`. `HOOK_KEYPRESS` passes
sfall-compatible args: arg0=pressed state, arg1=DIK, arg2=VK.

In practice for RPU/Et Tu scripts:

- Pass DIK codes (e.g. `DIK_A` = 30) or VK + flag (`0x80000041` for VK_A) to `key_pressed()`/`tap_key()`.
  Do not pass raw `SDL_Keycode` values — `SDLK_a` = 97 is not `DIK_A` = 30.
- In `HOOK_KEYPRESS` handlers, arg1 is already DIK — pass it directly to a `key_pressed()` trampoline
  if needed.
- VK→SDL mapping is a fully-populated 256-entry table (`kVkToSdl`); DIK→SDL (`kDiks`) covers the
  common character/navigation/numpad/function-key ranges and returns `SDL_SCANCODE_UNKNOWN` for
  unmapped entries (OEM keys, reserved ranges). For unmapped DIK codes, use the VK form.

## Et Tu (Fallout 1 in FO2)

Et Tu runs as a total conversion: its global scripts, 1000+ map scripts, FO1 content data, and
three config files (`Fallout2.cfg`, `data/config/game#patch.cfg`, `ddraw.ini`) run on CE
(analysis snapshot: et tu master `c154bb8`, 2026-08-11).

### Requirement status (Et Tu)

| Requirement | Status |
| --- | --- |
| sfall version gate (`sfall_ver_major < 5`) | ✅ — CE reports 4.5.1 |
| Rotators detection (`read_byte(0x410003)==0xF4` + `metarule_exist("rotators")`) | ✅ |
| CE detection (`ce_enabled` = `opcode_exists(0x823B)==false`) | ✅ — `modified_ini` intentionally not implemented |
| Startup gate (`get_ini_setting("ddraw.ini\|...")` for AllowUnsafeScripting, DisableHorrigan, UseFileSystemOverride, Fallout1Behavior) | ✅ — `contentConfigSeedEtTuGateKeys` seeds the script-visible keys when the `game#patch.cfg` overlay is deployed |
| FO1 hit chance (`set_fo1_hit_chance(true)`) | ✅ — consumed at `combat.cc:4859-4868` with `gFallout1Behavior` |
| FO1 worldmap labels (`remove_wm_town_names(true)`) | ✅ — label gate at `worldmap.cc:6762-6763` |
| Encounter handling (`encounter_detection(false)`) | ✅ — metarule → `wmSetEncounterDetection` |
| HOOK_ENCOUNTER | ✅ — sfall arg0 encoding; forced encounters do not fire |
| Reaction thresholds (`set_reaction_thresholds(25, 75)`) | ✅ — FO1/FO2 defaults preserved on reset |
| Rest hours/strings (`rest_option_msgs(320)`, `set_rest_option(REST_OPTION_MORNING, 6)`) | ✅ — FO1 defaults 6:00 / base 321 |
| FO1 XP progression (`XPTable`, `[stats] xp_table`) | ✅ — both consumed |
| FO1 start position (`[start] worldmap_x/y`) | ✅ — `-1` falls back to starting map's area; ddraw.ini alternative works |
| Worldmap viewport (`[start] worldmap_view_x/y`) | ✅ |
| Worldmap terrain info (`[worldmap] terrain_info=1`) | ✅ |
| Worldmap travel speed (`WorldMapFPSPatch`/`WorldMapDelay2`, `[worldmap] travel_delay`) | ✅ |
| Travel markers | ✅ |
| Special-map-ID disabling | ✅ |
| QuickPockets AP reduction | ✅ |
| PA weight (FO1 = not halved) | ✅ — CE halves unconditionally; et tu's `adjust_pa_weight` makes the total exact |
| Party member dialog (`set_party_member_cc_msg_ids`) | ✅ |
| Party armor appearance / weapon restrictions | ✅ |
| TMA (Tell Me About) | ✅ — HOOK_KEYPRESS DIK arg1 + ret0=255 swallow |
| Motorcycle | ✅ |
| Auto doors / push / armor destroy | ✅ |
| Ammo INI loader | ✅ |
| `get/set_proto_data` offsets used by et tu | ✅ — bounds + alignment checked |
| `game#patch.cfg` overlay mechanism | ✅ — VFS-aware |
| Perks.ini (`PerksFile=config\Perks.ini`) | ✅ — `[PerksTweak]`, `[Perks]`, `[Traits]` applied |
| NPC combat control (`[CombatControl] Mode=3`) | ⛔ — out of scope (owner decision); no et tu script depends on it |
| Item highlighting (`[Highlighting]`) | ⛔ — out of scope; no et tu script depends on it |
| Optional engine settings (`WorldMapTimeMod`, `WorldMapEncounterFix/Rate`, `UseScrollingQuestsList`, `ItemCounterAutoCaps`, `DeathScreenFontPatch`, `EnableMusicInDialogue`) | ⚠️ — worldmap trio done; rest out of scope (et tu ships them disabled or relies on cosmetic-only behavior) |
| Rotators-only metarules (`r_call_offset*`, `r_hrp*`) | ✅ — registered as safe no-ops so `metarule_exist()` probes succeed; VOODOO/HRP behavior itself not implemented (zero runtime impact — no shipped et tu script calls them) |

### Remaining work (et tu)

- **P3 in-game verification** — `set_reaction_thresholds(25,75)` persistence across game reset
  (partially verified; needs an in-game run).
- **Out of scope** — NPC combat control, item highlighting, and the four optional QoL settings.
- **Standing check** — keep the HOOK_ENCOUNTER / HOOK_KEYPRESS rows accurate when hook code changes.

## RPU (Fallout 2 Restoration Project)

RPU requires sfall 4.5 (CE reports 4.5.1). RPU's scripting surface is small: 4 sfall hooks,
~25 opcodes/metarules, and ddraw.ini config keys (analysis snapshot: RPU master `f7c10859`,
2026-08-10). All are implemented.

### Requirement status (RPU)

| Requirement | Status |
| --- | --- |
| Hooks: UseObjOn, UseObj, GameModeChange, CombatDamage | ✅ — RPU registers only these 4; CombatDamage's 13-arg layout matches RPU's sequential `get_sfall_arg` reads |
| `get/set_ini_setting`, `get_ini_string` (mods\*.ini, ddraw.ini keys) | ✅ |
| `WorldMapSlots=21` (RPU gate) | ✅ — default 21 via sfall config |
| `BoostScriptDialogLimit=1` (RPU gate) | ✅ — absent key returns -1 (never 0); dialog capacity inherently satisfies the boost |
| `EnableHeroAppearanceMod=1` | ✅ — always-on in CE |
| `ElevatorsFile=mods\elevators.ini` format | ✅ |
| `UseFileSystemOverride=1` gate | ✅ — seeded for et-tu-style deployments; flag intentionally unwired engine-side (CE VFS priority handles override), only the script-visible value matters |
| `ExtraSaveSlots=1` | ✅ — 100 save pages / 1000 slots |
| `KarmaFRMs` / `KarmaPoints` | ✅ — `[karma] frms/points` |
| `FemaleDialogMsgs=1-2` | ✅ — female dialog/cutscene dirs with per-directory fallback (no impact on English installs) |
| `OverrideArtCacheSize=1` | ✅ — art cache selection 8..512 MB |
| `BoxBarColours` | ⚠️ — parsed, accepted-but-inert (no CE box-bar rendering equivalent) |
| `ProcessorIdle=1` | ✅ — satisfied by the FPS limiter (no busy-wait exists) |
| `[Scripts] IniConfigFolder=mods` | ✅ |
| Global scripts + scripts.lst override | ✅ — auto-discovery incl. inside .dat mods |
| Worldmap content (61 cities / 20 tiles / 173 maps) | ✅ — Pipboy automap covers all 173 maps |
| `.edg` files (175) | ✅ |
| Data files (ai.txt, party.txt, quests.txt, karmavar.txt, endgame.txt, vault13.gam) | ✅ |
| Hero appearance `set_hero_style`/`set_hero_race` | ✅ — stores the globals RPU reads |
| `get_sfall_global_int` on unset keys | ✅ — returns 0, matching sfall |
| `fs_copy(path, path)` in-place FRM patch (UPU Goris de-robing, walk speed) | ✅ — same-path `r+b` handle; unbuffered writes visible to the art loader on loose-data installs. DAT-resident FRMs still need pre-patched dats (sfall parity). |
| `get_object_data(combat_data, C_ATTACK_*)` (boxing KO check) | ✅ — sfall-matching offsets; `set_object_data` writes adopted to the same field routing |
| Mod loading (`mods_order.txt` + `mods/rpu.dat`) | ✅ |

## HRP `.edg` scroll-blocker support

CE reads `maps/<mapname>.edg` on map load when present (missing file = silent fallback to the
scroll-blocker object system):

- Per-elevation rectangle boundary zones, multiple chained zones supported.
- Loaded zones replace both vanilla scroll blocking and CE hi-res stencil for that map.
- v2 files carry a `SquareRect` square-grid stencil ("angled edges") — also supported.
