#include "botpch.h"
#include "../../playerbot.h"
#include "PartyMemberToResurrect.h"

using namespace ai;

class IsTargetOfResurrectSpell : public SpellEntryPredicate
{
public:
    virtual bool Check(SpellEntry const* spell)
    {
        for (int i=0; i<3; i++)
        {
            const SpellEffectEntry* spelleffect = spell->GetSpellEffect(SpellEffectIndex(i));
            if (spelleffect->Effect == SPELL_EFFECT_RESURRECT ||
                spelleffect->Effect == SPELL_EFFECT_RESURRECT_NEW ||
                spelleffect->Effect == SPELL_EFFECT_SELF_RESURRECT)
                return true;
        }
        return false;
    }

};

class FindDeadPlayer : public FindPlayerPredicate
{
public:
    FindDeadPlayer(PartyMemberValue* value) : value(value) {}

    virtual bool Check(Unit* unit)
    {
        Player* player = dynamic_cast<Player*>(unit);
        return player && player->GetDeathState() == CORPSE && !value->IsTargetOfSpellCast(player, predicate);
    }

private:
    PartyMemberValue* value;
    IsTargetOfResurrectSpell predicate;
};

Unit* PartyMemberToResurrect::Calculate()
{
    FindDeadPlayer finder(this);
    return FindPartyMember(finder);
}
