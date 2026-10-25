# ---- Test Framework ----
# doctest: single-header C++ testing framework
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_SOURCE_DIR}/third_party/doctest")

# ---- Common test stubs ----
# Stub implementations of engine functions that source files reference but
# that are not needed for unit-test level correctness (debugPrint, compat_*).
add_library(test_stubs STATIC
    test_common_stubs.cc
)
target_include_directories(test_stubs PUBLIC "${CMAKE_SOURCE_DIR}/src")
# SDL2 is needed for header includes: test_common_stubs.cc includes
# settings.h -> svga.h -> SDL.h (WindowMode type added by upstream
# incorporation 82cb826).
target_link_libraries(test_stubs PUBLIC doctest ${SDL2_LIBRARIES})

# ---- Source-under-test library ----
# Compiles only the translation units needed for unit tests.
# Links against SDL2/zlib (vendored) to satisfy header includes.
add_library(test_sources STATIC
    "${CMAKE_SOURCE_DIR}/src/config.cc"
    "${CMAKE_SOURCE_DIR}/src/content_config.cc"
    "${CMAKE_SOURCE_DIR}/src/dictionary.cc"
    "${CMAKE_SOURCE_DIR}/src/memory.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_global_vars.cc"
    "${CMAKE_SOURCE_DIR}/src/game_config_migration.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_config.cc"
    "${CMAKE_SOURCE_DIR}/src/perk_tweak.cc"
    "${CMAKE_SOURCE_DIR}/src/trait_tweak.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_arrays.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_ini.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_lists.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_global_scripts.cc"
    "${CMAKE_SOURCE_DIR}/src/sfall_kb_helpers.cc"
)
target_include_directories(test_sources PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sources PUBLIC ${ZLIB_INCLUDE_DIRS})
target_compile_definitions(test_sources PUBLIC TEST_ACCESSORS_ENABLED)
target_link_libraries(test_sources PUBLIC test_stubs ${SDL2_LIBRARIES})

# ---- Test: config.cc ----
add_executable(test_config
    test_config.cc
)
target_link_libraries(test_config PRIVATE test_sources test_stubs doctest)
add_test(NAME test_config COMMAND test_config)

# ---- Test: sfall_global_vars.cc ----
add_executable(test_global_vars
    test_global_vars.cc
)
target_link_libraries(test_global_vars PRIVATE test_sources test_stubs doctest)
add_test(NAME test_global_vars COMMAND test_global_vars)

# ---- Test: criticals (combat.cc data structures) ----
# Self-contained test — does not link combat.cc (too many engine deps).
add_executable(test_criticals
    test_criticals.cc
)
target_link_libraries(test_criticals PRIVATE doctest)
add_test(NAME test_criticals COMMAND test_criticals)

# ---- Test: script hooks (header-level type validation) ----
# Tests HookType enums, constants, and sub-type definitions.
# Header-only test — does not link sfall_script_hooks.cc.
add_executable(test_script_hooks
    test_script_hooks.cc
)
target_include_directories(test_script_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
# SDL2/zlib headers needed for transitive includes from interpreter.h
target_include_directories(test_script_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_script_hooks PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_script_hooks COMMAND test_script_hooks)

# ---- Test: game_config_migration.cc ----
add_executable(test_game_config_migration
    test_game_config_migration.cc
)
target_link_libraries(test_game_config_migration PRIVATE test_sources test_stubs doctest)
add_test(NAME test_game_config_migration COMMAND test_game_config_migration)

# ============================================================
# Stage 4 Implementation — New Test Executables (29 entries)
# Added by consolidation agent (s4-impl-cmake-consolidate)
# ============================================================

# ---- Test: combat data structures ----
# Self-contained — does not link combat.cc (40+ engine deps).
add_executable(test_combat
    test_combat.cc
)
target_include_directories(test_combat PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_combat PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_combat PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_combat COMMAND test_combat)

# ---- Test: combat hooks ----
# Self-contained — does not link combat.cc.
add_executable(test_combat_hooks
    test_combat_hooks.cc
)
target_include_directories(test_combat_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_combat_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_combat_hooks PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_combat_hooks COMMAND test_combat_hooks)

# ---- Test: interpreter core (data structures) ----
# Self-contained test — does not link interpreter.cc (50+ engine deps).
add_executable(test_interpreter_core
    test_interpreter_core.cc
)
target_link_libraries(test_interpreter_core PRIVATE doctest)
add_test(NAME test_interpreter_core COMMAND test_interpreter_core)

# ---- Test: interpreter_extra (data structures + logic patterns) ----
# Self-contained test — does not link interpreter_extra.cc (50+ engine deps).
add_executable(test_interpreter_extra
    test_interpreter_extra.cc
)
target_link_libraries(test_interpreter_extra PRIVATE doctest)
add_test(NAME test_interpreter_extra COMMAND test_interpreter_extra)

# ---- Test: inventory.cc core logic patterns ----
# Self-contained test — does not link inventory.cc (60+ engine deps).
add_executable(test_inventory
    test_inventory.cc
)
target_link_libraries(test_inventory PRIVATE doctest)
add_test(NAME test_inventory COMMAND test_inventory)

# ---- Test: item.cc (addiction, power armor weight, Fast Shot AP) ----
# Self-contained test — does not link item.cc (70+ engine deps).
add_executable(test_item
    test_item.cc
)
target_link_libraries(test_item PRIVATE doctest)
add_test(NAME test_item COMMAND test_item)

# ---- Test: stat.cc (experience, stat min/max, XP modifier) ----
# Self-contained test — does not link stat.cc.
add_executable(test_stat
    test_stat.cc
)
target_link_libraries(test_stat PRIVATE doctest)
add_test(NAME test_stat COMMAND test_stat)

# ---- Test: game.cc (state machine, SpeedMulti, globalVarsRead fix) ----
# Self-contained test — does not link game.cc.
add_executable(test_game
    test_game.cc
)
target_link_libraries(test_game PRIVATE doctest)
add_test(NAME test_game COMMAND test_game)

# ---- Test: map.cc / map_edge.cc (coordinate math, aging fix) ----
# Self-contained test — does not link map.cc or map_edge.cc.
add_executable(test_map
    test_map.cc
)
target_link_libraries(test_map PRIVATE doctest)
add_test(NAME test_map COMMAND test_map)

# ---- Test: sfall_config (configGetInt/Double/IntList fork validation) ----
# Links test_sources (config.cc, sfall_config.cc) for config/sfall_config testing.
add_executable(test_config_sfall
    test_config_sfall.cc
)
target_link_libraries(test_config_sfall PRIVATE test_sources test_stubs doctest)
add_test(NAME test_config_sfall COMMAND test_config_sfall)

# ---- Test: misc engine (configGetBool/SetDouble/IntList edges, ScopedConfig) ----
# Links test_sources (config.cc) for config function testing.
add_executable(test_misc_engine
    test_misc_engine.cc
)
target_link_libraries(test_misc_engine PRIVATE test_sources test_stubs doctest)
add_test(NAME test_misc_engine COMMAND test_misc_engine)

# ---- Test: object.cc (data structures, RAII, pure logic) ----
# Self-contained test — does not link object.cc (40+ engine deps).
# Includes obj_types.h from src/ for real type definitions.
add_executable(test_object
    test_object.cc
)
target_include_directories(test_object PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_object PRIVATE doctest)
add_test(NAME test_object COMMAND test_object)

# ---- Test: party_member.h (self-contained test-local mirrors) ----
add_executable(test_party
    test_party.cc
)
target_link_libraries(test_party PRIVATE doctest)
add_test(NAME test_party COMMAND test_party)

# ---- Test: perk.h (self-contained test-local mirrors) ----
add_executable(test_perk
    test_perk.cc
)
target_link_libraries(test_perk PRIVATE doctest)
add_test(NAME test_perk COMMAND test_perk)

# ---- Test: critter.h (self-contained test-local mirrors) ----
add_executable(test_critter
    test_critter.cc
)
target_link_libraries(test_critter PRIVATE doctest)
add_test(NAME test_critter COMMAND test_critter)

# ---- Test: proto data structures ----
# Self-contained test — does not link proto.cc (50+ engine deps).
# Includes proto_types.h, obj_types.h from src/ for real type definitions.
add_executable(test_proto
    test_proto.cc
)
target_include_directories(test_proto PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_proto PRIVATE doctest)
add_test(NAME test_proto COMMAND test_proto)

# ---- Test: scripts.cc (data structures, accessors, sfallModsIni config) ----
# Links test_sources (config.cc) for sfallModsIni Config integration.
add_executable(test_scripts
    test_scripts.cc
)
target_link_libraries(test_scripts PRIVATE test_sources test_stubs doctest)
add_test(NAME test_scripts COMMAND test_scripts)

# ---- Test: skill.cc (data structures, table integrity, skill max cap) ----
# Self-contained — uses test-local stubs only.
# Needs src/ include for skill_defs.h (Skill enum, SKILL_COUNT constant).
add_executable(test_skill
    test_skill.cc
)
target_include_directories(test_skill PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_skill PRIVATE doctest)
add_test(NAME test_skill COMMAND test_skill)

# ---- Test: sfall_arrays.cc ----
# Links test_sources (sfall_arrays.cc) for array functions.
# Local stubs in test file override engine dependencies.
add_executable(test_sfall_arrays
    test_sfall_arrays.cc
)
target_link_libraries(test_sfall_arrays PRIVATE test_sources test_stubs doctest)
add_test(NAME test_sfall_arrays COMMAND test_sfall_arrays)

# ---- Test: sfall_ini.cc ----
# Links test_sources (sfall_ini.cc) for INI functions.
# Local stubs in test file override engine dependencies.
add_executable(test_sfall_ini
    test_sfall_ini.cc
)
target_link_libraries(test_sfall_ini PRIVATE test_sources test_stubs doctest)
add_test(NAME test_sfall_ini COMMAND test_sfall_ini)

# ---- Test: sfall_config lifecycle (from sfall-infra) ----
# Links test_sources (sfall_config.cc) for sfallConfigInit/Exit.
add_executable(test_sfall_config
    test_sfall_config.cc
)
target_link_libraries(test_sfall_config PRIVATE test_sources test_stubs doctest)
add_test(NAME test_sfall_config COMMAND test_sfall_config)

# ---- Test: perk_tweak.cc (PerksTweak parsing) ----
# Links test_sources (sfall_config.cc + perk_tweak.cc) for PerksFile
# path resolution and gate+clamp parsing semantics.
add_executable(test_perk_tweak
    test_perk_tweak.cc
)
target_link_libraries(test_perk_tweak PRIVATE test_sources test_stubs doctest)
add_test(NAME test_perk_tweak COMMAND test_perk_tweak)

# ---- Test: trait_tweak.cc ([Traits] section parsing) ----
# Links test_sources (sfall_config.cc + perk_tweak.cc + trait_tweak.cc)
# for [Traits] section parsing (NoHardcode/StatMod/SkillMod/overrides).
add_executable(test_trait_tweak
    test_trait_tweak.cc
)
target_link_libraries(test_trait_tweak PRIVATE test_sources test_stubs doctest)
add_test(NAME test_trait_tweak COMMAND test_trait_tweak)

# ---- Test: sfall_opcodes (lifecycle, extern globals) ----
# Extern globals from sfall_opcodes.cc are defined in test_common_stubs.cc.
# Links test_sources for sfall_global_vars.cc (M-092 tests).
add_executable(test_sfall_opcodes
    test_sfall_opcodes.cc
)
target_link_libraries(test_sfall_opcodes PRIVATE test_sources test_stubs doctest)
target_include_directories(test_sfall_opcodes PRIVATE ${ZLIB_INCLUDE_DIRS})
add_test(NAME test_sfall_opcodes COMMAND test_sfall_opcodes)

# ---- Test: sfall metarules (header-level type validation) ----
# Header-level test — does not link sfall_metarules.cc.
# SDL2/zlib headers needed for transitive includes from sfall_metarules.h.
add_executable(test_sfall_metarules
    test_sfall_metarules.cc
)
target_include_directories(test_sfall_metarules PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_metarules PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_metarules PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_metarules COMMAND test_sfall_metarules)

# ---- Test: sfall hooks (extended hook type validation) ----
# Complements test_script_hooks.cc. Header-level test.
add_executable(test_sfall_hooks
    test_sfall_hooks.cc
)
target_include_directories(test_sfall_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_hooks PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_hooks COMMAND test_sfall_hooks)

# ---- Test: window.cc (word wrap, color conversion, enums) ----
# Self-contained — does NOT link test_sources or SDL2.
# window.h removed from test to avoid SDL initialization hang on headless macOS.
# Memory stubs (internal_malloc_safe, etc.) come from test_stubs.
# Enums from window.h defined locally in test_window.cc.
add_executable(test_window
    test_window.cc
)
target_include_directories(test_window PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_window PRIVATE test_stubs doctest)
add_test(NAME test_window COMMAND test_window)

# ---- Test: word_wrap.cc (real production function, H-10 null guard) ----
# word_wrap.cc is a standalone translation unit (font function pointers +
# strlen only) and can be linked directly, unlike window.cc.
add_executable(test_word_wrap
    test_word_wrap.cc
    "${CMAKE_SOURCE_DIR}/src/word_wrap.cc"
)
target_include_directories(test_word_wrap PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_word_wrap PRIVATE doctest)
add_test(NAME test_word_wrap COMMAND test_word_wrap)

# ---- Test: dbox.cc (enum validation, file dialog layout constants) ----
# Header-only test — self-contained with dbox.h constants.
add_executable(test_dbox
    test_dbox.cc
)
target_include_directories(test_dbox PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_dbox PRIVATE doctest)
add_test(NAME test_dbox COMMAND test_dbox)

# ---- Test: dialog.cc (dialog state, color conversion, F-05 fix) ----
# Links test_sources (memory.cc). SDL2/zlib for dialog.h cascade.
add_executable(test_dialog
    test_dialog.cc
)
target_include_directories(test_dialog PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_dialog PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_dialog PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_dialog COMMAND test_dialog)

# ---- Test: worldmap (self-contained data structure tests) ----
# Self-contained test — does not link worldmap.cc (45+ engine deps).
# Needs src/ include for worldmap.h (static_assert cross-checks against
# CAR_FUEL_MAX, ENCOUNTER_FLAG_*, and CityState constants).
add_executable(test_worldmap
    test_worldmap.cc
)
target_include_directories(test_worldmap PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_worldmap PRIVATE doctest)
add_test(NAME test_worldmap COMMAND test_worldmap)

# ---- Test: character_editor.cc (pure logic mirrors) ----
# Self-contained test — does not link character_editor.cc (40+ engine deps).
# Needs src/ include for stat_defs.h (STAT_MAX / stat constants).
add_executable(test_character_editor
    test_character_editor.cc
)
target_include_directories(test_character_editor PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_character_editor PRIVATE doctest)
add_test(NAME test_character_editor COMMAND test_character_editor)

# ---- Test: character_selector.cc (pure logic mirrors, C-07) ----
# Self-contained test — does not link character_selector.cc (heavy rendering
# deps). Mirrors the C-07 render-stats null guards (invalid skill/trait ids are
# skipped, never reaching strcpy with nullptr).
add_executable(test_character_selector
    test_character_selector.cc
)
target_include_directories(test_character_selector PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_character_selector PRIVATE doctest)
add_test(NAME test_character_selector COMMAND test_character_selector)

# ============================================================
# Stage 6 Implementation — New Test Executables (8 entries)
# ============================================================

# ---- Test: loadsave.cc (H-013/H-014/H-015 fork change mirrors) ----
# Self-contained test — does not link loadsave.cc (engine file I/O deps).
add_executable(test_loadsave
    test_loadsave.cc
)
target_include_directories(test_loadsave PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_loadsave PRIVATE doctest)
add_test(NAME test_loadsave COMMAND test_loadsave)

# ---- Test: sfall_animation.cc (M-057: 8-function module mirrors) ----
# Self-contained test — does not link sfall_animation.cc (50+ engine deps).
# SDL2/zlib headers needed for transitive includes from sfall_animation.h.
add_executable(test_sfall_animation
    test_sfall_animation.cc
)
target_include_directories(test_sfall_animation PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_animation PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_animation PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_animation COMMAND test_sfall_animation)

# ---- Test: endgame.cc (N2-H-001/N2-044/N2-045/N2-046 fork change mirrors) ----
# Self-contained test — does not link endgame.cc (40+ engine deps).
# Includes obj_types.h and proto_types.h from src/.
add_executable(test_endgame
    test_endgame.cc
)
target_include_directories(test_endgame PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_endgame PRIVATE doctest)
add_test(NAME test_endgame COMMAND test_endgame)

# ---- Test: pipboy.cc (M-081/N2-026/N2-027 bounds-checking mirrors) ----
# Self-contained test — does not link pipboy.cc (30+ engine deps).
add_executable(test_pipboy
    test_pipboy.cc
)
target_link_libraries(test_pipboy PRIVATE doctest)
add_test(NAME test_pipboy COMMAND test_pipboy)

# ---- Test: game_dialog.cc (N2-001/N2-049 fork change mirrors) ----
# Self-contained test — does not link game_dialog.cc (40+ engine deps).
add_executable(test_game_dialog
    test_game_dialog.cc
)
target_link_libraries(test_game_dialog PRIVATE doctest)
add_test(NAME test_game_dialog COMMAND test_game_dialog)

# ---- Test: game_mouse.cc (M-089/N2-047 fork change mirrors) ----
# Self-contained test — does not link game_mouse.cc (40+ engine deps).
add_executable(test_game_mouse
    test_game_mouse.cc
)
target_link_libraries(test_game_mouse PRIVATE doctest)
add_test(NAME test_game_mouse COMMAND test_game_mouse)

# ---- Test: sound.cc (N2-003 preloadBuffers error-propagation mirrors) ----
# Self-contained test — does not link sound.cc.
add_executable(test_sound
    test_sound.cc
)
target_link_libraries(test_sound PRIVATE doctest)
add_test(NAME test_sound COMMAND test_sound)

# ---- Test: animation.cc (H-009/H-010/N2-018 fork change mirrors) ----
# Self-contained test — does not link animation.cc.
add_executable(test_animation
    test_animation.cc
)
target_link_libraries(test_animation PRIVATE doctest)
add_test(NAME test_animation COMMAND test_animation)

# ---- Test: platform_compat.cc (path traversal security + overflow regression) ----
# Self-contained test — does not link platform_compat.cc.
# Tests compat_path_contains_traversal from platform_compat.cc:446-480.
# Includes Stage 5 overflow regression static_assert guards (F-11).
# Needs src/ include path for file_find.h and platform_compat.h (sizeof checks).
add_executable(test_platform_compat
    test_platform_compat.cc
)
target_include_directories(test_platform_compat PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_platform_compat PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_platform_compat PRIVATE doctest)
add_test(NAME test_platform_compat COMMAND test_platform_compat)

# ---- Test: platform_compat overflow regression (Stage 5: F-01, F-11, F-13) ----
# Self-contained test — mirrors compat_makepath, compat_splitpath, and
# compat_mkdir_recursive from platform_compat.cc to avoid stub conflicts.
# The sizeof static_assert in test_platform_compat.cc catches structural
# regressions (shrinking pattern back to COMPAT_MAX_FNAME).
add_executable(test_platform_compat_overflow
    test_platform_compat_overflow.cc
)
target_link_libraries(test_platform_compat_overflow PRIVATE doctest)
add_test(NAME test_platform_compat_overflow COMMAND test_platform_compat_overflow)

# ============================================================
# Stage 5 Implementation — New Test Executables (4 entries)
# Added by s6-impl-cmake agent (F2-018, F2-019, F2-060, F2-061)
# ============================================================

# ---- Test: sfall hooks register (F2-018: scriptHooksRegister zero tests) ----
# Header-level + behavioral test for scriptHooksRegister() lifecycle.
# Follows test_sfall_hooks pattern: engine headers cascade to SDL2/zlib.
add_executable(test_sfall_hooks_register
    test_sfall_hooks_register.cpp
)
target_include_directories(test_sfall_hooks_register PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_hooks_register PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_hooks_register PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_hooks_register COMMAND test_sfall_hooks_register)

# ---- Test: sfall hook call (F2-019: ScriptHookCall::call() zero tests) ----
# Behavioral test for ScriptHookCall dispatch mechanism.
# Follows test_sfall_hooks pattern: engine headers cascade to SDL2/zlib.
add_executable(test_sfall_hook_call
    test_sfall_hook_call.cpp
)
target_include_directories(test_sfall_hook_call PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_hook_call PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_hook_call PRIVATE test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_hook_call COMMAND test_sfall_hook_call)

# ---- Test: config migration (F2-060: contentConfigMigrateFromSfall zero tests) ----
# Behavioral test for contentConfigMigrateFromSfall covering all 5 guard conditions.
# game_config_migration.cc is in test_sources — links real implementation.
add_executable(test_config_migration
    test_config_migration.cpp
)
target_link_libraries(test_config_migration PRIVATE test_sources test_stubs doctest)
add_test(NAME test_config_migration COMMAND test_config_migration)

# ---- Test: sfall kb helpers (F2-061: sfall_kb_helpers.cc zero tests) ----
# Tests DIK mapping, key simulation, synthetic event queue.
# Links test_sources (sfall_kb_helpers.cc production code).
# SDL2/zlib headers needed for scancode types and transitive includes.
add_executable(test_sfall_kb_helpers
    test_sfall_kb_helpers.cpp
)
target_include_directories(test_sfall_kb_helpers PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_kb_helpers PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_kb_helpers PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_kb_helpers COMMAND test_sfall_kb_helpers)

# ---- Test: loadsave integration (F-055: end-to-end save/load round-trip) ----
# Real file I/O test for save format components. Uses std::fstream temp files.
# Self-contained — does not link loadsave.cc or sfall_global_vars.cc.
add_executable(test_loadsave_integration
    test_loadsave_integration.cpp
)
target_link_libraries(test_loadsave_integration PRIVATE doctest)
add_test(NAME test_loadsave_integration COMMAND test_loadsave_integration)

# ============================================================
# Stage 6 Implementation — Script Test Harness (F2-012)
# ============================================================

# ---- Test: script harness basic (F2-012: script execution test harness) ----
# Tests the mock Program / TestProgram infrastructure that enables
# opcode handler testing without the full game engine.
# Links test_script_harness.cpp (harness implementation),
# test_sources (sfall_global_vars.cc for set/get sfall global opcodes),
# and test_stubs (debugPrint, compat_*, etc.).
# SDL2/zlib headers needed for transitive includes from interpreter.h.
add_executable(test_script_harness_basic
    test_script_harness.cpp
    test_script_harness_basic.cpp
)
target_include_directories(test_script_harness_basic PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_script_harness_basic PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_script_harness_basic PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_script_harness_basic COMMAND test_script_harness_basic)

# ============================================================
# Stage 7 Implementation — OPCODES CORE & EXT Tests (2 entries)
# Added by s7-impl-opcodes-a agent
# ============================================================

# ---- Test: opcodes core & ext (F-063, F-065, F-079, F-082, F-084-F-088) ----
# Comprehensive behavioral tests for opcode findings.
# Links test_sources for sfall_global_vars, test_stubs for extern globals.
# SDL2/zlib headers needed for transitive includes from interpreter.h/perk.h.
add_executable(test_opcodes_core_ext
    test_opcodes_core_ext.cc
)
target_include_directories(test_opcodes_core_ext PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_opcodes_core_ext PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_opcodes_core_ext PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_opcodes_core_ext COMMAND test_opcodes_core_ext)

# ---- Test: opcodes boundary values (F-063, F-079, F-082, F-084-F-088) ----
# Boundary value tests: INT_MIN/INT_MAX, capacity limits, edge-case sweeps.
# Same link dependencies as test_opcodes_core_ext.
add_executable(test_opcodes_boundary
    test_opcodes_boundary.cc
)
target_include_directories(test_opcodes_boundary PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_opcodes_boundary PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_opcodes_boundary PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_opcodes_boundary COMMAND test_opcodes_boundary)

# ============================================================
# Stage 7 Implementation — META Domain Tests (2 entries)
# Added by s7-impl-meta agent
# Covers F-111 through F-115, F2-042, F2-051
# ============================================================

# ---- Test: meta comprehensive (F-111, F-112, F-113, F-114, F-115, F2-042) ----
# Self-contained mirror tests — does not link sfall_metarules.cc or
# sfall_opcodes.cc (50+ engine dependencies each).
add_executable(test_meta_comprehensive
    test_meta_comprehensive.cc
)
target_link_libraries(test_meta_comprehensive PRIVATE doctest)
add_test(NAME test_meta_comprehensive COMMAND test_meta_comprehensive)

# ---- Test: meta integration (F2-051: ListAsArray → sfall_lists_fill) ----
# Self-contained mirror test — does not link sfall_arrays.cc or
# sfall_lists.cc (50+ engine dependencies each).
add_executable(test_meta_integration
    test_meta_integration.cc
)
target_include_directories(test_meta_integration PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_meta_integration PRIVATE doctest)
add_test(NAME test_meta_integration COMMAND test_meta_integration)

# ============================================================
# Stage 7 Implementation — SAVELOAD Domain Tests (3 entries)
# Added by s7-impl-saveload agent
# Covers F-001 through F-013, F2-003 through F2-007
# ============================================================

# ---- Test: saveload roundtrip (F-001, F-003, F-007, F-013) ----
# Self-contained mirror tests — 27-handler round-trip, header identity,
# quick save/load, perk min level key format.
# Does not link loadsave.cc (50+ engine file I/O dependencies).
add_executable(test_saveload_roundtrip
    test_saveload_roundtrip.cc
)
target_link_libraries(test_saveload_roundtrip PRIVATE doctest)
add_test(NAME test_saveload_roundtrip COMMAND test_saveload_roundtrip)

# ---- Test: saveload state (F-002: 97-global sfallOpcodeState round-trip) ----
# Self-contained mirror tests — sfallOpcodeStateSave→Load for all
# categories of globals with key format validation.
add_executable(test_saveload_state
    test_saveload_state.cc
)
target_link_libraries(test_saveload_state PRIVATE doctest)
add_test(NAME test_saveload_state COMMAND test_saveload_state)

# ---- Test: saveload recovery (F-004, F-010, F-011, F-012, F2-004, F2-005, F2-007) ----
# Self-contained mirror tests — error-path recovery, handler failure,
# backup/restore, _isLoadingGame temporal window, sfallgv.sav sub-paths.
add_executable(test_saveload_recovery
    test_saveload_recovery.cc
)
target_include_directories(test_saveload_recovery PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_saveload_recovery PRIVATE doctest)
add_test(NAME test_saveload_recovery COMMAND test_saveload_recovery)

# ============================================================
# Stage 9 Fix — Register Unregistered Test Files (7 entries)
# ============================================================

# ---- Test: state leak comprehensive (C-01: F-043–F-049, F2-016) ----
# Self-contained — provides its own extern globals. Does NOT link
# test_common_stubs.cc or test_sources to avoid duplicate symbol errors.
add_executable(test_state_leak_comprehensive
    test_state_leak_comprehensive.cc
)
target_include_directories(test_state_leak_comprehensive PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_state_leak_comprehensive PRIVATE doctest)
add_test(NAME test_state_leak_comprehensive COMMAND test_state_leak_comprehensive)

# ---- Test: hooks comprehensive (H-02: 74 TEST_CASEs) ----
# Header-level test — validates HookType mechanics, dispatch, handlers.
# Does not link sfall_script_hooks.cc.
# SDL2: sfall_script_hooks.h -> interpreter.h -> game.h -> touch.h -> SDL.h
# (upstream sync2 added game.h to interpreter.h's include chain).
add_executable(test_hooks_comprehensive
    test_hooks_comprehensive.cc
)
target_include_directories(test_hooks_comprehensive PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_hooks_comprehensive PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_hooks_comprehensive COMMAND test_hooks_comprehensive)

# ---- Test: hooks new types (H-02) ----
# Header-level test — validates new hook subtype definitions.
# Does not link sfall_script_hooks.cc.
add_executable(test_hooks_new_types
    test_hooks_new_types.cc
)
target_include_directories(test_hooks_new_types PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_hooks_new_types PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_hooks_new_types COMMAND test_hooks_new_types)

# ---- Test: hooks return values (H-02) ----
# Header-level test — validates return value dispatch and handler chaining.
# Does not link sfall_script_hooks.cc.
add_executable(test_hooks_return_values
    test_hooks_return_values.cc
)
target_include_directories(test_hooks_return_values PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_hooks_return_values PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_hooks_return_values COMMAND test_hooks_return_values)

# ---- Test: opcodes ext animation (H-04) ----
# Self-contained mirror test — extended animation opcodes.
# Does not include any src/ headers.
add_executable(test_opcodes_ext_animation
    test_opcodes_ext_animation.cc
)
target_link_libraries(test_opcodes_ext_animation PRIVATE doctest)
add_test(NAME test_opcodes_ext_animation COMMAND test_opcodes_ext_animation)

# ---- Test: opcodes ext combat (H-04) ----
# Self-contained mirror test — extended combat opcodes.
# Does not include any src/ headers.
add_executable(test_opcodes_ext_combat
    test_opcodes_ext_combat.cc
)
target_link_libraries(test_opcodes_ext_combat PRIVATE doctest)
add_test(NAME test_opcodes_ext_combat COMMAND test_opcodes_ext_combat)

# ---- Test: opcodes ext misc (H-04, M-15) ----
# Integration test — calls sfall_gl_vars_store/fetch (in test_sources).
# Follows test_opcodes_core_ext pattern for link dependencies.
add_executable(test_opcodes_ext_misc
    test_opcodes_ext_misc.cc
)
target_include_directories(test_opcodes_ext_misc PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_opcodes_ext_misc PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_opcodes_ext_misc PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_opcodes_ext_misc COMMAND test_opcodes_ext_misc)

# ============================================================
# Stage 7 Implementation — sfall Callbacks, Ext, Lists (F-060)
# Added by s7-impl-tests agent
# ============================================================

# ---- Test: sfall callbacks (F-060: sfall_callbacks.cc zero tests) ----
# Header-level test — validates function declarations and types.
# Does not link sfall_callbacks.cc (heavy engine deps).
add_executable(test_sfall_callbacks
    test_sfall_callbacks.cc
)
target_include_directories(test_sfall_callbacks PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_sfall_callbacks PRIVATE doctest)
add_test(NAME test_sfall_callbacks COMMAND test_sfall_callbacks)

# ---- Test: sfall ext (F-060: sfall_ext.cc zero tests) ----
# Header-level test — validates function declarations and return types.
# Does not link sfall_ext.cc (heavy engine deps).
add_executable(test_sfall_ext
    test_sfall_ext.cc
)
target_include_directories(test_sfall_ext PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_sfall_ext PRIVATE doctest)
add_test(NAME test_sfall_ext COMMAND test_sfall_ext)

# ---- Test: sfall lists (F-060: sfall_lists.cc zero tests) ----
# Links test_sources (sfall_lists.cc) for list lifecycle functions.
# test_stubs provides object.h/scripts.h engine dependencies.
add_executable(test_sfall_lists
    test_sfall_lists.cc
)
target_include_directories(test_sfall_lists PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_lists PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_lists PRIVATE test_sources test_stubs doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_lists COMMAND test_sfall_lists)

# ---- Test: interpreter fixes (F-07, F-22, F-29, F2-13, F2-15) ----
# Self-contained test — does not link interpreter.cc or interpreter_extra.cc.
# Validates result init, stack balance, null/type checks, heap bounds, and
# wait duration clamping via local mirrors.
add_executable(test_interpreter_fixes
    test_interpreter_fixes.cc
)
target_link_libraries(test_interpreter_fixes PRIVATE doctest)
add_test(NAME test_interpreter_fixes COMMAND test_interpreter_fixes)

# ============================================================
# Stage 6 Implementation — Sfall Hook Fix Validation Tests
# F-01, F-27, F-48, F2-06, F-38
# ============================================================

# ---- Test: sfall fixes (F-01, F-27, F-48, F2-06, F-38) ----
# Self-contained header-level test — does not link sfall_script_hooks.cc
# or sfall_opcodes.cc. Validates clamping logic, fseek error checking,
# VFS sandbox resolution, and FID_TYPE validation via local mirrors.
# Needs src/ include for combat_defs.h, obj_types.h, sfall_script_hooks.h.
add_executable(test_sfall_fixes
    test_sfall_fixes.cc
)
target_include_directories(test_sfall_fixes PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_sfall_fixes PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_sfall_fixes PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_sfall_fixes COMMAND test_sfall_fixes)

# ---- Test: validation fixes (F-04, F-46, F-23, F2-11, F2-12, F-37) ----
# Self-contained header-level test — validates sscanf checks, bounds checks,
# CRC handling, and null guards via local mirrors.
add_executable(test_validation_fixes
    test_validation_fixes.cc
)
target_link_libraries(test_validation_fixes PRIVATE doctest)
add_test(NAME test_validation_fixes COMMAND test_validation_fixes)

# ============================================================
# Stage 9 Fix — Register Stage 9 Test Files (4 entries)
# ============================================================

# ---- Test: fixes metarules (F-26, F-27, F-28, F-29, F-54) ----
# Self-contained mirror test — does not link sfall_metarules.cc.
# No engine headers needed — all type constants mirrored locally.
add_executable(test_fixes_metarules
    test_fixes_metarules.cc
)
target_link_libraries(test_fixes_metarules PRIVATE doctest)
add_test(NAME test_fixes_metarules COMMAND test_fixes_metarules)

# ---- Test: fixes opcodes (F-09, F2-05, F2-06, F2-08, F2-16) ----
# Self-contained mirror test — includes sfall_opcodes.h and obj_types.h
# for type constants (OBJ_TYPE_CRITTER, FID_TYPE, etc.).
# SDL2/zlib headers needed for transitive includes from interpreter.h/perk.h.
add_executable(test_fixes_opcodes
    test_fixes_opcodes.cc
)
target_include_directories(test_fixes_opcodes PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_fixes_opcodes PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_fixes_opcodes PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_fixes_opcodes COMMAND test_fixes_opcodes)

# ---- Test: fixes hooks (F-21, F-42, F2-02, F2-04) ----
# Self-contained mirror test — includes sfall_script_hooks.h and obj_types.h
# for HookType enum, HOOK_COUNT, OBJ_TYPE_*, DAM_DEAD, PID_TYPE constants.
# SDL2/zlib headers needed for transitive includes from interpreter.h/combat.h.
add_executable(test_fixes_hooks
    test_fixes_hooks.cc
)
target_include_directories(test_fixes_hooks PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_fixes_hooks PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_fixes_hooks PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_fixes_hooks COMMAND test_fixes_hooks)

# ---- Test: window guards (UF-H-025, UF-H-026, UF-H-028 through UF-H-034) ----
# Self-contained mirror test — does not link window.cc (40+ engine deps).
# Verifies all guarded script window functions return gracefully at index -1.
add_executable(test_window_guards
    test_window_guards.cc
)
target_link_libraries(test_window_guards PRIVATE doctest)
add_test(NAME test_window_guards COMMAND test_window_guards)

# ---- Test: fixes saveload (F-55, F-61, F2-20, F2-21, F2-22, F2-25) ----
# Self-contained mirror test — does not link loadsave.cc or sfall_global_scripts.cc.
# No engine headers needed — CRC-32, atomic write FSM, path traversal, and
# procCount validation are all mirrored locally.
add_executable(test_fixes_saveload
    test_fixes_saveload.cc
)
target_link_libraries(test_fixes_saveload PRIVATE doctest)
add_test(NAME test_fixes_saveload COMMAND test_fixes_saveload)

# ============================================================
# Stage 10 Test-Add — New Fix-Validation Test Executables (4 entries)
# ============================================================

# ---- Test: fixes stage6 arithmetic (interpreter INT_MIN/float-to-int/NaN UB) ----
# Self-contained mirror test — does not link interpreter.cc, sfall_opcodes.cc,
# or sfall_metarules.cc (50+ engine deps each).
# Covers F-M015/M016/M017/M018/M019/M020, I2-M001/M002/M003/M016/M019/M021,
# F-06/F-07/F-17: INT_MIN guards, float-to-int UB, NaN UB, kill counter overflow.
add_executable(test_fixes_stage6_arithmetic
    test_fixes_stage6_arithmetic.cc
)
target_link_libraries(test_fixes_stage6_arithmetic PRIVATE doctest)
add_test(NAME test_fixes_stage6_arithmetic COMMAND test_fixes_stage6_arithmetic)

# ---- Test: fixes stage6 combat (killType OOB, gBlockCombat, knockback, team validation) ----
# Self-contained mirror test — does not link combat.cc or combat_ai.cc (40+ engine deps).
# Covers I2-H002, I2-M035/M036/M045/M046, F-M043/M044/M045, F-H025, F-03/F-04/F-05.
add_executable(test_fixes_stage6_combat
    test_fixes_stage6_combat.cc
)
target_link_libraries(test_fixes_stage6_combat PRIVATE doctest)
add_test(NAME test_fixes_stage6_combat COMMAND test_fixes_stage6_combat)

# ---- Test: fixes stage6 saveload (write ordering, CRC init, backup handling) ----
# Self-contained mirror test — does not link loadsave.cc or scripts.cc (engine I/O deps).
# Covers I2-M028/M029/M031/M033/M034, F-M041/M042/M060.
add_executable(test_fixes_stage6_saveload
    test_fixes_stage6_saveload.cc
)
target_link_libraries(test_fixes_stage6_saveload PRIVATE doctest)
add_test(NAME test_fixes_stage6_saveload COMMAND test_fixes_stage6_saveload)

# ---- Test: fixes stage6 misc (interpreter_lib, dialog, arrays, hooks, scripts, metarules) ----
# Self-contained mirror test — does not link production .cc files (50+ engine deps).
# Covers F-M011/M013/M014/M022/M023/M039/M040/M054/M057/M058/M059, I2-M011/M012/
# M013/M014/M015/M017/M018/M022/M024/M026/M027/M053, F-14.
add_executable(test_fixes_stage6_misc
    test_fixes_stage6_misc.cc
)
target_link_libraries(test_fixes_stage6_misc PRIVATE doctest)
add_test(NAME test_fixes_stage6_misc COMMAND test_fixes_stage6_misc)

# ---- Test: fixes stage6 misc audio/movie/lips/party/pipboy/object/mouse/kb (s6-fix-misc) ----
# Self-contained mirror test — does not link production .cc files (50+ engine deps).
# Covers M-76..M-79, M-129..M-139, M-146, M-155, M-156, M-158, M-179..M-181,
# M-183, M-186, M-191, M-193, M-196..M-198.
add_executable(test_fixes_stage6_misc_av
    test_fixes_stage6_misc_av.cc
)
target_link_libraries(test_fixes_stage6_misc_av PRIVATE doctest)
add_test(NAME test_fixes_stage6_misc_av COMMAND test_fixes_stage6_misc_av)

# ============================================================
# Stage 4 Implementation — RENDER + SCANNER Fix Tests (1 entry)
# ============================================================

# ---- Test: render scanner fixes (UF-H-047 + UF-011) ----
# Self-contained mirror test — does not link draw.cc or scanner sources
# (50+ engine deps). Validates zero-guard pattern in blit stretch functions
# and hook implementation filter table.
# SDL2/zlib headers needed for transitive includes from sfall_script_hooks.h.
add_executable(test_render_scanner_fixes
    test_render_scanner_fixes.cc
)
target_include_directories(test_render_scanner_fixes PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_render_scanner_fixes PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_render_scanner_fixes PRIVATE doctest ${SDL2_LIBRARIES})
add_test(NAME test_render_scanner_fixes COMMAND test_render_scanner_fixes)

# ============================================================
# Stage 4 Audio — Audio Fix Regression Tests (3 entries)
# ============================================================

# ---- Test: audio subsystem fixes (UF-006, UF-007, UF-H-037) ----
# Self-contained test — does not link audio.cc, sound_decoder.cc,
# sound_effects_cache.cc, or sound_effects_list.cc.
# Validates forward seek arithmetic, null-decoder error propagation,
# and odd-size buffer bounds via local mirrors.
add_executable(test_audio_fixes
    test_audio_fixes.cc
)
target_link_libraries(test_audio_fixes PRIVATE doctest)
add_test(NAME test_audio_fixes COMMAND test_audio_fixes)

# ============================================================
# Stage 4 MISC UI/CONFIG — Fix Regression Tests (1 entry)
# ============================================================

# ---- Test: misc UI/config fixes (UF-H-015/H-016, UF-H-020, UF-H-039, UF-H-043) ----
# Self-contained mirror test — does not link inventory.cc, stat.cc, movie.cc,
# or config.cc (50+ engine deps). The configGetInt base=10 test uses a local
# strtol-based mirror to validate the behavioral change without linking config.cc.
# Covers: barter bounds offset, bonus stat clamping, movie error codes,
# configGetInt base=10 octal-parse fix.
add_executable(test_misc_ui_config_fixes
    test_misc_ui_config_fixes.cc
)
target_link_libraries(test_misc_ui_config_fixes PRIVATE doctest)
add_test(NAME test_misc_ui_config_fixes COMMAND test_misc_ui_config_fixes)

# ============================================================
# Stage 4 Game Core — Fix Regression Tests (1 entry)
# Covers: UF-H-008, UF-H-014, UF-H-017, UF-H-018, UF-004, UF-010, UF-H-044
# ============================================================

# ---- Test: game core fixes (null guards, bounds checks, error propagation) ----
# Self-contained mirror test — does not link interpreter_extra.cc, combat_ai.cc,
# item.cc, proto.cc, or actions.cc (50+ engine deps each).
add_executable(test_game_core_fixes
    test_game_core_fixes.cc
)
target_link_libraries(test_game_core_fixes PRIVATE doctest)
add_test(NAME test_game_core_fixes COMMAND test_game_core_fixes)

# ============================================================
# Stage 4 sfall Compat Fixes — New Test Executable (1 entry)
# Covers UF-H-035 (rest mode translation), UF-H-050 (VK mapping),
# UF-H-022 (NPC trait removal).
# ============================================================

# ---- Test: sfall compat fixes (UF-H-035, UF-H-050, UF-H-022) ----
# Self-contained mirror test — does not link sfall_metarules.cc or
# sfall_kb_helpers.cc (50+ engine dependencies each).
add_executable(test_sfall_compat_fixes
    test_sfall_compat_fixes.cc
)
target_link_libraries(test_sfall_compat_fixes PRIVATE doctest)
add_test(NAME test_sfall_compat_fixes COMMAND test_sfall_compat_fixes)

# ============================================================
# Stage 6 Art/IO/Heap/Version Fix — Regression Tests (1 entry)
# ============================================================

# ---- Test: art/IO/heap/version fixes (H-16, art NEW-01, H-17, P-16, M-160,
# art N-1, M-168, M-169, M-187, M-188, M-202) ----
# Self-contained mirror test — does not link pcx.cc/dfile.cc/art.cc/
# heap.cc/version.cc/file_utils.cc/dialog.cc (50+ engine deps each).
# Validates the dimension guards, DAT accounting-field validation, heap
# negative-size rejection, version_string format-string safety, fwrite
# error propagation, and dialog slot selection via local mirrors.
add_executable(test_art_io_fixes
    test_art_io_fixes.cc
)
target_link_libraries(test_art_io_fixes PRIVATE doctest)
add_test(NAME test_art_io_fixes COMMAND test_art_io_fixes)

# ============================================================
# RPU Automap Capacity Fix — Regression Tests (1 entry)
# AUTOMAP_MAP_COUNT raised 160 -> 173 (remaining-work item 4, P2)
# ============================================================

# ---- Test: automap (RPU automap capacity: count, header dataSize, guards) ----
# Self-contained mirror test — does not link automap.cc (35+ engine deps).
# Includes automap.h/map_defs.h for the REAL constants (compile-time
# static_assert pins AUTOMAP_MAP_COUNT == 173, derived dataSize == 2081,
# last valid index 172) and mirrors automapMapIndexIsValid/_automapDisplayMap/
# automapSetDisplayMap and the pipboy mapCount clamp logic.
# automap.h -> db.h -> xfile.h -> zlib.h only; no SDL2 or test_stubs needed.
add_executable(test_automap
    test_automap.cc
)
target_include_directories(test_automap PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(test_automap PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(test_automap PRIVATE doctest)
add_test(NAME test_automap COMMAND test_automap)

# ---- Test: FemaleDialogMsgs directory selection (RPU P3, 2026-08-17) ----
# Links test_sources (sfall_config.cc for gFemaleDialogMsgs) + test_stubs and
# compiles message.cc directly into this target (messageListGetLocalizedDir is
# the function under test; message.cc is NOT part of test_sources because its
# proto_types/settings/random dependency chain would ripple into every test
# target that links test_sources). The test file provides local stubs for the
# message.cc symbols test_common_stubs does not define (fileReadChar, fileSeek,
# fileTell, compat_strupr, randomBetween).
add_executable(test_message_female_dialog
    test_message_female_dialog.cc
    "${CMAKE_SOURCE_DIR}/src/message.cc"
)
target_link_libraries(test_message_female_dialog PRIVATE test_sources test_stubs doctest)
add_test(NAME test_message_female_dialog COMMAND test_message_female_dialog)

# ---- Test: global script frame-interval timers (PRODUCTION-LINKED) ----
# Links test_sources, so sfall_global_scripts.cc is the real implementation
# (tests/CMakeLists.txt:34) — not a mirror. The test file supplies strong
# overrides for the five weak stubs in test_common_stubs.cc
# (fileNameListInit, fileOpen, programCreateByPath, programFindProcedure,
# programExecuteProcedure) so the global-script registry can be populated
# without game data. Pins sfall_gl_scr_set_repeat / _set_type frame counting,
# F-M040's counter reset, the F-058 game_loaded() tri-state and the F-21
# SCRIPT_PROC_NO_PROC sentinel.
add_executable(test_global_scripts_timers
    test_global_scripts_timers.cc
)
target_link_libraries(test_global_scripts_timers PRIVATE test_sources test_stubs doctest)
add_test(NAME test_global_scripts_timers COMMAND test_global_scripts_timers)
