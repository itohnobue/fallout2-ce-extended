// Stubs for symbols needed by source files under test.
// These provide minimal no-op implementations so tests can link
// without pulling in the entire engine dependency graph.
//
// Functions that are NOT tested (e.g. file I/O) return failure values.
// The actual unit tests exercise only the in-memory data structure operations.

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "animation.h"
#include "config.h"
#include "db.h"
#include "debug.h"
#include "game_config.h"
#include "game_movie.h"
#include "input.h"
#include "interpreter.h"
#include "object.h"
#include "scripts.h"
#include "sfall_ext.h"
#include "stat_defs.h"
#include "platform_compat.h"
#include "settings.h"
#include "sfall_script_hooks.h"

// Forward declarations for engine globals used by sfall_kb_helpers.cc.
// We cannot include svga.h or game.h here because test_stubs does not
// have SDL2 include paths. The actual type definitions are not needed
// for the variable definitions below.
struct SDL_Window;

namespace fallout {

// =============================================================
// debug.h stubs
// =============================================================

// memory.cc calls debugPrint for error messages; no-op for tests.
int debugPrint(const char* /*format*/, ...)
{
    return 0;
}

// =============================================================
// platform_compat.h stubs
// =============================================================

// dictionary.cc uses compat_stricmp for case-insensitive binary search.
int compat_stricmp(const char* s1, const char* s2)
{
    while (*s1 && *s2) {
        int c1 = static_cast<unsigned char>(*s1);
        int c2 = static_cast<unsigned char>(*s2);
        if (c1 >= 'A' && c1 <= 'Z')
            c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 += 'a' - 'A';
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    }
    return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
}

// config.cc uses compat_itoa for int-to-string conversion (configSetInt).
char* compat_itoa(int value, char* buffer, int /*radix*/)
{
    snprintf(buffer, 20, "%d", value);
    return buffer;
}

// config.cc uses compat_fopen for configRead/configWrite file I/O.
// Stub: always returns nullptr (file not found).
FILE* compat_fopen(const char* /*path*/, const char* /*mode*/)
{
    return nullptr;
}

// config.cc and dictionary.cc use compat_fgets.
// Stub: always returns nullptr (EOF/error).
char* compat_fgets(char* /*buffer*/, int /*maxCount*/, FILE* /*stream*/)
{
    return nullptr;
}

// config.cc uses compat_remove for configWriteSideBySide.
int compat_remove(const char* /*path*/)
{
    return -1;
}

// config.cc uses compat_rename for configWriteSideBySide.
int compat_rename(const char* /*oldName*/, const char* /*newName*/)
{
    return -1;
}

// game_config_migration.cc uses compat_splitpath to decompose paths.
void compat_splitpath(const char* /*path*/, char* drive, char* dir,
                      char* fname, char* ext)
{
    // Stub: zero-initialize output buffers to produce empty strings
    // for all path components. Matches the real implementation's contract
    // of producing null-terminated empty strings when a component is absent.
    // See platform_compat.cc:71-140 for the real implementation.
    if (drive != nullptr) drive[0] = '\0';
    if (dir != nullptr) dir[0] = '\0';
    if (fname != nullptr) fname[0] = '\0';
    if (ext != nullptr) ext[0] = '\0';
}

// game_config_migration.cc uses compat_makepath to construct paths.
void compat_makepath(char* path, const char* /*drive*/, const char* /*dir*/,
                     const char* /*fname*/, const char* /*ext*/)
{
    // Stub: zero-initialize the output path to produce an empty string.
    // See platform_compat.cc:142-220 for the real implementation.
    if (path != nullptr) path[0] = '\0';
}

// game_config_migration.cc uses compat_file_exists to check for existing configs.
bool compat_file_exists(const char* /*filePath*/)
{
    return false;
}

// game_config_migration.cc uses compat_mkdir_recursive for output directory creation.
int compat_mkdir_recursive(const char* /*path*/)
{
    return -1;
}

// game_config_migration.cc uses compat_is_dir to validate paths.
bool compat_is_dir(const char* /*path*/)
{
    return false;
}

// sfall_ini.cc uses compat_path_contains_traversal to validate filenames
// before INI lookups (sfall_ini.cc:75). Return false (no traversal detected)
// so that test cases can exercise the INI parsing path without being blocked
// by traversal rejection. The real traversal logic is tested separately by
// test_platform_compat.cc.
bool compat_path_contains_traversal(const char* path)
{
    (void)path;
    return false;
}

// =============================================================
// db.h / xfile.h stubs (File I/O)
// =============================================================

// config.cc uses fileOpen for DAT-based config reading.
File* fileOpen(const char* /*filename*/, const char* /*mode*/)
{
    return nullptr;
}

// config.cc uses fileClose.
// __attribute__((weak)) allows test files (e.g. test_sfall_arrays.cc,
// test_global_vars.cc) to override these stubs with mock-backed versions.
__attribute__((weak)) int fileClose(File* /*stream*/)
{
    return -1;
}

// sfall_global_vars.cc uses fileRead for save/load.
__attribute__((weak)) size_t fileRead(void* /*buf*/, size_t /*size*/, size_t /*count*/, File* /*stream*/)
{
    return 0;
}

// sfall_global_vars.cc uses fileWrite for save.
__attribute__((weak)) size_t fileWrite(const void* /*buf*/, size_t /*size*/, size_t /*count*/, File* /*stream*/)
{
    return 0;
}

// config.cc uses fileReadString for configRead.
char* fileReadString(char* /*str*/, size_t /*size*/, File* /*stream*/)
{
    return nullptr;
}

// config.cc uses filePrintFormatted for configWriteDb.
int filePrintFormatted(File* /*stream*/, const char* /*format*/, ...)
{
    return 0;
}

// sfall_global_scripts.cc uses fileNameListInit / fileNameListFree
// to discover script files on disk. Return 0 files found.
int fileNameListInit(const char* /*pattern*/, char*** /*fileNames*/)
{
    return 0;
}

void fileNameListFree(char*** /*fileNames*/, int /*unused*/)
{
}

// game_config_migration.cc's contentConfigMigrateSfallMovieOverrides calls
// gameMovieGetDefaultFileName to skip migrating sfall MovieN overrides that
// match the engine default. game_movie.cc is not linked into the test build,
// so provide a stub that reports no known defaults (nullptr for every movie).
const char* gameMovieGetDefaultFileName(int /*movie*/)
{
    return nullptr;
}

} // namespace fallout

// =============================================================
// interpreter.h stubs (inside namespace fallout)
// =============================================================

// sfall_opcodes test calls hookOpcodeGetCurrentCall, which calls
// programPrintError when no ScriptHookCall is active.
namespace fallout {
    void programPrintError(const char* /*format*/, ...)
    {
        // no-op: tests don't validate error message content
    }

    // sfall_global_scripts.cc calls programInterpret to run loaded scripts.
    void programInterpret(Program* /*program*/, int /*numInstructions*/)
    {
        // no-op: script execution requires the full engine runtime.
    }

    // sfall_global_scripts.cc calls programCreateByPath / programFree
    // to load compiled scripts from disk and free them.
    Program* programCreateByPath(const char* /*path*/)
    {
        return nullptr;  // no scripts available in test context
    }

    void programFree(Program* /*program*/)
    {
        // no-op: test context allocates no program resources.
    }

    // sfall_global_scripts.cc calls programFindProcedure to look up
    // script procedures (start, map_enter, etc.) by name.
    int programFindProcedure(Program* /*prg*/, const char* /*name*/)
    {
        return -1;  // procedure not found
    }

    // sfall_global_scripts.cc calls programGetIdentifier to resolve
    // string identifiers in script bytecode.
    char* programGetIdentifier(Program* /*program*/, int /*offset*/)
    {
        return nullptr;
    }

    // sfall_global_scripts.cc calls programExecuteProcedure to run
    // a procedure (e.g. "start") on a loaded program.
    void programExecuteProcedure(Program* /*program*/, int /*procedureIndex*/)
    {
        // no-op: script execution requires the full engine runtime.
    }

    // sfall_global_scripts.cc calls stackReadInt32 to read the
    // procedure name offset from bytecode data.
    int stackReadInt32(unsigned char* /*data*/, int /*pos*/)
    {
        return 0;
    }
} // namespace fallout

// =============================================================
// sfall_script_hooks.h stubs
// =============================================================

// test_sfall_opcodes calls hookOpcodeGetCurrentCall, which calls
// ScriptHookCall::current(). In unit test context there is no active
// hook call, so return nullptr.
namespace fallout {
    ScriptHookCall* ScriptHookCall::current()
    {
        return nullptr;
    }

    // sfall_kb_helpers.cc constructs ScriptHookCall objects in
    // sfall_kb_handle_key_pressed. Provide minimal stubs.
    ScriptHookCall::ScriptHookCall(HookType /*hookType*/, int /*maxReturnValues*/,
                                   std::initializer_list<ProgramValue> /*args*/)
    {
        // no-op: test context has no hook scripts to dispatch
    }

    void ScriptHookCall::call()
    {
        // no-op: test context has no registered hook scripts
    }

    int ScriptHookCall::numReturnValues() const
    {
        return 0;  // no scripts ran, no return values
    }

    ProgramValue ScriptHookCall::getReturnValueAt(int /*idx*/) const
    {
        return ProgramValue();  // empty ProgramValue
    }
} // namespace fallout

// ---- Global variables defined by the engine ----
// game_config_migration.cc references Settings settings and gSfallConfig.
// NOTE: gSfallConfig is defined in sfall_config.cc (in test_sources).
// Do NOT redefine here — causes duplicate symbol when linking test_sources.
namespace fallout {
    Settings settings;
    // Config gSfallConfig; — MOVED to sfall_config.cc (via test_sources)

    // game_config_migration.cc (M-199 SkipOpeningMovies migration) references
    // the fallout2.cfg globals; game_config.cc is not linked in tests.
    bool gGameConfigInitialized = false;
    Config gGameConfig;
    char gGameConfigFilePath[COMPAT_MAX_PATH] = "";

    // sfall_opcodes.cc extern globals — defined here so test_sfall_opcodes
    // can be self-contained without linking sfall_opcodes.cc (150+ engine deps).
    int gPerkFrequencyOverride = 0;
    int gSkillPointsPerLevelMod = 0;
    int gLastAttacker = -1;
    int gLastTarget = -1;
    int gSkillMaxCap = 300;
    int gXpModPercentage = 100;

    // sfall_opcodes.cc knockback globals (F-004) — defined here so
    // test_sfall_opcodes can reference them without linking sfall_opcodes.cc.
    int sfallWeaponKnockbackType = 0;
    float sfallWeaponKnockbackValue = 0.0f;
    int sfallTargetKnockbackType = 0;
    float sfallTargetKnockbackValue = 0.0f;
    int sfallAttackerKnockbackType = 0;
    float sfallAttackerKnockbackValue = 0.0f;

    // sfall_kb_helpers.cc references these engine globals.
    // gSdlWindow: SDL window handle (null in test context).
    // gGameLoaded: whether a game is actively loaded (false in test context).
    SDL_Window* gSdlWindow = nullptr;
    bool gGameLoaded = false;
}

// =============================================================
// memory_manager.h stubs (safe allocation wrappers)
// =============================================================
// test_window.cc uses the _safe variants that include file/line
// tracking. Minimal forwarding stubs.

namespace fallout {
    void* internal_malloc_safe(size_t size, const char* /*file*/, int /*line*/)
    {
        return malloc(size);
    }

    void* internal_realloc_safe(void* ptr, size_t size, const char* /*file*/, int /*line*/)
    {
        return realloc(ptr, size);
    }

    void internal_free_safe(void* ptr, const char* /*file*/, int /*line*/)
    {
        free(ptr);
    }
} // namespace fallout

// =============================================================
// interpreter.h stubs — ProgramValue (used by sfall_arrays.cc)
// =============================================================
namespace fallout {

    ProgramValue::ProgramValue()
        : opcode(VALUE_TYPE_INT), integerValue(0) {}

    ProgramValue::ProgramValue(int value)
        : opcode(VALUE_TYPE_INT), integerValue(value) {}

    ProgramValue::ProgramValue(unsigned int value)
        : opcode(VALUE_TYPE_INT), integerValue(static_cast<int>(value)) {}

    ProgramValue::ProgramValue(bool value)
        : opcode(VALUE_TYPE_INT), integerValue(value ? 1 : 0) {}

    ProgramValue::ProgramValue(float value)
        : opcode(VALUE_TYPE_FLOAT), floatValue(value) {}

    ProgramValue::ProgramValue(Object* value)
        : opcode(VALUE_TYPE_PTR), pointerValue(value) {}

    ProgramValue::ProgramValue(Attack* value)
        : opcode(VALUE_TYPE_PTR), pointerValue(value) {}

    ProgramValue::ProgramValue(const char* value)
        : opcode(VALUE_TYPE_STRING), pointerValue(const_cast<char*>(value))
    {
        integerValue = -1; // sentinel
    }

    bool ProgramValue::isEmpty() const {
        switch (opcode) {
        case VALUE_TYPE_INT:
            return integerValue == 0;
        case VALUE_TYPE_FLOAT:
            return floatValue == 0.0f;
        case VALUE_TYPE_STRING:
        case VALUE_TYPE_DYNAMIC_STRING:
            return integerValue == 0;
        default:
            return integerValue == 0;
        }
    }
    bool ProgramValue::isInt() const { return opcode == VALUE_TYPE_INT; }
    bool ProgramValue::isFloat() const { return opcode == VALUE_TYPE_FLOAT; }
    bool ProgramValue::isString() const { return opcode == VALUE_TYPE_STRING || opcode == VALUE_TYPE_DYNAMIC_STRING; }
    float ProgramValue::asFloat() const {
        switch (opcode) {
        case VALUE_TYPE_INT:
            return static_cast<float>(integerValue);
        case VALUE_TYPE_FLOAT:
            return floatValue;
        default:
            return 0.0f;
        }
    }
    bool ProgramValue::isPointer() const { return opcode == VALUE_TYPE_PTR; }
    int ProgramValue::asInt() const { return integerValue; }
    Object* ProgramValue::asObject() const {
        if (opcode == VALUE_TYPE_PTR) return static_cast<Object*>(pointerValue);
        return nullptr;
    }
    const char* ProgramValue::asString(Program*) const { return nullptr; }
    const char* ProgramValue::typeDebugString() const { return "UNKNOWN"; }

} // namespace fallout

// =============================================================
// stat.h stubs
// =============================================================
namespace fallout {

    // Test-local mirror for stat max/min values.
    // Production stores in gStatDescriptions[].maximumValue (file-static in stat.cc).
    // This array enables set/get round-trips in unit tests.
    static int gTestStatMaxValues[STAT_COUNT] = {};
    static int gTestStatMinValues[STAT_COUNT] = {};

    void statSetMaxValue(int stat, int value)
    {
        if (stat >= 0 && stat < STAT_COUNT) {
            gTestStatMaxValues[stat] = value;
        }
    }

    void statSetMinValue(int stat, int value)
    {
        if (stat >= 0 && stat < STAT_COUNT) {
            gTestStatMinValues[stat] = value;
        }
    }

    int statGetMaxValue(int stat)
    {
        if (stat >= 0 && stat < STAT_COUNT) {
            return gTestStatMaxValues[stat];
        }
        return -1;  // invalid stat
    }

    int statGetMinValue(int stat)
    {
        if (stat >= 0 && stat < STAT_COUNT) {
            return gTestStatMinValues[stat];
        }
        return -1;  // invalid stat
    }

    int perkGetMaxRank(int /*perk*/)
    {
        return -1;  // conservative: "perk has no ranks" in pre-init state
    }

} // namespace fallout

// =============================================================
// sfall_opcodes.h stubs — lifecycle functions
// test_sfall_opcodes.cc calls these but we no longer link
// sfall_opcodes.cc (it has 150+ engine deps).
// =============================================================
namespace fallout {

    // Animation callback globals — defined as extern in sfall_opcodes.h.
    // The real definitions live in sfall_opcodes.cc but that file has 150+
    // engine dependencies, so we provide test-safe definitions here.
    Program* sfallAnimCallbackProgram = nullptr;
    int sfallAnimCallbackProcedureIndex = -1;

    void sfallVfsCloseAll()
    {
        // No-op: VFS handle slots are not available in test context.
    }

    void sfallAnimCallbackReset()
    {
        // Resets animation callback state to defaults.
        sfallAnimCallbackProgram = nullptr;
        sfallAnimCallbackProcedureIndex = -1;
    }

    // Hit-chance globals — defined as extern in sfall_opcodes.h.
    // The real definitions live in sfall_opcodes.cc.
    int sfallHitChanceMod = 0;
    int sfallHitChanceMax = 95;

    int sfallGetPerkLevelMod() { return 0; }
    int sfallGetHpPerLevelMod() { return 0; }
    int sfallGetPyromaniacMod() { return 0; }
    int sfallGetSwiftLearnerMod() { return 0; }

    bool sfallGetCritterHitChanceMod(Object* /*critter*/, int& /*outMod*/, int& /*outMax*/)
    {
        return false;  // no per-critter override in test context
    }

    // programStackPopValue / programStackPushValue — called by ArrayFilter
    // and ArrayTransform in sfall_arrays.cc. Provide minimal no-op stubs.
    // Also used by OpcodeContext::pushReturnValue in sfall_ini.cc.
    ProgramValue programStackPopValue(Program* /*program*/)
    {
        return ProgramValue();  // empty value
    }

    void programStackPushValue(Program* /*program*/, const ProgramValue& /*value*/)
    {
        // no-op: test context has no interpreter stack
    }

    void sfallOpcodesReset()
    {
        // Reset extern globals to their documented defaults.
        // Matches sfall_opcodes.cc lines ~3460-3475.
        gPerkFrequencyOverride = 0;
        gSkillPointsPerLevelMod = 0;
        gLastAttacker = -1;
        gLastTarget = -1;
        gSkillMaxCap = 300;
        gXpModPercentage = 100;

        // Reset knockback globals (F-004).
        sfallWeaponKnockbackType = 0;
        sfallWeaponKnockbackValue = 0.0f;
        sfallTargetKnockbackType = 0;
        sfallTargetKnockbackValue = 0.0f;
        sfallAttackerKnockbackType = 0;
        sfallAttackerKnockbackValue = 0.0f;

        // Reset hit-chance globals (F-20).
        sfallHitChanceMod = 0;
        sfallHitChanceMax = 95;
    }

    void sfallOpcodesExit()
    {
        // Matches sfall_opcodes.cc lines ~4163-4167.
        sfallAnimCallbackReset();
        sfallVfsCloseAll();
    }

    ScriptHookCall* hookOpcodeGetCurrentCall(const char* /*opcodeName*/)
    {
        // In test context there is no active hook call — return nullptr.
        return nullptr;
    }

int compat_strnicmp(const char* /*string1*/, const char* /*string2*/, size_t /*size*/) { return -1; }

// =============================================================
// input.h stubs — ticker system
// =============================================================
// sfall_global_scripts.cc registers/unregisters a ticker callback
// (sfall_gl_scr_process_input) via tickersAdd / tickersRemove.
// The ticker system requires the full engine main loop; no-op in tests.

void tickersAdd(TickerProc* /*fn*/)
{
}

void tickersRemove(TickerProc* /*fn*/)
{
}

// =============================================================
// scripts.h stubs — spatial script iteration
// =============================================================
// sfall_lists.cc iterates spatial scripts to build lists of nearby
// objects. Return nullptr to indicate no scripts/objects available.
// The real engine populates these from the loaded map.

Object* scriptGetSelf(Program* /*program*/)
{
    return nullptr;
}

Script* scriptGetFirstSpatialScript(int /*elevation*/)
{
    return nullptr;
}

Script* scriptGetNextSpatialScript()
{
    return nullptr;
}

// =============================================================
// scripts.h stubs — script list enumeration
// =============================================================
// sfall_global_scripts.cc's sfall_gl_scr_is_game_script iterates the
// loaded scripts list. scripts.cc is not linked into the test build,
// so provide a stub reporting an empty list (length 0 — the caller's
// loop never iterates).
int scriptsGetListLength()
{
    return 0;
}

int scriptsGetFileName(int /*scriptIndex*/, char* name, size_t size)
{
    if (size != 0) {
        name[0] = '\0';
    }
    return -1;
}

// =============================================================
// object.h stubs — object iteration
// =============================================================
// sfall_lists.cc iterates all objects via objectFindFirst / objectFindNext
// to build object lists. Return nullptr to indicate no objects.
// The real engine populates these from the loaded map.

Object* objectFindFirst()
{
    return nullptr;
}

Object* objectFindNext()
{
    return nullptr;
}

// =============================================================
// sfall_ext.h stubs — global script path helpers
// =============================================================
// sfall_global_scripts.cc calls sfallGetGlobalScriptPaths and
// sfallGetHookScriptsPath to discover script files on disk.
// Return empty containers in test context.

const std::vector<std::string>& sfallGetGlobalScriptPaths()
{
    static const std::vector<std::string> empty;
    return empty;
}

const std::string& sfallGetHookScriptsPath()
{
    static const std::string empty;
    return empty;
}

// =============================================================
// animation.h stubs — combat check reset
// =============================================================
// sfall_global_scripts.cc calls animationResetCombatCheck in
// the process_input and process_main ticker callbacks.

void animationResetCombatCheck()
{
}

// =============================================================
// scripts.h stubs — procedure name table
// =============================================================
// sfall_global_scripts.cc reads gScriptProcNames to decode
// procedure indices when loading hook scripts.

const char* gScriptProcNames[SCRIPT_PROC_COUNT] = {};

// =============================================================
// sfall_script_hooks.h stubs — hook registration
// =============================================================
// sfall_global_scripts.cc registers hook scripts via
// scriptHooksRegister and unregisters via scriptHooksUnregisterProgram.

bool scriptHooksRegister(Program* /*program*/, HookType /*hookType*/, int /*procedureIndex*/, bool /*atEnd*/)
{
    return false;  // no hooks registered in test context
}

void scriptHooksUnregisterProgram(Program* /*program*/)
{
    // no-op: hooks registry is empty in test context.
}

// =============================================================
// interpreter.h stubs — Program::procedureCount
// =============================================================
// sfall_global_scripts.cc calls Program::procedureCount() to
// check whether a program has at least one procedure before
// issuing executeProcedure calls when mode==0 (immediate start).

int Program::procedureCount() const
{
    return 0;
}

// sync2 (upstream 1cce144): sfall_global_scripts.cc now calls
// programProcessProcedureEvents (interpreter.cc) and
// scriptDetachedContextRegister/Unregister (scripts.cc). Those
// translation units are not part of test_sources, so provide
// no-op stubs here (same pattern as programInterpret above).
void programProcessProcedureEvents(Program* /*program*/)
{
    // no-op: event processing requires the full interpreter runtime.
}

bool scriptDetachedContextRegister(Program* /*program*/, DetachedScriptOwnerKind /*ownerKind*/)
{
    return true;
}

void scriptDetachedContextUnregister(Program* /*program*/)
{
    // no-op: detached-context tracking requires the full script runtime.
}

} // namespace fallout
