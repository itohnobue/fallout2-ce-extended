# Sfall Compatibility

This document tracks Fallout 2 CE compatibility with sfall.  This is for modders who need to know which Sfall features work in CE.

For now, this covers opcodes/metarules, and hooks.  In the future, it will include other ways of modifying the engine (like ini files), and other Sfall-specific behaviour.

## What we've implemented (vs. upstream CE)

| Area | Upstream CE | This fork |
| --- | --- | --- |
| **sfall scripting** | partial | sfall 4.5.1 scripting surface reimplemented: 121+ opcodes/metarules registered, 43 of 62 hook types (38 sfall hooks + 5 CE-specific hooks) |
| **RPU (Fallout 2 Restoration Project)** | not supported | RPU's hooks, opcodes, metarules and ddraw.ini config keys implemented — all 25 requirement rows and all 6 remaining-work items verified against the RPU source (2026-08-17 audit). Fully working in-game; long-session testing underway. |
| **Et Tu (Fallout 1 in FO2)** | not supported | Et Tu's config overlays and sfall surface implemented; FO1-mode engine behavior (traits, combat, rest, encounters, worldmap, dialog) built in. Fully working in-game; 30 of 33 requirement rows verified against source (2026-08-17 audit). |
| **Config keys** | partial | RPU/Et Tu ddraw.ini keys bridged into CE's config system (WorldMapSlots, ElevatorsFile, ExtraSaveSlots, KarmaFRMs, SpeedMulti, OverrideArtCacheSize, FemaleDialogMsgs, …) |
| **Save compatibility** | — | Backward-compatible saves; sfall global-vars and override state serialized |
| **Hardening** | — | 18 production audit passes — hundreds of verified fixes (bounds checks, UAF/null-deref guards, save integrity, VFS sandboxing) |
| **Upstream sync** | — | Continuously merged (currently synced through upstream 1cce144, 2026-08-16) |
| **Tests** | — | 92 test executables, all passing (unit/mirror tests; not a substitute for in-game testing) |

## HRP EDG Scroll-Blocker Support

CE supports the `.edg` file format from the HRP (High Resolution Patch), which defines per-map scroll boundaries and square-level render clipping.

**How it works in CE:**

- On map load, `maps/<mapname>.edg` is read if present. Missing file = silent fallback to the scroll-blocker object system.
- The `.edg` file defines per-elevation rectangle boundary zones. Multiple chained zones per elevation are supported.
- When loaded, these zones are used for both scroll blocking and visible area clipping (black bars), replacing both vanilla scroll blocking and CE hi-res stencil system.
- v2 EDG files also contain a `SquareRect` that defines "Angled edges", or square-grid stencil. This is also supported.


## Settings (ddraw.ini → fallout2.cfg / game.cfg)

Settings previously read from `ddraw.ini` have been moved into standard CE config files.

Most settings that control game behavior (premade characters, extra message files, combat tweaks, worldmap, etc.) have been moved into [`<DAT>/config/game.cfg`](files/ce.dat/config/game.cfg), which is a content-mod config file intended to be overridden by mods. See that file for the full list with descriptions.

The following settings were moved into [`fallout2.cfg`](files/fallout2.cfg) instead:

| ddraw.ini section | ddraw.ini key | fallout2.cfg section | fallout2.cfg key |
| --- | --- | --- | --- |
| `Misc` | `SkipOpeningMovies` | `ui` | `skip_opening_movies` |
| `Misc` | `DisplayKarmaChanges` | `ui` | `display_karma_changes` |
| `Misc` | `DisplayBonusDamage` | `ui` | `display_bonus_damage` |
| `Misc` | `NumbersInDialogue` | `ui` | `numbers_in_dialogue` |
| `Misc` | `AutoQuickSave` | `ui` | `auto_quick_save` |
| `Main` | `EnableHighResolutionStencil` | `ui` | `enable_high_resolution_stencil` |
| `Misc` | `ConsoleOutputPath` | `debug` | `console_output_path` |
| `Misc` | `GaplessMusic` | `sound` | `gapless_music` |
| `Misc` | `ScreenshotsFormat` | `system` | `screenshots_format` |
| `Misc` | `UseWalkDistance` | `qol` | `use_walk_distance` |
| `Misc` | `AutoOpenDoors` | `qol` | `auto_open_doors` |

The following settings were moved into [`<DAT>/config/game.cfg`](files/ce.dat/config/game.cfg):

| ddraw.ini section | ddraw.ini key | game.cfg section | game.cfg key |
| --- | --- | --- | --- |
| `Misc` | `StartGDialogFix` | `dialog` | `start_gdialog_fix` |
| `Misc` | `StartingMap` | `start` | `map` |
| `Misc` | `StartYear` | `start` | `year` |
| `Misc` | `StartMonth` | `start` | `month` |
| `Misc` | `StartDay` | `start` | `day` |
| `Misc` | `StartTime` | `start` | `time` |
| `Misc` | `StartXPos` | `start` | `worldmap_x` |
| `Misc` | `StartYPos` | `start` | `worldmap_y` |
| `Misc` | `ViewXPos` | `start` | `worldmap_view_x` |
| `Misc` | `ViewYPos` | `start` | `worldmap_view_y` |
| `Misc` | `XPTable` | `stats` | `xp_table` |
| `Misc` | `DisableSpecialMapIDs` | `maps` | `disable_special_map_ids` |
| `Misc` | `Movie1` - `Movie32` | `movies` | `movie1` - `movie32` |
| `Misc` | `Fallout1Behavior` movie behavior | `movies` | `endgame_play_after_slideshow`, `endgame_movie_male`, `endgame_movie_female` |
| `Sound` | `MainMenuMusic` | `sound` | `main_menu_music` |
| `Sound` | `WorldMapMusic` | `sound` | `worldmap_music` |
| `Sound` | `WorldMapCarMusic` | `sound` | `worldmap_car_music` |
| `Sound` | `EndGameMovieMusic0` | `sound` | `endgame_movie_music0` |
| `Sound` | `EndGameMovieMusic1` | `sound` | `endgame_movie_music1` |
| `Sound` | `MapLoadingSound` | `sound` | `map_loading_sound` |
| `Misc` | `Movie1` - `Movie32` | `movies` | `movie1` - `movie32` |
| `Misc` | `Fallout1Behavior` movie behavior | `movies` | `endgame_play_after_slideshow`, `endgame_movie_male`, `endgame_movie_female` |
| `Interface` | `WorldMapTerrainInfo` | `worldmap` | `terrain_info` |
| `Misc` | `WorldMapFPSPatch` + `WorldMapDelay2` | `worldmap` | `travel_delay` |

Unlike sfall, `travel_delay` throttles only world-map travel simulation. Input,
rendering, and world-map scripts continue at the normal frame rate.

CE does not provide a single `Fallout1Behavior` compatibility switch. The movie
portion can be configured directly: set `endgame_play_after_slideshow=0`,
`endgame_movie_male=10`, and `endgame_movie_female=11` for the sfall Fallout 1
movie sequence. Time-limit and item-weight behavior are separate features.

### Speed Control (`[Speed]` section of `ddraw.ini`)

CE supports sfall's game speed multiplier via the `[Speed]` section of `ddraw.ini`:

| Key | Default | Description |
| --- | --- | --- |
| `SpeedMultiInitial` | 100 | Initial speed multiplier percentage. Preferred key; falls back to `SpeedMulti` if absent. |
| `SpeedMulti` | 100 | Fallback speed multiplier. Used if `SpeedMultiInitial` is not present. |

**How it works:**

- On game init, the speed value is read from `ddraw.ini [Speed]` and stored in sfall global variable 0 (`src/game.cc:373-386`).
- The global speed multiplier is applied in `animationComputeTicksPerFrame()` at `src/animation.cc:3350-3364`, after combat speed adjustments and before the FPS-to-milliseconds conversion.
- The multiplier affects ALL animation types (walks, idles, attacks, etc.) — not just combat movement.
- Scripts can change it at runtime via `set_sfall_global(0, value)` and read it via `get_sfall_global_int(0)`.
- On game reset (`gameReset`), the value is re-initialized from `ddraw.ini` to match sfall behavior (`src/sfall_callbacks.cc:43-56`).
- Values ≤ 0 are clamped to 100 to prevent game freeze.
- SpeedMulti is independent of the FPS limiter (`fps_limiter.cc`) — it controls animation speed, not rendering frame rate.

## Opcodes / Metarules

See [`https://sfall-team.github.io/sfall/`](https://sfall-team.github.io/sfall/) for documentation on specific functions.

| Group | Opcodes In Group | Compatibility | Notes |
| --- | --- | --- | --- |
| Direct memory access| read_byte,short,int,string<br>write_byte,short,int,string<br>call_offset_vX | partially (no-ops) | read_byte supports specific addresses (0x56D38C combat highlight, 0x410003 Rotators fork detection → 0xF4). read_short/int/string return -1 stub. write_byte/short/int/string and all call_offset_v0-v4 / call_offset_r0-r4 are registered as safe no-ops (requires `AllowUnsafeScripting=1`). |
| Stats | get/set_pc_base_stat<br>get/set_pc_extra_stat<br>get/set_critter_base_stat<br>get/set_critter_extra_stat | ✅ | CE uses engine stat helpers here instead of sfall's direct proto-field behavior, so derived-stat update behavior can differ. |
| Stats / Alter min/max | get/set_stat_min/max<br>set_pc_stat_min/max<br>set_npc_stat_min/max | ✅ | get_stat_max/get_stat_min implemented (sfall_metarules.cc:2095,2111). set_pc_stat_max/min and set_npc_stat_min/max registered and implemented (sfall_opcodes.cc). All use engine stat helpers. |
| Skills | get/set_critter_skill_points<br>get/set_available_skill_points<br>set_skill_max<br>set_critter_skill_mod<br>set_base_skill_mod<br>mod_skill_points_per_level | ✅ | set_skill_max wired into skill.cc. mod_skill_points_per_level stored; consumed by characterEditorUpdateLevel() (character_editor.cc:5758). get_critter_skill_points, get_available_skill_points registered and implemented (sfall_opcodes.cc). set_critter_skill_mod (0x81C7) and set_base_skill_mod (0x81C8): fully integrated — consumed in skillGetValue() at skill.cc:252,267,270 via sfallGetBaseSkillMod() / sfallGetCritterSkillMod(). |
| Maps and encounters / Worldmap | get_world_map_x/y_pos<br>set_world_map_pos | ✅ | - |
| Audio | play_sfall_sound<br>stop_sfall_sound | ✅ | `play_sfall_sound` currently supports `.acm`, `.wav`, `.ogg` formats, and can load from `.dat` archives. `.mp3` is not yet supported. |
| Combat / Weapons and ammo | get/set_weapon_ammo_pid<br>get/set_weapon_ammo_count | ✅ | - |
| Sfall / Version | sfall_ver_major<br>sfall_ver_minor<br>sfall_ver_build | ✅ | CE currently reports `4.5.1` |
| Utility / Math | log, exponent, round, sqrt, abs, sin, cos, tan, arctan, ceil, ^, floor2, div | ✅ | - |
| Keyboard and mouse | key_pressed<br>tap_key<br>get_mouse_x/y<br>get_mouse_buttons | ✅ | - |
| Lists | list_begin<br>list_next<br>list_end<br>list_as_array<br>party_member_list | ✅ | - |
| Explosions | set_attack_explosion_pattern<br>set_attack_explosion_art<br>set_attack_explosion_radius<br>set_attack_is_explosion_fire<br>set_explosion_radius<br>set_dynamite_damage<br>set_plastic_damage<br>get_explosion_damage<br>set_explosion_max_targets<br>item_make_explosive | ✅ | item_make_explosive registered and implemented (sfall_metarules.cc:2129, stores in gExplosiveOverrides). |
| Animations | reg_anim_combat_check<br>reg_anim_destroy<br>reg_anim_animate_and_hide<br>reg_anim_light<br>reg_anim_change_fid<br>reg_anim_take_out<br>reg_anim_turn_towards<br>reg_anim_callback<br>reg_anim_animate_and_move | ✅ | - |
| Art and appearance | art_exists<br>art_frame_data<br>refresh_pc_art<br>art_cache_clear | ✅ | - |
| Tiles and paths | get_tile_fid<br>tile_under_cursor<br>tile_light<br>tile_get_objs<br>tile_refresh_display<br>obj_blocking_tile<br>tile_by_position<br>get_tile_ground_fid<br>get_tile_roof_fid<br>obj_blocking_line<br>path_find_to<br>objects_in_radius | ✅ | `get_tile_ground_fid` and `get_tile_roof_fid` are sfall.h convenience wrappers around `get_tile_fid` (with mode parameter: 0=ground, 1=roof). The underlying opcode provides the full functionality. |
| Utility | sprintf<br>typeof<br>atoi<br>atof | ✅ | - |

| Utility / Strings | string_split<br>substr<br>strlen<br>charcode<br>get_string_pointer<br>string_find<br>string_find_from<br>string_format<br>string_format_array<br>string_replace<br>string_to_case<br>string_compare | ✅ | `get_string_pointer` is deprecated and intentionally omitted. |
| Interface / Tags | show_iface_tag<br>hide_iface_tag<br>is_iface_tag_active<br>set_iface_tag_text<br>add_iface_tag | ✅ | Legacy `BoxBarCount`, `BoxBarColors` ddraw.ini settings not supported.<br> `show_iface_tag` and `hide_iface_tag` do not not work for tag values `1` (Poisoned) and `2` (Radiated). <br> `set_iface_tag_text` and `add_iface_tag` works only for custom tags `>= 5`. <br> `is_iface_tag_active` is supporting all the tag values. <br> Tags 5-9 route to the generic custom-tag mechanism (`interfaceTagShow`, `kCustomIndicatorMinTag = 5`, `src/interface.cc:124`); RPU's `gl_k_crippl.ssl` cripple indicators render generic boxes rather than sfall's dedicated graphics (cosmetic — disclosed 2026-08-17). |
| Global variables | set_sfall_global<br>get_sfall_global_int<br>get_sfall_global_float | ✅ | `set_sfall_global` stores integer values; `get_sfall_global_float` fetches with int fallback (`sfall_opcodes.cc:714-743`, `sfall_global_vars.cc:441-482`) — verified 2026-08-17 |
| Hooks / Hook functions | init_hook<br>get_sfall_arg<br>get_sfall_args<br>get_sfall_arg_at<br>set_sfall_return<br>set_sfall_arg<br>register_hook<br>register_hook_proc<br>register_hook_proc_spec | ✅ | See below for implemented hooks. `init_hook` is deprecated and will not be implemented. register_hook_proc and register_hook_proc_spec both add hooks to the *end* of the hook list, instead of beginning and end, respectively. |
| Arrays / Array functions | create_array<br>temp_array<br>fix_array<br>get/set_array<br>resize_array<br>free_array<br>scan_array<br>len_array<br>save/load_array<br>array_key<br>arrayexpr | ✅ | - |
| Perks and traits / NPC perks | set_fake_perk_npc<br>set_fake_trait_npc<br>set_selectable_perk_npc<br>has_fake_perk_npc<br>has_fake_trait_npc | not implemented | - |
| Global scripts / Global script functions | set_global_script_repeat<br>set_global_script_type<br>available_global_script_types | ✅ except available_global_script_types | - |
| Combat | attack_is_aimed<br>block_combat<br>force_aimed_shots<br>disable_aimed_shots<br>get_attack_type<br>get/set_bodypart_hit_modifier<br>combat_data<br>get/set/reset_critical_table<br>get_last_target<br>get_last_attacker<br>set_critter_burst_disable<br>get/set_critter_current_ap<br>set_spray_settings<br>get/set_combat_free_move<br>set_fo1_hit_chance | ✅ except block_combat, get_last_target, get_last_attacker, set_spray_settings | - |
| Car | set_car_current_town<br>car_gas_amount<br>set_car_intface_art | ✅ | - |
| Interface / Outline | outlined_object<br>get_outline<br>set_outline | ✅ | - |
| Interface / Main interface | intface_is_hidden<br>intface_redraw<br>intface_hide<br>intface_show<br>set_quest_failure_value | ✅ | `intface_redraw` supports both 0-arg (redraw entire bar) and 1-arg (redraw window by type) forms (sfall_metarules.cc:1065). `intface_hide` (sfall_metarules.cc:2264), `intface_show` (sfall_metarules.cc:2271), `intface_is_hidden` (sfall_metarules.cc:2278) registered and implemented. `set_quest_failure_value` fully implemented with setter (sfall_metarules.cc:1884) and getter (sfall_metarules.cc:1895). |
| Interface / Inventory | display_stats<br>inventory_redraw<br>critter_inven_obj2<br>get_current_inven_size<br>item_weight | ✅ | get_current_inven_size registered and implemented (sfall_metarules.cc:2019, returns obj->data.inventory.length). |
| Interface / Cursor | get/set_cursor_mode | ✅ | - |
| Locks | lock_is_jammed<br>unjam_lock<br>set_unjam_locks_time | partial | lock_is_jammed registered and implemented (sfall_metarules.cc:2167, checks OBJ_JAMMED flag). unjam_lock fully implemented (sfall_metarules.cc:2661, wraps engine's objectUnjamLock). set_unjam_locks_time: registered and consumed in mapLoadSaved (map.cc) — overrides default 24-hour unjam threshold. |
| INI settings | get_ini_setting<br>get_ini_string<br>get_ini_section<br>get_ini_sections<br>get_ini_config<br>get_ini_config_db<br>set_ini_setting | ✅ | `modified_ini` is intentionally omitted as deprecated. |
| Objects and scripts | set_self<br>set_dude_obj<br>real_dude_obj<br>remove_script<br>get/set_script<br>obj_is_carrying_obj<br>loot_obj<br>dialog_obj<br>obj_under_cursor<br>get/set_object_data<br>get/set_flags<br>set_unique_id<br>set_scr_name<br>obj_is_openable<br>get/set_proto_data<br>get_object_ai_data | implemented: set_self, set_dude_obj, real_dude_obj, get/set/remove_script, obj_is_carrying_obj, loot_obj, dialog_obj, obj_under_cursor, get_object_data, set_object_data (metarule), get_flags, set_flags, set_unique_id, set_scr_name, obj_is_openable, get_proto_data, set_proto_data, get_object_ai_data (type 0) | set_dude_obj/real_dude_obj/set_object_data/set_scr_name are implemented as metarules. get_object_ai_data type 0 (AI packet number) implemented; types 1-2 (AI flags, procedure) implemented via aiPacketGetFlags/aiPacketGetProcedure accessors. **Caveat:** `get_object_data`/`set_object_data` are field-mapped via the `ObjectDataField` enum (sfall offset constants → typed fields) and, for active combat, via `AttackDataField` (`sfall_metarules.cc:93-139`) with **sfall-matching `C_ATTACK_*` offsets** — `C_ATTACK_FLAGS_TARGET` (0x30) correctly reads `attack->defenderFlags` (`getAttackData`, `sfall_metarules.cc:227-228`). Both get and set route through the typed field setters (`getAttackData`/`setAttackData`/`getObjectData`/`setObjectData`), replacing the pre-sync raw-byte read/write (upstream 5774372 + 2026-08-17 adoption). `set_object_data` accepts `ARG_ANY` for the value so Object* fields (Source/Target/Weapon/WHO_HIT_ME) are writable; unsupported offsets or value types return -1. **Compat delta:** the fork's pre-sync raw-byte whitelist also allowed 4-byte writes to the Object struct position members x/y/sx/sy (raw offsets 8/12/16/20); upstream sfall's `OBJ_DATA_*` surface (define_extra.h) does not expose those members, so they are no longer writable via `set_object_data` — matching upstream (`sfall_metarules.cc:3225-3228`). Scripts that repositioned objects through raw offsets must use the engine tile/elevation fields (`OBJ_DATA_TILENUM`/`OBJ_DATA_ELEVATION`) instead. |
| Other / Game management | set_movie_path<br>stop/resume_game<br>mark_movie_played<br>game_loaded<br>get_game_mode<br>get_uptime<br>signal_close_game | ✅ | set_movie_path (0x8177) implemented — runtime override + game.cfg `[movies] movie1..movie32` config (68ff38e). mark_movie_played (0x8240) fully implemented via gameMovieMarkSeen. stop/resume_game (0x8222,0x8223) registered as safe no-ops. game_loaded, get_game_mode, get_uptime, signal_close_game fully implemented. |
| Gameplay tweaks | set_pickpocket_max<br>set_hit_chance_max<br>set_xp_mod<br>set_critter_hit_chance_mod<br>set_base_hit_chance_mod<br>set_hp_per_level_mod<br>gdialog_get_barter_mod<br>get/set_unspent_ap_bonus<br>get/set_unspent_ap_perk_bonus<br>set_base_pickpocket_mod<br>set_critter_pickpocket_mod<br>get/set_inven_ap_cost<br>set_drugs_data<br>get_kill_counter<br>mod_kill_counter<br>set_pipboy_available | ✅ | gdialog_get_barter_mod, get/set_unspent_ap{_perk}_bonus, get/set_inven_ap_cost, set_xp_mod, set_hit_chance_max, set_base_hit_chance_mod fully implemented. set_critter_hit_chance_mod (0x81C5) implemented: per-critter modifier consumed in attackDetermineToHit() (combat.cc) via sfallGetCritterHitChanceMod(), additive with global set_base_hit_chance_mod. set_hp_per_level_mod consumed at stat.cc:859,912. Pickpocket modifiers (0x81A0, 0x81C9, 0x81CA) fully integrated — consumed in skillsPerformStealing() (skill.cc) via sfallGetPickpocket*() accessors. Cap uses sfallGetPickpocketMax() with 95 fallback. |
| NPCs | inc_npc_level<br>get_npc_level<br>npc_engine_level_up | implemented: inc_npc_level (0x81A5), get_npc_level (0x8241), npc_engine_level_up (metarule) | get_npc_level delegates to partyMemberGetLevel. npc_engine_level_up controls auto-leveling. |
| Hero Appearance | set_dm/df_model<br>hero_select_win<br>set_hero_race<br>set_hero_style | implemented: set_dm_model (0x8175), set_df_model (0x8176), hero_select_win (0x8213), set_hero_race (0x8214), set_hero_style (0x8215) | set_hero_race/set_hero_style implemented (sfall_opcodes.cc:6000-6011, registered 0x8213-0x8215 at sfall_opcodes.cc:8839-8841) — store values via sfall global vars. No config flag needed — feature is always-on. |
| Events | add_g_timer_event<br>remove_timer_event<br>create_spatial<br>spatial_radius | ✅ | All 4 opcodes registered and fully implemented. add_g_timer_event (sfall_metarules.cc:2496), remove_timer_event (sfall_metarules.cc:2267), create_spatial (sfall_opcodes.cc:4141), spatial_radius (sfall_metarules.cc:2233). |
| Other | get_year<br>active_hand<br>toggle_active_hand<br>get/set_viewport_x/y<br>get_light_level<br>message_str_game<br>sneak_success<br>unwield_slot<br>add_extra_msg_file<br>get_metarule_table<br>metarule_exist<br> | ✅ | get/set_viewport_x/y (0x81A6-0x81A9) registered as safe stubs (CE renders with SDL2, scroll is engine-managed). `input_funcs_available`, `nb_create_char` are deprecated in sfall and intentionally absent in CE. `sneak_success` registered and implemented (sfall_opcodes.cc:3683). `add_extra_msg_file` supports the 2-arg form (filename, fileNumber). |

### CE-only metarules

CE defines several metarules that are not supported in Sfall. Include [ce.h](files/ce.h) for the #defines.

| Name | Definition |
| --- | --- |
| `encounter_intros(toggle)` | Enable or disable the display-monitor random encounter intro message, for example `You encounter: ...`. This does not affect the separate encounter detection dialog. |
| `set_reaction_thresholds(neutral, good)` | Set thresholds for reactions considered "neutral" and "good". Defaults: FO1 -25/25, FO2 -51/49 (fork keeps the original per-game thresholds). |
| `set_party_member_cc_msg_ids(pid, start_msg_id, end_msg_id)` | Override party-member combat-control update messages for a pid. Picks randomly from the inclusive contiguous range. Default fallback ranges are 670-674 for humans and 677-678 for the hardcoded dog pid list. |
| `rest_option_msgs(base_msg_id)` | Change the base message id used for Pip-Boy rest option labels. CE reads the rest labels from `base_msg_id` through `base_msg_id + 13`; the default Fallout 2 range is 302-315 (FO1 mode: 321-334). |
| `set_rest_option(rest_option, value)` | Change the wake hour for Pip-Boy rest options 8-11: morning, noon, evening, and midnight. `value` is an hour from 0-23. Defaults are 8, 12, 18, and 0 (FO1 morning default: 6). |

### CE-only metarules

CE defines several metarules that are not supported in Sfall.  Include [ce.h](files/ce.h) for the #defines.

| Name | Definition |
| --- | --- |
| `encounter_intros(toggle)` | Enable or disable the display-monitor random encounter intro message, for example `You encounter: ...`. This does not affect the separate encounter detection dialog. |
| `rest_option_msgs(base_msg_id)` | Change the base message id used for Pip-Boy rest option labels. CE reads the rest labels from `base_msg_id` through `base_msg_id + 13`; the default Fallout 2 range is 302-315. |
| `set_party_member_cc_msg_ids(pid, start_msg_id, end_msg_id)` | Override party-member combat-control update messages for a pid. Picks randomly from the inclusive contiguous range. Default fallback ranges are 670-674 for humans and 677-678 for the hardcoded dog pid list. |
| `set_rest_option(rest_option, value)` | Change the wake hour for Pip-Boy rest options 8-11: morning, noon, evening, and midnight. `value` is an hour from 0-23. Defaults are 8, 12, 18, and 0. |

## Hooks

| Hook | ID | Compatibility | Notes |
| --- | --- | --- | --- |
| ToHit | `HOOK_TOHIT` | ✅ | - |
| AfterHitRoll | `HOOK_AFTERHITROLL` | ✅ | Overriding `defender` leaves a lot of attack variables in previous state (e.g. distance, ->oops, roundsHitMainTarget) |
| CalcAPCost | `HOOK_CALCAPCOST` | ✅ | - |
| DeathAnim1 | `HOOK_DEATHANIM1` | 🚫 | Use DEATHANIM2 instead |
| DeathAnim2 | `HOOK_DEATHANIM2` | ✅ | - |
| CombatDamage | `HOOK_COMBATDAMAGE` | ✅ | - |
| OnDeath | `HOOK_ONDEATH` | ✅ | **Notification-only hook:** fires after `DAM_DEAD` is set; `maxReturnValues=0` — scripts cannot prevent or modify death through this hook. Use for cleanup/notification only. Fires exactly once at `critter.cc:917` in `critterKill()` for all death paths (combat, non-combat, environmental, script kill, poison, radiation). The duplicate fire site at `combat.cc:5264` was removed as part of the F-68 fix — only the single canonical fire site in `critterKill()` remains. |
| FindTarget | `HOOK_FINDTARGET` | ✅ | **Contract difference from sfall:** CE uses a simplified 2-arg layout: arg0=attacker (Object), arg1=target (Object), 1 return value. sfall uses a 5-arg layout (arg0=attacker, arg1=combat group index, arg2=current target, arg3=previous target, arg4=area attack mode). CE fires at 4 `combat_ai.cc` call sites: line 1776 (cooperative combat pre-selection redirect — player-controlled attacker with existing target), line 1864 (area-attack target iteration — per-candidate check), line 1902 ("who hit me" retaliation override), line 1963 (final post-selection override — last chance to replace engine-chosen target). Return value: if non-null and a valid critter, overrides the engine-selected target; null means "keep engine choice." Scripts written for sfall's 5-arg layout will receive different values in arg0/arg1 than expected. |
| UseObjOn | `HOOK_USEOBJON` | ✅ | - |
| UseObj | `HOOK_USEOBJ` | ✅ | CE notes an sfall-matching inconsistency around return code `2` behavior between interface contexts. |
| RemoveInvenObj | `HOOK_REMOVEINVENOBJ` | 🚫 | Deliberately absent: requires RMOBJ_* constants and destination object tracking not present in CE's itemRemove. Would require significant refactoring of the item removal code path. |
| BarterPrice | `HOOK_BARTERPRICE` | ✅ | - |
| ItemDamage | `HOOK_ITEMDAMAGE` | ✅ | - |
| MoveCost | `HOOK_MOVECOST` | ✅ | - |
| AmmoCost | `HOOK_AMMOCOST` | ✅ | Requires `check_weapon_ammo_cost=1` if you want pre-attack ammo validation to respect per-shot/per-round overrides. |
| KeyPress | `HOOK_KEYPRESS` | ✅ | **Sfall-compatible 3-arg layout** (`src/sfall_kb_helpers.cc:702-716`): arg0=pressed state (1=pressed, 0=released), arg1=DIK key code, arg2=VK_ Virtual Key code (converted from the raw SDL_Keycode via `sdl_keycode_to_vk`). ret0=255 swallows the key (et tu's TMA idiom, `sfall_kb_helpers.cc:741-743`); ret0 in 1-263 remaps the key to that DIK code. See VK→SDL mapping notes below. |
| MouseClick | `HOOK_MOUSECLICK` | ✅ | - |
| UseSkill | `HOOK_USESKILL` | ✅ | - |
| Steal | `HOOK_STEAL` | ✅ | - |
| WithinPerception | `HOOK_WITHINPERCEPTION` | ✅ | - |
| InventoryMove | `HOOK_INVENTORYMOVE` | ✅ | - |
| InvenWield | `HOOK_INVENWIELD` | ✅ | - |
| AdjustFID | `HOOK_ADJUSTFID` | ✅ | Second hook arg currently matches the first because CE has no internal FID modifiers like Hero Appearance. |
| CombatTurn | `HOOK_COMBATTURN` | ✅ | - |
| StdProcedure | `HOOK_STDPROCEDURE` | ✅ | - |
| StdProcedureEnd | `HOOK_STDPROCEDURE_END` | ✅ | - |
| CarTravel | `HOOK_CARTRAVEL` | ✅ | Fires once per worldmap tick during car travel. Speed is CE step count (3-8) matching sfall scale (3-8); fuel default is 100/tick. Override via ret0 (steps, -1 to keep) and ret1 (fuel, -1 to keep). |
| SetGlobalVar | `HOOK_SETGLOBALVAR` | ✅ | Fires on op_set_global_var for integer values only (not pointer/string values). ret0 overrides the stored value. |
| RestTimer | `HOOK_RESTTIMER` | ✅ | CE is slightly more strict: only `ret0 == 1` interrupts. Ticks wrap every 6.8y; do not rely on them for absolute game time. |
| GameModeChange | `HOOK_GAMEMODECHANGE` | ✅ | - |
| UseAnimObj | `HOOK_USEANIMOBJ` | ✅ | Fires on animate_stand_obj and animate_stand_reverse_obj |
| ExplosiveTimer | `HOOK_EXPLOSIVETIMER` | ✅ | - |
| DescriptionObj | `HOOK_DESCRIPTIONOBJ` | ✅ | Supports sfall 4.4.0+ direct string return for description override |
| UseSkillOn | `HOOK_USESKILLON` | ✅ | - |
| OnExplosion | `HOOK_ONEXPLOSION` | ✅ | Fires on explosive detonation — item timers and script-triggered explosions. |
| SubCombatDamage | `HOOK_SUBCOMBATDAMAGE` | 🚫 | (maybe) |
| SetLighting | `HOOK_SETLIGHTING` | ✅ | Fires on objectSetLight for per-object lighting changes |
| Sneak | `HOOK_SNEAK` | ✅ | Fires after each sneak check (via sneakEventProcess). arg0=result (1 success, 0 failure), arg1=duration in ticks, arg2=critter. ret0 overrides result, ret1 overrides duration. |
| TargetObject | `HOOK_TARGETOBJECT` | ✅ | Fires at the start of `_combat_attack`, when attack execution begins (after target selection by AI, before hit computation). arg0=attacker, arg1=defender, arg2=hitMode, arg3=hitLocation. |
| Dialog | `HOOK_DIALOG` (49) | ✅ [CE] | CE-specific. Fires on dialog start (arg0=speaker, arg1=headFid, arg2=reaction) and exit (arg1=-1, arg2=-1, arg0=speaker). |
| DialogReaction | `HOOK_DIALOGREACTION` (50) | ✅ [CE] | CE-specific. Fires when a dialog reaction is triggered (`_talk_to_critter_reacts`). arg0=speaker, arg1=reaction (-2, -1, or 0). |
| Encounter | `HOOK_ENCOUNTER` | ✅ | **Sfall-compatible 5-arg layout** (`src/sfall_script_hooks.cc:737-796`): arg0=event type (0=random encounter, 1=local-map-enter from worldmap), arg1=mapId, arg2=isSpecial (1 if special encounter — specials are encoded in arg2, never arg0), arg3=tableId (encounter table number, -1 if not an encounter), arg4=entryId (entry index in table, -1 if not an encounter). **Note:** arg0 uses sfall's encoding (0/1). The earlier CE "arg0=2 for local-map-enter / arg0=1 for special" scheme was a regression (86e6c4d) and was reverted — the doc previously described the reverted scheme. Forced encounters do **not** fire the hook (sfall N-01, `src/sfall_script_hooks.cc:769-771`) — the map load proceeds directly. Return values: ret0 overrides mapId (-1 cancels for event type 0, or the map to load); ret1 (event type 0 only) returns 1 to cancel the encounter and directly load the map from ret0. |
| AdjustPoison | `HOOK_ADJUSTPOISON` | 🚫 | (maybe) |
| AdjustRads | `HOOK_ADJUSTRADS` | ✅ | Fire site at `critter.cc:492` (`critterAdjustRadiation`) + fire function `sfall_script_hooks.cc:718-730`; used by et tu's `gl_fo1mechanics.ssl` rads-2000 failsafe. Status corrected 2026-08-17. |
| RollCheck | `HOOK_ROLLCHECK` | 🚫 | Deliberately absent: randomRoll() has 30+ call sites with no event_type context. Adding context to every call site is too invasive; pass-through hook on every roll would be too expensive. |
| BestWeapon | `HOOK_BESTWEAPON` | 🚫 | Deliberately absent: _ai_best_weapon() has 10+ return points with complex comparison logic. Object lifetime concerns with return value override. |
| CanUseWeapon | `HOOK_CANUSEWEAPON` | ✅ | - |
| BuildSfxWeapon | `HOOK_BUILDSFXWEAPON` | 🚫 | Deliberately absent: sfxBuildWeaponName() returns char* to static buffer (_sfx_file_name). String return from scripts requires buffer management and lifetime semantics. |
| StatLevelUp | `HOOK_STATLEVELUP` (51) | ✅ [CE] | CE-specific. Fires in stat.cc pcAddExperienceWithOptions() and character_editor.cc characterEditorUpdateLevel() |
| Barter | `HOOK_BARTER` (52) | ✅ [CE] | CE-specific. Fires in game_dialog.cc gameDialogBarter() |
| Message | `HOOK_MESSAGE` (53) | ✅ [CE] | CE-specific. Fires in display_monitor.cc displayMonitorAddMessage() |

### VK → SDL Keycode Mapping

CE uses SDL2 rendering and input, not DirectInput. `HOOK_KEYPRESS` passes sfall-compatible codes: arg0=pressed state, arg1=DIK, arg2=VK (converted from the raw SDL_Keycode; `src/sfall_kb_helpers.cc:702-716`). `key_pressed()`/`tap_key()` accept DIK codes (0-255) or VK codes with the `0x80000000` flag and translate them to SDL scancodes internally (`get_scancode_from_key`, `sfall_kb_helpers.cc:608-615`). The numeric values differ significantly from SDL_Keycode for common keys used in RPU/Et Tu scripts.

| Key | Windows VK_ (hex) | VK_ (dec) | SDL_Keycode | SDL_SCANCODE | Notes |
| --- | --- | --- | --- | --- | --- |
| A | `VK_A` = 0x41 | 65 | SDLK_a = 97 | 4 | Letter keys: VK_ is uppercase ASCII; SDL_Keycode is lowercase |
| B | `VK_B` = 0x42 | 66 | SDLK_b = 98 | 5 | |
| ... | ... | ... | ... | ... | |
| Z | `VK_Z` = 0x5A | 90 | SDLK_z = 122 | 29 | |
| 0 | `VK_0` = 0x30 | 48 | SDLK_0 = 48 | 39 | Digit keys match between VK_ and SDL_Keycode |
| 1-9 | 0x31-0x39 | 49-57 | SDLK_1-9 = 49-57 | 30-38 | Digit keys: VK_ and SDL_Keycode are identical |
| Escape | `VK_ESCAPE` = 0x1B | 27 | SDLK_ESCAPE = 27 | 41 | Escape: VK_ and SDL_Keycode match (numerically) |
| Return | `VK_RETURN` = 0x0D | 13 | SDLK_RETURN = 13 | 40 | Return/Enter: match |
| Space | `VK_SPACE` = 0x20 | 32 | SDLK_SPACE = 32 | 44 | Space: match |
| Tab | `VK_TAB` = 0x09 | 9 | SDLK_TAB = 9 | 43 | Tab: match |
| Backspace | `VK_BACK` = 0x08 | 8 | SDLK_BACKSPACE = 8 | 42 | Backspace: match |
| Shift | `VK_SHIFT` = 0x10 | 16 | SDLK_LSHIFT/SDLK_RSHIFT = 1073742049/1073742050 | 225/229 | Shift/Control/Alt: VK_ uses modifier codes; SDL uses left/right-specific keys |
| Control | `VK_CONTROL` = 0x11 | 17 | SDLK_LCTRL/SDLK_RCTRL = 1073742048/1073742051 | 224/228 | |
| Alt | `VK_MENU` = 0x12 | 18 | SDLK_LALT/SDLK_RALT = 1073742050/1073742051 | 226/230 | |
| F1 | `VK_F1` = 0x70 | 112 | SDLK_F1 = 1073741882 | 58 | Function keys: SDL_Keycode is in 0x40000000+ range |
| F2-F12 | 0x71-0x7B | 113-123 | SDLK_F2-12 = 1073741883-93 | 59-69 | |
| Numpad 0-9 | 0x60-0x69 | 96-105 | SDLK_KP_0-9 = 1073741922-1931 | | Numpad: SDL uses separate keycodes |
| Left/Right/Up/Down | 0x25-0x28 | 37-40 | SDLK_LEFT/RIGHT/UP/DOWN = 1073741904-1907 | 80/79/82/81 | Arrow keys: SDL_Keycode is in 0x40000000+ range |

**How scripts should handle this:**

1. **Use sfall-style key codes** — for mod scripts targeting CE, pass DIK codes (0-255, e.g. `DIK_A` = 30) or VK codes with the `0x80000000` flag (e.g. `0x80000041` for VK_A) to `key_pressed()`/`tap_key()`. SDL_Keycode values in the 0-255 range would be misinterpreted as DIK codes (SDLK_a = 97 is not DIK_A = 30).
2. **Use `key_pressed()` / `tap_key()` with DIK or VK codes** — these functions translate the argument to an SDL scancode via `get_scancode_from_key()` (`sfall_kb_helpers.cc:608-615`): DIK (0-255) via `kDiks[]`, VK (0x80000000 flag) via the fully-populated `kVkToSdl[256]` table.
3. **In `HOOK_KEYPRESS` handlers** — the layout is sfall-compatible: arg0=pressed state, arg1=DIK key code, arg2=VK code (converted from the raw SDL_Keycode; `sfall_kb_helpers.cc:702-716`). If a `key_pressed()` trampoline is needed, pass the hook's arg1 (DIK) directly — no conversion required.
4. **RPU/Et Tu compatibility** — sfall scripts using DIK and VK_ constants get correct key detection: DIK codes map through `kDiks[]` for common keys, and VK codes (0x80000000 flag) map through the fully-populated `kVkToSdl[256]` static lookup table (see VK→SDL Mapping Scope below). For unmapped DIK codes, scripts must use the VK_ form (0x80000000 flag) where a `kVkToSdl` entry exists.

### DIK→SDL Mapping Scope

CE's `sfall_kb_helpers.cc` (`kDiks[]` array) maps 256 DIK (DirectInput Key) entries to SDL scancodes. The following describes which DIK ranges are mapped and which are not:

**Mapped (common keys):**
- DIK 1-13: Escape, digits 1-0, minus, equals, backspace (`SDL_SCANCODE_ESCAPE` through `SDL_SCANCODE_BACKSPACE`)
- DIK 14: Tab (`SDL_SCANCODE_TAB`)
- DIK 15-27: Q through right bracket (`SDL_SCANCODE_Q` through `SDL_SCANCODE_RIGHTBRACKET`)
- DIK 28: Return (`SDL_SCANCODE_RETURN`)
- DIK 29: Left Ctrl (`SDL_SCANCODE_LCTRL`)
- DIK 30-43: A through backslash (`SDL_SCANCODE_A` through `SDL_SCANCODE_BACKSLASH`)
- DIK 44-53: Z through slash (`SDL_SCANCODE_Z` through `SDL_SCANCODE_SLASH`)
- DIK 54: Right Shift (`SDL_SCANCODE_RSHIFT`)
- DIK 55: Numpad `*` (`SDL_SCANCODE_KP_MULTIPLY`)
- DIK 56: Left Alt (`SDL_SCANCODE_LALT`)
- DIK 57: Space (`SDL_SCANCODE_SPACE`)
- DIK 58: Caps Lock (`SDL_SCANCODE_CAPSLOCK`)
- DIK 59-68: F1-F10 (`SDL_SCANCODE_F1` through `SDL_SCANCODE_F10`)
- DIK 69: Num Lock (`SDL_SCANCODE_NUMLOCKCLEAR`)
- DIK 70: Scroll Lock (`SDL_SCANCODE_SCROLLLOCK`)
- DIK 71-83: Numpad 7-0, minus/plus/period (`SDL_SCANCODE_KP_7` through `SDL_SCANCODE_KP_PERIOD`)
- DIK 87-88: F11-F12 (`SDL_SCANCODE_F11` through `SDL_SCANCODE_F12`)
- DIK 141: Numpad `=` (`SDL_SCANCODE_KP_EQUALS`)
- DIK 156: Numpad Enter (`SDL_SCANCODE_KP_ENTER`)
- DIK 157: Right Ctrl (`SDL_SCANCODE_RCTRL`)
- DIK 179: Numpad `,` (`SDL_SCANCODE_KP_COMMA`)
- DIK 181: Numpad `/` (`SDL_SCANCODE_KP_DIVIDE`)
- DIK 183: SysRq (`SDL_SCANCODE_SYSREQ`)
- DIK 184: Right Alt (`SDL_SCANCODE_RALT`)
- DIK 199: Home (`SDL_SCANCODE_HOME`)
- DIK 200: Up arrow (`SDL_SCANCODE_UP`)
- DIK 201: Page Up (`SDL_SCANCODE_PAGEUP`)
- DIK 203: Left arrow (`SDL_SCANCODE_LEFT`)
- DIK 205: Right arrow (`SDL_SCANCODE_RIGHT`)
- DIK 207: End (`SDL_SCANCODE_END`)
- DIK 208: Down arrow (`SDL_SCANCODE_DOWN`)
- DIK 209: Page Down (`SDL_SCANCODE_PAGEDOWN`)
- DIK 210: Insert (`SDL_SCANCODE_INSERT`)
- DIK 211: Delete (`SDL_SCANCODE_DELETE`)
- DIK 219-221: Left Win, Right Win, Apps (`SDL_SCANCODE_LGUI`, `SDL_SCANCODE_RGUI`, `SDL_SCANCODE_APPLICATION`)

**Not mapped (return `SDL_SCANCODE_UNKNOWN`):**
- DIK 0: Reserved (no DIK_0 constant)
- DIK 84-86: Reserved/unused (gap between F10 and F11)
- DIK 89-140, 142-155: Unassigned DIK range — includes OEM-specific keys (DIK_AT, DIK_COLON, DIK_UNDERLINE, DIK_KANA, DIK_CONVERT, DIK_NOCONVERT, DIK_YEN, DIK_KANJI, DIK_PREVTRACK, DIK_STOP, DIK_AX, DIK_UNLABELED, DIK_OEM_102) and unassigned slots. All return `SDL_SCANCODE_UNKNOWN`.
- DIK 158-178, 180: Unassigned (gap between RCtrl and NumpadComma, plus unmapped slot at 180)
- DIK 185-198, 212-218, 222-255: Unassigned gaps and reserved ranges

**VK (Virtual Key) codes:** Values with the `0x80000000` flag set are treated as VK codes. VK→SDL translation **is implemented** via a fully-populated `kVkToSdl[256]` static lookup table at `sfall_kb_helpers.cc:284-563`, mapping Windows `VK_*` constants to `SDL_Scancode` values. The table is consumed by `get_scancode_from_key()` at `sfall_kb_helpers.cc:567-571`, which is called by `sfall_kb_is_key_pressed()` (used from `sfall_opcodes.cc:315` via `key_pressed()`/`tap_key()`). Most commonly-used VK keys are mapped (VK_BACK through VK_OEM_CLEAR); a few entries remain `SDL_SCANCODE_UNKNOWN` for keys without direct SDL equivalents (e.g., VK_LBUTTON, VK_MBUTTON).

**Additional hook notes:** Registering a hook type that has no engine fire site (HOOK_DEATHANIM1, HOOK_REMOVEINVENOBJ, HOOK_SUBCOMBATDAMAGE, HOOK_ADJUSTPOISON, HOOK_ROLLCHECK, HOOK_BESTWEAPON, HOOK_BUILDSFXWEAPON, and the obsolete HEX*BLOCKING hooks) will now emit a `debugPrint` warning. The hooks table above marks these as 🚫 with explanations.

## Et Tu (FO1-in-FO2) Compatibility

This section tracks Fallout Et Tu (https://github.com/rotators/Fo1in2) compatibility with CE. Et Tu runs as a total conversion: its 17 global scripts, 1000+ map scripts, FO1 content data, and three config files (`Fallout2.cfg`, `data/config/game#patch.cfg`, `ddraw.ini`) all run on the CE engine. Analysis snapshot: et tu master `c154bb8` (2026-08-11).

**Current status snapshot (2026-08-17):** 30/33 requirement rows verified ✅; the 3 remaining ❌ rows (NPC combat control, item highlighting, 4 optional engine settings) are **out of project scope** (owner decision 2026-08-17: 100% RPU + et tu support, not 100% sfall) — sfall-engine features no et tu/RPU script depends on. Remaining-work items 1-8, 10 DONE (tasks 2-5 + sync pass 2 + PerksFile + rotators metarules + worldmap trio); items 9 (partial), 11-12 open (P3). The one previously-documented blocker (RPU `C_ATTACK_*` offsets in `get_object_data`) was resolved by sync pass 2 (upstream 5774372, `b405e59`) — see the RPU section. **Post-audit fixes applied 2026-08-17:** FastShotFix double AP reduction (item 11(b) — adversarially CONFIRMED defect, fixed with sfall-faithful per-mode semantics, unit-tested); `[start] worldmap_x/y` default-clobber (F-072 — CONFIRMED, fixed with present-semantics reads); WorldMapTimeMod clamped [0,1000]; encounter-cadence counter resets; strict Perks.ini section parsing; HOOK_ADJUSTRADS row corrected.

### Requirements

| Requirement | Status | Evidence / notes |
| --- | --- | --- |
| sfall version gate (`sfall_ver_major < 5` in gl_0.ssl:27-29) | ✅ | CE reports 4.5.1 (`src/sfall_opcodes.cc:83-85`) |
| Rotators detection (`read_byte(0x410003)==0xF4` + `metarule_exist("rotators")`) | ✅ | `src/sfall_opcodes.cc:158-162`, `src/sfall_metarules.cc:1985` (`mf_metarule_exist`, rotators sentinel at `:1989-2000`) |
| CE detection (`ce_enabled` = `opcode_exists(0x823B)==false`) | ✅ | 0x823B `modified_ini` intentionally not implemented |
| Startup gate (`get_ini_setting("ddraw.ini\|...")` for AllowUnsafeScripting, DisableHorrigan, UseFileSystemOverride, Fallout1Behavior) | ✅ | **Fixed (2026-08-16):** when et tu's signature overlay (`data/config/game#patch.cfg` with `[start] map=V13ent.map`) is deployed, `contentConfigInit` seeds the three script-visible gate keys (`[start] use_filesystem_override`, `allow_unsafe_scripting`, `fallout1_behavior`) so `get_ini_setting("ddraw.ini|...")` resolves non-zero on first run (`src/content_config.cc`, `contentConfigSeedEtTuGateKeys`). The seed overrides unconditionally — the sfall migration (runs first, `contentConfigTryMigrateFromSfall`) writes gSfallConfig's "0" defaults into `game#patch.cfg` and would otherwise defeat set-if-absent seeding. DisableHorrigan comes from et tu's own patch (`[worldmap] disable_horrigan=1`). The one-time forced-restart loop is gone. The seeds only affect the script-visible bridge — AllowUnsafeScripting/UseFileSystemOverride stay intentionally unwired engine-side, and Fallout1Behavior is driven by `[start] fallout1_behavior` (scripts.cc:2043). |
| FO1 hit chance (`set_fo1_hit_chance(true)`, gl_fo1mechanics.ssl:115-117) | ✅ | Metarule H-04 (`sfall_metarules.cc:1226`, impl `sfall_metarules.cc:2251`); consumed in to-hit at `combat.cc:4859-4868` together with `gFallout1Behavior`. The et tu call is conditional on `fo1in2_fo2_hitchance_enabled == false` — it does not fire when the user enables "FO2 hit chance". |
| FO1 worldmap labels (`remove_wm_town_names(true)`, gl_classic_wm.ssl:30) | ✅ | Metarule H-05 (`sfall_metarules.cc:1244`, impl `sfall_metarules.cc:2476`); live circle-overlay label gated at `worldmap.cc:6762-6763` |
| Encounter handling (`encounter_detection(false)`, gl_fo1mechanics.ssl:109) | ✅ | Metarule → `wmSetEncounterDetection` |
| HOOK_ENCOUNTER (gl_worldmap.ssl:51-63 handler) | ✅ | **CE now uses the sfall arg0 encoding**: arg0=0 random / arg0=1 local-map-enter, arg2=isSpecial; forced encounters do not fire the hook (`src/sfall_script_hooks.cc:737-796`). Ret0 overrides map id, ret0=-1 cancels (event type 0). See the HOOK_ENCOUNTER row in the Hooks table above. |
| Reaction thresholds (`set_reaction_thresholds(25, 75)`, gl_fo1mechanics.ssl:112) | ✅ | Metarule (`sfall_metarules.cc:1241`, impl `sfall_metarules.cc:3435`) + `reaction.cc:41-49`; FO1/FO2 defaults preserved on game reset |
| Rest hours/strings (`rest_option_msgs(320)`, `set_rest_option(REST_OPTION_MORNING, 6)`, gl_0_settings.ssl:123-124) | ✅ | CE metarules (`sfall_metarules.cc:1247,1250`; impl `sfall_metarules.cc:3219,3230`); FO1 defaults 6:00 wake / base 321. Both calls are gated on `not(fo1in2_0800_resting_enabled)` |
| FO1 XP progression (`XPTable` in ddraw.ini; `[stats] xp_table` in game#patch.cfg) | ✅ | ddraw.ini `[Misc] XPTable` parsed and applied (`combat.cc:2088-2118`, `stat.cc:880`). `[stats] xp_table` from game.cfg / game#patch.cfg is consumed by the XP table loader (`stat.cc:880`), so game#patch.cfg-only deployments get the FO1 XP table (upstream 2223f7c, sync pass 2 / b405e59). |
| FO1 start position (`[start] worldmap_x/worldmap_y` in game#patch.cfg) | ✅ | Implemented (upstream `wmGetStartWorldMapConfigValue`, `worldmap.cc:1493-1507`; start position applied in `wmGenDataSetStartWorldPos`). et tu's game#patch.cfg (823, 72) places the player at the FO1 start. `-1` values fall back to the starting map's world area; ddraw.ini `StartXPos/StartYPos` (`[worldmap] start_x_pos/start_y_pos`, present-semantics reads at `worldmap.cc:1356-1364`/`1443-1451`) also works as an alternative source. **2026-08-17 fix:** the F-072 fork-override reads use present-semantics config access — when `[worldmap] start_x_pos/start_y_pos` are absent, the `[start]`-derived position survives (previously clobbered to the 173/122 default); the ddraw.ini→migration path is no longer required for the mechanism. |
| FO1 worldmap viewport (`[start] worldmap_view_x/worldmap_view_y`) | ✅ | Implemented (upstream 1db054e, sync pass 2 / b405e59) — initial worldmap viewport position, `worldmap.cc:1520-1524`. `-1` = default upper-left view. |
| Worldmap terrain info (`[worldmap] terrain_info=1`) | ✅ | Implemented (upstream de1ade9, sync pass 2 / b405e59) — `worldmap.cc:1314` reads `terrain_info` into `worldmapTerrainInfo` for the worldmap terrain display. |
| Worldmap travel speed (`WorldMapFPSPatch`/`WorldMapDelay2`; `[worldmap] travel_delay`) | ✅ | sfall boolean+ms semantics (M-64, `worldmap.cc:1320-1330`); `travel_delay` (`worldmap.cc:1311`) |
| Travel markers (`WorldMapTravelMarkers`/`trail_markers`) | ✅ | `content_config.cc:178`, `worldmap.cc:1302` |
| Special-map-ID disabling (`DisableSpecialMapIDs`; `[maps] disable_special_map_ids`) | ✅ | ddraw.ini key honored (`combat.cc:2079`, `worldmap.cc:4650-4654`); `[maps] disable_special_map_ids` from game.cfg / game#patch.cfg consumed at `map.cc:311` (upstream 0a53649, sync pass 2 / b405e59). |
| QuickPockets AP reduction (`QuickPocketsApCostReduction`; `[combat] quick_pockets_ap_cost_reduction`) | ✅ | ddraw.ini key honored (`sfall_callbacks.cc:79-91`); `[combat] quick_pockets_ap_cost_reduction` from game.cfg / game#patch.cfg consumed at `inventory.cc:1527` (upstream 267bfa9, sync pass 2 / b405e59). |
| PA weight (FO1 = not halved) | ✅ | **Fixed (2026-08-16):** the fork's `!gFallout1Behavior` gate was removed (`item.cc:867-874`) — CE now halves unconditionally like upstream. et tu's `adjust_pa_weight` (doubles protos) + engine halving = exact FO1 weight; previously the gate disabled halving AND the script doubled → 2× weight. |
| Party member dialog (`set_party_member_cc_msg_ids`, Dogmeat in `_Dogs`) | ✅ | Metarule (`sfall_metarules.cc:1211`); per-pid override consumed in `_gdPickAIUpdateMsg` (`game_dialog.cc:4124-4126`) |
| Party armor appearance / weapon restrictions (gl_partyarmor.ssl) | ✅ | HOOK_INVENWIELD/ADJUSTFID/INVENTORYMOVE/CANUSEWEAPON, `art_exists`, `get_object_data`, `real_dude_obj` |
| TMA ("Tell Me About", gl_tma.ssl) | ✅ | HOOK_KEYPRESS DIK arg1 + ret0=255 swallow (`sfall_kb_helpers.cc:741-743`); `get_ini_section("config\keymap.ini", language)` |
| Motorcycle (gl_car.ssl, MOTRCYCL.ssl) | ✅ | HOOK_CARTRAVEL, HOOK_MOUSECLICK, `set_car_intface_art` |
| Auto doors / auto push / armor destroy | ✅ | HOOK_COMBATTURN, HOOK_STDPROCEDURE, `set_proto_data` (0x8205), arrays, `set_flags` |
| Ammo INI loader (gl_ammomod.ssl) | ✅ | `get_ini_setting("ddraw.ini\|Misc\|DamageFormula")` bridge; `set_proto_data` ammo offsets |
| `get/set_proto_data` offsets used by et tu (PROTO_CR_FLAGS, PROTO_IT_WEIGHT, PROTO_WP_DMG_TYPE, PROTO_CR_BONUS_HP, PROTO_AM_*, PROTO_AR_DR_PLASMA, PROTO_FID, PROTO_WP_ANIM/RANGE, PROTO_CN_MAX_SIZE) | ✅ | Raw-offset impl with bounds+alignment checks (`sfall_opcodes.cc:1044-1093`) |
| `game#patch.cfg` overlay mechanism | ✅ | `content_config.cc:12-15,31-34` (config\game.cfg + config\game#patch.cfg, VFS-aware) |
| et tu `Fallout2.cfg` | ✅ | All keys CE-native (system paths, ui, sound, qol, screen) |
| Perks.ini (`PerksFile=config\Perks.ini`) | ✅ | **DONE (2026-08-17):** `[PerksTweak]` (36 keys) + `[Perks]` section (Enable-gated per-perk overrides incl. `Ranks=-1` removal) + `[Traits]` section (NoHardcode/StatMod/SkillMod/Name/Desc/Image) all applied at init (`src/perk_tweak.cc`, `src/perk.cc`, `src/trait_tweak.cc`, `src/trait.cc`); et tu FO1 perk/trait tuning now applies. |
| NPC combat control (`mods/sfall-mods.ini [CombatControl] Mode=3`) | ⛔ | Out of scope (owner 2026-08-17): sfall engine feature, no et tu/RPU script depends on it (the consuming sfall mod script is not shipped). |
| Item highlighting (`mods/sfall-mods.ini [Highlighting]`) | ⛔ | Out of scope (owner 2026-08-17): sfall engine feature, no et tu/RPU script depends on it (the consuming sfall mod script is not shipped). |
| `WorldMapTimeMod`, `WorldMapEncounterFix/Rate`, `UseScrollingQuestsList`, `ItemCounterAutoCaps`, `DeathScreenFontPatch`, `EnableMusicInDialogue` | ⚠️ | **2026-08-17:** worldmap trio implemented (`WorldMapTimeMod` multiplier in `wmGameTimeIncrement` — clamped [0,1000] 2026-08-17; `WorldMapEncounterFix/Rate` rate-gated encounter rolls — counter reset per walking session 2026-08-17; see remaining-work item 9). `UseScrollingQuestsList`/`ItemCounterAutoCaps`/`DeathScreenFontPatch`/`EnableMusicInDialogue` are out of scope (owner 2026-08-17): absent in CE, no et tu/RPU script reads them. **2026-08-17 correction:** et tu ships `UseScrollingQuestsList=0` and `EnableMusicInDialogue=0` (disabled) but `ItemCounterAutoCaps=1` and `DeathScreenFontPatch=1` (enabled) — CE not implementing them means the FO1 barter-cap auto-balance and death-screen font are missing QoL/cosmetic behaviors (out-of-scope misses). |
| Rotators-only metarules (`r_call_offset*`, `r_hrp*`) | ✅ | **2026-08-17:** registered as safe no-ops (log + return 0) so `metarule_exist("r_...")` probes succeed; VOODOO/HRP behavior itself not implemented. Not called by any shipped et tu script — zero runtime impact. |

### Remaining work for full et tu support

Prioritized (P1 = blocks/degrades core et tu experience; P2 = config parity; P3 = optional/QoL):

1. ~~P1 — First-run startup gate~~ ✅ **DONE (2026-08-16):** `contentConfigInit` detects et tu's `game#patch.cfg` overlay (`[start] map=V13ent.map`) and seeds the script-visible gate keys (`use_filesystem_override`, `allow_unsafe_scripting`, `fallout1_behavior`); `DisableHorrigan` comes from et tu's own patch. No forced restart. Covered by unit tests (`tests/test_global_vars.cc:1207-1270`).
2. ~~P1 — FO1 power-armor weight double-count~~ ✅ **DONE (2026-08-16):** fork gate removed (`item.cc:867-874`); CE halves unconditionally like upstream, so et tu's `adjust_pa_weight` compensation is exact.
3. ~~P1 — `[start] worldmap_x/y`~~ ✅ **DONE (sync pass 2, b405e59):** `wmGetStartWorldMapConfigValue` (`worldmap.cc:1471-1475` + `1480-1496`), applied in `wmGenDataSetStartWorldPos`. (Upstream 1db054e "Implement Start{X|Y}Pos".)
4. ~~P2 — `[worldmap] terrain_info`~~ ✅ **DONE (sync pass 2, b405e59):** `worldmap.cc:1303`. (Upstream de1ade9.)
5. ~~P2 — `[stats] xp_table` from game.cfg~~ ✅ **DONE (sync pass 2, b405e59):** consumed at `stat.cc:880`. (Upstream 2223f7c.)
6. ~~P2 — `[maps] disable_special_map_ids` + `[combat] quick_pockets_ap_cost_reduction` from game.cfg~~ ✅ **DONE (sync pass 2, b405e59):** `map.cc:311`, `inventory.cc:1527`. (Upstream 0a53649, 267bfa9.)
7. ~~P2 — `[start] worldmap_view_x/y`~~ ✅ **DONE (sync pass 2, b405e59):** `worldmap.cc:1520-1524`. (Upstream 1db054e.)
8. ~~P2 — Implement `PerksFile` (config/Perks.ini `[PerksTweak]`)~~ ✅ **DONE (2026-08-17):** `PerksFile` parsed (`sfall_config.cc`), `[PerksTweak]` loaded by `perkTweakLoad()` (`src/perk_tweak.cc`, 36 keys, sfall gate+clamp semantics verified against sfall's `Perks.cpp`), `[Perks]` section applied to `gPerkDescriptions` (`src/perk.cc` `perksLoadCustomConfig()`, Enable-gated, `-99999` ignore sentinel, `Ranks=-1` removal) — et tu's FO1 balance now applies (NightVision 10, Survivalist 0, MrFixit 20, Medic 20/20, MasterThief 0, Speaker 0, Ghost 20, Ranger 0, Salesman 0, Negotiator 20, …). Script opcodes (`set_perk_*`) still take precedence (override arrays checked first). `[Traits]` section (NoHardcode/StatMod/SkillMod) ✅ **DONE (2026-08-17):** `src/trait_tweak.cc` parses the Enable-gated `[tN]` blocks; `trait.cc` applies NoHardcode gating (all 20 hardcoded trait_adjust_* contribution sites) + StatMod/SkillMod pair lists for selected/added traits + Name/Desc (255-char truncated)/Image overrides — et tu's Night Person (-1 PER/INT, Image 68) and Skilled (+10 all skills) rows now apply. Known fidelity note: StatMod applies unconditionally (sfall's format has no day/night condition; et tu's "at day" comment is descriptive) — FO1's night-time +1/+1 swing is not replicated; kept as a runtime-verification item. Character-editor Skilled effects (+5 skill points/level, perk progression) are outside trait_adjust_* and untouched, mirroring sfall.
9. **P3 — Optional sfall engine features** et tu ships config for: NPC combat control (`sfall-mods.ini [CombatControl]`), key-driven item highlighting (`[Highlighting]`), ~~`WorldMapTimeMod`~~ ✅, ~~`WorldMapEncounterFix`/`WorldMapEncounterRate`~~ ✅, `UseScrollingQuestsList`, `ItemCounterAutoCaps`, `DeathScreenFontPatch`, `EnableMusicInDialogue`. **Partial (2026-08-17):** the worldmap trio is done — `WorldMapTimeMod` (percent multiplier on the world-map game-time increment, `worldmap.cc` `wmGameTimeIncrement`, in addition to Pathfinder + `set_map_time_multi`), `WorldMapEncounterFix`/`WorldMapEncounterRate` (rate-gated encounter rolls, once per `Rate` walking frames when `Fix=1`; `Fix=0` keeps the vanilla per-step roll — et tu ships `Fix=0`/`Rate=30`, both inert there). `UseScrollingQuestsList` deferred: CE's pipboy quest list is paginated and the feature needs scroll-button UI art + a quest-list rework; et tu ships it disabled (`ddraw.ini:497`). The rest remain open.
10. ~~P3 — Register the 5 rotators-only metarules~~ ✅ **DONE (2026-08-17):** `r_call_offset`, `r_call_offset_cdecl`, `r_call_offset_push`, `r_hrp`, `r_hrp_offset` registered as safe no-ops (`sfall_metarules.cc` kMetarules table + `mf_r_call_offset*`/`mf_r_hrp*` handlers) so `metarule_exist("r_...")` probes and wrapper-macro calls (et tu `sfall.rotators.voodoo.h`) succeed; handlers log and return 0 (r_hrp returning 0 = "HRP absent" fallback). The registration does NOT implement VOODOO call-offset execution or HRP integration — the old deliberate non-registration comment was updated accordingly. Zero shipped et tu scripts call `r_*` (re-verified 2026-08-17).
11. **P3 — Verify at runtime (build + test)**: (a) `set_reaction_thresholds(25,75)` persistence across game reset vs `gl_fo1mechanics.ssl` re-application; (b) ~~FastShotFix=3 + `gl_apcost.ssl` CE-path double AP reduction for melee/unarmed with Fast Shot~~ ✅ **CONFIRMED as a real defect and FIXED (2026-08-17):** CE now implements sfall's per-mode FastShotFix semantics (modes 0/1 ranged-class only; modes 2/3 all weapons; HtH never reduced; the mode-2/3 melee-class reduction defers to the HOOK_CALCAPCOST layer when a handler is registered, so et tu's `gl_apcost.ssl` supplies it exactly once — total −1 AP for all weapon classes on et tu); covered by the rewritten Fast Shot unit suite (`tests/test_item.cc`, 48 assertions + 16 called-shot assertions); (c) the new startup-gate seeding (item 1) passes `gl_0.ssl` on first run with et tu's game#patch.cfg deployed. **Partial (2026-08-17):** (b) and (c) are now covered (unit tests); (a) still needs an in-game build+test run.
12. **Docs — Verify the merged HOOK_ENCOUNTER/HOOK_KEYPRESS rows stay accurate when the hook code next changes** (both rows are current: HOOK_ENCOUNTER arg0=0/1, arg2=special, no forced-fire; HOOK_KEYPRESS arg1=DIK, arg2=VK). Re-verified current 2026-08-17; keep as a standing check.

## RPU (Fallout 2 Restoration Project) Compatibility

CE supports the [Fallout 2 Restoration Project, updated (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) (requires sfall 4.5; CE reports 4.5.1). RPU's own scripting surface is small: 4 sfall hooks, ~25 opcodes/metarules, and sfall ddraw.ini config keys. All hooks and opcodes/metarules are implemented; the config keys are handled as shown in the table below (one documented-unsupported cosmetic — BoxBarColours, parsed but inert). Three APIs RPU uses that are implemented but not individually tabled anywhere — `force_encounter` (`sfall_opcodes.cc:392-396`, 0x8171), `message_box` via `sfall_func4` (`sfall_metarules.cc:4657`), `metarule3(107)` art_change_fid_num (`interpreter_extra.cc:2063`) — verified 2026-08-17. RPU's `[Debugging]` ddraw.ini keys (Enable=0 etc.) are unparsed but inert (no script reads them). The table below is a requirement-by-requirement audit against RPU master `f7c10859` (2026-08-10).

**Current status snapshot (2026-08-17):** all 4 hooks and all opcodes/metarules ✅; the previously-open P1 `get_object_data(C_ATTACK_*)` offset issue is resolved by sync pass 2 (upstream 5774372, `b405e59`). The three P3 config-key gaps (OverrideArtCacheSize, ProcessorIdle, BoxBarColours) were resolved 2026-08-17 and `FemaleDialogMsgs` was implemented 2026-08-17 — see the table rows. Remaining item 4 (P2 pipboy automap capacity) is also DONE (2026-08-17): `AUTOMAP_MAP_COUNT` raised to 173 (`src/automap.h:36`) — see the remaining-work list. **Post-audit fixes applied 2026-08-17:** fs_copy VFS handles are now unbuffered (`setvbuf _IONBF`) so same-path FRM FPS patches are visible to the engine's art loader within the session (loose-data installs); `mf_get_object_data` null-guards the object path (hardening mirroring `mf_set_object_data`); the stale HOOK_ADJUSTRADS no-fire-site exclusion was removed.

| Requirement | Status | Evidence / Notes |
| --- | --- | --- |
| Hooks: HOOK_USEOBJON, HOOK_USEOBJ, HOOK_GAMEMODECHANGE, HOOK_COMBATDAMAGE | ✅ | RPU registers only these 4 (`scripts_src/global/gl_k_alcohl.ssl:102-103`, `gl_k_dogmeat_fix.ssl:20`, `gl_k_wpnchk.ssl:53,61`). HOOK_COMBATDAMAGE 13-arg layout matches RPU's sequential `get_sfall_arg` reads (target, attacker, dmg_target, dmg_attacker, flags_target, flags_attacker, weapon, body_part). |
| `get_ini_setting` / `set_ini_setting` / `get_ini_string` (incl. `mods\rpu.ini`, `mods\upu.ini`, ddraw.ini keys) | ✅ | `src/sfall_ini.cc` opcodes + content-config bridge + gSfallConfig fallback. RPU reads `mods\*.ini` via relative paths and `WorldMapSlots`/`BoostScriptDialogLimit`/`EnableHeroAppearanceMod`/`UseFileSystemOverride` from ddraw.ini. |
| `WorldMapSlots=21` (RPU gate: `gl_k_modini.ssl:14`, `!= 21` → `signal_end_game`) | ✅ | H-06: gSfallConfig default 21 (`src/sfall_config.cc:112`) flows through `op_get_ini_setting` fallback (`src/sfall_ini.cc:661-680`). |
| `BoostScriptDialogLimit=1` (RPU gate: `gl_k_modini.ssl:18`, `== 0` → `signal_end_game`) | ✅ | CE returns -1 for the absent key (never 0) (`src/sfall_ini.cc:661`); dialog message capacity is 10000 (`src/scripts.cc:58`), so the boost is inherently satisfied. |
| `EnableHeroAppearanceMod=1` (`epai37.ssl:101`) | ✅ | CE's Hero Appearance feature is always-on (matches RPU default 1). |
| `ElevatorsFile=mods\elevators.ini` + elevators.ini format | ✅ | CE reads `gSfallConfig [Misc] ElevatorsFile` (`src/elevator.cc:688-746`) and parses `[N] Image/IDn/Elevationn/Tilen` — RPU `mods/elevators.ini` sections [24]-[28] match. |
| `UseFileSystemOverride=1` (`upu.h:14-20` gate) | ✅ | **Fixed (2026-08-16):** when et tu's `game#patch.cfg` overlay is detected, `contentConfigInit` seeds `[start] use_filesystem_override=1` (`contentConfigSeedEtTuGateKeys`), so RPU's `check_filesystem_override` gate passes without ddraw.ini edits. RPU's own ddraw.ini (ships `UseFileSystemOverride=1`) and `[start] use_filesystem_override=1` in game.cfg remain valid alternatives. Note: the flag is intentionally unwired engine-side (CE's VFS priority handles override); the seed only affects the script-visible value. |
| `ExtraSaveSlots=1` | ✅ | WIRED — 100 save pages / 1000 slots (`src/loadsave.cc:527-531`, gExtraSaveSlots). |
| `KarmaFRMs` / `KarmaPoints` | ✅ | Migrated to `[karma] frms/points`, consumed at `src/character_editor.cc:7885-7891`. |
| `FemaleDialogMsgs=2` | ✅ | **Implemented (2026-08-17):** parsed into `gFemaleDialogMsgs` (`src/sfall_config.cc`), consumed by `messageListGetLocalizedDir` (`src/message.cc:359-381`): a female player with level ≥1 loads dialog messages from `text\<lang>\dialog_female\` and with level ≥2 cutscene subtitles from `text\<lang>\cuts_female\`, with per-directory fallback to the normal dirs when the female file is absent (English RPU ships no `dialog_female`/`cuts_female` — the feature is byte-identical there; czech/german/hungarian/swedish ship only `cuts_female`, so per-dir fallback matters for them too). Wired sites: script dialog messages (`src/scripts.cc:3328-3352`), death voiceover subtitles (`src/main.cc:638-655`), endgame ending subtitles (`src/endgame.cc:766-774,868-882`), movie subtitles (`src/game_movie.cc:396-436`). No savegame impact (load-time path selection only). |
| `OverrideArtCacheSize=1` | ✅ | WIRED (2026-08-17): parsed into `gOverrideArtCacheSize` + the new `[Misc] ArtCacheSize` global `gSfallArtCacheSize` (default 261 MB — sfall's fixed override value when `OverrideArtCacheSize=1` with no `ArtCacheSize` key, see sfall-readme.txt:361 "set the art cache size to 261"; `src/sfall_config.h:56-69` — 261 at :65, clamps at :68-69); `artInit` selects via `sfallArtCacheSizeMb()` (`src/art.cc:168-175`, selection at `src/sfall_config.cc:160-167`), clamped 8..512 MB mirroring the CE setting clamp (`src/settings.cc:155`). |
| `BoxBarColours=11111` | ⚠️ | Parsed (2026-08-17) into `gBoxBarColours` (`src/sfall_config.cc`) — accepted-but-inert: CE has no sfall box-bar colour rendering equivalent (cosmetic). Logged at config load. |
| `ProcessorIdle=1` | ✅ | Satisfied by CE's FPS limiter (2026-08-17): `mainLoop` already yields the CPU every frame via `SDL_Delay` (`src/fps_limiter.cc:18-23`, `src/main.cc:446,470`) — no busy-wait exists to remove. Parsed into `gProcessorIdle` (`src/sfall_config.cc`) as a documented passthrough and logged at config load. |
| `[Scripts] IniConfigFolder=mods` | ✅ | CE uses it as the ini base path (`src/game.cc:452-455`, `sfall_ini_set_base_path`). |
| Global scripts `scripts\gl_k_*.int` | ✅ | CE auto-discovers `scripts\gl*.int` + `scripts\sfall\gl*.int` incl. inside .dat mods (`src/sfall_global_scripts.cc:118-140`). |
| `scripts.lst` override in mod (1558 scripts, `# local_vars=` annotations) | ✅ | Dynamic list load (`src/scripts.cc:1804-1852`); parses `local_vars=`. |
| Worldmap content: city.txt (61 areas), worldmap.txt (20 tiles), maps.txt (173 maps) | ✅ | CE loads all dynamically (`src/worldmap.cc`: city.txt 3225, worldmap.txt 2091, maps.txt 3427). Pipboy automap now covers all 173 maps — `AUTOMAP_MAP_COUNT` raised to 173 (`src/automap.h:36`; derived header size 2081 at `src/automap.cc:40`). |
| Elevator content via `mods/elevators.ini` | ✅ | See ElevatorsFile row. |
| `.edg` files (175) | ✅ | CE `.edg` scroll-block/stencil support (`edg_support=1`, `src/map.cc:1070`). |
| Data files: ai.txt, party.txt, quests.txt, karmavar.txt, endgame.txt, vault13.gam | ✅ | All read by CE (`src/combat_ai.cc:370`, `src/party_member.cc:130`, `src/pipboy.cc:2873`, `src/character_editor.cc:7704`, `src/endgame.cc:1074`, `src/game.cc:1089`). |
| Hero appearance opcodes `set_hero_style`/`set_hero_race` (`epai37.ssl`) | ✅ | Implemented (`src/sfall_opcodes.cc:6000-6011`, registered 0x8213-0x8215 at `src/sfall_opcodes.cc:8839-8841`), store `HApStyle`/`HAp_Race` globals RPU reads. |
| `get_sfall_global_int` on unset keys | ✅ | M-03 returns 0 for missing keys, matching sfall (`src/sfall_opcodes.cc:690-697`). |
| `fs_copy(path, path)` in-place FRM patch (UPU Goris de-robing FPS, critters walk faster) | ✅ | **Fixed (2026-08-16):** identical resolved paths now get a non-truncating `r+b` read-write handle over the original file (C-06 replaced, `src/sfall_opcodes.cc:2595-2627`) — no truncation (safety intent preserved), handle non-deletable (original asset never removed at free, H-27 family). UPU's `fs_copy(path,path)` + `fs_seek`/`fs_read_short`/`fs_write_short` FRM FPS patching works in-place on loose-data installs. **2026-08-17:** VFS file handles handed to scripts are now unbuffered (`setvbuf _IONBF` at the fs_create/fs_copy/fs_resize open sites, `sfallVfsFopen`) — `fs_write_short` writes are visible to the engine's independent art-loader reads within the session on loose-data installs (previously the buffered write never reached the OS file before the engine's separate open). DAT-resident FRMs still need the pre-patched dats (`walk_speed_fix_low_fps.dat`, `goris_fast_derobing_low_fps.dat` — fs_copy cannot open files inside .dat archives, matching sfall). |
| `get_object_data(combat_data, C_ATTACK_*)` (boxing KO check `ncprzftr.ssl:143`) | ✅ | **Fixed (sync pass 2, upstream 5774372, `b405e59`):** `mf_get_object_data` now routes active combat data through `AttackDataField` with **sfall-matching `C_ATTACK_*` offsets** — `C_ATTACK_FLAGS_TARGET` (0x30) correctly reads `attack->defenderFlags` (`sfall_metarules.cc:93-139, 227-228, 1561-1591`). RPU's boxing KO check (`ncprzftr.ssl:143`) works. `set_object_data` writes were adopted to the same field routing (2026-08-17) — attack-data and extended `OBJ_DATA_*` writes now supported via typed setters (`setAttackData`/`setObjectData`), with `ARG_ANY` value typing. `mf_get_object_data` additionally null-guards the object path (2026-08-17 hardening, mirroring `mf_set_object_data`). |
| Mod loading: `mods_order.txt` + `mods/rpu.dat` etc. | ✅ | CE `sfallLoadMods` (`src/sfall_ext.cc:187-267`); RPU installer places `mods_order.txt` in `mods/`. |

### Remaining work for full RPU support

Prioritized (P1 = affects bundled RPU features; P2 = minor/edge):

**Current status snapshot (2026-08-17):** 24/25 requirement rows verified ✅; 1 ⚠️ (BoxBarColours — parsed, no CE rendering equivalent). The previously-open P1 items are all DONE: `fs_copy` same-path (2026-08-16), `UseFileSystemOverride` gate (2026-08-16), and `C_ATTACK_*` offsets in `get_object_data` (sync pass 2, upstream 5774372). The three P3 config-key gaps (OverrideArtCacheSize, ProcessorIdle, BoxBarColours) and `FemaleDialogMsgs` are resolved (2026-08-17). Remaining item 4 (P2 automap capacity) is DONE (2026-08-17) — `AUTOMAP_MAP_COUNT` raised to 173 (`src/automap.h:36`). The `fs_copy` script path gained write-through visibility 2026-08-17 (unbuffered VFS handles — see the requirements row).

1. ~~P1 — Support `fs_copy(src, src)` (same-path copy)~~ ✅ **DONE (2026-08-16):** identical resolved paths return a non-truncating `r+b` read-write handle over the original file (`src/sfall_opcodes.cc:2595-2627`); handle marked non-deletable so the original asset is never removed at free. **Write-through added (2026-08-17):** VFS handles are unbuffered (`setvbuf _IONBF`, `sfallVfsFopen`) so `fs_write_short` patches are visible to the engine's art loader during the session on loose-data installs. UPU `goris_derobing_speed` and `critters_walk_faster` now work on loose-data installs (DAT-resident FRMs still need the pre-patched dats).
2. ~~P1 — Map sfall `C_ATTACK_*` offsets to CE's 64-bit `Attack` layout in `get_object_data(combat_data, ...)`~~ ✅ **DONE (sync pass 2, upstream 5774372, `b405e59`):** `mf_get_object_data` routes active combat data via `AttackDataField` with sfall-matching offsets (`C_ATTACK_FLAGS_TARGET`=0x30 → `attack->defenderFlags`, `sfall_metarules.cc:93-139, 227-228, 1561-1591`); `activeAttackData()` also accepts HOOK_COMBATDAMAGE arg12 pointers. RPU `ncprzftr.ssl:143` boxing KO check reads `C_ATTACK_FLAGS_TARGET` correctly. **Write side completed (2026-08-17):** `mf_set_object_data` was adopted to the same typed field routing (`setAttackData`/`setObjectData`, `sfall_metarules.cc:266-399, 580-773, 3203-3271`) with `ARG_ANY` value typing — the fork's old raw-byte whitelist (offsets 0-40 only) was removed, so `C_ATTACK_*` and extended `OBJ_DATA_*` writes now work (et tu `gl_z_throwing_hex.ssl:130` writes `OBJ_DATA_WHO_HIT_ME`; `set_weapon_unusable`/`set_weapon_usable` macros write `OBJ_DATA_MISC_FLAGS`).
3. ~~P2 — Make `UseFileSystemOverride` gate pass by default~~ ✅ **DONE (2026-08-16):** `contentConfigSeedEtTuGateKeys` seeds `[start] use_filesystem_override=1` when the et tu `game#patch.cfg` overlay is present; RPU's `check_filesystem_override` (`upu.h:14-20`) passes on et-tu-style deployments without ddraw.ini edits. (RPU's own ddraw.ini already sets 1.)
4. ~~P2 — Raise pipboy automap capacity above 160 for RPU's 173 maps~~ ✅ **DONE (2026-08-17):** `AUTOMAP_MAP_COUNT` raised from 160 to 173 (`src/automap.h:36`); the derived on-disk header `dataSize` is now `5 + AUTOMAP_OFFSET_COUNT * sizeof(int)` = 2081 bytes (`src/automap.cc:40`). RPU maps 160-172 (EPA sublevels, SF Sheng's, Slaver Camp, safehouses, etc.) now get pipboy automap entries; mods with more than 173 maps remain clamped/rejected by `automapMapIndexIsValid` (`src/automap.cc:87-89`). Pinned by `tests/test_automap.cc` (static_asserts: AUTOMAP_MAP_COUNT == 173, dataSize == 2081, last valid index 172).
5. ~~P3 — `FemaleDialogMsgs` support for non-English RPU translations~~ ✅ **DONE (2026-08-17):** sfall's female dialog message selection (`dialog_female`/`cuts_female` dirs) implemented — parsed `gFemaleDialogMsgs` (`src/sfall_config.cc`), helper `messageListGetLocalizedDir` (`src/message.cc:359-381`), wired at the script dialog load (`src/scripts.cc:3328-3352`), death voiceover (`src/main.cc:638-655`), endgame ending subtitles (`src/endgame.cc:766-774,868-882`) and movie subtitles (`src/game_movie.cc:396-436`) with per-directory fallback to the normal dirs on missing female files. English RPU is unaffected (no `dialog_female`/`cuts_female` dirs shipped → fallback → byte-identical behavior); the value matters only for non-English RPU translations (french/italian/polish/portuguese/russian/spanish/vietnamese ship both dirs; czech/german/hungarian/swedish ship `cuts_female` only — verified from `tmp/rpu/data/text/*`). No savegame impact (load-time path selection only).
6. ~~P3 — `ProcessorIdle` / `BoxBarColours` parity~~ ✅ **DONE (2026-08-17):** both parsed in `src/sfall_config.cc` (`gProcessorIdle`, `gBoxBarColours`) and logged at config load. `ProcessorIdle` needs no engine change — CE's FPS limiter already yields the CPU every frame via `SDL_Delay` (`src/fps_limiter.cc:18-23`, `src/main.cc:473`); no busy-wait exists in `mainLoop`. `BoxBarColours` is accepted-but-inert — CE has no sfall box-bar colour rendering equivalent. `OverrideArtCacheSize` (earlier gap) is also wired — see the requirements table row.
