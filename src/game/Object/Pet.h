/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOSSERVER_PET_H
#define MANGOSSERVER_PET_H

#include "Common.h"
#include "ObjectGuid.h"
#include "Creature.h"
#include "Unit.h"

enum PetType
{
    SUMMON_PET              = 0,
    HUNTER_PET              = 1,
    GUARDIAN_PET            = 2,
    MINI_PET                = 3,
    PROTECTOR_PET           = 4,                            // work as defensive guardian with mini pet suffix in name
    MAX_PET_TYPE            = 5
};

#define MAX_PET_STABLES         4

// stored in character_pet.slot
enum PetSaveMode
{
    PET_SAVE_AS_DELETED        = -1,                        // not saved in fact
    PET_SAVE_AS_CURRENT        =  0,                        // in current slot (with player)
    PET_SAVE_FIRST_STABLE_SLOT =  1,
    PET_SAVE_LAST_STABLE_SLOT  =  MAX_PET_STABLES,          // last in DB stable slot index (including), all higher have same meaning as PET_SAVE_NOT_IN_SLOT
    PET_SAVE_NOT_IN_SLOT       =  100,                      // for avoid conflict with stable size grow will use 100
    PET_SAVE_REAGENTS          =  101                       // PET_SAVE_NOT_IN_SLOT with reagents return
};

// There might be a lot more
enum PetModeFlags
{
    PET_MODE_STAY              = 0x0000000,
    PET_MODE_FOLLOW            = 0x0000001,
    PET_MODE_ATTACK            = 0x0000002,
    PET_MODE_PASSIVE           = 0x0000000,
    PET_MODE_DEFENSIVE         = 0x0000100,
    PET_MODE_AGGRESSIVE        = 0x0000200,

    PET_MODE_DISABLE_ACTIONS   = 0x8000000,

    // autoset in client at summon
    PET_MODE_DEFAULT           = PET_MODE_FOLLOW | PET_MODE_DEFENSIVE,
};

enum PetSpellState
{
    PETSPELL_UNCHANGED = 0,
    PETSPELL_CHANGED   = 1,
    PETSPELL_NEW       = 2,
    PETSPELL_REMOVED   = 3
};

enum PetSpellType
{
    PETSPELL_NORMAL = 0,
    PETSPELL_FAMILY = 1,
};

struct PetSpell
{
    uint8 active;                                           // use instead enum (not good use *uint8* limited enum in case when value in enum not possitive in *int8*)

    PetSpellState state : 8;
    PetSpellType type   : 8;
};

enum ActionFeedback
{
    FEEDBACK_NONE            = 0,
    FEEDBACK_PET_DEAD        = 1,
    FEEDBACK_NOTHING_TO_ATT  = 2,
    FEEDBACK_CANT_ATT_TARGET = 3,
    FEEDBACK_NO_PATH         = 4,
};

enum PetTalk
{
    PET_TALK_SPECIAL_SPELL  = 0,
    PET_TALK_ATTACK         = 1
};

enum PetNameInvalidReason
{
    // custom, not send
    PET_NAME_SUCCESS                                        = 0,

    PET_NAME_INVALID                                        = 1,
    PET_NAME_NO_NAME                                        = 2,
    PET_NAME_TOO_SHORT                                      = 3,
    PET_NAME_TOO_LONG                                       = 4,
    PET_NAME_MIXED_LANGUAGES                                = 6,
    PET_NAME_PROFANE                                        = 7,
    PET_NAME_RESERVED                                       = 8,
    PET_NAME_THREE_CONSECUTIVE                              = 11,
    PET_NAME_INVALID_SPACE                                  = 12,
    PET_NAME_CONSECUTIVE_SPACES                             = 13,
    PET_NAME_RUSSIAN_CONSECUTIVE_SILENT_CHARACTERS          = 14,
    PET_NAME_RUSSIAN_SILENT_CHARACTER_AT_BEGINNING_OR_END   = 15,
    PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME              = 16
};

typedef std::unordered_map<uint32, PetSpell> PetSpellMap;
typedef std::vector<uint32> AutoSpellList;

#define HAPPINESS_LEVEL_SIZE        333000

#define ACTIVE_SPELLS_MAX           4

#define PET_FOLLOW_DIST  1.0f
#define PET_FOLLOW_ANGLE (M_PI_F / 4.00f) * 3.50f

class Player;

class Pet : public Creature
{
    public:
        explicit Pet(PetType type = MAX_PET_TYPE);
        virtual ~Pet();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        PetType getPetType() const { return m_petType; }
        void setPetType(PetType type) { m_petType = type; }
        bool isControlled() const { return getPetType() == SUMMON_PET || getPetType() == HUNTER_PET; }
        bool isTemporarySummoned() const { return m_duration > 0; }

        bool Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number);
        bool CreateBaseAtCreature(Creature* creature);
        bool LoadPetFromDB(Player* owner, uint32 petentry = 0, uint32 petnumber = 0, bool current = false);
        void SavePetToDB(PetSaveMode mode);
        void Unsummon(PetSaveMode mode, Unit* owner = NULL);

        static void DeleteFromDB(uint32 guidlow, bool separate_transaction = true);

        void SetDeathState(DeathState s) override;          // overwrite virtual Creature::SetDeathState and Unit::SetDeathState
        void Update(uint32 update_diff, uint32 diff) override;  // overwrite virtual Creature::Update and Unit::Update

        uint8 GetPetAutoSpellSize() const override { return m_autospells.size(); }
        uint32 GetPetAutoSpellOnPos(uint8 pos) const override
        {
            if (pos >= m_autospells.size())
            {
                return 0;
            }
            else
            {
                return m_autospells[pos];
            }
        }

        bool CanSwim() const override
        {
            Unit const* owner = GetOwner();
            if (owner)
            {
                return owner->GetTypeId() == TYPEID_PLAYER ? true : ((Creature const*)owner)->CanSwim();
            }
            else
            {
                return Creature::CanSwim();
            }
        }

        void RegenerateAll(uint32 update_diff) override;    // overwrite Creature::RegenerateAll
        void GivePetXP(uint32 xp);
        void GivePetLevel(uint32 level);
        void SynchronizeLevelWithOwner();
        void InitStatsForLevel(uint32 level);
        bool HaveInDiet(ItemPrototype const* item) const;
        uint32 GetCurrentFoodBenefitLevel(uint32 itemlevel);
        void SetDuration(int32 dur) { m_duration = dur; }
        int32 GetDuration() { return m_duration; }

        int32 GetBonusDamage() { return m_bonusdamage; }
        void SetBonusDamage(int32 damage) { m_bonusdamage = damage; }

        bool UpdateStats(Stats stat) override;
        bool UpdateAllStats() override;
        void UpdateResistances(uint32 school) override;
        void UpdateArmor() override;
        void UpdateMaxHealth() override;
        void UpdateMaxPower(Powers power) override;
        void UpdateAttackPowerAndDamage(bool ranged = false) override;
        void UpdateDamagePhysical(WeaponAttackType attType) override;
        uint32 GetCurrentEquipmentId() const { return m_equipmentId; }

        bool CanTakeMoreActiveSpells(uint32 SpellIconID);
        void ToggleAutocast(uint32 spellid, bool apply);

        void SetModeFlags(PetModeFlags mode);
        void ApplyModeFlags(PetModeFlags mode, bool apply);
        PetModeFlags GetModeFlags() const { return m_petModeFlags; }

        bool HasSpell(uint32 spell) const override;

        void LearnPetPassives();
        void CastPetAuras(bool current);
        void CastOwnerTalentAuras();
        void CastPetAura(PetAura const* aura);

        void _LoadSpellCooldowns();
        void _SaveSpellCooldowns();
        void _LoadAuras(uint32 timediff);
        void _SaveAuras();
        void _LoadSpells();
        void _SaveSpells();

        bool addSpell(uint32 spell_id, ActiveStates active = ACT_DECIDE, PetSpellState state = PETSPELL_NEW, PetSpellType type = PETSPELL_NORMAL);
        bool learnSpell(uint32 spell_id);
        void learnSpellHighRank(uint32 spellid);
        void InitLevelupSpellsForLevel();
        bool unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        bool removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab = true);
        void CleanupActionBar();

        bool m_retreating;

        void SetIsRetreating(bool retreating = false) { m_retreating = retreating; }
        bool GetIsRetreating() { return m_retreating; }

        bool m_stayPosSet;
        float m_stayPosX;
        float m_stayPosY;
        float m_stayPosZ;
        float m_stayPosO;

        void SetStayPosition(bool stay = false);
        bool IsStayPosSet() { return m_stayPosSet; }

        float GetStayPosX() { return m_stayPosX; }
        float GetStayPosY() { return m_stayPosY; }
        float GetStayPosZ() { return m_stayPosZ; }
        float GetStayPosO() { return m_stayPosO; }

        PetSpellMap     m_spells;
        AutoSpellList   m_autospells;

        uint32 m_opener;
        uint32 m_openerMinRange;
        uint32 m_openerMaxRange;

        uint32 GetSpellOpener()         { return m_opener; }
        uint32 GetSpellOpenerMinRange() { return m_openerMinRange; }
        uint32 GetSpellOpenerMaxRange() { return m_openerMaxRange; }

        void SetSpellOpener(uint32 spellId = 0, uint32 minRange = 0, uint32 maxRange = 0)
        {
            m_opener = spellId;
            m_openerMinRange = minRange;
            m_openerMaxRange = maxRange;
        }

        void InitPetCreateSpells();

        bool resetTalents(bool no_cost = false);
        static void resetTalentsForAllPetsOf(Player* owner, Pet* online_pet = NULL);
        uint32 resetTalentsCost() const;
        void InitTalentForLevel();

        uint8 GetMaxTalentPointsForLevel(uint32 level);
        uint8 GetFreeTalentPoints() { return GetByteValue(UNIT_FIELD_BYTES_1, 1); }
        void SetFreeTalentPoints(uint8 points) { SetByteValue(UNIT_FIELD_BYTES_1, 1, points); }
        void UpdateFreeTalentPoints(bool resetIfNeed = true);

        uint32  m_resetTalentsCost;
        time_t  m_resetTalentsTime;
        uint32  m_usedTalentCount;

        const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }
        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }
        void ResetAuraUpdateMask() { m_auraUpdateMask = 0; }

        // overwrite Creature function for name localization back to WorldObject version without localization
        const char* GetNameForLocaleIdx(int32 locale_idx) const override { return WorldObject::GetNameForLocaleIdx(locale_idx); }

        DeclinedName const* GetDeclinedNames() const { return m_declinedname; }

        bool    m_removed;                                  // prevent overwrite pet state in DB at next Pet::Update if pet already removed(saved)
    protected:
        PetType m_petType;
        int32   m_duration;                                 // time until unsummon (used mostly for summoned guardians and not used for controlled pets)
        int32   m_bonusdamage;
        uint64  m_auraUpdateMask;
        bool    m_loading;

        DeclinedName* m_declinedname;

    private:
        PetModeFlags m_petModeFlags;

        void SaveToDB(uint32, uint8, uint32) override       // overwrite of Creature::SaveToDB     - don't must be called
        {
            MANGOS_ASSERT(false);
        }
        void DeleteFromDB() override                        // overwrite of Creature::DeleteFromDB - don't must be called
        {
            MANGOS_ASSERT(false);
        }
};
#endif
