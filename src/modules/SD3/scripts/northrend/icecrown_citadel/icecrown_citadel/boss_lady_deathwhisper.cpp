/**
 * ScriptDev3 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2014-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/* ScriptData
SDName: boss_lady_deathwhisper
SD%Complete: 95%
SDComment: Minor adjustments may be required
SDCategory: Icecrown Citadel
EndScriptData */

#include "precompiled.h"
#include "icecrown_citadel.h"

enum
{
    // yells
    SAY_AGGRO                   = -1631018,
    SAY_PHASE_TWO               = -1631019,
    SAY_DARK_EMPOWERMENT        = -1631020,
    SAY_DARK_TRANSFORMATION     = -1631021,
    SAY_ANIMATE_DEAD            = -1631022,
    SAY_DOMINATE_MIND           = -1631023,
    SAY_BERSERK                 = -1631024,
    SAY_DEATH                   = -1631025,
    SAY_SLAY_1                  = -1631026,
    SAY_SLAY_2                  = -1631027,

    // spells - phase 1
    SPELL_SHADOW_CHANNELING     = 43897,
    SPELL_MANA_BARRIER          = 70842,
    SPELL_SHADOW_BOLT           = 71254,

    // phase 2
    SPELL_INSIGNIFICANCE        = 71204,
    SPELL_FROSTBOLT             = 71420,
    SPELL_FROSTBOLT_VOLLEY      = 72905,
    SPELL_SUMMON_SPIRIT         = 71363,            // triggers 71426

    // common
    SPELL_BERSERK               = 26662,
    SPELL_DOMINATE_MIND         = 71289,
    SPELL_DEATH_AND_DECAY       = 71001,
    SPELL_DARK_EMPOWERMENT      = 70896,            // dummy - triggers 70901 - only on Adherents - transforms target into 38136
    SPELL_DARK_TRANSFORMATION   = 70895,            // dummy - triggers 70900 - only on Fanatics - transforms target into 38135
    SPELL_DARK_MARTYRDOM        = 70897,            // dummy - triggers 70903 on Adherents or 71236 on Fanatics
    // SPELL_SUMMON_ADHERENT     = 70820,            // cast by the stalkers - only server side
    // SPELL_SUMMON_FANATIC      = 70819,            // cast by the stalkers - only server side

    // npcs
    NPC_VENGEFUL_SHADE          = 38222,            // has aura 71494
};

struct boss_lady_deathwhisper : public CreatureScript
{
    boss_lady_deathwhisper() : CreatureScript("boss_lady_deathwhisper") {}

    struct boss_lady_deathwhisperAI : public ScriptedAI
    {
        boss_lady_deathwhisperAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        }

        ScriptedInstance* m_pInstance;

        bool m_bIsPhaseOne;
        bool m_bIsLeftSideSummon;

        uint32 m_uiBerserkTimer;
        uint32 m_uiSummonWaveTimer;
        uint32 m_uiCultistBuffTimer;
        uint32 m_uiDarkMartyrdomTimer;
        uint32 m_uiTouchOfInsignificanceTimer;
        uint32 m_uiShadowBoltTimer;
        uint32 m_uiDeathAndDecayTimer;
        uint32 m_uiFrostboltTimer;
        uint32 m_uiFrostboltVolleyTimer;
        uint32 m_uiDominateMindTimer;
        uint32 m_uiVengefulShadeTimer;

        uint8 m_uiMindControlCount;

        GuidList m_lCultistSpawnedGuidList;

        void Reset() override
        {
            m_bIsPhaseOne = true;
            m_bIsLeftSideSummon = roll_chance_i(50) ? true : false;
            m_uiBerserkTimer = 10 * MINUTE * IN_MILLISECONDS;
            m_uiSummonWaveTimer = 10000;
            m_uiCultistBuffTimer = 0;
            m_uiDarkMartyrdomTimer = 30000;
            m_uiTouchOfInsignificanceTimer = 7000;
            m_uiShadowBoltTimer = 2000;
            m_uiDeathAndDecayTimer = urand(10000, 15000);
            m_uiFrostboltTimer = urand(5000, 10000);
            m_uiFrostboltVolleyTimer = 5000;
            m_uiDominateMindTimer = urand(30000, 45000);
            m_uiVengefulShadeTimer = 10000;
            m_uiMindControlCount = 0;

            SetCombatMovement(false);
            DoCastSpellIfCan(m_creature, SPELL_SHADOW_CHANNELING);

            // Set the max allowed mind control targets
            if (m_pInstance)
            {
                if (m_pInstance->GetData(TYPE_DATA_IS_25MAN))
                {
                    m_uiMindControlCount = m_pInstance->GetData(TYPE_DATA_IS_HEROIC) ? 3 : 1;
                }
                else
                {
                    m_uiMindControlCount = m_pInstance->GetData(TYPE_DATA_IS_HEROIC) ? 1 : 0;
                }
            }
        }

        void Aggro(Unit* /*pWho*/) override
        {
            DoScriptText(SAY_AGGRO, m_creature);
            DoCastSpellIfCan(m_creature, SPELL_MANA_BARRIER, CAST_TRIGGERED);

            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_LADY_DEATHWHISPER, IN_PROGRESS);

                // Sort the summoning stalkers: now in SetData(IN_PROGRESS)
            }
        }

        void KilledUnit(Unit* pVictim) override
        {
            if (pVictim->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            if (urand(0, 1))
            {
                DoScriptText(urand(0, 1) ? SAY_SLAY_1 : SAY_SLAY_2, m_creature);
            }
        }

        void JustDied(Unit* /*pKiller*/) override
        {
            DoScriptText(SAY_DEATH, m_creature);

            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_LADY_DEATHWHISPER, DONE);
            }
        }

        void JustReachedHome() override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_LADY_DEATHWHISPER, FAIL);
            }
        }

        void JustSummoned(Creature* pSummoned) override
        {
            switch (pSummoned->GetEntry())
            {
            case NPC_CULT_ADHERENT:
            case NPC_CULT_FANATIC:
                m_lCultistSpawnedGuidList.push_back(pSummoned->GetObjectGuid());
                break;
            }

            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                pSummoned->AI()->AttackStart(pTarget);
            }
        }

        void SummonedCreatureJustDied(Creature* pSummoned) override
        {
            if (pSummoned->GetEntry() != NPC_VENGEFUL_SHADE)
            {
                m_lCultistSpawnedGuidList.remove(pSummoned->GetObjectGuid());
            }
        }

        // Wrapper to select a random cultist
        Creature* DoSelectRandomCultist(uint32 uiEntry = 0)
        {
            std::vector<Creature*> vCultists;
            vCultists.reserve(m_lCultistSpawnedGuidList.size());

            for (GuidList::const_iterator itr = m_lCultistSpawnedGuidList.begin(); itr != m_lCultistSpawnedGuidList.end(); ++itr)
            {
                if (Creature* pCultist = m_creature->GetMap()->GetCreature(*itr))
                {
                    // Allow to be sorted them by entry
                    if (!uiEntry)
                    {
                        vCultists.push_back(pCultist);
                    }
                    else if (pCultist->GetEntry() == uiEntry)
                    {
                        vCultists.push_back(pCultist);
                    }
                }
            }

            if (vCultists.empty())
            {
                return nullptr;
            }

            return vCultists[urand(0, vCultists.size() - 1)];
        }

        // Wrapper to handle the second phase start
        void ReceiveAIEvent(AIEventType eventType, Creature*, Unit*, uint32) override
        {
            if (eventType == AI_EVENT_CUSTOM_A)
            {
                DoScriptText(SAY_PHASE_TWO, m_creature);
                SetCombatMovement(true);
                m_creature->GetMotionMaster()->MoveChase(m_creature->getVictim());
                m_bIsPhaseOne = false;
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            if (m_uiBerserkTimer)
            {
                if (m_uiBerserkTimer <= uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                    {
                        DoScriptText(SAY_BERSERK, m_creature);
                        m_uiBerserkTimer = 0;
                    }
                }
                else
                {
                    m_uiBerserkTimer -= uiDiff;
                }
            }

            if (m_uiDeathAndDecayTimer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_DEATH_AND_DECAY) == CAST_OK)
                    {
                        m_uiDeathAndDecayTimer = 20000;
                    }
                }
            }
            else
            {
                m_uiDeathAndDecayTimer -= uiDiff;
            }

            if (m_uiMindControlCount)
            {
                if (m_uiDominateMindTimer < uiDiff)
                {
                    for (uint8 i = 0; i < m_uiMindControlCount; ++i)
                    {
                        if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, SPELL_DOMINATE_MIND, SELECT_FLAG_PLAYER))
                        {
                            DoCastSpellIfCan(pTarget, SPELL_DOMINATE_MIND, CAST_TRIGGERED);
                        }
                    }

                    DoScriptText(SAY_DOMINATE_MIND, m_creature);
                    m_uiDominateMindTimer = 40000;
                }
                else
                {
                    m_uiDominateMindTimer -= uiDiff;
                }
            }

            // Summon waves - in phase 1 or on heroic
            if (m_pInstance && (m_pInstance->GetData(TYPE_DATA_IS_HEROIC) || m_bIsPhaseOne))
            {
                if (m_uiSummonWaveTimer < uiDiff)
                {
                    m_pInstance->SetData(TYPE_DO_SUMMON_CULTIST_WAVE, uint32(m_bIsPhaseOne));
                    m_uiCultistBuffTimer = 10000;
                    m_uiDarkMartyrdomTimer = 40000;
                    m_uiSummonWaveTimer = m_pInstance->GetData(TYPE_DATA_IS_HEROIC) ? 45000 : 60000;
                }
                else
                {
                    m_uiSummonWaveTimer -= uiDiff;
                }

                if (m_uiCultistBuffTimer)
                {
                    if (m_uiCultistBuffTimer <= uiDiff)
                    {
                        // Choose a random of Fanatic or Adherent
                        bool bIsFanatic = roll_chance_i(50) ? true : false;
                        uint32 uiNpcEntry = bIsFanatic ? NPC_CULT_FANATIC : NPC_CULT_ADHERENT;
                        uint32 uiSpellEntry = bIsFanatic ? SPELL_DARK_TRANSFORMATION : SPELL_DARK_EMPOWERMENT;
                        int32 iTextEntry = bIsFanatic ? SAY_DARK_TRANSFORMATION : SAY_DARK_EMPOWERMENT;

                        Creature* pTarget = DoSelectRandomCultist(uiNpcEntry);
                        if (pTarget && DoCastSpellIfCan(pTarget, uiSpellEntry) == CAST_OK)
                        {
                            // Remove the selected cultist from the list because we don't want it selected twice
                            m_lCultistSpawnedGuidList.remove(pTarget->GetObjectGuid());
                            DoScriptText(iTextEntry, m_creature);
                            m_uiCultistBuffTimer = 0;
                        }
                    }
                    else
                    {
                        m_uiCultistBuffTimer -= uiDiff;
                    }
                }

                if (m_uiDarkMartyrdomTimer)
                {
                    if (m_uiDarkMartyrdomTimer <= uiDiff)
                    {
                        // Try to get a target on which to cast Martyrdom
                        if (Creature* pTarget = DoSelectRandomCultist())
                        {
                            if (DoCastSpellIfCan(pTarget, SPELL_DARK_MARTYRDOM) == CAST_OK)
                            {
                                DoScriptText(SAY_ANIMATE_DEAD, m_creature);
                                m_uiDarkMartyrdomTimer = 0;
                            }
                        }
                        else
                        {
                            m_uiDarkMartyrdomTimer = 0;
                        }
                    }
                    else
                    {
                        m_uiDarkMartyrdomTimer -= uiDiff;
                    }
                }
            }

            // Phase 1 specific spells
            if (m_bIsPhaseOne)
            {
                if (m_uiShadowBoltTimer < uiDiff)
                {
                    if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    {
                        if (DoCastSpellIfCan(pTarget, SPELL_SHADOW_BOLT) == CAST_OK)
                        {
                            m_uiShadowBoltTimer = 2000;
                        }
                    }
                }
                else
                {
                    m_uiShadowBoltTimer -= uiDiff;
                }
            }
            // Phase 2 specific spells
            else
            {
                if (m_uiTouchOfInsignificanceTimer < uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_INSIGNIFICANCE) == CAST_OK)
                    {
                        m_uiTouchOfInsignificanceTimer = 7000;
                    }
                }
                else
                {
                    m_uiTouchOfInsignificanceTimer -= uiDiff;
                }

                if (m_uiFrostboltTimer < uiDiff)
                {
                    if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    {
                        if (DoCastSpellIfCan(pTarget, SPELL_FROSTBOLT) == CAST_OK)
                        {
                            m_uiFrostboltTimer = urand(2000, 4000);
                        }
                    }
                }
                else
                {
                    m_uiFrostboltTimer -= uiDiff;
                }

                if (m_uiFrostboltVolleyTimer < uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_FROSTBOLT_VOLLEY) == CAST_OK)
                    {
                        m_uiFrostboltVolleyTimer = urand(15000, 20000);
                    }
                }
                else
                {
                    m_uiFrostboltVolleyTimer -= uiDiff;
                }

                if (m_uiVengefulShadeTimer < uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_SUMMON_SPIRIT) == CAST_OK)
                    {
                        m_uiVengefulShadeTimer = 10000;
                    }
                }
                else
                {
                    m_uiVengefulShadeTimer -= uiDiff;
                }

                DoMeleeAttackIfReady();
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new boss_lady_deathwhisperAI(pCreature);
    }
};

struct spell_mana_barrier : public SpellScript
{
    spell_mana_barrier() : SpellScript("spell_mana_barrier") {}

    bool EffectDummy(Unit* /*pCaster*/, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        // always check spellid and effectindex
        if (uiSpellId == SPELL_MANA_BARRIER && uiEffIndex == EFFECT_INDEX_0)
        {
            Creature* pCreatureTarget = pTarget->ToCreature();
            uint32 uiDamage = pCreatureTarget->GetMaxHealth() - pCreatureTarget->GetHealth();
            if (!uiDamage)
            {
                return true;
            }

            if (pCreatureTarget->GetPower(POWER_MANA) < uiDamage)
            {
                uiDamage = pCreatureTarget->GetPower(POWER_MANA);
                pCreatureTarget->RemoveAurasDueToSpell(SPELL_MANA_BARRIER);

                if (CreatureAI* pBossAI = pCreatureTarget->AI())
                {
                    pBossAI->ReceiveAIEvent(AI_EVENT_CUSTOM_A, (Creature*)nullptr, (Unit*)nullptr, 0);
                }
            }

            pCreatureTarget->DealHeal(pCreatureTarget, uiDamage, GetSpellStore()->LookupEntry(SPELL_MANA_BARRIER));
            pCreatureTarget->ModifyPower(POWER_MANA, -int32(uiDamage));

            // always return true when we are handling this spell and effect
            return true;
        }

        return false;
    }
};

void AddSC_boss_lady_deathwhisper()
{
    Script* s;

    s = new boss_lady_deathwhisper();
    s->RegisterSelf();

    s = new spell_mana_barrier();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "boss_lady_deathwhisper";
    //pNewScript->GetAI = &GetAI_boss_lady_deathwhisper;
    //pNewScript->pEffectDummyNPC = &EffectDummyCreature_spell_mana_barrier;
    //pNewScript->RegisterSelf();
}
