#include "botpch.h"
#include "../../playerbot.h"
#include "PartyMemberToHeal.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

class IsTargetOfHealingSpell : public SpellEntryPredicate
{
public:
    virtual bool Check(SpellEntry const* spell) {
        for (int i=0; i<3; i++) {
            const SpellEffectEntry* spelleffect = spell->GetSpellEffect(SpellEffectIndex(i));
            if (spelleffect->Effect == SPELL_EFFECT_HEAL ||
                spelleffect->Effect == SPELL_EFFECT_HEAL_MAX_HEALTH ||
                spelleffect->Effect == SPELL_EFFECT_HEAL_MECHANICAL)
                return true;
        }
        return false;
    }

};

Unit* PartyMemberToHeal::Calculate()
{

    IsTargetOfHealingSpell predicate;

    Group* group = bot->GetGroup();
    if (!group)
    {
        return NULL;
    }

    bool isRaid = bot->GetGroup()->isRaidGroup();
    MinValueCalculator calc(100);
    for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->getSource();
        if (!Check(player) || !player->IsAlive())
        {
            continue;
        }

        uint8 health = player->GetHealthPercent();
        if (isRaid || health < sPlayerbotAIConfig.mediumHealth || !IsTargetOfSpellCast(player, predicate))
        {
            calc.probe(health, player);
        }

        Pet* pet = player->GetPet();
        if (pet && CanHealPet(pet))
        {
            health = pet->GetHealthPercent();
            if (isRaid || health < sPlayerbotAIConfig.mediumHealth || !IsTargetOfSpellCast(player, predicate))
            {
                calc.probe(health, player);
            }
        }
    }
    return (Unit*)calc.param;
}

bool PartyMemberToHeal::CanHealPet(Pet* pet)
{
    return MINI_PET != pet->getPetType();
}
