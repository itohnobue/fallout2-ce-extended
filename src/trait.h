#ifndef TRAIT_H
#define TRAIT_H

#include "db.h"
#include "skill_defs.h"
#include "stat_defs.h"
#include "trait_defs.h"

namespace fallout {

int traitsInit();
void traitsReset();
void traitsExit();
int traitsLoad(File* stream);
int traitsSave(File* stream);
void traitsSetSelected(Trait trait1, Trait trait2, Trait trait3 = TRAIT_INVALID);
void traitsGetSelected(Trait* trait1, Trait* trait2, Trait* trait3 = nullptr);
char* traitGetName(Trait trait);
char* traitGetDescription(Trait trait);
int traitGetFrmId(Trait trait);
bool traitIsSelected(Trait trait);
// Returns `true` if the trait is selected and still contributes its
// hardcoded engine effects (PerksFile [Traits] NoHardcode=1 disables them).
bool traitIsSelectedAndActive(Trait trait);
int traitGetStatModifier(Stat stat);
int traitGetSkillModifier(Skill skill);

// Returns the max number of traits the player can select.
// FO1 (gFallout1Behavior=true) allows 3; FO2 defaults to 2.
int traitGetMaxSelectedCount();

} // namespace fallout

#endif /* TRAIT_H */
