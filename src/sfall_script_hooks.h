#ifndef FALLOUT_SFALL_SCRIPT_HOOKS_H_
#define FALLOUT_SFALL_SCRIPT_HOOKS_H_

#include "animation.h"
#include "interpreter.h"
#include "interpreter_extra.h"
#include "queue.h"
#include "scripts.h"
#include "skill_defs.h"

#include <array>
#include <initializer_list>
#include <memory>
#include <string>

namespace fallout {

// F-1 (FIX): gPartyCooperativeCombat — cooperatively attacks player's target.
// Set by metarule3(999, mode) in interpreter_extra.cc. When enabled (true),
// party member NPCs should select the same target as the player (gDude) in
// combat. Wire this flag into combat_ai.cc target selection: when a party
// member evaluates targets, if gPartyCooperativeCombat is true, prefer
// gDude->data.critter.combat.whoHitMe (the player's current target) over
// the default AI target selection logic.
extern bool gPartyCooperativeCombat;

// Some hooks are implemented in sfall but aren't worth porting over:
// - useless, never deployed in popular mods
// - bad for performance and/or stability
// - awkwardly implemented (need better alternatives)

typedef enum {
    // Any hit chance calculation.
    HOOK_TOHIT = 0,

    // Attack hit roll.
    HOOK_AFTERHITROLL = 1,

    // AP cost of attacks.
    HOOK_CALCAPCOST = 2,

    // Death animation v1 — variant of the standard procedure that fires before
    // the death animation is selected (as opposed to HOOK_DEATHANIM2 which fires
    // after). Not implemented because sfall's DEATHANIM1 fires from a distinct
    // engine callback (action_destroy_p_proc → combatDamage) that does not
    // exist in CE's cleaned-up combat pipeline. Mods that need death animation
    // overrides should use HOOK_DEATHANIM2 (id 4), which provides the same
    // anim-override capability from the standard death animation path.
    // Engine change needed to implement: add a fire call in
    // endgameEndingDestroyStuff or the combatDeath handler before
    // HOOK_DEATHANIM2's fire site at actions.cc:325.
    // HOOK_DEATHANIM1 = 3,

    // Death animation selection.
    HOOK_DEATHANIM2 = 4,

    // Damage calculation.
    HOOK_COMBATDAMAGE = 5,

    // Critter death.
    HOOK_ONDEATH = 6,

    // AI target selection hook. Fires at combat_ai.cc:1836,1874,1934.
    HOOK_FINDTARGET = 7,

    // Using item on critter or scenery, before normal script proc.
    // TODO: rename to USEITEMON
    HOOK_USEOBJON = 8,

    // Removing object from inventory.
    // sync2: upstream enabled this hook with RemoveInventoryObjectHookReason
    // constants + scriptHooks_RemoveInventoryObject; the fork's item.cc
    // itemRemoveInternal now fires it (runHook=true path). The old fork
    // comment ("deliberately absent — not present in CE's itemRemove") is
    // obsolete after the sync2 merge.
    HOOK_REMOVEINVENOBJ = 9,

    // Barter price calculation.
    HOOK_BARTERPRICE = 10,

    // Movement AP cost calculation. For AI triggers for every hex they move.
    HOOK_MOVECOST = 11,

    // Hex-level blocking overrides (pathfinding, AI, shooting, sight).
    // Deliberately absent — these hooks intercepted low-level hex passability
    // checks in sfall's tile_obstructs() / hex_blocking_*() callbacks, which
    // ran on every hex evaluation during pathfinding and AI targeting.
    // CE restructured pathfinding to use a different architecture where
    // hex-by-hex blocking callbacks would require re-specialization of
    // the entire pathfinding pipeline. Performance impact on AI-heavy
    // encounters (30+ critters scanning hundreds of hexes per turn) makes
    // per-hex script callbacks impractical. Mods needing hex-level overrides
    // should use HOOK_FINDTARGET or HOOK_CANUSEWEAPON instead.
    // HOOK_HEXMOVEBLOCKING = 12,
    // HOOK_HEXAIBLOCKING = 13,
    // HOOK_HEXSHOOTBLOCKING = 14,
    // HOOK_HEXSIGHTBLOCKING = 15,

    // Weapon min/max damage calculation.
    HOOK_ITEMDAMAGE = 16,

    // Weapon ammo cost per attack (or per round for bursts).
    HOOK_AMMOCOST = 17,

    // Using items from inventory or main interface.
    // TODO: rename to USEITEM
    HOOK_USEOBJ = 18,

    // Keypress/release. Originally implemented in DirectInput layer, not in the game code. Used e.g. in Party Orders addon.
    HOOK_KEYPRESS = 19,

    // Similar to KEYPRESS but for mouse buttons.
    HOOK_MOUSECLICK = 20,

    // Skill is used on object.
    HOOK_USESKILL = 21,

    // Steal is attempted on a critter.
    HOOK_STEAL = 22,

    // A check if one critter can see another (in and out of combat).
    HOOK_WITHINPERCEPTION = 23,

    // Item is moved around the player's inventory via UI.
    HOOK_INVENTORYMOVE = 24,

    // Critter is equipping/unequiping item in armor/hand slots.
    HOOK_INVENWIELD = 25,

    // Character FID calculation in UI (inventory, barter). Used for npc_armor.mod
    HOOK_ADJUSTFID = 26,

    // Before/after combat turn (global version of combat_p_proc).
    HOOK_COMBATTURN = 27,

    // Car travel on worldmap. Can change car speed and fuel consumption.
    HOOK_CARTRAVEL = 28,

    // Normal global variable is set. Allows to override value.
    HOOK_SETGLOBALVAR = 29,

    // Continuously during rest, allows to interrupt it.
    HOOK_RESTTIMER = 30,

    // Game mode is changed. Used in many mods.
    HOOK_GAMEMODECHANGE = 31,

    HOOK_USEANIMOBJ = 32,

    // Explosive timer is set. Allows to override the time.
    HOOK_EXPLOSIVETIMER = 33,

    // Object is examined by player. Allows to override text.
    HOOK_DESCRIPTIONOBJ = 34,

    // Runs before using a skill, allows to override a critter using the skill.
    // TODO: maybe combine with USESKILL?
    HOOK_USESKILLON = 35,

    HOOK_ONEXPLOSION = 36,
    // Sub-combat-damage hook — fires for each individual bullet/melee-hit within
    // a single attack action, providing per-hit damage modification before
    // HOOK_COMBATDAMAGE (which fires for the overall attack). Not implemented
    // because sfall injects this at the damage application site inside
    // compute_explosion_on_extras / computeAttack (which computes per-target
    // damage). CE merged burst-handling logic into the main damage pipeline;
    // separating per-hit and per-attack hooks requires restructuring the
    // damage application loop. Most mods use HOOK_COMBATDAMAGE which provides
    // the same total-damage override capability.
    // HOOK_SUBCOMBATDAMAGE = 37,
    HOOK_SETLIGHTING = 38,

    // A continuous sneak check.
    HOOK_SNEAK = 39,

    // A script procedure is called.
    // Note: those two are basically the same hook with different flag argument value.
    HOOK_STDPROCEDURE = 40,
    HOOK_STDPROCEDURE_END = 41,

    HOOK_TARGETOBJECT = 42,

    // Random encounter occurs. Override map or cancel the encounter.
    HOOK_ENCOUNTER = 43,

    // Poison/radiation adjustment hooks — fire when the engine applies
    // poison or radiation damage to a critter (usually dude_obj).
    // HOOK_ADJUSTRADS is implemented (M-26): it fires from
    // critterAdjustRadiation() (critter.cc) before the radiation delta is
    // applied, matching sfall's AdjustRads_Script (ObjectHs.cpp:362-385).
    // HOOK_ADJUSTPOISON remains unimplemented because CE's poison pipeline
    // applies per-tick adjustments directly in critter.cc without an
    // sfall-style callback injection point; implementing it would require
    // refactoring the poison update loop to interpose a hook fire call.
    // Mods tracking poison should use HOOK_ONDEATH or HOOK_GAMEMODECHANGE.
    // HOOK_ADJUSTPOISON = 44,
    HOOK_ADJUSTRADS = 45,

    // NOTE: Deliberately absent — randomRoll() has 30+ call sites (skill
    // checks, combat rolls, AI evaluations) but no event_type context to
    // distinguish them. A pass-through hook at randomRoll() would fire on
    // every roll indiscriminately — hundreds per turn in heavy combat,
    // making per-roll script callbacks prohibitively expensive. Adding
    // event-type context to all 30+ call sites requires invasive engine
    // changes across skill.cc, combat.cc, combat_ai.cc, and critter.cc.
    // Mods that need roll modification should use targeted hooks:
    // HOOK_TOHIT (combat rolls), HOOK_AFTERHITROLL (hit/miss override),
    // HOOK_USESKILL (skill rolls), HOOK_STEAL (steal checks).
    // Engine change needed to implement: add event_type enum parameter
    // to randomRoll() and all callers, then gate the hook fire on opt-in.
    // HOOK_ROLLCHECK = 46,

    // NOTE: Deliberately absent — _ai_best_weapon() is a comparison function
    // with 10+ return points, each comparing the current best weapon against
    // a candidate using nested if/else chains. Adding a post-hoc object
    // override would require restructuring the comparison into a single-exit
    // pattern, which changes the function contract for all callers (6 sites
    // in combat_ai.cc, critter.cc). Script-returned Object* lifetime is
    // also problematic: the returned weapon must outlive the AI decision
    // cycle, but scripts don't own inventory objects.
    // Engine change needed to implement: refactor _ai_best_weapon to single-
    // exit, add Object* lifetime management for script-returned items.
    // HOOK_BESTWEAPON = 47,

    // Allows to prevent PC or NPC from using a weapon.
    HOOK_CANUSEWEAPON = 48,

    // Dialog start/end. Fires when a dialog session begins or ends.
    // Arguments: speaker (Object), headFid (int), reaction (int, -1 if not applicable).
    HOOK_DIALOG = 49,

    // Dialog reaction calculation. Fires when a reaction value is computed
    // for a dialog speaker.
    // Arguments: speaker (Object), reaction (int).
    HOOK_DIALOGREACTION = 50,

    // Fires when the player character levels up (after XP gain triggers a level-up).
    // Fire sites: stat.cc pcAddExperienceWithOptions(), character_editor.cc characterEditorUpdateLevel().
    // Arguments: critter (Object).
    HOOK_STATLEVELUP = 51,

    // Fires when barter/trade is initiated between the player and an NPC.
    // Fire site: game_dialog.cc gameDialogBarter().
    // Arguments: dude (Object), npc (Object), mode (int).
    HOOK_BARTER = 52,

    // Fires when a message is displayed to the player in the message monitor.
    // Fire site: display_monitor.cc displayMonitorAddMessage().
    // Arguments: message (string).
    HOOK_MESSAGE = 53,

    // RESERVED 54..60 — available for future hook types. Hook registers for
    // these IDs will be accepted by scriptHooksRegister() but never fire
    // (no fire sites exist). Add fire sites before claiming a reserved slot.

    // NOTE: Deliberately absent — sfxBuildWeaponName() returns char* to a
    // static 16-byte buffer (_sfx_file_name). Script string return values
    // would require buffer management, lifetime semantics, and may overflow
    // the static buffer. Audio system restructured in CE; weapon sound
    // naming is handled through a different pipeline. Mods needing custom
    // weapon sounds should modify proto data directly via set_weapon_sound.
    // Engine change needed to implement: replace static 16-byte buffer with
    // dynamic allocation, or add a string-table-based sound name resolver.
    // HOOK_BUILDSFXWEAPON = 61,

    HOOK_COUNT = 62,
} HookType;

// Number of implemented hook types (active enum entries with fire functions or
// declared fire sites). Updated when new hook types are added to the enum.
// Phase 7 added HOOK_DIALOG(49) through HOOK_MESSAGE(53): +5 → 43.
// M-26 enabled HOOK_ADJUSTRADS(45) with a fire site in critterAdjustRadiation: +1 → 44.
constexpr int HOOK_IMPLEMENTED_COUNT = 44;

typedef enum {
    REST_EVENT_TYPE_CANCEL = -1,
    REST_EVENT_TYPE_PROGRESS = 0,
    REST_EVENT_TYPE_COMPLETE = 1,
} RestEventType;

typedef enum {
    HOOK_INVENTORYMOVE_MAIN_BACKPACK = 0,
    HOOK_INVENTORYMOVE_LEFT_HAND = 1,
    HOOK_INVENTORYMOVE_RIGHT_HAND = 2,
    HOOK_INVENTORYMOVE_ARMOR_SLOT = 3,
    HOOK_INVENTORYMOVE_WEAPON_RELOAD = 4,
    HOOK_INVENTORYMOVE_CONTAINER = 5,
    HOOK_INVENTORYMOVE_GROUND = 6,
    HOOK_INVENTORYMOVE_PICKUP = 7,
    HOOK_INVENTORYMOVE_CHARACTER_PORTRAIT = 8,
    HOOK_INVENTORYMOVE_BARTER = 9,
} HookInventoryMoveType;

typedef enum {
    PERCEPTION_OTHER = 0,
    PERCEPTION_SEE = 1,
    PERCEPTION_HEAR = 2,
    PERCEPTION_AI_TARGET = 3,
} PerceptionType;

typedef enum {
    PERCEPTION_OUT_OF_RANGE = 0,
    PERCEPTION_IN_RANGE = 1,
    PERCEPTION_FORCE = 2,
} PerceptionResult;

constexpr size_t HOOKS_MAX_ARGUMENTS = 16;
constexpr size_t HOOKS_MAX_RETURN_VALUES = 8;

/**
 * Allows to delegate some logic to scripts:
 * - Each hook type has different number of arguments and return values
 * - Set up arguments, invoke `call()` and read return values
 * - Return values can be used to alter normal engine behavior if scripts request it
 */
class ScriptHookCall {
public:
    static ScriptHookCall* current();

    ScriptHookCall(HookType hookType, int maxReturnValues, std::initializer_list<ProgramValue> args);
    ~ScriptHookCall() = default;

    ScriptHookCall(const ScriptHookCall& other) = delete;
    ScriptHookCall(ScriptHookCall&& other) = delete;
    ScriptHookCall& operator=(const ScriptHookCall& other) = delete;
    ScriptHookCall& operator=(ScriptHookCall&& other) = delete;

    // Sets an argument value at given index.
    void setArgAt(int idx, ProgramValue value);
    // Adds return value from script.
    // numReturnValues will only increase if current script called this more times than the last one.
    void addReturnValueFromScript(ProgramValue value);

    void call();

    ProgramValue getNextArgFromScript();

    // Number of arguments supplied from the engine.
    int numArgs() const;
    // Maximum expected number of return values by the engine.
    int maxReturnValues() const;
    // Number of actually supplied values from all scripts.
    int numReturnValues() const;
    // Number of supplied values from the last script.
    int numScriptReturnValues() const;

    ProgramValue getArgAt(int idx) const;
    ProgramValue getReturnValueAt(int idx) const;
    HookType hookType() const;

    // Returns the Program* that set the return value at the given index.
    // For string return values, this is the program whose string table should
    // be used for resolution. Falls back to _lastProgram if no program was
    // recorded for this slot (e.g., return values set outside the call loop).
    Program* programForReturnValueAt(int idx) const;

    // Returns the Program* of the last script that ran during call(),
    // so hook consumers can resolve string-table-based return values.
    Program* lastProgram() const { return _lastProgram; }

    // Reentrancy guard for HOOK_GAMEMODECHANGE (I2-52).
    // Must be public — accessed by both ScriptHookCall methods and
    // the free function scriptHooks_GameModeChange().
    static bool _gameModeChangeInProgress;

    // I2-M16/I2-M35: Drain stale call-stack entries left by longjmp'd
    // ScriptHookCall frames.  Uses address-based staleness detection:
    // entries with addresses below currentStackAddr are from unwound
    // frames.  Called from call() and scriptHooks_GameModeChange().
    static void drainStaleEntries(uintptr_t currentStackAddr);

    // I2-M26: Clear all call-stack entries and per-type counters.
    // Called by scriptHooksReset() on game reset / new game cycle
    // to prevent stale entries from blocking hook execution after
    // a game state transition.
    static void clearCallStack();

private:
    static std::vector<ScriptHookCall*> _callStack;
    // I2-M08: Per-type depth counters for hook call reentrancy tracking.
    // Each hook type gets a guaranteed minimum allocation (MAX_PER_TYPE_DEPTH=4)
    // from the global budget (MAX_HOOK_CALL_DEPTH=8), preventing one hook type
    // from starving critical hooks like HOOK_ONDEATH.
    static std::array<int, HOOK_COUNT> _callStackPerType;

    HookType _hookType;
    int _maxRetVals = 0;

    ProgramValue _args[HOOKS_MAX_ARGUMENTS] = {};
    int _numArgs = 0;
    ProgramValue _retVals[HOOKS_MAX_RETURN_VALUES] = {};
    Program* _retValPrograms[HOOKS_MAX_RETURN_VALUES] = {};
    int _numRetVals = 0;

    int _scriptArgs = 0;
    int _scriptRetVals = 0;
    Program* _lastProgram = nullptr;
    bool _active = false;               // Set in call() before push, cleared after pop. Drain uses this to skip live frames.
};

struct BarterPriceContext {
    Object* dude;
    Object* npc;
    Object* requestTable;
    Object* offerTable;
    int value;
    int offerValue;
    int rawValue;
    int caps;
    bool offerButton;
    bool partyMember;
};

struct UseSkillOnHookResult {
    bool shouldContinue;
    bool userOverridden;
    bool allowInCombat;
};

enum AmmoCostHookType {
    AMMO_COST_HOOK_SINGLE_SHOT = 0,
    AMMO_COST_HOOK_CHECK_OUT_OF_AMMO = 1,
    AMMO_COST_HOOK_BURST_ROUNDS = 2,
    AMMO_COST_HOOK_BURST_SHOT = 3,
};

enum class RemoveInventoryObjectHookReason {
    // sfall's value for its "Unlimited Ammo" fix (see `inventoryEquipFunc`).
    WieldStack = -2,
    ItemRemovedInventory = 4831349,
    ItemRemoved = 4548572,
    ItemRemovedMulti = 4563866,
    ItemDestroyed = 4543215,
    ItemDestroyMulti = 4571599,
    ItemMove = 4683293,
    ItemReplace = 4686256,
    ConsumeDrug = 4666772,
    UseObj = 4666865,
    EquipArmor = 4658121,
    EquipWeapon = 4658675,
    UnloadWeapon = 4667030,
    UseDrugOn = 4834866,
    StealView = 4668206,
    ArmorEquipped = 4651961,
    LeftHandEquipped = 4651899,
    RightHandEquipped = 4651934,
    ReplaceWeapon = 4658526,
    Throw = 4266040,
    SubContainer = 4683191,
    AIUseDrugOn = 4359920,
    BarterArmor = 4675656,
    BarterWeapon = 4675722,
    InventoryDropCaps = 4667295,
    DropIntoContainer = 4678833,
};

enum class EncounterHookEventType {
    RandomEncounter = 0,
    // H-03/P-05: LocalMapEnter MUST be 1 to match sfall's HOOK_ENCOUNTER
    // arg0 encoding (hooks.yml:731 — "1 - when the player enters from the
    // world map"). Commit 86e6c4d changed this from 1 to 2 under the false
    // premise that sfall's arg0=1 meant "special encounter" — sfall never
    // encodes special encounters in arg0 (they live in arg2, encType==3).
    // The old 86e6c4d comment was a PRIOR_FIX_ATTEMPT regression: prior to
    // that pass CE's encoding matched sfall exactly.
    LocalMapEnter = 1,
    // ForcedEncounter is a CE-internal marker (0x100) used by the
    // worldmap.cc forced-encounter call site. Per sfall, HOOK_ENCOUNTER
    // NEVER fires for forced encounters (op_force_encounter jumps straight
    // to map load) — scriptHooks_Encounter() returns without firing when
    // this event type is passed (sfall N-01).
    ForcedEncounter = 256,
};

enum class EncounterHookResult {
    ContinueEncounter,
    ContinueTravel,
    LoadMapDirectly,
};

// Register a script procedure as a hook callback for the given hook type.
//
// Position behavior (matching sfall semantics):
//   atEnd=false (register_hook_proc):  push_back → highest index → executed FIRST
//                                      (last registered = highest priority).
//   atEnd=true  (register_hook_proc_spec): emplace at index 0 → executed LAST
//                                      (last registered = final override).
//
// NOTE: Only two positions are supported (start/highest-index and end/index-0).
// Sfall's optional explicit position parameter (insert at specific index) is
// not implemented — the binary atEnd flag covers all known RPU/Et Tu use cases.
// Hook execution order is determined by registration order, not by a numeric
// position parameter. Adding per-position insertion would require shifting
// existing registrations, which changes the execution order contract for
// scripts that registered earlier.
bool scriptHooksRegister(Program* program, HookType hookType, int procedureIndex, bool atEnd = false);
void scriptHooksUnregisterProgram(Program* program);

// Returns true when at least one script handler is registered for the given
// hook type. The engine uses this to decide whether the script layer will
// handle an adjustment that the engine would otherwise apply itself (e.g. the
// FastShotFix mode 2/3 melee-class AP reduction, which et tu's gl_apcost.ssl
// owns when HOOK_CALCAPCOST is registered).
bool scriptHooks_IsHookRegistered(HookType hookType);
bool scriptHooks_StdProcedure(int procedureNumber, Object* self, Object* source, Object* target, int fixedParam, bool after);
void scriptHooks_ItemDamage(Object* weapon, Object* critter, HitMode hitMode, bool isMeleeWeaponAttack, int* minDamagePtr, int* maxDamagePtr);
int scriptHooks_AmmoCost(Object* weapon, int rounds, int ammoCost, AmmoCostHookType hookType);
int scriptHooks_Steal(Object* thief, Object* target, Object* item, bool isPlanting, int quantity, int* xpOverride);

bool scriptHooksInit();
void scriptHooksReset();
void scriptHooksExit();

void scriptHooks_GameModeChange(int exit, int previousGameMode);

// Slideshow transition hooks — fire HOOK_GAMEMODECHANGE during endgame
// slideshow transitions. These must be called from the slideshow entry/exit
// points in game.cc or endgame.cc.
//
// Wire-in points:
//   scriptHooks_SlideshowStart() → call at endgame.cc:214 (start of endgamePlaySlideshow)
//     or at the point where game mode transitions to slideshow/kSpecial in the
//     endgame sequence. Passes exit=0, previous=GameMode::getCurrentGameMode().
//   scriptHooks_SlideshowEnd()   → call at endgame.cc:233 (after endgamePlaySlideshow
//     returns, before endgameEndingSlideshowWindowFree). Passes exit=0,
//     previous=GameMode::getCurrentGameMode().
//
// These are scaffolding functions — the actual wire-in requires modifying the
// slideshow code in endgame.cc. Until wired, slideshow transitions will not
// trigger HOOK_GAMEMODECHANGE, and scripts listening for mode changes to
// detect slideshow start/end will not receive callbacks.
void scriptHooks_SlideshowStart();
void scriptHooks_SlideshowEnd();

bool scriptHooks_RestTimer(unsigned int gameTime, RestEventType eventType, int hours, int minutes);
void scriptHooks_OnDeath(Object* critter);
int scriptHooks_ExplosiveTimer(Object* explosive, int delay, EventType eventType);
EncounterHookResult scriptHooks_Encounter(EncounterHookEventType eventType, int* mapIdPtr, bool isSpecial, int tableId, int entryId);

// M-26: HOOK_ADJUSTRADS fire function. Fires from critterAdjustRadiation()
// before the radiation delta is applied to a critter (sfall ObjectHs.cpp:
// AdjustRads_Script). Returns the (possibly script-overridden) amount.
int scriptHooks_AdjustRads(Object* critter, int amount);
bool scriptHooks_InventoryMove(HookInventoryMoveType actionType, Object* item, Object* targetItem);
bool scriptHooks_CombatTurnStart(Object* critter, bool reloadedDuringCombat);
bool scriptHooks_CombatTurnEnd(Object* critter, int turnResult, bool reloadedDuringCombat);
void scriptHooks_CombatTurnCombatEnd(Object* critter);
PerceptionResult scriptHooks_WithinPerception(Object* watcher, Object* target, PerceptionType type, PerceptionResult result);
int scriptHooks_CalcApCost(Object* critter, HitMode hitMode, bool aiming, int actionPoints, Object* weapon);
int scriptHooks_MoveCost(Object* critter, int distance, int actionPoints);
int scriptHooks_ToHit(Object* attacker, Object* defender, int tile, HitMode hitMode, HitLocation hitLocation, int hitChance, int hitChanceUncapped, bool useDistance);
int scriptHooks_AfterHitRoll(Object* attacker, Object** defenderPtr, HitLocation* hitLocationPtr, int hitChance, int roll);
void scriptHooks_DeathAnim(Object* attacker, Object* defender, Object* weapon, int damage, AnimationType* anim);
UseSkillOnHookResult scriptHooks_UseSkillOn(Object** userPtr, Object* target, Skill skill);
int scriptHooks_UseSkill(Object* user, Object* target, Skill skill, int skillBonus);
int scriptHooks_UseItem(Object* user, Object* objUsed);
int scriptHooks_UseItemOn(Object* user, Object* target, Object* objUsed);
void scriptHooks_RemoveInventoryObject(Object* owner, Object* item, int quantity, RemoveInventoryObjectHookReason reason, Object* target);
void scriptHooks_ComputeDamage(Attack* attack, int numRounds, int baseDmgMult);
void scriptHooks_BarterPrice(BarterPriceContext* ctx);

int scriptHooks_AdjustFid(int vanillaFid, int modifiedFid);
bool scriptHooks_InvenWield(Object* critter, Object* item, InvenSlot slot, int isWield, int isRemove, bool filterInactiveHand = true);
bool scriptHooks_CanUseWeapon(bool result, Object* critter, Object* weapon, HitMode hitMode);

// Hook fire functions for Phase 2
int scriptHooks_UseAnimObj(Object* object, int animId, int delay);
void scriptHooks_DescriptionObj(Object* examiner, Object* target, std::string& description);
void scriptHooks_SetLighting(Object* object, int* lightIntensityPtr, int* lightDistancePtr);

// Hook fire functions — Phase 7 (hook restoration — HOOK_STATLEVELUP, HOOK_BARTER, HOOK_MESSAGE)
void scriptHooks_StatLevelUp(Object* critter);
void scriptHooks_Barter(Object* dude, Object* npc, int mode);
void scriptHooks_Message(const char* msg);

// Hook fire functions for Phase 5 (RPU hardening)
void scriptHooks_CarTravel(int* speedPtr, int* fuelConsumptionPtr);
int scriptHooks_SetGlobalVar(int varIndex, int value);
void scriptHooks_Sneak(int* resultPtr, int* durationPtr, Object* critter);

// Hook fire functions for Phase 6 (Hooks restoration)
void scriptHooks_OnExplosion(Object* explosive, int tile, int elevation, int minDamage, int maxDamage, Object* sourceObj);
void scriptHooks_TargetObject(Object* attacker, Object* defender, int hitMode, int hitLocation);

} // namespace fallout

#endif /* FALLOUT_SFALL_SCRIPT_HOOKS_H_ */
