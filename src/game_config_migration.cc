#include "game_config_migration.h"

#include <algorithm>
#include <assert.h>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>

#include "content_config.h"
#include "debug.h"
#include "game_config.h"
#include "game_movie.h"
#include "platform_compat.h"
#include "settings.h"
#include "sfall_config.h"

namespace fallout {

namespace {

#define F2_RES_CONFIG_FILE_NAME "f2_res.ini"
#define F2_RES_MAIN_MENU_BUTTON_X 30
#define F2_RES_MAIN_MENU_BUTTON_Y 19
#define F2_RES_DEFAULT_MAIN_MENU_PANEL_OFFSET_X 16
#define F2_RES_DEFAULT_MAIN_MENU_PANEL_OFFSET_Y 15

    struct F2ResMigrationEntry {
        const char* legacySection;
        const char* legacyKey;
        const char* targetSection;
        const char* targetKey;
    };

    static bool gameConfigHasKey(Config* config, const char* section, const char* key);
    static bool gameConfigNeedsF2ResMigration(Config* gameConfig);
    static bool gameConfigMigrateMainMenuScaleModeKey(Config* legacyConfig, Config* gameConfig);
    static bool gameConfigMigrateStringKey(Config* legacyConfig, Config* gameConfig, const F2ResMigrationEntry& entry);
    static bool gameConfigMigrateScaleKey(Config* legacyConfig, Config* gameConfig);
    static bool contentConfigMigrateF2ResMainMenuPanelOffsetKey(Config* legacyConfig, Config* contentConfig, const char* legacyKey, const char* targetKey, int baseValue, int defaultValue);
    static bool contentConfigMigrateF2ResBoolKey(Config* legacyConfig, Config* contentConfig, const char* legacyKey, const char* targetKey);

    static constexpr F2ResMigrationEntry kF2ResMigrationEntries[] = {
        { "MAIN", "SCR_WIDTH", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY },
        { "MAIN", "SCR_HEIGHT", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_Y_KEY },
        { "MAIN", "WINDOWED", GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_WINDOWED_KEY },
        { "MAIN", "f2_res_dat", GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_F2_RES_DAT_KEY },
        { "IFACE", "IFACE_BAR_MODE", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_MODE_KEY },
        { "IFACE", "IFACE_BAR_WIDTH", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_WIDTH_KEY },
        { "IFACE", "IFACE_BAR_SIDE_ART", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDE_ART_KEY },
        { "IFACE", "IFACE_BAR_SIDES_ORI", GAME_CONFIG_UI_KEY, GAME_CONFIG_IFACE_BAR_SIDES_ORI_KEY },
        // P-18: EDGE_CLIPPING_ON is the sibling of IGNORE_MAP_EDGES — both
        // are read by the HRP edge subsystem (map_edge.cc:399-403 consults
        // settings.ui.edg_support AND settings.ui.ignore_map_edges). The
        // old table migrated IGNORE_MAP_EDGES but not EDGE_CLIPPING_ON, so
        // a migrated HRP user who disabled edge clipping (EDGE_CLIPPING_ON=0)
        // silently got edge loading re-enabled (edg_support defaults true).
        { "MAPS", "EDGE_CLIPPING_ON", GAME_CONFIG_UI_KEY, "edg_support" },
        { "MAPS", "IGNORE_PLAYER_SCROLL_LIMITS", GAME_CONFIG_UI_KEY, GAME_CONFIG_IGNORE_SCROLL_LIMIT_KEY },
        { "IFACE", "ALTERNATE_AMMO_METRE", GAME_CONFIG_UI_KEY, GAME_CONFIG_ALTERNATE_AMMO_METER_KEY },
        { "MAPS", "IGNORE_MAP_EDGES", GAME_CONFIG_UI_KEY, GAME_CONFIG_IGNORE_MAP_EDGES_KEY },
        { "STATIC_SCREENS", "SPLASH_SCRN_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_SPLASH_SCREEN_SIZE_KEY },
        { "MOVIES", "MOVIE_SIZE", GAME_CONFIG_UI_KEY, GAME_CONFIG_MOVIE_ASPECT_FIT_KEY },
    };

    static bool gameConfigHasKey(Config* config, const char* section, const char* key)
    {
        char* value;
        return configGetString(config, section, key, &value);
    }

    static bool gameConfigNeedsF2ResMigration(Config* gameConfig)
    {
        assert(gameConfig != nullptr);

        return !gameConfigHasKey(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_RESOLUTION_X_KEY);
    }

    static bool gameConfigMigrateStringKey(Config* legacyConfig, Config* gameConfig, const F2ResMigrationEntry& entry)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, entry.targetSection, entry.targetKey)) {
            return false;
        }

        char* value;
        if (!configGetString(legacyConfig, entry.legacySection, entry.legacyKey, &value)) {
            return false;
        }

        return configSetString(gameConfig, entry.targetSection, entry.targetKey, value);
    }

    static bool gameConfigMigrateMainMenuScaleModeKey(Config* legacyConfig, Config* gameConfig)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, GAME_CONFIG_UI_KEY, GAME_CONFIG_MAIN_MENU_SCALE_MODE_KEY)) {
            return false;
        }

        int scaleMode;
        if (!configGetInt(legacyConfig, "MAINMENU", "MAIN_MENU_SIZE", &scaleMode)) {
            return false;
        }

        bool legacyScaleButtonsAndText = false;
        if (configGetBool(legacyConfig, "MAINMENU", "SCALE_BUTTONS_AND_TEXT_MENU", &legacyScaleButtonsAndText)
            && legacyScaleButtonsAndText
            && scaleMode != 0) {
            scaleMode = 2;
        }

        return configSetInt(gameConfig, GAME_CONFIG_UI_KEY, GAME_CONFIG_MAIN_MENU_SCALE_MODE_KEY, scaleMode);
    }

    static bool gameConfigMigrateScaleKey(Config* legacyConfig, Config* gameConfig)
    {
        assert(legacyConfig != nullptr && gameConfig != nullptr);

        if (gameConfigHasKey(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_SCALE_KEY)) {
            return false;
        }

        int value;
        if (!configGetInt(legacyConfig, "MAIN", "SCALE_2X", &value)) {
            return false;
        }

        return configSetInt(gameConfig, GAME_CONFIG_SCREEN_KEY, GAME_CONFIG_SCALE_KEY, value + 1);
    }

    static bool contentConfigMigrateF2ResMainMenuPanelOffsetKey(Config* legacyConfig, Config* contentConfig, const char* legacyKey, const char* targetKey, int baseValue, int defaultValue)
    {
        assert(legacyConfig != nullptr && contentConfig != nullptr);

        if (gameConfigHasKey(contentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, targetKey)) {
            return false;
        }

        int value;
        if (!configGetInt(legacyConfig, "MAINMENU", legacyKey, &value)) {
            return false;
        }

        value += baseValue;
        if (value == defaultValue) {
            return false;
        }

        return configSetInt(contentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, targetKey, value);
    }

    static bool contentConfigMigrateF2ResBoolKey(Config* legacyConfig, Config* contentConfig, const char* legacyKey, const char* targetKey)
    {
        assert(legacyConfig != nullptr && contentConfig != nullptr);

        if (gameConfigHasKey(contentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, targetKey)) {
            return false;
        }

        bool value;
        if (!configGetBool(legacyConfig, "MAINMENU", legacyKey, &value)) {
            return false;
        }

        return configSetBool(contentConfig, CONTENT_CONFIG_MAIN_MENU_SECTION, targetKey, value);
    }
} // namespace

// Migrate settings F2_RES.INI to fallout2.cfg
//
// Only happens a single time, after which fallout2.cfg is the source of truth
bool gameConfigMigrateFromF2Res(const char* gameConfigFilePath, Config* gameConfig)
{
    if (gameConfigFilePath == nullptr || gameConfig == nullptr) {
        return false;
    }

    if (!gameConfigNeedsF2ResMigration(gameConfig)) {
        return false;
    }

    char f2ResFilePath[COMPAT_MAX_PATH];
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(gameConfigFilePath, drive, dir, nullptr, nullptr);
    compat_makepath(f2ResFilePath, drive, dir, F2_RES_CONFIG_FILE_NAME, nullptr);

    Config legacyConfig;
    if (!configInit(&legacyConfig)) {
        return false;
    }

    bool migrated = false;
    if (configRead(&legacyConfig, f2ResFilePath, false)) {
        for (const auto& entry : kF2ResMigrationEntries) {
            if (gameConfigMigrateStringKey(&legacyConfig, gameConfig, entry)) {
                migrated = true;
            }
        }

        if (gameConfigMigrateMainMenuScaleModeKey(&legacyConfig, gameConfig)) {
            migrated = true;
        }

        if (gameConfigMigrateScaleKey(&legacyConfig, gameConfig)) {
            migrated = true;
        }
    }

    configFree(&legacyConfig);
    return migrated;
}

namespace {

    constexpr char kSfallMisc[] = "Misc";
    constexpr char kSfallInterface[] = "Interface";
    constexpr char kSfallSound[] = "Sound";
    constexpr char kSfallDebugging[] = "Debugging";

    struct SfallMigrationEntry {
        const char* sfallSection;
        const char* sfallKey;
        const char* targetSection;
        const char* targetKey;
        // If the sfall value matches this string, skip migration (value is already the default).
        // nullptr means always migrate when the key is present.
        const char* defaultValue;
    };

    constexpr SfallMigrationEntry kSfallMigrationEntries[] = {
        // [start]
        { kSfallMisc, "StartingMap", CONTENT_CONFIG_START_SECTION, "map", "" },
        { kSfallMisc, "ViewXPos", CONTENT_CONFIG_START_SECTION, "worldmap_view_x", "-1" },
        { kSfallMisc, "ViewYPos", CONTENT_CONFIG_START_SECTION, "worldmap_view_y", "-1" },
        { kSfallMisc, "MaleStartModel", CONTENT_CONFIG_START_SECTION, "model_male", "hmwarr" },
        { kSfallMisc, "MaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_male_default", "hmjmps" },
        { kSfallMisc, "FemaleStartModel", CONTENT_CONFIG_START_SECTION, "model_female", "hfprim" },
        { kSfallMisc, "FemaleDefaultModel", CONTENT_CONFIG_START_SECTION, "model_female_default", "hfjmps" },
        { kSfallMisc, "PipBoyAvailableAtGameStart", CONTENT_CONFIG_START_SECTION, "pipboy", "0" },
        { kSfallMisc, "Fallout1Behavior", CONTENT_CONFIG_START_SECTION, "fallout1_behavior", "0" },
        // F-04: UseFileSystemOverride migration — eliminates first-launch RPU friction.
        // The feature itself is intentionally unwired in CE (VFS handles it),
        // but the migration entry prevents RPU scripts from incorrectly detecting
        // a missing config (they check != 1, and absence returns 0/false).
        { kSfallMisc, "UseFileSystemOverride", CONTENT_CONFIG_START_SECTION, "use_filesystem_override", "0" },
        // et tu startup gate: AllowUnsafeScripting (ddraw.ini [Debugging]) —
        // bridges to [start] allow_unsafe_scripting so the script-visible
        // get_ini_setting value can be seeded without ddraw.ini keys
        // (SFALL_COMPATIBILITY.md Et Tu remaining-work item 1).
        { kSfallDebugging, "AllowUnsafeScripting", CONTENT_CONFIG_START_SECTION, "allow_unsafe_scripting", "0" },
        // [maps]
        { kSfallMisc, "DisableSpecialMapIDs", CONTENT_CONFIG_MAPS_SECTION, "disable_special_map_ids", "0" },
        // [karma]
        { kSfallMisc, "KarmaFRMs", CONTENT_CONFIG_KARMA_SECTION, "frms" },
        { kSfallMisc, "KarmaPoints", CONTENT_CONFIG_KARMA_SECTION, "points" },
        // [dialog]
        { kSfallMisc, "DialogueFix", CONTENT_CONFIG_DIALOG_SECTION, "no_exit_hotkey", "1" },
        { kSfallMisc, "DialogGenderWords", CONTENT_CONFIG_DIALOG_SECTION, "gender_words", "0" },
        { kSfallMisc, "StartGDialogFix", CONTENT_CONFIG_DIALOG_SECTION, "start_gdialog_fix", "0" },
        // [main_menu]
        { kSfallMisc, "VersionString", CONTENT_CONFIG_MAIN_MENU_SECTION, "version_string" },
        { kSfallMisc, "MainMenuFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "font_color", "0" },
        { kSfallMisc, "MainMenuBigFontColour", CONTENT_CONFIG_MAIN_MENU_SECTION, "big_font_color", "0" },
        { kSfallMisc, "MainMenuOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_x", "0" },
        { kSfallMisc, "MainMenuOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "offset_y", "0" },
        { kSfallMisc, "MainMenuCreditsOffsetX", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_x", "0" },
        { kSfallMisc, "MainMenuCreditsOffsetY", CONTENT_CONFIG_MAIN_MENU_SECTION, "credits_offset_y", "0" },
        // [sound]
        { kSfallSound, "MainMenuMusic", CONTENT_CONFIG_SOUND_SECTION, "main_menu_music", "07desert" },
        { kSfallSound, "WorldMapMusic", CONTENT_CONFIG_SOUND_SECTION, "worldmap_music", "23world" },
        { kSfallSound, "WorldMapCarMusic", CONTENT_CONFIG_SOUND_SECTION, "worldmap_car_music", "20car" },
        { kSfallSound, "EndGameMovieMusic0", CONTENT_CONFIG_SOUND_SECTION, "endgame_movie_music0", "akiss" },
        { kSfallSound, "EndGameMovieMusic1", CONTENT_CONFIG_SOUND_SECTION, "endgame_movie_music1", "10labone" },
        { kSfallSound, "MapLoadingSound", CONTENT_CONFIG_SOUND_SECTION, "map_loading_sound", "wind2" },
        // [movies]
        { kSfallMisc, "MovieTimer_artimer1", CONTENT_CONFIG_MOVIES_SECTION, "artimer1", "90" },
        { kSfallMisc, "MovieTimer_artimer2", CONTENT_CONFIG_MOVIES_SECTION, "artimer2", "180" },
        { kSfallMisc, "MovieTimer_artimer3", CONTENT_CONFIG_MOVIES_SECTION, "artimer3", "270" },
        { kSfallMisc, "MovieTimer_artimer4", CONTENT_CONFIG_MOVIES_SECTION, "artimer4", "360" },
        // [combat]
        { kSfallMisc, "DamageFormula", CONTENT_CONFIG_COMBAT_SECTION, "damage_formula", "0" },
        { kSfallMisc, "BonusHtHDamageFix", CONTENT_CONFIG_COMBAT_SECTION, "bonus_hth_damage_fix", "1" },
        { kSfallMisc, "RemoveCriticalTimelimits", CONTENT_CONFIG_COMBAT_SECTION, "remove_critical_time_limits", "0" },
        { kSfallMisc, "ScienceOnCritters", CONTENT_CONFIG_COMBAT_SECTION, "science_on_critters", "0" },
        { kSfallMisc, "CheckWeaponAmmoCost", CONTENT_CONFIG_COMBAT_SECTION, "check_weapon_ammo_cost", nullptr },
        { kSfallMisc, "InventoryApCost", CONTENT_CONFIG_COMBAT_SECTION, "inventory_ap_cost", "4" },
        { kSfallMisc, "QuickPocketsApCostReduction", CONTENT_CONFIG_COMBAT_SECTION, "quick_pockets_ap_cost_reduction", "2" },
        { kSfallMisc, "ComputeSprayMod", CONTENT_CONFIG_COMBAT_SECTION, "burst_enabled", "0" },
        { kSfallMisc, "ComputeSpray_CenterMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_mult", "1" },
        { kSfallMisc, "ComputeSpray_CenterDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_center_div", "3" },
        { kSfallMisc, "ComputeSpray_TargetMult", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_mult", "1" },
        { kSfallMisc, "ComputeSpray_TargetDiv", CONTENT_CONFIG_COMBAT_SECTION, "burst_target_div", "2" },
        // [explosions]
        { kSfallMisc, "ExplosionsEmitLight", CONTENT_CONFIG_EXPLOSIONS_SECTION, "emit_light", "0" },
        { kSfallMisc, "Dynamite_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_max", "50" },
        { kSfallMisc, "Dynamite_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "dynamite_min", "30" },
        { kSfallMisc, "PlasticExplosive_DmgMax", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_max", "80" },
        { kSfallMisc, "PlasticExplosive_DmgMin", CONTENT_CONFIG_EXPLOSIONS_SECTION, "plastic_explosive_min", "40" },
        // [skilldex]
        { kSfallMisc, "Lockpick", CONTENT_CONFIG_SKILLDEX_SECTION, "lockpick", "293" },
        { kSfallMisc, "Steal", CONTENT_CONFIG_SKILLDEX_SECTION, "steal", "293" },
        { kSfallMisc, "Traps", CONTENT_CONFIG_SKILLDEX_SECTION, "traps", "293" },
        { kSfallMisc, "FirstAid", CONTENT_CONFIG_SKILLDEX_SECTION, "first_aid", "293" },
        { kSfallMisc, "Doctor", CONTENT_CONFIG_SKILLDEX_SECTION, "doctor", "293" },
        { kSfallMisc, "Science", CONTENT_CONFIG_SKILLDEX_SECTION, "science", "293" },
        { kSfallMisc, "Repair", CONTENT_CONFIG_SKILLDEX_SECTION, "repair", "293" },
        // [worldmap]
        { kSfallMisc, "TownMapHotkeysFix", CONTENT_CONFIG_WORLDMAP_SECTION, "town_map_hotkeys_fix", "1" },
        { kSfallMisc, "DisableHorrigan", CONTENT_CONFIG_WORLDMAP_SECTION, "disable_horrigan", "0" },
        { kSfallMisc, "CityRepsList", CONTENT_CONFIG_WORLDMAP_SECTION, "city_reputation_list" },
        { kSfallInterface, "WorldMapTravelMarkers", CONTENT_CONFIG_WORLDMAP_SECTION, "trail_markers", "0" },
        { kSfallMisc, "StartXPos", CONTENT_CONFIG_WORLDMAP_SECTION, "start_x_pos" },
        { kSfallMisc, "StartYPos", CONTENT_CONFIG_WORLDMAP_SECTION, "start_y_pos" },
        // P-19: ViewXPos/ViewYPos/WorldMapSlots migration rows removed —
        // the targets (worldmap view_x_pos/view_y_pos/encounter_slots) have
        // zero consumers anywhere in src/ (verified by grep); the sfall
        // starting world-map viewport patch and the WorldMapSlots feature
        // (cities-list scroll height) are unimplemented. encounter_slots was
        // the structural root of H-06 (the config bridge returned a value
        // for an inert feature). WorldMapSlots is now served by the
        // gSfallConfig default of 21 (H-06) through the op_get_ini_setting
        // fallback tier. ElevatorsFile remains — elevator.cc reads it from
        // gSfallConfig and the bridge keeps the value script-accessible.
        { kSfallMisc, "ElevatorsFile", CONTENT_CONFIG_WORLDMAP_SECTION, "elevators_file" },

        // BoxBarCount migration intentionally removed — `add_iface_tag` metarule provides
        // equivalent functionality and 5 pre-allocated tag slots match the sfall baseline.
        // There is no engine-level consumer for BoxBarCount in CE.

        { kSfallMisc, "BoostScriptDialogLimit", CONTENT_CONFIG_DIALOG_SECTION, "boost_dialog_limit", "0" },
        { kSfallInterface, "WorldMapTerrainInfo", CONTENT_CONFIG_WORLDMAP_SECTION, "terrain_info", "0" },
        // [characters]
        { kSfallMisc, "PremadePaths", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_paths" },
        { kSfallMisc, "PremadeFIDs", CONTENT_CONFIG_CHARACTERS_SECTION, "premade_fids" },
        // [text]
        { kSfallMisc, "ExtraGameMsgFileList", CONTENT_CONFIG_TEXT_SECTION, "extra_msg_file_list" },
        // [stats]
        { kSfallMisc, "XPTable", CONTENT_CONFIG_STATS_SECTION, "xp_table" },
    };

// SYNC WARNING: kSfallMigrationEntries MUST be kept synchronized with
// kSfallContentMappings in content_config.cc (same ddraw.ini keys
// covering the same target sections). When adding or removing entries
// here, update kSfallContentMappings and kSfallMigrationEntryCount
// in game_config_migration.h to match.
static_assert(std::size(kSfallMigrationEntries) == kSfallMigrationEntryCount,
    "kSfallMigrationEntries entry count does not match kSfallMigrationEntryCount; "
    "update BOTH tables in game_config_migration.cc and content_config.cc");

// Compile-time verification: kSfallMigrationEntries must contain no duplicate
// (sfallSection, sfallKey) pairs. A duplicate entry means one of the tables
// (migration or content mapping) has a stale copy-paste error that the
// count-only static_assert above cannot detect.
constexpr bool sfallMigrationEntriesNoDuplicates()
{
    for (size_t i = 0; i < std::size(kSfallMigrationEntries); ++i) {
        std::string_view iSection(kSfallMigrationEntries[i].sfallSection);
        std::string_view iKey(kSfallMigrationEntries[i].sfallKey);
        for (size_t j = i + 1; j < std::size(kSfallMigrationEntries); ++j) {
            std::string_view jSection(kSfallMigrationEntries[j].sfallSection);
            std::string_view jKey(kSfallMigrationEntries[j].sfallKey);
            if (iSection == jSection && iKey == jKey) {
                return false;
            }
        }
    }
    return true;
}

static_assert(sfallMigrationEntriesNoDuplicates(),
    "kSfallMigrationEntries contains duplicate (sfallSection, sfallKey) pairs. "
    "Remove the duplicate entry and verify kSfallContentMappings in content_config.cc.");

// 68ff38e: migrate sfall [Misc] Movie1..Movie32 overrides to game.cfg
// [movies] movie1..movie32. Default file names are not migrated (the
// [movies] defaults in game.cfg already cover them).
    static bool contentConfigMigrateSfallMovieOverrides(Config* sfallConfig, Config* migratedConfig)
    {
        assert(sfallConfig != nullptr && migratedConfig != nullptr);

        bool migrated = false;
        for (int index = 0; index < GAME_MOVIE_MAX_COUNT; index++) {
            char sfallKey[16];
            snprintf(sfallKey, sizeof(sfallKey), "Movie%d", index + 1);

            char* value;
            if (!configGetString(sfallConfig, kSfallMisc, sfallKey, &value) || value[0] == '\0') {
                continue;
            }

            const char* defaultFileName = gameMovieGetDefaultFileName(index);
            if (defaultFileName != nullptr && strcmp(value, defaultFileName) == 0) {
                continue;
            }

            char targetKey[16];
            snprintf(targetKey, sizeof(targetKey), "movie%d", index + 1);

            if (gameConfigHasKey(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, targetKey)) {
                continue;
            }

            configSetString(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, targetKey, value);
            migrated = true;
        }

        return migrated;
    }

    // 97f9a3b: when sfall Fallout1Behavior is enabled, migrate the FO1
    // endgame movie configuration (skip the auto movie after slideshow;
    // male movie 10, female movie 11).
    static bool contentConfigMigrateSfallFallout1MovieBehavior(Config* sfallConfig, Config* migratedConfig)
    {
        assert(sfallConfig != nullptr && migratedConfig != nullptr);

        bool enabled = false;
        if (!configGetBool(sfallConfig, kSfallMisc, "Fallout1Behavior", &enabled) || !enabled) {
            return false;
        }

        bool migrated = false;
        if (!gameConfigHasKey(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_play_after_slideshow")) {
            configSetInt(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_play_after_slideshow", 0);
            migrated = true;
        }

        if (!gameConfigHasKey(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_movie_male")) {
            configSetInt(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_movie_male", 10);
            migrated = true;
        }

        if (!gameConfigHasKey(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_movie_female")) {
            configSetInt(migratedConfig, CONTENT_CONFIG_MOVIES_SECTION, "endgame_movie_female", 11);
            migrated = true;
        }

        return migrated;
    }

} // anonymous namespace

// Migrate sfall settings from ddraw.ini to game.cfg.
//
// Re-migrates on each run: loads the existing patch file (if any) and updates
// only the migration entries whose values have changed in ddraw.ini. This
// prevents stale migrated values from overriding user ddraw.ini changes.
// Non-migration keys in the patch file are preserved.
static bool contentConfigMigrateFromSfall(Config* sfallConfig, const char* contentConfigFilePath)
{
    assert(sfallConfig != nullptr && contentConfigFilePath != nullptr);

    Config migratedConfig;
    if (!configInit(&migratedConfig)) {
        return false;
    }

    // Load the existing patch file to preserve non-migration keys and detect
    // which migration entries need updating. Returns false if the file does
    // not exist (first run) — that's fine; we proceed with an empty config.
    configRead(&migratedConfig, contentConfigFilePath, false);

    bool migrated = false;

    // 30bf0c3: migrate sfall WorldMapFPSPatch + WorldMapDelay2 to game.cfg
    // [worldmap] travel_delay. Only migrates when WorldMapFPSPatch is
    // enabled (sfall semantics: it is a boolean enable).
    {
        bool worldMapFpsPatch = false;
        if (configGetBool(sfallConfig, kSfallMisc, "WorldMapFPSPatch", &worldMapFpsPatch) && worldMapFpsPatch) {
            int travelDelay = 66;
            configGetInt(sfallConfig, kSfallMisc, "WorldMapDelay2", &travelDelay);
            travelDelay = std::clamp(travelDelay, 1, 150);
            configSetInt(&migratedConfig, CONTENT_CONFIG_WORLDMAP_SECTION, "travel_delay", travelDelay);
            migrated = true;
        }
    }

    // 97f9a3b: when sfall Fallout1Behavior is enabled, migrate the FO1
    // endgame movie sequence (skip auto movie after slideshow; male movie 10,
    // female movie 11). CE does not provide a single Fallout1Behavior switch,
    // so only the movie portion is configured here.
    if (contentConfigMigrateSfallFallout1MovieBehavior(sfallConfig, &migratedConfig)) {
        migrated = true;
    }

    // Migrate start year/month/day only when explicitly set (not the sfall -1 sentinel).
    // M-38: Do NOT skip values that equal defaultValue — matching the UH-05
    // behavior of the main loop below. The old `value != defaultValue` guard
    // meant reverting ddraw.ini StartYear back to the CE default (2241) — or
    // deleting the key — never propagated: a stale non-default value stayed in
    // game#patch.cfg forever. The comparison against existingValue below still
    // avoids rewriting the patch file when nothing changed.
    auto migrateStartInt = [&](const char* sfallKey, const char* targetKey, int defaultValue) {
        int value;
        if (configGetInt(sfallConfig, "Misc", sfallKey, &value) && value >= 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", value);
            char* existingValue;
            if (!configGetString(&migratedConfig, CONTENT_CONFIG_START_SECTION, targetKey, &existingValue)
                || strcmp(existingValue, buf) != 0) {
                configSetString(&migratedConfig, CONTENT_CONFIG_START_SECTION, targetKey, buf);
                migrated = true;
            }
        }
    };
    migrateStartInt("StartYear", "year", 2241);
    migrateStartInt("StartMonth", "month", 6);
    migrateStartInt("StartDay", "day", 24);
    migrateStartInt("StartTime", "time", 824);

    for (const auto& entry : kSfallMigrationEntries) {
        char* value;
        if (configGetString(sfallConfig, entry.sfallSection, entry.sfallKey, &value)) {
            // UH-05: Skip only truly empty sfall values. Do NOT skip values
            // that match entry.defaultValue — a prior migration may have left
            // a non-default value in the content config that needs to be
            // reverted when the user changes ddraw.ini back to the default.
            // The comparison against existingValue below handles that correctly.
            if (value[0] == '\0') {
                continue;
            }
            // Only update if the value in the patch file differs from the
            // current ddraw.ini value. This avoids unnecessary writes and
            // preserves the patch file when nothing has changed.
            char* existingValue;
            if (!configGetString(&migratedConfig, entry.targetSection, entry.targetKey, &existingValue)
                || strcmp(existingValue, value) != 0) {
                configSetString(&migratedConfig, entry.targetSection, entry.targetKey, value);
                migrated = true;
            }
        }
    }

    // 68ff38e: sfall Movie1..Movie32 overrides are per-slot with no 1:1 table
    // entry (the default names must not be migrated), so they get their own
    // migration pass.
    if (contentConfigMigrateSfallMovieOverrides(sfallConfig, &migratedConfig)) {
        migrated = true;
    }

    // M-199: Migrate ddraw.ini [Misc] SkipOpeningMovies → fallout2.cfg
    // [ui] skip_opening_movies. This key cannot go through
    // kSfallMigrationEntries: it lives in the SETTINGS config (fallout2.cfg,
    // read by main.cc:93 / game.cc:202 via settings.ui.skip_opening_movies),
    // not in game.cfg — a table entry would write a [ui] section that no
    // consumer reads. SFALL_COMPATIBILITY.md:29 documents the manual move;
    // this makes it automatic. The live settings struct is updated too so the
    // current session sees the value (settingsInit already ran before
    // contentConfigInit).
    {
        char* skipOpeningMoviesValue = nullptr;
        if (configGetString(sfallConfig, kSfallMisc, "SkipOpeningMovies", &skipOpeningMoviesValue)
            && skipOpeningMoviesValue != nullptr && skipOpeningMoviesValue[0] != '\0') {
            int skipValue = atoi(skipOpeningMoviesValue);
            int existingSkip = -1;
            // Persist to fallout2.cfg (gGameConfig) only when the value differs,
            // avoiding file churn on every launch.
            if (!configGetInt(&gGameConfig, GAME_CONFIG_UI_KEY, "skip_opening_movies", &existingSkip)
                || existingSkip != skipValue) {
                configSetInt(&gGameConfig, GAME_CONFIG_UI_KEY, "skip_opening_movies", skipValue);
                if (gGameConfigInitialized && gGameConfigFilePath[0] != '\0') {
                    if (!configWriteEx(&gGameConfig, gGameConfigFilePath, CONFIG_RETAIN_ALL)) {
                        debugPrint("Failed to write SkipOpeningMovies migration to %s!\n", gGameConfigFilePath);
                    }
                }
            }
            // Update the live setting for the current session.
            settings.ui.skip_opening_movies = skipValue;
        }
    }

    if (contentConfigMigrateSfallMovieOverrides(sfallConfig, &migratedConfig)) {
        migrated = true;
    }

    if (contentConfigMigrateSfallFallout1MovieBehavior(sfallConfig, &migratedConfig)) {
        migrated = true;
    }

    if (migrated) {
        // Ensure all directory components exist before writing.
        char drive[COMPAT_MAX_DRIVE];
        char dirPart[COMPAT_MAX_DIR];
        char pathWithoutFile[COMPAT_MAX_PATH];
        compat_splitpath(contentConfigFilePath, drive, dirPart, nullptr, nullptr);
        compat_makepath(pathWithoutFile, drive, dirPart, nullptr, nullptr);
        compat_mkdir_recursive(pathWithoutFile);

        if (!configWrite(&migratedConfig, contentConfigFilePath, false)) {
            debugPrint("Failed to write migrated settings to %s!\n", contentConfigFilePath);
            migrated = false;
        }
    }

    configFree(&migratedConfig);
    return migrated;
}

static bool contentConfigEnsureDirectory(const char* contentConfigFilePath)
{
    char drive[COMPAT_MAX_DRIVE];
    char dirPart[COMPAT_MAX_DIR];
    char pathWithoutFile[COMPAT_MAX_PATH];
    compat_splitpath(contentConfigFilePath, drive, dirPart, nullptr, nullptr);
    compat_makepath(pathWithoutFile, drive, dirPart, nullptr, nullptr);
    return compat_is_dir(pathWithoutFile)
        || compat_mkdir_recursive(pathWithoutFile) == 0
        || compat_is_dir(pathWithoutFile);
}

static bool contentConfigMigrateFromF2Res(Config* legacyConfig, const char* contentConfigFilePath)
{
    assert(legacyConfig != nullptr && contentConfigFilePath != nullptr);

    Config migratedConfig;
    if (!configInit(&migratedConfig)) {
        return false;
    }

    configRead(&migratedConfig, contentConfigFilePath, false);

    bool migrated = false;
    if (contentConfigMigrateF2ResMainMenuPanelOffsetKey(legacyConfig, &migratedConfig, "MENU_BG_OFFSET_X", "main_menu_panel_offset_x", F2_RES_MAIN_MENU_BUTTON_X, F2_RES_DEFAULT_MAIN_MENU_PANEL_OFFSET_X)) {
        migrated = true;
    }
    if (contentConfigMigrateF2ResMainMenuPanelOffsetKey(legacyConfig, &migratedConfig, "MENU_BG_OFFSET_Y", "main_menu_panel_offset_y", F2_RES_MAIN_MENU_BUTTON_Y, F2_RES_DEFAULT_MAIN_MENU_PANEL_OFFSET_Y)) {
        migrated = true;
    }
    if (contentConfigMigrateF2ResBoolKey(legacyConfig, &migratedConfig, "SCALE_BUTTONS_AND_TEXT_MENU", "scale_buttons_and_text")) {
        migrated = true;
    }

    if (migrated) {
        if (contentConfigEnsureDirectory(contentConfigFilePath)) {
            if (!configWriteEx(&migratedConfig, contentConfigFilePath, CONFIG_RETAIN_ALL)) {
                debugPrint("Failed to write migrated settings to %s!\n", contentConfigFilePath);
            }
        } else {
            debugPrint("Failed to create directory for migrated settings at %s!\n", contentConfigFilePath);
        }
    }

    configFree(&migratedConfig);
    return migrated;
}

void contentConfigTryMigrateFromF2Res(const char* contentConfigPath)
{
    const auto& masterPatches = settings.system.master_patches_path;
    if (masterPatches.empty()) {
        debugPrint("Failed to migrate from f2_res.ini: no master_patches is set.\n");
        return;
    }
    if (!compat_is_dir(masterPatches.c_str())) {
        return;
    }

    char f2ResFilePath[COMPAT_MAX_PATH];
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(gGameConfigFilePath, drive, dir, nullptr, nullptr);
    compat_makepath(f2ResFilePath, drive, dir, F2_RES_CONFIG_FILE_NAME, nullptr);

    Config legacyConfig;
    if (!configInit(&legacyConfig)) {
        return;
    }

    if (configRead(&legacyConfig, f2ResFilePath, false)) {
        char contentCfgPath[COMPAT_MAX_PATH];
        snprintf(contentCfgPath, sizeof(contentCfgPath), "%s\\%s", masterPatches.c_str(), contentConfigPath);
        if (contentConfigMigrateFromF2Res(&legacyConfig, contentCfgPath)) {
            debugPrint("Migrated settings from f2_res.ini to %s.\n", contentCfgPath);
        }
    }

    configFree(&legacyConfig);
}

void contentConfigTryMigrateFromSfall(const char* contentConfigPath)
{
    if (!gSfallConfig.isInitialized() || gSfallConfig.entriesLength == 0) {
        debugPrint("Skipping ddraw.ini migration: sfall config not initialized or empty.\n");
        // Nothing to migrate.
        return;
    }
    const auto& masterPatches = settings.system.master_patches_path;
    if (masterPatches.empty()) {
        debugPrint("Failed to migrate from ddraw.ini: no master_patches is set.\n");
        return;
    }
    if (!compat_is_dir(masterPatches.c_str())) {
        // master_patches must point to an existing folder. Don't migrate when it's missing or not a directory.
        debugPrint("Skipping ddraw.ini migration: master_patches \"%s\" is not a directory.\n", masterPatches.c_str());
        return;
    }
    char contentCfgPath[COMPAT_MAX_PATH];
    int pathResult = snprintf(contentCfgPath, sizeof(contentCfgPath), "%s\\%s", masterPatches.c_str(), contentConfigPath);
    if (pathResult < 0 || pathResult >= (int)sizeof(contentCfgPath)) {
        debugPrint("Failed to construct content config path: path too long.\n");
        return;
    }
    if (contentConfigMigrateFromSfall(&gSfallConfig, contentCfgPath)) {
        debugPrint("Migrated settings from ddraw.ini to game.cfg.\n");
    }
}

} // namespace fallout
