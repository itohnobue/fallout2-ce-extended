#include "sfall_opcodes.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <memory>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

#include "animation.h"
#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "dbox.h"
#include "db.h"
#include "debug.h"
#include "game.h"
#include "game_dialog.h"
#include "game_movie.h"
#include "input.h"
#include "interface.h"
#include "interpreter.h"
#include "inventory.h"
#include "item.h"
#include "light.h"
#include "map.h"
#include "memory.h"
#include "memory_manager.h"
#include "message.h"
#include "mouse.h"
#include "obj_types.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "proto.h"
#include "proto_instance.h"
#include "random.h"
#include "script_sound.h"
#include "scripts.h"
#include "sfall_animation.h"
#include "sfall_arrays.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "settings.h"
#include "sfall_ini.h"
#include "sfall_kb_helpers.h"
#include "sfall_lists.h"
#include "sfall_metarules.h"
#include "sfall_script_hooks.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "tile.h"
#include "trait.h"
#include "window_manager.h"
#include "worldmap.h"

namespace fallout {

typedef enum ExplosionMetarule {
    EXPL_FORCE_EXPLOSION_PATTERN = 1,
    EXPL_FORCE_EXPLOSION_ART = 2,
    EXPL_FORCE_EXPLOSION_RADIUS = 3,
    EXPL_FORCE_EXPLOSION_DMGTYPE = 4,
    EXPL_STATIC_EXPLOSION_RADIUS = 5,
    EXPL_GET_EXPLOSION_DAMAGE = 6,
    EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE = 7,
    EXPL_SET_PLASTIC_EXPLOSION_DAMAGE = 8,
    EXPL_SET_EXPLOSION_MAX_TARGET = 9,
} ExplosionMetarule;

// Version constants: reported to scripts via sfall_ver_major/minor/build.
// Bumped from 4.4.9 to 4.5.1 for RPU/ET Tu compatibility.
// These remain constexpr for now; CMake-configured version would allow
// compile-time customization for different sfall compatibility targets.
static constexpr int kVersionMajor = 4;
static constexpr int kVersionMinor = 5;
static constexpr int kVersionPatch = 1;
static constexpr int kSfallPathBufferSize = 3200; // matches rotation path size in animation.cc

static void op_art_exists(Program* program)
{
    int fid = programStackPopInteger(program);
    programStackPushInteger(program, artExists(fid));
}

static void op_obj_is_carrying_obj(Program* program)
{
    Object* itemObj = static_cast<Object*>(programStackPopPointer(program));
    Object* invenObj = static_cast<Object*>(programStackPopPointer(program));

    int count = 0;
    if (invenObj != nullptr && itemObj != nullptr && objectTypeFromFid(invenObj->fid) == OBJ_TYPE_CRITTER) {
        Inventory* inventory = &(invenObj->data.inventory);
        for (int index = 0; index < inventory->length; index++) {
            InventoryItem* inventoryItem = &(inventory->items[index]);
            if (inventoryItem->item == itemObj) {
                if (inventoryItem->quantity <= 0) {
                    debugPrint("%s: obj_is_carrying_obj found non-positive inventory quantity for item %p in owner %p",
                        program->name,
                        itemObj,
                        invenObj);
                    count = 1;
                } else {
                    count = inventoryItem->quantity;
                }
                break;
            }
        }
    }

    programStackPushInteger(program, count);
}

// ============================================================
// VOODOO read_* opcodes — emulated sfall memory reads (UF-H-001)
//
// CE cannot dereference arbitrary engine addresses.  Instead, known
// sfall memory locations are emulated with CE-native equivalents.
// Unknown addresses return -1 and log a diagnostic.
//
// Supported read_byte addresses:
//   0x56D38C — combat target highlight state → combatGetTargetHighlight()
//   0x410003 — Rotators fork detection signature → hardcoded 0xF4
//              (ETu's gl_rotators.ssl checks this byte to enable
//               Rotators-specific features)
//
// Migration guidance for mods:
//   - Most FO1/Fallout2 engine state available through CE-native metarules
//     (see sfall_metarules.h for the full catalog).
//   - gFallout1Behavior flag covers many FO1 behavioral differences
//     (hit chance formula, rest healing, encounter dialog, level cap, etc.).
//   - Use opcode_exists() at runtime to check whether read_* opcodes are
//     registered before calling them.
//   - read_short / read_int / read_string currently have zero implemented
//     addresses — all known mod usage goes through read_byte or metarules.
// ============================================================

static void op_read_byte(Program* program)
{
    int addr = programStackPopInteger(program);

    int value = -1;
    switch (addr) {
    case 0x56D38C:
        value = combatGetTargetHighlight();
        break;
    // 0x410003 — Rotators fork detection signature.
    // ETu's gl_rotators.ssl checks this byte to detect the Rotators
    // sfall fork. Returning 0xF4 signals "Rotators engine present"
    // so scripts enable Rotators-specific features.
    case 0x410003:
        value = 0xF4;
        break;
    default:
#ifndef NDEBUG
        debugPrint("%s: VOODOO read_byte at 0x%x — address not supported. "
                   "Returning -1. See sfall_opcodes.cc for supported addresses "
                   "and migration guidance.\n", program->name, addr);
#endif
        programPrintError("%s: attempt to 'read_byte' at 0x%x (not supported)", program->name, addr);
        break;
    }

    programStackPushInteger(program, value);
}

// read_short — reads a 16-bit value from a specified address.
// CE cannot dereference arbitrary engine addresses. Registered as a
// stub returning -1 so scripts that check for this opcode do not crash.
// Currently has zero implemented addresses (all known mod usage is
// through read_byte or CE-native metarules).
static void op_read_short(Program* program)
{
    int addr = programStackPopInteger(program);
#ifndef NDEBUG
    debugPrint("%s: VOODOO read_short at 0x%x — not supported in CE engine. "
               "Returning -1. Prefer CE-native metarule alternatives.\n",
               program->name, addr);
#endif
    programPrintError("%s: read_short at 0x%x — not supported in CE engine, returning -1", program->name, addr);
    programStackPushInteger(program, -1);
}

// read_int — reads a 32-bit value from a specified address.
// CE cannot dereference arbitrary engine addresses. Registered as a
// stub returning -1 so scripts that check for this opcode do not crash.
// Currently has zero implemented addresses.
static void op_read_int(Program* program)
{
    int addr = programStackPopInteger(program);
#ifndef NDEBUG
    debugPrint("%s: VOODOO read_int at 0x%x — not supported in CE engine. "
               "Returning -1. Prefer CE-native metarule alternatives.\n",
               program->name, addr);
#endif
    programPrintError("%s: read_int at 0x%x — not supported in CE engine, returning -1", program->name, addr);
    programStackPushInteger(program, -1);
}

// read_string — reads a null-terminated string from a specified address.
// CE cannot dereference arbitrary engine addresses. Registered as a
// stub returning -1 so scripts that check for this opcode do not crash.
// Previously unregistered — calling opcode 0x8159 triggered
// programFatalError (longjmp abort). Now returns -1 gracefully
// like read_short/read_int/read_byte.
static void op_read_string(Program* program)
{
    int addr = programStackPopInteger(program);
#ifndef NDEBUG
    debugPrint("%s: VOODOO read_string at 0x%x — not supported in CE engine. "
               "Returning -1. Prefer CE-native metarule alternatives.\n",
               program->name, addr);
#endif
    programPrintError("%s: read_string at 0x%x — not supported in CE engine, returning -1", program->name, addr);
    programStackPushInteger(program, -1);
}

// set_pc_base_stat
static void op_set_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    Stat stat = programStackPopEnum<Stat>(program);
    if (gDude == nullptr) {
        return;
    }
    critterSetBaseStat(gDude, stat, value);
}

static void op_set_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBaseStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    Stat stat = programStackPopEnum<Stat>(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || objectTypeFromFid(obj->fid) != OBJ_TYPE_CRITTER) {
        return;
    }
    critterSetBaseStat(obj, stat, value);
}

// set_pc_extra_stat
static void op_set_pc_bonus_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    Stat stat = programStackPopEnum<Stat>(program);
    if (gDude == nullptr) {
        return;
    }
    critterSetBonusStat(gDude, stat, value);
}

static void op_set_critter_extra_stat(Program* program)
{
    // CE: Implementation is different. Sfall changes value directly on the
    // dude's proto, without calling |critterSetBonusStat|. This function has
    // important call to update derived stats, which is not present in Sfall.
    int value = programStackPopInteger(program);
    Stat stat = programStackPopEnum<Stat>(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || objectTypeFromFid(obj->fid) != OBJ_TYPE_CRITTER) {
        return;
    }
    critterSetBonusStat(obj, stat, value);
}








// get_pc_base_stat
static void op_get_pc_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    Stat stat = programStackPopEnum<Stat>(program);
    if (gDude == nullptr) {
        programStackPushInteger(program, 0);
        return;
    }
    programStackPushInteger(program, critterGetBaseStat(gDude, stat));
}

static void op_get_critter_base_stat(Program* program)
{
    // CE: Implementation is different. Sfall obtains value directly from
    // dude's proto. This can have unforeseen consequences when dealing with
    // current stats.
    Stat stat = programStackPopEnum<Stat>(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || objectTypeFromFid(obj->fid) != OBJ_TYPE_CRITTER) {
        programStackPushInteger(program, 0);
        return;
    }
    programStackPushInteger(program, critterGetBaseStat(obj, stat));
}

// get_pc_extra_stat
static void op_get_pc_bonus_stat(Program* program)
{
    Stat stat = programStackPopEnum<Stat>(program);
    if (gDude == nullptr) {
        programStackPushInteger(program, 0);
        return;
    }
    int value = critterGetBonusStat(gDude, stat);
    programStackPushInteger(program, value);
}

static void op_get_critter_extra_stat(Program* program)
{
    Stat stat = programStackPopEnum<Stat>(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || objectTypeFromFid(obj->fid) != OBJ_TYPE_CRITTER) {
        programStackPushInteger(program, 0);
        return;
    }
    int value = critterGetBonusStat(obj, stat);
    programStackPushInteger(program, value);
}

// tap_key
static void op_tap_key(Program* program)
{
    int key = programStackPopInteger(program);
    sfall_kb_press_key(key);
}

// get_year
static void op_get_year(Program* program)
{
    int year;
    gameTimeGetDate(nullptr, nullptr, &year);
    programStackPushInteger(program, year);
}



// game_loaded
// F-058: Returns tri-state per sfall 4.x spec:
//   2 = first load (new game or first load of save)
//   1 = reload (subsequent save/load cycles)
//   0 = otherwise (already consumed for this script, or not a global script)
static void op_game_loaded(Program* program)
{
    int loaded = sfall_gl_scr_is_loaded(program);
    programStackPushInteger(program, loaded);
}

// set_global_script_repeat
static void op_set_global_script_repeat(Program* program)
{
    int frames = programStackPopInteger(program);
    sfall_gl_scr_set_repeat(program, frames);
}

// key_pressed
static void op_key_pressed(Program* program)
{
    int key = programStackPopInteger(program);
    bool pressed = sfall_kb_is_key_pressed(key);
    programStackPushInteger(program, pressed ? 1 : 0);
}

// in_world_map
static void op_in_world_map(Program* program)
{
    programStackPushInteger(program, GameMode::isInGameMode(GameMode::kWorldmap) ? 1 : 0);
}

// force_encounter
static void op_force_encounter(Program* program)
{
    int map = programStackPopInteger(program);
    wmForceEncounter(map, ENCOUNTER_FLAG_NONE);
}

// set_world_map_pos
static void op_set_world_map_pos(Program* program)
{
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);
    wmSetPartyWorldPos(x, y);
}

// get_world_map_x_pos
static void op_get_world_map_x_pos(Program* program)
{
    int x;
    wmGetPartyWorldPos(&x, nullptr);
    programStackPushInteger(program, x);
}

// get_world_map_y_pos
static void op_get_world_map_y_pos(Program* program)
{
    int y;
    wmGetPartyWorldPos(nullptr, &y);
    programStackPushInteger(program, y);
}

// set_map_time_multi
void op_set_map_time_multi(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    wmSetScriptWorldMapMulti(value.asFloat());
}

// active_hand
static void op_active_hand(Program* program)
{
    programStackPushInteger(program, interfaceGetCurrentHand());
}

static void op_get_critter_current_ap(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    int actionPoints = 0;
    if (critter != nullptr && objectTypeFromFid(critter->fid) == OBJ_TYPE_CRITTER) {
        actionPoints = critter->data.critter.combat.ap;
    }

    programStackPushInteger(program, actionPoints);
}

static void op_set_critter_current_ap(Program* program)
{
    int actionPoints = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_current_ap: expected critter object");
        return;
    }

    if (actionPoints < 0) {
        actionPoints = 0;
    }

    critter->data.critter.combat.ap = actionPoints;
    if (critter == gDude && isInCombat()) {
        interfaceRenderActionPoints(actionPoints, _combat_free_move);
    }
}

static void op_set_critter_burst_disable(Program* program)
{
    int disable = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_burst_disable: expected critter object");
        return;
    }

    aiSetBurstDisabled(critter, disable != 0);
}

static void refreshUnspentApArmorClass()
{
    if (isInCombat() && _combat_whose_turn() != gDude) {
        interfaceRenderArmorClass(false);
    }
}

static void op_set_unspent_ap_bonus(Program* program)
{
    int multiplier = programStackPopInteger(program);
    statSetUnspentApBonus(multiplier);
    refreshUnspentApArmorClass();
}

static void op_get_unspent_ap_bonus(Program* program)
{
    programStackPushInteger(program, statGetUnspentApBonus());
}

static void op_set_unspent_ap_perk_bonus(Program* program)
{
    int multiplier = programStackPopInteger(program);
    statSetUnspentApPerkBonus(multiplier);
    refreshUnspentApArmorClass();
}

static void op_get_unspent_ap_perk_bonus(Program* program)
{
    programStackPushInteger(program, statGetUnspentApPerkBonus());
}

static void op_set_inven_ap_cost(Program* program)
{
    int cost = programStackPopInteger(program);
    if (cost < 0 || cost > 100) {
        programPrintError("set_inven_ap_cost: value %d clamped to range [0, 100]", cost);
    }
    cost = std::clamp(cost, 0, 100);
    inventorySetInvenApCost(cost);
}

// toggle_active_hand
static void op_toggle_active_hand(Program* program)
{
    interfaceBarSwapHands(true);
}

// set_global_script_type
static void op_set_global_script_type(Program* program)
{
    int type = programStackPopInteger(program);
    sfall_gl_scr_set_type(program, type);
}

// available_global_script_types
// Returns a bitmask of the global script types supported by the engine.
// Type 0 = timed, Type 1 = background/always, Type 2 = world map only, Type 3 = gameplay.
static void op_available_global_script_types(Program* program)
{
    // All 4 types are supported: bits 0-3 = 0xF = 15
    constexpr int kSupportedTypes = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
    programStackPushInteger(program, kSupportedTypes);
}

// set_sfall_global
// F-15 (ITER2-M14): Fire HOOK_SETGLOBALVAR for int-keyed int-value sfall
// global writes. Vanilla opSetGlobalVar (interpreter_extra.cc:1276) routes
// through scriptHooks_SetGlobalVar() which dispatches to registered hook
// scripts. sfall globals bypassed this hook entirely — two separate global
// var systems, only vanilla got the hook coverage.
//
// M-10: HOOK_SETGLOBALVAR is NOT dispatched from set_sfall_global at all.
// sfall's only HOOK_SETGLOBALVAR injection is at the VANILLA op_set_global_var
// path (MiscHs.cpp:787, 0x455A6D) — never the sfall-global opcode. The old
// code fired the hook for int-keyed int-value writes, so mods registering the
// hook received spurious callbacks (and the possibly-overridden value was
// stored). All sfall-global paths now store directly without hook dispatch.
//
// The following globals write paths bypass HOOK_SETGLOBALVAR entirely:
//   1. String-keyed globals (both int and float values) — key is a string,
//      which does not fit the int-keyed hook contract.
//   2. Float-valued int-keyed globals — value is a float, which would require
//      truncation or re-interpretation to fit the int-typed hook signature.
//   3. Int-valued int-keyed globals — matches sfall: plain map store.
//
// Mods that need to intercept STRING or FLOAT global writes should use one of
// these alternative patterns:
//   (a) Store a companion int-keyed sentinel alongside the string/float value
//       to trigger hook notification on the companion key.
//   (b) Use a companion global script (hs_*.int) that polls or observes the
//       relevant string/float globals via get_sfall_global_str / float.
//   (c) Extend the hook contract in a future sfall API version to accept
//       string keys and variant-typed values.
//
// Recursion protection: the ScriptHookCall system has MAX_HOOK_CALL_DEPTH=8
// (global) and per-type depth cap of 4, which guards against infinite
// recursion if a hook handler calls set_sfall_global again.
static void op_set_sfall_global(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    ProgramValue variable = programStackPopValue(program);

    // F-647: Reject float/pointer variable keys — set_sfall_global only
    // supports int or string keys. Float keys silently consumed in the old
    // code (the float-value branch checks STRING/INT, the int-value branch
    // falls through to the "unsupported value type" error at line ~613).
    // String keys with pointer-value payloads would similarly be dropped
    // without any diagnostic, making debugging nearly impossible.
    if (!variable.isInt() && (variable.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_STRING) {
        programPrintError("set_sfall_global: variable key must be int or string, got %s",
            variable.typeDebugString());
        return;
    }

    if (value.isFloat()) {
        // Float values use parallel float storage to preserve precision.
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            // UM-06: Reject script keys starting with "SF" (see int path).
            if (key != nullptr && key[0] == 'S' && key[1] == 'F') {
                programPrintError("set_sfall_global_float: key \"%s\" uses reserved 'SF' prefix", key);
                return;
            }
            sfall_gl_vars_store_float(key, value.asFloat());
        } else if (variable.opcode == VALUE_TYPE_INT) {
            sfall_gl_vars_store_float(variable.integerValue, value.asFloat());
        }
    } else if (value.isInt()) {
        // Integer values: store directly. The previous code treated all
        // non-float ProgramValues as integers, but string/pointer values
        // hold heap offsets in integerValue — storing those as globals
        // causes data corruption that persists through save/load.
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            // UM-06: Reject script keys starting with "SF" to prevent
            // collision with 40+ CE-internal sfall global var keys
            // (e.g., "SFXpMod%", "SFSkillM"). All internal keys use
            // exactly 8-char names beginning with "SF"; script keys
            // matching this pattern would overwrite internal state.
            if (key != nullptr && key[0] == 'S' && key[1] == 'F') {
                programPrintError("set_sfall_global: key \"%s\" uses reserved 'SF' prefix", key);
                return;
            }
            sfall_gl_vars_store(key, value.integerValue);
        } else if (variable.opcode == VALUE_TYPE_INT) {
            // M-10: sfall's only HOOK_SETGLOBALVAR injection is at the VANILLA
            // op_set_global_var path (MiscHs.cpp:787, 0x455A6D), never the
            // sfall-global opcode. The fork fired the hook here for int-keyed
            // set_sfall_global, so mods registering the hook received spurious
            // callbacks (and the possibly-overridden value was stored). Store
            // directly without hook dispatch, matching sfall.
            sfall_gl_vars_store(variable.integerValue, value.integerValue);
        }
    } else {
        // Reject unsupported value types (string, pointer, dynamic string).
        // Storing these would write a heap offset as a global integer,
        // causing data corruption across save/load cycles.
        programPrintError("set_sfall_global: unsupported value type %s", value.typeDebugString());
    }
}

// get_sfall_global_int
// F-011 (ITER2-M2): Use the bool return from sfall_gl_vars_fetch to
// distinguish "key not found" from "key stored with value 0". When the
// key is not found, we push INT_MIN as a sentinel so scripts can
// reliably detect absence vs. a stored value of 0 (or any other valid
// int). Positive scripts never set INT_MIN as a meaningful value.
// The float counterpart (op_get_sfall_global_float) already correctly
// uses the bool return and falls back to int storage.
static void op_get_sfall_global_int(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    int value = 0;
    bool found = false;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        found = sfall_gl_vars_fetch(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        found = sfall_gl_vars_fetch(variable.integerValue, value);
    }

    if (!found) {
        // Fall back to float storage: if a script stores a float via
        // set_sfall_global_float and reads it back via get_sfall_global_int,
        // return the truncated integer value instead of the INT_MIN sentinel.
        // This mirrors the int→float fallback in op_get_sfall_global_float
        // and ensures symmetric cross-type read behavior between the two
        // accessors (both int→float and float→int now work).
        float floatValue = 0.0f;
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            if (sfall_gl_vars_fetch_float(key, floatValue)) {
                // Guard against float-to-int overflow UB (I2-M002).
                // Use long long intermediate, matching op_ceil pattern.
                long long llValue = static_cast<long long>(floatValue);
                if (llValue > INT_MAX) {
                    programStackPushInteger(program, INT_MAX);
                } else if (llValue < INT_MIN) {
                    programStackPushInteger(program, INT_MIN);
                } else {
                    programStackPushInteger(program, static_cast<int>(floatValue));
                }
                return;
            }
        } else if (variable.opcode == VALUE_TYPE_INT) {
            if (sfall_gl_vars_fetch_float(variable.integerValue, floatValue)) {
                // Guard against float-to-int overflow UB (I2-M002).
                long long llValue = static_cast<long long>(floatValue);
                if (llValue > INT_MAX) {
                    programStackPushInteger(program, INT_MAX);
                } else if (llValue < INT_MIN) {
                    programStackPushInteger(program, INT_MIN);
                } else {
                    programStackPushInteger(program, static_cast<int>(floatValue));
                }
                return;
            }
        }

        // M-03: sfall GetGlobalVarInternal returns 0 for missing keys
        // (ScriptExtender.cpp:393-397) — "unset" and "stored 0" are
        // indistinguishable. The fork's INT_MIN sentinel broke sfall mods
        // that rely on `get_sfall_global_int(key) == 0` for unset keys
        // (RPU gl_k_alcohl.ssl:48 `game_time - INT_MIN` overflow;
        // epai37.ssl:210-220 unset != MALE_REG_HAIR(0) → all options shown).
        // Push 0 for not-found, matching the float counterpart and sfall.
        programStackPushInteger(program, 0);
    } else {
        programStackPushInteger(program, value);
    }
}

// get_sfall_global_float
static void op_get_sfall_global_float(Program* program)
{
    ProgramValue variable = programStackPopValue(program);

    float value = 0.0f;
    bool found = false;
    if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        const char* key = programGetString(program, variable.opcode, variable.integerValue);
        found = sfall_gl_vars_fetch_float(key, value);
    } else if (variable.opcode == VALUE_TYPE_INT) {
        found = sfall_gl_vars_fetch_float(variable.integerValue, value);
    }

    // Fall back to int storage if no float entry exists (backward compat).
    if (!found) {
        int intValue = 0;
        if ((variable.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* key = programGetString(program, variable.opcode, variable.integerValue);
            if (sfall_gl_vars_fetch(key, intValue)) {
                value = static_cast<float>(intValue);
            }
        } else if (variable.opcode == VALUE_TYPE_INT) {
            if (sfall_gl_vars_fetch(variable.integerValue, intValue)) {
                value = static_cast<float>(intValue);
            }
        }
    }

    programStackPushFloat(program, value);
}

// get_game_mode
static void op_get_game_mode(Program* program)
{
    programStackPushInteger(program, GameMode::getCurrentGameMode());
}

// get_uptime
static void op_get_uptime(Program* program)
{
    programStackPushInteger(program, getTicks());
}

// set_car_current_town
static void op_set_car_current_town(Program* program)
{
    int area = programStackPopInteger(program);
    wmCarSetCurrentArea(area);
}

// get_bodypart_hit_modifier
static void op_get_bodypart_hit_modifier(Program* program)
{
    HitLocation hitLocation = programStackPopEnum<HitLocation>(program);
    programStackPushInteger(program, combat_get_hit_location_penalty(hitLocation));
}

// set_bodypart_hit_modifier
static void op_set_bodypart_hit_modifier(Program* program)
{
    int penalty = programStackPopInteger(program);
    HitLocation hitLocation = programStackPopEnum<HitLocation>(program);
    combat_set_hit_location_penalty(hitLocation, penalty);
}

static bool criticalTableArgsAreValid(Program* program, const char* opcodeName, KillType killType, HitLocation hitLocation, int effect, CriticalHitDataMember dataMember)
{
    if (!killTypeOverrideIsValid(killType)
        || !hitLocationIsValid(hitLocation)
        || !criticalEffectIsValid(effect)
        || !criticalHitDataMemberIsValid(dataMember)) {
        programPrintError("%s: argument values out of range", opcodeName);
        return false;
    }

    return true;
}

static void op_set_critical_table(Program* program)
{
    int value = programStackPopInteger(program);
    CriticalHitDataMember dataMember = programStackPopEnum<CriticalHitDataMember>(program);
    CriticalEffect effect = programStackPopEnum<CriticalEffect>(program);
    HitLocation hitLocation = programStackPopEnum<HitLocation>(program);
    KillType killType = programStackPopEnum<KillType>(program);

    if (!criticalTableArgsAreValid(program, "set_critical_table", killType, hitLocation, effect, dataMember)) {
        return;
    }

    criticalsSetValue(killType, hitLocation, effect, dataMember, value);
}

static void op_get_critical_table(Program* program)
{
    CriticalHitDataMember dataMember = programStackPopEnum<CriticalHitDataMember>(program);
    CriticalEffect effect = programStackPopEnum<CriticalEffect>(program);
    HitLocation hitLocation = programStackPopEnum<HitLocation>(program);
    KillType killType = programStackPopEnum<KillType>(program);

    if (!criticalTableArgsAreValid(program, "get_critical_table", killType, hitLocation, effect, dataMember)) {
        programStackPushInteger(program, 0);
        return;
    }

    programStackPushInteger(program, criticalsGetValue(killType, hitLocation, effect, dataMember));
}

static void op_reset_critical_table(Program* program)
{
    CriticalHitDataMember dataMember = programStackPopEnum<CriticalHitDataMember>(program);
    CriticalEffect effect = programStackPopEnum<CriticalEffect>(program);
    HitLocation hitLocation = programStackPopEnum<HitLocation>(program);
    KillType killType = programStackPopEnum<KillType>(program);

    if (!criticalTableArgsAreValid(program, "reset_critical_table", killType, hitLocation, effect, dataMember)) {
        return;
    }

    criticalsResetValue(killType, hitLocation, effect, dataMember);
}

// sqrt
static void op_sqrt(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sqrtf(programValue.asFloat()));
}

// abs
static void op_abs(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);

    if (programValue.isInt()) {
        // Guard against abs(INT_MIN) which is undefined behavior per C++ standard.
        // INT_MIN has no positive representation in two's complement.
        if (programValue.integerValue == INT_MIN) {
            programStackPushInteger(program, 0);
            return;
        }
        programStackPushInteger(program, abs(programValue.integerValue));
    } else {
        programStackPushFloat(program, abs(programValue.asFloat()));
    }
}

// sin
static void op_sin(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, sinf(programValue.asFloat()));
}

// cos
static void op_cos(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, cosf(programValue.asFloat()));
}

// tan
static void op_tan(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, tanf(programValue.asFloat()));
}

// arctan
static void op_arctan(Program* program)
{
    ProgramValue xValue = programStackPopValue(program);
    ProgramValue yValue = programStackPopValue(program);
    programStackPushFloat(program, atan2f(yValue.asFloat(), xValue.asFloat()));
}

// pow (^)
static void op_power(Program* program)
{
    ProgramValue expValue = programStackPopValue(program);
    ProgramValue baseValue = programStackPopValue(program);

    // CE: Implementation is slightly different, check.
    float result = powf(baseValue.asFloat(), expValue.asFloat());

    if (baseValue.isInt() && expValue.isInt()) {
        // Guard against float-to-int overflow UB (I2-F-005).
        // powf(2.0f, 31.0f) = 2147483648.0f exceeds INT_MAX (2147483647).
        // Fall back to float path on overflow, matching the pattern used by
        // vanilla opAdd/opSubtract/opMultiply in interpreter.cc.
        if (result > INT_MAX || result < INT_MIN) {
            programStackPushFloat(program, result);
        } else {
            // Note: this will truncate the result if power is negative.
            // Keeping it to match sfall.
            programStackPushInteger(program, static_cast<int>(result));
        }
    } else {
        programStackPushFloat(program, result);
    }
}

// log
static void op_log(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, logf(programValue.asFloat()));
}

// ceil
static void op_ceil(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    float result = ceilf(programValue.asFloat());

    // Guard against float-to-int overflow UB (F-012, R-001).
    // ceilf(INT_MAX + 1.0f) = 2147483648.0f exceeds INT_MAX.
    // Cast to long long first — comparing float to int loses precision
    // because (float)INT_MAX == 2147483648.0f, same as the overflow value.
    // Fall back to float on overflow, matching op_power pattern.
    long long llResult = static_cast<long long>(result);
    if (llResult > INT_MAX || llResult < INT_MIN) {
        programStackPushFloat(program, result);
    } else {
        programStackPushInteger(program, static_cast<int>(result));
    }
}

// exp
static void op_exponent(Program* program)
{
    ProgramValue programValue = programStackPopValue(program);
    programStackPushFloat(program, expf(programValue.asFloat()));
}

// get_script
static void op_get_script(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr) {
        programStackPushInteger(program, -1);
        return;
    }

    if (obj->sid == -1) {
        programStackPushInteger(program, 0);
        return;
    }

    Script* script;
    if (scriptGetScript(obj->sid, &script) == -1 || script->index < 0) {
        programStackPushInteger(program, 0);
        return;
    }

    programStackPushInteger(program, script->index + 1);
}

// remove_script
static void op_remove_script(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj == nullptr || obj->sid == -1) {
        return;
    }

    scriptRemove(obj->sid);
    obj->sid = -1;
    obj->scriptIndex = -1;
}

// set_script
static void op_set_script(Program* program)
{
    int scriptId = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == nullptr) {
        return;
    }

    unsigned int rawScriptId = static_cast<unsigned int>(scriptId);
    // sfall encodes set_script() ids as a 1-based script index in the low
    // 28 bits, with the upper bits reserved for flags. The top bit
    // (0x80000000) suppresses map_enter_p_proc after start().
    int scriptIndex = static_cast<int>(rawScriptId & ~0xF0000000u);
    if (scriptIndex == 0) {
        programPrintError("set_script: invalid script index number %d.", scriptIndex);
        return;
    }

    scriptIndex--;
    if (!scriptsIsValidScriptIndex(scriptIndex)) {
        programPrintError("set_script: invalid script index (engine) number %d.", scriptIndex);
        return;
    }

    if (obj->sid != -1) {
        scriptRemove(obj->sid);
        obj->sid = -1;
        obj->scriptIndex = -1;
    }

    int scriptType = (objectTypeFromPid(obj->pid) == OBJ_TYPE_CRITTER) ? SCRIPT_TYPE_CRITTER : SCRIPT_TYPE_ITEM;
    if (objectSetScript(obj, scriptType, scriptIndex) == -1) {
        obj->sid = -1;
        obj->scriptIndex = -1;
        return;
    }

    Script* script;
    if (scriptGetScript(obj->sid, &script) == -1) {
        scriptRemove(obj->sid);
        obj->sid = -1;
        obj->scriptIndex = -1;
        return;
    }

    int sid = obj->sid;
    script->owner = obj;
    obj->scriptIndex = scriptIndex;

    scriptExecProc(sid, SCRIPT_PROC_START);
    if ((rawScriptId & 0x80000000u) == 0) {
        // note: if map_enter_p_proc is missing, START gets executed again
        scriptExecProc(sid, SCRIPT_PROC_MAP_ENTER);
    }
}

// get_proto_data
static void op_get_proto_data(Program* program)
{
    int rawOffset = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        programPrintError("get_proto_data: bad proto %d", pid);
        programStackPushInteger(program, -1);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (rawOffset < 0 || rawOffset % static_cast<int>(sizeof(int)) != 0) {
        programPrintError("get_proto_data: bad offset %d", rawOffset);
        programStackPushInteger(program, -1);
        return;
    }

    size_t offset = static_cast<size_t>(rawOffset);
    size_t size = proto_size(objectTypeFromPid(pid));
    if (offset > size || size - offset < sizeof(int)) {
        programPrintError("get_proto_data: bad offset %zu", offset);
        programStackPushInteger(program, -1);
        return;
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset);
    programStackPushInteger(program, value);
}

// set_proto_data
static void op_set_proto_data(Program* program)
{
    int value = programStackPopInteger(program);
    int rawOffset = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    Proto* proto;
    if (protoGetProto(pid, &proto) != 0) {
        programPrintError("set_proto_data: bad proto %d", pid);
        return;
    }

    // CE: Make sure the requested offset is within memory bounds and is
    // properly aligned.
    if (rawOffset < 0 || rawOffset % static_cast<int>(sizeof(int)) != 0) {
        programPrintError("set_proto_data: bad offset %d", rawOffset);
        return;
    }

    size_t offset = static_cast<size_t>(rawOffset);
    size_t size = proto_size(objectTypeFromPid(pid));
    if (offset > size || size - offset < sizeof(int)) {
        programPrintError("set_proto_data: bad offset %zu", offset);
        return;
    }

    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(proto) + offset) = value;
    protoMarkDirty(pid);
}

// set_self
static void op_set_self(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    scriptContextSetOverrideSelf(program, obj);
}

// list_begin
static void op_list_begin(Program* program)
{
    int listType = programStackPopInteger(program);
    int listId = sfallListsCreate(listType);
    programStackPushInteger(program, listId);
}

// list_next
static void op_list_next(Program* program)
{
    int listId = programStackPopInteger(program);
    Object* obj = sfallListsGetNext(listId);
    programStackPushPointer(program, obj);
}

// list_end
static void op_list_end(Program* program)
{
    int listId = programStackPopInteger(program);
    sfallListsDestroy(listId);
}

// sfall_ver_major
static void op_get_version_major(Program* program)
{
    programStackPushInteger(program, kVersionMajor);
}

// sfall_ver_minor
static void op_get_version_minor(Program* program)
{
    programStackPushInteger(program, kVersionMinor);
}

// sfall_ver_build
static void op_get_version_patch(Program* program)
{
    programStackPushInteger(program, kVersionPatch);
}

// get_weapon_ammo_pid
static void op_get_weapon_ammo_pid(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int pid = -1;
    if (obj != nullptr) {
        if (objectTypeFromPid(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON:
                pid = weaponGetAmmoTypePid(obj);
                break;
            case ITEM_TYPE_MISC:
                pid = miscItemGetPowerTypePid(obj);
                break;
            default:
                break;
            }
        }
    }

    programStackPushInteger(program, pid);
}

// There are two problems with this function.
//
// 1. Sfall's implementation changes ammo PID of misc items, which is impossible
// since it's stored in proto, not in the object.
// 2. Changing weapon's ammo PID is done without checking for ammo
// quantity/capacity which can probably lead to bad things.
//
// set_weapon_ammo_pid
static void op_set_weapon_ammo_pid(Program* program)
{
    int ammoTypePid = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != nullptr) {
        if (objectTypeFromPid(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_WEAPON: {
                // Validate that the new ammo PID references a valid ammo proto.
                Proto* ammoProto;
                if (protoGetProto(ammoTypePid, &ammoProto) != 0) {
                    programPrintError("set_weapon_ammo_pid: invalid ammo PID %d (proto not found)", ammoTypePid);
                    return;
                }
                if (objectTypeFromPid(ammoTypePid) != OBJ_TYPE_ITEM || ammoProto->item.type != ITEM_TYPE_AMMO) {
                    programPrintError("set_weapon_ammo_pid: PID %d is not ITEM_TYPE_AMMO", ammoTypePid);
                    return;
                }
                // Clamp current ammo quantity to the new ammo type's max capacity.
                int maxCapacity = ammoProto->item.data.ammo.quantity;
                if (obj->data.item.weapon.ammoQuantity > maxCapacity) {
                    obj->data.item.weapon.ammoQuantity = maxCapacity;
                }
                obj->data.item.weapon.ammoTypePid = ammoTypePid;
                break;
            }
            default:
                break;
            }
        }
    }
}

// get_weapon_ammo_count
static void op_get_weapon_ammo_count(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    int ammoQuantityOrCharges = 0;
    if (obj != nullptr) {
        if (objectTypeFromPid(obj->pid) == OBJ_TYPE_ITEM) {
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoQuantityOrCharges = ammoGetQuantity(obj);
                break;
            case ITEM_TYPE_MISC:
                ammoQuantityOrCharges = miscItemGetCharges(obj);
                break;
            default:
                break;
            }
        }
    }

    programStackPushInteger(program, ammoQuantityOrCharges);
}

// set_weapon_ammo_count
static void op_set_weapon_ammo_count(Program* program)
{
    int ammoQuantityOrCharges = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    // CE: Implementation is different.
    if (obj != nullptr) {
        if (objectTypeFromPid(obj->pid) == OBJ_TYPE_ITEM) {
            // Guard: negative charges/ammo don't make sense. ammoSetQuantity()
            // handles it for ammo/weapons; miscItemSetCharges() only guards the
            // upper bound — clamp negative here for the MISC path.
            if (ammoQuantityOrCharges < 0) {
                ammoQuantityOrCharges = 0;
            }
            switch (itemGetType(obj)) {
            case ITEM_TYPE_AMMO:
            case ITEM_TYPE_WEAPON:
                ammoSetQuantity(obj, ammoQuantityOrCharges);
                break;
            case ITEM_TYPE_MISC:
                miscItemSetCharges(obj, ammoQuantityOrCharges);
                break;
            default:
                break;
            }
        }
    }
}

// get_mouse_x
static void op_get_mouse_x(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, x);
}

// get_mouse_y
static void op_get_mouse_y(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);
    programStackPushInteger(program, y);
}

// get_mouse_buttons
// Returns mouse button state as a bitmask matching sfall convention:
//   bit 0 (1) = left button  (MOUSE_STATE_LEFT_BUTTON_DOWN  = 0x01)
//   bit 1 (2) = right button (MOUSE_STATE_RIGHT_BUTTON_DOWN = 0x02)
//   bit 2 (4) = middle button — NOT SUPPORTED (CE MouseData has only
//               buttons[2]; dinput.cc does not check SDL_BUTTON_MIDDLE)
static void op_get_mouse_buttons(Program* program)
{
    programStackPushInteger(program, mouse_get_last_buttons());
}

static void op_get_window_under_mouse(Program* program)
{
    // FIXED (UH-04): Use windowGetAtPoint() which iterates gWindows[]
    // topmost-to-bottommost and returns the window whose rect contains
    // the given coordinates. Previously used _win_last_button_winID()
    // which returns the last button-interaction window (stale after
    // mouse move without click).
    int x;
    int y;
    mouseGetPosition(&x, &y);
    int win = windowGetAtPoint(x, y);
    programStackPushInteger(program, win >= 0 ? win : -1);
}

// get_screen_width
static void op_get_screen_width(Program* program)
{
    programStackPushInteger(program, screenGetWidth());
}

// get_screen_height
static void op_get_screen_height(Program* program)
{
    programStackPushInteger(program, screenGetHeight());
}

// get_light_level
static void op_get_light_level(Program* program)
{
    programStackPushInteger(program, lightGetAmbientIntensity());
}

// Custom death model FID numbers set via set_dm_model / set_df_model.
// Zero means "use engine default". These are applied in op_refresh_pc_art
// by overriding the proto FID before armor/weapon layering.
static int gCustomMaleHeroModelNum = 0;
static int gCustomFemaleHeroModelNum = 0;

// note: Might need to be updated when Hero Appearance is implemented.
// F-003: Now reads HAp_Race / HApStyle from sfall globals and applies
// them as base FID offsets during model refresh. Race offsets the base
// model number (tens digit) and style is forwarded to the FID pipeline.
// The exact FID mapping table is mod-specific; this provides the engine-level
// infrastructure to consume the stored values.
static void op_refresh_pc_art(Program* program)
{
    if (gDude == nullptr) {
        return;
    }

    Rect rect;
    objectGetRect(gDude, &rect);

    AnimationType anim = animationTypeFromFid(gDude->fid);
    Rotation rotation = rotationFromFid(gDude->fid);

    _proto_dude_update_gender();

    // Apply custom hero model if one was set via set_dm_model / set_df_model.
    int customModelNum = 0;
    if (critterGetStat(gDude, STAT_GENDER) == GENDER_MALE && gCustomMaleHeroModelNum > 0) {
        customModelNum = gCustomMaleHeroModelNum;
    } else if (critterGetStat(gDude, STAT_GENDER) == GENDER_FEMALE && gCustomFemaleHeroModelNum > 0) {
        customModelNum = gCustomFemaleHeroModelNum;
    }

    // Apply Hero Appearance race/style overrides (F-003).
    // HAp_Race and HApStyle are stored by set_hero_race (0x8214) /
    // set_hero_style (0x8215) as sfall global vars.
    int heroRace = 0;
    int heroStyle = 0;
    if (customModelNum <= 0) {
        sfall_gl_vars_fetch("HAp_Race", heroRace);
        sfall_gl_vars_fetch("HApStyle", heroStyle);
    }

    if (customModelNum > 0) {
        Proto* proto;
        if (protoGetProto(gDude->pid, &proto) != -1) {
            proto->fid = buildFid(OBJ_TYPE_CRITTER, customModelNum, 0, 0, ROTATION_NE);
        }
    } else if (heroRace > 0 || heroStyle > 0) {
        // Hero Appearance race/style overrides: read the current base model
        // number from the proto FID and apply race as a tens-digit offset,
        // style as an animation variant. The exact mapping is mod-specific;
        // a HOOK_ADJUSTFID handler can further customize the output.
        Proto* proto;
        if (protoGetProto(gDude->pid, &proto) != -1) {
            int baseFid = proto->fid & 0xFFF; // Extract base model number.
            // Race offsets the tens digit (each race = model variant group).
            // Style shifts the units digit (each style = sub-variant).
            if (heroRace > 0) {
                int raceOffset = (heroRace - 1) * 10;
                baseFid = (baseFid / 10) * 10 + raceOffset + (baseFid % 10);
            }
            if (heroStyle > 0) {
                int styleOffset = heroStyle - 1;
                int baseUnits = baseFid % 10;
                baseFid = baseFid + styleOffset;
                // If style would overflow the units digit, carry over
                // to the tens digit (F-006: fix dead check).
                if (baseUnits + styleOffset >= 10) {
                    baseFid = ((baseFid / 10) + 1) * 10;
                }
            }
            proto->fid = buildFid(OBJ_TYPE_CRITTER, baseFid, 0, 0, ROTATION_NE);
        }
    }

    int fid = inventoryComputeCritterFid(gDude,
        gDude->pid,
        critterGetItem2(gDude),
        critterGetItem1(gDude),
        critterGetArmor(gDude),
        interfaceGetCurrentHand(),
        anim,
        rotation);

    // CE: When changing gender, the refreshed rect can be smaller than the original one,
    // which can leave a momentary ghost.  We union with old rect to avoid that.
    Rect newRect;
    objectSetFid(gDude, fid, nullptr);
    objectGetRect(gDude, &newRect);
    rectUnion(&rect, &newRect, &rect);
    tileWindowRefreshRect(&rect, gDude->elevation);
}

// create_message_window
//
// F-034: sfall's create_message_window is a non-blocking notification window.
// The original CE implementation used showDialogBox which enters a blocking
// event loop — scripts halted until the user dismissed the dialog. This
// replacement creates a floating window via windowCreate + windowPrintBuf
// and returns immediately without blocking script execution.
// The window is destroyed on the next call or at opcode reset.
static int gMessageWindow = -1;

static void op_create_message_window(Program* program)
{
    // Destroy any previous message window.
    if (gMessageWindow != -1) {
        windowDestroy(gMessageWindow);
        gMessageWindow = -1;
    }

    const char* string = programStackPopString(program);
    if (string == nullptr || string[0] == '\0') {
        return;
    }

    char* copy = internal_strdup(string);

    // internal_strdup can return nullptr on allocation failure (OOM).
    // mf_message_box (sfall_metarules.cc:3463-3469) has the same guard.
    // Without this check, strchr(copy, '\n') dereferences nullptr.
    if (copy == nullptr) {
        return;
    }

    // Parse newline-delimited body lines; first segment is the title.
    char* body[4];
    int bodyCount = 0;

    char* pch = strchr(copy, '\n');
    while (pch != nullptr && bodyCount < 4) {
        *pch = '\0';
        body[bodyCount++] = pch + 1;
        pch = strchr(pch + 1, '\n');
    }

    // Create a non-blocking notification window with the same default
    // position and approximate size as the sfall message window.
    int win = windowCreate(192, 116, 256, 128, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        internal_free(copy);
        return;
    }

    // Print the title line (text before first '\n').
    int ypos = 8;
    windowPrintBuf(win, copy, static_cast<int>(strlen(copy)), 240, 120, 8, ypos, 0, TEXT_ALIGNMENT_LEFT);

    // Print each body line below the title (16 px per line — Fallout font height).
    for (int i = 0; i < bodyCount; i++) {
        ypos += 16;
        windowPrintBuf(win, body[i], static_cast<int>(strlen(body[i])),
            240, 120, 8, ypos, 0, TEXT_ALIGNMENT_LEFT);
    }

    windowRefresh(win);
    gMessageWindow = win;
    internal_free(copy);
}

// get_attack_type
static void op_get_attack_type(Program* program)
{
    HitMode hit_mode;
    if (interface_get_current_attack_mode(&hit_mode)) {
        programStackPushInteger(program, hit_mode);
    } else {
        programStackPushInteger(program, -1);
    }
}

static bool sfallVfsPathContainsTraversal(const char* path);
static bool sfallVfsResolvePath(const char* rawPath, char* outBuf, size_t outBufSize);

static void op_play_sfall_sound(Program* program)
{
    int mode = programStackPopInteger(program);
    const char* path = programStackPopString(program);

    // F-063: Reject paths containing ".." components to prevent path
    // traversal. Matches the VFS opcode pattern (fs_create, fs_copy,
    // fs_find). Without this check, a script could supply a path like
    // "../secrets.dat" which passes through 7 function calls with zero
    // guards. File contents are not returned to the script (only
    // internally processed for audio decoding), so this is defense-in-
    // depth, not data-exfiltration prevention.
    if (sfallVfsPathContainsTraversal(path)) {
        programPrintError("play_sfall_sound: path traversal rejected '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    // F-M18: Reject absolute paths for consistency with VFS opcodes.
    // The VFS ops use sfallVfsResolvePath which rejects absolute paths;
    // play_sfall_sound should follow the same sandbox policy.
    if (path[0] == '/' || path[0] == '\\') {
        programPrintError("play_sfall_sound: absolute path rejected '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    // F2-06: Resolve path against VFS sandbox root. Every other VFS opcode
    // (fs_create, fs_copy, fs_find) calls sfallVfsResolvePath to prepend the
    // configured sandbox root. play_sfall_sound was the only VFS opcode that
    // did not, causing audio paths to resolve relative to the game CWD while
    // file paths resolved relative to the sandbox root.
    char resolvedPath[COMPAT_MAX_PATH];
    if (!sfallVfsResolvePath(path, resolvedPath, sizeof(resolvedPath))) {
        programPrintError("play_sfall_sound: path rejected by sandbox '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    programStackPushInteger(program, scriptSoundPlay(resolvedPath, mode));
}

static void op_stop_sfall_sound(Program* program)
{
    int soundId = programStackPopInteger(program);
    scriptSoundStop(soundId);
}

// force_encounter_with_flags
static void op_force_encounter_with_flags(Program* program)
{
    EncounterFlag flags = static_cast<EncounterFlag>(programStackPopInteger(program));
    int map = programStackPopInteger(program);
    wmForceEncounter(map, flags);
}

// list_as_array
static void op_list_as_array(Program* program)
{
    int type = programStackPopInteger(program);
    int arrayId = ListAsArray(type);
    programStackPushInteger(program, arrayId);
}

// atoi
// F-12 (ITER2-M3): Check strtol result against INT_MAX/INT_MIN to prevent
// silent truncation on LP64 platforms. strtol returns long (64-bit on LP64)
// and ERANGE only catches values exceeding LONG_MAX. Input "3000000000" →
// strtol returns 3000000000L (no ERANGE) → static_cast<int>() truncates to
// -1294967296. The same fix pattern is used in config.cc:275 and
// sfall_ini.cc:280: errno == ERANGE || l < INT_MIN || l > INT_MAX.
static void op_parse_int(Program* program)
{
    const char* string = programStackPopString(program);
    errno = 0;
    long result = strtol(string, nullptr, 0);
    if (errno == ERANGE || result < INT_MIN || result > INT_MAX) {
        programStackPushInteger(program, (result == LONG_MAX || result > INT_MAX) ? INT_MAX : INT_MIN);
    } else {
        programStackPushInteger(program, static_cast<int>(result));
    }
}

// atof — converts string to float with NaN/inf rejection.
// F-009: Invalid float strings (NaN, inf, overflow) are rejected
// and replaced with 0.0f to prevent poisoning the VM float stack.
static void op_atof(Program* program)
{
    const char* string = programStackPopString(program);
    char* end = nullptr;
    errno = 0;
    float val = strtof(string, &end);
    if (errno == ERANGE || std::isnan(val) || std::isinf(val)) {
        programPrintError("op_atof: invalid float value \"%s\" — returning 0.0f", string != nullptr ? string : "(null)");
        val = 0.0f;
    }
    programStackPushFloat(program, val);
}

// tile_under_cursor
static void op_tile_under_cursor(Program* program)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);

    int tile = tileFromScreenXY(x, y);
    programStackPushInteger(program, tile);
}

// gdialog_get_barter_mod
static void op_gdialog_get_barter_mod(Program* program)
{
    programStackPushInteger(program, gameDialogGetBarterModifier());
}

// get_tile_fid
static void op_get_tile_fid(Program* program)
{
    int tileData = programStackPopInteger(program);
    int tile = tileData & 0xFFFFFF;
    int elevation = (tileData >> 24) & 0x0F;
    int mode = tileData >> 28;

    if (!hexGridTileIsValid(tile) || elevation < 0 || elevation >= ELEVATION_COUNT) {
        debugPrint("%s: op_get_tile_fid invalid tile data: tile=%d elevation=%d", program->name, tile, elevation);
        programStackPushInteger(program, 0);
        return;
    }

    int squareTile = squareTileFromTile(tile);
    if (!squareGridTileIsValid(squareTile)) {
        debugPrint("%s: op_get_tile_fid failed to map tile=%d to square index", program->name, tile);
        programStackPushInteger(program, 0);
        return;
    }

    int squareData = _square[elevation]->field_0[squareTile];

    switch (mode) {
    case 1:
        programStackPushInteger(program, (squareData >> 16) & 0x3FFF);
        break;
    case 2:
        programStackPushInteger(program, squareData);
        break;
    default:
        programStackPushInteger(program, squareData & 0x3FFF);
        break;
    }
}

// substr
static void op_substr(Program* program)
{
    auto length = programStackPopInteger(program);
    auto startPos = programStackPopInteger(program);
    const char* str = programStackPopString(program);

    char buf[5120] = { 0 };

    int len = static_cast<int>(strlen(str));

    if (startPos < 0) {
        startPos += len; // start from end
        if (startPos < 0) {
            startPos = 0;
        }
    }

    if (length < 0) {
        // M-01: `length += len - startPos` can leave `length` still negative
        // and then `abs(INT_MIN)` is UB; more importantly the later check
        // `length + startPos > len` used signed int arithmetic that overflowed
        // for length=INT_MAX (from the INT_MIN→INT_MAX guard), wrapping to a
        // negative value so the clamp was skipped and a ~5117-byte OOB read
        // occurred. Use 64-bit arithmetic so the cutoff-at-end computation
        // cannot overflow, then normalize: negative → 0 (return empty).
        long long adjusted = static_cast<long long>(length) + (len - startPos);
        if (adjusted <= 0) {
            programStackPushString(program, buf);
            return;
        }
        if (adjusted > INT_MAX) {
            adjusted = INT_MAX;
        }
        length = static_cast<int>(adjusted);
    }

    // check position
    if (startPos >= len) {
        // start position is out of string length, return empty string
        programStackPushString(program, buf);
        return;
    }

    // M-01: use 64-bit arithmetic here too — `length + startPos` overflows
    // when a script passes a huge positive length (e.g. INT_MAX), wrapping to
    // a negative value and defeating the clamp.
    if (length == 0 || static_cast<long long>(length) + startPos > len) {
        length = len - startPos; // set the correct length, the length of characters goes beyond the end of the string
    }

    if (length > sizeof(buf) - 1) {
        length = sizeof(buf) - 1;
    }

    memcpy(buf, &str[startPos], length);
    buf[length] = '\0';
    programStackPushString(program, buf);
}

// strlen
static void op_get_string_length(Program* program)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, static_cast<int>(strlen(string)));
}

// metarule2_explosions
static void op_explosions_metarule(Program* program)
{
    int param2 = programStackPopInteger(program);
    int param1 = programStackPopInteger(program);
    int metarule = programStackPopInteger(program);

    switch (metarule) {
    case EXPL_FORCE_EXPLOSION_PATTERN:
        // F-13: Wire param1/param2 as actual explosion rotation parameters.
        // See sfall_metarules.cc EXPL_MF_FORCE_PATTERN for full explanation.
        if (param2 != 0) {
            explosionSetPattern(static_cast<Rotation>(param1), static_cast<Rotation>(param2));
        } else if (param1 != 0) {
            explosionSetPattern(ROTATION_SE, ROTATION_W);
        } else {
            explosionSetPattern(ROTATION_FIRST, ROTATION_COUNT);
        }
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_ART:
        explosionSetFrm(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_RADIUS:
        explosionSetRadius(param1);
        programStackPushInteger(program, 0);
        break;
    case EXPL_FORCE_EXPLOSION_DMGTYPE:
        if (!damageTypeIsValid(param1)) {
            debugPrint("\n%s: explosions_metarule: EXPL_FORCE_EXPLOSION_DMGTYPE invalid damage type %d", program->name, param1);
        }
        explosionSetDamageType(static_cast<DamageType>(param1));
        programStackPushInteger(program, 0);
        break;
    case EXPL_STATIC_EXPLOSION_RADIUS:
        weaponSetGrenadeExplosionRadius(param1);
        weaponSetRocketExplosionRadius(param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_GET_EXPLOSION_DAMAGE:
        if (1) {
            int minDamage = 0;
            int maxDamage = 0;
            explosiveGetDamage(param1, &minDamage, &maxDamage);

            ArrayId arrayId = CreateTempArray(2, 0);
            SetArray(arrayId, ProgramValue { 0 }, ProgramValue { minDamage }, false, program);
            SetArray(arrayId, ProgramValue { 1 }, ProgramValue { maxDamage }, false, program);

            programStackPushInteger(program, arrayId);
        }
        break;
    case EXPL_SET_DYNAMITE_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_DYNAMITE_I, param1, param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_SET_PLASTIC_EXPLOSION_DAMAGE:
        explosiveSetDamage(PROTO_ID_PLASTIC_EXPLOSIVES_I, param1, param2);
        programStackPushInteger(program, 0);
        break;
    case EXPL_SET_EXPLOSION_MAX_TARGET:
        explosionSetMaxTargets(param1);
        programStackPushInteger(program, 0);
        break;
    default:
        programStackPushInteger(program, 0);
        break;
    }
}

// message_str_game
static void op_get_message(Program* program)
{
    int messageId = programStackPopInteger(program);
    int messageListId = programStackPopInteger(program);
    char* text = messageListRepositoryGetMsg(messageListId, messageId);
    programStackPushString(program, text);
}

// save_array
static void op_save_array(Program* program)
{
    auto arrayId = static_cast<ArrayId>(programStackPopInteger(program));
    auto key = programStackPopValue(program);
    auto result = SaveArray(key, arrayId, program);
    switch (result) {
    case SaveArrayResult::InvalidId:
        programPrintError("save_array: array with id %d doesn't exist.", arrayId);
        break;
    case SaveArrayResult::ReservedKey:
        programPrintError("save_array: trying to save array under reserved key.");
        break;
    case SaveArrayResult::InvalidKeyType:
        programPrintError("save_array: invalid key type: %s.", key.typeDebugString());
        break;
    default:;
        // OK
    }
}

// load_array
static void op_load_array(Program* program)
{
    auto key = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(LoadArray(key, program)));
}

// array_key
static void op_get_array_key(Program* program)
{
    auto index = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    auto value = GetArrayKey(arrayId, index, program);
    programStackPushValue(program, value);
}

// create_array
static void op_create_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);
    auto arrayId = CreateArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// temp_array
static void op_temp_array(Program* program)
{
    auto flags = programStackPopInteger(program);
    auto len = programStackPopInteger(program);

    // Special case for array sub-expressions.
    if ((flags & SFALL_ARRAYFLAG_EXPR_POP) != 0) {
        PopExpressionArray();
        programStackPushInteger(program, 0);
        return;
    }

    auto arrayId = CreateTempArray(len, flags);
    programStackPushInteger(program, arrayId);
}

// fix_array
static void op_fix_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FixArray(arrayId);
}

// string_split
static void op_string_split(Program* program)
{
    auto split = programStackPopString(program);
    auto str = programStackPopString(program);
    auto arrayId = StringSplit(str, split);
    programStackPushInteger(program, arrayId);
}

// set_array
static void op_set_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    SetArray(arrayId, key, value, true, program);
}

// This special opcode is used to implement array expressions.
// It should always push 0 on the stack.
// arrayexpr
static void op_arrayexpr(Program* program)
{
    auto value = programStackPopValue(program);
    auto key = programStackPopValue(program);
    SetArrayFromExpression(key, value, program);
    programStackPushInteger(program, 0);
}

// arrays_equal(array1, array2) -> int
// Returns 1 if both arrays have the same keys and values, 0 otherwise.
// F-003 (M-8): Implements the missing arrays_equal function.
static void op_arrays_equal(Program* program)
{
    auto array2 = static_cast<ArrayId>(programStackPopInteger(program));
    auto array1 = static_cast<ArrayId>(programStackPopInteger(program));
    programStackPushInteger(program, ArraysEqual(array1, array2, program));
}

// array_filter(array, filterProcedurePtr) -> newArrayId
// Creates a new array containing only elements that pass the filter.
// The filter procedure is called for each element value and the return
// value determines inclusion (non-zero = keep, 0 = discard).
// F-004 (M-9): Implements the missing array_filter function.
static void op_array_filter(Program* program)
{
    auto procedureIndex = programStackPopInteger(program);
    auto arrayId = static_cast<ArrayId>(programStackPopInteger(program));
    auto resultId = ArrayFilter(arrayId, program, procedureIndex);
    programStackPushInteger(program, static_cast<int>(resultId));
}

// array_transform(array, transformProcedurePtr) -> newArrayId
// Creates a new array with each element value replaced by the transform
// procedure's return value.
// F-005 (M-10): Implements the missing array_transform function.
static void op_array_transform(Program* program)
{
    auto procedureIndex = programStackPopInteger(program);
    auto arrayId = static_cast<ArrayId>(programStackPopInteger(program));
    auto resultId = ArrayTransform(arrayId, program, procedureIndex);
    programStackPushInteger(program, static_cast<int>(resultId));
}

// scan_array
static void op_scan_array(Program* program)
{
    auto value = programStackPopValue(program);
    auto arrayId = programStackPopInteger(program);
    auto returnValue = ScanArray(arrayId, value, program);
    programStackPushValue(program, returnValue);
}

// get_array
static void op_get_array(Program* program)
{
    auto key = programStackPopValue(program);
    auto arrayId = programStackPopValue(program);

    if (arrayId.isInt()) {
        auto value = GetArray(arrayId.integerValue, key, program);
        programStackPushValue(program, value);
    } else if (arrayId.isString() && key.isInt()) {
        auto pos = key.asInt();
        auto str = programGetString(program, arrayId.opcode, arrayId.integerValue);

        char buf[2] = { 0 };
        if (pos < strlen(str)) {
            buf[0] = str[pos];
            programStackPushString(program, buf);
        } else {
            programStackPushString(program, buf);
        }
    } else {
        // F-648: Float/pointer arrayId silently pushed 0 with no diagnostic.
        // Scripts cannot distinguish "empty element" from "invalid arrayId type".
        // Log the error so mod authors can diagnose the crash.
        programPrintError("get_array: arrayId must be int or string, got %s",
            arrayId.typeDebugString());
        programStackPushInteger(program, 0);
    }
}

// free_array
static void op_free_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    FreeArray(arrayId);
}

// len_array
static void op_len_array(Program* program)
{
    auto arrayId = programStackPopInteger(program);
    programStackPushInteger(program, LenArray(arrayId));
}

// resize_array
static void op_resize_array(Program* program)
{
    auto newLen = programStackPopInteger(program);
    auto arrayId = programStackPopInteger(program);
    ResizeArray(arrayId, newLen);
}

// party_member_list
static void op_party_member_list(Program* program)
{
    auto includeHidden = programStackPopInteger(program);
    auto objects = get_all_party_members_objects(includeHidden);
    // CreateTempArray(0, flags) creates an associative array per sfall convention
    // (len <= 0 forces ASSOC flag).  For an empty list, pass count=1 to force
    // list type, then immediately resize to 0 to produce a valid empty list.
    int count = static_cast<int>(objects.size());
    auto arrayId = CreateTempArray(count > 0 ? count : 1, SFALL_ARRAYFLAG_RESERVED);
    if (count == 0) {
        ResizeArray(arrayId, 0);
    }
    for (int i = 0; i < count; i++) {
        SetArray(arrayId, ProgramValue { i }, ProgramValue { objects[i] }, false, program);
    }
    programStackPushInteger(program, arrayId);
}

// typeof
static void op_type_of(Program* program)
{
    auto value = programStackPopValue(program);
    if (value.isInt()) {
        programStackPushInteger(program, 1);
    } else if (value.isFloat()) {
        programStackPushInteger(program, 2);
    } else {
        programStackPushInteger(program, 3);
    };
}

// round
static void op_round(Program* program)
{
    float floatValue = programStackPopValue(program).asFloat();
    long int result = lroundf(floatValue);

    // Guard against long-to-int overflow UB (F-012).
    // lroundf(INT_MAX + 1.0f) = 2147483648L exceeds INT_MAX.
    // Fall back to float on overflow, matching op_power pattern.
    if (result > INT_MAX || result < INT_MIN) {
        programStackPushFloat(program, static_cast<float>(result));
    } else {
        programStackPushInteger(program, static_cast<int>(result));
    }
}

enum BlockType {
    BLOCKING_TYPE_BLOCK,
    BLOCKING_TYPE_SHOOT,
    BLOCKING_TYPE_AI,
    BLOCKING_TYPE_SIGHT,
    BLOCKING_TYPE_SCROLL,
};

PathBuilderCallback* get_blocking_func(int type)
{
    switch (type) {
    case BLOCKING_TYPE_SHOOT:
        return _obj_shoot_blocking_at;
    case BLOCKING_TYPE_AI:
        return _obj_ai_blocking_at;
    case BLOCKING_TYPE_SIGHT:
        return _obj_sight_blocking_at;
    default:
        return _obj_blocking_at;
    }
}

// obj_blocking_line
static void op_make_straight_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == nullptr) {
        programStackPushPointer(program, nullptr);
        return;
    }

    int flags = type == BLOCKING_TYPE_SHOOT ? 32 : 0;

    Object* obstacle = nullptr;
    _make_straight_path_func(object, object->tile, dest, nullptr, &obstacle, flags, get_blocking_func(type));
    programStackPushPointer(program, obstacle);
}

// obj_blocking_tile
static void op_obj_blocking_at(Program* program)
{
    int type = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    PathBuilderCallback* func = get_blocking_func(type);
    Object* obstacle = func(nullptr, tile, elevation);
    if (obstacle != nullptr) {
        if (type == BLOCKING_TYPE_SHOOT) {
            if ((obstacle->flags & OBJECT_SHOOT_THRU) != OBJECT_NONE) {
                obstacle = nullptr;
            }
        }
    }
    programStackPushPointer(program, obstacle);
}

// tile_light
static void op_tile_light(Program* program)
{
    int tile = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    programStackPushInteger(program, lightGetTileIntensity(elevation, tile));
}

// tile_get_objs
static void op_tile_get_objects(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    ArrayId arrayId = CreateTempArray(0, SFALL_ARRAYFLAG_RESERVED);

    if (!hexGridTileIsValid(tile) || elevation < 0 || elevation >= ELEVATION_COUNT) {
        debugPrint("%s: op_tile_get_objects invalid tile data: tile=%d elevation=%d", program->name, tile, elevation);
        programStackPushInteger(program, arrayId);
        return;
    }

    int index = 0;
    for (Object* object = objectFindFirstAtLocation(elevation, tile); object != nullptr; object = objectFindNextAtLocation()) {
        ResizeArray(arrayId, index + 1);
        SetArray(arrayId, ProgramValue(index++), ProgramValue(object), false, program);
    }

    programStackPushInteger(program, arrayId);
}

// path_find_to
static void op_make_path(Program* program)
{
    int type = programStackPopInteger(program);
    int dest = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    ArrayId arrayId = CreateTempArray(0, 0);

    if (object == nullptr
        || !hexGridTileIsValid(dest)
        || object->elevation < 0
        || object->elevation >= ELEVATION_COUNT
        || !hexGridTileIsValid(object->tile)) {
        debugPrint("%s: op_make_path invalid input: object=%p dest=%d elevation=%d", program->name, object, dest, object != nullptr ? object->elevation : -1);
        programStackPushInteger(program, arrayId);
        return;
    }

    // sfall only requires an empty destination tile when the source object is a critter.
    int requireEmptyDest = objectTypeFromPid(object->pid) == OBJ_TYPE_CRITTER;

    // XXX: pathfinderFindPath does not accept a destination buffer length. Use the
    // same capacity as the engine's AnimationSad::rotations storage so this
    // wrapper is not the limiting factor.  Sfall uses 800 here
    unsigned char rotations[kSfallPathBufferSize];
    int pathLength = pathfinderFindPath(object, object->tile, dest, rotations, requireEmptyDest, get_blocking_func(type));
    ResizeArray(arrayId, pathLength);
    for (int index = 0; index < pathLength; index++) {
        SetArray(arrayId, ProgramValue(index), ProgramValue(static_cast<int>(rotations[index])), false, program);
    }

    programStackPushInteger(program, arrayId);
}
// sfall_func0
static void op_sfall_func0(Program* program)
{
    sfall_metarule(program, 0);
}

// sfall_func1
static void op_sfall_func1(Program* program)
{
    sfall_metarule(program, 1);
}

// sfall_func2
static void op_sfall_func2(Program* program)
{
    sfall_metarule(program, 2);
}

// sfall_func3
static void op_sfall_func3(Program* program)
{
    sfall_metarule(program, 3);
}

// sfall_func4
static void op_sfall_func4(Program* program)
{
    sfall_metarule(program, 4);
}

// sfall_func5
static void op_sfall_func5(Program* program)
{
    sfall_metarule(program, 5);
}

// sfall_func6
static void op_sfall_func6(Program* program)
{
    sfall_metarule(program, 6);
}

// sfall_func7
static void op_sfall_func7(Program* program)
{
    sfall_metarule(program, 7);
}

// sfall_func8
static void op_sfall_func8(Program* program)
{
    sfall_metarule(program, 8);
}

// ============================================================
// Virtual File System (VFS) opcodes
// sfall VFS allows scripts to read/write files via integer handles.
// Used by RPU (gl_k_goris_derobing, gl_k_walking_speed) for modifying
// FRM animation files at runtime.
// ============================================================

static constexpr int kVfsMaxFiles = 100;
// P-07: sfall caps file size at MAX_FILE_SIZE (0xA00000 = 10 MB) in both
// FScreate (FileSystem.cpp:520) and FSresize (FileSystem.cpp:690). The fork's
// real-disk VFS would otherwise create 2 GB sparse files (200 GB logical for
// 100 handles) on disk. Matching sfall's documented limit.
static constexpr int kMaxVfsFileSize = 0xA00000;
static FILE* sfallVfsFiles[kVfsMaxFiles] = {};
static bool sfallVfsFileOpen[kVfsMaxFiles] = {};
// Track open mode: 0 = unopened, 1 = read-only ("rb"), 2 = read-write ("w+b").
static int sfallVfsFileMode[kVfsMaxFiles] = {};
// H-27 / sfall NEW-2 / M-11: separate deletable flag. sfall's VFS is
// memory-only; on-disk files created by fs_create/fs_copy are the ONLY files
// a script owns and may be removed at handle-free/close-all. Files opened via
// fs_find (mode 1) — including after the fs_resize mode-1→2 reopen (F-028) —
// must NEVER be deleted: they are pre-existing game assets. The old code
// conflated "writable" with "deletable" via mode==2, so fs_resize's reopen
// flip silently made fs_find'd originals deletable (compat_remove at free).
static bool sfallVfsFileDeletable[kVfsMaxFiles] = {};
// M-12: fs_resize(id, -1) marks the file to be saved into savegames (sfall
// FileSystem.cpp:689-693 isSave semantics). The fork's disk-backed VFS
// persists files itself; the flag records the contract so the call is
// accepted instead of rejected.
static bool sfallVfsFileIsSave[kVfsMaxFiles] = {};
// Store the original filename for handles so fs_resize can reopen read-only
// handles in read-write mode when needed (F-028).
// NOTE (F-270): Filesystem paths are OWNED COPIES (via internal_strdup),
// not raw pointers into program->dynamicStrings. programStackPopString
// returns a pointer into the program's owned string storage which is freed
// on program exit — storing raw pointers would be use-after-free.
static char* sfallVfsFilePath[kVfsMaxFiles] = {};

// M3 (LOW-MEDIUM, fs_copy same-path write-through): open a VFS file for
// script use in UNBUFFERED mode. sfall's VFS is memory-only; this fork is
// disk-backed, and RPU scripts patch FRM files in place via
// fs_copy(path, path) → fs_seek → fs_read_short → fs_seek → fs_write_short
// (gl_k_walking_speed.ssl:88-93, gl_k_goris_derobing.ssl:40-46) with no
// fs_close/fs_delete afterwards. With default buffered stdio (compat_fopen =
// plain fopen, platform_compat.cc:430-447) the small write sits in the user
// buffer and the engine's INDEPENDENT fopen (art loader art.cc:1088-1108)
// sees the ORIGINAL bytes — the script patch silently never takes effect.
// _IONBF makes every script write hit the OS immediately so independent
// readers observe the patched content. Read-only handles (fs_find, the
// fs_copy source temp, the fs_resize "rb" fallback) stay with plain
// compat_fopen: they cannot carry writes, and unbuffered fgetc loops
// (fs_read_string/fs_read_bstring) would degrade to per-byte syscalls.
static FILE* sfallVfsFopen(const char* path, const char* mode)
{
    FILE* file = compat_fopen(path, mode);
    if (file != nullptr) {
        setvbuf(file, nullptr, _IONBF, 0);
    }
    return file;
}

// Reads a source file into a heap buffer. Tries the plain OS filesystem
// first (compat_fopen — loose files under the CWD/sandbox root), then falls
// back to the engine VFS (fileOpen — directory-based mods like mods/fo1_base
// and .dat archives like master.dat/critter.dat). fs_copy sources routinely
// live inside archives or mod directories, which plain fopen cannot see:
// RPU's gl_k_goris_derobing.int patches art\critters\*.frm from critter.dat,
// et tu's gl_classic_wm.int copies art\INTRFACE\classicWM\*.frm from the
// fo1_base directory mod. Returns true and allocates *outData/*outSize on
// success; caller frees with internal_free.
static bool sfallVfsReadFile(const char* vfsPath, const char* resolvedPath, unsigned char** outData, int* outSize)
{
    // Fast path: plain disk file.
    FILE* file = compat_fopen(resolvedPath, "rb");
    if (file == nullptr) {
        // VFS fallback: engine fileOpen searches directory bases and .dat
        // archives. Read the whole file through the engine's File API.
        File* stream = fileOpen(vfsPath, "rb");
        if (stream == nullptr) {
            return false;
        }
        int size = fileGetSize(stream);
        if (size <= 0) {
            fileClose(stream);
            return false;
        }
        if (size > kMaxVfsFileSize) {
            debugPrint("sfallVfsReadFile: '%s' exceeds sfall MAX_FILE_SIZE (0xA00000)\n", vfsPath);
            fileClose(stream);
            return false;
        }
        unsigned char* data = (unsigned char*)internal_malloc_safe(size, __FILE__, __LINE__);
        size_t read = fileRead(data, 1, size, stream);
        fileClose(stream);
        if (read != static_cast<size_t>(size)) {
            internal_free_safe(data, __FILE__, __LINE__);
            return false;
        }
        *outData = data;
        *outSize = size;
        return true;
    }

    // Plain file: read it the same way the rest of fs_copy does.
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long size = ftell(file);
    if (size <= 0 || size > kMaxVfsFileSize) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    unsigned char* data = (unsigned char*)internal_malloc_safe(size, __FILE__, __LINE__);
    size_t read = fread(data, 1, size, file);
    fclose(file);
    if (read != static_cast<size_t>(size)) {
        internal_free_safe(data, __FILE__, __LINE__);
        return false;
    }
    *outData = data;
    *outSize = static_cast<int>(size);
    return true;
}

static int sfallVfsAllocHandle()
{
    for (int i = 0; i < kVfsMaxFiles; i++) {
        if (!sfallVfsFileOpen[i]) {
            sfallVfsFileOpen[i] = true;
            sfallVfsFileMode[i] = 0;
            sfallVfsFileDeletable[i] = false;
            sfallVfsFileIsSave[i] = false;
            sfallVfsFilePath[i] = nullptr;
            return i;
        }
    }
    return -1;
}

static void sfallVfsFreeHandle(int id)
{
    if (id >= 0 && id < kVfsMaxFiles) {
        if (sfallVfsFiles[id] != nullptr) {
            fclose(sfallVfsFiles[id]);
            sfallVfsFiles[id] = nullptr;
        }
        // F2-062: fs_delete must actually delete the file, not just close the
        // handle and drop the in-memory slot. Call compat_remove() on the stored
        // path to remove the file from disk. fs_find handles (read-only, mode 1)
        // should NOT delete the file — only write handles (mode 2) created via
        // fs_create/fs_copy should be deletable. The mode check prevents
        // accidental deletion of data files opened via fs_find.
        // H-27 / sfall NEW-2 / M-11: use the SEPARATE deletable flag instead of
        // mode==2. The fs_resize mode-1→2 reopen (F-028) must not make an
        // fs_find'd pre-existing file deletable. Only files explicitly created
        // by fs_create/fs_copy are deletable.
        if (sfallVfsFileDeletable[id] && sfallVfsFilePath[id] != nullptr) {
            if (compat_remove(sfallVfsFilePath[id]) != 0) {
                debugPrint("sfallVfsFreeHandle: compat_remove failed for '%s'\n",
                    sfallVfsFilePath[id]);
            }
        }
        sfallVfsFileOpen[id] = false;
        sfallVfsFileMode[id] = 0;
        sfallVfsFileDeletable[id] = false;
        sfallVfsFileIsSave[id] = false;
        // F-270: free the owned path copy to prevent use-after-free
        if (sfallVfsFilePath[id] != nullptr) {
            internal_free(sfallVfsFilePath[id]);
            sfallVfsFilePath[id] = nullptr;
        }
    }
}

void sfallVfsCloseAll()
{
    for (int i = 0; i < kVfsMaxFiles; i++) {
        sfallVfsFreeHandle(i);
    }
}

// Reject paths containing ".." components — prevents path traversal attacks
// (I2-F-014). compat_resolve_path() only case-normalizes; it does not strip or
// reject path separators or directory traversal sequences.
static bool sfallVfsPathContainsTraversal(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return true; // empty path is invalid
    }
    // Check for literal ".." as a path component (preceded by start-of-string,
    // "/", or "\").
    if (strncmp(path, "..", 2) == 0
        && (path[2] == '\0' || path[2] == '/' || path[2] == '\\')) {
        return true;
    }
    const char* p = path;
    while (*p != '\0') {
        p = strpbrk(p, "/\\");
        if (p == nullptr) {
            break;
        }
        p++; // skip the separator
        if (strncmp(p, "..", 2) == 0
            && (p[2] == '\0' || p[2] == '/' || p[2] == '\\')) {
            return true;
        }
    }
    return false;
}

// F-083: VFS root directory for sandbox enforcement.
// When set, all VFS paths are resolved relative to this root.
// Absolute paths (starting with '/' or '\') are always rejected.
// Paths containing ".." are rejected by sfallVfsPathContainsTraversal.
// This prevents scripts from accessing files outside the game directory.
static char* sfallVfsRootDir = nullptr;

void sfallVfsSetRoot(const char* root)
{
    if (sfallVfsRootDir != nullptr) {
        internal_free(sfallVfsRootDir);
        sfallVfsRootDir = nullptr;
    }
    if (root != nullptr && root[0] != '\0') {
        sfallVfsRootDir = internal_strdup(root);
    }
}

// Simple wildcard pattern matcher supporting '*' (matches any sequence of
// characters) and '?' (matches any single character). Used by fs_find for
// glob-based file searching. Returns true if 'name' matches 'pattern'.
// Does NOT support character classes ([abc]), brace expansion, or other
// extended glob syntax — this matches the subset supported by original sfall.
// F-006 (M-11): Implements the wildcard matching that sfall's fs_find uses
// for glob-based file searching.
static bool sfallVfsGlobMatch(const char* pattern, const char* name)
{
    const char* star = nullptr;
    const char* nameAfterStar = nullptr;

    while (*name != '\0') {
        if (*pattern == '*') {
            star = pattern++;
            nameAfterStar = name;
        } else if (*pattern == '?' || *pattern == *name) {
            pattern++;
            name++;
        } else if (star != nullptr) {
            pattern = star + 1;
            name = ++nameAfterStar;
        } else {
            return false;
        }
    }

    // Consume any trailing '*' characters in the pattern.
    while (*pattern == '*') {
        pattern++;
    }

    return *pattern == '\0';
}

// Check if a path contains any wildcard characters that would indicate
// a glob pattern rather than a literal filename.
static bool sfallVfsPathHasGlob(const char* path)
{
    if (path == nullptr) return false;
    while (*path != '\0') {
        if (*path == '*' || *path == '?') {
            return true;
        }
        path++;
    }
    return false;
}

// Resolve a VFS path against the sandbox root. Returns true and writes
// the resolved path to outBuf (size COMPAT_MAX_PATH) on success.
// Returns false if the path is invalid (absolute, empty, contains "..",
// or the resolved path would exceed COMPAT_MAX_PATH).
static bool sfallVfsResolvePath(const char* rawPath, char* outBuf, size_t outBufSize)
{
    if (rawPath == nullptr || rawPath[0] == '\0') {
        return false;
    }

    // 1. Check for ".." traversal (redundant with caller checks; defense-in-depth).
    if (sfallVfsPathContainsTraversal(rawPath)) {
        return false;
    }

    // 2. Reject absolute paths unconditionally (F-083).
    // Scripts should only use relative paths within the game directory.
    if (rawPath[0] == '/' || rawPath[0] == '\\') {
        return false;
    }

    // P-06: Reject Windows drive-letter paths (C:\..., c:/..., C:foo). The
    // sibling compat_path_contains_traversal guard (platform_compat.cc:522-525)
    // rejects drive letters but was never wired into the VFS layer, so on
    // Windows a script could fs_create("C:\Users\victim\file.txt") and
    // truncate/read/delete arbitrary user files outside the sandbox. Mirror
    // that check here — a drive letter is an absolute path in Windows terms.
    if (rawPath[0] != '\0' && rawPath[1] == ':'
        && ((rawPath[0] >= 'A' && rawPath[0] <= 'Z')
            || (rawPath[0] >= 'a' && rawPath[0] <= 'z'))) {
        return false;
    }

    // 3. If root is set, prepend it.
    if (sfallVfsRootDir != nullptr) {
        int written = snprintf(outBuf, outBufSize, "%s/%s", sfallVfsRootDir, rawPath);
        if (written < 0 || static_cast<size_t>(written) >= outBufSize) {
            return false;
        }
    } else {
        // No sandbox root — use path as-is.
        strncpy(outBuf, rawPath, outBufSize - 1);
        outBuf[outBufSize - 1] = '\0';
    }
    return true;
}

// fs_create(path, size) -> fileId (or -1 on error)
static void op_fs_create(Program* program)
{
    int size = programStackPopInteger(program);
    const char* path = programStackPopString(program);

    if (sfallVfsPathContainsTraversal(path)) {
        programPrintError("fs_create: path traversal rejected '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    // F-083: Resolve path against sandbox root.
    char resolvedPath[COMPAT_MAX_PATH];
    if (!sfallVfsResolvePath(path, resolvedPath, sizeof(resolvedPath))) {
        programPrintError("fs_create: path rejected by sandbox '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    int handle = sfallVfsAllocHandle();
    if (handle < 0) {
        programPrintError("fs_create: no free VFS handles");
        programStackPushInteger(program, -1);
        return;
    }

    // P-07: sfall FScreate rejects size > MAX_FILE_SIZE (0xA00000 = 10 MB).
    // Validate BEFORE opening the file so an oversized request cannot create
    // a sparse 2 GB file on the real disk.
    if (size > kMaxVfsFileSize) {
        programPrintError("fs_create: size %d exceeds sfall MAX_FILE_SIZE (0xA00000)", size);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    // Validate size before fseek to prevent UB with size <= 0
    if (size <= 0) {
        programPrintError("fs_create: invalid size %d", size);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    // sfall NEW-2: refuse to truncate pre-existing assets. sfall's VFS is
    // memory-only — FScreate never touches an existing on-disk file. The
    // fork's "w+b" open would truncate any real file at the resolved path
    // (including game assets in loose-data deployments), so refuse when the
    // target already exists. Scripts that need to overwrite their own
    // previously-created file should fs_delete the handle first.
    if (compat_access(resolvedPath, F_OK) == 0) {
        programPrintError("fs_create: refusing to truncate existing file '%s'", resolvedPath);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    FILE* file = sfallVfsFopen(resolvedPath, "w+b");
    if (file == nullptr) {
        programPrintError("fs_create: cannot create file '%s'", resolvedPath);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }

    // Allocate the requested size (simple approach: seek + write)
    // F-48: Check fseek return value matching the pattern used by
    // fs_size (lines 2517, 2528) and fs_seek (line 2582).
    if (fseek(file, static_cast<long>(size) - 1, SEEK_SET) != 0) {
        programPrintError("fs_create: fseek failed for '%s'", resolvedPath);
        fclose(file);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }
    if (fputc(0, file) == EOF) {
        programPrintError("fs_create: fputc failed for '%s'", resolvedPath);
        fclose(file);
        sfallVfsFileOpen[handle] = false;
        programStackPushInteger(program, -1);
        return;
    }
    rewind(file);

    sfallVfsFiles[handle] = file;
    sfallVfsFileMode[handle] = 2; // read-write ("w+b")
    sfallVfsFileDeletable[handle] = true; // script-created file may be deleted at free
    // F-270: store an owned copy to prevent use-after-free when the
    // program exits and frees its dynamicStrings backing store.
    sfallVfsFilePath[handle] = internal_strdup(resolvedPath);
    programStackPushInteger(program, handle);
}

// fs_copy(path, source_path) -> fileId (or -1 on error)
static void op_fs_copy(Program* program)
{
    const char* sourcePath = programStackPopString(program);
    const char* destPath = programStackPopString(program);

    if (sfallVfsPathContainsTraversal(sourcePath)) {
        programPrintError("fs_copy: path traversal rejected in source '%s'", sourcePath);
        programStackPushInteger(program, -1);
        return;
    }
    if (sfallVfsPathContainsTraversal(destPath)) {
        programPrintError("fs_copy: path traversal rejected in dest '%s'", destPath);
        programStackPushInteger(program, -1);
        return;
    }

    // F-083: Resolve paths against sandbox root.
    char resolvedSrc[COMPAT_MAX_PATH];
    char resolvedDst[COMPAT_MAX_PATH];
    if (!sfallVfsResolvePath(sourcePath, resolvedSrc, sizeof(resolvedSrc))) {
        programPrintError("fs_copy: source path rejected by sandbox '%s'", sourcePath);
        programStackPushInteger(program, -1);
        return;
    }
    if (!sfallVfsResolvePath(destPath, resolvedDst, sizeof(resolvedDst))) {
        programPrintError("fs_copy: dest path rejected by sandbox '%s'", destPath);
        programStackPushInteger(program, -1);
        return;
    }

    // C-06 / RPU P1: accept identical source/destination paths. sfall's
    // FScopy is memory-only and accepts same-path copies; RPU's UPU scripts
    // call fs_copy(path, path) for in-place FRM FPS patching
    // (gl_k_goris_derobing.ssl:40, gl_k_walking_speed.ssl:88) — without this
    // both features are inert on CE. The fork's disk-backed copy would
    // truncate the source with a "w+b" destination open, so identical paths
    // get a NON-TRUNCATING read-write handle ("r+b") over the original file:
    // the file content is never destroyed (C-06 safety intent preserved) and
    // the handle is marked non-deletable so handle-free never removes the
    // original asset (H-27 family). Compare the RESOLVED paths so different
    // spellings that normalize to the same file are also handled this way.
    if (compat_stricmp(resolvedSrc, resolvedDst) == 0) {
        // The patched file may live inside a .dat archive or a directory mod
        // (art\critters\*.frm in critter.dat, art\INTRFACE\*.frm in mods) —
        // plain fopen cannot open archive members, and r+b on an archive
        // path would fail even if the source were a loose file. Read the
        // source through the engine VFS and MATERIALIZE it to disk so the
        // script's fs_seek/fs_read_short/fs_write_short patch a real file
        // the engine (art loader, fileOpen) will read back.
        //
        // The materialized copy MUST land in the patches directory
        // (master_patches_path, e.g. "data/"): the engine VFS searches
        // patches dirs BEFORE .dat archives, so a file written there
        // shadows the archive copy. Writing to the resolved path itself
        // (CWD-relative) would be shadowed BY the archive — the patch
        // would silently never take effect.
        unsigned char* srcData = nullptr;
        int srcSize = 0;
        if (!sfallVfsReadFile(sourcePath, resolvedSrc, &srcData, &srcSize)) {
            programPrintError("fs_copy: cannot open identical source/dest '%s' for read", resolvedSrc);
            programStackPushInteger(program, -1);
            return;
        }

        char materializedPath[COMPAT_MAX_PATH];
        const std::string& patchesDir = settings.system.master_patches_path;
        if (!patchesDir.empty() && compat_is_dir(patchesDir.c_str())) {
            // Prepend the patches dir to the RESOLVED path. The resolved
            // path is either sandbox-rooted or CWD-relative; the patches
            // dir is the VFS-searchable location that precedes archives.
            snprintf(materializedPath, sizeof(materializedPath), "%s\\%s", patchesDir.c_str(), resolvedSrc);
        } else {
            snprintf(materializedPath, sizeof(materializedPath), "%s", resolvedSrc);
        }

        // Ensure the parent directory exists so the materialized file can be
        // created at the target path.
        {
            char dir[COMPAT_MAX_PATH];
            compat_splitpath(materializedPath, nullptr, dir, nullptr, nullptr);
            if (dir[0] != '\0') {
                compat_mkdir_recursive(dir);
            }
        }

        FILE* inPlaceFile = sfallVfsFopen(materializedPath, "w+b");
        if (inPlaceFile == nullptr) {
            programPrintError("fs_copy: cannot materialize identical source/dest '%s' for read-write", materializedPath);
            internal_free_safe(srcData, __FILE__, __LINE__);
            programStackPushInteger(program, -1);
            return;
        }

        size_t written = fwrite(srcData, 1, srcSize, inPlaceFile);
        internal_free_safe(srcData, __FILE__, __LINE__);
        if (written != static_cast<size_t>(srcSize)) {
            programPrintError("fs_copy: fwrite failed materializing '%s'", materializedPath);
            fclose(inPlaceFile);
            programStackPushInteger(program, -1);
            return;
        }

        rewind(inPlaceFile); // position at start for reading

        int handle = sfallVfsAllocHandle();
        if (handle < 0) {
            programPrintError("fs_copy: no free VFS handles");
            fclose(inPlaceFile);
            programStackPushInteger(program, -1);
            return;
        }

        sfallVfsFiles[handle] = inPlaceFile;
        sfallVfsFileMode[handle] = 2; // read-write ("w+b")
        sfallVfsFileDeletable[handle] = false; // original game asset — never delete
        sfallVfsFilePath[handle] = internal_strdup(materializedPath);
        programStackPushInteger(program, handle);
        return;
    }

    // Read the source through the engine VFS (loose files, directory mods,
    // and .dat archives alike). Buffered read-only temp, closed before the
    // dest is handed to the script — no write-through need.
    unsigned char* srcData = nullptr;
    int srcSize = 0;
    if (!sfallVfsReadFile(sourcePath, resolvedSrc, &srcData, &srcSize)) {
        programPrintError("fs_copy: cannot open source '%s'", resolvedSrc);
        programStackPushInteger(program, -1);
        return;
    }

    int handle = sfallVfsAllocHandle();
    if (handle < 0) {
        programPrintError("fs_copy: no free VFS handles");
        internal_free_safe(srcData, __FILE__, __LINE__);
        programStackPushInteger(program, -1);
        return;
    }

    // Materialize the destination into the patches directory when one is
    // configured, so the engine VFS (art loader, fileOpen) finds the copy
    // BEFORE the .dat archives / mod directory it may shadow (e.g. et tu's
    // gl_classic_wm.int swapping art\INTRFACE\*.frm). Without this, a
    // CWD-relative dest would be shadowed by the archive copy and the
    // swap would silently never take effect.
    char materializedDst[COMPAT_MAX_PATH];
    const std::string& patchesDir = settings.system.master_patches_path;
    if (!patchesDir.empty() && compat_is_dir(patchesDir.c_str())) {
        snprintf(materializedDst, sizeof(materializedDst), "%s\\%s", patchesDir.c_str(), resolvedDst);
    } else {
        snprintf(materializedDst, sizeof(materializedDst), "%s", resolvedDst);
    }

    // Ensure the destination's parent directory exists.
    {
        char dir[COMPAT_MAX_PATH];
        compat_splitpath(materializedDst, nullptr, dir, nullptr, nullptr);
        if (dir[0] != '\0') {
            compat_mkdir_recursive(dir);
        }
    }

    FILE* destFile = sfallVfsFopen(materializedDst, "w+b");
    if (destFile == nullptr) {
        programPrintError("fs_copy: cannot create dest '%s'", materializedDst);
        sfallVfsFileOpen[handle] = false;
        internal_free_safe(srcData, __FILE__, __LINE__);
        programStackPushInteger(program, -1);
        return;
    }

    // Copy the buffered file contents with fwrite error checking (F2-064).
    size_t written = fwrite(srcData, 1, srcSize, destFile);
    internal_free_safe(srcData, __FILE__, __LINE__);
    if (written != static_cast<size_t>(srcSize)) {
        programPrintError("fs_copy: fwrite failed (wrote %zu of %d bytes) for '%s'",
            written, srcSize, materializedDst);
        fclose(destFile);
        sfallVfsFileOpen[handle] = false;
        compat_remove(materializedDst);
        programStackPushInteger(program, -1);
        return;
    }

    rewind(destFile); // position at start for reading
    sfallVfsFiles[handle] = destFile;
    sfallVfsFileMode[handle] = 2; // read-write ("w+b")
    sfallVfsFileDeletable[handle] = true; // script-created copy may be deleted at free
    // F-270: store an owned copy to prevent use-after-free when the
    // program exits and frees its dynamicStrings backing store.
    sfallVfsFilePath[handle] = internal_strdup(materializedDst);
    programStackPushInteger(program, handle);
}

// fs_find(path) -> fileId (or -1 if not found)
// Supports wildcard/glob patterns ('*' and '?') for file searching.
// When the path contains wildcards, the directory portion is resolved
// and the first matching file is opened. When the path is a literal
// filename (no wildcards), the existing direct fopen behavior is used.
// F-006 (M-11): Implements wildcard pattern matching. Original code
// did plain fopen with no glob support, matching sfall's fs_find API.
static void op_fs_find(Program* program)
{
    const char* path = programStackPopString(program);

    if (sfallVfsPathContainsTraversal(path)) {
        programPrintError("fs_find: path traversal rejected '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    // F-083: Resolve path against sandbox root.
    char resolvedPath[COMPAT_MAX_PATH];
    if (!sfallVfsResolvePath(path, resolvedPath, sizeof(resolvedPath))) {
        programPrintError("fs_find: path rejected by sandbox '%s'", path);
        programStackPushInteger(program, -1);
        return;
    }

    // Check if the resolved path contains wildcards.
    const char* lastSlash = nullptr;
    const char* pattern = resolvedPath;
    for (const char* p = resolvedPath; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            lastSlash = p;
        }
    }

    if (lastSlash != nullptr) {
        pattern = lastSlash + 1;
    }

    // If no wildcards in the pattern, use the fast path (direct fopen).
    // Buffered on purpose: read-only handle (mode 1), no write-through need.
    if (!sfallVfsPathHasGlob(pattern)) {
        FILE* file = compat_fopen(resolvedPath, "rb");
        if (file == nullptr) {
            programStackPushInteger(program, -1);
            return;
        }

        int handle = sfallVfsAllocHandle();
        if (handle < 0) {
            programPrintError("fs_find: no free VFS handles");
            fclose(file);
            programStackPushInteger(program, -1);
            return;
        }

        sfallVfsFiles[handle] = file;
        sfallVfsFileMode[handle] = 1; // read-only ("rb")
        sfallVfsFilePath[handle] = internal_strdup(resolvedPath);
        programStackPushInteger(program, handle);
        return;
    }

    // Wildcard path: extract the directory portion and list its contents.
    char dirPath[COMPAT_MAX_PATH];
    if (lastSlash != nullptr) {
        size_t dirLen = static_cast<size_t>(lastSlash - resolvedPath);
        if (dirLen >= sizeof(dirPath)) {
            dirLen = sizeof(dirPath) - 1;
        }
        memcpy(dirPath, resolvedPath, dirLen);
        dirPath[dirLen] = '\0';
    } else {
        // No directory separator — pattern is relative to CWD.
        strncpy(dirPath, ".", sizeof(dirPath) - 1);
        dirPath[sizeof(dirPath) - 1] = '\0';
    }

    DIR* dir = opendir(dirPath);
    if (dir == nullptr) {
        programPrintError("fs_find: cannot open directory '%s' for glob search", dirPath);
        programStackPushInteger(program, -1);
        return;
    }

    char foundPath[COMPAT_MAX_PATH];
    bool matchFound = false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip "." and ".." entries.
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Only match regular files (not directories, symlinks, etc.).
        // We construct the full path for stat, but only if it fits.
        char fullPath[COMPAT_MAX_PATH];
        if (lastSlash != nullptr) {
            int written = snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);
            if (written < 0 || static_cast<size_t>(written) >= sizeof(fullPath)) {
                continue;
            }
        } else {
            // Pattern was just a filename with wildcards, no directory.
            strncpy(fullPath, entry->d_name, sizeof(fullPath) - 1);
            fullPath[sizeof(fullPath) - 1] = '\0';
        }

        struct stat st;
        if (stat(fullPath, &st) != 0) {
            continue;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        if (sfallVfsGlobMatch(pattern, entry->d_name)) {
            strncpy(foundPath, fullPath, sizeof(foundPath) - 1);
            foundPath[sizeof(foundPath) - 1] = '\0';
            matchFound = true;
            break;
        }
    }
    closedir(dir);

    if (!matchFound) {
        programStackPushInteger(program, -1);
        return;
    }

    // Buffered on purpose: read-only handle (mode 1), no write-through need.
    FILE* file = compat_fopen(foundPath, "rb");
    if (file == nullptr) {
        programStackPushInteger(program, -1);
        return;
    }

    int handle = sfallVfsAllocHandle();
    if (handle < 0) {
        programPrintError("fs_find: no free VFS handles");
        fclose(file);
        programStackPushInteger(program, -1);
        return;
    }

    sfallVfsFiles[handle] = file;
    sfallVfsFileMode[handle] = 1; // read-only ("rb")
    sfallVfsFilePath[handle] = internal_strdup(foundPath);
    programStackPushInteger(program, handle);
}

// fs_write_byte(id, data)
static void op_fs_write_byte(Program* program)
{
    int data = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_byte: invalid VFS handle %d", id);
        return;
    }

    if (sfallVfsFileMode[id] == 1) {
        programPrintError("fs_write_byte: handle %d is read-only (opened via fs_find)", id);
        return;
    }

    // F2-064: check fputc return value.
    if (fputc(data & 0xFF, sfallVfsFiles[id]) == EOF) {
        programPrintError("fs_write_byte: fputc failed on handle %d", id);
    }
}

// fs_write_short(id, data)
static void op_fs_write_short(Program* program)
{
    int data = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_short: invalid VFS handle %d", id);
        return;
    }

    if (sfallVfsFileMode[id] == 1) {
        programPrintError("fs_write_short: handle %d is read-only (opened via fs_find)", id);
        return;
    }

    uint16_t value = static_cast<uint16_t>(data);
    // M-07 (F-07): sfall FSwrite_short writes the bytes REVERSED (big-endian).
    // The fork's native little-endian fwrite broke cross-engine file exchange.
    unsigned char be[2];
    be[0] = static_cast<unsigned char>((value >> 8) & 0xFF);
    be[1] = static_cast<unsigned char>(value & 0xFF);
    // F2-064: check fwrite return value.
    if (fwrite(be, sizeof(be), 1, sfallVfsFiles[id]) != 1) {
        programPrintError("fs_write_short: fwrite failed on handle %d", id);
    }
}

// fs_write_int(id, data)
static void op_fs_write_int(Program* program)
{
    int data = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_int: invalid VFS handle %d", id);
        return;
    }

    if (sfallVfsFileMode[id] == 1) {
        programPrintError("fs_write_int: handle %d is read-only (opened via fs_find)", id);
        return;
    }

    int32_t value = static_cast<int32_t>(data);
    // M-07 (F-07): sfall FSwrite_int writes the bytes REVERSED (big-endian).
    unsigned char be[4];
    be[0] = static_cast<unsigned char>((static_cast<uint32_t>(value) >> 24) & 0xFF);
    be[1] = static_cast<unsigned char>((static_cast<uint32_t>(value) >> 16) & 0xFF);
    be[2] = static_cast<unsigned char>((static_cast<uint32_t>(value) >> 8) & 0xFF);
    be[3] = static_cast<unsigned char>(static_cast<uint32_t>(value) & 0xFF);
    // F2-064: check fwrite return value.
    if (fwrite(be, sizeof(be), 1, sfallVfsFiles[id]) != 1) {
        programPrintError("fs_write_int: fwrite failed on handle %d", id);
    }
}

// M-07 (F-10): fs_write_float is REMOVED — sfall has no fs_write_float
// opcode. 0x81FD is registered as fs_write_int (a duplicate of 0x81FC,
// Opcodes.cpp:137-138); the "fs_write_float" name in the opcode-list docs
// is a doc artifact. The old op_fs_write_float handler (which wrote a
// native IEEE float) is deleted; scripts that need a float wire value use
// fs_write_int with the bit pattern (matching sfall op_fs_read_float, which
// reads an int via FSread_int and reinterprets it as a float).

// fs_write_string(id, string)
static void op_fs_write_string(Program* program)
{
    const char* string = programStackPopString(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_string: invalid VFS handle %d", id);
        return;
    }

    if (sfallVfsFileMode[id] == 1) {
        programPrintError("fs_write_string: handle %d is read-only (opened via fs_find)", id);
        return;
    }

    // M-07 (F-08): sfall FSwrite_string writes strlen+1 bytes INCLUDING the
    // NUL terminator (FileSystem.cpp:623-628). The fork's fputs omitted the
    // NUL, so fs_read_string never saw the terminator it expects.
    size_t len = strlen(string);
    // F2-064: check fwrite return value.
    if (fwrite(string, 1, len + 1, sfallVfsFiles[id]) != len + 1) {
        programPrintError("fs_write_string: fwrite failed on handle %d", id);
    }
}

// fs_write_bstring(id, string) — writes a length-prefixed byte string
static void op_fs_write_bstring(Program* program)
{
    const char* string = programStackPopString(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_write_bstring: invalid VFS handle %d", id);
        return;
    }

    if (sfallVfsFileMode[id] == 1) {
        programPrintError("fs_write_bstring: handle %d is read-only (opened via fs_find)", id);
        return;
    }

    // M-07 (F-09): sfall FSwrite_bstring writes the RAW string bytes with NO
    // length prefix (FileSystem.cpp:630-635). The fork's 1-byte length prefix
    // broke the sfall wire format.
    size_t len = strlen(string);
    // F2-064: check fwrite return value.
    if (fwrite(string, 1, len, sfallVfsFiles[id]) != len) {
        programPrintError("fs_write_bstring: fwrite failed on handle %d", id);
    }
}

// fs_read_byte(id) -> int (-1 on error)
static void op_fs_read_byte(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_byte: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    // M-07 (F-04): sfall FSread_byte returns 0 past the end of the file,
    // not -1 (FileSystem.cpp:637-641). The fork's raw fgetc pushed EOF (-1),
    // so scripts checking for end-of-file got a negative value instead of 0.
    int value = fgetc(sfallVfsFiles[id]);
    programStackPushInteger(program, value == EOF ? 0 : value);
}

// fs_read_short(id) -> int (-1 on error)
static void op_fs_read_short(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_short: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    // M-07 (F-05): sfall FSread_short returns 0 at EOF and reads the bytes
    // REVERSED (big-endian, FileSystem.cpp:643-654). The fork read native
    // little-endian and pushed -1 at EOF.
    unsigned char be[2];
    if (fread(be, 1, sizeof(be), sfallVfsFiles[id]) != sizeof(be)) {
        programStackPushInteger(program, 0);
        return;
    }
    uint16_t value = (static_cast<uint16_t>(be[0]) << 8) | static_cast<uint16_t>(be[1]);
    programStackPushInteger(program, static_cast<int>(static_cast<int16_t>(value)));
}

// fs_read_int(id) -> int (-1 on error)
static void op_fs_read_int(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_int: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    // M-07 (F-05): sfall FSread_int returns 0 at EOF and reads the bytes
    // REVERSED (big-endian, FileSystem.cpp:655-665). The fork read native
    // little-endian and pushed -1 at EOF.
    unsigned char be[4];
    if (fread(be, 1, sizeof(be), sfallVfsFiles[id]) != sizeof(be)) {
        programStackPushInteger(program, 0);
        return;
    }
    uint32_t raw = (static_cast<uint32_t>(be[0]) << 24)
        | (static_cast<uint32_t>(be[1]) << 16)
        | (static_cast<uint32_t>(be[2]) << 8)
        | static_cast<uint32_t>(be[3]);
    programStackPushInteger(program, static_cast<int>(static_cast<int32_t>(raw)));
}

// fs_read_string(id) -> string (empty string on error)
// Reads a null-terminated string from the VFS file. Reads up to 1024
// bytes including the null terminator. Returns empty string on error.
// F-002 (M-7): Implements the missing VFS string read path. Write
// counterparts (fs_write_string at 0x81fe, fs_write_bstring at 0x8208)
// exist but no read path for strings — RPU/ET Tu scripts that write
// strings via fs_write_string had no way to read them back.
static void op_fs_read_string(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_string: invalid VFS handle %d", id);
        programStackPushString(program, "");
        return;
    }

    // Read bytes until null terminator or EOF, max 1024 chars.
    char buf[1025];
    int i = 0;
    int c;
    while (i < 1024 && (c = fgetc(sfallVfsFiles[id])) != EOF && c != '\0') {
        buf[i++] = static_cast<char>(c);
    }
    buf[i] = '\0';
    programStackPushString(program, buf);
}

// fs_read_bstring(id) -> string (empty string on error)
// Reads a byte string from the VFS file. Matches the wire format written by
// fs_write_bstring, which (per sfall FSwrite_bstring, FileSystem.cpp:630-635)
// writes RAW string bytes with NO length prefix. Reads up to 255 bytes until
// EOF. Returns empty string on error.
// F-002 (M-7): Implements the missing VFS bstring read path.
static void op_fs_read_bstring(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_bstring: invalid VFS handle %d", id);
        programStackPushString(program, "");
        return;
    }

    // Read raw bytes until EOF, max 255 chars (buffer size).
    char buf[256];
    size_t nread = 0;
    int c;
    while (nread < sizeof(buf) - 1 && (c = fgetc(sfallVfsFiles[id])) != EOF) {
        buf[nread++] = static_cast<char>(c);
    }
    buf[nread] = '\0';
    programStackPushString(program, buf);
}

// fs_read_float(id) -> float (0.0 on error)
static void op_fs_read_float(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_read_float: invalid VFS handle %d", id);
        programStackPushFloat(program, 0.0f);
        return;
    }

    // M-07 (F-06): sfall op_fs_read_float reads an INT via FSread_int
    // (big-endian, Handlers/FileSystem.cpp:75-77) and reinterprets the 4
    // bytes as a FLOAT. The fork read a native little-endian float directly.
    unsigned char be[4];
    if (fread(be, 1, sizeof(be), sfallVfsFiles[id]) != sizeof(be)) {
        programStackPushFloat(program, 0.0f);
        return;
    }
    uint32_t raw = (static_cast<uint32_t>(be[0]) << 24)
        | (static_cast<uint32_t>(be[1]) << 16)
        | (static_cast<uint32_t>(be[2]) << 8)
        | static_cast<uint32_t>(be[3]);
    float value;
    memcpy(&value, &raw, sizeof(value));
    programStackPushFloat(program, value);
}

// fs_delete(id)
static void op_fs_delete(Program* program)
{
    int id = programStackPopInteger(program);
    sfallVfsFreeHandle(id);
}

// fs_size(id) -> int (-1 on error)
static void op_fs_size(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_size: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    // I2-M01: Check all ftell/fseek return values. ftell returns -1L on
    // error; fseek returns non-zero on error. Using a failed ftell result
    // in a subsequent fseek is UB.
    long pos = ftell(sfallVfsFiles[id]);
    if (pos == -1L) {
        programPrintError("fs_size: ftell failed on handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }
    if (fseek(sfallVfsFiles[id], 0, SEEK_END) != 0) {
        programPrintError("fs_size: fseek to end failed on handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }
    long size = ftell(sfallVfsFiles[id]);
    if (size == -1L) {
        programPrintError("fs_size: ftell (size) failed on handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }
    if (fseek(sfallVfsFiles[id], pos, SEEK_SET) != 0) {
        programPrintError("fs_size: fseek restore failed on handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    // I2-M05: On LP64 platforms long is 64-bit, int is 32-bit.
    // File sizes > ~2.1 GiB would silently truncate.
#if defined(__LP64__)
    if (size > static_cast<long>(INT_MAX)) {
        programPrintError("fs_size: file size %ld exceeds int range", size);
        programStackPushInteger(program, -1);
        return;
    }
#endif
    programStackPushInteger(program, static_cast<int>(size));
}

// fs_pos(id) -> int (-1 on error)
static void op_fs_pos(Program* program)
{
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_pos: invalid VFS handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }

    // I2-M02: Check ftell return value — -1L indistinguishable from
    // valid position without error checking.
    long pos = ftell(sfallVfsFiles[id]);
    if (pos == -1L) {
        programPrintError("fs_pos: ftell failed on handle %d", id);
        programStackPushInteger(program, -1);
        return;
    }
    programStackPushInteger(program, static_cast<int>(pos));
}

// fs_seek(id, pos)
static void op_fs_seek(Program* program)
{
    int pos = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_seek: invalid VFS handle %d", id);
        return;
    }

    // I2-M03: Check fseek return value — script-provided pos could be
    // any int value (negative, beyond file bounds). Non-zero return
    // indicates seek failure.
    if (fseek(sfallVfsFiles[id], pos, SEEK_SET) != 0) {
        programPrintError("fs_seek: fseek to %d failed on handle %d", pos, id);
    }
}

// fs_resize(id, size)
static void op_fs_resize(Program* program)
{
    int size = programStackPopInteger(program);
    int id = programStackPopInteger(program);

    if (id < 0 || id >= kVfsMaxFiles || sfallVfsFiles[id] == nullptr) {
        programPrintError("fs_resize: invalid VFS handle %d", id);
        return;
    }

    // M-12: sfall's FSresize treats size == -1 as "mark this file to be saved
    // into savegames" (FileSystem.cpp:689-693, isSave=1). The fork rejected
    // size <= 0, breaking the documented persistence idiom. Accept -1 as the
    // mark-for-save contract (the disk-backed VFS persists files itself).
    if (size == -1) {
        sfallVfsFileIsSave[id] = true;
        return;
    }

    if (size <= 0) {
        programPrintError("fs_resize: invalid size %d", size);
        return;
    }

    // P-07: sfall FSresize rejects size > MAX_FILE_SIZE (FileSystem.cpp:690).
    if (size > kMaxVfsFileSize) {
        programPrintError("fs_resize: size %d exceeds sfall MAX_FILE_SIZE (0xA00000)", size);
        return;
    }

    // F-028: handles opened via fs_find are read-only ("rb"). Reopen in
    // read-write mode ("r+b") to safely extend the file. fs_create/fs_copy
    // handles ("w+b") are already writable — mode 2 path is a no-op.
    if (sfallVfsFileMode[id] == 1) {
        const char* path = sfallVfsFilePath[id];
        if (path == nullptr) {
            programPrintError("fs_resize: cannot reopen read-only handle %d (no path)", id);
            return;
        }
        fclose(sfallVfsFiles[id]);
        // M3: unbuffered — this handle becomes writable (mode 2) and script
        // writes must be visible to independent engine reads.
        sfallVfsFiles[id] = sfallVfsFopen(path, "r+b");
        if (sfallVfsFiles[id] == nullptr) {
            // Fallback: reopen as read-only so the handle remains usable
            // for reads even though resize cannot proceed.
            sfallVfsFiles[id] = compat_fopen(path, "rb");
            if (sfallVfsFiles[id] == nullptr) {
                // I2-M04: Both reopen attempts failed — the handle slot
                // is now a zombie (sfallVfsFileOpen still true, FILE*
                // is nullptr). Free the handle so the slot can be reused.
                sfallVfsFreeHandle(id);
            }
            programPrintError("fs_resize: cannot reopen '%s' for writing", path);
            return;
        }
        sfallVfsFileMode[id] = 2; // now read-write
        // H-27: DO NOT set sfallVfsFileDeletable[id] here. This reopen turns a
        // read-only fs_find handle into a writable one, but the file is a
        // pre-existing game asset — it must never be deleted at handle-free /
        // close-all. The old code flipped mode 1→2 which sfallVfsFreeHandle
        // treated as deletable (compat_remove), destroying the original file
        // at game reset/exit.
    }

    // F-13 (ITER2-M5): Support file truncation on shrink. The old code used a
    // grow-only pattern (fseek + fputc) which leaves stale data when the new
    // size is smaller than the current file size. If original file is 100 bytes
    // and script calls fs_resize(handle, 50), position 49 gets \0 but bytes
    // 50-99 remain as stale data. Use ftruncate() (POSIX) on shrink.
    //
    // Logic:
    //   1. Save current file position.
    //   2. Determine current file size (seek to end, ftell).
    //   3. If new_size > current_size: grow via existing fseek + fputc pattern.
    //   4. If new_size < current_size: truncate via ftruncate.
    //   5. If new_size == current_size: no-op.
    //   6. Restore position to min(saved_pos, new_size).
    long savedPos = ftell(sfallVfsFiles[id]);
    if (savedPos == -1L) {
        programPrintError("fs_resize: ftell (save pos) failed on handle %d", id);
        return;
    }

    if (fseek(sfallVfsFiles[id], 0, SEEK_END) != 0) {
        programPrintError("fs_resize: fseek to end failed on handle %d", id);
        return;
    }
    long currentSize = ftell(sfallVfsFiles[id]);
    if (currentSize == -1L) {
        programPrintError("fs_resize: ftell (size) failed on handle %d", id);
        return;
    }

    long newSize = static_cast<long>(size);
    if (newSize > currentSize) {
        // Growing: use existing fseek + fputc pattern to extend the file.
        // F-48: Check fseek return value matching the pattern used by
        // fs_size (lines 2517, 2528) and fs_seek (line 2582).
        if (fseek(sfallVfsFiles[id], newSize - 1, SEEK_SET) != 0) {
            programPrintError("fs_resize: fseek failed on handle %d", id);
            return;
        }
        // F2-064: check fputc return value.
        if (fputc(0, sfallVfsFiles[id]) == EOF) {
            programPrintError("fs_resize: fputc failed on handle %d", id);
        }
    } else if (newSize < currentSize) {
        // Shrinking: truncate the file to the requested size.
        if (ftruncate(fileno(sfallVfsFiles[id]), static_cast<off_t>(newSize)) != 0) {
            programPrintError("fs_resize: ftruncate to %ld failed on handle %d", newSize, id);
        }
    }
    // else: newSize == currentSize, no-op.

    // Restore position to min(original position, new size) so the file
    // pointer is valid after resize. If the file was shrunk below the
    // original position, seek to the new end of file.
    long restorePos = (savedPos < newSize) ? savedPos : newSize;
    if (fseek(sfallVfsFiles[id], restorePos, SEEK_SET) != 0) {
        programPrintError("fs_resize: fseek restore failed on handle %d", id);
    }
}

// ============================================================
// NPC/Hero opcodes
// ============================================================

// eax_available() — 0x81A3 (deprecated in sfall)
// Returns whether EAX environmental audio is available. The CE engine has no
// EAX support (no DirectSound EAX hooks), so this always returns 0. Some mod
// scripts (et tu's GLZFTERM.int, gl_pipboychk.int) still call it; leaving the
// opcode undefined aborts the calling script with "Undefined opcode 81a3".
static void op_eax_available(Program* program)
{
    programStackPushInteger(program, 0);
}

// set_eax_environment(environment) — 0x81A4 (deprecated in sfall)
// No-op: CE has no EAX audio environment to configure.
static void op_set_eax_environment(Program* program)
{
    programStackPopInteger(program);
}

// inc_npc_level(pid or string name) — 0x81A5
// Increments the level of the party member NPC matching the given PID.
// When a specific PID is provided, only that party member is targeted;
// otherwise all eligible party members are leveled up.
// Calls _partyMemberIncLevels() which handles the full party member
// level-up logic: level_up_every checks, probability rolls, stat
// adjustments, and display messages.
static void op_inc_npc_level(Program* program)
{
    ProgramValue arg = programStackPopValue(program);
    int pid = -1;
    if (arg.isInt()) {
        pid = arg.asInt();
    } else if (arg.isString()) {
        // String input (NPC name): resolve to PID by iterating known party
        // member PIDs and matching the proto display name against the input.
        const char* name = arg.asString(program);

        for (int i = 0; i < gPartyMemberDescriptionsLength; i++) {
            int candidatePid = gPartyMemberPids[i];
            const char* protoName = protoGetName(candidatePid);
            if (protoName != nullptr && strcmp(name, protoName) == 0) {
                pid = candidatePid;
                break;
            }
        }

        if (pid <= 0) {
            debugPrint("inc_npc_level: could not find party member proto matching name '%s'", name);
            return;
        }
    } else {
        debugPrint("inc_npc_level: argument must be an int (PID) or string (name)");
        return;
    }

    _partyMemberIncLevels(pid);
}

// get_npc_level(pid or string name) -> int — 0x8241
// Returns the current level of the specified party member NPC.
// Party member level data is stored in _partyMemberLevelUpInfoList
// (static in party_member.cc). Until a public accessor is added to
// party_member.h, level retrieval is limited to verifying the NPC
// exists as a party member.
static void op_get_npc_level(Program* program)
{
    ProgramValue arg = programStackPopValue(program);

    int level = 0;

    if (arg.isInt()) {
        int pid = arg.asInt();
        level = partyMemberGetLevel(pid);
    } else if (arg.isString()) {
        // String input (NPC name): resolve to PID by iterating known party
        // member PIDs and matching the proto display name against the input.
        const char* name = arg.asString(program);
        int pid = -1;

        for (int i = 0; i < gPartyMemberDescriptionsLength; i++) {
            int candidatePid = gPartyMemberPids[i];
            const char* protoName = protoGetName(candidatePid);
            if (protoName != nullptr && strcmp(name, protoName) == 0) {
                pid = candidatePid;
                break;
            }
        }

        if (pid > 0) {
            level = partyMemberGetLevel(pid);
        } else {
            debugPrint("get_npc_level: could not find party member proto matching name '%s'", name);
        }
    } else {
        debugPrint("get_npc_level: argument must be an int (PID) or string (name)");
    }

    programStackPushInteger(program, level);
}

// set_dm_model(string model_name)
// Sets the default male hero model (e.g. "hmjmps"). The model name is resolved
// via artListIndex and applied in op_refresh_pc_art by overriding proto->fid.
// Full Hero Appearance integration (armor-aware model switching, death animation
// hooking) requires changes to _proto_dude_update_gender() in proto.cc.
static void op_set_dm_model(Program* program)
{
    const char* modelName = programStackPopString(program);
    if (modelName == nullptr || modelName[0] == '\0') {
        gCustomMaleHeroModelNum = 0;
        return;
    }

    int modelNum = artListIndex(OBJ_TYPE_CRITTER, modelName);
    if (modelNum >= 0) {
        gCustomMaleHeroModelNum = modelNum;
    } else {
        programPrintError("set_dm_model: model '%s' not found in art list", modelName);
    }
}

// set_df_model(string model_name)
// Sets the default female hero model (e.g. "hfjmps").
static void op_set_df_model(Program* program)
{
    const char* modelName = programStackPopString(program);
    if (modelName == nullptr || modelName[0] == '\0') {
        gCustomFemaleHeroModelNum = 0;
        return;
    }

    int modelNum = artListIndex(OBJ_TYPE_CRITTER, modelName);
    if (modelNum >= 0) {
        gCustomFemaleHeroModelNum = modelNum;
    } else {
        programPrintError("set_df_model: model '%s' not found in art list", modelName);
    }
}

// div (/)
static void op_div(Program* program)
{
    ProgramValue divisorValue = programStackPopValue(program);
    ProgramValue dividendValue = programStackPopValue(program);

    // F-633: Reject non-numeric operands (string, pointer, dynamic string).
    // Vanilla opDivide calls programFatalError for non-numeric types; CE's
    // op_div must do the same to prevent silent nonsense results when scripts
    // accidentally push strings onto the stack before calling div.
    if (!dividendValue.isInt() && !dividendValue.isFloat()) {
        programFatalError("%s: div: dividend operand must be numeric, got %s",
            program->name, dividendValue.typeDebugString());
        return;
    }
    if (!divisorValue.isInt() && !divisorValue.isFloat()) {
        programFatalError("%s: div: divisor operand must be numeric, got %s",
            program->name, divisorValue.typeDebugString());
        return;
    }

    // Zero-division check: for float values, compare as float to catch -0.0f
    // (IEEE 754 bit pattern 0x80000000 ≠ 0 integer, bypassing the integer check).
    // For integer values, compare the raw integerValue.
    if ((divisorValue.isFloat() && divisorValue.asFloat() == 0.0f)
        || divisorValue.integerValue == 0) {
        programPrintError("Division by zero");

        // Execution is not halted, matching sfall behavior — push 0 and continue.
        programStackPushInteger(program, 0);
        return;
    }

    if (dividendValue.isFloat() || divisorValue.isFloat()) {
        programStackPushFloat(program, dividendValue.asFloat() / divisorValue.asFloat());
    } else {
        // Guard against INT_MIN / -1 which causes signed overflow UB (SIGFPE on x86).
        if (divisorValue.integerValue == -1 && dividendValue.integerValue == INT_MIN) {
            debugPrint("Division overflow: INT_MIN / -1");
            programStackPushInteger(program, 0);
            return;
        }
        // Signed division — matches sfall and universal scripting convention.
        programStackPushInteger(program, dividendValue.integerValue / divisorValue.integerValue);
    }
}

static void op_sprintf(Program* program)
{
    static constexpr MetaruleInfo kSprintfStringFormatScaffold = {
        "string_format",
        mf_string_format,
        2,
        2,
        0,
        { ARG_STRING, ARG_ANY },
    };

    ProgramValue args[2];
    args[0] = programStackPopValue(program);
    args[1] = programStackPopValue(program);

    OpcodeContext ctx(program, &kSprintfStringFormatScaffold, 2, args);
    if (!ctx.validateArguments()) {
        ctx.setReturn("");
        ctx.pushReturnValue();
        return;
    }

    mf_string_format(ctx);
    ctx.pushReturnValue();
}

// string_null_or_empty(str) -> int
// Returns 1 if the string is null or empty (zero length), 0 otherwise.
// F-007 (M-14): Implements the missing string_null_or_empty function
// used by newer sfall scripts (observed in Et Tu script sources).
static void op_string_null_or_empty(Program* program)
{
    const char* str = programStackPopString(program);
    if (str == nullptr || str[0] == '\0') {
        programStackPushInteger(program, 1);
    } else {
        programStackPushInteger(program, 0);
    }
}

static void op_charcode(Program* program)
{
    const char* str = programStackPopString(program);
    if (str != nullptr && str[0] != '\0') {
        programStackPushInteger(program, static_cast<int>(str[0]));
    } else {
        programStackPushInteger(program, 0);
    }
}

static void op_show_iface_tag(Program* program)
{
    int tag = programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeEnableState(static_cast<DudeState>(tag));
        break;
    case 1: // POISONED — engine-driven indicator, cannot be force-shown via sfall API.
            // The indicator appears automatically when critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD.
        debugPrint("show_iface_tag(POISONED): poison indicator is engine-driven and cannot be force-shown via sfall script.");
        break;
    case 2: // RADIATED — engine-driven indicator, cannot be force-shown via sfall API.
            // The indicator appears automatically when critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD.
        debugPrint("show_iface_tag(RADIATED): radiation indicator is engine-driven and cannot be force-shown via sfall script.");
        break;
    default:
        interfaceTagShow(tag);
    }

    interfaceTagShow(tag);
}

static void op_hide_iface_tag(Program* program)
{
    int tag = programStackPopInteger(program);

    switch (tag) {
    case DudeState::DUDE_STATE_SNEAKING:
    case DudeState::DUDE_STATE_LEVEL_UP_AVAILABLE:
    case DudeState::DUDE_STATE_ADDICTED:
        dudeDisableState(static_cast<DudeState>(tag));
        break;
    case 1: // POISONED — engine-driven indicator, cannot be force-hidden via sfall API.
            // The indicator disappears automatically when poison level falls below the threshold.
        debugPrint("hide_iface_tag(POISONED): poison indicator is engine-driven and cannot be force-hidden via sfall script.");
        break;
    case 2: // RADIATED — engine-driven indicator, cannot be force-hidden via sfall API.
            // The indicator disappears automatically when radiation level falls below the threshold.
        debugPrint("hide_iface_tag(RADIATED): radiation indicator is engine-driven and cannot be force-hidden via sfall script.");
        break;
    default:
        interfaceTagHide(tag);
    }

    interfaceTagHide(tag);
}

static void op_is_iface_tag_active(Program* program)
{
    int tag = programStackPopInteger(program);
    bool isActive = false;

    if (dudeStateIsValid(tag)) {
        isActive = dudeHasState(static_cast<DudeState>(tag));
    } else {
        isActive = interfaceTagIsActive(tag);
    }

    programStackPushInteger(program, isActive ? 1 : 0);
}

// TODO: move opcodes into several files
// TODO: reduce code duplication by introducing something like OpcodeContext in sfall

// Returns true if the given hook type has a fire site (dispatch point) in the
// engine code. Several hooks exist in the enum for sfall compatibility but
// have no ScriptHookCall::call() invocation anywhere — registering them
// silently succeeds but the handler never fires.
// See sfall_script_hooks.h for the full enum and SFALL_COMPATIBILITY.md
// for per-hook status.
static bool sfallHookHasFireSite(HookType hookId)
{
    switch (hookId) {
    // Values below match the HOOK_* enum in sfall_script_hooks.h.
    // These hooks are commented out / deliberately absent — their enum
    // entries are not compiled, so we use numeric literals.
    case static_cast<HookType>(3): // HOOK_DEATHANIM1: use DEATHANIM2 instead
    case static_cast<HookType>(9): // HOOK_REMOVEINVENOBJ: requires RMOBJ_* infrastructure
    case static_cast<HookType>(12): // HOOK_HEXMOVEBLOCKING: obsolete
    case static_cast<HookType>(13): // HOOK_HEXAIBLOCKING: obsolete
    case static_cast<HookType>(14): // HOOK_HEXSHOOTBLOCKING: obsolete
    case static_cast<HookType>(15): // HOOK_HEXSIGHTBLOCKING: obsolete
    case static_cast<HookType>(37): // HOOK_SUBCOMBATDAMAGE: per-hit not supported
    case static_cast<HookType>(44): // HOOK_ADJUSTPOISON: requires engine refactor
    // HOOK_ADJUSTRADS(45) removed from the exclusion list (M-26/F-03): it now
    // has a real fire site — critterAdjustRadiation() calls
    // scriptHooks_AdjustRads (critter.cc:492, sfall_script_hooks.cc:718-732).
    case static_cast<HookType>(46): // HOOK_ROLLCHECK: 30+ call sites, lacks context
    case static_cast<HookType>(47): // HOOK_BESTWEAPON: 10+ return points, lifetime issues
    // F-22 (FIXED): Reserved hooks 54-60 have no fire sites (sfall_script_hooks.h:247-249).
    // Add them to the exclusion list so registration produces a "has no fire site"
    // debugPrint instead of silently accepting handlers that will never be called.
    case static_cast<HookType>(54):
    case static_cast<HookType>(55):
    case static_cast<HookType>(56):
    case static_cast<HookType>(57):
    case static_cast<HookType>(58):
    case static_cast<HookType>(59):
    case static_cast<HookType>(60):
    case static_cast<HookType>(61): // HOOK_BUILDSFXWEAPON: static buffer, lifetime issues
        return false;
    default:
        return true;
    }
}

static void op_register_hook(Program* program)
{
    constexpr char opcodeName[] = "register_hook";

    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    if (!sfallHookHasFireSite(static_cast<HookType>(hookId))) {
        debugPrint("%s: hook %d has no fire site — handler will never be called\n", opcodeName, hookId);
    }
    // NOTE (F-106): register_hook (0x8207) only registers the "start"
    // procedure. In original sfall this opcode accepts an optional procedure
    // parameter; CE requires the explicit register_hook_proc variant
    // (0x8262) for non-start procedures. If a script has no "start" procedure,
    // this opcode will fail with an error on purpose — scripts without a
    // start procedure cannot serve as hook handlers because the interpreter
    // dispatches hooks through the start procedure's entry point.
    int startProcIndex = programFindProcedure(program, gScriptProcNames[SCRIPT_PROC_START]);
    if (startProcIndex == -1) {
        programPrintError("%s: 'start' procedure not found", opcodeName);
        return;
    }
    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), startProcIndex)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, startProcIndex);
    }
}

static void op_register_hook_proc(Program* program)
{
    constexpr char opcodeName[] = "register_hook_proc";

    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    if (!sfallHookHasFireSite(static_cast<HookType>(hookId))) {
        debugPrint("%s: hook %d has no fire site — handler will never be called\n", opcodeName, hookId);
    }
    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("%s: procedure index %d is out of range [0; %d]", opcodeName, procedureIndex, program->procedureCount());
        return;
    }
    // CE uses reverse iteration (LIFO) for hook dispatch: ScriptHookCall::call()
    // iterates hooks in reverse order (highest index → lowest index, i.e. last
    // registered = first executed). register_hook_proc calls push_back() which
    // adds to the end (highest index) so the most recently registered script
    // runs first. register_hook_proc_spec calls emplace(begin()) which adds to
    // the beginning (index 0) so it runs last — effectively an override hook.
    //
    // The conscious design rationale: the last handler to register wins (its
    // return values are what the caller sees), matching sfall's general
    // convention that specs registered via register_hook_proc_spec serve as
    // final overrides. See scriptHooksRegister() in sfall_script_hooks.cc and
    // SFALL_COMPATIBILITY.md for details on ordering semantics.
    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), procedureIndex)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, procedureIndex);
    }
}

// register_hook_proc_spec
// In sfall: inserts at end of hook order (spec = special/append) so the
// hook runs LAST. This is used by RPU/ETu for final-override hooks like
// armor-based weapon restrictions (HOOK_CANUSEWEAPON in gl_partyarmor.ssl).
//
// Hook iteration in ScriptHookCall::call() is reverse (size-1 down to 0),
// so inserting at the end (highest index) means this hook executes last.
// Default register_hook_proc inserts at beginning (index 0 = first executed).
static void op_register_hook_proc_spec(Program* program)
{
    constexpr char opcodeName[] = "register_hook_proc_spec";

    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    if (hookId < 0 || hookId >= HOOK_COUNT) {
        programPrintError("%s: invalid hook ID: %d", opcodeName, hookId);
        return;
    }
    if (!sfallHookHasFireSite(static_cast<HookType>(hookId))) {
        debugPrint("%s: hook %d has no fire site — handler will never be called\n", opcodeName, hookId);
    }
    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("%s: procedure index %d is out of range [0; %d]", opcodeName, procedureIndex, program->procedureCount());
        return;
    }

    if (!scriptHooksRegister(program, static_cast<HookType>(hookId), procedureIndex, true)) {
        programPrintError("%s(%d, %d): failed", opcodeName, hookId, procedureIndex);
    }
}

// ============================================================
// reg_anim_callback: called when registered animations complete.
// In sfall 4.x, this registers a procedure to be invoked upon
// animation completion. The callback receives the animated object
// and a pre-set argument.
// Currently stores the callback for future integration with the
// animation system. The opcode is registered so RPU/ET Tu scripts
// do not crash.
// ============================================================

Program* sfallAnimCallbackProgram = nullptr;
int sfallAnimCallbackProcedureIndex = -1;

// F-084 NOTE: sfallAnimCallbackProgram is set by reg_anim_callback and
// used by sfallAnimCallbackInvoke (which snap-and-clears before invoke).
// However, programFree() in interpreter.cc does NOT clear this pointer.
// When a program is freed after reg_anim_callback registered it, this
// global becomes a dangling pointer. The snap-and-clear pattern in
// sfallAnimCallbackInvoke handles the invocation-time UAF but doesn't
// prevent post-free reads in other code paths. The permanent fix requires
// clearing sfallAnimCallbackProgram inside programFree() (interpreter.cc
// line ~475), which is outside this translation unit.
// sfallAnimCallbackReset() provides a manual cleanup path called from
// sfallOpcodesExit() and sfallScriptHooksReset().

// M-128: sfall binds reg_anim_callback as an ANIM_KIND_CALLBACK step in the
// CURRENT animation sequence (Anims.cpp:127-135 → register_object_call →
// animationRegisterCallback). The fork's single-slot global fired on ANY
// pure-animation completion with the completing object — the registering
// sequence's step/owner was never honored. Bind through the engine's
// per-sequence ANIM_KIND_CALLBACK mechanism instead: the callback fires when
// that sequence reaches its step, matching sfall.
//
// The engine's AnimationCallback signature is int(void*, void*). We pass the
// program as param1 and the procedure index as param2; the trampoline
// executes the script procedure (the same execution the old invoke path used,
// minus the object-argument push — sfall's ExecuteCallback also executes the
// procedure without an explicit object arg; the sequence step carries the
// animation context).
static int sfallAnimCallbackTrampoline(void* param1, void* param2)
{
    Program* program = static_cast<Program*>(param1);
    intptr_t procIndex = reinterpret_cast<intptr_t>(param2);

    if (program == nullptr || procIndex < 0 || procIndex >= program->procedureCount()) {
        return 0;
    }
    if (program->exited || program->data == nullptr) {
        return 0;
    }

    programExecuteProcedure(program, static_cast<int>(procIndex));
    return 0;
}

static void op_reg_anim_callback(Program* program)
{
    int procedureIndex = programStackPopInteger(program);

    if (procedureIndex < 0 || procedureIndex >= program->procedureCount()) {
        programPrintError("reg_anim_callback: procedure index %d is out of range [0; %d]",
            procedureIndex, program->procedureCount());
        return;
    }

    // M-128: bind as an ANIM_KIND_CALLBACK step in the current animation
    // sequence. animationRegisterCallback returns -1 when no sequence is
    // being registered (e.g. outside reg_anim_begin/reg_anim_end) — the
    // script's call is then rejected with -1, matching sfall's "executes it
    // in the registered sequence" contract.
    if (animationRegisterCallback(program,
            reinterpret_cast<void*>(static_cast<intptr_t>(procedureIndex)),
            sfallAnimCallbackTrampoline, -1) != 0) {
        programPrintError("reg_anim_callback: no active animation sequence to bind the callback");
        programStackPushInteger(program, -1);
        return;
    }

    // Do NOT set the legacy single-slot globals (sfallAnimCallbackProgram /
    // sfallAnimCallbackProcedureIndex). animation.cc's pure-animation
    // completion paths still call sfallAnimCallbackInvoke(object) — if the
    // globals were set, that path would ALSO fire the script proc on
    // arbitrary completions with the wrong object (the exact M-128 defect).
    // The ANIM_KIND_CALLBACK step bound above is the authoritative firing
    // point, so the globals remain null and the legacy invoke path is inert.
}

void sfallAnimCallbackReset()
{
    sfallAnimCallbackProgram = nullptr;
    sfallAnimCallbackProcedureIndex = -1;
}

ScriptHookCall* hookOpcodeGetCurrentCall(const char* opcodeName)
{
    const auto hookCall = ScriptHookCall::current();
    if (hookCall == nullptr) {
        programPrintError("%s: called outside of a script hook", opcodeName);
    }
    return hookCall;
}

static void op_get_sfall_arg(Program* program)
{
    constexpr char opcodeName[] = "get_sfall_arg";

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    programStackPushValue(program, hookCall != nullptr ? hookCall->getNextArgFromScript() : ProgramValue(0));
}

static void op_get_sfall_args(Program* program)
{
    constexpr char opcodeName[] = "get_sfall_args";

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    ArrayId result = 0;
    if (hookCall != nullptr) {
        result = CreateTempArray(hookCall->numArgs(), 0);
        for (int i = 0; i < hookCall->numArgs(); ++i) {
            SetArray(result, i, hookCall->getArgAt(i), false, program);
        }
    }
    programStackPushInteger(program, static_cast<int>(result));
}

static void op_set_sfall_arg(Program* program)
{
    constexpr char opcodeName[] = "set_sfall_arg";

    const ProgramValue value = programStackPopValue(program);
    const int argNum = programStackPopInteger(program);

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    if (hookCall == nullptr) return;

    if (argNum < 0 || argNum >= hookCall->numArgs()) {
        programPrintError("%s: argNum %d out of range [0, %d]", opcodeName, argNum, hookCall->numArgs() - 1);
        return;
    }
    hookCall->setArgAt(argNum, value);
}

static void op_set_sfall_return(Program* program)
{
    constexpr char opcodeName[] = "set_sfall_return";

    const ProgramValue value = programStackPopValue(program);

    const auto hookCall = hookOpcodeGetCurrentCall(opcodeName);
    if (hookCall == nullptr) return;

    if (hookCall->numScriptReturnValues() >= hookCall->maxReturnValues()) {
        programPrintError("%s: trying to add next return value while only %d is expected", opcodeName, hookCall->maxReturnValues());
        return;
    }
    hookCall->addReturnValueFromScript(value);
}

// ============================================================
// Skill point opcodes
// ============================================================

// set_critter_skill_points(object critter, int skill, int value)
// Sets the base skill points (proto-level) for a critter.
// Arguments are popped in reverse order: value, skill, critter.
static void op_set_critter_skill_points(Program* program)
{
    int value = programStackPopInteger(program);
    int skill = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_skill_points: expected critter object");
        return;
    }

    if (!skillIsValid(skill)) {
        programPrintError("set_critter_skill_points: invalid skill %d (valid range 0-%d)",
            skill, SKILL_COUNT - 1);
        return;
    }

    Proto* proto;
    if (protoGetProto(critter->pid, &proto) == -1) {
        programPrintError("set_critter_skill_points: cannot get proto for pid %d", critter->pid);
        return;
    }

    proto->critter.data.skills[skill] = value;

    // F2-043: Mark proto dirty so skill mutations survive LRU eviction.
    // This opcode accepts any critter (no gDude guard), following the
    // pattern used by op_set_proto_data at sfall_opcodes.cc:861-862.
    protoMarkDirty(critter->pid);
}

// get_critter_skill_points(object critter, int skill) -> int
// Returns the base skill points (proto-level) for a critter.
static void op_get_critter_skill_points(Program* program)
{
    int skill = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr || objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("get_critter_skill_points: expected critter object");
        programStackPushInteger(program, 0);
        return;
    }

    if (!skillIsValid(skill)) {
        programPrintError("get_critter_skill_points: invalid skill %d (valid range 0-%d)",
            skill, SKILL_COUNT - 1);
        programStackPushInteger(program, 0);
        return;
    }

    programStackPushInteger(program, skillGetBaseValue(critter, skill));
}

// set_available_skill_points(int value)
// Sets the player's unspent skill points.
static void op_set_available_skill_points(Program* program)
{
    int value = programStackPopInteger(program);
    if (value < 0) {
        value = 0;
    }
    pcSetStat(PC_STAT_UNSPENT_SKILL_POINTS, value);
}

// get_available_skill_points() -> int
// Returns the player's current unspent skill points.
static void op_get_available_skill_points(Program* program)
{
    programStackPushInteger(program, pcGetStat(PC_STAT_UNSPENT_SKILL_POINTS));
}

// mod_skill_points_per_level(int value)
// Sets a modifier for skill points gained per level. The modifier is stored
// and consumed by characterEditorUpdateLevel() in character_editor.cc.
// M-172: sfall op_mod_skill_points_per_level clamps to [-100, 100]
// (Stats.cpp:207-225) then adds the FO default 5. The fork rejected all
// negatives (clamped to 0), so a script using the documented negative range
// (e.g. mod_skill_points_per_level(-50)) silently no-oped. The negative range
// is consumed correctly at character_editor.cc:5941 (sp += gSkillPointsPerLevelMod).
int gSkillPointsPerLevelMod = 0;
static constexpr int kMaxSkillPointsPerLevelMod = 100;

static void op_mod_skill_points_per_level(Program* program)
{
    int val = programStackPopInteger(program);
    if (val < -100) {
        programPrintError("mod_skill_points_per_level: value %d clamped to range [-100, 100]", val);
        val = -100;
    }
    if (val > 100) {
        programPrintError("mod_skill_points_per_level: value %d clamped to range [-100, 100]", val);
        val = 100;
    }
    gSkillPointsPerLevelMod = val;
    sfall_gl_vars_store("SFSkillP", gSkillPointsPerLevelMod);
}

// ============================================================
// VOODOO memory write opcodes — safe no-ops in CE engine.
//
// sfall allowed scripts to read/write arbitrary memory addresses
// in the Fallout2.exe process space. CE's address space is completely
// different, so direct memory access is impossible. These opcodes are
// registered as safe no-ops that pop their arguments and log a debug
// message, preventing script crashes on unregistered opcode errors.
//
// UF-001: Many FO1 behaviors that mods achieve via write_* opcodes have
// native CE implementations gated behind gFallout1Behavior:
//
//   gFallout1Behavior flag controls (set via [Misc] Fallout1Behavior=1):
//     - 3-trait limit at character creation (trait.cc:343)
//     - Level cap of 21 instead of 99 (stat.cc:742)
//     - Instant full party heal during rest (party_member.cc:862)
//     - Alternative encounter computation (worldmap.cc:3936,4160)
//     - Reaction thresholds (reaction.cc:24)
//     - Water timer (sfall_metarules.cc:935)
//     - Combat AI weapon preference (combat_ai.cc:1149,2196,2640)
//     - Endgame slide logic (endgame.cc:277)
//     - Book repair logic (item.cc:818)
//     - Game-mode flag fallback (game.cc:173,313)
//     - Pipboy rest duration (pipboy.cc:2098)
//
// Scripts using write_byte at FO1 memory addresses (e.g., 0x451A6F for
// hit chance formula) should migrate to gFallout1Behavior or CE-native
// metarule alternatives where available.  See sfall_metarules.h for the
// full metarule catalog (set_rest_mode, set_spray_settings, etc.).
//
// VOODOO write opcodes are always registered as safe no-ops.
// AllowUnsafeScripting (ddraw.ini [Debugging] section) is parsed but
// intentionally unwired — these opcodes cannot perform actual memory
// writes in CE regardless of the setting.
// ============================================================

static void op_write_byte(Program* program)
{
    int value = programStackPopInteger(program);
    int addr = programStackPopInteger(program);

#ifndef NDEBUG
    debugPrint("VOODOO write_byte(0x%08X, %d) — NOT SUPPORTED in CE engine. "
               "See gFallout1Behavior flag and CE-native metarule alternatives.\n", addr, value);
#endif
    programPrintError("VOODOO write_byte(0x%08X, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, value);
}

static void op_write_short(Program* program)
{
    int value = programStackPopInteger(program);
    int addr = programStackPopInteger(program);

#ifndef NDEBUG
    debugPrint("VOODOO write_short(0x%08X, %d) — NOT SUPPORTED in CE engine. "
               "See gFallout1Behavior flag and CE-native metarule alternatives.\n", addr, value);
#endif
    programPrintError("VOODOO write_short(0x%08X, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, value);
}

static void op_write_int(Program* program)
{
    int value = programStackPopInteger(program);
    int addr = programStackPopInteger(program);

#ifndef NDEBUG
    debugPrint("VOODOO write_int(0x%08X, %d) — NOT SUPPORTED in CE engine. "
               "See gFallout1Behavior flag and CE-native metarule alternatives.\n", addr, value);
#endif
    programPrintError("VOODOO write_int(0x%08X, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, value);
}

static void op_write_string(Program* program)
{
    const char* value = programStackPopString(program);
    int addr = programStackPopInteger(program);

#ifndef NDEBUG
    debugPrint("VOODOO write_string(0x%08X, \"%s\") — NOT SUPPORTED in CE engine. "
               "See gFallout1Behavior flag and CE-native metarule alternatives.\n",
               addr, value != nullptr ? value : "(null)");
#endif
    programPrintError("VOODOO write_string(0x%08X, \"%s\") — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, value != nullptr ? value : "(null)");
}

// ============================================================
// VOODOO call_offset opcodes — deferred implementation (permanent stubs).
//
// call_offset_* opcodes are designed to call functions at arbitrary
// addresses in the original Fallout2.exe process space. CE's address
// space is completely different, making this fundamentally impossible.
// TODO: These will remain permanent no-op stubs. Scripts should use
// CE-native metarule or opcode alternatives instead. Consider making
// opcode_exists() return 0 for these so scripts can detect the absence
// of call_offset at runtime — currently it returns 1 (misleading).
// ============================================================
// F2-009: call_offset_v* void stubs are true void functions — the Fallout 2 VM
// expects void opcodes to leave the stack unchanged after popping their arguments.
// Pushing an extraneous int 0 shifts all subsequent stack operands by one slot.
// Fixed by removing programStackPushInteger(program, 0) from all five stubs.
static void op_call_offset_v0(Program* program)
{
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_v0(0x%08X) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr);
}

static void op_call_offset_v1(Program* program)
{
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_v1(0x%08X, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1);
}

static void op_call_offset_v2(Program* program)
{
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_v2(0x%08X, %d, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1, arg2);
}

static void op_call_offset_v3(Program* program)
{
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_v3(0x%08X, %d, %d, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1, arg2, arg3);
}

static void op_call_offset_v4(Program* program)
{
    int arg4 = programStackPopInteger(program);
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_v4(0x%08X, %d, %d, %d, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1, arg2, arg3, arg4);
}

// call_offset_r0-r4: call functions returning int with 0-4 args.
// Same as v0-v4 but with return values. Registered as no-ops pushing 0.
static void op_call_offset_r0(Program* program)
{
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_r0(0x%08X) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r1(Program* program)
{
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_r1(0x%08X, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r2(Program* program)
{
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_r2(0x%08X, %d, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1, arg2);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r3(Program* program)
{
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_r3(0x%08X, %d, %d, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1, arg2, arg3);
    programStackPushInteger(program, 0);
}

static void op_call_offset_r4(Program* program)
{
    int arg4 = programStackPopInteger(program);
    int arg3 = programStackPopInteger(program);
    int arg2 = programStackPopInteger(program);
    int arg1 = programStackPopInteger(program);
    int addr = programStackPopInteger(program);
    programPrintError("VOODOO call_offset_r4(0x%08X, %d, %d, %d, %d) — NOT SUPPORTED in CE engine (different address space). "
                       "Use CE-native opcodes or metarules instead.\n", addr, arg1, arg2, arg3, arg4);
    programStackPushInteger(program, 0);
}

// ============================================================
// Perk frequency override (set_perk_freq, 0x8247).
//
// Sets the number of levels between perk selections.
// Default: 0 (use engine default of 3 levels, or 4 with Skilled trait).
// When overridden to a positive value, the override is used directly.
// Integration: character_editor.cc:5745 must read this value in
// characterEditorUpdateLevel() — see sfall_opcodes.h for the extern.
// ============================================================
int gPerkFrequencyOverride = 0;

static void op_set_perk_freq(Program* program)
{
    int value = programStackPopInteger(program);
    if (value < 0) {
        programPrintError("set_perk_freq: value %d clamped to range [0, 50]", value);
        value = 0;
    }
    // Guard: unreasonably large perk frequency would block all future perk
    // gains. 50 levels is the practical upper bound (max 99 player level / 2).
    if (value > 50) {
        programPrintError("set_perk_freq: value %d clamped to range [0, 50]", value);
        value = 50;
    }
    gPerkFrequencyOverride = value;
    sfall_gl_vars_store("SFPerkFr", value);
}

// set_perk_level(int perkID, int value) — 0x817A
// Sets the minimum level requirement for a specific perk.
// value of 0 effectively disables the level gate (any level qualifies).
// ETu uses value 999 for Magnetic Personality to effectively disable it.
static void op_set_perk_level(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);

    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_level: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }

    perkSetMinLevel(perkID, value);
}

// set_skill_max(int value) — 0x81A2
// Sets the maximum skill value cap. Default is 300 (vanilla match).
// The cap must be enforced in skill increment paths (skillAddForce,
// character editor skill point allocation) — those integration points
// need to be updated separately to read this value.
int gSkillMaxCap = 300;

// F-040: Store original stat min/max compile-time defaults captured at
// sfallOpcodesInit() time, before any script can modify them. Restored
// in sfallOpcodesReset() to ensure stat bounds don't persist across
// gameReset. Follows the same pattern as gSkillMaxCap.
static int sfallOriginalStatMins[STAT_COUNT];
static int sfallOriginalStatMaxs[STAT_COUNT];
static bool sfallStatBoundsCaptured = false;

static void op_set_skill_max(Program* program)
{
    int value = programStackPopInteger(program);
    if (value < 0) {
        value = 300;
    }
    // M-171: sfall op_set_skill_max clamps to [0, 300] (Stats.cpp:411-425:
    // `mov ecx, 300; cmp eax, ecx; cmova eax, ecx`). The fork's 999 cap
    // allowed set_skill_max(400) to raise the skill cap above sfall's
    // documented maximum, giving mods different skill economies.
    if (value > 300) {
        value = 300;
    }
    gSkillMaxCap = value;
    sfall_gl_vars_store("SFSkillM", value);
}

// ============================================================
// Stat max/min opcodes.
// Modify stat description limits in gStatDescriptions[] via accessors
// in stat.cc. critterGetStat() and related functions automatically
// respect the modified limits.
//
// ARCHITECTURE NOTE (I2-M03): CE uses a SINGLE global gStatDescriptions[]
// table shared across all entities (PC, NPCs, critters). The engine does
// NOT provide per-entity stat limit storage.
//
// M-21: sfall maintains SEPARATE PC and NPC cap tables (Stats.cpp:34-36
// statMaximumsPC/statMaximumsNPC, both initialized from stat_data[i].maxValue)
// and op_set_stat_max calls BOTH SetPCStatMax and SetNPCStatMax, while
// set_pc_stat_max / set_npc_stat_max write only their own table. To make
// get_stat_max/get_stat_min honor the isNpc argument (sfall Stats.cpp:387-396),
// we maintain shadow PC/NPC cap tables here, updated alongside the shared
// table by the six setters, and exposed to sfall_metarules.cc.
// ================================================================

// M-21: shadow PC/NPC stat cap tables (initialized from gStatDescriptions).
static int sfallPcStatMaxs[STAT_COUNT];
static int sfallNpcStatMaxs[STAT_COUNT];
static int sfallPcStatMins[STAT_COUNT];
static int sfallNpcStatMins[STAT_COUNT];

// F-044/F-045: Clamp stat boundary values (min/max) to per-category
// reasonable ranges. Prevents corrupted stat bounds from pathological
// inputs (e.g., AGE=999999 or STRENGTH=-9999).
//
// Stat category bounds:
//   SPECIAL stats      (0-6):  [0, 100]     — primary attributes
//   Derived stats      (7-32): [0, 9999]    — HP, AP, AC, DR, etc.
//   AGE                (33):   [0, 9999]    — age can legitimately be high
//   GENDER             (34):   [0, 1]       — binary
//   Current HP/Poison/Rad (35-37): [0, 99999] — runtime pseudostats
//
// Logs a warning when clamping occurs so script authors can detect issues.
static int clampStatBoundaryValue(int stat, int value, const char* opcodeName)
{
    // Absolute floor: values below 0 are nonsensical for most stats.
    // GENDER (34) is already clamped to [0,1] below.
    int floorValue = 0;
    int ceilingValue = 9999;

    if (stat < 7) {
        // SPECIAL stats (STRENGTH through LUCK): primary attributes with
        // a typical engine range of 1-10. Allow generous bounds [0, 100]
        // for mod flexibility while preventing overflow/corruption.
        ceilingValue = 100;
    } else if (stat == STAT_GENDER) {
        // Gender is strictly binary (0=male, 1=female).
        floorValue = 0;
        ceilingValue = 1;
    } else if (stat == STAT_AGE) {
        // Age can legitimately be many years; cap at 9999 for sanity.
        ceilingValue = 9999;
    } else if (stat >= STAT_CURRENT_HIT_POINTS) {
        // Current-value pseudostats (HP, poison, radiation): allow larger
        // values since some mods inflate HP dramatically.
        ceilingValue = 99999;
    } else {
        // Derived stats (HP max through POISON_RESISTANCE): clamp to
        // reasonable game-playable range.
        ceilingValue = 9999;
    }

    if (value < floorValue) {
        debugPrint("%s: stat %d value %d clamped to floor %d", opcodeName, stat, value, floorValue);
        value = floorValue;
    }
    if (value > ceilingValue) {
        debugPrint("%s: stat %d value %d clamped to ceiling %d", opcodeName, stat, value, ceilingValue);
        value = ceilingValue;
    }
    return value;
}

// set_stat_max(int stat, int value) — 0x81B4
static void op_set_stat_max(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_stat_max: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    value = clampStatBoundaryValue(stat, value, "set_stat_max");
    // M-21: sfall op_set_stat_max calls BOTH SetPCStatMax and SetNPCStatMax
    // (Stats.cpp:430-446), so update both shadow tables.
    statSetMaxValue(stat, value);
    sfallPcStatMaxs[stat] = value;
    sfallNpcStatMaxs[stat] = value;
}

// set_stat_min(int stat, int value) — 0x81B5
static void op_set_stat_min(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_stat_min: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    value = clampStatBoundaryValue(stat, value, "set_stat_min");
    statSetMinValue(stat, value);
    sfallPcStatMins[stat] = value;
    sfallNpcStatMins[stat] = value;
}

// set_pc_stat_max(int stat, int value) — 0x81B7
// ETu needs this for rad resist cap at 100.
// M-21: writes ONLY the PC shadow table (sfall SetPCStatMax, Stats.cpp:403),
// plus the shared engine table for runtime consumption.
static void op_set_pc_stat_max(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_pc_stat_max: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    value = clampStatBoundaryValue(stat, value, "set_pc_stat_max");
    statSetMaxValue(stat, value);
    sfallPcStatMaxs[stat] = value;
}

// set_pc_stat_min(int stat, int value) — 0x81B8
static void op_set_pc_stat_min(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_pc_stat_min: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    value = clampStatBoundaryValue(stat, value, "set_pc_stat_min");
    statSetMinValue(stat, value);
    sfallPcStatMins[stat] = value;
}

// set_npc_stat_max(int stat, int value) — 0x81B9
// M-21: writes ONLY the NPC shadow table (sfall SetNPCStatMax, Stats.cpp:415),
// plus the shared engine table for runtime consumption.
static void op_set_npc_stat_max(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_npc_stat_max: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    value = clampStatBoundaryValue(stat, value, "set_npc_stat_max");
    statSetMaxValue(stat, value);
    sfallNpcStatMaxs[stat] = value;
}

// set_npc_stat_min(int stat, int value) — 0x81BA
static void op_set_npc_stat_min(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);

    if (!statIsValid(stat)) {
        programPrintError("set_npc_stat_min: invalid stat %d (valid range 0-%d)", stat, STAT_COUNT - 1);
        return;
    }

    value = clampStatBoundaryValue(stat, value, "set_npc_stat_min");
    statSetMinValue(stat, value);
    sfallNpcStatMins[stat] = value;
}

// M-21: sfall get_stat_max/get_stat_min accessors honoring the isNpc argument
// (Stats.cpp:387-396: `(isNPC) ? statMaximumsNPC[stat] : statMaximumsPC[stat]`).
// Returns -1 for an invalid stat, matching the metarule's existing error path.
int sfallGetStatMax(int stat, bool isNpc)
{
    if (!statIsValid(stat)) {
        return -1;
    }
    return isNpc ? sfallNpcStatMaxs[stat] : sfallPcStatMaxs[stat];
}

int sfallGetStatMin(int stat, bool isNpc)
{
    if (!statIsValid(stat)) {
        return -1;
    }
    return isNpc ? sfallNpcStatMins[stat] : sfallPcStatMins[stat];
}

// ============================================================
// set_perk_name (0x8189) — sets display name override for a perk.
// Stored in static table; perkGetName() needs integration to read it.
//
// NOTE: sfallPerkNameOverrides and sfallPerkDescOverrides ARE now persisted
// in save/load via sfall_gl_vars_store_string() / sfall_gl_vars_fetch_string()
// (added in global vars version 2). The string data is serialized at the end
// of sfallOpcodeStateSave() and restored at the end of sfallOpcodeStateLoad().
// ============================================================
static constexpr int kMaxPerkNameOverrides = 128;
static char* sfallPerkNameOverrides[kMaxPerkNameOverrides] = {};

static void op_set_perk_name(Program* program)
{
    const char* name = programStackPopString(program);
    int perkID = programStackPopInteger(program);

    if (!perkIsValid(perkID) || perkID >= kMaxPerkNameOverrides) {
        programPrintError("set_perk_name: invalid perk ID %d", perkID);
        return;
    }

    if (sfallPerkNameOverrides[perkID] != nullptr) {
        delete[] sfallPerkNameOverrides[perkID];
        sfallPerkNameOverrides[perkID] = nullptr;
    }

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        sfallPerkNameOverrides[perkID] = new char[len];
        memcpy(sfallPerkNameOverrides[perkID], name, len);
    }
}

// ============================================================
// set_perk_desc (0x818A) — sets description override for a perk.
// ============================================================
static char* sfallPerkDescOverrides[kMaxPerkNameOverrides] = {};

static void op_set_perk_desc(Program* program)
{
    const char* desc = programStackPopString(program);
    int perkID = programStackPopInteger(program);

    if (!perkIsValid(perkID) || perkID >= kMaxPerkNameOverrides) {
        programPrintError("set_perk_desc: invalid perk ID %d", perkID);
        return;
    }

    if (sfallPerkDescOverrides[perkID] != nullptr) {
        delete[] sfallPerkDescOverrides[perkID];
        sfallPerkDescOverrides[perkID] = nullptr;
    }

    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        sfallPerkDescOverrides[perkID] = new char[len];
        memcpy(sfallPerkDescOverrides[perkID], desc, len);
    }
}

// ============================================================
// Perk property override arrays (F-015) — set_perk_* opcodes.
//
// These arrays store script-level overrides for gPerkDescriptions[]
// fields. Indexed by perk ID, initialized to -1 (no override).
// Integration point: perk.cc should check these arrays when
// returning perk property values (similar to how perkGetName()
// should check sfallPerkNameOverrides for name overrides).
//
// Opcode listing (from sfall opcode list):
//   0x8178 set_perk_image     — sets frmId
//   0x8179 set_perk_ranks     — sets maxRank
//   0x817b set_perk_stat      — sets stat (stat to modify per rank)
//   0x817c set_perk_stat_mag  — sets statModifier
//   0x817d set_perk_skill1    — sets param1 (primary skill/gvar)
//   0x817e set_perk_skill1_mag — sets value1
//   0x817f set_perk_type      — sets paramMode (AND/OR mode)
//   0x8180 set_perk_skill2    — sets param2 (secondary skill/gvar)
//   0x8181 set_perk_skill2_mag — sets value2
//   0x8182 set_perk_str       — sets stats[STAT_STRENGTH]
//   0x8183 set_perk_per       — sets stats[STAT_PERCEPTION]
//   0x8184 set_perk_end       — sets stats[STAT_ENDURANCE]
//   0x8185 set_perk_chr       — sets stats[STAT_CHARISMA]
//   0x8186 set_perk_int       — sets stats[STAT_INTELLIGENCE]
//   0x8187 set_perk_agl       — sets stats[STAT_AGILITY]
//   0x8188 set_perk_lck       — sets stats[STAT_LUCK]
// ============================================================
static constexpr int kMaxPerkPropertyOverrides = PERK_COUNT;
static int sfallPerkImageOverrides[PERK_COUNT] = {};
static int sfallPerkRanksOverrides[PERK_COUNT] = {};
static int sfallPerkStatOverrides[PERK_COUNT] = {};
static int sfallPerkStatMagOverrides[PERK_COUNT] = {};
static int sfallPerkSkill1Overrides[PERK_COUNT] = {};
static int sfallPerkSkill1MagOverrides[PERK_COUNT] = {};
static int sfallPerkSkill2Overrides[PERK_COUNT] = {};
static int sfallPerkSkill2MagOverrides[PERK_COUNT] = {};
static int sfallPerkTypeOverrides[PERK_COUNT] = {};
static int sfallPerkSpecialOverrides[PERK_COUNT][PRIMARY_STAT_COUNT] = {};
static bool sfallPerkOverridesInited = false;

// F-008: Saved original compile-time perk minLevels for set_perk_level opcode.
// Indexed by perk ID. -1 = this perk has never been overridden.
// Populated on first override in perkSetMinLevel(); cleared on reset.
int sfallPerkMinLevelOriginal[PERK_COUNT];

static void sfallInitPerkOverrideArrays()
{
    if (sfallPerkOverridesInited) {
        return;
    }
    for (int i = 0; i < PERK_COUNT; i++) {
        sfallPerkImageOverrides[i] = -1;
        sfallPerkRanksOverrides[i] = -1;
        sfallPerkStatOverrides[i] = -1000; // distinct sentinel from -1 (valid stat)
        sfallPerkStatMagOverrides[i] = -1000;
        sfallPerkSkill1Overrides[i] = -1;
        sfallPerkSkill1MagOverrides[i] = -1000;
        sfallPerkSkill2Overrides[i] = -1;
        sfallPerkSkill2MagOverrides[i] = -1000;
        sfallPerkTypeOverrides[i] = -1;
        sfallPerkMinLevelOriginal[i] = -1; // F-008: no override yet
        for (int s = 0; s < PRIMARY_STAT_COUNT; s++) {
            sfallPerkSpecialOverrides[i][s] = -1;
        }
    }
    sfallPerkOverridesInited = true;
}

// Getter implementations for perk override arrays.
int sfallGetPerkImageOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkImageOverrides[perkID] : -1; }
int sfallGetPerkRanksOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkRanksOverrides[perkID] : -1; }
int sfallGetPerkStatOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkStatOverrides[perkID] : -1000; }
int sfallGetPerkStatMagOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkStatMagOverrides[perkID] : -1000; }
int sfallGetPerkSkill1Override(int perkID) { return (perkIsValid(perkID)) ? sfallPerkSkill1Overrides[perkID] : -1; }
int sfallGetPerkSkill1MagOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkSkill1MagOverrides[perkID] : -1000; }
int sfallGetPerkSkill2Override(int perkID) { return (perkIsValid(perkID)) ? sfallPerkSkill2Overrides[perkID] : -1; }
int sfallGetPerkSkill2MagOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkSkill2MagOverrides[perkID] : -1000; }
int sfallGetPerkTypeOverride(int perkID) { return (perkIsValid(perkID)) ? sfallPerkTypeOverrides[perkID] : -1; }
int sfallGetPerkSpecialOverride(int perkID, int statIdx)
{
    if (!perkIsValid(perkID) || statIdx < 0 || statIdx >= PRIMARY_STAT_COUNT) {
        return -1;
    }
    return sfallPerkSpecialOverrides[perkID][statIdx];
}

// set_perk_image(int perkID, int value) — 0x8178
static void op_set_perk_image(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_image: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkImageOverrides[perkID] = value;
}

// set_perk_ranks(int perkID, int value) — 0x8179
// I2-048: Validate value — only accept positive integer (rank count) or -1
// (unlimited). Reject values <= -2 that would permanently lock out the perk.
static void op_set_perk_ranks(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_ranks: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    if (value <= -2) {
        programPrintError("set_perk_ranks: invalid rank value %d for perk %d (must be >= 0 or -1 for unlimited)",
            value, perkID);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkRanksOverrides[perkID] = value;
}

// set_perk_stat(int perkID, int value) — 0x817B
static void op_set_perk_stat(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_stat: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkStatOverrides[perkID] = value;
}

// set_perk_stat_mag(int perkID, int value) — 0x817C
static void op_set_perk_stat_mag(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_stat_mag: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkStatMagOverrides[perkID] = value;
}

// set_perk_skill1(int perkID, int value) — 0x817D
static void op_set_perk_skill1(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_skill1: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSkill1Overrides[perkID] = value;
}

// set_perk_skill1_mag(int perkID, int value) — 0x817E
static void op_set_perk_skill1_mag(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_skill1_mag: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSkill1MagOverrides[perkID] = value;
}

// set_perk_type(int perkID, int value) — 0x817F
static void op_set_perk_type(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_type: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkTypeOverrides[perkID] = value;
}

// set_perk_skill2(int perkID, int value) — 0x8180
static void op_set_perk_skill2(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_skill2: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSkill2Overrides[perkID] = value;
}

// set_perk_skill2_mag(int perkID, int value) — 0x8181
static void op_set_perk_skill2_mag(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_skill2_mag: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSkill2MagOverrides[perkID] = value;
}

// set_perk_str(int perkID, int value) — 0x8182
static void op_set_perk_str(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_str: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_STRENGTH] = value;
}

// set_perk_per(int perkID, int value) — 0x8183
static void op_set_perk_per(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_per: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_PERCEPTION] = value;
}

// set_perk_end(int perkID, int value) — 0x8184
static void op_set_perk_end(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_end: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_ENDURANCE] = value;
}

// set_perk_chr(int perkID, int value) — 0x8185
static void op_set_perk_chr(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_chr: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_CHARISMA] = value;
}

// set_perk_int(int perkID, int value) — 0x8186
// NOTE: Correct opcode is 0x8186. The original comment referenced
// 0x8196 which is used by set_target_knockback.
static void op_set_perk_int(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_int: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_INTELLIGENCE] = value;
}

// set_perk_agl(int perkID, int value) — 0x8187
static void op_set_perk_agl(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_agl: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_AGILITY] = value;
}

// set_perk_lck(int perkID, int value) — 0x8188
static void op_set_perk_lck(Program* program)
{
    int value = programStackPopInteger(program);
    int perkID = programStackPopInteger(program);
    if (!perkIsValid(perkID)) {
        programPrintError("set_perk_lck: invalid perk ID %d (valid range 0-%d)", perkID, PERK_COUNT - 1);
        return;
    }
    sfallInitPerkOverrideArrays();
    sfallPerkSpecialOverrides[perkID][STAT_LUCK] = value;
}

// ============================================================
// get_perk_available (0x8190) — returns 1 if perk is selectable.
// Checks against perk availability criteria using the dude.
// ============================================================
static void op_get_perk_available(Program* program)
{
    int perk = programStackPopInteger(program);

    if (!perkIsValid(perk)) {
        programStackPushInteger(program, 0);
        return;
    }

    // Check if the perk has valid ranks and the dude meets level requirement.
    int result = 0;
    int maxRank = perkGetMaxRank(perk);
    if (maxRank >= 0) {
        int level = pcGetStat(PC_STAT_LEVEL);
        if (level >= perkGetMinLevel(perk)) {
            result = 1;
        }
    }
    programStackPushInteger(program, result);
}

// ============================================================
// Knockback opcodes (0x8195-0x819A).
// Store knockback settings; exposed via sfall_opcodes.h for combat
// integration in combat.cc (attackComputeDamage).
//
// NOTE (F-091): Knockback values are stored in global singletons, NOT per-entity.
// If two scripts set knockback for different weapons/critters, only the last
// call takes effect. The weapon/critter object parameters are accepted for
// API compatibility but do not distinguish per-entity knockback. Converting
// to per-entity storage would require a map from (Object*, knockbackType)
// to values, which is not implemented. Reset via sfallOpcodesReset().
//
// Design tradeoff: Single active knockback override at a time is sufficient
// for the synchronous single-attack combat model — the engine processes one
// attack at a time, consumes the globals in combat.cc, and the values are
// re-read on each attack. Scripts that need different knockback values for
// different entities must set the globals before each attack and be aware that
// only the most recent setter wins if called in rapid succession. Per-entity
// storage would add per-object map complexity without changing the synchronous
// consumption pattern.
// ============================================================
int sfallWeaponKnockbackType = 0;
float sfallWeaponKnockbackValue = 0.0f;
int sfallTargetKnockbackType = 0;
float sfallTargetKnockbackValue = 0.0f;
int sfallAttackerKnockbackType = 0;
float sfallAttackerKnockbackValue = 0.0f;

static void op_set_weapon_knockback(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int type = programStackPopInteger(program);
    Object* weapon = static_cast<Object*>(programStackPopPointer(program));

    if (weapon == nullptr) {
        return;
    }

    sfallWeaponKnockbackType = type;
    sfallWeaponKnockbackValue = value.isFloat() ? value.asFloat() : static_cast<float>(value.integerValue);
    debugPrint("set_weapon_knockback(obj=%p, type=%d) — knockback system integration pending\n",
        static_cast<void*>(weapon), type);
}

static void op_set_target_knockback(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int type = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr) {
        return;
    }

    sfallTargetKnockbackType = type;
    sfallTargetKnockbackValue = value.isFloat() ? value.asFloat() : static_cast<float>(value.integerValue);
    debugPrint("set_target_knockback(obj=%p, type=%d) — knockback system integration pending\n",
        static_cast<void*>(critter), type);
}

static void op_set_attacker_knockback(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int type = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr) {
        return;
    }

    sfallAttackerKnockbackType = type;
    sfallAttackerKnockbackValue = value.isFloat() ? value.asFloat() : static_cast<float>(value.integerValue);
    debugPrint("set_attacker_knockback(obj=%p, type=%d) — knockback system integration pending\n",
        static_cast<void*>(critter), type);
}

static void op_remove_weapon_knockback(Program* program)
{
    Object* weapon = static_cast<Object*>(programStackPopPointer(program));
    (void)weapon;
    sfallWeaponKnockbackType = 0;
    sfallWeaponKnockbackValue = 0.0f;
}

static void op_remove_target_knockback(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    (void)critter;
    sfallTargetKnockbackType = 0;
    sfallTargetKnockbackValue = 0.0f;
}

static void op_remove_attacker_knockback(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    (void)critter;
    sfallAttackerKnockbackType = 0;
    sfallAttackerKnockbackValue = 0.0f;
}

// ============================================================
// set_xp_mod (0x81AA) — sets percentage modifier on XP gain.
// Integration point: pcAddExperience() in stat.cc.
// ============================================================
int gXpModPercentage = 100;

static constexpr int kMaxXpModPercentage = 10000;

static void op_set_xp_mod(Program* program)
{
    int val = programStackPopInteger(program);
    if (val < 0) {
        programPrintError("set_xp_mod: value %d clamped to range [0, %d]", val, kMaxXpModPercentage);
        val = 0;
    }
    if (val > kMaxXpModPercentage) {
        programPrintError("set_xp_mod: value %d clamped to range [0, %d]", val, kMaxXpModPercentage);
        val = kMaxXpModPercentage;
    }
    gXpModPercentage = val;
    sfall_gl_vars_store("SFXpMod%", gXpModPercentage);
}

// ============================================================
// Fake perk/trait opcodes (0x81BB-0x81C2).
// Allow scripts to register custom perks/traits on the perk/trait
// selection screen. Stored in static tables for now; full integration
// requires UI changes in perk_dialog.cc and character_editor.cc.
// ============================================================
static constexpr int kMaxFakePerks = 64;
static FakePerkEntry sfallFakePerks[kMaxFakePerks] = {};
static int sfallFakePerkCount = 0;

static constexpr int kMaxFakeTraits = 16;
static FakeTraitEntry sfallFakeTraits[kMaxFakeTraits] = {};
static int sfallFakeTraitCount = 0;

static char* sfallPerkboxTitle = nullptr;
static bool sfallHideRealPerks = false;

static void sfallFreeFakePerkEntry(FakePerkEntry& entry)
{
    delete[] entry.name;
    delete[] entry.desc;
    entry.name = nullptr;
    entry.desc = nullptr;
}

static void sfallFreeFakeTraitEntry(FakeTraitEntry& entry)
{
    delete[] entry.name;
    delete[] entry.desc;
    entry.name = nullptr;
    entry.desc = nullptr;
}

// set_fake_perk(name, level, image, desc)
static void op_set_fake_perk(Program* program)
{
    const char* desc = programStackPopString(program);
    int image = programStackPopInteger(program);
    int level = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    if (sfallFakePerkCount >= kMaxFakePerks) {
        programPrintError("set_fake_perk: too many fake perks (max %d)", kMaxFakePerks);
        return;
    }

    FakePerkEntry& entry = sfallFakePerks[sfallFakePerkCount++];
    entry.active = true;
    entry.level = level;
    entry.image = image;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        entry.name = new char[len];
        memcpy(entry.name, name, len);
    }
    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        entry.desc = new char[len];
        memcpy(entry.desc, desc, len);
    }
}

// set_fake_trait(name, active, image, desc)
static void op_set_fake_trait(Program* program)
{
    const char* desc = programStackPopString(program);
    int image = programStackPopInteger(program);
    int active = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    if (sfallFakeTraitCount >= kMaxFakeTraits) {
        programPrintError("set_fake_trait: too many fake traits (max %d)", kMaxFakeTraits);
        return;
    }

    FakeTraitEntry& entry = sfallFakeTraits[sfallFakeTraitCount++];
    entry.active = (active != 0) ? 1 : 0;
    entry.image = image;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        entry.name = new char[len];
        memcpy(entry.name, name, len);
    }
    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        entry.desc = new char[len];
        memcpy(entry.desc, desc, len);
    }
}

// set_selectable_perk(name, active, image, desc)
// Alias for set_fake_perk in sfall; registers a perk as selectable.
static void op_set_selectable_perk(Program* program)
{
    const char* desc = programStackPopString(program);
    int image = programStackPopInteger(program);
    int active = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    // Simply delegate to set_fake_perk logic.
    if (!active) {
        return;
    }
    if (sfallFakePerkCount >= kMaxFakePerks) {
        debugPrint("set_selectable_perk: capacity exceeded (%d max) — perk '%s' not added\n", kMaxFakePerks, name ? name : "(null)");
        return;
    }

    FakePerkEntry& entry = sfallFakePerks[sfallFakePerkCount++];
    entry.active = true;
    entry.level = 0;
    entry.image = image;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        entry.name = new char[len];
        memcpy(entry.name, name, len);
    }
    if (desc != nullptr && desc[0] != '\0') {
        size_t len = strlen(desc) + 1;
        entry.desc = new char[len];
        memcpy(entry.desc, desc, len);
    }
}

// set_perkbox_title(title)
static void op_set_perkbox_title(Program* program)
{
    const char* title = programStackPopString(program);

    delete[] sfallPerkboxTitle;
    sfallPerkboxTitle = nullptr;

    if (title != nullptr && title[0] != '\0') {
        size_t len = strlen(title) + 1;
        sfallPerkboxTitle = new char[len];
        memcpy(sfallPerkboxTitle, title, len);
    }
}

// hide_real_perks()
static void op_hide_real_perks(Program* program)
{
    sfallHideRealPerks = true;
}

// show_real_perks()
static void op_show_real_perks(Program* program)
{
    sfallHideRealPerks = false;
}

// has_fake_perk(name) -> int
static void op_has_fake_perk(Program* program)
{
    // Accept either string name or integer extraPerkID.
    ProgramValue arg = programStackPopValue(program);
    int result = 0;

    if (arg.isString()) {
        const char* name = arg.asString(program);
        for (int i = 0; i < sfallFakePerkCount; i++) {
            if (sfallFakePerks[i].name != nullptr
                && strcmp(sfallFakePerks[i].name, name) == 0
                && sfallFakePerks[i].active) {
                result = i + 1;
                break;
            }
        }
    } else {
        int extraPerkID = arg.integerValue;
        // extraPerkID is 1-indexed
        if (extraPerkID > 0 && extraPerkID <= sfallFakePerkCount
            && sfallFakePerks[extraPerkID - 1].active) {
            result = extraPerkID;
        }
    }

    programStackPushInteger(program, result);
}

// has_fake_trait(name or id) -> int
static void op_has_fake_trait(Program* program)
{
    // Accept either string name or integer extraTraitID.
    // Mirrors op_has_fake_perk's dual-mode (lines 4144-4171) for API symmetry.
    ProgramValue arg = programStackPopValue(program);
    int result = 0;

    if (arg.isString()) {
        const char* name = arg.asString(program);
        for (int i = 0; i < sfallFakeTraitCount; i++) {
            // F-M21: Check .active field — mirror op_has_fake_perk pattern.
            if (sfallFakeTraits[i].name != nullptr
                && strcmp(sfallFakeTraits[i].name, name) == 0
                && sfallFakeTraits[i].active) {
                result = i + 1;
                break;
            }
        }
    } else {
        int extraTraitID = arg.integerValue;
        // extraTraitID is 1-indexed
        // F-M21: Check .active field — mirror op_has_fake_perk pattern.
        if (extraTraitID > 0 && extraTraitID <= sfallFakeTraitCount
            && sfallFakeTraits[extraTraitID - 1].active) {
            result = extraTraitID;
        }
    }

    programStackPushInteger(program, result);
}

// ============================================================
// remove_trait (0x8225) — removes a selected trait from the player (F-023).
// Previously commented out; now registered. ET Tu depends on this for
// its trait system. Uses the engine's traitsSetSelected/traitsGetSelected
// API to check if the trait is currently selected and replace it with
// -1 (none).
//
// F-056: Updated to use 3-slot traitsGetSelected/traitsSetSelected to
// support FO1's third trait slot (gFallout1Behavior allows 3 traits).
// In FO2 mode, trait3 defaults to -1 and the third slot is ignored.
//
// I2-049: Also removes the trait from gAddedTraits (set via add_trait
// metarule) to properly clean up stat/skill modifiers. Previously only
// manipulated gSelectedTraits (2-slot engine system), leaving gameplay
// effects from add_trait active after removal via this opcode.
//
// H-11: the empty-slot sentinel is -1, NOT TRAIT_COUNT (16). sfall writes
// -1 (Perks.cpp:239-254); traitGetName(16) returns nullptr (trait.cc:161-167)
// and the character editor (character_editor.cc:2158-2161) draws the raw
// slot with no sanitization → fontDrawText(nullptr) crash at sheet open.
// ============================================================
static void op_remove_trait(Program* program)
{
    int traitID = programStackPopInteger(program);

    if (traitID < 0 || traitID >= TRAIT_COUNT) {
        debugPrint("remove_trait(%d): traitID out of range [0, %d)\n", traitID, TRAIT_COUNT);
        return;
    }

    Trait trait1, trait2, trait3;
    traitsGetSelected(&trait1, &trait2, &trait3);

    if (trait1 == traitID) {
        traitsSetSelected(TRAIT_INVALID, trait2, trait3);
        debugPrint("remove_trait(%d): removed from slot 1\n", traitID);
    } else if (trait2 == traitID) {
        traitsSetSelected(trait1, TRAIT_INVALID, trait3);
        debugPrint("remove_trait(%d): removed from slot 2\n", traitID);
    } else if (trait3 == traitID) {
        traitsSetSelected(trait1, trait2, TRAIT_INVALID);
        debugPrint("remove_trait(%d): removed from slot 3\n", traitID);
    } else {
        debugPrint("remove_trait(%d): trait not currently selected (selected: %d, %d, %d)\n",
            traitID, trait1, trait2, trait3);
    }

    // I2-049: Also remove from gAddedTraits to clean up stat/skill modifiers
    // from the add_trait metarule. This bridges the two independent trait
    // tracking systems. A trait may be in both gSelectedTraits (engine slots)
    // and gAddedTraits (sfall metarule) — remove from both.
    if (sfallRemoveTraitAdded(traitID)) {
        debugPrint("remove_trait(%d): also removed from gAddedTraits\n", traitID);
    }
}

// ============================================================
// Perk/trait modifier opcodes (F-029, F-032). These opcodes are used
// by ET Tu for gameplay modifications. Values are stored in globals
// for future integration with the character/perk systems.
//  0x81C3 perk_add_mode, 0x81C4 clear_selectable_perks,
//  0x81CB set_pyromaniac_mod, 0x81CC apply_heaveho_fix,
//  0x81CD set_swiftlearner_mod, 0x81CE set_hp_per_level_mod.
// ============================================================

static int sfallPerkAddMode = 0;
static bool sfallClearSelectablePerks = false;
static int sfallPyromaniacMod = 0;
static int sfallSwiftLearnerMod = 0;
static int sfallHpPerLevelMod = 0;

static void op_perk_add_mode(Program* program)
{
    sfallPerkAddMode = programStackPopInteger(program);
}

static void op_clear_selectable_perks(Program* program)
{
    (void)program;
    sfallClearSelectablePerks = true;
}

static void op_set_pyromaniac_mod(Program* program)
{
    int val = programStackPopInteger(program);
    if (val < -100) {
        programPrintError("set_pyromaniac_mod: value %d clamped to range [-100, 100]", val);
        val = -100;
    }
    if (val > 100) {
        programPrintError("set_pyromaniac_mod: value %d clamped to range [-100, 100]", val);
        val = 100;
    }
    sfallPyromaniacMod = val;
}

// apply_heaveho_fix: no-op stub — the Heave Ho! perk fix is hardcoded
// in the item code (item.cc:1677-1685) which caps effectiveStrength at
// PRIMARY_STAT_MAX after applying Heave Ho bonus. This opcode exists
// only for script compatibility with original sfall mods.
static void op_apply_heaveho_fix(Program* program)
{
    (void)program;
    debugPrint("apply_heaveho_fix: fix is hardcoded at item.cc:1677-1685; opcode is a no-op");
}

static void op_set_swiftlearner_mod(Program* program)
{
    int val = programStackPopInteger(program);
    if (val < -100) {
        programPrintError("set_swiftlearner_mod: value %d clamped to range [-100, 100]", val);
        val = -100;
    }
    if (val > 100) {
        programPrintError("set_swiftlearner_mod: value %d clamped to range [-100, 100]", val);
        val = 100;
    }
    sfallSwiftLearnerMod = val;
}

static void op_set_hp_per_level_mod(Program* program)
{
    int val = programStackPopInteger(program);
    if (val < -50) {
        programPrintError("set_hp_per_level_mod: value %d clamped to range [-50, 50]", val);
        val = -50;
    }
    if (val > 50) {
        programPrintError("set_hp_per_level_mod: value %d clamped to range [-50, 50]", val);
        val = 50;
    }
    sfallHpPerLevelMod = val;
}

// ============================================================
// set_critter_hit_chance_mod (0x81C5) — modifies hit chance per-critter.
// set_base_hit_chance_mod (0x81C6) — modifies hit chance globally.
//
// Per-critter storage (set_critter_hit_chance_mod) uses a map keyed by
// Object::id so different critters can have independent mods. The global
// sfallHitChanceMod/sfallHitChanceMax are used by set_base_hit_chance_mod
// for a universal floor/ceiling applied to ALL attack rolls.
//
// getter: sfallGetCritterHitChanceMod() returns per-critter override if set.
// ============================================================
int sfallHitChanceMod = 0;
int sfallHitChanceMax = 95;

struct CritterHitChanceEntry {
    int mod;
    int max;
};
static std::unordered_map<int, CritterHitChanceEntry> gCritterHitChanceOverrides;

// Maximum number of per-critter hit chance overrides accepted on load.
// The save format keys use %03d indexing (0-999), so 1000 entries
// is the format's natural capacity. This cap prevents a crafted save
// from injecting a huge hcCount and causing an unbounded restore loop.
static constexpr int kMaxHitChanceOverrides = 1000;

// Maximum numbers of per-critter aimed-shot map entries accepted on load.
// These caps prevent crafted saves from injecting huge counts and causing
// unbounded restore loops. 500 entries matches existing kMaxPickpocketEntries.
static constexpr int kMaxAimedShotEntries = 500;

// Maximum number of skill modifier entries accepted on load (one per skill
// index). Shared across gBaseSkillModMap, gGlobalCritterSkillModMap, and
// per-critter skill overrides (EX-04, EX-05, EX-06 inner).
static constexpr int kMaxSkillModEntries = SKILL_COUNT;

// Maximum number of critter PID entries in gCritterSkillModMap accepted on
// load. 500 matches existing kMaxPickpocketEntries for other pid-keyed maps.
static constexpr int kMaxCritterSkillPidEntries = 500;

// Maximum number of perk min-level overrides accepted on load. PERK_COUNT
// is the natural bound — each entry corresponds to at most one valid perk ID.
static constexpr int kMaxPerkMinLevelOverrides = PERK_COUNT;

bool sfallGetCritterHitChanceMod(Object* critter, int& outMod, int& outMax)
{
    if (critter == nullptr) {
        return false;
    }
    auto it = gCritterHitChanceOverrides.find(critter->id);
    if (it != gCritterHitChanceOverrides.end()) {
        outMod = it->second.mod;
        outMax = it->second.max;
        return true;
    }
    return false;
}

static void op_set_critter_hit_chance_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == nullptr) {
        programPrintError("set_critter_hit_chance_mod: expected critter object");
        return;
    }

    // F-38: Validate FID_TYPE — all 6 sibling op_set_critter_* handlers
    // (op_set_critter_base_stat, op_set_critter_extra_stat,
    // op_set_critter_current_ap, op_set_critter_burst_disable,
    // op_set_critter_skill_mod, op_set_critter_pickpocket_mod) validate
    // that the object is actually a critter before operating on it.
    if (objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_hit_chance_mod: object is not a critter");
        return;
    }

    int origMax = max;
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    if (max != origMax) {
        programPrintError("set_critter_hit_chance_mod: max %d clamped to range [1, 100]", origMax);
    }

    CritterHitChanceEntry entry;
    entry.mod = mod;
    entry.max = max;
    // M-14: Guard against unbounded map growth from script bugs — sibling
    // setters (op_set_critter_skill_mod, op_set_critter_pickpocket_mod,
    // op_mod_kill_counter) all enforce capacity. kMaxHitChanceOverrides
    // matches the load-side cap (line ~7201), so a session exceeding it would
    // otherwise skip the ENTIRE hit-chance override restoration on load.
    if (static_cast<int>(gCritterHitChanceOverrides.size()) >= kMaxHitChanceOverrides
        && gCritterHitChanceOverrides.find(critter->id) == gCritterHitChanceOverrides.end()) {
        debugPrint("set_critter_hit_chance_mod: hit chance map full (%d entries), entry for id %d rejected\n",
            kMaxHitChanceOverrides, critter->id);
        return;
    }
    gCritterHitChanceOverrides[critter->id] = entry;
    debugPrint("set_critter_hit_chance_mod(obj=%p(id=%d), max=%d, mod=%d) — per-critter override stored\n",
        static_cast<void*>(critter), critter->id, max, mod);
}

// ============================================================
// set_hit_chance_max (0x81A1) — sets the maximum to-hit cap.
// Previously commented out; now registered (F-006). ET Tu uses
// this for Fallout 1 hit mechanics where the cap may differ from 95.
// Clamps to valid range [1, 100].
// ============================================================
static void op_set_hit_chance_max(Program* program)
{
    int max = programStackPopInteger(program);
    int origMax = max;
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    if (max != origMax) {
        programPrintError("set_hit_chance_max: value %d clamped to range [1, 100]", origMax);
    }
    sfallHitChanceMax = max;
}

// ============================================================
// set_base_hit_chance_mod (0x81C6) — sets flat hit chance bonus/malus.
// Previously commented out; now registered (F-006). ET Tu uses this
// for Fallout 1 hit mechanics where base chance is adjusted globally.
//
// Writes to global sfallHitChanceMax/sfallHitChanceMod. Per-critter
// hit chance mod (0x81C5) uses separate per-object storage via
// gCritterHitChanceOverrides and sfallGetCritterHitChanceMod().
// ============================================================
static void op_set_base_hit_chance_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    int origMax = max;
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    if (max != origMax) {
        programPrintError("set_base_hit_chance_mod: max %d clamped to range [1, 100]", origMax);
    }
    sfallHitChanceMax = max;
    sfallHitChanceMod = mod;
}

// ============================================================
// Per-critter aimed-shot override flags (F-016).
// force_aimed_shots(pid) forces aimed shots for critter type PID.
// disable_aimed_shots(pid) disables aimed shots for critter type PID.
// Keyed by proto ID (PID). Integration point: combat.cc aimed-shot
// decision should check sfallGetForceAimedShots/sfallGetDisableAimedShots.
// ============================================================
static std::unordered_map<int, bool> gForceAimedShotsMap;
static std::unordered_map<int, bool> gDisableAimedShotsMap;

bool sfallGetForceAimedShots(int pid)
{
    auto it = gForceAimedShotsMap.find(pid);
    return it != gForceAimedShotsMap.end() && it->second;
}

bool sfallGetDisableAimedShots(int pid)
{
    auto it = gDisableAimedShotsMap.find(pid);
    return it != gDisableAimedShotsMap.end() && it->second;
}

static void op_force_aimed_shots(Program* program)
{
    int pid = programStackPopInteger(program);
    gForceAimedShotsMap[pid] = true;
    debugPrint("force_aimed_shots(pid=%d) — per-critter aimed-shot override stored\n", pid);
    // Upstream #701: also seed the engine-level skip set so critterCanAim
    // honors the override at combat time.
    forceAimedShots(pid);
}

static void op_disable_aimed_shots(Program* program)
{
    int pid = programStackPopInteger(program);
    gDisableAimedShotsMap[pid] = true;
    debugPrint("disable_aimed_shots(pid=%d) — per-critter aimed-shot override stored\n", pid);
    disableAimedShots(pid);
}

// ============================================================
// Set skill/pickpocket/critter modifier opcodes (F-029).
// 0x81C7 set_critter_skill_mod, 0x81C8 set_base_skill_mod,
// 0x81C9 set_critter_pickpocket_mod, 0x81CA set_base_pickpocket_mod.
// These opcodes are used by ET Tu for Fallout 1 skill/pickpocket
// mechanics. Values are stored in globals for future integration
// with the combat and skill systems. Also includes:
// 0x81A0 set_pickpocket_max, 0x81AB set_perk_level_mod.
// ============================================================

// Pickpocket max percentage (0x81A0).
static int sfallPickpocketMax = 0;

static void op_set_pickpocket_max(Program* program)
{
    int percentage = programStackPopInteger(program);
    if (percentage < 0) {
        programPrintError("set_pickpocket_max: value %d clamped to range [0, 100]", percentage);
        percentage = 0;
    }
    if (percentage > 100) {
        programPrintError("set_pickpocket_max: value %d clamped to range [0, 100]", percentage);
        percentage = 100;
    }
    sfallPickpocketMax = percentage;
}

// Perk level mod (0x81AB) — reduces number of levels between perk
// selections. Integration point: characterEditorUpdateLevel().
static int sfallPerkLevelMod = 0;

static void op_set_perk_level_mod(Program* program)
{
    int val = programStackPopInteger(program);
    if (val < -10) {
        programPrintError("set_perk_level_mod: value %d clamped to range [-10, 10]", val);
        val = -10;
    }
    if (val > 10) {
        programPrintError("set_perk_level_mod: value %d clamped to range [-10, 10]", val);
        val = 10;
    }
    sfallPerkLevelMod = val;
}

// Skill mod globals (0x81C7, 0x81C8).
// H-23: match sfall max-cap semantics. sfall's op_set_critter_skill_mod reads
// (critter, max) and calls SetSkillMax(obj, max) — a per-critter skill MAX CAP
// applied as min(value, maximum) via CheckSkillMax (sfall Stats.cpp:387-396,
// Skills.cpp:102-109, 267-281). op_set_base_skill_mod reads (max) and calls
// SetSkillMax(0xFFFFFFFF, max) — the GLOBAL base skill max cap, which in this
// fork is gSkillMaxCap (already clamped to by skill.cc:287-291). The fork's
// previous 3-arg/2-arg additive per-skill maps caused real sfall calls
// (e.g. set_critter_skill_mod(dude_obj, 100)) to abort via programFatalError
// (PTR popped as int / stack underflow) and applied modifiers additively
// instead of capping.
//
// Per-critter max caps are stored in gCritterSkillMaxMap (keyed by pid) and
// exposed via sfallGetCritterSkillMax(). The consumer-side clamp belongs in
// skill.cc's skillGetValue (coordinated with the stat/critter fix pass).
//
// gBaseSkillModMap / gGlobalCritterSkillModMap / gCritterSkillModMap additive
// storage is retained for save/load compatibility but is no longer written by
// the opcodes.
const int kNoSkillModOverride = INT_MIN;
static std::unordered_map<int, int> gBaseSkillModMap;
static std::unordered_map<int, int> gGlobalCritterSkillModMap;
static std::unordered_map<int, std::unordered_map<int, int>> gCritterSkillModMap;
// H-23: per-critter skill MAX caps (pid → max), matching sfall SetSkillMax.
static std::unordered_map<int, int> gCritterSkillMaxMap;

static void op_set_critter_skill_mod(Program* program)
{
    int max = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    if (critter == nullptr) {
        programPrintError("set_critter_skill_mod: expected critter object");
        return;
    }
    if (objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_skill_mod: object is not a critter");
        return;
    }
    // sfall SetSkillMax stores the raw maximum; guard pathological values
    // (negative caps would zero every skill through the min() clamp).
    if (max < 0) {
        programPrintError("set_critter_skill_mod: negative max %d rejected", max);
        return;
    }
    // Guard against unbounded map growth from script bugs. Allow existing
    // entries to be modified even at capacity; only reject new entries.
    // kMaxCritterSkillPidEntries matches the load-side cap (line ~6698).
    if (static_cast<int>(gCritterSkillMaxMap.size()) >= kMaxCritterSkillPidEntries
        && gCritterSkillMaxMap.find(critter->pid) == gCritterSkillMaxMap.end()) {
        debugPrint("set_critter_skill_mod: critter skill max map full (%d entries), entry for pid %d rejected\n",
            kMaxCritterSkillPidEntries, critter->pid);
        return;
    }
    gCritterSkillMaxMap[critter->pid] = max;
}

static void op_set_base_skill_mod(Program* program)
{
    int max = programStackPopInteger(program);
    // sfall op_set_base_skill_mod → SetSkillMax(0xFFFFFFFF, max) → the GLOBAL
    // base skill max. In this fork that is gSkillMaxCap, consumed by
    // skillGetValue (skill.cc:287-291: `maxSkill = gSkillMaxCap > 0 ? gSkillMaxCap : 300`).
    if (max < 0) {
        programPrintError("set_base_skill_mod: negative max %d rejected", max);
        return;
    }
    gSkillMaxCap = max;
    sfall_gl_vars_store("SFSkillM", gSkillMaxCap);
}

// H-23: returns the per-critter skill max cap for the given critter's pid,
// or -1 if none is set (callers fall back to the global gSkillMaxCap).
int sfallGetCritterSkillMax(Object* critter)
{
    if (critter == nullptr) {
        return -1;
    }
    auto it = gCritterSkillMaxMap.find(critter->pid);
    if (it != gCritterSkillMaxMap.end()) {
        return it->second;
    }
    return -1;
}

// Pickpocket mod globals (0x81C9, 0x81CA).
static int sfallCritterPickpocketMod = 0;
static int sfallBasePickpocketMod = 0;
static int sfallCritterPickpocketMax = 0;
static int sfallBasePickpocketMax = 0;

// F-001: Per-critter pickpocket mod storage. Key is critter proto ID (pid).
// Previously op_set_critter_pickpocket_mod discarded the critter parameter.
// Each entry stores both the mod value and the max percentage cap.
struct CritterPickpocketEntry {
    int mod;
    int max;
};
static std::unordered_map<int, CritterPickpocketEntry> gCritterPickpocketModMap;

static constexpr int kMaxPickpocketEntries = 500;

static void op_set_critter_pickpocket_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    if (critter == nullptr) {
        // No critter object — fall back to global modifiers for backward compat.
        sfallCritterPickpocketMod = mod;
        sfallCritterPickpocketMax = max;
        return;
    }
    if (objectTypeFromFid(critter->fid) != OBJ_TYPE_CRITTER) {
        programPrintError("set_critter_pickpocket_mod: object is not a critter");
        return;
    }
    // Guard against unbounded map growth from script bugs. Allow existing
    // entries to be modified even at capacity; only reject new entries.
    if (static_cast<int>(gCritterPickpocketModMap.size()) >= kMaxPickpocketEntries
        && gCritterPickpocketModMap.find(critter->pid) == gCritterPickpocketModMap.end()) {
        debugPrint("set_critter_pickpocket_mod: pickpocket map full (%d entries), entry for pid %d rejected\n",
            kMaxPickpocketEntries, critter->pid);
        return;
    }
    CritterPickpocketEntry entry;
    entry.mod = mod;
    entry.max = max;
    gCritterPickpocketModMap[critter->pid] = entry;
}

static void op_set_base_pickpocket_mod(Program* program)
{
    int mod = programStackPopInteger(program);
    int max = programStackPopInteger(program);
    if (max < 1) {
        max = 1;
    }
    if (max > 100) {
        max = 100;
    }
    sfallBasePickpocketMod = mod;
    sfallBasePickpocketMax = max;
}

// ============================================================
// Pickpocket accessor functions — exposed for skill.cc integration.
// These return the values set by set_pickpocket_max (0x81A0),
// set_critter_pickpocket_mod (0x81C9), and set_base_pickpocket_mod (0x81CA).
// Integration point: skillDetermineStealResult() in skill.cc should consult
// sfallGetPickpocketMax() when clamping stealChance at line 1107,
// sfallGetCritterPickpocketMod() / sfallGetBasePickpocketMod() when computing
// stealModifier before line 1105, and the max globals when clamping per-critter
// or global pickpocket caps. See F-021 / F-029 in stage summary.
// ============================================================
int sfallGetPickpocketMax()
{
    return sfallPickpocketMax;
}

int sfallGetCritterPickpocketMod()
{
    return sfallCritterPickpocketMod;
}

int sfallGetBasePickpocketMod()
{
    return sfallBasePickpocketMod;
}

int sfallGetCritterPickpocketMax()
{
    return sfallCritterPickpocketMax;
}

int sfallGetBasePickpocketMax()
{
    return sfallBasePickpocketMax;
}

// F-001: Per-critter pickpocket mod accessor.
// Returns the per-critter pickpocket override if one was set via
// set_critter_pickpocket_mod for this specific critter. Returns false
// if no per-critter override exists; caller should fall back to the
// global sfallGetCritterPickpocketMod() / sfallGetCritterPickpocketMax().
bool sfallGetCritterPickpocketModForCritter(Object* critter, int& outMod, int& outMax)
{
    if (critter == nullptr) {
        return false;
    }
    auto it = gCritterPickpocketModMap.find(critter->pid);
    if (it != gCritterPickpocketModMap.end()) {
        outMod = it->second.mod;
        outMax = it->second.max;
        return true;
    }
    return false;
}

// ============================================================
// sneak_success (0x826C) — returns 1 if sneaking is successful.
// Uses the engine's skill check on the dude's Sneak skill vs 0.
// Pushes 0 if not sneaking or check fails.
// ============================================================
static void op_sneak_success(Program* program)
{
    int result = 0;
    if (gDude != nullptr && dudeHasState(DUDE_STATE_SNEAKING)) {
        int sneakSkill = skillGetValue(gDude, SKILL_SNEAK);
        result = (randomBetween(1, 100) <= sneakSkill) ? 1 : 0;
    }
    programStackPushInteger(program, result);
}

// ============================================================
// create_spatial (0x8273) — creates a spatial script object at a tile.
// Spatial scripts execute when a critter enters their radius.
// ============================================================
static void op_create_spatial(Program* program)
{
    int radius = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    int scriptID = programStackPopInteger(program);

    // Convert 1-based sfall script index to 0-based CE index.
    // Matches the convention used by set_script (0x81F4).
    int scriptIndex = scriptID;
    if (scriptIndex <= 0) {
        programPrintError("create_spatial: invalid script index %d (must be >= 1)", scriptID);
        programStackPushInteger(program, 0);
        return;
    }
    scriptIndex--;

    if (!scriptsIsValidScriptIndex(scriptIndex)) {
        programPrintError("create_spatial: script index %d out of range", scriptID);
        programStackPushInteger(program, 0);
        return;
    }

    // Validate tile and elevation.
    if (!hexGridTileIsValid(tile) || !elevationIsValid(elevation)) {
        programPrintError("create_spatial: invalid tile/elevation (%d, %d)", tile, elevation);
        programStackPushInteger(program, 0);
        return;
    }

    if (radius < 1) {
        programPrintError("create_spatial: invalid radius %d (must be >= 1)", radius);
        programStackPushInteger(program, 0);
        return;
    }

    // Create the spatial script entry.
    int sid;
    if (scriptAdd(&sid, SCRIPT_TYPE_SPATIAL) == -1) {
        programPrintError("create_spatial: failed to create spatial script");
        programStackPushInteger(program, 0);
        return;
    }

    Script* scr;
    if (scriptGetScript(sid, &scr) == -1) {
        programPrintError("create_spatial: failed to get spatial Script for SID %d", sid);
        programStackPushInteger(program, 0);
        return;
    }

    // Set spatial properties — matches mapper's spatial creation pattern
    // (mp_scrpt.cc:map_create_spatial).
    scr->sp.built_tile = builtTileCreate(tile, elevation);
    scr->sp.radius = radius;
    scr->flags |= SCRIPT_FLAG_NO_SAVE; // spatials are not persisted in save files

    // Load and attach the script program.
    // _scr_find_str_run_info loads the .int file from the script index
    // and binds the start procedure to the spatial script.
    if (_scr_find_str_run_info(scriptIndex, nullptr, sid) == -1) {
        programPrintError("create_spatial: failed to load script program for index %d", scriptID);
        programStackPushInteger(program, 0);
        return;
    }

    // Create a marker object at the spatial location (matches mapper pattern).
    // The object is invisible — it serves as the spatial position anchor.
    // In sfall, create_spatial returns this object handle (Object*), which is
    // compatible with spatial_radius and other spatial metarules. Previously
    // we returned the SID (integer), but that caused type mismatch failures
    // in metarule calls (F-002).
    Object* obj = nullptr;
    int markerFid = buildFid(OBJ_TYPE_INTERFACE, 3, 0, 0, ROTATION_NE);
    if (objectCreateWithFidPid(&obj, markerFid, -1) != -1) {
        obj->flags |= OBJECT_NO_SAVE;
        objectHide(obj, nullptr);
        _obj_toggle_flat(obj, nullptr);
        objectSetLocation(obj, tile, elevation, nullptr);
    }

    // Return the spatial object handle (match sfall behavior).
    // If no object was created, return nullptr (0) to signal failure.
    if (obj != nullptr) {
        programStackPushPointer(program, obj);
    } else {
        programStackPushInteger(program, 0);
    }
}

// ============================================================
// hero_select_win (0x8213) — opens hero selection window.
// The win parameter in sfall specifies which window type to open
// (0 = character sheet, 1 = perks, 2 = karma, 3 = skills).
// CE provides equivalent functionality through the character editor
// and interface bar; full window-type routing requires UI integration.
// ============================================================
static void op_hero_select_win(Program* program)
{
    int win = programStackPopInteger(program);

    // Map sfall win parameter to character editor folder:
    //   0 = character sheet (main stats/skills view),
    //   1 = perks tab,
    //   2 = karma tab,
    //   3 = kills (level/location history).
    // Silently no-ops if the character editor is not currently open
    // (characterEditorShow() is a blocking modal dialog — it cannot be
    // opened from within a script handler).
    if (characterEditorGetWindow() >= 0) {
        switch (win) {
        case 0:
            characterEditorSelectFolder(0);
            break;
        case 1:
            characterEditorSelectFolder(1);
            break;
        case 2:
            characterEditorSelectFolder(2);
            break;
        case 3:
            characterEditorSelectFolder(3);
            break;
        default:
            debugPrint("hero_select_win(win=%d) — unsupported window type\n", win);
            break;
        }
    } else {
        debugPrint("hero_select_win(win=%d) — character editor not open, navigation skipped\n", win);
    }
}

// ============================================================
// set_hero_race (0x8214) — stores custom hero race for Hero Appearance.
// ============================================================
static void op_set_hero_race(Program* program)
{
    int race = programStackPopInteger(program);
    sfall_gl_vars_store("HAp_Race", race);
}

// ============================================================
// set_hero_style (0x8215) — stores custom hero style for Hero Appearance.
// ============================================================
static void op_set_hero_style(Program* program)
{
    int style = programStackPopInteger(program);
    sfall_gl_vars_store("HApStyle", style);
}

// ============================================================
// set_pipboy_available (0x818B) — enables/disables pipboy at runtime (F-019).
// Previously commented out; now registered. Overrides the static
// config value pipboy_available_at_game_start (pipboy.cc:442).
// Integration point: pipboyOpen() in pipboy.cc should check
// gPipboyAvailableOverride before the engine's static config.
// ============================================================
int gPipboyAvailableOverride = -1; // -1 = not set, 0 = unavailable, 1 = available

static void op_set_pipboy_available(Program* program)
{
    gPipboyAvailableOverride = programStackPopInteger(program) != 0 ? 1 : 0;
}

// ============================================================
// get_last_target (0x8248) / get_last_attacker (0x8249).
// Returns the last target/attacker of a critter. Maintains both a
// per-critter map and a global fallback for backward compatibility.
// Combat.cc writes gLastAttacker/gLastTarget on each attack; the
// get functions lazily populate per-critter storage from the global
// when the critter parameter matches the most recent attacker.
// ============================================================
int gLastAttacker = -1;
int gLastTarget = -1;

// Per-critter last target/attacker storage (F-030, F-088).
// Keyed by critter Object::id.
static std::unordered_map<int, int> gCritterLastTarget;
static std::unordered_map<int, int> gCritterLastAttacker;

static void op_get_last_target(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    // M-08: sfall returns 0 outside combat — the sources/targets are cleared
    // on combat start AND end (AI.cpp:579-582, 589-590) and the docs state
    // "outside of combat both functions will always return 0". The fork's
    // global fallback returned stale gLastTarget/gLastAttacker for ANY
    // critter outside combat. Return 0 unless combat is active.
    if (!isInCombat()) {
        programStackPushInteger(program, 0);
        return;
    }

    if (critter != nullptr) {
        int critterId = critter->id;

        // Lazy-populate per-critter storage from the global: if this critter
        // was the most recent attacker, cache its last target.
        if (gLastAttacker == critterId) {
            gCritterLastTarget[critterId] = gLastTarget;
        }

        // Check per-critter storage first.
        auto it = gCritterLastTarget.find(critterId);
        if (it != gCritterLastTarget.end() && it->second >= 0) {
            Object* obj = objectFindById(it->second);
            if (obj != nullptr) {
                programStackPushPointer(program, obj);
                return;
            }
            // Object no longer exists — remove stale entry.
            gCritterLastTarget.erase(it);
        }
    }

    // Global fallback for backward compatibility.
    if (gLastTarget >= 0) {
        Object* obj = objectFindById(gLastTarget);
        if (obj != nullptr) {
            programStackPushPointer(program, obj);
            return;
        }
    }
    programStackPushInteger(program, 0);
}

static void op_get_last_attacker(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    // M-08: return 0 outside combat (see op_get_last_target).
    if (!isInCombat()) {
        programStackPushInteger(program, 0);
        return;
    }

    if (critter != nullptr) {
        int critterId = critter->id;

        // Lazy-populate per-critter storage from the global: if this critter
        // was the most recent target, cache its last attacker.
        if (gLastTarget == critterId) {
            gCritterLastAttacker[critterId] = gLastAttacker;
        }

        // Check per-critter storage first.
        auto it = gCritterLastAttacker.find(critterId);
        if (it != gCritterLastAttacker.end() && it->second >= 0) {
            Object* obj = objectFindById(it->second);
            if (obj != nullptr) {
                programStackPushPointer(program, obj);
                return;
            }
            // Object no longer exists — remove stale entry.
            gCritterLastAttacker.erase(it);
        }
    }

    // Global fallback for backward compatibility.
    if (gLastAttacker >= 0) {
        Object* obj = objectFindById(gLastAttacker);
        if (obj != nullptr) {
            programStackPushPointer(program, obj);
            return;
        }
    }
    programStackPushInteger(program, 0);
}

// ============================================================
// get_kill_counter (0x818C) — returns kill counter for critter type.
// mod_kill_counter (0x818D) — modifies kill counter for critter type.
// ET Tu uses these for tracking faction/enemy kill counts. The engine
// maintains kill counters internally (gKillsByType, incremented by
// killsIncByType at combat.cc:5445, displayed at character_editor.cc:
// 2361/4813, exposed to scripts via metarule3 GET_KILL_COUNT).
// ============================================================
// M-15: sfall's op_get_kill_counter / op_mod_kill_counter read/write the
// engine's pc_kill_counts array directly (Handlers/Combat.cpp:41-87). The
// fork's private gSfallKillCounters map was never populated by the engine, so
// get_kill_counter returned 0 for engine-registered kills and mod_kill_counter
// had no effect on the in-game tally. Use the engine API (killsGetByType /
// killsIncByType) so both directions align. The map now stores ONLY negative
// mod deltas (the engine has no decrement API).
static std::unordered_map<int, int> gSfallKillCounters;

static void op_get_kill_counter(Program* program)
{
    int critterType = programStackPopInteger(program);
    // M-15: read the ENGINE tally. killsGetByType bounds-checks (returns 0
    // for invalid indices, critter.cc:748-753).
    int count = killsGetByType(static_cast<KillType>(critterType));
    // Negative mod deltas (mod_kill_counter with amount < 0) cannot be applied
    // to the engine array (no decrement API), so they live in
    // gSfallKillCounters and are layered on top here for consistency.
    auto it = gSfallKillCounters.find(critterType);
    if (it != gSfallKillCounters.end()) {
        count += it->second;
    }
    programStackPushInteger(program, count);
}

static constexpr int kMaxKillCounterEntries = 5000;

// Per-critter last target/attacker maps are keyed by Object::id and grow
// with the number of distinct critters that have been involved in combat.
// 5000 entries is generous for any practical gameplay scenario and matches
// the kill counter limit.
static constexpr int kMaxLastTargetEntries = 5000;

static void op_mod_kill_counter(Program* program)
{
    int amount = programStackPopInteger(program);
    int critterType = programStackPopInteger(program);

    // F-M14: Input validation. critterType is used as a map key; negative
    // values create meaningless entries. Unreasonably large positive values
    // (beyond the proto ID namespace) are also suspicious.
    if (critterType < 0) {
        debugPrint("mod_kill_counter: negative critterType %d rejected\n", critterType);
        return;
    }
    if (critterType >= KILL_TYPE_DEFAULT_COUNT) {
        debugPrint("mod_kill_counter: critterType %d out of range [0, %d)\n", critterType, KILL_TYPE_DEFAULT_COUNT);
        return;
    }

    // Clamp amount to prevent signed integer overflow in the += operation.
    // Range [-1e6, 1e6] is generous for any practical gameplay scenario
    // while preventing arithmetic UB.
    if (amount > 1000000) {
        amount = 1000000;
    } else if (amount < -1000000) {
        amount = -1000000;
    }

    // M-15: apply to the ENGINE tally via the bounded engine API. The engine
    // only exposes +1 increments (killsIncByType), so loop for positive
    // amounts (bounded by the 1e6 clamp above). Negative amounts are applied
    // by... the engine has no decrement API; sfall's mod_kill_counter does a
    // direct array add which supports negatives. Keep the negative delta in a
    // bounded private map layered on top of the engine tally so
    // get_kill_counter remains consistent for negative mods.
    if (amount > 0) {
        for (int i = 0; i < amount; i++) {
            killsIncByType(static_cast<KillType>(critterType));
        }
    } else if (amount < 0) {
        // Guard against unbounded map growth from script bugs. Allow existing
        // entries to be modified even at capacity; only reject new entries.
        if (static_cast<int>(gSfallKillCounters.size()) >= kMaxKillCounterEntries
            && gSfallKillCounters.find(critterType) == gSfallKillCounters.end()) {
            debugPrint("mod_kill_counter: kill counter map full (%d entries), new entry for critterType %d rejected\n",
                kMaxKillCounterEntries, critterType);
            return;
        }
        // Guard against signed integer overflow (I2-M003).
        int currentValue = gSfallKillCounters[critterType];
        if (amount < 0 && currentValue < INT_MIN - amount) {
            gSfallKillCounters[critterType] = INT_MIN;
        } else {
            gSfallKillCounters[critterType] = currentValue + amount;
        }
    }
}

// ============================================================
// get_perk_owed (0x818E) — returns how many perk levels the player is
// behind. set_perk_owed (0x818F) — sets the perk-owed counter.
// ET Tu perk system depends on these for "missed perk" tracking.
// ============================================================
static int gSfallPerkOwed = 0;

static void op_get_perk_owed(Program* program)
{
    programStackPushInteger(program, gSfallPerkOwed);
}

static void op_set_perk_owed(Program* program)
{
    int value = programStackPopInteger(program);
    // Guard: perk-owed is a counter of missed perks, linearly decremented.
    // Negative or unreasonably large values are bogus script input.
    if (value < 0) {
        value = 0;
    } else if (value > 255) {
        value = 255;
    }
    sfallSetPerkOwed(value);
}

// ============================================================
// sfallAnimCallbackInvoke — public API to invoke the registered
// sfall animation callback procedure on the given object.
// Should be called from animation completion paths in animation.cc.
// ============================================================
void sfallAnimCallbackInvoke(Object* object)
{
    if (sfallAnimCallbackProgram == nullptr || sfallAnimCallbackProcedureIndex < 0) {
        return;
    }

    if (object == nullptr) {
        return;
    }

    // Snapshot the callback state and clear globals IMMEDIATELY to prevent
    // use-after-free (F-105). The sfallAnimCallbackProgram pointer references
    // a Program* whose lifetime is managed by the interpreter. If we clear
    // the globals before calling programExecuteProcedure, the following
    // scenarios are all safe:
    //
    // 1. The callback procedure itself calls reg_anim_callback to set a new
    //    callback — it won't interfere with this invocation.
    // 2. The callback procedure triggers script cleanup that frees this
    //    program — we no longer hold a dangling reference to it.
    // 3. programExecuteProcedure longjmps out (e.g., programFatalError) —
    //    the globals are already cleared, so re-entry is safe.
    //
    // The permanent fix requires clearing sfallAnimCallbackProgram in
    // programFree() (interpreter.cc:line ~1340), which is outside this
    // translation unit. This one-shot clearing is the best-effort guard
    // available within sfall_opcodes.cc.
    Program* program = sfallAnimCallbackProgram;
    int procIndex = sfallAnimCallbackProcedureIndex;
    sfallAnimCallbackProgram = nullptr;
    sfallAnimCallbackProcedureIndex = -1;

    // Validate the program is still alive before proceeding.
    if (program->exited || program->data == nullptr) {
        return;
    }

    if (procIndex < 0 || procIndex >= program->procedureCount()) {
        return;
    }

    programStackPushValue(program, ProgramValue(object));
    programExecuteProcedure(program, procIndex);
    // Already cleared above; no need to reset again.
}

// Movie path override storage (F-021). Set via set_movie_path (0x8177).
// Declared at file scope (referenced by sfallOpcodesReset).
static constexpr int kMaxMoviePathOverrides = 32;
static char* sfallMoviePathOverrides[kMaxMoviePathOverrides] = {};

void sfallOpcodesReset()
{
    // Free and clear fake perk entries.
    for (int i = 0; i < sfallFakePerkCount; i++) {
        sfallFreeFakePerkEntry(sfallFakePerks[i]);
    }
    sfallFakePerkCount = 0;

    // Free and clear fake trait entries.
    for (int i = 0; i < sfallFakeTraitCount; i++) {
        sfallFreeFakeTraitEntry(sfallFakeTraits[i]);
    }
    sfallFakeTraitCount = 0;

    // Reset XP modifier to default.
    gXpModPercentage = 100;

    // Reset hit chance globals.
    sfallHitChanceMod = 0;
    sfallHitChanceMax = 95;

    // Reset knockback globals.
    sfallWeaponKnockbackType = 0;
    sfallWeaponKnockbackValue = 0.0f;
    sfallTargetKnockbackType = 0;
    sfallTargetKnockbackValue = 0.0f;
    sfallAttackerKnockbackType = 0;
    sfallAttackerKnockbackValue = 0.0f;

    // ============================================================
    // String state (perk names, descriptions, perkbox title) IS now
    // persisted to savegames via sfall_gl_vars_store_string() (F-003/F-004
    // fix, global vars version 2). These arrays are cleared here on game
    // reset and restored by sfallOpcodeStateLoad() after loading a save.
    // ============================================================
    // Free perk name/desc override entries.
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        if (sfallPerkNameOverrides[i] != nullptr) {
            delete[] sfallPerkNameOverrides[i];
            sfallPerkNameOverrides[i] = nullptr;
        }
    }
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        if (sfallPerkDescOverrides[i] != nullptr) {
            delete[] sfallPerkDescOverrides[i];
            sfallPerkDescOverrides[i] = nullptr;
        }
    }

    // Reset perkbox UI state.
    delete[] sfallPerkboxTitle;
    sfallPerkboxTitle = nullptr;
    sfallHideRealPerks = false;

    // Reset last target/attacker tracking.
    gLastAttacker = -1;
    gLastTarget = -1;
    gCritterLastTarget.clear();
    gCritterLastAttacker.clear();

    // Reset hero model overrides set via set_dm_model / set_df_model.
    gCustomMaleHeroModelNum = 0;
    gCustomFemaleHeroModelNum = 0;

    // Reset skill/perk modifiers set via metarule opcodes.
    gSkillMaxCap = 300;
    gPerkFrequencyOverride = 0;
    gSkillPointsPerLevelMod = 0;

    // F-034: Destroy any lingering non-blocking message window.
    if (gMessageWindow != -1) {
        windowDestroy(gMessageWindow);
        gMessageWindow = -1;
    }

    // Reset pipboy availability override set by set_pipboy_available (0x818B).
    gPipboyAvailableOverride = -1;

    // Reset kill counter map.
    gSfallKillCounters.clear();

    // Reset perk owed counter.
    gSfallPerkOwed = 0;

    // F-008: Reset perk min level overrides set by set_perk_level (0x817A).
    // Restore compile-time minLevels that were saved on first override,
    // then clear the override tracking array.
    // F2-023: Guard with sfallPerkOverridesInited — before sfallOpcodesInit()
    // sets sentinels to -1, sfallPerkMinLevelOriginal is zero-initialized.
    // Without this guard, the first pre-init reset calls perkSetMinLevel(i, 0)
    // for all 119 perks (0 != -1 sentinel check passes, but 0 is not a valid
    // sentinel — it's the uninitialized zero value).
    if (sfallPerkOverridesInited) {
        for (int i = 0; i < PERK_COUNT; i++) {
            if (sfallPerkMinLevelOriginal[i] != -1) {
                perkSetMinLevel(i, sfallPerkMinLevelOriginal[i]);
                sfallPerkMinLevelOriginal[i] = -1;
            }
        }
    }

    // Reset pickpocket max override.
    sfallPickpocketMax = 0;

    // Reset perk level mod.
    sfallPerkLevelMod = 0;

    // Reset skill/pickpocket mod globals.
    gBaseSkillModMap.clear();
    gGlobalCritterSkillModMap.clear();
    sfallCritterPickpocketMod = 0;
    sfallBasePickpocketMod = 0;
    sfallCritterPickpocketMax = 0;
    sfallBasePickpocketMax = 0;

    // Reset per-critter skill/pickpocket mod maps (F-001, F-013, H-23).
    gCritterSkillModMap.clear();
    gCritterSkillMaxMap.clear();
    gCritterPickpocketModMap.clear();

    // Reset perk add mode and selectable perks flag.
    sfallPerkAddMode = 0;
    sfallClearSelectablePerks = false;

    // Reset pyromaniac/swiftlearner/HP per level mods.
    sfallPyromaniacMod = 0;
    sfallSwiftLearnerMod = 0;
    sfallHpPerLevelMod = 0;

    // Reset per-critter hit chance overrides (F-019).
    gCritterHitChanceOverrides.clear();

    // Reset aimed-shot per-critter override maps (F-016).
    gForceAimedShotsMap.clear();
    gDisableAimedShotsMap.clear();

    // Reset movie path overrides (F-021).
    for (int i = 0; i < kMaxMoviePathOverrides; i++) {
        delete[] sfallMoviePathOverrides[i];
        sfallMoviePathOverrides[i] = nullptr;
    }

    // Reset perk property override arrays (F-015).
    sfallPerkOverridesInited = false;
    sfallInitPerkOverrideArrays();

    // F-040: Reset stat min/max bounds to compile-time defaults.
    // Stat bounds are modified by op_set_stat_max/op_set_stat_min (0x81B4/0x81B5)
    // and friends, but were not reset on gameReset. Restore original values
    // captured in sfallOpcodesInit(). Follows the same pattern as gSkillMaxCap.
    if (sfallStatBoundsCaptured) {
        for (int stat = 0; stat < STAT_COUNT; stat++) {
            statSetMinValue(stat, sfallOriginalStatMins[stat]);
            statSetMaxValue(stat, sfallOriginalStatMaxs[stat]);
            // M-21: restore the PC/NPC shadow cap tables alongside the shared table.
            sfallPcStatMins[stat] = sfallOriginalStatMins[stat];
            sfallPcStatMaxs[stat] = sfallOriginalStatMaxs[stat];
            sfallNpcStatMins[stat] = sfallOriginalStatMins[stat];
            sfallNpcStatMaxs[stat] = sfallOriginalStatMaxs[stat];
        }
    }
}

// ============================================================
// F2-042: TEST-ONLY accessors for file-static maps.
// gCritterHitChanceOverrides, gForceAimedShotsMap, and
// gDisableAimedShotsMap are declared file-static and were
// previously structurally inaccessible from unit tests.
// These accessors allow tests to verify that sfallOpcodesReset()
// correctly clears all three maps.
// Guarded behind TEST_ACCESSORS_ENABLED to prevent accidental
// production use; test files define the macro before including
// sfall_opcodes.h.
// ============================================================
#if defined(TEST_ACCESSORS_ENABLED)
int sfallGetCritterHitChanceOverrideCount()
{
    return static_cast<int>(gCritterHitChanceOverrides.size());
}

int sfallGetForceAimedShotsMapCount()
{
    return static_cast<int>(gForceAimedShotsMap.size());
}

int sfallGetDisableAimedShotsMapCount()
{
    return static_cast<int>(gDisableAimedShotsMap.size());
}

// I2-M74: TEST-ONLY accessors for PerkboxTitle and KillCounters.
// These file-static variables were previously inaccessible from unit tests.
const char* sfallGetPerkboxTitleForTest()
{
    return sfallPerkboxTitle;
}

int sfallGetKillCounterCountForTest()
{
    return static_cast<int>(gSfallKillCounters.size());
}

int sfallGetKillCounterValueForTest(int critterType)
{
    auto it = gSfallKillCounters.find(critterType);
    return (it != gSfallKillCounters.end()) ? it->second : -1;
}

void sfallSetKillCounterForTest(int critterType, int count)
{
    gSfallKillCounters[critterType] = count;
}
#endif

// ============================================================
// Save/load key constants (F-M03) — shared between
// sfallOpcodeStateSave() and sfallOpcodeStateLoad() to prevent
// typo-induced data loss from independently-duplicated string literals.
// When adding a new key: define the constexpr here, use in both functions.
// Generated keys (sprintf pattern) use format-key constants instead.
// ============================================================
static constexpr const char kGlVarXpMod[] = "SFXpMod%";
static constexpr const char kGlVarSkillMax[] = "SFSkillM";
static constexpr const char kGlVarPerkFreq[] = "SFPerkFr";
static constexpr const char kGlVarSkillPts[] = "SFSkillP";
static constexpr const char kGlVarHitChMod[] = "SFHCMod ";
static constexpr const char kGlVarHitChMax[] = "SFHCMax ";
static constexpr const char kGlVarWpnKnTyp[] = "SFWKnTyp";
static constexpr const char kGlVarWpnKnVal[] = "SFWKnVl ";
static constexpr const char kGlVarTgtKnTyp[] = "SFTKnTyp";
static constexpr const char kGlVarTgtKnVal[] = "SFTKnVl ";
static constexpr const char kGlVarAtkKnTyp[] = "SFAKnTyp";
static constexpr const char kGlVarAtkKnVal[] = "SFAKnVl ";
static constexpr const char kGlVarPipboy[] = "SFPipboy";
static constexpr const char kGlVarPerkAddMode[] = "SFPerkAM";
static constexpr const char kGlVarPerkClrSel[] = "SFPerkCS";
static constexpr const char kGlVarPyroMod[] = "SFPyroMd";
static constexpr const char kGlVarSwiftMod[] = "SFSwiftM";
static constexpr const char kGlVarHpPMod[] = "SFHpPMd ";
static constexpr const char kGlVarStatCnt[] = "SFStMCnt";
static constexpr const char kGlVarPkpMax[] = "SFPkpMax";
static constexpr const char kGlVarCrtPMax[] = "SFCrtPMx";
static constexpr const char kGlVarBasePMax[] = "SFBasePx";
static constexpr const char kGlVarCrtPMod[] = "SFCrtPMn";
static constexpr const char kGlVarBasePMod[] = "SFBasePn";
static constexpr const char kGlVarPerkLvlMod[] = "SFPerkLM";
static constexpr const char kGlVarUnApBn[] = "SFUnApBn";
static constexpr const char kGlVarUnApPk[] = "SFUnApPk";
static constexpr const char kGlVarPerkMLCnt[] = "SFPMLCt";
static constexpr const char kGlVarBaseSkCnt[] = "SFBSMcnt";
static constexpr const char kGlVarGlobCt[] = "SFGCnt  ";
static constexpr const char kGlVarCrtSkCnt[] = "SFCrtSc";
static constexpr const char kGlVarCrtSkMaxCnt[] = "SFCrtMx";
static constexpr const char kGlVarPerkOwed[] = "SFPerkOw";
static constexpr const char kGlVarHideRP[] = "SFHideRP";
static constexpr const char kGlVarPerkImgCnt[] = "SFPicnt ";
static constexpr const char kGlVarPerkRankCnt[] = "SFPRcnt ";
static constexpr const char kGlVarPerkStatCnt[] = "SFPscnt ";
static constexpr const char kGlVarPerkMagCnt[] = "SFPmcnt ";
static constexpr const char kGlVarPerkSk1Cnt[] = "SFs1cnt ";
static constexpr const char kGlVarPerk1MagCnt[] = "SF1Mcnt ";
static constexpr const char kGlVarPerkSk2Cnt[] = "SFs2cnt ";
static constexpr const char kGlVarPerk2MagCnt[] = "SF2Mcnt ";
static constexpr const char kGlVarPerkTypeCnt[] = "SFPtcnt ";
static constexpr const char kGlVarPerkSpecCnt[] = "SFSacnt ";
static constexpr const char kGlVarKillCtrCnt[] = "SFKCcnt ";
static constexpr const char kGlVarHitChCtrCnt[] = "SFHCcnt ";
static constexpr const char kGlVarCrtPMapCnt[] = "SFCPMcnt";
static constexpr const char kGlVarFakePkCnt[] = "SFFPkcnt";
static constexpr const char kGlVarFakeTrCnt[] = "SFFTkcnt";
static constexpr const char kGlVarFASMapCnt[] = "SFFAScnt";
static constexpr const char kGlVarDASMapCnt[] = "SFDAScnt";
static constexpr const char kGlVarDMaleModel[] = "SFDMale ";
static constexpr const char kGlVarDFemaleModel[] = "SFDFmale";
static constexpr const char kGlVarLastTrgCnt[] = "SFLTcnt ";
static constexpr const char kGlVarLastAtkCnt[] = "SFLAcnt ";
// UM-17: sfall animation callback serialization.
// sfallAnimCallbackProgram is a Program* pointer (non-serializable);
// only the procedure index and active flag survive save/load.
// On load, the procedure index is restored but the program pointer
// remains nullptr — op_reg_anim_callback must be called again after
// load to re-establish the callback.
static constexpr const char kGlVarAnimCbAct[]  = "SFCBAct ";
static constexpr const char kGlVarAnimCbProc[] = "SFCBPrc ";
static constexpr const char kGlVarInvApCost[] = "SFInvApC";
static constexpr const char kGlVarInvQPRed[] = "SFQPApRd";

// Persist all opcode-related global state into the sfall global vars map
// before sfall_gl_vars_save() writes to sfallgv.sav.
// Keys match the 8-character sfall convention.
void sfallOpcodeStateSave()
{
    // Core opcode globals (already persisted before F-001 fix).
    sfall_gl_vars_store(kGlVarXpMod, gXpModPercentage);
    sfall_gl_vars_store(kGlVarSkillMax, gSkillMaxCap);
    sfall_gl_vars_store(kGlVarPerkFreq, gPerkFrequencyOverride);
    sfall_gl_vars_store(kGlVarSkillPts, gSkillPointsPerLevelMod);

    // Hit chance globals (extern in header, owned here).
    sfall_gl_vars_store(kGlVarHitChMod, sfallHitChanceMod);
    sfall_gl_vars_store(kGlVarHitChMax, sfallHitChanceMax);

    // Knockback globals (extern in header, owned here).
    sfall_gl_vars_store(kGlVarWpnKnTyp, sfallWeaponKnockbackType);
    sfall_gl_vars_store_float(kGlVarWpnKnVal, sfallWeaponKnockbackValue);
    sfall_gl_vars_store(kGlVarTgtKnTyp, sfallTargetKnockbackType);
    sfall_gl_vars_store_float(kGlVarTgtKnVal, sfallTargetKnockbackValue);
    sfall_gl_vars_store(kGlVarAtkKnTyp, sfallAttackerKnockbackType);
    sfall_gl_vars_store_float(kGlVarAtkKnVal, sfallAttackerKnockbackValue);

    // Pipboy availability override.
    sfall_gl_vars_store(kGlVarPipboy, gPipboyAvailableOverride);

    // UM-17: Persist sfall animation callback state.
    // The Program* pointer is non-serializable; only the active flag and
    // procedure index are saved. On load, the procedure index is restored
    // but the program pointer must be re-established by a subsequent
    // reg_anim_callback call.
    sfall_gl_vars_store(kGlVarAnimCbAct, sfallAnimCallbackProgram != nullptr ? 1 : 0);
    sfall_gl_vars_store(kGlVarAnimCbProc, sfallAnimCallbackProcedureIndex);

    // Perk/trait modifier opcodes — storage-only globals.
    sfall_gl_vars_store(kGlVarPerkAddMode, sfallPerkAddMode);
    sfall_gl_vars_store(kGlVarPerkClrSel, sfallClearSelectablePerks ? 1 : 0);
    sfall_gl_vars_store(kGlVarPyroMod, sfallPyromaniacMod);
    sfall_gl_vars_store(kGlVarSwiftMod, sfallSwiftLearnerMod);
    sfall_gl_vars_store(kGlVarHpPMod, sfallHpPerLevelMod);

    // F-003/F-004: sfallPerkboxTitle and other string state is now persisted
    // via sfall_gl_vars_store_string() (added in global vars version 2).
    // The string serialization occurs at the end of this function after all
    // int/float globals have been saved.

    // F-002: Save per-stat min/max bounds set via set_stat_max (0x81B4),
    // set_stat_min (0x81B5), and their PC/NPC variants. These modify
    // gStatDescriptions[] limits in stat.cc and must survive save/load.
    // R-15 (M-21, CONFIRMED): the save format previously persisted ONLY the
    // single shared engine value (statGetMinValue/statGetMaxValue) under
    // SFStMn/SFStMx, and the load path wrote BOTH PC and NPC shadow tables
    // from it — the PC/NPC cap distinction (set_pc_stat_max vs
    // set_npc_stat_max) was lost on save/load. Now persist each shadow table
    // under its own key (SFPcMn/SFPcMx/SFNpMn/SFNpMx). The shared SFStMn/
    // SFStMx keys are retained for the engine table and old-save compat.
    {
        sfall_gl_vars_store(kGlVarStatCnt, STAT_COUNT);
        for (int stat = 0; stat < STAT_COUNT; stat++) {
            char key[16] = {};
            sprintf(key, "SFStMn%02d", stat);
            sfall_gl_vars_store(key, statGetMinValue(stat));
            sprintf(key, "SFStMx%02d", stat);
            sfall_gl_vars_store(key, statGetMaxValue(stat));
            // R-15: PC/NPC shadow tables under distinct keys.
            sprintf(key, "SFPcMn%02d", stat);
            sfall_gl_vars_store(key, sfallPcStatMins[stat]);
            sprintf(key, "SFPcMx%02d", stat);
            sfall_gl_vars_store(key, sfallPcStatMaxs[stat]);
            sprintf(key, "SFNpMn%02d", stat);
            sfall_gl_vars_store(key, sfallNpcStatMins[stat]);
            sprintf(key, "SFNpMx%02d", stat);
            sfall_gl_vars_store(key, sfallNpcStatMaxs[stat]);
        }
    }

    // Pickpocket modifier opcodes.
    // F-013: sfallCritterPickpocketMax and sfallBasePickpocketMax are
    // saved/loaded here. sfallGetBasePickpocketMax() is now consumed by
    // skillsPerformStealing()/skillDetermineStealResult() at skill.cc:1142-1162
    // as part of the F-09 fix (priority: per-critter > base > global > 95 fallback).
    sfall_gl_vars_store(kGlVarPkpMax, sfallPickpocketMax);
    sfall_gl_vars_store(kGlVarCrtPMax, sfallCritterPickpocketMax);
    sfall_gl_vars_store(kGlVarBasePMax, sfallBasePickpocketMax);
    sfall_gl_vars_store(kGlVarCrtPMod, sfallCritterPickpocketMod);
    sfall_gl_vars_store(kGlVarBasePMod, sfallBasePickpocketMod);

    // Perk level modifier.
    sfall_gl_vars_store(kGlVarPerkLvlMod, sfallPerkLevelMod);

    // F-015: Save unspent AP bonuses (set via set_unspent_ap_bonus /
    // set_unspent_ap_perk_bonus opcodes). Stored in stat.cc static ints;
    // must be serialized or they reset to default (4) on game load.
    sfall_gl_vars_store(kGlVarUnApBn, statGetUnspentApBonus());
    sfall_gl_vars_store(kGlVarUnApPk, statGetUnspentApPerkBonus());

    // F-59: Save inventory AP cost and Quick Pockets reduction.
    // These are modified at runtime via op_set_inven_ap_cost (0x8172)
    // and must survive save/load.
    sfall_gl_vars_store(kGlVarInvApCost, inventoryGetRawApCost());
    sfall_gl_vars_store(kGlVarInvQPRed, inventoryGetQuickPocketsApCostReduction());

    // F-008: Save perk min level overrides set by set_perk_level (0x817A).
    // Store count + per-index (perk ID, current override value) pairs.
    {
        int savedCount = 0;
        for (int i = 0; i < PERK_COUNT; i++) {
            if (sfallPerkMinLevelOriginal[i] != -1) {
                savedCount++;
            }
        }
        sfall_gl_vars_store(kGlVarPerkMLCnt, savedCount);
        int idx = 0;
        for (int i = 0; i < PERK_COUNT; i++) {
            if (sfallPerkMinLevelOriginal[i] != -1) {
                char pkKey[16] = {};
                char pvKey[16] = {};
                sprintf(pkKey, "SFPk%03d", idx);
                sprintf(pvKey, "SFPv%03d", idx);
                sfall_gl_vars_store(pkKey, i);
                sfall_gl_vars_store(pvKey, perkGetMinLevel(i));
                idx++;
            }
        }
    }

    // Skill modifier globals (F-013: stored per-skill).
    // gBaseSkillModMap: skill → mod for set_base_skill_mod.
    {
        int skCount = static_cast<int>(gBaseSkillModMap.size());
        sfall_gl_vars_store(kGlVarBaseSkCnt, skCount);
        int idx = 0;
        for (const auto& [skill, mod] : gBaseSkillModMap) {
            char skKey[16] = {};
            char modKey[16] = {};
            sprintf(skKey, "SFBSMsk%02d", idx);
            sprintf(modKey, "SFBSMmv%02d", idx);
            sfall_gl_vars_store(skKey, skill);
            sfall_gl_vars_store(modKey, mod);
            idx++;
        }
    }
    // gGlobalCritterSkillModMap: skill → mod for set_critter_skill_mod
    // with no critter (nullptr fallback).  Saved with "SFGCrt" prefix
    // to avoid collision with the per-critter gCritterSkillModMap.
    {
        int gcCount = static_cast<int>(gGlobalCritterSkillModMap.size());
        sfall_gl_vars_store(kGlVarGlobCt, gcCount);
        int idx = 0;
        for (const auto& [skill, mod] : gGlobalCritterSkillModMap) {
            char skKey[16] = {};
            char modKey[16] = {};
            sprintf(skKey, "SFGCsk%02d", idx);
            sprintf(modKey, "SFGCmv%02d", idx);
            sfall_gl_vars_store(skKey, skill);
            sfall_gl_vars_store(modKey, mod);
            idx++;
        }
    }
    // gCritterSkillModMap: pid → (skill → mod) for set_critter_skill_mod.
    {
        int crtCount = static_cast<int>(gCritterSkillModMap.size());
        sfall_gl_vars_store(kGlVarCrtSkCnt, crtCount);
        int idx = 0;
        for (const auto& [pid, skillMap] : gCritterSkillModMap) {
            char pidKey[16] = {};
            sprintf(pidKey, "SFCrP%04d", idx);
            sfall_gl_vars_store(pidKey, pid);
            int skCount2 = static_cast<int>(skillMap.size());
            char skCntKey[16] = {};
            sprintf(skCntKey, "SFCrPn%04d", idx);
            sfall_gl_vars_store(skCntKey, skCount2);
            int skIdx = 0;
            for (const auto& [skill, mod] : skillMap) {
                char skKey[32] = {};
                char modKey[32] = {};
                sprintf(skKey, "SFCrSk%04d_%02d", idx, skIdx);
                sprintf(modKey, "SFCrSv%04d_%02d", idx, skIdx);
                sfall_gl_vars_store(skKey, skill);
                sfall_gl_vars_store(modKey, mod);
                skIdx++;
            }
            idx++;
        }
    }
    // H-23: gCritterSkillMaxMap: pid → max (per-critter skill max cap).
    // Saved with "SFCm" prefix to avoid collision with the additive maps.
    {
        int crtMaxCount = static_cast<int>(gCritterSkillMaxMap.size());
        sfall_gl_vars_store(kGlVarCrtSkMaxCnt, crtMaxCount);
        int idx = 0;
        for (const auto& [pid, max] : gCritterSkillMaxMap) {
            char pidKey[16] = {};
            char maxKey[16] = {};
            sprintf(pidKey, "SFCmP%04d", idx);
            sprintf(maxKey, "SFCmV%04d", idx);
            sfall_gl_vars_store(pidKey, pid);
            sfall_gl_vars_store(maxKey, max);
            idx++;
        }
    }

    // Perk owed counter.
    sfall_gl_vars_store(kGlVarPerkOwed, gSfallPerkOwed);

    // Hide real perks flag.
    sfall_gl_vars_store(kGlVarHideRP, sfallHideRealPerks ? 1 : 0);

    // Perk property override arrays — persisted as indexed key/value pairs.
    // Each array stores its count (always PERK_COUNT) followed by per-index entries.
    sfall_gl_vars_store(kGlVarPerkImgCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFPi%03d", i);
        sfall_gl_vars_store(key, sfallPerkImageOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkRankCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFPr%03d", i);
        sfall_gl_vars_store(key, sfallPerkRanksOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkStatCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFPs%03d", i);
        sfall_gl_vars_store(key, sfallPerkStatOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkMagCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFPm%03d", i);
        sfall_gl_vars_store(key, sfallPerkStatMagOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkSk1Cnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFs1%03d", i);
        sfall_gl_vars_store(key, sfallPerkSkill1Overrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerk1MagCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SF1M%03d", i);
        sfall_gl_vars_store(key, sfallPerkSkill1MagOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkSk2Cnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFs2%03d", i);
        sfall_gl_vars_store(key, sfallPerkSkill2Overrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerk2MagCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SF2M%03d", i);
        sfall_gl_vars_store(key, sfallPerkSkill2MagOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkTypeCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        char key[16] = {};
        sprintf(key, "SFPt%03d", i);
        sfall_gl_vars_store(key, sfallPerkTypeOverrides[i]);
    }
    sfall_gl_vars_store(kGlVarPerkSpecCnt, PERK_COUNT);
    for (int i = 0; i < PERK_COUNT; i++) {
        for (int s = 0; s < PRIMARY_STAT_COUNT; s++) {
            char key[16] = {};
            sprintf(key, "SFS%02d%03d", s, i);
            sfall_gl_vars_store(key, sfallPerkSpecialOverrides[i][s]);
        }
    }

    // Kill counters — persisted as indexed key/value pairs.
    // Format: kGlVarKillCtrCnt stores entry count, "SFKCkNNN" stores key (critter type),
    // "SFKCvNNN" stores value (count) for entry index NNN.
    int kcCount = static_cast<int>(gSfallKillCounters.size());
    sfall_gl_vars_store(kGlVarKillCtrCnt, kcCount);
    int idx = 0;
    for (const auto& entry : gSfallKillCounters) {
        char key[16] = {};
        sprintf(key, "SFKCk%03d", idx);
        sfall_gl_vars_store(key, entry.first);
        sprintf(key, "SFKCv%03d", idx);
        sfall_gl_vars_store(key, entry.second);
        idx++;
    }

    // Per-critter hit chance overrides — serialized as indexed key/value pairs.
    // Format: kGlVarHitChCtrCnt stores entry count, "SFHCkNNN" stores key
    // (Object::id), "SFHCmNNN" stores mod, "SFHCxNNN" stores max for entry NNN.
    //
    // F-388: Object::id is a runtime counter from scriptsNewObjectId() and is
    // unstable across save/load — restored entries WILL NOT match post-load
    // object IDs. The data is persisted ONLY so that scripts which re-establish
    // overrides via set_critter_hit_chance_mod after load can verify they were
    // correctly applied.  Scripts MUST NOT rely on auto-restoration of
    // per-critter hit-chance overrides across save/load cycles; they must
    // re-apply them in a global script's "load" handler via hs_glob.int.
    // See matching comment in sfallOpcodeStateLoad().
    //
    // WARNING: These keys are NOT stable.  If a critter is destroyed and
    // re-created, its Object::id changes.  Use the critter's PID (proto ID)
    // or a string key derived from script-owned stable identifiers for
    // persistent per-critter state.
    int hcCount = static_cast<int>(gCritterHitChanceOverrides.size());
    sfall_gl_vars_store(kGlVarHitChCtrCnt, hcCount);
    idx = 0;
    for (const auto& entry : gCritterHitChanceOverrides) {
        char key[16] = {};
        sprintf(key, "SFHCk%03d", idx);
        sfall_gl_vars_store(key, entry.first);           // critter Object::id (UNSTABLE — see F-388 above)
        sprintf(key, "SFHCm%03d", idx);
        sfall_gl_vars_store(key, entry.second.mod);      // mod value
        sprintf(key, "SFHCx%03d", idx);
        sfall_gl_vars_store(key, entry.second.max);      // max value
        idx++;
    }

    // F-001: Per-critter pickpocket mod map.
    int cpmCount = static_cast<int>(gCritterPickpocketModMap.size());
    sfall_gl_vars_store(kGlVarCrtPMapCnt, cpmCount);
    idx = 0;
    for (const auto& entry : gCritterPickpocketModMap) {
        char key[16] = {};
        sprintf(key, "SFCPMk%03d", idx);
        sfall_gl_vars_store(key, entry.first);           // critter PID
        sprintf(key, "SFCPMv%03d", idx);
        sfall_gl_vars_store(key, entry.second.mod);      // mod value
        sprintf(key, "SFCPMx%03d", idx);
        sfall_gl_vars_store(key, entry.second.max);      // max value
        idx++;
    }

    // F-062 (I2-088): Serialize fake perks and traits. Int fields (level,
    // image, active) are stored directly. name/desc strings are persisted
    // separately via sfall_gl_vars_store_string() in the string section
    // below (F-003/F-004 fix).
    // Format: count per type, then indexed {level, image, active}.
    sfall_gl_vars_store(kGlVarFakePkCnt, sfallFakePerkCount);
    for (int i = 0; i < sfallFakePerkCount && i < kMaxFakePerks; i++) {
        char key[16] = {};
        sprintf(key, "SFFPL%03d", i);
        sfall_gl_vars_store(key, sfallFakePerks[i].level);
        sprintf(key, "SFFPI%03d", i);
        sfall_gl_vars_store(key, sfallFakePerks[i].image);
        sprintf(key, "SFFPA%03d", i);
        sfall_gl_vars_store(key, sfallFakePerks[i].active ? 1 : 0);
    }
    sfall_gl_vars_store(kGlVarFakeTrCnt, sfallFakeTraitCount);
    for (int i = 0; i < sfallFakeTraitCount && i < kMaxFakeTraits; i++) {
        char key[16] = {};
        sprintf(key, "SFFTA%03d", i);
        sfall_gl_vars_store(key, sfallFakeTraits[i].active);
        sprintf(key, "SFFTI%03d", i);
        sfall_gl_vars_store(key, sfallFakeTraits[i].image);
    }

    // Custom death model globals — set via set_dm_model / set_df_model (0x81FF/0x8200).
    // These are simple int values used in op_refresh_pc_art to override the hero proto FID.
    sfall_gl_vars_store(kGlVarDMaleModel, gCustomMaleHeroModelNum);
    sfall_gl_vars_store(kGlVarDFemaleModel, gCustomFemaleHeroModelNum);

    // Per-critter aimed-shot override maps (F-016).
    // Format: entry count + indexed key/value pairs (prefix "SFFAS" / "SFDAS").
    int fasCount = static_cast<int>(gForceAimedShotsMap.size());
    sfall_gl_vars_store(kGlVarFASMapCnt, fasCount);
    idx = 0;
    for (const auto& entry : gForceAimedShotsMap) {
        char key[16] = {};
        sprintf(key, "SFFASk%03d", idx);
        sfall_gl_vars_store(key, entry.first);
        sprintf(key, "SFFASv%03d", idx);
        sfall_gl_vars_store(key, entry.second ? 1 : 0);
        idx++;
    }

    // Disable aimed shots map — same format with "SFDAS" prefix.
    int dasCount = static_cast<int>(gDisableAimedShotsMap.size());
    sfall_gl_vars_store(kGlVarDASMapCnt, dasCount);
    idx = 0;
    for (const auto& entry : gDisableAimedShotsMap) {
        char key[16] = {};
        sprintf(key, "SFDASk%03d", idx);
        sfall_gl_vars_store(key, entry.first);
        sprintf(key, "SFDASv%03d", idx);
        sfall_gl_vars_store(key, entry.second ? 1 : 0);
        idx++;
    }

    // UH-14: Per-critter last target/attacker maps are NOT serialized.
    // Both the key (critter Object::id) AND the value (target/attacker
    // Object::id) are runtime counters from scriptsNewObjectId(), unstable
    // across save/load. After reload, the saved keys never match post-load
    // object IDs, and even if they did, the stored target/attacker IDs
    // would be dangling references. Scripts must re-populate these maps
    // after loading a save.

    // F-003/F-004/I2F-001/I2F-002: Persist string state using the string
    // global vars API (added in sfall_gl_vars version 2). Strings are stored
    // as length-prefixed UTF-8 records after the float section in sfallgv.sav.
    // Each string array uses 8-char keys encoding the array index.

    // Clear all string keys before repopulating (OP-2 fix).
    // Without this, stale entries from prior saves survive when an
    // override is set → saved → cleared → saved again without reset.
    // This mirrors the int/float pattern where every key is always
    // unconditionally overwritten.
    char clearKey[9] = {};
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        sprintf(clearKey, "SFpN%03d", i);
        sfall_gl_vars_remove_string(clearKey);
        sprintf(clearKey, "SFpD%03d", i);
        sfall_gl_vars_remove_string(clearKey);
    }
    for (int i = 0; i < kMaxFakePerks; i++) {
        sprintf(clearKey, "SFFPn%02d", i);
        sfall_gl_vars_remove_string(clearKey);
        sprintf(clearKey, "SFFPd%02d", i);
        sfall_gl_vars_remove_string(clearKey);
    }
    for (int i = 0; i < kMaxFakeTraits; i++) {
        sprintf(clearKey, "SFFTn%02d", i);
        sfall_gl_vars_remove_string(clearKey);
        sprintf(clearKey, "SFFTd%02d", i);
        sfall_gl_vars_remove_string(clearKey);
    }
    sfall_gl_vars_remove_string("SFPTitle");
    for (int i = 0; i < kMaxMoviePathOverrides; i++) {
        sprintf(clearKey, "SFMVp%02d", i);
        sfall_gl_vars_remove_string(clearKey);
    }

    // Perk name/desc overrides (F-003).
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        if (sfallPerkNameOverrides[i] != nullptr) {
            char key[9] = {};
            sprintf(key, "SFpN%03d", i);
            sfall_gl_vars_store_string(key, sfallPerkNameOverrides[i]);
        }
        if (sfallPerkDescOverrides[i] != nullptr) {
            char key[9] = {};
            sprintf(key, "SFpD%03d", i);
            sfall_gl_vars_store_string(key, sfallPerkDescOverrides[i]);
        }
    }

    // Fake perk name/desc strings (F-004).
    for (int i = 0; i < sfallFakePerkCount && i < kMaxFakePerks; i++) {
        if (sfallFakePerks[i].name != nullptr) {
            char key[9] = {};
            sprintf(key, "SFFPn%02d", i);
            sfall_gl_vars_store_string(key, sfallFakePerks[i].name);
        }
        if (sfallFakePerks[i].desc != nullptr) {
            char key[9] = {};
            sprintf(key, "SFFPd%02d", i);
            sfall_gl_vars_store_string(key, sfallFakePerks[i].desc);
        }
    }

    // Fake trait name/desc strings (F-004).
    for (int i = 0; i < sfallFakeTraitCount && i < kMaxFakeTraits; i++) {
        if (sfallFakeTraits[i].name != nullptr) {
            char key[9] = {};
            sprintf(key, "SFFTn%02d", i);
            sfall_gl_vars_store_string(key, sfallFakeTraits[i].name);
        }
        if (sfallFakeTraits[i].desc != nullptr) {
            char key[9] = {};
            sprintf(key, "SFFTd%02d", i);
            sfall_gl_vars_store_string(key, sfallFakeTraits[i].desc);
        }
    }

    // Perkbox title (I2F-001).
    if (sfallPerkboxTitle != nullptr) {
        sfall_gl_vars_store_string("SFPTitle", sfallPerkboxTitle);
    }

    // Movie path overrides (I2F-002).
    for (int i = 0; i < kMaxMoviePathOverrides; i++) {
        if (sfallMoviePathOverrides[i] != nullptr) {
            char key[9] = {};
            sprintf(key, "SFMVp%02d", i);
            sfall_gl_vars_store_string(key, sfallMoviePathOverrides[i]);
        }
    }
}

// Restore opcode globals from the sfall global vars map after
// sfall_gl_vars_load() has read sfallgv.sav.
void sfallOpcodeStateLoad()
{
    int val;
    // Core opcode globals.
    // F-003: Apply the same range validation on load that the setter opcodes
    // apply. Prevents crafted saves from injecting out-of-range values that
    // would bypass the setters' clamping.
    if (sfall_gl_vars_fetch(kGlVarXpMod, val)) {
        if (val < 0) val = 0;
        if (val > kMaxXpModPercentage) val = kMaxXpModPercentage;
        gXpModPercentage = val;
    }
    if (sfall_gl_vars_fetch(kGlVarSkillMax, val)) {
        if (val < 0) val = 300;
        if (val > 999) val = 999;
        gSkillMaxCap = val;
    }
    if (sfall_gl_vars_fetch(kGlVarPerkFreq, val)) {
        if (val < 0) val = 0;
        if (val > 50) val = 50;
        gPerkFrequencyOverride = val;
    }
    if (sfall_gl_vars_fetch(kGlVarSkillPts, val)) {
        // M-172: the range is [-100, 100] — preserve negative values (the old
        // `val < 0 → 0` clamp discarded legitimate negative mods on load).
        if (val < -100) val = -100;
        if (val > 100) val = 100;
        gSkillPointsPerLevelMod = val;
    }

    // Hit chance globals.
    if (sfall_gl_vars_fetch(kGlVarHitChMod, val)) {
        // Hit chance modifier — clamp to reasonable [-100, 100] range.
        if (val < -100) val = -100;
        if (val > 100) val = 100;
        sfallHitChanceMod = val;
    }
    if (sfall_gl_vars_fetch(kGlVarHitChMax, val)) {
        if (val < 1) val = 1;
        if (val > 100) val = 100;
        sfallHitChanceMax = val;
    }

    // Knockback globals.
    if (sfall_gl_vars_fetch(kGlVarWpnKnTyp, val)) {
        // Knockback type enum: values outside [0, 3] are undefined.
        if (val < 0) val = 0;
        sfallWeaponKnockbackType = val;
    }
    {
        float fval;
        if (sfall_gl_vars_fetch_float(kGlVarWpnKnVal, fval)) {
            if (std::isnan(fval) || std::isinf(fval)) fval = 0.0f;
            sfallWeaponKnockbackValue = fval;
        }
    }
    if (sfall_gl_vars_fetch(kGlVarTgtKnTyp, val)) {
        if (val < 0) val = 0;
        sfallTargetKnockbackType = val;
    }
    {
        float fval;
        if (sfall_gl_vars_fetch_float(kGlVarTgtKnVal, fval)) {
            if (std::isnan(fval) || std::isinf(fval)) fval = 0.0f;
            sfallTargetKnockbackValue = fval;
        }
    }
    if (sfall_gl_vars_fetch(kGlVarAtkKnTyp, val)) {
        if (val < 0) val = 0;
        sfallAttackerKnockbackType = val;
    }
    {
        float fval;
        if (sfall_gl_vars_fetch_float(kGlVarAtkKnVal, fval)) {
            if (std::isnan(fval) || std::isinf(fval)) fval = 0.0f;
            sfallAttackerKnockbackValue = fval;
        }
    }

    // Pipboy availability override — setter stores 0 or 1; default is -1.
    // Clamp to [-1, 1] on load.
    if (sfall_gl_vars_fetch(kGlVarPipboy, val)) {
        if (val < -1) val = -1;
        if (val > 1) val = 1;
        gPipboyAvailableOverride = val;
    }

    // UM-17: Restore sfall animation callback state.
    // The Program* cannot be restored (old program is dead after load);
    // only the procedure index is preserved as a hint. The script must
    // call reg_anim_callback again to re-establish the callback.
    bool animCbWasActive = false;
    if (sfall_gl_vars_fetch(kGlVarAnimCbAct, val)) {
        animCbWasActive = (val != 0);
    }
    if (sfall_gl_vars_fetch(kGlVarAnimCbProc, val)) {
        sfallAnimCallbackProcedureIndex = val;
    }
    if (animCbWasActive && sfallAnimCallbackProcedureIndex >= 0) {
        // Keep the procedure index but leave program nullptr —
        // it must be re-registered after load.
        sfallAnimCallbackProgram = nullptr;
        debugPrint("sfallOpcodeStateLoad: anim callback was active (proc=%d); re-register after load\n",
                   sfallAnimCallbackProcedureIndex);
    }

    // Perk/trait modifier opcodes.
    if (sfall_gl_vars_fetch(kGlVarPerkAddMode, val)) {
        sfallPerkAddMode = val;
    }
    if (sfall_gl_vars_fetch(kGlVarPerkClrSel, val)) {
        sfallClearSelectablePerks = (val != 0);
    }
    if (sfall_gl_vars_fetch(kGlVarPyroMod, val)) {
        if (val < -100) val = -100;
        if (val > 100) val = 100;
        sfallPyromaniacMod = val;
    }
    if (sfall_gl_vars_fetch(kGlVarSwiftMod, val)) {
        if (val < -100) val = -100;
        if (val > 100) val = 100;
        sfallSwiftLearnerMod = val;
    }
    if (sfall_gl_vars_fetch(kGlVarHpPMod, val)) {
        if (val < -50) val = -50;
        if (val > 50) val = 50;
        sfallHpPerLevelMod = val;
    }

    // F-003/F-004: sfallPerkboxTitle is now restored via sfall_gl_vars_fetch_string()
    // (global vars version 2). See the string restoration section at the end of
    // this function.

    // F-002: Restore per-stat min/max bounds set via set_stat_max (0x81B4),
    // set_stat_min (0x81B5), and their PC/NPC variants.
    // F-003: Apply clampStatBoundaryValue() to loaded stat bounds so crafted
    // saves cannot inject out-of-range bounds that bypass the setters.
    // R-15 (M-21): the save format now persists the PC and NPC shadow tables
    // under their own keys (SFPcMn/SFPcMx/SFNpMn/SFNpMx). Restore each table
    // from its own key; older saves (no PC/NPC keys) fall back to the single
    // shared value (previous collapse behavior), preserving backward compat.
    {
        int statCount = 0;
        if (sfall_gl_vars_fetch(kGlVarStatCnt, statCount)) {
            for (int stat = 0; stat < statCount && stat < STAT_COUNT; stat++) {
                char key[16] = {};
                int ival = 0;
                int pcVal = 0;
                int npVal = 0;
                sprintf(key, "SFStMn%02d", stat);
                if (sfall_gl_vars_fetch(key, ival)) {
                    ival = clampStatBoundaryValue(stat, ival, "sfallOpcodeStateLoad");
                    statSetMinValue(stat, ival);
                }
                sprintf(key, "SFStMx%02d", stat);
                if (sfall_gl_vars_fetch(key, ival)) {
                    ival = clampStatBoundaryValue(stat, ival, "sfallOpcodeStateLoad");
                    statSetMaxValue(stat, ival);
                }
                // R-15: restore each shadow table from its own key; fall back
                // to the shared engine value when the key is absent (old saves).
                sprintf(key, "SFPcMn%02d", stat);
                if (sfall_gl_vars_fetch(key, pcVal)) {
                    sfallPcStatMins[stat] = clampStatBoundaryValue(stat, pcVal, "sfallOpcodeStateLoad");
                } else {
                    sfallPcStatMins[stat] = statGetMinValue(stat);
                }
                sprintf(key, "SFPcMx%02d", stat);
                if (sfall_gl_vars_fetch(key, pcVal)) {
                    sfallPcStatMaxs[stat] = clampStatBoundaryValue(stat, pcVal, "sfallOpcodeStateLoad");
                } else {
                    sfallPcStatMaxs[stat] = statGetMaxValue(stat);
                }
                sprintf(key, "SFNpMn%02d", stat);
                if (sfall_gl_vars_fetch(key, npVal)) {
                    sfallNpcStatMins[stat] = clampStatBoundaryValue(stat, npVal, "sfallOpcodeStateLoad");
                } else {
                    sfallNpcStatMins[stat] = statGetMinValue(stat);
                }
                sprintf(key, "SFNpMx%02d", stat);
                if (sfall_gl_vars_fetch(key, npVal)) {
                    sfallNpcStatMaxs[stat] = clampStatBoundaryValue(stat, npVal, "sfallOpcodeStateLoad");
                } else {
                    sfallNpcStatMaxs[stat] = statGetMaxValue(stat);
                }
            }
        }
    }

    // Pickpocket modifier opcodes.
    if (sfall_gl_vars_fetch(kGlVarPkpMax, val)) {
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        sfallPickpocketMax = val;
    }
    if (sfall_gl_vars_fetch(kGlVarCrtPMax, val)) {
        if (val < 1) val = 1;
        if (val > 100) val = 100;
        sfallCritterPickpocketMax = val;
    }
    if (sfall_gl_vars_fetch(kGlVarBasePMax, val)) {
        if (val < 1) val = 1;
        if (val > 100) val = 100;
        sfallBasePickpocketMax = val;
    }
    if (sfall_gl_vars_fetch(kGlVarCrtPMod, val)) {
        sfallCritterPickpocketMod = val;
    }
    if (sfall_gl_vars_fetch(kGlVarBasePMod, val)) {
        sfallBasePickpocketMod = val;
    }

    // Custom death model globals — set via set_dm_model / set_df_model.
    if (sfall_gl_vars_fetch(kGlVarDMaleModel, val)) {
        gCustomMaleHeroModelNum = val;
    }
    if (sfall_gl_vars_fetch(kGlVarDFemaleModel, val)) {
        gCustomFemaleHeroModelNum = val;
    }

    // Perk level modifier.
    if (sfall_gl_vars_fetch(kGlVarPerkLvlMod, val)) {
        if (val < -10) {
            debugPrint("set_perk_level_mod: saved value %d clamped to range [-10, 10]", val);
            val = -10;
        }
        if (val > 10) {
            debugPrint("set_perk_level_mod: saved value %d clamped to range [-10, 10]", val);
            val = 10;
        }
        sfallPerkLevelMod = val;
    }

    // F-015: Restore unspent AP bonuses set via set_unspent_ap_bonus /
    // set_unspent_ap_perk_bonus opcodes.
    if (sfall_gl_vars_fetch(kGlVarUnApBn, val)) {
        statSetUnspentApBonus(val);
    }
    if (sfall_gl_vars_fetch(kGlVarUnApPk, val)) {
        statSetUnspentApPerkBonus(val);
    }

    // F-59: Restore inventory AP cost and Quick Pockets reduction.
    if (sfall_gl_vars_fetch(kGlVarInvApCost, val)) {
        if (val < 0) val = 0;
        inventorySetInvenApCost(val);
    }
    if (sfall_gl_vars_fetch(kGlVarInvQPRed, val)) {
        if (val < 0) val = 0;
        inventorySetQuickPocketsApCostReduction(val);
    }

    // F-008: Restore perk min level overrides set by set_perk_level (0x817A).
    {
        int savedCount = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkMLCnt, savedCount) && savedCount <= kMaxPerkMinLevelOverrides) {
            for (int idx = 0; idx < savedCount; idx++) {
                char pkKey[16] = {};
                char pvKey[16] = {};
                sprintf(pkKey, "SFPk%03d", idx);
                sprintf(pvKey, "SFPv%03d", idx);
                int perkId = 0;
                int minLevel = 0;
                if (sfall_gl_vars_fetch(pkKey, perkId) && sfall_gl_vars_fetch(pvKey, minLevel)) {
                    if (perkIsValid(perkId)) {
                        perkSetMinLevel(perkId, minLevel);
                    }
                }
            }
        }
    }

    // Skill modifier globals (F-013: loaded per-skill).
    // gBaseSkillModMap: skill → mod for set_base_skill_mod.
    {
        int skCount = 0;
        if (sfall_gl_vars_fetch(kGlVarBaseSkCnt, skCount) && skCount <= kMaxSkillModEntries) {
            for (int idx = 0; idx < skCount; idx++) {
                char skKey[16] = {};
                char modKey[16] = {};
                sprintf(skKey, "SFBSMsk%02d", idx);
                sprintf(modKey, "SFBSMmv%02d", idx);
                int skill = 0;
                int mod = 0;
                if (sfall_gl_vars_fetch(skKey, skill) && sfall_gl_vars_fetch(modKey, mod)) {
                    if (skill >= 0 && skill < SKILL_COUNT) {
                        gBaseSkillModMap[skill] = mod;
                    }
                }
            }
        }
    }
    // gGlobalCritterSkillModMap: skill → mod for set_critter_skill_mod
    // with no critter (nullptr fallback).
    {
        int gcCount = 0;
        if (sfall_gl_vars_fetch(kGlVarGlobCt, gcCount) && gcCount <= kMaxSkillModEntries) {
            for (int idx = 0; idx < gcCount; idx++) {
                char skKey[16] = {};
                char modKey[16] = {};
                sprintf(skKey, "SFGCsk%02d", idx);
                sprintf(modKey, "SFGCmv%02d", idx);
                int skill = 0;
                int mod = 0;
                if (sfall_gl_vars_fetch(skKey, skill) && sfall_gl_vars_fetch(modKey, mod)) {
                    if (skill >= 0 && skill < SKILL_COUNT) {
                        gGlobalCritterSkillModMap[skill] = mod;
                    }
                }
            }
        }
    }
    // gCritterSkillModMap: pid → (skill → mod) for set_critter_skill_mod.
    {
        int crtCount = 0;
        if (sfall_gl_vars_fetch(kGlVarCrtSkCnt, crtCount) && crtCount <= kMaxCritterSkillPidEntries) {
            for (int idx = 0; idx < crtCount; idx++) {
                char pidKey[16] = {};
                char skCntKey[16] = {};
                sprintf(pidKey, "SFCrP%04d", idx);
                sprintf(skCntKey, "SFCrPn%04d", idx);
                int pid = 0;
                int skCount2 = 0;
                if (sfall_gl_vars_fetch(pidKey, pid) && sfall_gl_vars_fetch(skCntKey, skCount2) && skCount2 <= kMaxSkillModEntries) {
                    for (int skIdx = 0; skIdx < skCount2; skIdx++) {
                        char skKey[32] = {};
                        char modKey[32] = {};
                        sprintf(skKey, "SFCrSk%04d_%02d", idx, skIdx);
                        sprintf(modKey, "SFCrSv%04d_%02d", idx, skIdx);
                        int skill = 0;
                        int mod = 0;
                        if (sfall_gl_vars_fetch(skKey, skill) && sfall_gl_vars_fetch(modKey, mod)) {
                            if (skill >= 0 && skill < SKILL_COUNT) {
                                gCritterSkillModMap[pid][skill] = mod;
                            }
                        }
                    }
                }
            }
        }
    }
    // H-23: gCritterSkillMaxMap: pid → max (per-critter skill max cap).
    {
        int crtMaxCount = 0;
        if (sfall_gl_vars_fetch(kGlVarCrtSkMaxCnt, crtMaxCount) && crtMaxCount <= kMaxCritterSkillPidEntries) {
            for (int idx = 0; idx < crtMaxCount; idx++) {
                char pidKey[16] = {};
                char maxKey[16] = {};
                sprintf(pidKey, "SFCmP%04d", idx);
                sprintf(maxKey, "SFCmV%04d", idx);
                int pid = 0;
                int max = 0;
                if (sfall_gl_vars_fetch(pidKey, pid) && sfall_gl_vars_fetch(maxKey, max) && max >= 0) {
                    gCritterSkillMaxMap[pid] = max;
                }
            }
        }
    }

    // Backward compat: old saves (pre F-013) used single-int globals
    // "SFCrtSM " and "SFBaseSM" instead of per-skill entries.
    if (gBaseSkillModMap.empty()) {
        int oldBase = 0;
        if (sfall_gl_vars_fetch("SFBaseSM", oldBase) && oldBase != 0) {
            for (int sk = 0; sk < SKILL_COUNT; sk++) {
                gBaseSkillModMap[sk] = oldBase;
            }
        }
    }
    if (gCritterSkillModMap.empty()) {
        int oldCrt = 0;
        if (sfall_gl_vars_fetch("SFCrtSM ", oldCrt) && oldCrt != 0) {
            if (gDude != nullptr) {
                for (int sk = 0; sk < SKILL_COUNT; sk++) {
                    gCritterSkillModMap[gDude->pid][sk] = oldCrt;
                }
            }
        }
    }

    // Perk owed counter.
    if (sfall_gl_vars_fetch(kGlVarPerkOwed, val)) {
        // Clamp to [0, 255] on load, matching the setter opcode (op_set_perk_owed).
        // The setter clamps to prevent negative values and unreasonably large
        // perk owed counts. Load bypassing this clamp allows crafted saves
        // to permanently block perk granting with negative values.
        if (val < 0) {
            val = 0;
        } else if (val > 255) {
            val = 255;
        }
        gSfallPerkOwed = val;
    }

    // Hide real perks flag.
    if (sfall_gl_vars_fetch(kGlVarHideRP, val)) {
        sfallHideRealPerks = (val != 0);
    }

    // Perk property override arrays — restore from indexed key/value pairs.
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkImgCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFPi%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkImageOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkRankCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFPr%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkRanksOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkStatCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFPs%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkStatOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkMagCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFPm%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkStatMagOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkSk1Cnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFs1%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkSkill1Overrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerk1MagCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SF1M%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkSkill1MagOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkSk2Cnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFs2%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkSkill2Overrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerk2MagCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SF2M%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkSkill2MagOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkTypeCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                char key[16] = {};
                sprintf(key, "SFPt%03d", i);
                sfall_gl_vars_fetch(key, sfallPerkTypeOverrides[i]);
            }
        }
    }
    {
        int count = 0;
        if (sfall_gl_vars_fetch(kGlVarPerkSpecCnt, count) && count == PERK_COUNT) {
            for (int i = 0; i < count; i++) {
                for (int s = 0; s < PRIMARY_STAT_COUNT; s++) {
                    char key[16] = {};
                    sprintf(key, "SFS%02d%03d", s, i);
                    sfall_gl_vars_fetch(key, sfallPerkSpecialOverrides[i][s]);
                }
            }
        }
    }

    // Mark perk overrides as initialized so sfallInitPerkOverrideArrays()
    // does not re-initialize with sentinel values after load.
    sfallPerkOverridesInited = true;

    // Kill counters — restore from indexed key/value pairs.
    {
        int kcCount = 0;
        if (sfall_gl_vars_fetch(kGlVarKillCtrCnt, kcCount) && kcCount <= kMaxKillCounterEntries) {
            gSfallKillCounters.clear();
            for (int idx2 = 0; idx2 < kcCount; idx2++) {
                char key[16] = {};
                sprintf(key, "SFKCk%03d", idx2);
                int critterType = 0;
                if (sfall_gl_vars_fetch(key, critterType)) {
                    sprintf(key, "SFKCv%03d", idx2);
                    int count = 0;
                    sfall_gl_vars_fetch(key, count);
                    gSfallKillCounters[critterType] = count;
                }
            }
        }
    }

    // Per-critter hit chance overrides — restored as indexed key/value pairs.
    // NOTE: Object::id keys are unstable across save/load — restored entries
    // will not match post-load object IDs (see matching comment in
    // sfallOpcodeStateSave()). The data is persisted so scripts that
    // re-establish overrides via set_critter_hit_chance_mod after load can
    // verify they were correctly applied.
    {
        int hcCount = 0;
        if (sfall_gl_vars_fetch(kGlVarHitChCtrCnt, hcCount) && hcCount <= kMaxHitChanceOverrides) {
            gCritterHitChanceOverrides.clear();
            for (int idx2 = 0; idx2 < hcCount; idx2++) {
                char key[16] = {};
                sprintf(key, "SFHCk%03d", idx2);
                int critterId = 0;
                if (sfall_gl_vars_fetch(key, critterId)) {
                    CritterHitChanceEntry entry;
                    int ival = 0;
                    sprintf(key, "SFHCm%03d", idx2);
                    sfall_gl_vars_fetch(key, ival);
                    entry.mod = ival;
                    ival = 0;
                    sprintf(key, "SFHCx%03d", idx2);
                    sfall_gl_vars_fetch(key, ival);
                    entry.max = ival;
                    gCritterHitChanceOverrides[critterId] = entry;
                }
            }
        }
    }

    // F-001: Per-critter pickpocket mod map.
    {
        int cpmCount = 0;
        if (sfall_gl_vars_fetch(kGlVarCrtPMapCnt, cpmCount) && cpmCount <= kMaxPickpocketEntries) {
            gCritterPickpocketModMap.clear();
            for (int idx2 = 0; idx2 < cpmCount; idx2++) {
                char key[16] = {};
                sprintf(key, "SFCPMk%03d", idx2);
                int critterPid = 0;
                if (sfall_gl_vars_fetch(key, critterPid)) {
                    CritterPickpocketEntry entry;
                    int ival = 0;
                    sprintf(key, "SFCPMv%03d", idx2);
                    sfall_gl_vars_fetch(key, ival);
                    entry.mod = ival;
                    ival = 0;
                    sprintf(key, "SFCPMx%03d", idx2);
                    sfall_gl_vars_fetch(key, ival);
                    entry.max = ival;
                    gCritterPickpocketModMap[critterPid] = entry;
                }
            }
        }
    }

    // F-062 (I2-088): Restore fake perks and traits from serialized state.
    // name/desc strings ARE now restored via sfall_gl_vars_fetch_string()
    // (F-003/F-004 fix, global vars version 2). The string restoration
    // happens after all int/float fields are loaded (see string section below).
    {
        int fpCount = 0;
        if (sfall_gl_vars_fetch(kGlVarFakePkCnt, fpCount) && fpCount > 0 && fpCount <= kMaxFakePerks) {
            sfallFakePerkCount = fpCount;
            for (int i = 0; i < fpCount; i++) {
                char key[16] = {};
                int ival = 0;
                sprintf(key, "SFFPL%03d", i);
                if (sfall_gl_vars_fetch(key, ival)) {
                    sfallFakePerks[i].level = ival;
                }
                sprintf(key, "SFFPI%03d", i);
                if (sfall_gl_vars_fetch(key, ival)) {
                    sfallFakePerks[i].image = ival;
                }
                sprintf(key, "SFFPA%03d", i);
                if (sfall_gl_vars_fetch(key, ival)) {
                    sfallFakePerks[i].active = (ival != 0);
                }
            }
        }
        int ftCount = 0;
        if (sfall_gl_vars_fetch(kGlVarFakeTrCnt, ftCount) && ftCount > 0 && ftCount <= kMaxFakeTraits) {
            sfallFakeTraitCount = ftCount;
            for (int i = 0; i < ftCount; i++) {
                char key[16] = {};
                int ival = 0;
                sprintf(key, "SFFTA%03d", i);
                if (sfall_gl_vars_fetch(key, ival)) {
                    sfallFakeTraits[i].active = ival;
                }
                sprintf(key, "SFFTI%03d", i);
                if (sfall_gl_vars_fetch(key, ival)) {
                    sfallFakeTraits[i].image = ival;
                }
            }
        }
    }

    // Per-critter aimed-shot override maps (F-016).
    {
        int fasCount = 0;
        if (sfall_gl_vars_fetch(kGlVarFASMapCnt, fasCount) && fasCount <= kMaxAimedShotEntries) {
            gForceAimedShotsMap.clear();
            for (int idx2 = 0; idx2 < fasCount; idx2++) {
                char key[16] = {};
                sprintf(key, "SFFASk%03d", idx2);
                int pid = 0;
                if (sfall_gl_vars_fetch(key, pid)) {
                    sprintf(key, "SFFASv%03d", idx2);
                    int ival = 0;
                    sfall_gl_vars_fetch(key, ival);
                    gForceAimedShotsMap[pid] = (ival != 0);
                }
            }
        }
        int dasCount = 0;
        if (sfall_gl_vars_fetch(kGlVarDASMapCnt, dasCount) && dasCount <= kMaxAimedShotEntries) {
            gDisableAimedShotsMap.clear();
            for (int idx2 = 0; idx2 < dasCount; idx2++) {
                char key[16] = {};
                sprintf(key, "SFDASk%03d", idx2);
                int pid = 0;
                if (sfall_gl_vars_fetch(key, pid)) {
                    sprintf(key, "SFDASv%03d", idx2);
                    int ival = 0;
                    sfall_gl_vars_fetch(key, ival);
                    gDisableAimedShotsMap[pid] = (ival != 0);
                }
            }
        }
        // Upstream #701: seed the engine-level pid sets from the restored
        // fork maps so critterCanAim honors saved force/disable overrides.
        for (const auto& entry : gForceAimedShotsMap) {
            if (entry.second) {
                forceAimedShots(entry.first);
            }
        }
        for (const auto& entry : gDisableAimedShotsMap) {
            if (entry.second) {
                disableAimedShots(entry.first);
            }
        }
    }

    // UH-14: Per-critter last target/attacker maps are NOT restored on
    // load. Both key and value are Object::id (unstable across save/load).
    // See UH-14 comment in sfallOpcodeStateSave for details.

    // F-003/F-004/I2F-001/I2F-002: Restore string state from the string
    // global vars (added in sfall_gl_vars version 2). String data was
    // serialized after all int/float globals. sfall_gl_vars_fetch_string()
    // returns a newly-allocated copy that we take ownership of.

    // Perk name/desc overrides (F-003).
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        char key[9] = {};
        sprintf(key, "SFpN%03d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallPerkNameOverrides[i];
            sfallPerkNameOverrides[i] = str;
        }
    }
    for (int i = 0; i < kMaxPerkNameOverrides; i++) {
        char key[9] = {};
        sprintf(key, "SFpD%03d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallPerkDescOverrides[i];
            sfallPerkDescOverrides[i] = str;
        }
    }

    // Fake perk name/desc strings (F-004).
    for (int i = 0; i < sfallFakePerkCount && i < kMaxFakePerks; i++) {
        char key[9] = {};
        sprintf(key, "SFFPn%02d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallFakePerks[i].name;
            sfallFakePerks[i].name = str;
        }
    }
    for (int i = 0; i < sfallFakePerkCount && i < kMaxFakePerks; i++) {
        char key[9] = {};
        sprintf(key, "SFFPd%02d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallFakePerks[i].desc;
            sfallFakePerks[i].desc = str;
        }
    }

    // Fake trait name/desc strings (F-004).
    for (int i = 0; i < sfallFakeTraitCount && i < kMaxFakeTraits; i++) {
        char key[9] = {};
        sprintf(key, "SFFTn%02d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallFakeTraits[i].name;
            sfallFakeTraits[i].name = str;
        }
    }
    for (int i = 0; i < sfallFakeTraitCount && i < kMaxFakeTraits; i++) {
        char key[9] = {};
        sprintf(key, "SFFTd%02d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallFakeTraits[i].desc;
            sfallFakeTraits[i].desc = str;
        }
    }

    // Perkbox title (I2F-001).
    {
        char* str = sfall_gl_vars_fetch_string("SFPTitle");
        if (str != nullptr) {
            delete[] sfallPerkboxTitle;
            sfallPerkboxTitle = str;
        }
    }

    // Movie path overrides (I2F-002).
    for (int i = 0; i < kMaxMoviePathOverrides; i++) {
        char key[9] = {};
        sprintf(key, "SFMVp%02d", i);
        char* str = sfall_gl_vars_fetch_string(key);
        if (str != nullptr) {
            delete[] sfallMoviePathOverrides[i];
            sfallMoviePathOverrides[i] = str;
        }
    }
}

// ============================================================
// Getter functions for storage-only opcode globals (F-007/F-030/F-033/F-034/F-035/F-036/F-037).
// These provide access to file-scope static variables for engine
// integration by other translation units (stat.cc, combat.cc, skill.cc,
// character_editor.cc, etc.).
// ============================================================

// F-007: HP per level modifier (0x81CE).
int sfallGetHpPerLevelMod()
{
    return sfallHpPerLevelMod;
}

// F-030: Pyromaniac damage modifier (0x81CB).
int sfallGetPyromaniacMod()
{
    return sfallPyromaniacMod;
}

// F-033: Swift Learner XP modifier (0x81CD).
int sfallGetSwiftLearnerMod()
{
    return sfallSwiftLearnerMod;
}

// F-034: Skill modifier globals (0x81C7/0x81C8).
int sfallGetCritterSkillMod(int skill)
{
    auto it = gGlobalCritterSkillModMap.find(skill);
    if (it != gGlobalCritterSkillModMap.end()) {
        return it->second;
    }
    return 0;
}

int sfallGetBaseSkillMod(int skill)
{
    auto it = gBaseSkillModMap.find(skill);
    if (it != gBaseSkillModMap.end()) {
        return it->second;
    }
    return 0;
}

// F-001: Per-critter skill mod accessor.
// Returns the per-(pid,skill) modifier if one was set via
// set_critter_skill_mod for this specific critter + skill pair.
// Returns kNoSkillModOverride (INT_MIN) if no per-critter override
// exists. The caller should then check sfallGetCritterSkillMod(skill)
// for the per-skill fallback.
// F-042: INT_MIN sentinel allows explicitly-set modifier=0 to override
// a non-zero global modifier. Previously 0 meant both "no override" and
// "override with value 0" — indistinguishable.
int sfallGetCritterSkillModForCritter(Object* critter, int skill)
{
    if (critter == nullptr) {
        return kNoSkillModOverride;
    }
    auto pidIt = gCritterSkillModMap.find(critter->pid);
    if (pidIt != gCritterSkillModMap.end()) {
        auto skillIt = pidIt->second.find(skill);
        if (skillIt != pidIt->second.end()) {
            return skillIt->second;
        }
    }
    return kNoSkillModOverride;
}

// F-035: Perk level modifier (0x81AB).
int sfallGetPerkLevelMod()
{
    return sfallPerkLevelMod;
}

// F-036: Perk add mode and clear-selectable-perks flag (0x81C3/0x81C4).
int sfallGetPerkAddMode()
{
    return sfallPerkAddMode;
}

bool sfallGetClearSelectablePerks()
{
    return sfallClearSelectablePerks;
}

// F-036: Perk owed counter (get/set_perk_owed 0x81AC).
int sfallGetPerkOwed()
{
    return gSfallPerkOwed;
}

void sfallSetPerkOwed(int value)
{
    gSfallPerkOwed = value;
}

// F2-041: Perk name/description overrides from set_perk_name (0x8189) /
// set_perk_desc (0x818A). Returns nullptr if no override set.
const char* sfallGetPerkNameOverride(int perkID)
{
    if (perkID >= 0 && perkID < kMaxPerkNameOverrides && sfallPerkNameOverrides[perkID] != nullptr) {
        return sfallPerkNameOverrides[perkID];
    }
    return nullptr;
}

const char* sfallGetPerkDescOverride(int perkID)
{
    if (perkID >= 0 && perkID < kMaxPerkNameOverrides && sfallPerkDescOverrides[perkID] != nullptr) {
        return sfallPerkDescOverrides[perkID];
    }
    return nullptr;
}

// F-037: Fake perk/trait arrays.
const FakePerkEntry* sfallGetFakePerks(int* outCount)
{
    if (outCount != nullptr) {
        *outCount = sfallFakePerkCount;
    }
    return sfallFakePerks;
}

int sfallGetFakePerkCount()
{
    return sfallFakePerkCount;
}

const FakeTraitEntry* sfallGetFakeTraits(int* outCount)
{
    if (outCount != nullptr) {
        *outCount = sfallFakeTraitCount;
    }
    return sfallFakeTraits;
}

// F-08: Hide real perks flag (0x81BF).
int sfallGetHideRealPerks()
{
    return sfallHideRealPerks;
}

// ============================================================
// Stub opcodes (F-034): opcodes expected by sfall mods that CE does not
// fully implement. Registered as safe no-ops/stubs so scripts do not
// crash with "Undefined opcode" fatal errors. Each stub pops its
// expected arguments and returns a safe default value.
// ============================================================

// Movie path override storage (F-021). Set via set_movie_path (0x8177).
// Keyed by movie ID. Integrated with movie.cc via sfallGetMoviePathOverride()
// (called at game_movie.cc:150-157 before resolving movie file paths).
// Note: movie path overrides ARE now serialized to save files via
// sfall_gl_vars_store_string() (F-003/F-004 fix, global vars version 2).
// Overrides survive save/load round-trips without script intervention.
// (Declarations for kMaxMoviePathOverrides and sfallMoviePathOverrides[]
// are at file scope before sfallOpcodesReset.)

const char* sfallGetMoviePathOverride(int movieId)
{
    if (movieId >= 0 && movieId < kMaxMoviePathOverrides) {
        return sfallMoviePathOverrides[movieId];
    }
    return nullptr;
}

// set_movie_path(string name, int movieid) — 0x8177
// Replaces a movie file path for the given movie ID.
// The override is consumed by sfallGetMoviePathOverride() at game_movie.cc:150-157.
// Note: overrides ARE now persisted across save/load via
// sfall_gl_vars_store_string() (F-003/F-004 fix, global vars version 2).
static void op_set_movie_path(Program* program)
{
    int movieid = programStackPopInteger(program);
    const char* name = programStackPopString(program);

    if (movieid < 0 || movieid >= kMaxMoviePathOverrides) {
        programPrintError("set_movie_path: movieid %d out of range (max %d)", movieid, kMaxMoviePathOverrides - 1);
        return;
    }

    // Free previous override for this movie ID.
    delete[] sfallMoviePathOverrides[movieid];
    sfallMoviePathOverrides[movieid] = nullptr;

    if (name != nullptr && name[0] != '\0') {
        size_t len = strlen(name) + 1;
        sfallMoviePathOverrides[movieid] = new char[len];
        memcpy(sfallMoviePathOverrides[movieid], name, len);
    }
    debugPrint("set_movie_path(movieid=%d, name=\"%s\") - override stored, consumed by game_movie.cc:150-157",
        movieid, name != nullptr ? name : "(null)");
}

// F2-008: Shader/graphics opcode stubs — previously unregistered (causing
// programFatalError → script termination). Registered as safe no-ops with
// debug logging so scripts compiled against original sfall graphics APIs
// degrade gracefully instead of crashing.
//
// CE uses SDL2 rendering without custom shaders; all graphics ops return
// defaults (0/false) and log warnings via debugPrint.
//
// graphics_funcs_available() -> int — 0x8165
// Returns 0 (false) — no hardware shader functions available in CE.
static void op_graphics_funcs_available(Program* program)
{
    debugPrint("op_graphics_funcs_available: shaders not supported in CE SDL2 renderer — returning 0");
    programStackPushInteger(program, 0);
}

// load_shader(string path) -> int — 0x8166
// Returns 0 (invalid shader ID) — shader loading not supported.
static void op_load_shader(Program* program)
{
    const char* path = programStackPopString(program);
    debugPrint("op_load_shader(\"%s\"): shaders not supported in CE — returning 0", path != nullptr ? path : "(null)");
    programStackPushInteger(program, 0);
}

// free_shader(int ID) — 0x8167
static void op_free_shader(Program* program)
{
    int id = programStackPopInteger(program);
    debugPrint("op_free_shader(%d): shaders not supported in CE — no-op", id);
}

// activate_shader(int ID) — 0x8168
static void op_activate_shader(Program* program)
{
    int id = programStackPopInteger(program);
    debugPrint("op_activate_shader(%d): shaders not supported in CE — no-op", id);
}

// deactivate_shader(int ID) — 0x8169
static void op_deactivate_shader(Program* program)
{
    int id = programStackPopInteger(program);
    debugPrint("op_deactivate_shader(%d): shaders not supported in CE — no-op", id);
}

// set_shader_int(int ID, string param, int value) — 0x816D
static void op_set_shader_int(Program* program)
{
    int value = programStackPopInteger(program);
    const char* param = programStackPopString(program);
    int id = programStackPopInteger(program);
    debugPrint("op_set_shader_int(%d, \"%s\", %d): shaders not supported in CE — no-op",
        id, param != nullptr ? param : "(null)", value);
}

// set_shader_float(int ID, string param, float value) — 0x816E
static void op_set_shader_float(Program* program)
{
    float value = programStackPopValue(program).asFloat();
    const char* param = programStackPopString(program);
    int id = programStackPopInteger(program);
    debugPrint("op_set_shader_float(%d, \"%s\", %f): shaders not supported in CE — no-op",
        id, param != nullptr ? param : "(null)", static_cast<double>(value));
}

// set_shader_vector(int ID, string param, float f1, float f2, float f3, float f4) — 0x816F
static void op_set_shader_vector(Program* program)
{
    float f4 = programStackPopValue(program).asFloat();
    float f3 = programStackPopValue(program).asFloat();
    float f2 = programStackPopValue(program).asFloat();
    float f1 = programStackPopValue(program).asFloat();
    const char* param = programStackPopString(program);
    int id = programStackPopInteger(program);
    debugPrint("op_set_shader_vector(%d, \"%s\", %f, %f, %f, %f): shaders not supported in CE — no-op",
        id, param != nullptr ? param : "(null)",
        static_cast<double>(f1), static_cast<double>(f2),
        static_cast<double>(f3), static_cast<double>(f4));
}

// force_graphics_refresh(bool enabled) — 0x81B0
static void op_force_graphics_refresh(Program* program)
{
    int enabled = programStackPopInteger(program);
    debugPrint("op_force_graphics_refresh(%d): not applicable in CE SDL2 renderer — no-op", enabled);
}

// get_shader_texture(int ID, int texture) -> int — 0x81B1
// Returns 0 — no shader textures available.
static void op_get_shader_texture(Program* program)
{
    int texture = programStackPopInteger(program);
    int id = programStackPopInteger(program);
    debugPrint("op_get_shader_texture(%d, %d): shaders not supported in CE — returning 0", id, texture);
    programStackPushInteger(program, 0);
}

// set_shader_texture(int ID, string param, int texID) — 0x81B2
static void op_set_shader_texture(Program* program)
{
    int texId = programStackPopInteger(program);
    const char* param = programStackPopString(program);
    int id = programStackPopInteger(program);
    debugPrint("op_set_shader_texture(%d, \"%s\", %d): shaders not supported in CE — no-op",
        id, param != nullptr ? param : "(null)", texId);
}

// get_shader_version() -> int — 0x81AD
// Returns the shader version supported by the graphics driver.
// CE uses SDL2 rendering without custom shaders; returns 0 to signal
// "no shader support" so scripts can fall back to non-shader paths.
static void op_get_shader_version(Program* program)
{
    debugPrint("op_get_shader_version: shaders not supported in CE SDL2 renderer — returning 0");
    programStackPushInteger(program, 0);
}

// set_shader_mode(int mode) — 0x81AE
// Sets the shader pipeline mode (0=none, 1=DX9, 2=GLSL).
// CE uses SDL2 rendering; shader modes are not applicable.
static void op_set_shader_mode(Program* program)
{
    int mode = programStackPopInteger(program);
    debugPrint("op_set_shader_mode(%d): shaders not supported in CE SDL2 renderer — no-op", mode);
    (void)mode;
}

// stop_game() — 0x8222
// Pauses the game clock and input processing.
// CE equivalent is not directly exposed to scripts; stub is a safe no-op.
static void op_stop_game(Program* program)
{
    (void)program;
    debugPrint("stop_game: not implemented in CE (SDL2 port)");
}

// resume_game() — 0x8223
// Resumes the game clock and input processing.
// CE equivalent is not directly exposed to scripts; stub is a safe no-op.
static void op_resume_game(Program* program)
{
    (void)program;
    debugPrint("resume_game: not implemented in CE (SDL2 port)");
}

// ============================================================
// Viewport opcode stubs — 0x81A6-0x81A9
// CE uses SDL2 hardware rendering with scroll managed by the engine;
// viewport override is not currently supported. Registered as safe
// stubs for script compatibility with RPU scroll/camera scripts.
// get_viewport_* return 0 (default viewport origin).
// ============================================================
static void op_get_viewport_x(Program* program)
{
    debugPrint("get_viewport_x: returns 0 (SDL2 native viewport, not implemented)");
    programStackPushInteger(program, 0);
}

static void op_get_viewport_y(Program* program)
{
    debugPrint("get_viewport_y: returns 0 (SDL2 native viewport, not implemented)");
    programStackPushInteger(program, 0);
}

static void op_set_viewport_x(Program* program)
{
    int viewX = programStackPopInteger(program);
    debugPrint("set_viewport_x: argument discarded (SDL2 native viewport, not implemented)");
    (void)viewX;
}

static void op_set_viewport_y(Program* program)
{
    int viewY = programStackPopInteger(program);
    debugPrint("set_viewport_y: argument discarded (SDL2 native viewport, not implemented)");
    (void)viewY;
}

// ============================================================
// set_palette(string path) — 0x81F2
// Sets a custom palette file for rendering. CE uses SDL2 hardware
// rendering; palette override is not currently supported.
// Registered as a safe no-op for script compatibility.
// ============================================================
static void op_set_palette(Program* program)
{
    const char* path = programStackPopString(program);
    debugPrint("set_palette: palette changes not supported in SDL2 hardware rendering (path=\"%s\")",
        path != nullptr ? path : "(null)");
    (void)path;
}

// ============================================================
// mark_movie_played(int id) — 0x8240
// Marks a movie as played so it won't play again.
//
// UH-60 FIX: gGameMoviesSeen[] is file-static in game_movie.cc. The mark is
// applied through gameMovieMarkSeen() (68ff38e) which sets the in-memory
// seen bit (persisted by gameMoviesSave), AND mirrored into sfall_gl_vars
// under the "SFMvSeen%03d" key pattern so scripts can still query the mark
// via get_sfall_global_int after loading (the global vars survive save/load
// in sfallgv.sav).
// ============================================================
static void op_mark_movie_played(Program* program)
{
    int movieId = programStackPopInteger(program);
    if (movieId < 0 || movieId >= MOVIE_COUNT) {
        programPrintError("mark_movie_played: movie ID %d out of range [0, %d)", movieId, MOVIE_COUNT);
        return;
    }
    gameMovieMarkSeen(movieId);
    char key[16] = {};
    sprintf(key, "SFMvSeen%03d", movieId);
    sfall_gl_vars_store(key, 1);
    debugPrint("mark_movie_played: movie %d marked as seen", movieId);
}

// ============================================================
// block_combat(int enable) — 0x824A
// Blocks or enables combat for all critters. Sets the global gBlockCombat
// flag which is checked at combat.cc:3482 and combat.cc:6058 to abort
// combat initiation and attacks when non-zero.
// ============================================================
static void op_block_combat(Program* program)
{
    int enable = programStackPopInteger(program);
    gBlockCombat = enable;
}

// Note: opcodes should pop arguments off the stack in reverse order
void sfallOpcodesInit()
{
    // F-040: Capture original stat min/max compile-time defaults before
    // any script can modify them. Restored in sfallOpcodesReset().
    if (!sfallStatBoundsCaptured) {
        for (int stat = 0; stat < STAT_COUNT; stat++) {
            sfallOriginalStatMins[stat] = statGetMinValue(stat);
            sfallOriginalStatMaxs[stat] = statGetMaxValue(stat);
        }
        sfallStatBoundsCaptured = true;
    }

    // M-21: initialize the PC/NPC shadow cap tables from the shared table
    // (sfall Stats.cpp:278: `statMaximumsPC[i] = statMaximumsNPC[i] = stat_data[i].maxValue`).
    for (int stat = 0; stat < STAT_COUNT; stat++) {
        sfallPcStatMaxs[stat] = statGetMaxValue(stat);
        sfallNpcStatMaxs[stat] = statGetMaxValue(stat);
        sfallPcStatMins[stat] = statGetMinValue(stat);
        sfallNpcStatMins[stat] = statGetMinValue(stat);
    }

    // ref. https://github.com/sfall-team/sfall/blob/71ecec3d405bd5e945f157954618b169e60068fe/artifacts/scripting/sfall%20opcode%20list.txt#L145
    // Note: we can't really implement these since address space is different.
    // We can potentially special case some of them, but we should try to avoid that.
    // 0x8156 - int   read_byte(int address)
    interpreterRegisterOpcode(0x8156, op_read_byte);
    // 0x8157 - int   read_short(int address)
    interpreterRegisterOpcode(0x8157, op_read_short);
    // 0x8158 - int   read_int(int address)
    interpreterRegisterOpcode(0x8158, op_read_int);
    // 0x8159 - int   read_string(int address) — registered as stub returning -1 (F-01)
    interpreterRegisterOpcode(0x8159, op_read_string);

    // VOODOO memory write opcodes — registered as safe no-ops.
    // These pop their arguments and log a debug message, preventing script
    // crashes on unregistered opcode errors.
    //
    // VOODOO write/call opcodes are always registered as no-ops.
    // AllowUnsafeScripting is parsed but intentionally unwired —
    // these opcodes cannot perform actual memory writes in CE.
    // Unconditional registration prevents script-terminating
    // programFatalError when scripts call VOODOO functions.
    {
        // 0x81cf - void  write_byte(int address, int value)
        interpreterRegisterOpcode(0x81CF, op_write_byte);
        // 0x81d0 - void  write_short(int address, int value)
        interpreterRegisterOpcode(0x81D0, op_write_short);
        // 0x81d1 - void  write_int(int address, int value)
        interpreterRegisterOpcode(0x81D1, op_write_int);
        // 0x821b - void  write_string(int address, string value)
        interpreterRegisterOpcode(0x821B, op_write_string);

        // VOODOO call_offset opcodes — registered as safe no-ops pushing 0.
        // CE cannot call arbitrary engine-internal functions at arbitrary
        // addresses. These push 0 so scripts that check return values can
        // skip the VOODOO patch and use CE-native alternatives.
        // 0x81d2 - void  call_offset_v0(int address)
        interpreterRegisterOpcode(0x81D2, op_call_offset_v0);
        // 0x81d3 - void  call_offset_v1(int address, int arg1)
        interpreterRegisterOpcode(0x81D3, op_call_offset_v1);
        // 0x81d4 - void  call_offset_v2(int address, int arg1, int arg2)
        interpreterRegisterOpcode(0x81D4, op_call_offset_v2);
        // 0x81d5 - void  call_offset_v3(int address, int arg1, int arg2, int arg3)
        interpreterRegisterOpcode(0x81D5, op_call_offset_v3);
        // 0x81d6 - void  call_offset_v4(int address, int arg1, int arg2, int arg3, int arg4)
        interpreterRegisterOpcode(0x81D6, op_call_offset_v4);
        // 0x81d7 - int   call_offset_r0(int address)
        interpreterRegisterOpcode(0x81D7, op_call_offset_r0);
        // 0x81d8 - int   call_offset_r1(int address, int arg1)
        interpreterRegisterOpcode(0x81D8, op_call_offset_r1);
        // 0x81d9 - int   call_offset_r2(int address, int arg1, int arg2)
        interpreterRegisterOpcode(0x81D9, op_call_offset_r2);
        // 0x81da - int   call_offset_r3(int address, int arg1, int arg2, int arg3)
        interpreterRegisterOpcode(0x81DA, op_call_offset_r3);
        // 0x81db - int   call_offset_r4(int address, int arg1, int arg2, int arg3, int arg4)
        interpreterRegisterOpcode(0x81DB, op_call_offset_r4);
    }

    // 0x815a - void set_pc_base_stat(Stat stat, int value)
    interpreterRegisterOpcode(0x815A, op_set_pc_base_stat);
    // 0x815b - void set_pc_extra_stat(Stat stat, int value)
    interpreterRegisterOpcode(0x815B, op_set_pc_bonus_stat);
    // 0x815c - int  get_pc_base_stat(Stat stat)
    interpreterRegisterOpcode(0x815C, op_get_pc_base_stat);
    // 0x815d - int  get_pc_extra_stat(Stat stat)
    interpreterRegisterOpcode(0x815D, op_get_pc_bonus_stat);

    // 0x815e - void set_critter_base_stat(object, Stat stat, int value)
    interpreterRegisterOpcode(0x815E, op_set_critter_base_stat);
    // 0x815f - void set_critter_extra_stat(object, Stat stat, int value)
    interpreterRegisterOpcode(0x815F, op_set_critter_extra_stat);
    // 0x8160 - int  get_critter_base_stat(object, Stat stat)
    interpreterRegisterOpcode(0x8160, op_get_critter_base_stat);
    // 0x8161 - int  get_critter_extra_stat(object, Stat stat)
    interpreterRegisterOpcode(0x8161, op_get_critter_extra_stat);
    // 0x8242 - void set_critter_skill_points(object critter, int skill, int value)
    interpreterRegisterOpcode(0x8242, op_set_critter_skill_points);
    // 0x8243 - int  get_critter_skill_points(object critter, int skill)
    interpreterRegisterOpcode(0x8243, op_get_critter_skill_points);
    // 0x8244 - void set_available_skill_points(int value)
    interpreterRegisterOpcode(0x8244, op_set_available_skill_points);
    // 0x8245 - int  get_available_skill_points()
    interpreterRegisterOpcode(0x8245, op_get_available_skill_points);
    // 0x8246 - void mod_skill_points_per_level(int value)
    interpreterRegisterOpcode(0x8246, op_mod_skill_points_per_level);

    // 0x81b4 - void set_stat_max(int stat, int value)
    interpreterRegisterOpcode(0x81B4, op_set_stat_max);
    // 0x81b5 - void set_stat_min(int stat, int value)
    interpreterRegisterOpcode(0x81B5, op_set_stat_min);
    // 0x81b7 - void set_pc_stat_max(int stat, int value)
    interpreterRegisterOpcode(0x81B7, op_set_pc_stat_max);
    // 0x81b8 - void set_pc_stat_min(int stat, int value)
    interpreterRegisterOpcode(0x81B8, op_set_pc_stat_min);
    // 0x81b9 - void set_npc_stat_max(int stat, int value)
    interpreterRegisterOpcode(0x81B9, op_set_npc_stat_max);
    // 0x81ba - void set_npc_stat_min(int stat, int value)
    interpreterRegisterOpcode(0x81BA, op_set_npc_stat_min);

    // 0x816b - int  input_funcs_available() // deprecated; do not implement
    // 0x816c - int  key_pressed(int dxScancode)
    interpreterRegisterOpcode(0x816C, op_key_pressed);
    // 0x8162 - void tap_key(int dxScancode)
    interpreterRegisterOpcode(0x8162, op_tap_key);
    // 0x821c - int  get_mouse_x()
    interpreterRegisterOpcode(0x821C, op_get_mouse_x);
    // 0x821d - int  get_mouse_y()
    interpreterRegisterOpcode(0x821D, op_get_mouse_y);
    // 0x821e - int  get_mouse_buttons()
    interpreterRegisterOpcode(0x821E, op_get_mouse_buttons);
    // 0x821f - int  get_window_under_mouse()
    interpreterRegisterOpcode(0x821F, op_get_window_under_mouse);

    // 0x8163 - int get_year()
    interpreterRegisterOpcode(0x8163, op_get_year);

    // 0x8164 - bool game_loaded()
    interpreterRegisterOpcode(0x8164, op_game_loaded);

    // F2-008: Register all shader/graphics opcodes as safe no-op stubs.
    // Previously these 11 opcodes had no registration — nullptr handler →
    // programFatalError → longjmp → script termination. Now they log warnings
    // and push safe defaults, matching the pattern of the 22 existing stubs.
    // 0x8165 - bool graphics_funcs_available()
    interpreterRegisterOpcode(0x8165, op_graphics_funcs_available);
    // 0x8166 - int  load_shader(string path)
    interpreterRegisterOpcode(0x8166, op_load_shader);
    // 0x8167 - void free_shader(int ID)
    interpreterRegisterOpcode(0x8167, op_free_shader);
    // 0x8168 - void activate_shader(int ID)
    interpreterRegisterOpcode(0x8168, op_activate_shader);
    // 0x8169 - void deactivate_shader(int ID)
    interpreterRegisterOpcode(0x8169, op_deactivate_shader);
    // 0x816d - void set_shader_int(int ID, string param, int value)
    interpreterRegisterOpcode(0x816D, op_set_shader_int);
    // 0x816e - void set_shader_float(int ID, string param, float value)
    interpreterRegisterOpcode(0x816E, op_set_shader_float);
    // 0x816f - void set_shader_vector(int ID, string param, float f1, float f2, float f3, float f4)
    interpreterRegisterOpcode(0x816F, op_set_shader_vector);
    // 0x81ad - int get_shader_version()
    interpreterRegisterOpcode(0x81AD, op_get_shader_version);
    // 0x81ae - void set_shader_mode(int mode)
    interpreterRegisterOpcode(0x81AE, op_set_shader_mode);
    // 0x81b0 - void force_graphics_refresh(bool enabled)
    interpreterRegisterOpcode(0x81B0, op_force_graphics_refresh);
    // 0x81b1 - int get_shader_texture(int ID, int texture)
    interpreterRegisterOpcode(0x81B1, op_get_shader_texture);
    // 0x81b2 - void set_shader_texture(int ID, string param, int texID)
    interpreterRegisterOpcode(0x81B2, op_set_shader_texture);

    // 0x816a - void set_global_script_repeat(int frames)
    interpreterRegisterOpcode(0x816A, op_set_global_script_repeat);
    // 0x819b - void set_global_script_type(int type)
    interpreterRegisterOpcode(0x819B, op_set_global_script_type);
    // 0x819c - int available_global_script_types()
    interpreterRegisterOpcode(0x819c, op_available_global_script_types);

    // 0x8170 - bool in_world_map()
    interpreterRegisterOpcode(0x8170, op_in_world_map);

    // 0x8171 - void force_encounter(int map)
    interpreterRegisterOpcode(0x8171, op_force_encounter);
    // 0x8229 - void force_encounter_with_flags(int map, int flags)
    interpreterRegisterOpcode(0x8229, op_force_encounter_with_flags);
    // 0x822a - void set_map_time_multi(float multi)
    interpreterRegisterOpcode(0x822A, op_set_map_time_multi);

    // 0x8172 - void set_world_map_pos(int x, int y)
    interpreterRegisterOpcode(0x8172, op_set_world_map_pos);
    // 0x8173 - int get_world_map_x_pos()
    interpreterRegisterOpcode(0x8173, op_get_world_map_x_pos);
    // 0x8174 - int get_world_map_y_pos()
    interpreterRegisterOpcode(0x8174, op_get_world_map_y_pos);

    // 0x8175 - void set_dm_model(string name)
    // 0x8176 - void set_df_model(string name)
    // 0x8177 - void set_movie_path(string filename, int movieid)
    // Always register these opcodes unconditionally to prevent script-
    // terminating programFatalError when HeroAppearanceMod is disabled.
    // op_set_dm_model / op_set_df_model store global model numbers which
    // are simply ignored by the engine when the config is off.
    // op_set_movie_path stores movie path overrides — zero connection to
    // hero model appearance (F-029).
    interpreterRegisterOpcode(0x8175, op_set_dm_model);
    interpreterRegisterOpcode(0x8176, op_set_df_model);
    interpreterRegisterOpcode(0x8177, op_set_movie_path);

    // 0x8178 - void set_perk_image(int perkID, int value)
    interpreterRegisterOpcode(0x8178, op_set_perk_image);
    // 0x8179 - void set_perk_ranks(int perkID, int value)
    interpreterRegisterOpcode(0x8179, op_set_perk_ranks);
    // 0x817a - void set_perk_level(int perkID, int value)
    interpreterRegisterOpcode(0x817A, op_set_perk_level);
    // 0x817b - void set_perk_stat(int perkID, int value)
    interpreterRegisterOpcode(0x817B, op_set_perk_stat);
    // 0x817c - void set_perk_stat_mag(int perkID, int value)
    interpreterRegisterOpcode(0x817C, op_set_perk_stat_mag);
    // 0x817d - void set_perk_skill1(int perkID, int value)
    interpreterRegisterOpcode(0x817D, op_set_perk_skill1);
    // 0x817e - void set_perk_skill1_mag(int perkID, int value)
    interpreterRegisterOpcode(0x817E, op_set_perk_skill1_mag);
    // 0x817f - void set_perk_type(int perkID, int value)
    interpreterRegisterOpcode(0x817F, op_set_perk_type);
    // 0x8180 - void set_perk_skill2(int perkID, int value)
    interpreterRegisterOpcode(0x8180, op_set_perk_skill2);
    // 0x8181 - void set_perk_skill2_mag(int perkID, int value)
    interpreterRegisterOpcode(0x8181, op_set_perk_skill2_mag);
    // 0x8182 - void set_perk_str(int perkID, int value)
    interpreterRegisterOpcode(0x8182, op_set_perk_str);
    // 0x8183 - void set_perk_per(int perkID, int value)
    interpreterRegisterOpcode(0x8183, op_set_perk_per);
    // 0x8184 - void set_perk_end(int perkID, int value)
    interpreterRegisterOpcode(0x8184, op_set_perk_end);
    // 0x8185 - void set_perk_chr(int perkID, int value)
    interpreterRegisterOpcode(0x8185, op_set_perk_chr);
    // 0x8186 - void set_perk_int(int perkID, int value)
    interpreterRegisterOpcode(0x8186, op_set_perk_int);
    // 0x8187 - void set_perk_agl(int perkID, int value)
    interpreterRegisterOpcode(0x8187, op_set_perk_agl);
    // 0x8188 - void set_perk_lck(int perkID, int value)
    interpreterRegisterOpcode(0x8188, op_set_perk_lck);
    // 0x8189 - void set_perk_name(int perkID, string value)
    interpreterRegisterOpcode(0x8189, op_set_perk_name);
    // 0x818a - void set_perk_desc(int perkID, string value)
    interpreterRegisterOpcode(0x818A, op_set_perk_desc);
    // 0x8247 - void set_perk_freq(int value)
    interpreterRegisterOpcode(0x8247, op_set_perk_freq);

    // 0x818b - void set_pipboy_available(int available)
    interpreterRegisterOpcode(0x818B, op_set_pipboy_available);

    // 0x818c - int get_kill_counter(int critterType)
    interpreterRegisterOpcode(0x818C, op_get_kill_counter);
    // 0x818d - void mod_kill_counter(int critterType, int amount)
    interpreterRegisterOpcode(0x818D, op_mod_kill_counter);

    // 0x818e - int get_perk_owed()
    interpreterRegisterOpcode(0x818E, op_get_perk_owed);
    // 0x818f - void set_perk_owed(int value)
    interpreterRegisterOpcode(0x818F, op_set_perk_owed);
    // 0x8190 - int get_perk_available(int perk)
    interpreterRegisterOpcode(0x8190, op_get_perk_available);

    // 0x8191 - int get_critter_current_ap(object critter)
    interpreterRegisterOpcode(0x8191, op_get_critter_current_ap);
    // 0x8192 - void set_critter_current_ap(object critter, int ap)
    interpreterRegisterOpcode(0x8192, op_set_critter_current_ap);

    // 0x8193 - int  active_hand()
    interpreterRegisterOpcode(0x8193, op_active_hand);
    // 0x8194 - void toggle_active_hand()
    interpreterRegisterOpcode(0x8194, op_toggle_active_hand);

    // 0x8195 - void set_weapon_knockback(object weapon, int type, int/float value)
    interpreterRegisterOpcode(0x8195, op_set_weapon_knockback);
    // 0x8196 - void set_target_knockback(object critter, int type, int/float value)
    interpreterRegisterOpcode(0x8196, op_set_target_knockback);
    // 0x8197 - void set_attacker_knockback(object critter, int type, int/float value)
    interpreterRegisterOpcode(0x8197, op_set_attacker_knockback);
    // 0x8198 - void remove_weapon_knockback(object weapon)
    interpreterRegisterOpcode(0x8198, op_remove_weapon_knockback);
    // 0x8199 - void remove_target_knockback(object critter)
    interpreterRegisterOpcode(0x8199, op_remove_target_knockback);
    // 0x819a - void remove_attacker_knockback(object critter)
    interpreterRegisterOpcode(0x819A, op_remove_attacker_knockback);

    // 0x819d - void  set_sfall_global(string/int varname, int/float value)
    interpreterRegisterOpcode(0x819D, op_set_sfall_global);
    // 0x819e - int   get_sfall_global_int(string/int varname)
    interpreterRegisterOpcode(0x819E, op_get_sfall_global_int);
    // 0x819f - float get_sfall_global_float(string/int varname)
    interpreterRegisterOpcode(0x819f, op_get_sfall_global_float);
    // 0x822d - int   create_array(int element_count, int flags)
    interpreterRegisterOpcode(0x822D, op_create_array);
    // 0x822e - void  set_array(int array, any element, any value)
    interpreterRegisterOpcode(0x822E, op_set_array);
    // 0x822f - any   get_array(int array, any element)
    interpreterRegisterOpcode(0x822F, op_get_array);
    // 0x8230 - void  free_array(int array)
    interpreterRegisterOpcode(0x8230, op_free_array);
    // 0x8231 - int   len_array(int array)
    interpreterRegisterOpcode(0x8231, op_len_array);
    // 0x8232 - void  resize_array(int array, int new_element_count)
    interpreterRegisterOpcode(0x8232, op_resize_array);
    // 0x8233 - int   temp_array(int element_count, int flags)
    interpreterRegisterOpcode(0x8233, op_temp_array);
    // 0x8234 - void  fix_array(int array)
    interpreterRegisterOpcode(0x8234, op_fix_array);
    // 0x8239 - int   scan_array(int array, int/float var)
    interpreterRegisterOpcode(0x8239, op_scan_array);
    // 0x8254 - void  save_array(any key, int array)
    interpreterRegisterOpcode(0x8254, op_save_array);
    // 0x8255 - int   load_array(any key)
    interpreterRegisterOpcode(0x8255, op_load_array);
    // 0x8256 - int   array_key(int array, int index)
    interpreterRegisterOpcode(0x8256, op_get_array_key);
    // 0x8257 - int   arrayexpr(any key, any value)
    interpreterRegisterOpcode(0x8257, op_arrayexpr);
    // 0x8285 - int   arrays_equal(int array1, int array2)
    interpreterRegisterOpcode(0x8285, op_arrays_equal);
    // 0x8286 - int   array_filter(int array, procedure filterProc)
    interpreterRegisterOpcode(0x8286, op_array_filter);
    // 0x8287 - int   array_transform(int array, procedure transformProc)
    interpreterRegisterOpcode(0x8287, op_array_transform);

    // 0x81a0 - void set_pickpocket_max(int percentage)
    interpreterRegisterOpcode(0x81A0, op_set_pickpocket_max);
    // 0x81a1 - void set_hit_chance_max(int percentage)
    interpreterRegisterOpcode(0x81A1, op_set_hit_chance_max);
    // 0x81a2 - void set_skill_max(int value)
    interpreterRegisterOpcode(0x81A2, op_set_skill_max);
    // 0x81aa - void set_xp_mod(int percentage)
    interpreterRegisterOpcode(0x81AA, op_set_xp_mod);
    // 0x81ab - void set_perk_level_mod(int levels)
    interpreterRegisterOpcode(0x81AB, op_set_perk_level_mod);

    // 0x81c5 - void set_critter_hit_chance_mod(object, int max, int mod)
    interpreterRegisterOpcode(0x81C5, op_set_critter_hit_chance_mod);
    // 0x81c6 - void set_base_hit_chance_mod(int max, int mod)
    interpreterRegisterOpcode(0x81C6, op_set_base_hit_chance_mod);
    // 0x81c7 - void set_critter_skill_mod(object, int skill, int mod)
    interpreterRegisterOpcode(0x81C7, op_set_critter_skill_mod);
    // 0x81c8 - void set_base_skill_mod(int skill, int mod)
    interpreterRegisterOpcode(0x81C8, op_set_base_skill_mod);
    // 0x81c9 - void set_critter_pickpocket_mod(object, int max, int mod)
    interpreterRegisterOpcode(0x81C9, op_set_critter_pickpocket_mod);
    // 0x81ca - void set_base_pickpocket_mod(int max, int mod)
    interpreterRegisterOpcode(0x81CA, op_set_base_pickpocket_mod);

    // note: these are deprecated; do not implement
    // 0x81a3 - int  eax_available()      (IMPLEMENTED as 0-stub — et tu mods call it)
    // 0x81a4 - void set_eax_environment(int environment)  (IMPLEMENTED as no-op)
    interpreterRegisterOpcode(0x81a3, op_eax_available);
    interpreterRegisterOpcode(0x81a4, op_set_eax_environment);

    // 0x81a5 - void inc_npc_level(int pid/string name)
    interpreterRegisterOpcode(0x81a5, op_inc_npc_level);
    // 0x8241 - int  get_npc_level(int pid/string name)
    interpreterRegisterOpcode(0x8241, op_get_npc_level);

    // 0x81a6 - int get_viewport_x()
    interpreterRegisterOpcode(0x81A6, op_get_viewport_x);
    // 0x81a7 - int get_viewport_y()
    interpreterRegisterOpcode(0x81A7, op_get_viewport_y);
    // 0x81a8 - void set_viewport_x(int view_x)
    interpreterRegisterOpcode(0x81A8, op_set_viewport_x);
    // 0x81a9 - void set_viewport_y(int view_y)
    interpreterRegisterOpcode(0x81A9, op_set_viewport_y);

    // 0x81ac - int   get_ini_setting(string setting)
    interpreterRegisterOpcode(0x81AC, op_get_ini_setting);
    // 0x81eb - string get_ini_string(string setting)
    interpreterRegisterOpcode(0x81EB, op_get_ini_string);

    // 0x81af - int get_game_mode()
    interpreterRegisterOpcode(0x81AF, op_get_game_mode);

    // 0x81b3 - int get_uptime()
    interpreterRegisterOpcode(0x81B3, op_get_uptime);

    // 0x81b6 - void set_car_current_town(int town)
    interpreterRegisterOpcode(0x81B6, op_set_car_current_town);

    // 0x81bb - void set_fake_perk(string name, int level, int image, string desc)
    interpreterRegisterOpcode(0x81BB, op_set_fake_perk);
    // 0x81bc - void set_fake_trait(string name, int active, int image, string desc)
    interpreterRegisterOpcode(0x81BC, op_set_fake_trait);
    // 0x81bd - void set_selectable_perk(string name, int active, int image, string desc)
    interpreterRegisterOpcode(0x81BD, op_set_selectable_perk);
    // 0x81be - void set_perkbox_title(string title)
    interpreterRegisterOpcode(0x81BE, op_set_perkbox_title);
    // 0x81bf - void hide_real_perks()
    interpreterRegisterOpcode(0x81BF, op_hide_real_perks);
    // 0x81c0 - void show_real_perks()
    interpreterRegisterOpcode(0x81C0, op_show_real_perks);
    // 0x81c1 - int has_fake_perk(string name/int extraPerkID)
    interpreterRegisterOpcode(0x81C1, op_has_fake_perk);
    // 0x81c2 - int has_fake_trait(string name)
    interpreterRegisterOpcode(0x81C2, op_has_fake_trait);
    // 0x81c3 - void perk_add_mode(int type)
    interpreterRegisterOpcode(0x81C3, op_perk_add_mode);
    // 0x81c4 - void clear_selectable_perks()
    interpreterRegisterOpcode(0x81C4, op_clear_selectable_perks);
    // 0x8225 - void remove_trait(int traitID)
    interpreterRegisterOpcode(0x8225, op_remove_trait);

    // 0x81cb - void set_pyromaniac_mod(int bonus)
    interpreterRegisterOpcode(0x81CB, op_set_pyromaniac_mod);
    // 0x81cc - void apply_heaveho_fix()
    interpreterRegisterOpcode(0x81CC, op_apply_heaveho_fix);
    // 0x81cd - void set_swiftlearner_mod(int bonus)
    interpreterRegisterOpcode(0x81CD, op_set_swiftlearner_mod);
    // 0x81ce - void set_hp_per_level_mod(int mod)
    interpreterRegisterOpcode(0x81CE, op_set_hp_per_level_mod);

    // 0x81dc - void show_iface_tag(int tag)
    interpreterRegisterOpcode(0x81DC, op_show_iface_tag);
    // 0x81dd - void hide_iface_tag(int tag)
    interpreterRegisterOpcode(0x81DD, op_hide_iface_tag);
    // 0x81de - int  is_iface_tag_active(int tag)
    interpreterRegisterOpcode(0x81DE, op_is_iface_tag_active);

    // 0x81df - int  get_bodypart_hit_modifier(int bodypart)
    interpreterRegisterOpcode(0x81DF, op_get_bodypart_hit_modifier);
    // 0x81e0 - void set_bodypart_hit_modifier(int bodypart, int value)
    interpreterRegisterOpcode(0x81E0, op_set_bodypart_hit_modifier);

    // 0x81e1 - void set_critical_table(int crittertype, int bodypart, int level, int valuetype, int value)
    interpreterRegisterOpcode(0x81E1, op_set_critical_table);
    // 0x81e2 - int  get_critical_table(int crittertype, int bodypart, int level, int valuetype)
    interpreterRegisterOpcode(0x81E2, op_get_critical_table);
    // 0x81e3 - void reset_critical_table(int crittertype, int bodypart, int level, int valuetype)
    interpreterRegisterOpcode(0x81E3, op_reset_critical_table);

    // 0x81e4 - int   get_sfall_arg()
    interpreterRegisterOpcode(0x81e4, op_get_sfall_arg);

    // 0x823c - array get_sfall_args()
    interpreterRegisterOpcode(0x823c, op_get_sfall_args);

    // 0x823d - void  set_sfall_arg(int argnum, int value)
    interpreterRegisterOpcode(0x823d, op_set_sfall_arg);

    // 0x81e5 - void  set_sfall_return(any value)
    interpreterRegisterOpcode(0x81e5, op_set_sfall_return);

    // 0x81ea - int   init_hook()  -> OBSOLETE, do not implement

    // 0x81e6 - void set_unspent_ap_bonus(int multiplier)
    interpreterRegisterOpcode(0x81E6, op_set_unspent_ap_bonus);
    // 0x81e7 - int  get_unspent_ap_bonus()
    interpreterRegisterOpcode(0x81E7, op_get_unspent_ap_bonus);
    // 0x81e8 - void set_unspent_ap_perk_bonus(int multiplier)
    interpreterRegisterOpcode(0x81E8, op_set_unspent_ap_perk_bonus);
    // 0x81e9 - int  get_unspent_ap_perk_bonus()
    interpreterRegisterOpcode(0x81E9, op_get_unspent_ap_perk_bonus);

    // 0x81ec - float sqrt(float)
    interpreterRegisterOpcode(0x81EC, op_sqrt);
    // 0x81ed - int/float abs(int/float)
    interpreterRegisterOpcode(0x81ED, op_abs);
    // 0x81ee - float sin(float)
    interpreterRegisterOpcode(0x81EE, op_sin);
    // 0x81ef - float cos(float)
    interpreterRegisterOpcode(0x81EF, op_cos);
    // 0x81f0 - float tan(float)
    interpreterRegisterOpcode(0x81F0, op_tan);
    // 0x81f1 - float arctan(float x, float y)
    interpreterRegisterOpcode(0x81F1, op_arctan);
    // 0x8263 - ^ operator (exponentiation)
    interpreterRegisterOpcode(0x8263, op_power);
    // 0x8264 - float log(float)
    interpreterRegisterOpcode(0x8264, op_log);
    // 0x8265 - float exponent(float)
    interpreterRegisterOpcode(0x8265, op_exponent);
    // 0x8266 - int ceil(float)
    interpreterRegisterOpcode(0x8266, op_ceil);
    // 0x8267 - int round(float)
    interpreterRegisterOpcode(0x8267, op_round);
    // 0x827f - div operator (signed integer division)
    interpreterRegisterOpcode(0x827F, op_div);

    // 0x81f2 - void set_palette(string path)
    interpreterRegisterOpcode(0x81F2, op_set_palette);

    // 0x81f3 - void remove_script(object)
    interpreterRegisterOpcode(0x81F3, op_remove_script);
    // 0x81f4 - void set_script(object, int scriptid)
    interpreterRegisterOpcode(0x81F4, op_set_script);
    // 0x81f5 - int get_script(object)
    interpreterRegisterOpcode(0x81F5, op_get_script);

    // 0x81f6 - int nb_create_char() // deprecated; do not implement

    // 0x81f7 - int   fs_create(string path, int size)
    interpreterRegisterOpcode(0x81f7, op_fs_create);
    // 0x81f8 - int   fs_copy(string path, string source)
    interpreterRegisterOpcode(0x81f8, op_fs_copy);
    // 0x81f9 - int   fs_find(string path)
    interpreterRegisterOpcode(0x81f9, op_fs_find);
    // 0x81fa - void  fs_write_byte(int id, int data)
    interpreterRegisterOpcode(0x81fa, op_fs_write_byte);
    // 0x81fb - void  fs_write_short(int id, int data)
    interpreterRegisterOpcode(0x81fb, op_fs_write_short);
    // 0x81fc - void  fs_write_int(int id, int data)
    interpreterRegisterOpcode(0x81fc, op_fs_write_int);
    // 0x81fd - void  fs_write_int(int id, int data)  (M-07 F-10)
    // sfall registers 0x81fd as fs_write_int — a DUPLICATE of 0x81fc
    // (Opcodes.cpp:137-138). The opcode-list "fs_write_float" name is a
    // doc artifact; the real sfall runtime dispatches 0x81fd to
    // op_fs_write_int. The fork registered op_fs_write_float here, so a
    // script calling fs_write_int via 0x81fd wrote a genuine IEEE float.
    interpreterRegisterOpcode(0x81fd, op_fs_write_int);
    // 0x81fe - void  fs_write_string(int id, string data)
    interpreterRegisterOpcode(0x81fe, op_fs_write_string);
    // 0x8208 - void  fs_write_bstring(int id, string data)
    interpreterRegisterOpcode(0x8208, op_fs_write_bstring);
    // 0x8209 - int   fs_read_byte(int id)
    interpreterRegisterOpcode(0x8209, op_fs_read_byte);
    // 0x820a - int   fs_read_short(int id)
    interpreterRegisterOpcode(0x820a, op_fs_read_short);
    // 0x820b - int   fs_read_int(int id)
    interpreterRegisterOpcode(0x820b, op_fs_read_int);
    // 0x820c - float fs_read_float(int id)
    interpreterRegisterOpcode(0x820c, op_fs_read_float);
    // 0x8282 - string fs_read_string(int id)
    interpreterRegisterOpcode(0x8282, op_fs_read_string);
    // 0x8283 - string fs_read_bstring(int id)
    interpreterRegisterOpcode(0x8283, op_fs_read_bstring);
    // 0x81ff - void  fs_delete(int id)
    interpreterRegisterOpcode(0x81ff, op_fs_delete);
    // 0x8200 - int   fs_size(int id)
    interpreterRegisterOpcode(0x8200, op_fs_size);
    // 0x8201 - int   fs_pos(int id)
    interpreterRegisterOpcode(0x8201, op_fs_pos);
    // 0x8202 - void  fs_seek(int id, int pos)
    interpreterRegisterOpcode(0x8202, op_fs_seek);
    // 0x8203 - void  fs_resize(int id, int size)
    interpreterRegisterOpcode(0x8203, op_fs_resize);

    // 0x8204 - int  get_proto_data(int pid, int offset)
    interpreterRegisterOpcode(0x8204, op_get_proto_data);
    // 0x8205 - void set_proto_data(int pid, int offset, int value)
    interpreterRegisterOpcode(0x8205, op_set_proto_data);

    // 0x8206 - void set_self(object)
    interpreterRegisterOpcode(0x8206, op_set_self);
    // 0x8207 - void register_hook(int hook)
    interpreterRegisterOpcode(0x8207, op_register_hook);

    // 0x820d - int   list_begin(int type)
    interpreterRegisterOpcode(0x820D, op_list_begin);
    // 0x820e - int   list_next(int listid)
    interpreterRegisterOpcode(0x820E, op_list_next);
    // 0x820f - void  list_end(int listid)
    interpreterRegisterOpcode(0x820F, op_list_end);
    // 0x8236 - array list_as_array(int type)
    interpreterRegisterOpcode(0x8236, op_list_as_array);

    // 0x8210 - int sfall_ver_major()
    interpreterRegisterOpcode(0x8210, op_get_version_major);
    // 0x8211 - int sfall_ver_minor()
    interpreterRegisterOpcode(0x8211, op_get_version_minor);
    // 0x8212 - int sfall_ver_build()
    interpreterRegisterOpcode(0x8212, op_get_version_patch);

    // 0x8213 - void hero_select_win(int)
    // 0x8214 - void set_hero_race(int race)
    // 0x8215 - void set_hero_style(int style)
    // Always register these opcodes unconditionally to prevent script-
    // terminating programFatalError when HeroAppearanceMod is disabled.
    // The opcode implementations safely no-op when the config is off:
    //   op_hero_select_win: checks characterEditorGetWindow() >= 0 first.
    //   op_set_hero_race / op_set_hero_style: just store in global vars.
    interpreterRegisterOpcode(0x8213, op_hero_select_win);
    interpreterRegisterOpcode(0x8214, op_set_hero_race);
    interpreterRegisterOpcode(0x8215, op_set_hero_style);

    // 0x8216 - void set_critter_burst_disable(object critter, int disable)
    interpreterRegisterOpcode(0x8216, op_set_critter_burst_disable);

    // 0x8217 - int  get_weapon_ammo_pid(object weapon)
    interpreterRegisterOpcode(0x8217, op_get_weapon_ammo_pid);
    // 0x8218 - void set_weapon_ammo_pid(object weapon, int pid)
    interpreterRegisterOpcode(0x8218, op_set_weapon_ammo_pid);
    // 0x8219 - int  get_weapon_ammo_count(object weapon)
    interpreterRegisterOpcode(0x8219, op_get_weapon_ammo_count);
    // 0x821a - void set_weapon_ammo_count(object weapon, int count)
    interpreterRegisterOpcode(0x821A, op_set_weapon_ammo_count);

    // 0x8220 - int get_screen_width()
    interpreterRegisterOpcode(0x8220, op_get_screen_width);
    // 0x8221 - int get_screen_height()
    interpreterRegisterOpcode(0x8221, op_get_screen_height);

    // 0x8222 - void stop_game()
    interpreterRegisterOpcode(0x8222, op_stop_game);
    // 0x8223 - void resume_game()
    interpreterRegisterOpcode(0x8223, op_resume_game);
    // 0x8224 - void create_message_window(string message)
    interpreterRegisterOpcode(0x8224, op_create_message_window);

    // 0x8226 - int get_light_level()
    interpreterRegisterOpcode(0x8226, op_get_light_level);

    // 0x8227 - void refresh_pc_art()
    interpreterRegisterOpcode(0x8227, op_refresh_pc_art);

    // 0x8228 - int get_attack_type()
    interpreterRegisterOpcode(0x8228, op_get_attack_type);

    // 0x822b - int  play_sfall_sound(string file, int mode)
    interpreterRegisterOpcode(0x822B, op_play_sfall_sound);
    // 0x822c - void stop_sfall_sound(int soundID)
    interpreterRegisterOpcode(0x822C, op_stop_sfall_sound);

    // 0x8235 - array string_split(string string, string split)
    interpreterRegisterOpcode(0x8235, op_string_split);
    // 0x8237 - int   atoi(string string)
    interpreterRegisterOpcode(0x8237, op_parse_int);
    // 0x8238 - float atof(string string)
    interpreterRegisterOpcode(0x8238, op_atof);
    // 0x824e - string substr(string string, int start, int length)
    interpreterRegisterOpcode(0x824E, op_substr);
    // 0x824f - int   strlen(string string)
    interpreterRegisterOpcode(0x824F, op_get_string_length);
    // 0x8250 - string sprintf(string format, any value)
    interpreterRegisterOpcode(0x8250, op_sprintf);
    // 0x8251 - int   charcode(string string)
    interpreterRegisterOpcode(0x8251, op_charcode);
    // 0x8253 - int   typeof(any value)
    interpreterRegisterOpcode(0x8253, op_type_of);
    // 0x8284 - int   string_null_or_empty(string str)
    interpreterRegisterOpcode(0x8284, op_string_null_or_empty);

    // 0x823a - int get_tile_fid(int tileData)
    interpreterRegisterOpcode(0x823A, op_get_tile_fid);

    // 0x823b - int modified_ini() // deprecated: do not implement

    // 0x823e - void force_aimed_shots(int pid)
    interpreterRegisterOpcode(0x823E, op_force_aimed_shots);

    // 0x823f - void disable_aimed_shots(int pid)
    interpreterRegisterOpcode(0x823F, op_disable_aimed_shots);

    // 0x8240 - void mark_movie_played(int id)
    interpreterRegisterOpcode(0x8240, op_mark_movie_played);

    // H-08: sfall runtime + compiler bind 0x8248 = get_last_attacker and
    // 0x8249 = get_last_target (sfall Opcodes.cpp:424-425; sslc enum order
    // O_TS_GET_LAST_ATTACKER=0x8248, O_TS_GET_LAST_TARGET=0x8249). The fork
    // followed the reversed docs-side table, so any third-party sfall mod
    // calling get_last_target() received the attacker and vice versa.
    // 0x8248 - object get_last_attacker(object critter)
    interpreterRegisterOpcode(0x8248, op_get_last_attacker);
    // 0x8249 - object get_last_target(object critter)
    interpreterRegisterOpcode(0x8249, op_get_last_target);
    // 0x824a - void block_combat(int enable)
    interpreterRegisterOpcode(0x824A, op_block_combat);

    // 0x824b - int tile_under_cursor()
    interpreterRegisterOpcode(0x824B, op_tile_under_cursor);
    // 0x824c - int gdialog_get_barter_mod()
    interpreterRegisterOpcode(0x824C, op_gdialog_get_barter_mod);
    // 0x824d - void set_inven_ap_cost(int cost)
    interpreterRegisterOpcode(0x824D, op_set_inven_ap_cost);

    // 0x825a - void reg_anim_destroy(object object)
    interpreterRegisterOpcode(0x825A, op_reg_anim_destroy);
    // 0x825b - void reg_anim_animate_and_hide(object object, int animID, int delay)
    interpreterRegisterOpcode(0x825B, op_reg_anim_animate_and_hide);
    // 0x825c - void reg_anim_combat_check(int enable)
    interpreterRegisterOpcode(0x825C, op_reg_anim_combat_check);
    // 0x825d - void reg_anim_light(object object, int radius, int delay)
    interpreterRegisterOpcode(0x825D, op_reg_anim_light);
    // 0x825e - void reg_anim_change_fid(object object, int FID, int delay)
    interpreterRegisterOpcode(0x825E, op_reg_anim_change_fid);
    // 0x825f - void reg_anim_take_out(object object, int holdFrameID, int delay)
    interpreterRegisterOpcode(0x825F, op_reg_anim_take_out);
    // 0x8260 - void reg_anim_turn_towards(object object, int tile/targetObj, int delay)
    interpreterRegisterOpcode(0x8260, op_reg_anim_turn_towards);

    // 0x8261 - int metarule2_explosions(object object)
    interpreterRegisterOpcode(0x8261, op_explosions_metarule);

    // 0x8262 - void register_hook_proc(int hook, procedure proc)
    interpreterRegisterOpcode(0x8262, op_register_hook_proc);

    // 0x826b - string message_str_game(int fileId, int messageId)
    interpreterRegisterOpcode(0x826B, op_get_message);
    // 0x826c - int sneak_success()
    interpreterRegisterOpcode(0x826C, op_sneak_success);
    // 0x826d - int tile_light(int elevation, int tileNum)
    interpreterRegisterOpcode(0x826D, op_tile_light);
    // 0x826e - object obj_blocking_line(object objFrom, int tileTo, int blockingType)
    interpreterRegisterOpcode(0x826E, op_make_straight_path);
    // 0x826f - object obj_blocking_tile(int tileNum, int elevation, int blockingType)
    interpreterRegisterOpcode(0x826F, op_obj_blocking_at);
    // 0x8270 - array tile_get_objs(int tileNum, int elevation)
    interpreterRegisterOpcode(0x8270, op_tile_get_objects);
    // 0x8271 - array party_member_list(int includeHidden)
    interpreterRegisterOpcode(0x8271, op_party_member_list);
    // 0x8272 - array path_find_to(object objFrom, int tileTo, int blockingType)
    interpreterRegisterOpcode(0x8272, op_make_path);
    // 0x8273 - object create_spatial(int scriptID, int tile, int elevation, int radius)
    interpreterRegisterOpcode(0x8273, op_create_spatial);
    // 0x8274 - int art_exists(int artFID)
    interpreterRegisterOpcode(0x8274, op_art_exists);
    // 0x8275 - int obj_is_carrying_obj(object invenObj, object itemObj)
    interpreterRegisterOpcode(0x8275, op_obj_is_carrying_obj);

    // 0x8276 - any sfall_func0(string funcName)
    interpreterRegisterOpcode(0x8276, op_sfall_func0);
    // 0x8277 - any sfall_func1(string funcName, arg1)
    interpreterRegisterOpcode(0x8277, op_sfall_func1);
    // 0x8278 - any sfall_func2(string funcName, arg1, arg2)
    interpreterRegisterOpcode(0x8278, op_sfall_func2);
    // 0x8279 - any sfall_func3(string funcName, arg1, arg2, arg3)
    interpreterRegisterOpcode(0x8279, op_sfall_func3);
    // 0x827a - any sfall_func4(string funcName, arg1, arg2, arg3, arg4)
    interpreterRegisterOpcode(0x827A, op_sfall_func4);
    // 0x827b - any sfall_func5(string funcName, arg1, arg2, arg3, arg4, arg5)
    interpreterRegisterOpcode(0x827B, op_sfall_func5);
    // 0x827c - any sfall_func6(string funcName, arg1, arg2, arg3, arg4, arg5, arg6)
    interpreterRegisterOpcode(0x827C, op_sfall_func6);
    // 0x8280 - any sfall_func7(string funcName, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
    interpreterRegisterOpcode(0x8280, op_sfall_func7);
    // 0x8281 - any sfall_func8(string funcName, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
    interpreterRegisterOpcode(0x8281, op_sfall_func8);

    // 0x827d - void register_hook_proc_spec(int hook, procedure proc)
    interpreterRegisterOpcode(0x827d, op_register_hook_proc_spec);
    // 0x827e - void reg_anim_callback(procedure proc)
    interpreterRegisterOpcode(0x827e, op_reg_anim_callback);

    // F-C01: Initialize VFS sandbox root from ddraw.ini.
    // Reads "VfsRootDir" from the [Misc] section. When set to a
    // non-empty directory path, all VFS operations are sandboxed:
    // absolute paths and ".." components are rejected, and all paths
    // are resolved relative to this root. When unset (default), the
    // sandbox remains off for backward compatibility.
    {
        char* vfsRoot = nullptr;
        configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "VfsRootDir", &vfsRoot);
        if (vfsRoot != nullptr && vfsRoot[0] != '\0') {
            sfallVfsSetRoot(vfsRoot);
        }
    }
}

void sfallOpcodesExit()
{
    sfallAnimCallbackReset();
    sfallVfsCloseAll();
    // F-083: clean up VFS sandbox root.
    sfallVfsSetRoot(nullptr);
}

} // namespace fallout
