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

#ifndef MANGOS_H_OBJECTMGR
#define MANGOS_H_OBJECTMGR

#include "Common.h"
#include "Log.h"
#include "Object.h"
#include "Bag.h"
#include "Creature.h"
#include "Player.h"
#include "GameObject.h"
#include "Corpse.h"
#include "QuestDef.h"
#include "ItemPrototype.h"
#include "NPCHandler.h"
#include "Database/DatabaseEnv.h"
#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"

#include <string>
#include <map>
#include <limits>

class Group;
class ArenaTeam;
class Item;
class SQLStorage;
struct PhaseDefinition;
struct SpellPhaseInfo;

typedef std::list<PhaseDefinition*> PhaseDefinitionContainer;
typedef std::unordered_map<uint32 /*zoneId*/, PhaseDefinitionContainer> PhaseDefinitionStore;

typedef std::unordered_map<uint32 /*spellId*/, SpellPhaseInfo*> SpellPhaseStore;

struct GameTele
{
    float  position_x;
    float  position_y;
    float  position_z;
    float  orientation;
    uint32 mapId;
    std::string name;
    std::wstring wnameLow;
};

typedef std::unordered_map<uint32, GameTele > GameTeleMap;

struct SpellClickInfo
{
    uint32 spellId;
    uint32 questStart;                                      // quest start (quest must be active or rewarded for spell apply)
    uint32 questEnd;                                        // quest end (quest don't must be rewarded for spell apply)
    bool   questStartCanActive;                             // if true then quest start can be active (not only rewarded)
    uint8 castFlags;
    uint16 conditionId;                                     // intends to replace questStart, questEnd, questStartCanActive

    // helpers
    bool IsFitToRequirements(Player const* player, Creature const* clickedCreature) const;
};

typedef std::multimap<uint32 /*npcEntry*/, SpellClickInfo> SpellClickInfoMap;
typedef std::pair<SpellClickInfoMap::const_iterator, SpellClickInfoMap::const_iterator> SpellClickInfoMapBounds;

struct AreaTrigger
{
    uint8  requiredLevel;
    uint32 requiredItem;
    uint32 requiredItem2;
    uint32 heroicKey;
    uint32 heroicKey2;
    uint32 requiredQuest;
    uint32 requiredQuestHeroic;
    uint32 target_mapId;
    float  target_X;
    float  target_Y;
    float  target_Z;
    float  target_Orientation;

    // Operators
    bool IsMinimal() const
    {
        return requiredLevel == 0 && requiredItem == 0 && requiredItem2 == 0 && heroicKey == 0 &&
               heroicKey2 == 0 && requiredQuest == 0 && requiredQuestHeroic == 0;
    }

    bool IsLessOrEqualThan(AreaTrigger const* l) const      // Expected to have same map
    {
        MANGOS_ASSERT(target_mapId == l->target_mapId);
        return requiredLevel <= l->requiredLevel && requiredItem <= l->requiredItem && requiredItem2 <= l->requiredItem2
               && heroicKey <= l->heroicKey && heroicKey2 <= l->heroicKey2 && requiredQuest <= l->requiredQuest && requiredQuestHeroic <= l->requiredQuestHeroic;
    }
};

typedef std::map < uint32/*player guid*/, uint32/*instance*/ > CellCorpseSet;
struct CellObjectGuids
{
    CellGuidSet creatures;
    CellGuidSet gameobjects;
    CellCorpseSet corpses;
};
typedef std::unordered_map < uint32/*cell_id*/, CellObjectGuids > CellObjectGuidsMap;
typedef std::unordered_map < uint32/*(mapid,spawnMode) pair*/, CellObjectGuidsMap > MapObjectGuids;

// mangos string ranges
#define MIN_MANGOS_STRING_ID           1                    // 'mangos_string'
#define MAX_MANGOS_STRING_ID           2000000000
#define MIN_DB_SCRIPT_STRING_ID        MAX_MANGOS_STRING_ID // 'db_script_string'
#define MAX_DB_SCRIPT_STRING_ID        2001000000
#define MIN_CREATURE_AI_TEXT_STRING_ID (-1)                 // 'creature_ai_texts'
#define MAX_CREATURE_AI_TEXT_STRING_ID (-1000000)
// Anything below MAX_CREATURE_AI_TEXT_STRING_ID is handled by the external script lib

static_assert(MAX_DB_SCRIPT_STRING_ID < ACE_INT32_MAX, "Must scope with int32 range");

struct MangosStringLocale
{
    MangosStringLocale() : SoundId(0), Type(0), LanguageId(LANG_UNIVERSAL), Emote(0) { }

    std::vector<std::string> Content;                       // 0 -> default, i -> i-1 locale index
    uint32 SoundId;
    uint8  Type;
    Language LanguageId;
    uint32 Emote;                       // 0 -> default, i -> i-1 locale index
};

typedef std::unordered_map<uint32, CreatureData> CreatureDataMap;
typedef CreatureDataMap::value_type CreatureDataPair;

class FindCreatureData
{
    public:
        FindCreatureData(uint32 id, Player* player) : i_id(id), i_player(player),
            i_anyData(NULL), i_mapData(NULL), i_mapDist(0.0f), i_spawnedData(NULL), i_spawnedDist(0.0f) {}

        bool operator()(CreatureDataPair const& dataPair);
        CreatureDataPair const* GetResult() const;

    private:
        uint32 i_id;
        Player* i_player;

        CreatureDataPair const* i_anyData;
        CreatureDataPair const* i_mapData;
        float i_mapDist;
        CreatureDataPair const* i_spawnedData;
        float i_spawnedDist;
};

typedef std::unordered_map<uint32, GameObjectData> GameObjectDataMap;
typedef GameObjectDataMap::value_type GameObjectDataPair;

class FindGOData
{
    public:
        FindGOData(uint32 id, Player* player) : i_id(id), i_player(player),
            i_anyData(NULL), i_mapData(NULL), i_mapDist(0.0f), i_spawnedData(NULL), i_spawnedDist(0.0f) {}

        bool operator()(GameObjectDataPair const& dataPair);
        GameObjectDataPair const* GetResult() const;

    private:
        uint32 i_id;
        Player* i_player;

        GameObjectDataPair const* i_anyData;
        GameObjectDataPair const* i_mapData;
        float i_mapDist;
        GameObjectDataPair const* i_spawnedData;
        float i_spawnedDist;
};

typedef std::unordered_map<uint32, CreatureLocale> CreatureLocaleMap;
typedef std::unordered_map<uint32, GameObjectLocale> GameObjectLocaleMap;
typedef std::unordered_map<uint32, ItemLocale> ItemLocaleMap;
typedef std::unordered_map<uint32, QuestLocale> QuestLocaleMap;
typedef std::unordered_map<uint32, NpcTextLocale> NpcTextLocaleMap;
typedef std::unordered_map<uint32, PageTextLocale> PageTextLocaleMap;
typedef std::unordered_map<int32, MangosStringLocale> MangosStringLocaleMap;
typedef std::unordered_map<uint32, GossipMenuItemsLocale> GossipMenuItemsLocaleMap;
typedef std::unordered_map<uint32, PointOfInterestLocale> PointOfInterestLocaleMap;
typedef std::unordered_map<uint32, uint32> ItemConvertMap;

typedef std::multimap<int32, uint32> ExclusiveQuestGroupsMap;
typedef std::multimap<uint32, ItemRequiredTarget> ItemRequiredTargetMap;
typedef std::multimap<uint32, uint32> QuestRelationsMap;
typedef std::pair<ExclusiveQuestGroupsMap::const_iterator, ExclusiveQuestGroupsMap::const_iterator> ExclusiveQuestGroupsMapBounds;
typedef std::pair<ItemRequiredTargetMap::const_iterator, ItemRequiredTargetMap::const_iterator> ItemRequiredTargetMapBounds;
typedef std::pair<QuestRelationsMap::const_iterator, QuestRelationsMap::const_iterator> QuestRelationsMapBounds;

struct PetLevelInfo
{
    PetLevelInfo() : health(0), mana(0), armor(0)
    {
        for (int i = 0; i < MAX_STATS; ++i) stats[i] = 0;
    }

    uint16 stats[MAX_STATS];
    uint16 health;
    uint16 mana;
    uint16 armor;
};

struct MailLevelReward
{
    MailLevelReward() : raceMask(0), mailTemplateId(0), senderEntry(0) {}
    MailLevelReward(uint32 _raceMask, uint32 _mailTemplateId, uint32 _senderEntry) : raceMask(_raceMask), mailTemplateId(_mailTemplateId), senderEntry(_senderEntry) {}

    uint32 raceMask;
    uint32 mailTemplateId;
    uint32 senderEntry;
};

typedef std::list<MailLevelReward> MailLevelRewardList;
typedef std::unordered_map<uint8, MailLevelRewardList> MailLevelRewardMap;

// We assume the rate is in general the same for all three types below, but chose to keep three for scalability and customization
struct RepRewardRate
{
    float quest_rate;                                       // We allow rate = 0.0 in database. For this case,
    float creature_rate;                                    // it means that no reputation are given at all
    float spell_rate;                                       // for this faction/rate type.
};

struct ReputationOnKillEntry
{
    uint32 repfaction1;
    uint32 repfaction2;
    bool is_teamaward1;
    uint32 reputation_max_cap1;
    int32 repvalue1;
    bool is_teamaward2;
    uint32 reputation_max_cap2;
    int32 repvalue2;
    bool team_dependent;
};

struct RepSpilloverTemplate
{
    uint32 faction[MAX_SPILLOVER_FACTIONS];
    float faction_rate[MAX_SPILLOVER_FACTIONS];
    uint32 faction_rank[MAX_SPILLOVER_FACTIONS];
};

struct PointOfInterest
{
    uint32 entry;
    float x;
    float y;
    uint32 icon;
    uint32 flags;
    uint32 data;
    std::string icon_name;
};

struct GossipMenuItems
{
    uint32          menu_id;
    uint32          id;
    uint8           option_icon;
    std::string     option_text;
    uint32          option_id;
    uint32          npc_option_npcflag;
    int32           action_menu_id;
    uint32          action_poi_id;
    uint32          action_script_id;
    bool            box_coded;
    uint32          box_money;
    std::string     box_text;
    uint16          conditionId;
};

struct GossipMenus
{
    uint32          entry;
    uint32          text_id;
    uint32          script_id;
    uint16          conditionId;
};

typedef std::multimap<uint32, GossipMenus> GossipMenusMap;
typedef std::pair<GossipMenusMap::const_iterator, GossipMenusMap::const_iterator> GossipMenusMapBounds;
typedef std::multimap<uint32, GossipMenuItems> GossipMenuItemsMap;
typedef std::pair<GossipMenuItemsMap::const_iterator, GossipMenuItemsMap::const_iterator> GossipMenuItemsMapBounds;

struct QuestPOIPoint
{
    int32 x;
    int32 y;

    QuestPOIPoint() : x(0), y(0) {}
    QuestPOIPoint(int32 _x, int32 _y) : x(_x), y(_y) {}
};

struct QuestPOI
{
    uint32 PoiId;
    int32  ObjectiveIndex;
    uint32 MapId;
    uint32 MapAreaId;
    uint32 FloorId;
    uint32 Unk3;
    uint32 Unk4;
    std::vector<QuestPOIPoint> points;

    QuestPOI() : PoiId(0), ObjectiveIndex(0), MapId(0), MapAreaId(0), FloorId(0), Unk3(0), Unk4(0) {}
    QuestPOI(uint32 poiId, int32 objIndex, uint32 mapId, uint32 mapAreaId, uint32 floorId, uint32 unk3, uint32 unk4) : PoiId(poiId), ObjectiveIndex(objIndex), MapId(mapId), MapAreaId(mapAreaId), FloorId(floorId), Unk3(unk3), Unk4(unk4) {}
};

typedef std::vector<QuestPOI> QuestPOIVector;
typedef std::unordered_map<uint32, QuestPOIVector> QuestPOIMap;

struct DungeonEncounter
{
    DungeonEncounter(DungeonEncounterEntry const* _dbcEntry, EncounterCreditType _creditType, uint32 _creditEntry, uint32 _lastEncounterDungeon)
        : dbcEntry(_dbcEntry), creditType(_creditType), creditEntry(_creditEntry), lastEncounterDungeon(_lastEncounterDungeon) { }
    DungeonEncounterEntry const* dbcEntry;
    EncounterCreditType creditType;
    uint32 creditEntry;
    uint32 lastEncounterDungeon;
};

typedef std::multimap<uint32, DungeonEncounter const*> DungeonEncounterMap;
typedef std::pair<DungeonEncounterMap::const_iterator, DungeonEncounterMap::const_iterator> DungeonEncounterMapBounds;

struct DungeonFinderRequirements
{
    uint32 minItemLevel;
    uint32 item;
    uint32 item2;
    uint32 allianceQuestId;
    uint32 hordeQuestId;
    uint32 achievement;
    const char* questIncompleteText;

    DungeonFinderRequirements()
        : minItemLevel(0), item(0), item2(0), allianceQuestId(0), hordeQuestId(0), achievement(0) {}
    DungeonFinderRequirements(uint32 MinItemLevel, uint32 Item, uint32 Item2, uint32 AllianceQuestId,
                              uint32 HordeQuestId, uint32 Achievement, const char* QuestIncompleteText)
        : minItemLevel(MinItemLevel), item(Item), item2(Item2), allianceQuestId(AllianceQuestId),
        hordeQuestId(HordeQuestId), achievement(Achievement), questIncompleteText(QuestIncompleteText) {}
};

struct DungeonFinderRewards
{
    uint32 baseXPReward;
    int32  baseMonetaryReward;

    DungeonFinderRewards() : baseXPReward(0), baseMonetaryReward(0) {}
    DungeonFinderRewards(uint32 BaseXPReward, int32 BaseMonetaryReward) : baseXPReward(BaseXPReward), baseMonetaryReward(BaseMonetaryReward) {}
};

struct DungeonFinderItems
{
    // sorted by auto-incrementing id
    uint32 minLevel;
    uint32 maxLevel;
    uint32 itemReward;
    uint32 itemAmount;
    uint32 dungeonType;

    DungeonFinderItems() : minLevel(0), maxLevel(0), itemReward(0), itemAmount(0), dungeonType(0) {}
    DungeonFinderItems(uint32 MinLevel, uint32 MaxLevel, uint32 ItemReward, uint32 ItemAmount, uint32 DungeonType)
        : minLevel(MinLevel), maxLevel(MaxLevel), itemReward(ItemReward), itemAmount(ItemAmount), dungeonType(DungeonType) {}
};

typedef std::unordered_map<uint32, DungeonFinderRequirements> DungeonFinderRequirementsMap;
typedef std::unordered_map<uint32, DungeonFinderRewards> DungeonFinderRewardsMap;
typedef std::unordered_map<uint32, DungeonFinderItems> DungeonFinderItemsMap;

struct GraveYardData
{
    uint32 safeLocId;
    Team team;
};
typedef std::multimap < uint32 /*zoneId*/, GraveYardData > GraveYardMap;
typedef std::pair<GraveYardMap::const_iterator, GraveYardMap::const_iterator> GraveYardMapBounds;

enum ConditionType
{
    //                                                      // value1       value2  for the Condition enumed
    CONDITION_NOT                   = -3,                   // cond-id-1    0          returns !cond-id-1
    CONDITION_OR                    = -2,                   // cond-id-1    cond-id-2  returns cond-id-1 OR cond-id-2
    CONDITION_AND                   = -1,                   // cond-id-1    cond-id-2  returns cond-id-1 AND cond-id-2
    CONDITION_NONE                  = 0,                    // 0            0
    CONDITION_AURA                  = 1,                    // spell_id     effindex
    CONDITION_ITEM                  = 2,                    // item_id      count   check present req. amount items in inventory
    CONDITION_ITEM_EQUIPPED         = 3,                    // item_id      0
    CONDITION_AREAID                = 4,                    // area_id      0, 1 (0: in (sub)area, 1: not in (sub)area)
    CONDITION_REPUTATION_RANK_MIN   = 5,                    // faction_id   min_rank
    CONDITION_TEAM                  = 6,                    // player_team  0,      (469 - Alliance 67 - Horde)
    CONDITION_SKILL                 = 7,                    // skill_id     skill_value
    CONDITION_QUESTREWARDED         = 8,                    // quest_id     0
    CONDITION_QUESTTAKEN            = 9,                    // quest_id     0,1,2   for condition true while quest active (0 any state, 1 if quest incomplete, 2 if quest completed).
    CONDITION_AD_COMMISSION_AURA    = 10,                   // 0            0,      for condition true while one from AD commission aura active
    CONDITION_NO_AURA               = 11,                   // spell_id     effindex
    CONDITION_ACTIVE_GAME_EVENT     = 12,                   // event_id     0
    CONDITION_AREA_FLAG             = 13,                   // area_flag    area_flag_not
    CONDITION_RACE_CLASS            = 14,                   // race_mask    class_mask
    CONDITION_LEVEL                 = 15,                   // player_level 0, 1 or 2 (0: equal to, 1: equal or higher than, 2: equal or less than)
    CONDITION_NOITEM                = 16,                   // item_id      count   check not present req. amount items in inventory
    CONDITION_SPELL                 = 17,                   // spell_id     0, 1 (0: has spell, 1: hasn't spell)
    CONDITION_INSTANCE_SCRIPT       = 18,                   // map_id       instance_condition_id (instance script specific enum)
    CONDITION_QUESTAVAILABLE        = 19,                   // quest_id     0       for case when loot/gossip possible only if player can start quest
    CONDITION_ACHIEVEMENT           = 20,                   // ach_id       0, 1 (0: has achievement, 1: hasn't achievement) for player
    CONDITION_ACHIEVEMENT_REALM     = 21,                   // ach_id       0, 1 (0: has achievement, 1: hasn't achievement) for server
    CONDITION_QUEST_NONE            = 22,                   // quest_id     0 (quest did not take and not rewarded)
    CONDITION_ITEM_WITH_BANK        = 23,                   // item_id      count   check present req. amount items in inventory or bank
    CONDITION_NOITEM_WITH_BANK      = 24,                   // item_id      count   check not present req. amount items in inventory or bank
    CONDITION_NOT_ACTIVE_GAME_EVENT = 25,                   // event_id     0
    CONDITION_ACTIVE_HOLIDAY        = 26,                   // holiday_id   0       preferred use instead CONDITION_ACTIVE_GAME_EVENT when possible
    CONDITION_NOT_ACTIVE_HOLIDAY    = 27,                   // holiday_id   0       preferred use instead CONDITION_NOT_ACTIVE_GAME_EVENT when possible
    CONDITION_LEARNABLE_ABILITY     = 28,                   // spell_id     0 or item_id
    // True when player can learn ability (using min skill value from SkillLineAbility).
    // Item_id can be defined in addition, to check if player has one (1) item in inventory or bank.
    // When player has spell or has item (when defined), condition return false.
    CONDITION_SKILL_BELOW           = 29,                   // skill_id     skill_value
    // True if player has skill skill_id and skill less than (and not equal) skill_value (for skill_value > 1)
    // If skill_value == 1, then true if player has not skill skill_id
    CONDITION_REPUTATION_RANK_MAX   = 30,                   // faction_id   max_rank
    CONDITION_COMPLETED_ENCOUNTER   = 31,                   // encounter_id encounter_id2       encounter_id[2] = DungeonEncounter(dbc).id (if value2 provided it will return value1 OR value2)
    CONDITION_SOURCE_AURA           = 32,                   // spell_id     effindex (returns true if the source of the condition check has aura of spell_id, effIndex)
    CONDITION_LAST_WAYPOINT         = 33,                   // waypointId   0 = exact, 1: wp <= waypointId, 2: wp > waypointId  Use to check what waypoint was last reached
    CONDITION_XP_USER               = 34,                   // 0, 1 (0: XP off, 1: XP on) for player    0
    CONDITION_GENDER                = 35,                   // 0=male, 1=female, 2=none (see enum Gender)
    CONDITION_DEAD_OR_AWAY          = 36,                   // value1: 0=player dead, 1=player is dead (with group dead), 2=player in instance are dead, 3=creature is dead
                                                            // value2: if != 0 only consider players in range of this value
    CONDITION_CREATURE_IN_RANGE     = 37,                   // value1: creature entry; value2: range; returns only alive creatures
    CONDITION_GAMEOBJECT_IN_RANGE   = 38,                   // value1: gameobject entry; value2: range
};

enum ConditionSource                                        // From where was the condition called?
{
    CONDITION_FROM_LOOT             = 0,                    // Used to check a *_loot_template entry
    CONDITION_FROM_REFERING_LOOT    = 1,                    // Used to check a entry refering to a reference_loot_template entry
    CONDITION_FROM_GOSSIP_MENU      = 2,                    // Used to check a gossip menu menu-text
    CONDITION_FROM_GOSSIP_OPTION    = 3,                    // Used to check a gossip menu option-item
    CONDITION_FROM_EVENTAI          = 4,                    // Used to check EventAI Event "On Receive Emote"
    CONDITION_FROM_HARDCODED        = 5,                    // Used to check a hardcoded event - not actually a condition
    CONDITION_FROM_VENDOR           = 6,                    // Used to check a condition from a vendor
    CONDITION_FROM_SPELL_AREA       = 7,                    // Used to check a condition from spell_area table
    CONDITION_FROM_SPELLCLICK       = 8,                    // Used to check a condition from npc_spellclick_spells table
    CONDITION_FROM_DBSCRIPTS        = 9,                    // Used to check a condition from DB Scripts Engine
};

class PlayerCondition
{
    public:
        // Default constructor, required for SQL Storage (Will give errors if used elsewise)
        PlayerCondition() : m_entry(0), m_condition(CONDITION_AND), m_value1(0), m_value2(0) {}

        PlayerCondition(uint16 _entry, int16 _condition, uint32 _value1, uint32 _value2)
            : m_entry(_entry), m_condition(ConditionType(_condition)), m_value1(_value1), m_value2(_value2) {}

        // Checks correctness of values
        bool IsValid() const
        {
            return IsValid(m_entry, m_condition, m_value1, m_value2);
        }
        static bool IsValid(uint16 entry, ConditionType condition, uint32 value1, uint32 value2);

        static bool CanBeUsedWithoutPlayer(uint16 entry);

        // Checks if the player meets the condition
        bool Meets(Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType) const;

    private:
        bool CheckParamRequirements(Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType) const;
        uint16 m_entry;                                     // entry of the condition

    public:
        ConditionType m_condition;                          // additional condition type
        uint32 m_value1;                                    // data for the condition - see ConditionType definition
        uint32 m_value2;
};

// NPC gossip text id
typedef std::unordered_map<uint32, uint32> CacheNpcTextIdMap;

typedef std::unordered_map<uint32, VendorItemData> CacheVendorItemMap;
typedef std::unordered_map<uint32, TrainerSpellData> CacheTrainerSpellMap;

enum SkillRangeType
{
    SKILL_RANGE_LANGUAGE,                                   // 300..300
    SKILL_RANGE_LEVEL,                                      // 1..max skill for level
    SKILL_RANGE_MONO,                                       // 1..1, grey monolite bar
    SKILL_RANGE_RANK,                                       // 1..skill for known rank
    SKILL_RANGE_NONE,                                       // 0..0 always
};

SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial);

struct HotfixInfo
{
    uint32 Type;
    uint32 Timestamp;
    uint32 Entry;
};

typedef std::vector<HotfixInfo> HotfixData;

#define MAX_PLAYER_NAME          12                         // max allowed by client name length
#define MAX_INTERNAL_PLAYER_NAME 15                         // max server internal player name length ( > MAX_PLAYER_NAME for support declined names )
#define MAX_PET_NAME             12                         // max allowed by client name length
#define MAX_CHARTER_NAME         24                         // max allowed by client name length

bool normalizePlayerName(std::string& name);

struct  LanguageDesc
{
    Language lang_id;
    uint32   spell_id;
    uint32   skill_id;
};

extern LanguageDesc lang_description[LANGUAGES_COUNT];
 LanguageDesc const* GetLanguageDescByID(uint32 lang);

class PlayerDumpReader;

template<typename T>
class IdGenerator
{
    public:                                                 // constructors
        explicit IdGenerator(char const* _name) : m_name(_name), m_nextGuid(1) {}

    public:                                                 // modifiers
        void Set(T val)
        {
            m_nextGuid = val;
        }
        T Generate();

    public:                                                 // accessors
        T GetNextAfterMaxUsed() const
        {
            return m_nextGuid;
        }

    private:                                                // fields
        char const* m_name;
        T m_nextGuid;
};

class ObjectMgr
{
        friend class PlayerDumpReader;

    public:
        ObjectMgr();
        ~ObjectMgr();

        typedef std::unordered_map<uint32, Item*> ItemMap;

        typedef std::unordered_map<uint32, Group*> GroupMap;

        typedef std::unordered_map<uint32, ArenaTeam*> ArenaTeamMap;

        typedef std::unordered_map<uint32, Quest*> QuestMap;

        typedef std::unordered_map<uint32, AreaTrigger> AreaTriggerMap;

        typedef std::unordered_map<uint32, RepRewardRate > RepRewardRateMap;
        typedef std::unordered_map<uint32, ReputationOnKillEntry> RepOnKillMap;
        typedef std::unordered_map<uint32, RepSpilloverTemplate> RepSpilloverTemplateMap;

        typedef std::unordered_map<uint32, PointOfInterest> PointOfInterestMap;

        void LoadGameobjectInfo();
        void AddGameobjectInfo(GameObjectInfo* goinfo);

        void PackGroupIds();
        Group* GetGroupById(uint32 id) const;
        void AddGroup(Group* group);
        void RemoveGroup(Group* group);

        ArenaTeam* GetArenaTeamById(uint32 arenateamid) const;
        ArenaTeam* GetArenaTeamByName(const std::string& arenateamname) const;
        ArenaTeam* GetArenaTeamByCaptain(ObjectGuid guid) const;
        void AddArenaTeam(ArenaTeam* arenaTeam);
        void RemoveArenaTeam(uint32 Id);
        ArenaTeamMap::iterator GetArenaTeamMapBegin() { return mArenaTeamMap.begin(); }
        ArenaTeamMap::iterator GetArenaTeamMapEnd()   { return mArenaTeamMap.end(); }

        CreatureModelInfo const* GetCreatureModelRandomGender(uint32 display_id) const;
        uint32 GetCreatureModelAlternativeModel(uint32 modelId) const;

        PetLevelInfo const* GetPetLevelInfo(uint32 creature_id, uint32 level) const;

        void GetPlayerClassLevelInfo(uint32 class_, uint32 level, uint32& baseHP, uint32& baseMana) const;

        PlayerInfo const* GetPlayerInfo(uint32 race, uint32 class_) const
        {
            if (race   >= MAX_RACES)
            {
                return NULL;
            }
            if (class_ >= MAX_CLASSES)
            {
                return NULL;
            }
            PlayerInfo const* info = &playerInfo[race][class_];
            if (info->displayId_m == 0 || info->displayId_f == 0)
            {
                return NULL;
            }
            return info;
        }
        void GetPlayerLevelInfo(uint32 race, uint32 class_, uint32 level, PlayerLevelInfo* info) const;

        ObjectGuid GetPlayerGuidByName(std::string name) const;
        bool GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const;
        Team GetPlayerTeamByGUID(ObjectGuid guid) const;
        uint32 GetPlayerAccountIdByGUID(ObjectGuid guid) const;
        uint32 GetPlayerAccountIdByPlayerName(const std::string& name) const;

        uint32 GetNearestTaxiNode(float x, float y, float z, uint32 mapid, Team team);
        void GetTaxiPath(uint32 source, uint32 destination, uint32& path, uint32& cost);
        uint32 GetTaxiMountDisplayId(uint32 id, Team team, bool allowed_alt_team = false);

        Quest const* GetQuestTemplate(uint32 quest_id) const
        {
            QuestMap::const_iterator itr = mQuestTemplates.find(quest_id);
            return itr != mQuestTemplates.end() ? itr->second : NULL;
        }
        QuestMap const& GetQuestTemplates() const
        {
            return mQuestTemplates;
        }

        uint32 GetQuestForAreaTrigger(uint32 Trigger_ID) const
        {
            QuestAreaTriggerMap::const_iterator itr = mQuestAreaTriggerMap.find(Trigger_ID);
            if (itr != mQuestAreaTriggerMap.end())
            {
                return itr->second;
            }
            return 0;
        }
        bool IsTavernAreaTrigger(uint32 Trigger_ID) const
        {
            return mTavernAreaTriggerSet.find(Trigger_ID) != mTavernAreaTriggerSet.end();
        }

        bool IsGameObjectForQuests(uint32 entry) const
        {
            return mGameObjectForQuestSet.find(entry) != mGameObjectForQuestSet.end();
        }

        GossipText const* GetGossipText(uint32 Text_ID) const;

        WorldSafeLocsEntry const* GetClosestGraveYard(float x, float y, float z, uint32 MapId, Team team);
        bool AddGraveYardLink(uint32 id, uint32 zone, Team team, bool inDB = true);
        void SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team);
        void LoadGraveyardZones();
        GraveYardData const* FindGraveYardData(uint32 id, uint32 zone) const;

        AreaTrigger const* GetAreaTrigger(uint32 trigger) const
        {
            AreaTriggerMap::const_iterator itr = mAreaTriggers.find(trigger);
            if (itr != mAreaTriggers.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        AreaTrigger const* GetGoBackTrigger(uint32 Map) const;
        AreaTrigger const* GetMapEntranceTrigger(uint32 Map) const;

        RepRewardRate const* GetRepRewardRate(uint32 factionId) const
        {
            RepRewardRateMap::const_iterator itr = m_RepRewardRateMap.find(factionId);
            if (itr != m_RepRewardRateMap.end())
            {
                return &itr->second;
            }

            return NULL;
        }

        ReputationOnKillEntry const* GetReputationOnKillEntry(uint32 id) const
        {
            RepOnKillMap::const_iterator itr = mRepOnKill.find(id);
            if (itr != mRepOnKill.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        RepSpilloverTemplate const* GetRepSpilloverTemplate(uint32 factionId) const
        {
            RepSpilloverTemplateMap::const_iterator itr = m_RepSpilloverTemplateMap.find(factionId);
            if (itr != m_RepSpilloverTemplateMap.end())
            {
                return &itr->second;
            }

            return NULL;
        }

        PointOfInterest const* GetPointOfInterest(uint32 id) const
        {
            PointOfInterestMap::const_iterator itr = mPointsOfInterest.find(id);
            if (itr != mPointsOfInterest.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        QuestPOIVector const* GetQuestPOIVector(uint32 questId)
        {
            QuestPOIMap::const_iterator itr = mQuestPOIMap.find(questId);
            if (itr != mQuestPOIMap.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        DungeonFinderRequirements const* GetDungeonFinderRequirements(uint32 mapId, uint32 difficulty) const
        {
            DungeonFinderRequirementsMap::const_iterator itr = mDungeonFinderRequirementsMap.find(MAKE_PAIR32(mapId, difficulty));
            if (itr != mDungeonFinderRequirementsMap.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        DungeonFinderRewards const* GetDungeonFinderRewards(uint32 level) const
        {
            DungeonFinderRewardsMap::const_iterator itr = mDungeonFinderRewardsMap.find(level);
            if (itr != mDungeonFinderRewardsMap.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        DungeonFinderRequirementsMap const& GetDungeonFinderRequirementsMap() const { return mDungeonFinderRequirementsMap; }
        DungeonFinderRewardsMap const& GetDungeonFinderRewardsMap() const { return mDungeonFinderRewardsMap; }
        DungeonFinderItemsMap const& GetDungeonFinderItemsMap() const { return mDungeonFinderItemsMap; }

        // Static wrappers for various accessors
        static GameObjectInfo const* GetGameObjectInfo(uint32 id);                  ///< Wrapper for sGOStorage.LookupEntry
        static Player* GetPlayer(const char* name);                                 ///< Wrapper for sObjectAccessor.FindPlayerByName
        static Player* GetPlayer(ObjectGuid guid, bool inWorld = true);             ///< Wrapper for sObjectAccessor.FindPlayer
        static CreatureInfo const* GetCreatureTemplate(uint32 id);                  ///< Wrapper for sCreatureStorage.LookupEntry
        static CreatureModelInfo const* GetCreatureModelInfo(uint32 modelid);       ///< Wrapper for sCreatureModelStorage.LookupEntry
        static EquipmentInfo const* GetEquipmentInfo(uint32 entry);                 ///< Wrapper for sEquipmentStorage.LookupEntry
        static CreatureDataAddon const* GetCreatureAddon(uint32 lowguid);           ///< Wrapper for sCreatureDataAddonStorage.LookupEntry
        static CreatureDataAddon const* GetCreatureTemplateAddon(uint32 entry);     ///< Wrapper for sCreatureInfoAddonStorage.LookupEntry
        static ItemPrototype const* GetItemPrototype(uint32 id);                    ///< Wrapper for sItemStorage.LookupEntry
        static InstanceTemplate const* GetInstanceTemplate(uint32 map);             ///< Wrapper for sInstanceTemplate.LookupEntry
        static WorldTemplate const* GetWorldTemplate(uint32 map);                   ///< Wrapper for sWorldTemplate.LookupEntry

        // Loading functions
        void LoadArenaTeams();
        void LoadGroups();
        void LoadQuests();
        void LoadQuestRelations()
        {
            LoadGameobjectQuestRelations();
            LoadGameobjectInvolvedRelations();
            LoadCreatureQuestRelations();
            LoadCreatureInvolvedRelations();
        }
        void LoadGameobjectQuestRelations();
        void LoadGameobjectInvolvedRelations();
        void LoadCreatureQuestRelations();
        void LoadCreatureInvolvedRelations();

        bool LoadMangosStrings(DatabaseType& db, char const* table, int32 min_value, int32 max_value, bool extra_content);
        bool LoadMangosStrings()
        {
            return LoadMangosStrings(WorldDatabase, "mangos_string", MIN_MANGOS_STRING_ID, MAX_MANGOS_STRING_ID, false);
        }
        void LoadCreatureLocales();
        void LoadCreatureTemplates();
        void LoadCreatures();
        void LoadCreatureAddons();
        void LoadCreatureClassLvlStats();
        void LoadCreatureModelInfo();
        void LoadCreatureModelRace();
        void LoadEquipmentTemplates();
        void LoadGameObjectLocales();
        void LoadGameObjects();
        void LoadGameObjectAddon();
        void LoadItemPrototypes();
        void LoadItemConverts();
        void LoadItemExpireConverts();
        void LoadItemRequiredTarget();
        void LoadItemLocales();
        void LoadQuestLocales();
        void LoadGossipTextLocales();
        void LoadPageTextLocales();
        void LoadGossipMenuItemsLocales();
        void LoadPointOfInterestLocales();
        void LoadInstanceEncounters();
        void LoadInstanceTemplate();
        void LoadWorldTemplate();
        void LoadConditions();
        void LoadMailLevelRewards();

        void LoadGossipText();

        void LoadAreaTriggerTeleports();
        void LoadQuestAreaTriggers();
        void LoadTavernAreaTriggers();
        void LoadGameObjectForQuests();

        void LoadPageTexts();

        void LoadPlayerInfo();
        void LoadPetLevelInfo();
        void LoadExplorationBaseXP();
        void LoadPetNames();
        void LoadPetNumber();
        void LoadCorpses();
        void LoadFishingBaseSkillLevel();

        void LoadReputationRewardRate();
        void LoadReputationOnKill();
        void LoadReputationSpilloverTemplate();

        void LoadPointsOfInterest();
        void LoadQuestPOI();

        void LoadDungeonFinderRequirements();
        void LoadDungeonFinderRewards();
        void LoadDungeonFinderItems();

        void LoadNPCSpellClickSpells();
        void LoadSpellTemplate();
        void LoadCreatureTemplateSpells();

        void LoadGameTele();

        void LoadGossipMenus();

        void LoadVendorTemplates();
        void LoadVendors()
        {
            LoadVendors("npc_vendor", false);
        }
        void LoadTrainerTemplates();
        void LoadTrainers()
        {
            LoadTrainers("npc_trainer", false);
        }

        /// @param _map Map* of the map for which to load active entities. If NULL active entities on continents are loaded
        void LoadActiveEntities(Map* _map);

        void LoadVehicleAccessory();

        std::string GeneratePetName(uint32 entry);
        uint32 GetBaseXP(uint32 level) const;
        uint32 GetXPForLevel(uint32 level) const;
        uint32 GetXPForPetLevel(uint32 level) const
        {
            return GetXPForLevel(level) / 20;
        }

        int32 GetFishingBaseSkillLevel(uint32 entry) const
        {
            FishingBaseSkillMap::const_iterator itr = mFishingBaseForArea.find(entry);
            return itr != mFishingBaseForArea.end() ? itr->second : 0;
        }

        void ReturnOrDeleteOldMails(bool serverUp);

        void SetHighestGuids();

        // used for set initial guid counter for map local guids
        uint32 GetFirstTemporaryCreatureLowGuid() const
        {
            return m_FirstTemporaryCreatureGuid;
        }
        uint32 GetFirstTemporaryGameObjectLowGuid() const
        {
            return m_FirstTemporaryGameObjectGuid;
        }

        // used in .npc add/.gobject add commands for adding static spawns
        uint32 GenerateStaticCreatureLowGuid()
        {
            if (m_StaticCreatureGuids.GetNextAfterMaxUsed() >= m_FirstTemporaryCreatureGuid)
            {
                return 0;
            }
            return m_StaticCreatureGuids.Generate();
        }
        uint32 GenerateStaticGameObjectLowGuid()
        {
            if (m_StaticGameObjectGuids.GetNextAfterMaxUsed() >= m_FirstTemporaryGameObjectGuid)
            {
                return 0;
            }
            return m_StaticGameObjectGuids.Generate();
        }

        uint32 GeneratePlayerLowGuid()
        {
            return m_CharGuids.Generate();
        }
        uint32 GenerateItemLowGuid()
        {
            return m_ItemGuids.Generate();
        }
        uint32 GenerateCorpseLowGuid()
        {
            return m_CorpseGuids.Generate();
        }
        uint32 GenerateInstanceLowGuid() { return m_InstanceGuids.Generate(); }
        uint32 GenerateGroupLowGuid()    { return m_GroupGuids.Generate();    }

        uint32 GenerateArenaTeamId() { return m_ArenaTeamIds.Generate(); }
        uint32 GenerateAuctionID()
        {
            return m_AuctionIds.Generate();
        }
        uint64 GenerateEquipmentSetGuid() { return m_EquipmentSetIds.Generate(); }
        uint32 GenerateGuildId()
        {
            return m_GuildIds.Generate();
        }
        // uint32 GenerateItemTextID() { return m_ItemGuids.Generate(); }
        uint32 GenerateMailID()
        {
            return m_MailIds.Generate();
        }
        uint32 GeneratePetNumber()
        {
            return m_PetNumbers.Generate();
        }

        std::string GetItemText(uint32 id)
        {
            if (QueryResult* result = CharacterDatabase.PQuery("SELECT `body` FROM `mail` WHERE `id` = '%u'", id))
            {
                Field* fields = result->Fetch();
                std::string body = fields[0].GetCppString();
                delete result;
                return body;
            }
            else
            {
                return "There is no info for this item";
            }
        }

        MailLevelReward const* GetMailLevelReward(uint32 level, uint32 raceMask)
        {
            MailLevelRewardMap::const_iterator map_itr = m_mailLevelRewardMap.find(level);
            if (map_itr == m_mailLevelRewardMap.end())
            {
                return NULL;
            }

            for (MailLevelRewardList::const_iterator set_itr = map_itr->second.begin(); set_itr != map_itr->second.end(); ++set_itr)
                if (set_itr->raceMask & raceMask)
                {
                    return &*set_itr;
                }

            return NULL;
        }

        CreatureDataPair const* GetCreatureDataPair(uint32 guid) const
        {
            CreatureDataMap::const_iterator itr = mCreatureDataMap.find(guid);
            if (itr == mCreatureDataMap.end())
            {
                return NULL;
            }
            return &*itr;
        }

        CreatureData const* GetCreatureData(uint32 guid) const
        {
            CreatureDataPair const* dataPair = GetCreatureDataPair(guid);
            return dataPair ? &dataPair->second : NULL;
        }

        CreatureData& NewOrExistCreatureData(uint32 guid)
        {
            return mCreatureDataMap[guid];
        }
        void DeleteCreatureData(uint32 guid);

        template<typename Worker>
        void DoCreatureData(Worker& worker) const
        {
            for (CreatureDataMap::const_iterator itr = mCreatureDataMap.begin(); itr != mCreatureDataMap.end(); ++itr)
                if (worker(*itr))
                {
                    break;
                }
        }

        CreatureLocale const* GetCreatureLocale(uint32 entry) const
        {
            CreatureLocaleMap::const_iterator itr = mCreatureLocaleMap.find(entry);
            if (itr == mCreatureLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        void GetCreatureLocaleStrings(uint32 entry, int32 loc_idx, char const** namePtr, char const** subnamePtr = NULL) const;

        GameObjectLocale const* GetGameObjectLocale(uint32 entry) const
        {
            GameObjectLocaleMap::const_iterator itr = mGameObjectLocaleMap.find(entry);
            if (itr == mGameObjectLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        ItemLocale const* GetItemLocale(uint32 entry) const
        {
            ItemLocaleMap::const_iterator itr = mItemLocaleMap.find(entry);
            if (itr == mItemLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        void GetItemLocaleStrings(uint32 entry, int32 loc_idx, std::string* namePtr, std::string* descriptionPtr = NULL) const;

        QuestLocale const* GetQuestLocale(uint32 entry) const
        {
            QuestLocaleMap::const_iterator itr = mQuestLocaleMap.find(entry);
            if (itr == mQuestLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        void GetQuestLocaleStrings(uint32 entry, int32 loc_idx, std::string* titlePtr) const;

        NpcTextLocale const* GetNpcTextLocale(uint32 entry) const
        {
            NpcTextLocaleMap::const_iterator itr = mNpcTextLocaleMap.find(entry);
            if (itr == mNpcTextLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        typedef std::string NpcTextArray[MAX_GOSSIP_TEXT_OPTIONS];
        void GetNpcTextLocaleStringsAll(uint32 entry, int32 loc_idx, NpcTextArray *text0_Ptr, NpcTextArray* text1_Ptr) const;
        void GetNpcTextLocaleStrings0(uint32 entry, int32 loc_idx, std::string* text0_0_Ptr, std::string* text1_0_Ptr) const;

        PageTextLocale const* GetPageTextLocale(uint32 entry) const
        {
            PageTextLocaleMap::const_iterator itr = mPageTextLocaleMap.find(entry);
            if (itr == mPageTextLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        GossipMenuItemsLocale const* GetGossipMenuItemsLocale(uint32 entry) const
        {
            GossipMenuItemsLocaleMap::const_iterator itr = mGossipMenuItemsLocaleMap.find(entry);
            if (itr == mGossipMenuItemsLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        PointOfInterestLocale const* GetPointOfInterestLocale(uint32 poi_id) const
        {
            PointOfInterestLocaleMap::const_iterator itr = mPointOfInterestLocaleMap.find(poi_id);
            if (itr == mPointOfInterestLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        GameObjectDataPair const* GetGODataPair(uint32 guid) const
        {
            GameObjectDataMap::const_iterator itr = mGameObjectDataMap.find(guid);
            if (itr == mGameObjectDataMap.end())
            {
                return NULL;
            }
            return &*itr;
        }

        GameObjectData const* GetGOData(uint32 guid) const
        {
            GameObjectDataPair const* dataPair = GetGODataPair(guid);
            return dataPair ? &dataPair->second : NULL;
        }

        GameObjectData& NewGOData(uint32 guid)
        {
            return mGameObjectDataMap[guid];
        }
        void DeleteGOData(uint32 guid);

        template<typename Worker>
        void DoGOData(Worker& worker) const
        {
            for (GameObjectDataMap::const_iterator itr = mGameObjectDataMap.begin(); itr != mGameObjectDataMap.end(); ++itr)
                if (worker(*itr))                           // arg = GameObjectDataPair
                {
                    break;
                }
        }

        MangosStringLocale const* GetMangosStringLocale(int32 entry) const
        {
            MangosStringLocaleMap::const_iterator itr = mMangosStringLocaleMap.find(entry);
            if (itr == mMangosStringLocaleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }
        uint32 GetLoadedStringsCount(int32 minEntry) const
        {
            std::map<int32, uint32>::const_iterator itr = m_loadedStringCount.find(minEntry);
            if (itr != m_loadedStringCount.end())
            {
                return itr->second;
            }
            return 0;
        }

        const char* GetMangosString(int32 entry, int locale_idx) const;
        const char* GetMangosStringForDBCLocale(int32 entry) const { return GetMangosString(entry, DBCLocaleIndex); }
        int32 GetDBCLocaleIndex() const { return DBCLocaleIndex; }
        void SetDBCLocaleIndex(uint32 lang) { DBCLocaleIndex = GetIndexForLocale(LocaleConstant(lang)); }

        // global grid objects state (static DB spawns, global spawn mods from gameevent system)
        CellObjectGuids const& GetCellObjectGuids(uint16 mapid, uint8 spawnMode, uint32 cell_id)
        {
            return mMapObjectGuids[MAKE_PAIR32(mapid, spawnMode)][cell_id];
        }

        // modifiers for global grid objects state (static DB spawns, global spawn mods from gameevent system)
        // Don't must be used for modify instance specific spawn state modifications
        void AddCreatureToGrid(uint32 guid, CreatureData const* data);
        void RemoveCreatureFromGrid(uint32 guid, CreatureData const* data);
        void AddGameobjectToGrid(uint32 guid, GameObjectData const* data);
        void RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data);
        void AddCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid, uint32 instance);
        void DeleteCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid);

        // reserved names
        void LoadReservedPlayersNames();
        bool IsReservedName(const std::string& name) const;

        // name with valid structure and symbols
        static uint8 CheckPlayerName(const std::string& name, bool create = false);
        static PetNameInvalidReason CheckPetName(const std::string& name);
        static bool IsValidCharterName(const std::string& name);

        static bool CheckDeclinedNames(const std::wstring& mainpart, DeclinedName const& names);

        int GetIndexForLocale(LocaleConstant loc);
        LocaleConstant GetLocaleForIndex(int i);

        // Check if a player meets condition conditionId
        bool IsPlayerMeetToCondition(uint16 conditionId, Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType) const;

        void GetConditions(uint32 conditionId, std::vector<PlayerCondition const*>& out) const;

        GameTele const* GetGameTele(uint32 id) const
        {
            GameTeleMap::const_iterator itr = m_GameTeleMap.find(id);
            if (itr == m_GameTeleMap.end())
            {
                return NULL;
            }
            return &itr->second;
        }

        GameTele const* GetGameTele(const std::string& name) const;
        GameTeleMap const& GetGameTeleMap() const
        {
            return m_GameTeleMap;
        }
        bool AddGameTele(GameTele& data);
        bool DeleteGameTele(const std::string& name);

        uint32 GetNpcGossip(uint32 entry) const
        {
            CacheNpcTextIdMap::const_iterator iter = m_mCacheNpcTextIdMap.find(entry);
            if (iter == m_mCacheNpcTextIdMap.end())
            {
                return 0;
            }

            return iter->second;
        }

        TrainerSpellData const* GetNpcTrainerSpells(uint32 entry) const
        {
            CacheTrainerSpellMap::const_iterator iter = m_mCacheTrainerSpellMap.find(entry);
            if (iter == m_mCacheTrainerSpellMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        TrainerSpellData const* GetNpcTrainerTemplateSpells(uint32 entry) const
        {
            CacheTrainerSpellMap::const_iterator iter = m_mCacheTrainerTemplateSpellMap.find(entry);
            if (iter == m_mCacheTrainerTemplateSpellMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        VendorItemData const* GetNpcVendorItemList(uint32 entry) const
        {
            CacheVendorItemMap::const_iterator  iter = m_mCacheVendorItemMap.find(entry);
            if (iter == m_mCacheVendorItemMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        VendorItemData const* GetNpcVendorTemplateItemList(uint32 entry) const
        {
            CacheVendorItemMap::const_iterator  iter = m_mCacheVendorTemplateItemMap.find(entry);
            if (iter == m_mCacheVendorTemplateItemMap.end())
            {
                return NULL;
            }

            return &iter->second;
        }

        void AddVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost);
        void AddVendorItem(uint32 entry, uint32 item, uint8 type, uint32 maxcount, uint32 incrtime, uint32 ExtendedCost);
        bool RemoveVendorItem(uint32 entry, uint32 item, uint8 type);
        bool IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item, uint8 type, uint32 maxcount, uint32 ptime, uint32 ExtendedCost, uint16 conditionId, Player* pl = NULL, std::set<uint32>* skip_vendors = NULL) const;
        bool IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item, uint32 maxcount, uint32 ptime, uint32 ExtendedCost, uint16 conditionId, Player* pl = NULL, std::set<uint32>* skip_vendors = NULL) const;

        static inline void GetLocaleString(const StringVector& data, int loc_idx, std::string& value)
        {
            if (data.size() > size_t(loc_idx) && !data[loc_idx].empty())
            {
                value = data[loc_idx];
            }
        }

        int GetOrNewIndexForLocale(LocaleConstant loc);

        SpellClickInfoMapBounds GetSpellClickInfoMapBounds(uint32 creature_id) const
        {
            return mSpellClickInfoMap.equal_range(creature_id);
        }

        uint32 GetItemConvert(uint32 itemEntry, uint32 raceMask) const
        {
            ItemConvertMap::const_iterator iter = m_ItemConvert.find(itemEntry);
            if (iter == m_ItemConvert.end())
            {
                return 0;
            }

            ItemPrototype const* proto = GetItemPrototype(iter->second);
            return (proto && proto->AllowableRace & raceMask) ? iter->second : 0;
        }

        uint32 GetItemExpireConvert(uint32 itemEntry) const
        {
            ItemConvertMap::const_iterator iter = m_ItemExpireConvert.find(itemEntry);
            return iter != m_ItemExpireConvert.end() ? iter->second : 0;
        }

        ItemRequiredTargetMapBounds GetItemRequiredTargetMapBounds(uint32 uiItemEntry) const
        {
            return m_ItemRequiredTarget.equal_range(uiItemEntry);
        }

        DungeonEncounterMapBounds GetDungeonEncounterBounds(uint32 creditEntry) const
        {
            return m_DungeonEncounters.equal_range(creditEntry);
        }

        GossipMenusMapBounds GetGossipMenusMapBounds(uint32 uiMenuId) const
        {
            return m_mGossipMenusMap.equal_range(uiMenuId);
        }

        GossipMenuItemsMapBounds GetGossipMenuItemsMapBounds(uint32 uiMenuId) const
        {
            return m_mGossipMenuItemsMap.equal_range(uiMenuId);
        }

        ExclusiveQuestGroupsMapBounds GetExclusiveQuestGroupsMapBounds(int32 groupId) const
        {
            return m_ExclusiveQuestGroups.equal_range(groupId);
        }

        QuestRelationsMapBounds GetCreatureQuestRelationsMapBounds(uint32 entry) const
        {
            return m_CreatureQuestRelations.equal_range(entry);
        }

        QuestRelationsMapBounds GetCreatureQuestInvolvedRelationsMapBounds(uint32 entry) const
        {
            return m_CreatureQuestInvolvedRelations.equal_range(entry);
        }

        QuestRelationsMapBounds GetGOQuestRelationsMapBounds(uint32 entry) const
        {
            return m_GOQuestRelations.equal_range(entry);
        }

        QuestRelationsMapBounds GetGOQuestInvolvedRelationsMapBounds(uint32 entry) const
        {
            return m_GOQuestInvolvedRelations.equal_range(entry);
        }

        QuestRelationsMap& GetCreatureQuestRelationsMap()
        {
            return m_CreatureQuestRelations;
        }
        uint32 GetModelForRace(uint32 sourceModelId, uint32 racemask);
        /**
        * \brief: Data returned is used to compute health, mana, armor, damage of creatures. May be NULL.
        * \param uint32 level               creature level
        * \param uint32 unitClass           creature class, related to CLASSMASK_ALL_CREATURES
        * \param uint32 expansion           creature expansion (we could have creature exp = 0 for vanilla, exp = 1 for tbc, exp = 2 for wotlk)
        * \return: CreatureClassLvlStats const* or NULL
        *
        * Description: GetCreatureClassLvlStats give fast access to creature stats data.
        * FullName: ObjectMgr::GetCreatureClassLvlStats
        * Access: public
        * Qualifier: const
        **/
        CreatureClassLvlStats const* GetCreatureClassLvlStats(uint32 level, uint32 unitClass, int32 expansion) const;

        void LoadHotfixData();
        HotfixData const& GetHotfixData() const { return m_hotfixData; }
        uint32 GetHotfixDate(uint32 entry, uint32 type) const
        {
            uint32 ret = 0;
            for (HotfixData::const_iterator itr = m_hotfixData.begin(); itr != m_hotfixData.end(); ++itr)
                if (itr->Entry == entry && itr->Type == type)
                    if (itr->Timestamp > ret)
                    {
                        ret = itr->Timestamp;
                    }

            return ret ? ret : uint32(time(NULL));
        }

        void LoadPhaseDefinitions();
        void LoadSpellPhaseInfo();

        PhaseDefinitionStore const* GetPhaseDefinitionStore() { return &_PhaseDefinitionStore; }
        SpellPhaseStore const* GetSpellPhaseStore() { return &_SpellPhaseStore; }

    protected:

        // first free id for selected id type
        IdGenerator<uint32> m_ArenaTeamIds;
        IdGenerator<uint32> m_AuctionIds;
        IdGenerator<uint64> m_EquipmentSetIds;
        IdGenerator<uint32> m_GuildIds;
        IdGenerator<uint32> m_MailIds;
        IdGenerator<uint32> m_PetNumbers;

        // initial free low guid for selected guid type for map local guids
        uint32 m_FirstTemporaryCreatureGuid;
        uint32 m_FirstTemporaryGameObjectGuid;

        // guids from reserved range for use in .npc add/.gobject add commands for adding new static spawns (saved in DB) from client.
        ObjectGuidGenerator<HIGHGUID_UNIT>        m_StaticCreatureGuids;
        ObjectGuidGenerator<HIGHGUID_GAMEOBJECT>  m_StaticGameObjectGuids;

        // first free low guid for selected guid type
        ObjectGuidGenerator<HIGHGUID_PLAYER>     m_CharGuids;
        ObjectGuidGenerator<HIGHGUID_ITEM>       m_ItemGuids;
        ObjectGuidGenerator<HIGHGUID_CORPSE>     m_CorpseGuids;
        ObjectGuidGenerator<HIGHGUID_INSTANCE>   m_InstanceGuids;
        ObjectGuidGenerator<HIGHGUID_GROUP>      m_GroupGuids;

        QuestMap            mQuestTemplates;

        typedef std::unordered_map<uint32, GossipText> GossipTextMap;
        typedef std::unordered_map<uint32, uint32> QuestAreaTriggerMap;
        typedef std::set<uint32> TavernAreaTriggerSet;
        typedef std::set<uint32> GameObjectForQuestSet;

        typedef std::multimap<uint32, CreatureModelRace> CreatureModelRaceMap;
        typedef std::pair<CreatureModelRaceMap::const_iterator, CreatureModelRaceMap::const_iterator> CreatureModelRaceMapBounds;

        GroupMap            mGroupMap;
        ArenaTeamMap        mArenaTeamMap;

        QuestAreaTriggerMap mQuestAreaTriggerMap;
        TavernAreaTriggerSet mTavernAreaTriggerSet;
        GameObjectForQuestSet mGameObjectForQuestSet;
        GossipTextMap       mGossipText;
        AreaTriggerMap      mAreaTriggers;

        RepRewardRateMap    m_RepRewardRateMap;
        RepOnKillMap        mRepOnKill;
        RepSpilloverTemplateMap m_RepSpilloverTemplateMap;

        GossipMenusMap      m_mGossipMenusMap;
        GossipMenuItemsMap  m_mGossipMenuItemsMap;
        PointOfInterestMap  mPointsOfInterest;

        QuestPOIMap         mQuestPOIMap;

        DungeonFinderRequirementsMap mDungeonFinderRequirementsMap;
        DungeonFinderRewardsMap mDungeonFinderRewardsMap;
        DungeonFinderItemsMap mDungeonFinderItemsMap;

        // character reserved names
        typedef std::set<std::wstring> ReservedNamesMap;
        ReservedNamesMap    m_ReservedNames;

        GraveYardMap        mGraveYardMap;

        GameTeleMap         m_GameTeleMap;

        SpellClickInfoMap   mSpellClickInfoMap;

        ItemConvertMap        m_ItemConvert;
        ItemConvertMap        m_ItemExpireConvert;
        ItemRequiredTargetMap m_ItemRequiredTarget;

        typedef             std::vector<LocaleConstant> LocalForIndex;
        LocalForIndex        m_LocalForIndex;

        ExclusiveQuestGroupsMap m_ExclusiveQuestGroups;

        QuestRelationsMap       m_CreatureQuestRelations;
        QuestRelationsMap       m_CreatureQuestInvolvedRelations;
        QuestRelationsMap       m_GOQuestRelations;
        QuestRelationsMap       m_GOQuestInvolvedRelations;

        int DBCLocaleIndex;

    private:
        void LoadCreatureAddons(SQLStorage& creatureaddons, char const* entryName, char const* comment);
        void ConvertCreatureAddonAuras(CreatureDataAddon* addon, char const* table, char const* guidEntryStr);
        void LoadQuestRelationsHelper(QuestRelationsMap& map, QuestActor actor, QuestRole role);
        void LoadVendors(char const* tableName, bool isTemplates);
        void LoadTrainers(char const* tableName, bool isTemplates);

        void LoadGossipMenu(std::set<uint32>& gossipScriptSet);
        void LoadGossipMenuItems(std::set<uint32>& gossipScriptSet);

        MailLevelRewardMap m_mailLevelRewardMap;

        typedef std::map<uint32, PetLevelInfo*> PetLevelInfoMap;
        // PetLevelInfoMap[creature_id][level]
        PetLevelInfoMap petInfo;                            // [creature_id][level]

        void BuildPlayerLevelInfo(uint8 race, uint8 class_, uint8 level, PlayerLevelInfo* plinfo) const;
        PlayerInfo playerInfo[MAX_RACES][MAX_CLASSES];

        typedef std::vector<uint32> PlayerXPperLevel;       // [level]
        PlayerXPperLevel mPlayerXPperLevel;

        typedef std::map<uint32, uint32> BaseXPMap;         // [area level][base xp]
        BaseXPMap mBaseXPTable;

        typedef std::map<uint32, int32> FishingBaseSkillMap;// [areaId][base skill level]
        FishingBaseSkillMap mFishingBaseForArea;

        typedef std::map<uint32, std::vector<std::string> > HalfNameMap;
        HalfNameMap PetHalfName0;
        HalfNameMap PetHalfName1;

        typedef std::multimap<uint32 /*mapId*/, uint32 /*guid*/> ActiveCreatureGuidsOnMap;

        // Array to store creature stats, Max creature level + 1 (for data alignement with in game level)
        CreatureClassLvlStats m_creatureClassLvlStats[DEFAULT_MAX_CREATURE_LEVEL + 1][MAX_CREATURE_CLASS][MAX_EXPANSION + 1];

        MapObjectGuids mMapObjectGuids;
        ActiveCreatureGuidsOnMap m_activeCreatures;
        CreatureDataMap mCreatureDataMap;
        CreatureLocaleMap mCreatureLocaleMap;
        GameObjectDataMap mGameObjectDataMap;
        GameObjectLocaleMap mGameObjectLocaleMap;
        ItemLocaleMap mItemLocaleMap;
        QuestLocaleMap mQuestLocaleMap;
        NpcTextLocaleMap mNpcTextLocaleMap;
        PageTextLocaleMap mPageTextLocaleMap;
        MangosStringLocaleMap mMangosStringLocaleMap;
        std::map < int32 /*minEntryOfBracket*/, uint32 /*count*/ > m_loadedStringCount;
        GossipMenuItemsLocaleMap mGossipMenuItemsLocaleMap;
        PointOfInterestLocaleMap mPointOfInterestLocaleMap;
        DungeonEncounterMap m_DungeonEncounters;

        CreatureModelRaceMap    m_mCreatureModelRaceMap;

        CacheNpcTextIdMap m_mCacheNpcTextIdMap;
        CacheVendorItemMap m_mCacheVendorTemplateItemMap;
        CacheVendorItemMap m_mCacheVendorItemMap;
        CacheTrainerSpellMap m_mCacheTrainerTemplateSpellMap;
        CacheTrainerSpellMap m_mCacheTrainerSpellMap;

        HotfixData m_hotfixData;

        PhaseDefinitionStore _PhaseDefinitionStore;
        SpellPhaseStore _SpellPhaseStore;
};

#define sObjectMgr MaNGOS::Singleton<ObjectMgr>::Instance()

/// generic text function
 bool DoDisplayText(WorldObject* source, int32 entry, Unit const* target = NULL);

// scripting access functions
 bool LoadMangosStrings(DatabaseType& db, char const* table, int32 start_value = MAX_CREATURE_AI_TEXT_STRING_ID, int32 end_value = std::numeric_limits<int32>::min(), bool extra_content = false);
 CreatureInfo const* GetCreatureTemplateStore(uint32 entry);
 Quest const* GetQuestTemplateStore(uint32 entry);
 MangosStringLocale const* GetMangosStringData(int32 entry);

#endif
