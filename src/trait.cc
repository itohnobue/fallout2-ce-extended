#include "trait.h"

#include <stdio.h>

#include "game.h"
#include "message.h"
#include "object.h"
#include "platform_compat.h"
#include "skill.h"
#include "sfall_config.h"
#include "sfall_metarules.h"
#include "stat.h"
#include "trait_tweak.h"

namespace fallout {

// Provides metadata about traits.
typedef struct TraitDescription {
    // The name of trait.
    char* name;

    // The description of trait.
    //
    // The description is only used in character editor to inform player about
    // effects of this trait.
    char* description;

    // Identifier of art in [intrface.lst].
    int frmId;
} TraitDescription;

// 0x66BE38 trait_message_file
static MessageList gTraitsMessageList;

// List of selected traits.
//
// 0x66BE40 pc_trait
static Trait gSelectedTraits[TRAITS_MAX_SELECTED_COUNT];

// 0x51DB84 trait_data
static TraitDescription gTraitDescriptions[TRAIT_COUNT] = {
    { nullptr, nullptr, 55 },
    { nullptr, nullptr, 56 },
    { nullptr, nullptr, 57 },
    { nullptr, nullptr, 58 },
    { nullptr, nullptr, 59 },
    { nullptr, nullptr, 60 },
    { nullptr, nullptr, 61 },
    { nullptr, nullptr, 62 },
    { nullptr, nullptr, 63 },
    { nullptr, nullptr, 64 },
    { nullptr, nullptr, 65 },
    { nullptr, nullptr, 66 },
    { nullptr, nullptr, 67 },
    { nullptr, nullptr, 94 },
    { nullptr, nullptr, 69 },
    { nullptr, nullptr, 70 },
};

// 0x4B39F0 trait_init
int traitsInit()
{
    if (!messageListInit(&gTraitsMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "trait.msg");

    if (!messageListLoad(&gTraitsMessageList, path)) {
        return -1;
    }

    for (Trait trait = TRAIT_FIRST; trait < TRAIT_COUNT; trait++) {
        MessageListItem messageListItem;

        messageListItem.num = 100 + trait;
        if (messageListGetItem(&gTraitsMessageList, &messageListItem)) {
            gTraitDescriptions[trait].name = messageListItem.text;
        }

        messageListItem.num = 200 + trait;
        if (messageListGetItem(&gTraitsMessageList, &messageListItem)) {
            gTraitDescriptions[trait].description = messageListItem.text;
        }
    }

    // NOTE: Uninline.
    traitsReset();

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_TRAIT, &gTraitsMessageList);

    // PerksFile [Traits] section (sfall PerksFile, et tu ships
    // NoHardcode rows for Night Person/Skilled): apply per-trait
    // overrides (NoHardcode flag, StatMod/SkillMod lists, Name/Desc/
    // Image). Runs after the trait.msg load so Name/Desc overrides win
    // over message texts.
    traitTweakLoad();

    return 0;
}

// 0x4B3ADC trait_reset
void traitsReset()
{
    for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
        gSelectedTraits[index] = TRAIT_INVALID;
    }
}

// 0x4B3AF8 trait_exit
void traitsExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_TRAIT, nullptr);
    messageListFree(&gTraitsMessageList);
    traitTweakFree();
}

// Loads trait system state from save game.
//
// 0x4B3B08 trait_load
int traitsLoad(File* stream)
{
    // Use runtime max count: 2 for FO2, 3 for FO1.
    // This ensures save format stays consistent with the runtime behavior.
    // Old saves (written before the 3-trait FO1 feature) always had 2 ints,
    // which matches FO2 mode's traitGetMaxSelectedCount() = 2.
    return fileReadInt32EnumList<Trait>(stream, gSelectedTraits, traitGetMaxSelectedCount());
}

// Saves trait system state to save game.
//
// 0x4B3B28 trait_save
int traitsSave(File* stream)
{
    return fileWriteInt32EnumList<Trait>(stream, gSelectedTraits, traitGetMaxSelectedCount());
}

// Sets selected traits.
// F-021: Added optional trait3 parameter (default -1) for FO1 3-trait mode.
// In FO2 mode, only trait1/trait2 are meaningful; trait3 defaults to -1.
//
// 0x4B3B48 trait_set
void traitsSetSelected(Trait trait1, Trait trait2, Trait trait3)
{
    gSelectedTraits[0] = trait1;
    gSelectedTraits[1] = trait2;
    gSelectedTraits[2] = trait3;
}

// Returns selected traits.
// F-021: Added optional trait3 output parameter (default nullptr) for FO1 3-trait mode.
// When trait3 is nullptr, only the first 2 slots are retrieved.
//
// 0x4B3B54 trait_get
void traitsGetSelected(Trait* trait1, Trait* trait2, Trait* trait3)
{
    *trait1 = gSelectedTraits[0];
    *trait2 = gSelectedTraits[1];
    if (trait3 != nullptr) {
        *trait3 = gSelectedTraits[2];
    }
}

// Returns a name of the specified trait, or `nullptr` if the specified trait is
// out of range. Returns "" if the trait is valid but its name was not loaded
// from trait.msg (prevents null dereference at call sites).
//
// 0x4B3B68 trait_name
char* traitGetName(Trait trait)
{
    if (!(trait >= 0 && trait < TRAIT_COUNT)) {
        return nullptr;
    }
    // PerksFile [Traits] Name override (sfall PerksFile support).
    if (gTraitTweak[trait].name != nullptr) {
        return gTraitTweak[trait].name;
    }
    return gTraitDescriptions[trait].name ? gTraitDescriptions[trait].name : (char*)"";
}

// Returns a description of the specified trait, or `nullptr` if the specified
// trait is out of range. Returns "" if valid but description was not loaded.
//
// 0x4B3B88 trait_description
char* traitGetDescription(Trait trait)
{
    if (!(trait >= 0 && trait < TRAIT_COUNT)) {
        return nullptr;
    }
    // PerksFile [Traits] Desc override (sfall PerksFile support).
    if (gTraitTweak[trait].description != nullptr) {
        return gTraitTweak[trait].description;
    }
    return gTraitDescriptions[trait].description ? gTraitDescriptions[trait].description : (char*)"";
}

// Return an art ID of the specified trait, or `0` if the specified trait is
// out of range.
//
// 0x4B3BA8 trait_pic
int traitGetFrmId(Trait trait)
{
    if (!traitIsValid(trait)) {
        return 0;
    }
    // PerksFile [Traits] Image override (sfall PerksFile support).
    if (gTraitTweak[trait].frmId != -1) {
        return gTraitTweak[trait].frmId;
    }
    return gTraitDescriptions[trait].frmId;
}

// Returns `true` if the specified trait is selected.
//
// 0x4B3BC8 trait_level
bool traitIsSelected(Trait trait)
{
    // F-076: In FO1 mode, check up to 3 selected traits instead of 2.
    for (int i = 0; i < traitGetMaxSelectedCount(); i++) {
        if (gSelectedTraits[i] == trait) return true;
    }
    return false;
}

// Returns `true` if the specified trait is selected and still contributes its
// hardcoded engine effects. A trait flagged NoHardcode=1 in the PerksFile
// [Traits] section contributes only its configured StatMod/SkillMod pairs, so
// every hardcoded effect outside trait_adjust_stat/trait_adjust_skill (level-up
// skill points and perk progression, combat, drug and critical-hit effects)
// must test this predicate rather than traitIsSelected().
bool traitIsSelectedAndActive(Trait trait)
{
    return traitIsValid(trait) && traitIsSelected(trait) && !traitTweakHasNoHardcode(trait);
}

// Returns stat modifier depending on selected traits.
//
// 0x4B3C7C trait_adjust_stat
int traitGetStatModifier(Stat stat)
{
    int modifier = 0;

    // PerksFile [Traits] NoHardcode (sfall PerksFile support): a trait
    // flagged NoHardcode=1 contributes none of its hardcoded effects
    // below; its configured StatMod pairs are applied at the end instead.
    switch (stat) {
    case STAT_STRENGTH:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if ((traitIsSelected(TRAIT_BRUISER) || sfallIsTraitAdded(TRAIT_BRUISER)) && !traitTweakHasNoHardcode(TRAIT_BRUISER)) {
            modifier += 2;
        }
        break;
    case STAT_PERCEPTION:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_ENDURANCE:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_CHARISMA:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_INTELLIGENCE:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_AGILITY:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if ((traitIsSelected(TRAIT_SMALL_FRAME) || sfallIsTraitAdded(TRAIT_SMALL_FRAME)) && !traitTweakHasNoHardcode(TRAIT_SMALL_FRAME)) {
            modifier += 1;
        }
        break;
    case STAT_LUCK:
        if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_MAXIMUM_ACTION_POINTS:
        if ((traitIsSelected(TRAIT_BRUISER) || sfallIsTraitAdded(TRAIT_BRUISER)) && !traitTweakHasNoHardcode(TRAIT_BRUISER)) {
            modifier -= 2;
        }
        break;
    case STAT_ARMOR_CLASS:
        if ((traitIsSelected(TRAIT_KAMIKAZE) || sfallIsTraitAdded(TRAIT_KAMIKAZE)) && !traitTweakHasNoHardcode(TRAIT_KAMIKAZE)) {
            modifier -= critterGetBaseStat(gDude, STAT_ARMOR_CLASS);
        }
        break;
    case STAT_MELEE_DAMAGE:
        if ((traitIsSelected(TRAIT_HEAVY_HANDED) || sfallIsTraitAdded(TRAIT_HEAVY_HANDED)) && !traitTweakHasNoHardcode(TRAIT_HEAVY_HANDED)) {
            modifier += 4;
        }
        break;
    case STAT_CARRY_WEIGHT:
        if ((traitIsSelected(TRAIT_SMALL_FRAME) || sfallIsTraitAdded(TRAIT_SMALL_FRAME)) && !traitTweakHasNoHardcode(TRAIT_SMALL_FRAME)) {
            modifier -= 10 * critterGetBaseStat(gDude, STAT_STRENGTH);
        }
        break;
    case STAT_SEQUENCE:
        if ((traitIsSelected(TRAIT_KAMIKAZE) || sfallIsTraitAdded(TRAIT_KAMIKAZE)) && !traitTweakHasNoHardcode(TRAIT_KAMIKAZE)) {
            modifier += 5;
        }
        break;
    case STAT_HEALING_RATE:
        if ((traitIsSelected(TRAIT_FAST_METABOLISM) || sfallIsTraitAdded(TRAIT_FAST_METABOLISM)) && !traitTweakHasNoHardcode(TRAIT_FAST_METABOLISM)) {
            modifier += 2;
        }
        break;
    case STAT_CRITICAL_CHANCE:
        if ((traitIsSelected(TRAIT_FINESSE) || sfallIsTraitAdded(TRAIT_FINESSE)) && !traitTweakHasNoHardcode(TRAIT_FINESSE)) {
            modifier += 10;
        }
        break;
    case STAT_BETTER_CRITICALS:
        if ((traitIsSelected(TRAIT_HEAVY_HANDED) || sfallIsTraitAdded(TRAIT_HEAVY_HANDED)) && !traitTweakHasNoHardcode(TRAIT_HEAVY_HANDED)) {
            modifier -= 30;
        }
        break;
    case STAT_RADIATION_RESISTANCE:
        if ((traitIsSelected(TRAIT_FAST_METABOLISM) || sfallIsTraitAdded(TRAIT_FAST_METABOLISM)) && !traitTweakHasNoHardcode(TRAIT_FAST_METABOLISM)) {
            modifier -= critterGetBaseStat(gDude, STAT_RADIATION_RESISTANCE);
        }
        break;
    case STAT_POISON_RESISTANCE:
        if ((traitIsSelected(TRAIT_FAST_METABOLISM) || sfallIsTraitAdded(TRAIT_FAST_METABOLISM)) && !traitTweakHasNoHardcode(TRAIT_FAST_METABOLISM)) {
            modifier -= critterGetBaseStat(gDude, STAT_POISON_RESISTANCE);
        }
        break;
    default:
        break;
    }

    // PerksFile [Traits] StatMod: apply the configured stat pairs for
    // every selected/added trait (sfall PerksFile support).
    for (int trait = TRAIT_FIRST; trait < TRAIT_COUNT; trait++) {
        TraitTweak* tweak = &(gTraitTweak[trait]);
        if (tweak->statModCount == 0) {
            continue;
        }
        if (!(traitIsSelected(static_cast<Trait>(trait)) || sfallIsTraitAdded(trait))) {
            continue;
        }
        for (int i = 0; i < tweak->statModCount; i++) {
            if (tweak->statMods[i].stat == stat) {
                modifier += tweak->statMods[i].mod;
            }
        }
    }

    return modifier;
}

// Returns skill modifier depending on selected traits.
//
// 0x4B40FC trait_adjust_skill
int traitGetSkillModifier(Skill skill)
{
    int modifier = 0;

    if ((traitIsSelected(TRAIT_GIFTED) || sfallIsTraitAdded(TRAIT_GIFTED)) && !traitTweakHasNoHardcode(TRAIT_GIFTED)) {
        modifier -= 10;
    }

    if ((traitIsSelected(TRAIT_GOOD_NATURED) || sfallIsTraitAdded(TRAIT_GOOD_NATURED)) && !traitTweakHasNoHardcode(TRAIT_GOOD_NATURED)) {
        switch (skill) {
        case SKILL_SMALL_GUNS:
        case SKILL_BIG_GUNS:
        case SKILL_ENERGY_WEAPONS:
        case SKILL_UNARMED:
        case SKILL_MELEE_WEAPONS:
        case SKILL_THROWING:
            modifier -= 10;
            break;
        case SKILL_FIRST_AID:
        case SKILL_DOCTOR:
        case SKILL_SPEECH:
        case SKILL_BARTER:
            modifier += 15;
            break;
        default:
            break;
        }
    }

    // PerksFile [Traits] SkillMod: apply the configured skill pairs for
    // every selected/added trait (sfall PerksFile support).
    for (int trait = TRAIT_FIRST; trait < TRAIT_COUNT; trait++) {
        TraitTweak* tweak = &(gTraitTweak[trait]);
        if (tweak->skillModCount == 0) {
            continue;
        }
        if (!(traitIsSelected(static_cast<Trait>(trait)) || sfallIsTraitAdded(trait))) {
            continue;
        }
        for (int i = 0; i < tweak->skillModCount; i++) {
            if (tweak->skillMods[i].skill == skill) {
                modifier += tweak->skillMods[i].mod;
            }
        }
    }

    return modifier;
}

// F-076: Returns the max number of traits the player can select.
// FO1 (gFallout1Behavior=true) allows 3 traits; FO2 defaults to 2.
int traitGetMaxSelectedCount()
{
    return gFallout1Behavior ? 3 : 2;
}

} // namespace fallout
