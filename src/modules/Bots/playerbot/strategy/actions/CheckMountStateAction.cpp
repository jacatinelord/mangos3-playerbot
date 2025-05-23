#include "botpch.h"
#include "../../playerbot.h"
#include "CheckMountStateAction.h"

using namespace ai;

uint64 extractGuid(WorldPacket& packet);

bool CheckMountStateAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!bot->GetGroup() || !master)
 {
     return false;
 }

    if (bot->IsTaxiFlying())
 {
     return false;
 }

    if (master->IsMounted() && !bot->IsMounted())
    {
        return Mount();
    }
    else if (!master->IsMounted() && bot->IsMounted())
    {
        WorldPacket emptyPacket;
        bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
        return true;
    }
    return false;
}

bool CheckMountStateAction::Mount()
{
    Player* master = GetMaster();
    ai->RemoveShapeshift();

    Unit::AuraList const& auras = master->GetAurasByType(SPELL_AURA_MOUNTED);
    if (auras.empty()) return false;

    const SpellEntry* const masterSpell = auras.front()->GetSpellProto();
    const SpellEffectEntry* const spelleffect1 = masterSpell->GetSpellEffect(SpellEffectIndex(1));
    const SpellEffectEntry* const spelleffect2 = masterSpell->GetSpellEffect(SpellEffectIndex(2));

    int32 masterSpeed = max(spelleffect1->EffectBasePoints, spelleffect2->EffectBasePoints);

    map<int32, vector<uint32> > spells;
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
  {
      continue;
  }

        const SpellEntry* const spellInfo = sSpellStore.LookupEntry(spellId);
        uint32 auraType = spellInfo->GetEffectApplyAuraNameByIndex(SpellEffectIndex(0));
        if (!spellInfo || auraType != SPELL_AURA_MOUNTED)
  {
      continue;
  }
        const SpellEffectEntry* const spelleffect1_1 = spellInfo->GetSpellEffect(SpellEffectIndex(1));
        const SpellEffectEntry* const spelleffect2_2 = spellInfo->GetSpellEffect(SpellEffectIndex(2));
        int32 effect = max(spelleffect1_1->EffectBasePoints, spelleffect2_2->EffectBasePoints);
        if (effect < masterSpeed)
  {
      continue;
  }

        spells[effect].push_back(spellId);
    }

    for (map<int32, vector<uint32> >::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        vector<uint32>& ids = i->second;
        int index = urand(0, ids.size() - 1);
        if (index >= ids.size())
  {
      continue;
  }

        ai->CastSpell(ids[index], bot);
        return true;
    }

    return false;
}
