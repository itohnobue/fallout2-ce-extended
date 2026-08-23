#include "perk.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "debug.h"
#include "dictionary.h"
#include "game.h"
#include "memory.h"
#include "message.h"
#include "object.h"
#include "party_member.h"
#include "perk_tweak.h"
#include "platform_compat.h"
#include "skill.h"
#include "sfall_opcodes.h"
#include "stat.h"

namespace fallout {

enum PerkParamMode {
    PERK_PARAM_MODE_FIRST_ONLY,
    PERK_PARAM_MODE_OR,
    PERK_PARAM_MODE_AND,
};

typedef struct PerkDescription {
    char* name;
    char* description;
    int frmId;
    int maxRank;
    int minLevel;
    // Critter stat to modify for every perk rank.
    Stat stat;
    // Stat modifier for every perk rank.
    int statModifier;
    // Skill number, normally. If bit 0x4000000 is set, will be treated as global var number instead.
    int param1;
    // Required value of a skill or global var.
    int value1;
    // Specifies wether to require both params, either one or just use the first one.
    int paramMode;
    // Skill or gvar number, see param1.
    int param2;
    // Required value of a skill or global var.
    int value2;
    // Required minimum value for every primary stat.
    int stats[PRIMARY_STAT_COUNT];
} PerkDescription;

typedef struct PerkRankData {
    int ranks[PERK_COUNT];
} PerkRankData;

static PerkRankData* perkGetRankData(Object* critter);
static bool perkCanAdd(Object* critter, Perk perk);
static void perkResetRanks();

// 0x519DCC perk_data
static PerkDescription gPerkDescriptions[PERK_COUNT] = {
    { nullptr, nullptr, 72, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 5, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 73, 1, 15, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 74, 3, 3, STAT_MELEE_DAMAGE, 2, -1, 0, 0, -1, 0, 6, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 75, 2, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 76, 2, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 6 },
    { nullptr, nullptr, 77, 1, 15, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 6, 7, 0 },
    { nullptr, nullptr, 78, 3, 3, STAT_SEQUENCE, 2, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 79, 3, 3, STAT_HEALING_RATE, 2, -1, 0, 0, -1, 0, 0, 0, 6, 0, 0, 0, 0 },
    { nullptr, nullptr, 80, 3, 6, STAT_CRITICAL_CHANCE, 5, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 6 },
    { nullptr, nullptr, 81, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 82, 3, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 6, 0, 0, 0 },
    { nullptr, nullptr, 83, 2, 6, STAT_RADIATION_RESISTANCE, 15, -1, 0, 0, -1, 0, 0, 0, 6, 0, 4, 0, 0 },
    { nullptr, nullptr, 84, 3, 3, STAT_DAMAGE_RESISTANCE, 10, -1, 0, 0, -1, 0, 0, 0, 6, 0, 0, 0, 6 },
    { nullptr, nullptr, 85, 3, 3, STAT_CARRY_WEIGHT, 50, -1, 0, 0, -1, 0, 6, 0, 6, 0, 0, 0, 0 },
    { nullptr, nullptr, 86, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 6, 0, 0 },
    { nullptr, nullptr, 87, 1, 6, STAT_INVALID, 0, 8, 50, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 88, 1, 3, STAT_INVALID, 0, 17, 40, 0, -1, 0, 0, 0, 6, 0, 6, 0, 0 },
    { nullptr, nullptr, 89, 1, 12, STAT_INVALID, 0, 15, 75, 0, -1, 0, 0, 0, 0, 7, 0, 0, 0 },
    { nullptr, nullptr, 90, 3, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 6, 0, 0 },
    { nullptr, nullptr, 91, 2, 3, STAT_INVALID, 0, 6, 40, 0, -1, 0, 0, 7, 0, 0, 5, 6, 0 },
    { nullptr, nullptr, 92, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 8 },
    { nullptr, nullptr, 93, 1, 9, STAT_BETTER_CRITICALS, 20, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 4, 6 },
    { nullptr, nullptr, 94, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 5, 0, 0 },
    { nullptr, nullptr, 95, 1, 24, STAT_INVALID, 0, 3, 80, 0, -1, 0, 8, 0, 0, 0, 0, 8, 0 },
    { nullptr, nullptr, 96, 1, 24, STAT_INVALID, 0, 0, 80, 0, -1, 0, 0, 8, 0, 0, 0, 8, 0 },
    { nullptr, nullptr, 97, 1, 18, STAT_INVALID, 0, 8, 80, 2, 3, 80, 0, 0, 0, 0, 0, 10, 0 },
    { nullptr, nullptr, 98, 2, 12, STAT_MAXIMUM_ACTION_POINTS, 1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 99, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 100, 2, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 4, 0, 0, 0, 0 },
    { nullptr, nullptr, 101, 1, 9, STAT_ARMOR_CLASS, 5, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 6, 0 },
    { nullptr, nullptr, 102, 2, 6, STAT_POISON_RESISTANCE, 25, -1, 0, 0, -1, 0, 0, 0, 3, 0, 0, 0, 0 },
    { nullptr, nullptr, 103, 1, 12, STAT_INVALID, 0, 13, 40, 1, 12, 40, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 104, 1, 12, STAT_INVALID, 0, 6, 40, 1, 7, 40, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 105, 1, 12, STAT_INVALID, 0, 10, 50, 2, 9, 50, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 106, 1, 9, STAT_INVALID, 0, 14, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 107, 3, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, -9, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 108, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 4, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 109, 1, 15, STAT_INVALID, 0, 10, 80, 0, -1, 0, 0, 0, 0, 0, 0, 8, 0 },
    { nullptr, nullptr, 110, 1, 6, STAT_INVALID, 0, 8, 60, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 111, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 10, 0, 0, 0 },
    { nullptr, nullptr, 112, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 8 },
    { nullptr, nullptr, 113, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 114, 1, 310, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 0, 0, 0 },
    { nullptr, nullptr, 115, 2, 6, STAT_INVALID, 0, 17, 40, 0, -1, 0, 0, 0, 6, 0, 0, 0, 0 },
    { nullptr, nullptr, 116, 1, 310, STAT_INVALID, 0, 17, 25, 0, -1, 0, 0, 0, 0, 0, 5, 0, 0 },
    { nullptr, nullptr, 117, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 7, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 118, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 4 },
    { nullptr, nullptr, 119, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 120, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 121, 3, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 4, 0, 0 },
    { nullptr, nullptr, 122, 3, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 4, 0, 0 },
    { nullptr, nullptr, 123, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 124, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 125, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 126, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, -2, 0, -2, 0, 0, -3, 0 },
    { nullptr, nullptr, 127, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -3, -2, 0 },
    { nullptr, nullptr, 128, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -2, 0, 0 },
    { nullptr, nullptr, 129, -1, 1, STAT_RADIATION_RESISTANCE, -20, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 130, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 131, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 132, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 133, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 134, -1, 1, STAT_RADIATION_RESISTANCE, 30, -1, 0, 0, -1, 0, 3, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 135, -1, 1, STAT_RADIATION_RESISTANCE, 20, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 136, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 137, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 138, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 139, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 140, -1, 1, STAT_RADIATION_RESISTANCE, 60, -1, 0, 0, -1, 0, 4, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 141, -1, 1, STAT_RADIATION_RESISTANCE, 75, -1, 0, 0, -1, 0, 4, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 136, -1, 1, STAT_MAXIMUM_ACTION_POINTS, -1, -1, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 149, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, -2, 0, 0, -1, 0, -1 },
    { nullptr, nullptr, 154, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 2, 0, 0, 0 },
    { nullptr, nullptr, 158, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 157, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 157, -1, 1, STAT_CHARISMA, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 168, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 168, -1, 1, STAT_CHARISMA, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 172, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 155, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, -10, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 156, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 6, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 122, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 6, 0, 0 },
    { nullptr, nullptr, 39, 1, 9, STAT_INVALID, 0, 11, 75, 0, -1, 0, 0, 0, 0, 0, 0, 4, 0 },
    { nullptr, nullptr, 44, 1, 6, STAT_INVALID, 0, 16, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 0, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, -10, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 1, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, -10, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 2, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, -10, 0, 0, 0, 0 },
    { nullptr, nullptr, 3, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, -10, 0, 0, 0 },
    { nullptr, nullptr, 4, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -10, 0, 0 },
    { nullptr, nullptr, 5, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, -10, 0 },
    { nullptr, nullptr, 6, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -10 },
    { nullptr, nullptr, 160, 1, 6, STAT_INVALID, 0, 10, 50, 2, 0x4000000, 50, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 161, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 159, 1, 12, STAT_INVALID, 0, 3, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 163, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 5, 0, 0, 5, 0 },
    { nullptr, nullptr, 162, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 6, 0, 0, 0 },
    { nullptr, nullptr, 164, 1, 9, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 5 },
    { nullptr, nullptr, 165, 1, 12, STAT_INVALID, 0, 7, 60, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 166, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, -10, 0, 0, 0 },
    { nullptr, nullptr, 43, 1, 6, STAT_INVALID, 0, 15, 50, 2, 14, 50, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 167, 1, 6, STAT_CARRY_WEIGHT, 50, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 169, 1, 9, STAT_INVALID, 0, 1, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 170, 1, 6, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 121, 1, 6, STAT_INVALID, 0, 15, 50, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 171, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 6, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 38, 1, 3, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 173, 1, 12, STAT_INVALID, 0, -1, 0, 0, -1, 0, -7, 0, 0, 0, 0, 5, 0 },
    { nullptr, nullptr, 104, -1, 1, STAT_INVALID, 0, 7, 75, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 142, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 142, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 52, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 52, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 104, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 104, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 35, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 35, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 154, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 154, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { nullptr, nullptr, 64, -1, 1, STAT_INVALID, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0 },
};

// An array of perk ranks for each party member.
//
// 0x51C120 perkLevelDataList
static PerkRankData* gPartyMemberPerkRanks = nullptr;

// Amount of experience points granted when player selected "Here and now"
// perk.
//
// 0x51C124 hereAndNowExps
static int hereAndNowBonusExperience = 0;

// perk.msg
//
// 0x6642D4 perk_message_file
static MessageList gPerksMessageList;

// Name/description strings allocated from the Perks.ini [Perks] section
// (Name=/Desc= overrides). The perk descriptions' name/description pointers
// normally reference strings owned by gPerksMessageList (freed at exit);
// these file-supplied strings are owned here and freed in perksExit().
static char* gPerksFileNameOverrides[PERK_COUNT] = {};
static char* gPerksFileDescOverrides[PERK_COUNT] = {};

// Perks.ini [Perks] section: apply per-perk description overrides.
//
// Mirrors sfall's PerksFile [Perks] section semantics:
//   - gated by Enable=1 in the [Perks] section
//   - each [ID] block overrides fields of that perk's description
//     (Name, Desc, Image, Ranks, Level, Type, Skill1/Skill1Mag,
//     Skill2/Skill2Mag, Stat/StatMag, STR..LCK)
//   - a field value of -99999 means "ignore this field" (keep current)
//   - Ranks=-1 removes the perk (maxRank=-1 -> perkCanAdd returns false)
//   - extra perk IDs (119+) exceed CE's PERK_COUNT table and are skipped
//
// Script opcodes (set_perk_*) still take precedence: perkCanAdd checks
// the override arrays before the description fields, and scripts run
// after init. Values persist across game resets (perkResetRanks only
// clears rank data, not descriptions).
//
// The [Traits] section (NoHardcode/StatMod/SkillMod) is handled by
// trait_tweak.cc/trait.cc (see traitTweakLoad).
static void perksLoadCustomConfig()
{
    char* perksFile = perkTweakGetPerksFilePath();
    if (perksFile == nullptr) {
        return;
    }

    ScopedConfig config { perksFile, false };
    if (!config) {
        return;
    }

    int enable = 0;
    if (!configGetInt(config.get(), "Perks", "Enable", &enable, 0) || enable == 0) {
        return;
    }

    Config* perksConfig = config.get();
    for (int i = 0; i < perksConfig->entriesLength; i++) {
        DictionaryEntry* entry = &(perksConfig->entries[i]);
        const char* sectionName = entry->key;

        // Perk ID sections are plain numbers (e.g. "1", "9"). Skip the
        // named sections (Perks/PerksTweak/Traits/...) and anything that
        // is not a numeric perk ID.
        if (sectionName[0] < '0' || sectionName[0] > '9') {
            continue;
        }

        // Strict parse: the whole section name must be consumed by strtol
        // (a section named "1abc" would otherwise silently retarget perk 1).
        // Leading whitespace is skipped by strtol; trailing whitespace is
        // tolerated here to match the config reader's section-name trimming.
        char* end = nullptr;
        errno = 0;
        long perkIdLong = strtol(sectionName, &end, 10);
        while (end != sectionName && isspace(static_cast<unsigned char>(*end))) {
            end++;
        }
        if (end == sectionName || *end != '\0' || errno == ERANGE || perkIdLong < 0 || perkIdLong > INT_MAX) {
            debugPrint("Perks config: malformed perk section '[%s]' ignored\n", sectionName);
            continue;
        }

        int perkID = static_cast<int>(perkIdLong);
        if (!perkIsValid(static_cast<Perk>(perkID))) {
            continue;
        }

        PerkDescription* description = &(gPerkDescriptions[perkID]);

        char* stringValue = nullptr;
        if (configGetString(perksConfig, sectionName, "Name", &stringValue)) {
            if (gPerksFileNameOverrides[perkID] != nullptr) {
                internal_free(gPerksFileNameOverrides[perkID]);
            }
            gPerksFileNameOverrides[perkID] = internal_strdup(stringValue);
            description->name = gPerksFileNameOverrides[perkID];
        }

        if (configGetString(perksConfig, sectionName, "Desc", &stringValue)) {
            if (gPerksFileDescOverrides[perkID] != nullptr) {
                internal_free(gPerksFileDescOverrides[perkID]);
            }
            gPerksFileDescOverrides[perkID] = internal_strdup(stringValue);
            description->description = gPerksFileDescOverrides[perkID];
        }

        // -99999 sentinel: field is ignored (current value kept).
        struct PerkFileField {
            const char* key;
            int* target;
        };

        PerkFileField fields[] = {
            { "Image", &description->frmId },
            { "Ranks", &description->maxRank },
            { "Level", &description->minLevel },
            { "Type", &description->paramMode },
            { "StatMag", &description->statModifier },
            { "Skill1", &description->param1 },
            { "Skill1Mag", &description->value1 },
            { "Skill2", &description->param2 },
            { "Skill2Mag", &description->value2 },
            { "STR", &description->stats[STAT_STRENGTH] },
            { "PER", &description->stats[STAT_PERCEPTION] },
            { "END", &description->stats[STAT_ENDURANCE] },
            { "CHR", &description->stats[STAT_CHARISMA] },
            { "INT", &description->stats[STAT_INTELLIGENCE] },
            { "AGL", &description->stats[STAT_AGILITY] },
            { "LCK", &description->stats[STAT_LUCK] },
        };

        for (size_t f = 0; f < sizeof(fields) / sizeof(fields[0]); f++) {
            int value = 0;
            if (configGetInt(perksConfig, sectionName, fields[f].key, &value) && value != -99999) {
                *fields[f].target = value;
            }
        }

        // Stat is an enum (Stat) — read into a temp int and convert
        // explicitly instead of aliasing through (int*)&description->stat.
        int statValue = 0;
        if (configGetInt(perksConfig, sectionName, "Stat", &statValue) && statValue != -99999) {
            description->stat = static_cast<Stat>(statValue);
        }
    }
}

// 0x4965A0 perk_init
int perksInit()
{
    gPartyMemberPerkRanks = (PerkRankData*)internal_malloc(sizeof(*gPartyMemberPerkRanks) * gPartyMemberDescriptionsLength);
    if (gPartyMemberPerkRanks == nullptr) {
        return -1;
    }

    perkResetRanks();

    if (!messageListInit(&gPerksMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "perk.msg");

    if (!messageListLoad(&gPerksMessageList, path)) {
        return -1;
    }

    for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
        MessageListItem messageListItem;

        messageListItem.num = 101 + perk;
        if (messageListGetItem(&gPerksMessageList, &messageListItem)) {
            gPerkDescriptions[perk].name = messageListItem.text;
        }

        messageListItem.num = 1101 + perk;
        if (messageListGetItem(&gPerksMessageList, &messageListItem)) {
            gPerkDescriptions[perk].description = messageListItem.text;
        }
    }

    // SFALL: Validate that all perk names were loaded (F-018).
    // perkGetName() returns "" for missing names (to protect downstream callers
    // from nullptr), defeating the null-check in proto.cc:1407. This explicit
    // validation catches truncated perk.msg files during initialization.
    for (int perk = 0; perk < PERK_COUNT; perk++) {
        if (gPerkDescriptions[perk].name == nullptr) {
            debugPrint("\nError: Initing perks: missing perk name!");
            return -1;
        }
    }

    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_PERK, &gPerksMessageList);

    // PerksFile support (sfall PerksFile config, et tu ships
    // PerksFile=config\Perks.ini): apply [PerksTweak] bonus values and
    // the [Perks] section description overrides. Runs after the perk.msg
    // load so Name/Desc overrides win over message texts.
    perkTweakLoad();
    perksLoadCustomConfig();

    return 0;
}

// 0x4966B0 perk_reset
void perksReset()
{
    perkResetRanks();
}

// 0x4966B8 perk_exit
void perksExit()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_PERK, nullptr);
    messageListFree(&gPerksMessageList);

    for (int perk = 0; perk < PERK_COUNT; perk++) {
        if (gPerksFileNameOverrides[perk] != nullptr) {
            internal_free(gPerksFileNameOverrides[perk]);
            gPerksFileNameOverrides[perk] = nullptr;
        }
        if (gPerksFileDescOverrides[perk] != nullptr) {
            internal_free(gPerksFileDescOverrides[perk]);
            gPerksFileDescOverrides[perk] = nullptr;
        }
    }

    if (gPartyMemberPerkRanks != nullptr) {
        internal_free(gPartyMemberPerkRanks);
        gPartyMemberPerkRanks = nullptr;
    }
}

// 0x4966E4 perk_load
int perksLoad(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
            if (fileReadInt32(stream, &(ranksData->ranks[perk])) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

// 0x496738 perk_save
int perksSave(File* stream)
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
            if (fileWriteInt32(stream, ranksData->ranks[perk]) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

// perkGetLevelData
// 0x49678C perkGetLevelData
static PerkRankData* perkGetRankData(Object* critter)
{
    if (critter == gDude) {
        return gPartyMemberPerkRanks;
    }

    for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
        if (critter->pid == gPartyMemberPids[index]) {
            return gPartyMemberPerkRanks + index;
        }
    }

    debugPrint("\nError: perkGetLevelData: Can't find party member match!");

    return nullptr;
}

// 0x49680C perk_can_add
static bool perkCanAdd(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return false;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    // Check sfall script-level ranks override (set_perk_ranks opcode 0x8179).
    int maxRankOverride = sfallGetPerkRanksOverride(perk);
    int effectiveMaxRank = (maxRankOverride != -1) ? maxRankOverride : perkDescription->maxRank;

    if (effectiveMaxRank == -1) {
        return false;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData != nullptr && ranksData->ranks[perk] >= effectiveMaxRank) {
        return false;
    }

    if (critter == gDude) {
        if (pcGetStat(PC_STAT_LEVEL) < perkDescription->minLevel) {
            return false;
        }

        if (perk == PERK_HERE_AND_NOW && pcGetExperienceForLevel(pcGetStat(PC_STAT_LEVEL) + 1) == -1) {
            return false;
        }
    }

    bool req1Fulfilled = true;

    int param1 = perkDescription->param1;
    // Check sfall script-level skill1 override (set_perk_skill1 opcode 0x8181).
    int skill1Override = sfallGetPerkSkill1Override(perk);
    if (skill1Override != -1) {
        param1 = skill1Override;
    }
    if (param1 != -1) {
        bool isVariable = false;
        if ((param1 & 0x4000000) != 0) {
            isVariable = true;
            param1 &= ~0x4000000;
        }

        int value1 = perkDescription->value1;
        // Check sfall script-level skill1 magnitude override (set_perk_skill1_mag opcode 0x8182).
        int skill1MagOverride = sfallGetPerkSkill1MagOverride(perk);
        if (skill1MagOverride != -1000) {
            value1 = skill1MagOverride;
        }
        if (value1 < 0) {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param1)) >= value1) {
                    req1Fulfilled = false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param1)) >= -value1) {
                    req1Fulfilled = false;
                }
            }
        } else {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param1)) < value1) {
                    req1Fulfilled = false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param1)) < value1) {
                    req1Fulfilled = false;
                }
            }
        }
    }

    if (!req1Fulfilled || perkDescription->paramMode == PERK_PARAM_MODE_AND) {
        if (perkDescription->paramMode == PERK_PARAM_MODE_FIRST_ONLY) {
            return false;
        }

        if (!req1Fulfilled && perkDescription->paramMode == PERK_PARAM_MODE_AND) {
            return false;
        }

        int param2 = perkDescription->param2;
        // Check sfall script-level skill2 override (set_perk_skill2 opcode 0x8183).
        int skill2Override = sfallGetPerkSkill2Override(perk);
        if (skill2Override != -1) {
            param2 = skill2Override;
        }
        bool isVariable = false;
        if (param2 != -1) {
            if ((param2 & 0x4000000) != 0) {
                isVariable = true;
                param2 &= ~0x4000000;
            }
        }

        if (param2 == -1) {
            return false;
        }

        int value2 = perkDescription->value2;
        // Check sfall script-level skill2 magnitude override (set_perk_skill2_mag opcode 0x8184).
        int skill2MagOverride = sfallGetPerkSkill2MagOverride(perk);
        if (skill2MagOverride != -1000) {
            value2 = skill2MagOverride;
        }
        if (value2 < 0) {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param2)) >= value2) {
                    return false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param2)) >= -value2) {
                    return false;
                }
            }
        } else {
            if (isVariable) {
                if (gameGetGlobalVar(static_cast<GameGlobalVar>(param2)) < value2) {
                    return false;
                }
            } else {
                if (skillGetValue(critter, static_cast<Skill>(param2)) < value2) {
                    return false;
                }
            }
        }
    }

    for (Stat stat = STAT_FIRST; stat < PRIMARY_STAT_COUNT; stat++) {
        int statReq = perkDescription->stats[stat];
        // Check sfall script-level special override (set_perk_special opcode 0x8188).
        int specialOverride = sfallGetPerkSpecialOverride(perk, stat);
        if (specialOverride != -1) {
            statReq = specialOverride;
        }
        if (statReq < 0) {
            if (critterGetStat(critter, stat) >= -statReq) {
                return false;
            }
        } else {
            if (critterGetStat(critter, stat) < statReq) {
                return false;
            }
        }
    }

    return true;
}

// Resets party member perks.
//
// 0x496A0C perk_defaults
static void perkResetRanks()
{
    for (int index = 0; index < gPartyMemberDescriptionsLength; index++) {
        PerkRankData* ranksData = &(gPartyMemberPerkRanks[index]);
        for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
            ranksData->ranks[perk] = 0;
        }
    }
}

// 0x496A5C perk_add
int perkAdd(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    if (!perkCanAdd(critter, perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData == nullptr) {
        return -1;
    }

    ranksData->ranks[perk] += 1;

    perkAddEffect(critter, perk);

    return 0;
}

// perk_add_force
// 0x496A9C perk_add_force
int perkAddForce(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData == nullptr) {
        return -1;
    }

    int value = ranksData->ranks[perk];

    // Check sfall script-level ranks override (set_perk_ranks opcode 0x8179).
    int ranksOverride = sfallGetPerkRanksOverride(perk);
    int maxRank = (ranksOverride != -1) ? ranksOverride : gPerkDescriptions[perk].maxRank;

    if (maxRank != -1 && value >= maxRank) {
        return -1;
    }

    ranksData->ranks[perk] += 1;

    perkAddEffect(critter, perk);

    return 0;
}

// perk_sub
// 0x496AFC perk_sub
int perkRemove(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData == nullptr) {
        return -1;
    }

    int value = ranksData->ranks[perk];

    if (value < 1) {
        return -1;
    }

    ranksData->ranks[perk] -= 1;

    perkRemoveEffect(critter, perk);

    return 0;
}

// Returns perks available to pick.
//
// 0x496B44 perk_make_list
int perkGetAvailablePerks(Object* critter, Perk* perks)
{
    int count = 0;
    for (Perk perk = PERK_FIRST; perk < PERK_COUNT; perk++) {
        if (perkCanAdd(critter, perk)) {
            perks[count] = perk;
            count++;
        }
    }
    return count;
}

// has_perk
// 0x496B78 perk_level
int perkGetRank(Object* critter, Perk perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }

    PerkRankData* ranksData = perkGetRankData(critter);
    if (ranksData == nullptr) {
        return 0;
    }

    return ranksData->ranks[perk];
}

// 0x496B90 perk_name
// Returns the name of the specified perk, or nullptr if the perk index is invalid.
// Returns "" (empty string) if the perk is valid but its name was not loaded
// (e.g., due to a truncated perk.msg file). This prevents null pointer
// dereference at the many call sites that pass the result directly to
// snprintf/strcmp/strcpy/fontDrawText without null checks.
char* perkGetName(Perk perk)
{
    if (!perkIsValid(perk)) {
        return nullptr;
    }
    // F2-041: Check sfall script-level name override (set_perk_name opcode 0x8189).
    const char* nameOverride = sfallGetPerkNameOverride(perk);
    if (nameOverride != nullptr) {
        return const_cast<char*>(nameOverride);
    }
    return gPerkDescriptions[perk].name ? gPerkDescriptions[perk].name : (char*)"";
}

// 0x496BB4 perk_description
// Returns the description of the specified perk, or nullptr if the perk index
// is invalid. Returns "" if valid but description was not loaded.
char* perkGetDescription(Perk perk)
{
    if (!perkIsValid(perk)) {
        return nullptr;
    }
    // F2-041: Check sfall script-level description override (set_perk_desc opcode 0x818A).
    const char* descOverride = sfallGetPerkDescOverride(perk);
    if (descOverride != nullptr) {
        return const_cast<char*>(descOverride);
    }
    return gPerkDescriptions[perk].description ? gPerkDescriptions[perk].description : (char*)"";
}

// 0x496BD8 perk_skilldex_fid
int perkGetFrmId(Perk perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }
    // F2-041: Check sfall script-level image override (set_perk_image opcode 0x8178).
    int imageOverride = sfallGetPerkImageOverride(perk);
    if (imageOverride != -1) {
        return imageOverride;
    }
    return gPerkDescriptions[perk].frmId;
}

// Sets the minimum level requirement for a perk.
// Used by set_perk_level sfall opcode (0x817A).
// F-008: Saves the original compile-time minLevel on first override
// so it can be restored on game reset.
void perkSetMinLevel(int perk, int minLevel)
{
    if (!perkIsValid(perk)) {
        return;
    }
    if (sfallPerkMinLevelOriginal[perk] == -1) {
        sfallPerkMinLevelOriginal[perk] = gPerkDescriptions[perk].minLevel;
    }
    gPerkDescriptions[perk].minLevel = minLevel;
}

// Returns the minimum level requirement for a perk.
int perkGetMinLevel(int perk)
{
    if (!perkIsValid(perk)) {
        return 0;
    }
    return gPerkDescriptions[perk].minLevel;
}

// Returns the maximum rank for a perk, or -1 if the perk has no ranks.
int perkGetMaxRank(int perk)
{
    if (!perkIsValid(perk)) {
        return -1;
    }
    // Check sfall script-level ranks override (set_perk_ranks opcode 0x8179).
    int ranksOverride = sfallGetPerkRanksOverride(perk);
    if (ranksOverride != -1) {
        return ranksOverride;
    }
    return gPerkDescriptions[perk].maxRank;
}

// perk_add_effect
// 0x496BFC perk_add_effect
void perkAddEffect(Object* critter, Perk perk)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perk_add_effect: Was called on non-critter!");
        return;
    }

    if (!perkIsValid(perk)) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    // Check sfall script-level stat override (set_perk_stat opcode 0x8185).
    int statOverride = sfallGetPerkStatOverride(perk);
    int effectiveStat = (statOverride != -1000) ? statOverride : perkDescription->stat;

    if (effectiveStat != -1) {
        // Check sfall script-level stat magnitude override (set_perk_stat_mag opcode 0x8186).
        int statMagOverride = sfallGetPerkStatMagOverride(perk);
        int effectiveModifier = (statMagOverride != -1000) ? statMagOverride : perkDescription->statModifier;

        int value = critterGetBonusStat(critter, static_cast<Stat>(effectiveStat));
        critterSetBonusStat(critter, static_cast<Stat>(effectiveStat), value + effectiveModifier);
    }

    if (perk == PERK_HERE_AND_NOW) {
        PerkRankData* ranksData = perkGetRankData(critter);
        if (ranksData == nullptr) {
            return;
        }

        ranksData->ranks[PERK_HERE_AND_NOW] -= 1;

        int level = pcGetStat(PC_STAT_LEVEL);
        int nextLevelExperience = pcGetExperienceForLevel(level + 1);

        hereAndNowBonusExperience = nextLevelExperience >= 0 ? nextLevelExperience - pcGetStat(PC_STAT_EXPERIENCE) : 0;
        pcAddExperienceWithOptions(hereAndNowBonusExperience, false);

        ranksData->ranks[PERK_HERE_AND_NOW] += 1;
    }

    if (perkDescription->maxRank == -1) {
        for (Stat stat = STAT_FIRST; stat < PRIMARY_STAT_COUNT; stat++) {
            int value = critterGetBonusStat(critter, stat);
            critterSetBonusStat(critter, stat, value + perkDescription->stats[stat]);
        }
    }
}

// perk_remove_effect
// 0x496CE0 perk_remove_effect
void perkRemoveEffect(Object* critter, Perk perk)
{
    if (objectTypeFromPid(critter->pid) != OBJ_TYPE_CRITTER) {
        debugPrint("\nERROR: perk_remove_effect: Was called on non-critter!");
        return;
    }

    if (!perkIsValid(perk)) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);

    // Check sfall script-level stat override (set_perk_stat opcode 0x8185).
    int statOverride = sfallGetPerkStatOverride(perk);
    int effectiveStat = (statOverride != -1000) ? statOverride : perkDescription->stat;

    if (effectiveStat != -1) {
        // Check sfall script-level stat magnitude override (set_perk_stat_mag opcode 0x8186).
        int statMagOverride = sfallGetPerkStatMagOverride(perk);
        int effectiveModifier = (statMagOverride != -1000) ? statMagOverride : perkDescription->statModifier;

        int value = critterGetBonusStat(critter, static_cast<Stat>(effectiveStat));
        critterSetBonusStat(critter, static_cast<Stat>(effectiveStat), value - effectiveModifier);
    }

    if (perk == PERK_HERE_AND_NOW) {
        int xp = pcGetStat(PC_STAT_EXPERIENCE);
        pcSetStat(PC_STAT_EXPERIENCE, xp - hereAndNowBonusExperience);
    }

    if (perkDescription->maxRank == -1) {
        for (Stat stat = STAT_FIRST; stat < PRIMARY_STAT_COUNT; stat++) {
            int value = critterGetBonusStat(critter, stat);
            critterSetBonusStat(critter, stat, value - perkDescription->stats[stat]);
        }
    }
}

// Returns modifier to specified skill accounting for perks.
//
// 0x496DD0 perk_adjust_skill
int perkGetSkillModifier(Object* critter, Skill skill)
{
    int modifier = 0;

    switch (skill) {
    case SKILL_FIRST_AID:
        if (perkHasRank(critter, PERK_MEDIC)) {
            modifier += gPerkTweak.medicFirstAidBonus;
        }

        if (perkHasRank(critter, PERK_VAULT_CITY_TRAINING)) {
            modifier += gPerkTweak.vaultCityTrainingFirstAidBonus;
        }

        break;
    case SKILL_DOCTOR:
        if (perkHasRank(critter, PERK_MEDIC)) {
            modifier += gPerkTweak.medicDoctorBonus;
        }

        if (perkHasRank(critter, PERK_LIVING_ANATOMY)) {
            modifier += gPerkTweak.livingAnatomyDoctorBonus;
        }

        if (perkHasRank(critter, PERK_VAULT_CITY_TRAINING)) {
            modifier += gPerkTweak.vaultCityTrainingDoctorBonus;
        }

        break;
    case SKILL_SNEAK:
        if (perkHasRank(critter, PERK_GHOST)) {
            int lightIntensity = objectGetLightIntensity(gDude);
            if (lightIntensity <= 45875) {
                modifier += gPerkTweak.ghostBonus;
            }
        }
        // FALLTHROUGH
    case SKILL_LOCKPICK:
    case SKILL_STEAL:
    case SKILL_TRAPS:
        if (perkHasRank(critter, PERK_THIEF)) {
            modifier += gPerkTweak.thiefBonus;
        }

        if (skill == SKILL_LOCKPICK || skill == SKILL_STEAL) {
            if (perkHasRank(critter, PERK_MASTER_THIEF)) {
                modifier += gPerkTweak.masterThiefBonus;
            }
        }

        if (skill == SKILL_STEAL) {
            if (perkHasRank(critter, PERK_HARMLESS)) {
                modifier += gPerkTweak.harmlessBonus;
            }
        }

        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (perkHasRank(critter, PERK_MR_FIXIT)) {
            modifier += gPerkTweak.mrFixitBonus;
        }

        break;
    case SKILL_SPEECH:
        if (perkHasRank(critter, PERK_SPEAKER)) {
            modifier += gPerkTweak.speakerBonus;
        }

        if (perkHasRank(critter, PERK_EXPERT_EXCREMENT_EXPEDITOR)) {
            modifier += gPerkTweak.expertExcrementExpeditorBonus;
        }

        // FALLTHROUGH
    case SKILL_BARTER:
        if (perkHasRank(critter, PERK_NEGOTIATOR)) {
            modifier += gPerkTweak.negotiatorBonus;
        }

        if (skill == SKILL_BARTER) {
            if (perkHasRank(critter, PERK_SALESMAN)) {
                modifier += gPerkTweak.salesmanBonus;
            }
        }

        break;
    case SKILL_GAMBLING:
        if (perkHasRank(critter, PERK_GAMBLER)) {
            modifier += gPerkTweak.gamblerBonus;
        }

        break;
    case SKILL_OUTDOORSMAN:
        if (perkHasRank(critter, PERK_RANGER)) {
            modifier += gPerkTweak.rangerOutdoorsmanBonus;
        }

        if (perkHasRank(critter, PERK_SURVIVALIST)) {
            modifier += gPerkTweak.survivalistBonus;
        }

        break;
    default:
        break;
    }

    return modifier;
}

} // namespace fallout
