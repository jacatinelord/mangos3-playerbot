#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotFactory.h"
#include "SQLStorages.h"
#include "ItemPrototype.h"
#include "PlayerbotAIConfig.h"
#include "AccountMgr.h"
#include "DBCStore.h"
#include "SharedDefines.h"
#include "ahbot/AhBot.h"
#include "RandomPlayerbotFactory.h"

using namespace ai;
using namespace std;

// List of trade skills available for player bots
uint32 PlayerbotFactory::tradeSkills[] =
{
    SKILL_ALCHEMY,
    SKILL_ENCHANTING,
    SKILL_SKINNING,
    SKILL_TAILORING,
#if !defined(CLASSIC)
    SKILL_JEWELCRAFTING,
#endif
    SKILL_LEATHERWORKING,
    SKILL_ENGINEERING,
    SKILL_HERBALISM,
    SKILL_MINING,
    SKILL_BLACKSMITHING,
    SKILL_COOKING,
    SKILL_FIRST_AID,
    SKILL_FISHING
};

// List of class-specific quest IDs
list<uint32> PlayerbotFactory::classQuestIds;

/**
 * Randomizes the player bot's attributes and equipment.
 */
void PlayerbotFactory::Randomize()
{
    Randomize(true);
}

/**
 * Refreshes the player bot's attributes and equipment.
 */
void PlayerbotFactory::Refresh()
{
    Prepare();
    InitEquipment(true);
    InitAmmo();
    InitFood();
    InitPotions();
    InitPet();

    uint32 money = urand(level * 1000, level * 5 * 1000);
    if (bot->GetMoney() < money)
    {
        bot->SetMoney(money);
    }
    bot->SaveToDB();
}

/**
 * Randomizes the player bot's attributes and equipment without incremental changes.
 */
void PlayerbotFactory::CleanRandomize()
{
    Randomize(false);
}

/**
 * Prepares the player bot for randomization by setting initial attributes and flags.
 */
void PlayerbotFactory::Prepare()
{
    if (!itemQuality)
    {
        if (level <= 10)
        {
            itemQuality = urand(ITEM_QUALITY_NORMAL, ITEM_QUALITY_UNCOMMON);
        }
        else if (level <= 20)
        {
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_RARE);
        }
        else if (level <= 40)
        {
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_EPIC);
        }
        else if (level < 60)
        {
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_EPIC);
        }
        else
        {
            itemQuality = urand(ITEM_QUALITY_RARE, ITEM_QUALITY_EPIC);
        }
    }

    if (bot->IsDead())
    {
        bot->ResurrectPlayer(1.0f, false);
    }

    bot->CombatStop(true);
    bot->SetLevel(level);
    if (!sPlayerbotAIConfig.randomBotShowHelmet)
    {
        bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
    }

    if (!sPlayerbotAIConfig.randomBotShowCloak)
    {
        bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
    }
}

/**
 * Randomizes the player bot's attributes and equipment.
 * @param incremental Whether to apply incremental changes.
 */
void PlayerbotFactory::Randomize(bool incremental)
{
    sLog.outDetail("Preparing to randomize...");
    Prepare();

    sLog.outDetail("Resetting player...");
    bot->resetTalents(true);
    ClearSpells();
    ClearInventory();
    bot->SaveToDB();

    sLog.outDetail("Initializing quests...");
    InitQuests();
    // quest rewards boost bot level, so reduce back
    bot->SetLevel(level);
    ClearInventory();
    bot->SetUInt32Value(PLAYER_XP, 0);
    CancelAuras();
    bot->SaveToDB();

    sLog.outDetail("Initializing spells (step 1)...");
    InitAvailableSpells();

    sLog.outDetail("Initializing skills (step 1)...");
    InitSkills();
    InitTradeSkills();

    sLog.outDetail("Initializing talents...");
    InitTalents();

    sLog.outDetail("Initializing spells (step 2)...");
    InitAvailableSpells();
    InitSpecialSpells();

    sLog.outDetail("Initializing mounts...");
    InitMounts();

    sLog.outDetail("Initializing skills (step 2)...");
    UpdateTradeSkills();
    bot->SaveToDB();

    sLog.outDetail("Initializing equipment...");
    InitEquipment(incremental);

    sLog.outDetail("Initializing bags...");
    InitBags();

    sLog.outDetail("Initializing ammo...");
    InitAmmo();

    sLog.outDetail("Initializing food...");
    InitFood();

    sLog.outDetail("Initializing potions...");
    InitPotions();

    sLog.outDetail("Initializing second equipment set...");
    InitSecondEquipmentSet();

    sLog.outDetail("Initializing inventory...");
    InitInventory();

    sLog.outDetail("Initializing guilds...");
    InitGuild();

    sLog.outDetail("Initializing pet...");
    InitPet();

    sLog.outDetail("Saving to DB...");
    bot->SetMoney(urand(level * 1000, level * 5 * 1000));
    bot->SetHealth(bot->GetMaxHealth());
    bot->SaveToDB();
    sLog.outDetail("Done.");
}

/**
 * Initializes the player bot's pet.
 */
void PlayerbotFactory::InitPet()
{
    Pet* pet = bot->GetPet();
    if (!pet)
    {
        if (bot->getClass() != CLASS_HUNTER)
        {
            return;
        }

        Map* map = bot->GetMap();
        if (!map)
        {
            return;
        }

        vector<uint32> ids;
        for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
        {
            CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
            if (!co)
            {
                continue;
            }

            if (!co->isTameable(bot->CanTameExoticPets()))
            {
                continue;
            }

            if ((int)co->MinLevel > (int)bot->getLevel())
            {
                continue;
            }

            ids.push_back(id);
        }

        if (ids.empty())
        {
            sLog.outError("No pets available for bot %s (%d level)", bot->GetName(), bot->getLevel());
            return;
        }

        for (int i = 0; i < 100; i++)
        {
            int index = urand(0, ids.size() - 1);
            CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(ids[index]);
            if (!co)
            {
                continue;
            }

            uint32 guid = map->GenerateLocalLowGuid(HIGHGUID_PET);
            CreatureCreatePos pos(map, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation(), bot->GetPhaseMask());
            uint32 pet_number = sObjectMgr.GeneratePetNumber();
            pet = new Pet(HUNTER_PET);
            if (!pet->Create(guid, pos, co, pet_number))
            {
                delete pet;
                pet = NULL;
                continue;
            }

            pet->SetOwnerGuid(bot->GetObjectGuid());
            pet->SetCreatorGuid(bot->GetObjectGuid());
            pet->setFaction(bot->getFaction());
            pet->SetLevel(bot->getLevel());
            pet->InitStatsForLevel(bot->getLevel());
            // pet->SetLoyaltyLevel(BEST_FRIEND);
            // pet->SetPower(POWER_HAPPINESS, HAPPINESS_LEVEL_SIZE * 2);
            pet->GetCharmInfo()->SetPetNumber(sObjectMgr.GeneratePetNumber(), true);
            pet->AIM_Initialize();
            pet->InitPetCreateSpells();
            bot->SetPet(pet);
            bot->SetPetGuid(pet->GetObjectGuid());

            sLog.outDebug("Bot %s: assign pet %d (%d level)", bot->GetName(), co->Entry, bot->getLevel());
            pet->SavePetToDB(PET_SAVE_AS_CURRENT);
            bot->PetSpellInitialize();
            break;
        }
    }

    pet = bot->GetPet();
    if (pet)
    {
        pet->InitStatsForLevel(bot->getLevel());
        pet->SetLevel(bot->getLevel());
        // pet->SetLoyaltyLevel(BEST_FRIEND);
        // pet->SetPower(POWER_HAPPINESS, HAPPINESS_LEVEL_SIZE * 2);
        pet->SetHealth(pet->GetMaxHealth());
    }
    else
    {
        sLog.outError("Cannot create pet for bot %s", bot->GetName());
        return;
    }

    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        uint32 spellId = itr->first;
        if (IsPassiveSpell(spellId))
        {
            continue;
        }

        pet->ToggleAutocast(spellId, true);
    }
}

/**
 * Clears the player bot's spells.
 */
void PlayerbotFactory::ClearSpells()
{
    list<uint32> spells;
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
        {
            continue;
        }

        spells.push_back(spellId);
    }

    for (list<uint32>::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        bot->removeSpell(*i, false, false);
    }
}

/**
 * Initializes the player bot's spells.
 */
void PlayerbotFactory::InitSpells()
{
    for (int i = 0; i < 15; i++)
    {
        InitAvailableSpells();
    }
}

/**
 * Initializes the player bot's talents.
 */
void PlayerbotFactory::InitTalents()
{
    uint32 point = urand(0, 100);
    uint8 cls = bot->getClass();
    uint32 p1 = sPlayerbotAIConfig.specProbability[cls][0];
    uint32 p2 = p1 + sPlayerbotAIConfig.specProbability[cls][1];

    uint32 specNo = (point < p1 ? 0 : (point < p2 ? 1 : 2));
    InitTalents(specNo);

    if (bot->GetFreeTalentPoints())
    {
        InitTalents(2 - specNo);
    }
}

/**
 * Visitor class for destroying items in the player bot's inventory.
 */
class DestroyItemsVisitor : public IterateItemsVisitor
{
public:
    DestroyItemsVisitor(Player* bot) : IterateItemsVisitor(), bot(bot) {}

    virtual bool Visit(Item* item)
    {
        uint32 id = item->GetProto()->ItemId;
        if (CanKeep(id))
        {
            keep.insert(id);
            return true;
        }

        bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        return true;
    }

private:
    bool CanKeep(uint32 id)
    {
        if (keep.find(id) != keep.end())
        {
            return false;
        }

        if (sPlayerbotAIConfig.IsInRandomQuestItemList(id))
        {
            return true;
        }

        ItemPrototype const* proto = sItemStorage.LookupEntry<ItemPrototype>(id);
        if (proto->Class == ITEM_CLASS_MISC && proto->SubClass == ITEM_SUBCLASS_JUNK)
        {
            return true;
        }

        return false;
    }

private:
    Player* bot;
    set<uint32> keep;
};

/**
 * Checks if the player bot can equip the specified armor item.
 * @param proto The item prototype.
 * @return True if the player bot can equip the item, false otherwise.
 */
bool PlayerbotFactory::CanEquipArmor(ItemPrototype const* proto)
{
    if (bot->HasSkill(SKILL_SHIELD) && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
    {
        return true;
    }

    if (bot->HasSkill(SKILL_PLATE_MAIL))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_PLATE)
        {
            return false;
        }
    }
    else if (bot->HasSkill(SKILL_MAIL))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_MAIL)
        {
            return false;
        }
    }
    else if (bot->HasSkill(SKILL_LEATHER))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_LEATHER)
        {
            return false;
        }
    }

    if (proto->Quality <= ITEM_QUALITY_NORMAL)
    {
        return true;
    }

    uint8 sp = 0, ap = 0, tank = 0;
    for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
    {
        // for ItemStatValue != 0
        if (!proto->ItemStat[j].ItemStatValue)
        {
            continue;
        }

        AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
    }

    return CheckItemStats(sp, ap, tank);
}

/**
 * Checks if the player bot's item stats are valid.
 * @param sp The spell power stat.
 * @param ap The attack power stat.
 * @param tank The tank stat.
 * @return True if the item stats are valid, false otherwise.
 */
bool PlayerbotFactory::CheckItemStats(uint8 sp, uint8 ap, uint8 tank)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            if (!sp || ap > sp || tank > sp)
            {
                return false;
            }
            break;
        case CLASS_PALADIN:
        case CLASS_WARRIOR:
            if ((!ap && !tank) || sp > ap || sp > tank)
            {
                return false;
            }
            break;
        case CLASS_HUNTER:
        case CLASS_ROGUE:
            if (!ap || sp > ap || sp > tank)
            {
                return false;
            }
            break;
    }

    return sp || ap || tank;
}

/**
 * Adds item stats to the player bot.
 * @param mod The stat modifier.
 * @param sp The spell power stat.
 * @param ap The attack power stat.
 * @param tank The tank stat.
 */
void PlayerbotFactory::AddItemStats(uint32 mod, uint8 &sp, uint8 &ap, uint8 &tank)
{
    switch (mod)
    {
        case ITEM_MOD_HEALTH:
        case ITEM_MOD_STAMINA:
        case ITEM_MOD_INTELLECT:
        case ITEM_MOD_SPIRIT:
            sp++;
            break;
    }

    switch (mod)
    {
        case ITEM_MOD_AGILITY:
        case ITEM_MOD_STRENGTH:
        case ITEM_MOD_HEALTH:
        case ITEM_MOD_STAMINA:
            tank++;
            break;
    }

    switch (mod)
    {
        case ITEM_MOD_HEALTH:
        case ITEM_MOD_STAMINA:
        case ITEM_MOD_AGILITY:
        case ITEM_MOD_STRENGTH:
            ap++;
            break;
    }
}

/**
 * Checks if the player bot can equip the specified weapon item.
 * @param proto The item prototype.
 * @return True if the player bot can equip the item, false otherwise.
 */
bool PlayerbotFactory::CanEquipWeapon(ItemPrototype const* proto)
{
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE)
                return false;
            break;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                return false;
            break;
        case CLASS_WARRIOR:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
                return false;
            break;
        case CLASS_PALADIN:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                return false;
            break;
        case CLASS_SHAMAN:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
                return false;
            break;
        case CLASS_DRUID:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
                return false;
            break;
        case CLASS_HUNTER:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW)
                return false;
            break;
        case CLASS_ROGUE:
            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
                return false;
            break;
    }

    return true;
}

/**
 * Checks if the player bot can equip the specified item.
 * @param proto The item prototype.
 * @param desiredQuality The desired quality of the item.
 * @return True if the player bot can equip the item, false otherwise.
 */
bool PlayerbotFactory::CanEquipItem(ItemPrototype const* proto, uint32 desiredQuality)
{
    if (proto->Duration & 0x80000000)
    {
        return false;
    }

    if (proto->Quality != desiredQuality)
    {
        return false;
    }

    if (proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_WHEN_USE)
    {
        return false;
    }

    if (proto->Class == ITEM_CLASS_CONTAINER)
    {
        return true;
    }

    uint32 requiredLevel = proto->RequiredLevel;
    if (!requiredLevel)
    {
        return false;
    }

    uint32 level = bot->getLevel();
    uint32 delta = 2;
    if (level < 15)
    {
        delta = urand(7, 15);
    }
    else if (proto->Class == ITEM_CLASS_WEAPON || proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
    {
        delta = urand(2, 3);
    }
    else if (!(level % 10) || (level % 10) == 9)
    {
        delta = 2;
    }
    else if (level < 40)
    {
        delta = urand(5, 10);
    }
    else if (level < 60)
    {
        delta = urand(3, 7);
    }
    else if (level < 70)
    {
        delta = urand(2, 5);
    }
    else if (level < 80)
    {
        delta = urand(2, 4);
    }

    if (desiredQuality > ITEM_QUALITY_NORMAL &&
        (requiredLevel > level || requiredLevel < level - delta))
        return false;

    for (uint32 gap = 60; gap <= 80; gap += 10)
    {
        if (level > gap && requiredLevel <= gap)
        {
            return false;
        }
    }

    return true;
}

/**
 * Initializes the player bot's equipment.
 * @param incremental Whether to apply incremental changes.
 */
void PlayerbotFactory::InitEquipment(bool incremental)
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor, ITERATE_ALL_ITEMS);

    map<uint8, vector<uint32> > items;
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
        {
            continue;
        }

        uint32 desiredQuality = itemQuality;
        if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL)
        {
            desiredQuality--;
        }

        do
        {
            for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
            {
                ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
                if (!proto)
                {
                    continue;
                }

                if (proto->Class != ITEM_CLASS_WEAPON &&
                    proto->Class != ITEM_CLASS_ARMOR &&
                    proto->Class != ITEM_CLASS_CONTAINER &&
                    proto->Class != ITEM_CLASS_PROJECTILE)
                    continue;

                if (!CanEquipItem(proto, desiredQuality))
                {
                    continue;
                }

                if (proto->Class == ITEM_CLASS_ARMOR && (
                    slot == EQUIPMENT_SLOT_HEAD ||
                    slot == EQUIPMENT_SLOT_SHOULDERS ||
                    slot == EQUIPMENT_SLOT_CHEST ||
                    slot == EQUIPMENT_SLOT_WAIST ||
                    slot == EQUIPMENT_SLOT_LEGS ||
                    slot == EQUIPMENT_SLOT_FEET ||
                    slot == EQUIPMENT_SLOT_WRISTS ||
                    slot == EQUIPMENT_SLOT_HANDS) && !CanEquipArmor(proto))
                {
                    continue;
                }

                if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(proto))
                {
                    continue;
                }

                if (slot == EQUIPMENT_SLOT_OFFHAND && bot->getClass() == CLASS_ROGUE && proto->Class != ITEM_CLASS_WEAPON)
                {
                    continue;
                }
                if (slot == EQUIPMENT_SLOT_OFFHAND && bot->getClass() == CLASS_PALADIN && proto->SubClass != ITEM_SUBCLASS_ARMOR_SHIELD)
                {
                    continue;
                }

                uint16 dest = 0;
                if (CanEquipUnseenItem(slot, dest, itemId))
                {
                    items[slot].push_back(itemId);
                }
            }
        } while (items[slot].empty() && desiredQuality-- > ITEM_QUALITY_NORMAL);
    }

    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
        {
            continue;
        }

        vector<uint32>& ids = items[slot];
        if (ids.empty())
        {
            sLog.outDebug("%s: no items to equip for slot %d", bot->GetName(), slot);
            continue;
        }

        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];
            Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

            if (incremental && !IsDesiredReplacement(oldItem))
            {
                continue;
            }

            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
            {
                continue;
            }

            if (oldItem)
            {
                bot->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                oldItem->DestroyForPlayer(bot);
            }

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                bot->AutoUnequipOffhandIfNeed();
                EnchantItem(newItem);
                break;
            }
        }
    }
}
/**
 * Checks if the given item is a desired replacement for the current item.
 * @param item The current item.
 * @return True if the item is a desired replacement, false otherwise.
 */
bool PlayerbotFactory::IsDesiredReplacement(Item* item)
{
    if (!item)
    {
        return true;
    }

    ItemPrototype const* proto = item->GetProto();
    int delta = 1 + (80 - bot->getLevel()) / 10;
    return (int)bot->getLevel() - (int)proto->RequiredLevel > delta;
}

/**
 * Initializes the second equipment set for the player bot.
 */
void PlayerbotFactory::InitSecondEquipmentSet()
{
    // Skip for classes that do not need a second equipment set
    if (bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_WARLOCK || bot->getClass() == CLASS_PRIEST)
    {
        return;
    }

    map<uint32, vector<uint32> > items;

    uint32 desiredQuality = itemQuality;
    while (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
        desiredQuality--;
    }

    do
    {
        for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
        {
            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
            {
                continue;
            }

            if (!CanEquipItem(proto, desiredQuality))
            {
                continue;
            }

            if (proto->Class == ITEM_CLASS_WEAPON)
            {
                if (!CanEquipWeapon(proto))
                {
                    continue;
                }

                Item* existingItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (existingItem)
                {
                    switch (existingItem->GetProto()->SubClass)
                    {
                        case ITEM_SUBCLASS_WEAPON_AXE:
                        case ITEM_SUBCLASS_WEAPON_DAGGER:
                        case ITEM_SUBCLASS_WEAPON_FIST:
                        case ITEM_SUBCLASS_WEAPON_MACE:
                        case ITEM_SUBCLASS_WEAPON_SWORD:
                            if (proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE || proto->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER ||
                                proto->SubClass == ITEM_SUBCLASS_WEAPON_FIST || proto->SubClass == ITEM_SUBCLASS_WEAPON_MACE ||
                                proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD)
                                continue;
                            break;
                        default:
                            if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE && proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                                proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                                continue;
                            break;
                    }
                }
            }
            else if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
            {
                if (!CanEquipArmor(proto))
                {
                    continue;
                }

                Item* existingItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (existingItem && existingItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            items[proto->Class].push_back(itemId);
        }
    } while (items[ITEM_CLASS_ARMOR].empty() && items[ITEM_CLASS_WEAPON].empty() && desiredQuality-- > ITEM_QUALITY_NORMAL);

    for (map<uint32, vector<uint32> >::iterator i = items.begin(); i != items.end(); ++i)
    {
        vector<uint32>& ids = i->second;
        if (ids.empty())
        {
            sLog.outDebug(  "%s: no items to make second equipment set for slot %d", bot->GetName(), i->first);
            continue;
        }

        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];

            Item* newItem = bot->StoreNewItemInInventorySlot(newItemId, 1);
            if (newItem)
            {
                EnchantItem(newItem);
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                break;
            }
        }
    }
}

/**
 * Initializes the bags for the player bot.
 */
void PlayerbotFactory::InitBags()
{
    vector<uint32> ids;

    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto || proto->Class != ITEM_CLASS_CONTAINER)
        {
            continue;
        }

        if (!CanEquipItem(proto, ITEM_QUALITY_NORMAL))
        {
            continue;
        }

        ids.push_back(itemId);
    }

    if (ids.empty())
    {
        sLog.outError("%s: no bags found", bot->GetName());
        return;
    }

    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];

            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
            {
                continue;
            }

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                break;
            }
        }
    }
}

/**
 * Enchants the given item for the player bot.
 * @param item The item to enchant.
 */
void PlayerbotFactory::EnchantItem(Item* item)
{
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance)
        return;

    if (bot->getLevel() < urand(40, 50))
        return;

    const ItemPrototype* proto = item->GetProto();
    int32 itemLevel = proto->ItemLevel;
    uint8 botLevel = bot->getLevel();

    std::vector<uint32> validEnchantIds;

    for (uint32 id = 0; id < sSpellEffectStore.GetNumRows(); ++id)
    {
        const SpellEffectEntry* effect = sSpellEffectStore.LookupEntry(id);
        if (!effect || effect->Effect != SPELL_EFFECT_ENCHANT_ITEM)
            continue;

        const SpellLevelsEntry* levels = sSpellLevelsStore.LookupEntry(effect->EffectSpellId);
        if (!levels)
            continue;

        int32 requiredLevel = levels->maxLevel;
        if (requiredLevel && (requiredLevel > itemLevel || requiredLevel < itemLevel - 35))
            continue;

        if (levels->maxLevel && botLevel > levels->maxLevel)
            continue;

        uint32 spellLevel = levels->baseLevel;
        if (spellLevel && (spellLevel > botLevel || spellLevel < botLevel - 10))
            continue;

        uint32 enchantId = effect->EffectMiscValue;
        if (!enchantId)
            continue;

        const SpellItemEnchantmentEntry* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!enchant || enchant->slot != PERM_ENCHANTMENT_SLOT)
            continue;

        const SpellEntry* enchantSpell = sSpellStore.LookupEntry(enchant->spellid[0]);
        if (!enchantSpell || (enchantSpell->GetSpellLevel() && enchantSpell->GetSpellLevel() > botLevel))
            continue;

        uint8 sp = 0, ap = 0, tank = 0;
        for (int i = 0; i < 3; ++i)
        {
            if (enchant->type[i] != ITEM_ENCHANTMENT_TYPE_STAT)
                continue;

            AddItemStats(enchant->spellid[i], sp, ap, tank);
        }

        if (!CheckItemStats(sp, ap, tank))
            continue;

        const SpellEntry* castSpell = sSpellStore.LookupEntry(effect->EffectSpellId);
        if (!castSpell || !item->IsFitToSpellRequirements(castSpell))
            continue;

        validEnchantIds.push_back(enchantId);
    }

    if (validEnchantIds.empty())
    {
        sLog.outDebug("%s: no enchantments found for item %d", bot->GetName(), proto->ItemId);
        return;
    }

    uint32 selectedEnchantId = validEnchantIds[urand(0, validEnchantIds.size() - 1)];
    const SpellItemEnchantmentEntry* selectedEnchant = sSpellItemEnchantmentStore.LookupEntry(selectedEnchantId);

    if (!selectedEnchant)
        return;

    bot->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
    item->SetEnchantment(PERM_ENCHANTMENT_SLOT, selectedEnchantId, 0, 0);
    bot->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);
}


/**
 * Checks if the player bot can equip the specified unseen item.
 * @param slot The equipment slot.
 * @param dest The destination slot.
 * @param item The item ID.
 * @return True if the player bot can equip the item, false otherwise.
 */
bool PlayerbotFactory::CanEquipUnseenItem(uint8 slot, uint16 &dest, uint32 item)
{
    dest = 0;
    Item *pItem = Item::CreateItem(item, 1, bot);
    if (pItem)
    {
        InventoryResult result = bot->CanEquipItem(slot, dest, pItem, true, false);
        pItem->RemoveFromUpdateQueueOf(bot);
        delete pItem;
        return result == EQUIP_ERR_OK;
    }

    return false;
}

/**
 * Initializes the trade skills for the player bot.
 */
#define PLAYER_SKILL_INDEX(x)       (PLAYER_PROFESSION_SKILL_LINE_1 + ((x)*3))
void PlayerbotFactory::InitTradeSkills()
{
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        bot->SetSkill(tradeSkills[i], 0, 0, 0);
    }

    bot->SetUInt32Value(PLAYER_SKILL_INDEX(0), 0);
    bot->SetUInt32Value(PLAYER_SKILL_INDEX(1), 0);

    vector<uint32> firstSkills;
    vector<uint32> secondSkills;
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            firstSkills.push_back(SKILL_MINING);
            secondSkills.push_back(SKILL_BLACKSMITHING);
            secondSkills.push_back(SKILL_ENGINEERING);
            break;
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_HUNTER:
        case CLASS_ROGUE:
            firstSkills.push_back(SKILL_SKINNING);
            secondSkills.push_back(SKILL_LEATHERWORKING);
            break;
        default:
            firstSkills.push_back(SKILL_TAILORING);
            secondSkills.push_back(SKILL_ENCHANTING);
    }

    SetRandomSkill(SKILL_FIRST_AID);
    SetRandomSkill(SKILL_FISHING);
    SetRandomSkill(SKILL_COOKING);

    switch (urand(0, 3))
    {
        case 0:
            SetRandomSkill(SKILL_HERBALISM);
            SetRandomSkill(SKILL_ALCHEMY);
            break;
        case 1:
            SetRandomSkill(SKILL_HERBALISM);
            break;
        case 2:
            SetRandomSkill(SKILL_MINING);
#if !defined(CLASSIC)
            SetRandomSkill(SKILL_JEWELCRAFTING);
#endif
            break;
        case 3:
            SetRandomSkill(firstSkills[urand(0, firstSkills.size() - 1)]);
            SetRandomSkill(secondSkills[urand(0, secondSkills.size() - 1)]);
            break;
    }
}

/**
 * Updates the trade skills for the player bot.
 */
void PlayerbotFactory::UpdateTradeSkills()
{
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        if (bot->GetSkillValue(tradeSkills[i]) == 1)
        {
            bot->SetSkill(tradeSkills[i], 0, 0, 0);
        }
    }
}

/**
 * Initializes the skills for the player bot based on its class and level.
 */
void PlayerbotFactory::InitSkills() //Cosine
{
    uint32 maxValue = level * 5;
    if (bot->getLevel() >= 70)
    {
        bot->SetSkill(SKILL_RIDING, 300, 300);
    }
    else if (bot->getLevel() >= 60)
    {
        bot->SetSkill(SKILL_RIDING, 225, 225);
    }
    else if (bot->getLevel() >= 40)
    {
        bot->SetSkill(SKILL_RIDING, 150, 150);
    }
    else if (bot->getLevel() >= 20)
    {
        bot->SetSkill(SKILL_RIDING, 75, 75);
    }
    else
    {
        bot->SetSkill(SKILL_RIDING, 0, 0);
    }

    uint32 skillLevel = bot->getLevel() < 40 ? 0 : 1;
    switch (bot->getClass())
    {
        case CLASS_DRUID:
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_POLEARMS);
            SetRandomSkill(SKILL_FIST_WEAPONS);
            break;
        case CLASS_WARRIOR:
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            SetRandomSkill(SKILL_BOWS);
            SetRandomSkill(SKILL_GUNS);
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_CROSSBOWS);
            SetRandomSkill(SKILL_POLEARMS);
            SetRandomSkill(SKILL_FIST_WEAPONS);
            SetRandomSkill(SKILL_THROWN);
            break;
        case CLASS_PALADIN:
            bot->SetSkill(SKILL_PLATE_MAIL, 0, skillLevel, skillLevel);
            SetRandomSkill(SKILL_SWORDS);
            SetRandomSkill(SKILL_AXES);
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_2H_SWORDS);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_POLEARMS);
            break;
        case CLASS_PRIEST:
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_WANDS);
            break;
        case CLASS_SHAMAN:
            SetRandomSkill(SKILL_AXES);
            SetRandomSkill(SKILL_MACES);
            SetRandomSkill(SKILL_STAVES);
            SetRandomSkill(SKILL_2H_MACES);
            SetRandomSkill(SKILL_2H_AXES);
            SetRandomSkill(SKILL_DAGGERS);
            SetRandomSkill(SKILL_FIST_WEAPONS);
            break;
        case CLASS_MAGE:
             SetRandomSkill(SKILL_SWORDS);
             SetRandomSkill(SKILL_STAVES);
             SetRandomSkill(SKILL_DAGGERS);
             SetRandomSkill(SKILL_WANDS);
             break;
        case CLASS_WARLOCK:
             SetRandomSkill(SKILL_SWORDS);
             SetRandomSkill(SKILL_STAVES);
             SetRandomSkill(SKILL_DAGGERS);
             SetRandomSkill(SKILL_WANDS);
             break;
        case CLASS_HUNTER:
             SetRandomSkill(SKILL_SWORDS);
             SetRandomSkill(SKILL_AXES);
             SetRandomSkill(SKILL_BOWS);
             SetRandomSkill(SKILL_GUNS);
             SetRandomSkill(SKILL_2H_SWORDS);
             SetRandomSkill(SKILL_STAVES);
             SetRandomSkill(SKILL_2H_AXES);
             SetRandomSkill(SKILL_DAGGERS);
             SetRandomSkill(SKILL_CROSSBOWS);
             SetRandomSkill(SKILL_POLEARMS);
             SetRandomSkill(SKILL_FIST_WEAPONS);
             SetRandomSkill(SKILL_THROWN);
             bot->SetSkill(SKILL_MAIL, 0, skillLevel, skillLevel);
             break;
       case CLASS_ROGUE:
             SetRandomSkill(SKILL_SWORDS);
             SetRandomSkill(SKILL_AXES);
             SetRandomSkill(SKILL_BOWS);
             SetRandomSkill(SKILL_GUNS);
             SetRandomSkill(SKILL_MACES);
             SetRandomSkill(SKILL_DAGGERS);
             SetRandomSkill(SKILL_CROSSBOWS);
             SetRandomSkill(SKILL_FIST_WEAPONS);
             SetRandomSkill(SKILL_THROWN);
    }
}

/**
 * Sets a random skill value for the player bot.
 * @param id The skill ID.
 */
void PlayerbotFactory::SetRandomSkill(uint16 id)
{
    uint32 maxValue = level * 5;
    uint32 curValue = urand(maxValue - level, maxValue);
    bot->SetSkill(id, curValue, maxValue);
}

/**
 * Initializes the available spells for the player bot.
 */
void PlayerbotFactory::InitAvailableSpells()
{
    // Learn default spells for the bot
    bot->learnDefaultSpells();

    // Iterate through all creature entries
    for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
    {
        CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!co)
        {
            continue;
        }

        // Check if the creature is a trainer for tradeskills or class
        if (co->TrainerType != TRAINER_TYPE_TRADESKILLS && co->TrainerType != TRAINER_TYPE_CLASS)
        {
            continue;
        }

        // Check if the trainer's class matches the bot's class
        if (co->TrainerType == TRAINER_TYPE_CLASS && co->TrainerClass != bot->getClass())
        {
            continue;
        }

        uint32 trainerId = co->TrainerTemplateId;
        if (!trainerId)
        {
            trainerId = co->Entry;
        }

        // Get the trainer's spells
        TrainerSpellData const* trainer_spells = sObjectMgr.GetNpcTrainerTemplateSpells(trainerId);
        if (!trainer_spells)
        {
            trainer_spells = sObjectMgr.GetNpcTrainerSpells(trainerId);
        }

        if (!trainer_spells)
        {
            continue;
        }

        // Iterate through the trainer's spells and learn them if the bot meets the requirements
        for (TrainerSpellMap::const_iterator itr = trainer_spells->spellList.begin(); itr != trainer_spells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            if (!tSpell)
            {
                continue;
            }

            uint32 reqLevel = 0;

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);
            TrainerSpellState state = bot->GetTrainerSpellState(tSpell, reqLevel);
            if (state != TRAINER_SPELL_GREEN)
            {
                continue;
            }

            bot->learnSpell(tSpell->spell, false);
        }
    }
}

/**
 * Initializes special spells for the player bot.
 */
void PlayerbotFactory::InitSpecialSpells()
{
    // Iterate through the list of random bot spell IDs and learn each spell
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotSpellIds.begin(); i != sPlayerbotAIConfig.randomBotSpellIds.end(); ++i)
    {
        uint32 spellId = *i;
        bot->learnSpell(spellId, false);
    }
}

/**
 * Initializes the talents for the player bot based on the specified specialization number.
 * @param specNo The specialization number.
 */
void PlayerbotFactory::InitTalents(uint32 specNo)
{
    uint32 classMask = bot->getClassMask();

    // Map to store spells by talent row
    map<uint32, vector<TalentEntry const*> > spells;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if(!talentInfo)
        {
            continue;
        }

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry( talentInfo->TalentTab );
        if(!talentTabInfo || talentTabInfo->tabpage != specNo)
        {
            continue;
        }

        if( (classMask & talentTabInfo->ClassMask) == 0 )
        {
            continue;
        }

        spells[talentInfo->Row].push_back(talentInfo);
    }

    uint32 freePoints = bot->GetFreeTalentPoints();
    for (map<uint32, vector<TalentEntry const*> >::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        vector<TalentEntry const*> &spells = i->second;
        if (spells.empty())
        {
            sLog.outError("%s: No spells for talent row %d", bot->GetName(), i->first);
            continue;
        }

        int attemptCount = 0;
        while (!spells.empty() && (int)freePoints - (int)bot->GetFreeTalentPoints() < 5 && attemptCount++ < 3 && bot->GetFreeTalentPoints())
        {
            int index = urand(0, spells.size() - 1);
            TalentEntry const *talentInfo = spells[index];
            for (int rank = 0; rank < MAX_TALENT_RANK && bot->GetFreeTalentPoints(); ++rank)
            {
                uint32 spellId = talentInfo->RankID[rank];
                if (!spellId)
                {
                    continue;
                }
                bot->learnSpell(spellId, false);
                bot->UpdateFreeTalentPoints(false);
            }
            spells.erase(spells.begin() + index);
        }

        freePoints = bot->GetFreeTalentPoints();
    }
}

/**
 * Retrieves a random bot from the list of random bot accounts.
 * @return The GUID of the random bot.
 */
ObjectGuid PlayerbotFactory::GetRandomBot()
{
    vector<ObjectGuid> guids;
    // Iterate through the list of random bot accounts
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        // Check if the account has any characters
        if (!sAccountMgr.GetCharactersCount(accountId))
        {
            continue;
        }

        // Query the database for character GUIDs associated with the account
        QueryResult *result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account` = '%u'", accountId);
        if (!result)
        {
            continue;
        }

        // Add the character GUIDs to the list if they are not already in use
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid guid = ObjectGuid(fields[0].GetUInt64());
            if (!sObjectMgr.GetPlayer(guid))
            {
                guids.push_back(guid);
            }
        } while (result->NextRow());

        delete result;
    }

    // Return a random GUID from the list
    if (guids.empty())
    {
        return ObjectGuid();
    }

    int index = urand(0, guids.size() - 1);
    return guids[index];
}

/**
 * Adds previous quests to the list of quest IDs.
 * @param questId The quest ID.
 * @param questIds The list of quest IDs.
 */
void AddPrevQuests(uint32 questId, list<uint32>& questIds)
{
    Quest const *quest = sObjectMgr.GetQuestTemplate(questId);
    // Recursively add previous quests to the list
    for (Quest::PrevQuests::const_iterator iter = quest->prevQuests.begin(); iter != quest->prevQuests.end(); ++iter)
    {
        uint32 prevId = abs(*iter);
        AddPrevQuests(prevId, questIds);
        questIds.remove(prevId);
        questIds.push_back(prevId);
    }
}

/**
 * Initializes the quests for the player bot.
 */
void PlayerbotFactory::InitQuests()
{
    // Initialize the list of class-specific quest IDs if it is empty
    if (classQuestIds.empty())
    {
        ObjectMgr::QuestMap const& questTemplates = sObjectMgr.GetQuestTemplates();
        for (ObjectMgr::QuestMap::const_iterator i = questTemplates.begin(); i != questTemplates.end(); ++i)
        {
            uint32 questId = i->first;
            Quest const *quest = i->second;

            if (!quest->GetRequiredClasses() || quest->IsRepeatable())
            {
                continue;
            }

            AddPrevQuests(questId, classQuestIds);
            classQuestIds.remove(questId);
            classQuestIds.push_back(questId);
        }
    }

    int count = 0;
    // Iterate through the list of class-specific quest IDs and complete the quests for the bot
    for (list<uint32>::iterator i = classQuestIds.begin(); i != classQuestIds.end(); ++i)
    {
        uint32 questId = *i;
        Quest const *quest = sObjectMgr.GetQuestTemplate(questId);

        if (!bot->SatisfyQuestClass(quest, false) ||
                quest->GetMinLevel() > bot->getLevel() ||
                !bot->SatisfyQuestRace(quest, false))
            continue;

        bot->SetQuestStatus(questId, QUEST_STATUS_COMPLETE);
        bot->RewardQuest(quest, 0, bot, false);
        if (!(count++ % 10))
        {
            ClearInventory();
        }
    }
    ClearInventory();
}

/**
 * Clears the inventory of the player bot.
 */
void PlayerbotFactory::ClearInventory()
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor);
}

/**
 * Initializes the ammo for the player bot.
 */
void PlayerbotFactory::InitAmmo()
{
    // Check if the bot's class requires ammo
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE && bot->getClass() != CLASS_WARRIOR)
    {
        return;
    }

    Item* const pItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!pItem)
    {
        return;
    }

    uint32 subClass = 0;
    // Determine the type of ammo required based on the ranged weapon
    switch (pItem->GetProto()->SubClass)
    {
    case ITEM_SUBCLASS_WEAPON_GUN:
        subClass = ITEM_SUBCLASS_BULLET;
        break;
    case ITEM_SUBCLASS_WEAPON_BOW:
    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
        subClass = ITEM_SUBCLASS_ARROW;
        break;
    }

    if (!subClass)
    {
        return;
    }

    // Query the database for the highest level ammo that the bot can use
    QueryResult *results = WorldDatabase.PQuery("select max(`entry`), max(`RequiredLevel`) from `item_template` where `class` = '%u' and `subclass` = '%u' and `RequiredLevel` <= '%u'",
            ITEM_CLASS_PROJECTILE, subClass, bot->getLevel());
    if (!results)
    {
        return;
    }

    Field* fields = results->Fetch();
    if (fields)
    {
        uint32 entry = fields[0].GetUInt32();
        // Add the ammo to the bot's inventory
        for (int i = 0; i < 10; i++)
        {
            Item* newItem = bot->StoreNewItemInInventorySlot(entry, 200);
            if (newItem)
            {
                newItem->AddToUpdateQueueOf(bot);
            }
        }
        bot->SetAmmo(entry);
    }

    delete results;
}

/**
 * Initializes the mounts for the player bot.
 */
void PlayerbotFactory::InitMounts()
{
    std::map<int32, std::vector<uint32>> spells;

    for (uint32 id = 0; id < sSpellEffectStore.GetNumRows(); ++id)
    {
        SpellEffectEntry const* effectEntry = sSpellEffectStore.LookupEntry(id);
        if (!effectEntry)
            continue;

        // We only want mount-related effects
        if (effectEntry->Effect != SPELL_AURA_MOUNTED)
            continue;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
        if (!spellInfo)
            continue;

        // Must be instant cast (mounts usually are)
        if (spellInfo->CastingTimeIndex && GetSpellCastTime(spellInfo) > 500)
            continue;

        // Must be permanent duration
        if (GetSpellDuration(spellInfo) != -1)
            continue;

        int32 speed = effectEntry->EffectBasePoints;
        if (speed < 50)
            continue;

        spells[speed].push_back(id);
    }

    for (uint32 type = 0; type < 2; ++type)
    {
        for (auto& [speed, spellIds] : spells)
        {
            if (spellIds.empty())
                continue;

            uint32 index = urand(0, spellIds.size() - 1);
            bot->learnSpell(spellIds[index], false);
        }
    }
}


/**
 * Initializes the potions for the player bot.
 */
void PlayerbotFactory::InitPotions()
{
    map<uint32, vector<uint32> > items;
    // Iterate through all item entries and find potions that the bot can use
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_CONSUMABLE ||
            proto->SubClass != ITEM_SUBCLASS_POTION ||
            proto->Spells[0].SpellCategory != 4 ||
            proto->Bonding != NO_BIND)
        {
            continue;
        }

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
        {
            continue;
        }

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
        {
            continue;
        }

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
        {
            continue;
        }

        // Add the potion to the list of items
        for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
        {
            const SpellEntry* const spellInfo = sSpellStore.LookupEntry(proto->Spells[j].SpellId);
            if (!spellInfo)
            {
                continue;
            }

            for (int i = 0 ; i < 3; i++)
            {
                const SpellEffectEntry const* spelleffectz = spellInfo->GetSpellEffect(SpellEffectIndex(i));
                if (spelleffectz->Effect == SPELL_EFFECT_HEAL || spelleffectz->Effect == SPELL_EFFECT_ENERGIZE)
                {
                    items[spelleffectz->Effect].push_back(itemId);
                    break;
                }
            }
        }
    }

    // Add a random potion to the bot's inventory
    uint32 effects[] = { SPELL_EFFECT_HEAL, SPELL_EFFECT_ENERGIZE };
    for (int i = 0; i < sizeof(effects) / sizeof(uint32); ++i)
    {
        uint32 effect = effects[i];
        vector<uint32>& ids = items[effect];
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        uint32 itemId = ids[index];
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        Item* newItem = bot->StoreNewItemInInventorySlot(itemId, urand(1, proto->GetMaxStackSize()));
        if (newItem)
        {
            newItem->AddToUpdateQueueOf(bot);
        }
   }
}

/**
 * Initializes the food for the player bot.
 */
void PlayerbotFactory::InitFood()
{
    map<uint32, vector<uint32> > items;
    // Iterate through all item entries and find food that the bot can use
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_CONSUMABLE ||
            proto->SubClass != ITEM_SUBCLASS_FOOD ||
            (proto->Spells[0].SpellCategory != 11 && proto->Spells[0].SpellCategory != 59) ||
            proto->Bonding != NO_BIND)
        {
            continue;
        }

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
        {
            continue;
        }

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
        {
            continue;
        }

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
        {
            continue;
        }

        items[proto->Spells[0].SpellCategory].push_back(itemId);
    }

    // Add a random food item to the bot's inventory
    uint32 categories[] = { 11, 59 };
    for (int i = 0; i < sizeof(categories) / sizeof(uint32); ++i)
    {
        uint32 category = categories[i];
        vector<uint32>& ids = items[category];
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        uint32 itemId = ids[index];
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        Item* newItem = bot->StoreNewItemInInventorySlot(itemId, urand(1, proto->GetMaxStackSize()));
        if (newItem)
        {
            newItem->AddToUpdateQueueOf(bot);
        }
   }
}

/**
 * Cancels all auras on the player bot.
 */
void PlayerbotFactory::CancelAuras()
{
    bot->RemoveAllAuras();
}

/**
 * Initializes the inventory for the player bot.
 */
void PlayerbotFactory::InitInventory()
{
    InitInventoryTrade();
    InitInventoryEquip();
    InitInventorySkill();
}

/**
 * Initializes the skill-related items in the player bot's inventory.
 */
void PlayerbotFactory::InitInventorySkill()
{
    if (bot->HasSkill(SKILL_MINING))
    {
        StoreItem(2901, 1); // Mining Pick
    }
#if !defined(CLASSIC)
    if (bot->HasSkill(SKILL_JEWELCRAFTING))
    {
        StoreItem(20815, 1); // Jeweler's Kit
        StoreItem(20824, 1); // Simple Grinder
    }
#endif
    if (bot->HasSkill(SKILL_BLACKSMITHING) || bot->HasSkill(SKILL_ENGINEERING))
    {
        StoreItem(5956, 1); // Blacksmith Hammer
    }
    if (bot->HasSkill(SKILL_ENGINEERING))
    {
        StoreItem(6219, 1); // Arclight Spanner
    }
    if (bot->HasSkill(SKILL_ENCHANTING))
    {
        StoreItem(16207, 1); // Runed Arcanite Rod
    }
    if (bot->HasSkill(SKILL_SKINNING))
    {
        StoreItem(7005, 1); // Skinning Knife
    }
}

/**
 * Stores an item in the player bot's inventory.
 * @param itemId The item ID.
 * @param count The quantity of the item.
 * @return The stored item.
 */
Item* PlayerbotFactory::StoreItem(uint32 itemId, uint32 count)
{
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    ItemPosCountVec sDest;
    InventoryResult msg = bot->CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, sDest, itemId, count);
    if (msg != EQUIP_ERR_OK)
    {
        return NULL;
    }

    return bot->StoreNewItem(sDest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
}

/**
 * Initializes the trade-related items in the player bot's inventory.
 */
void PlayerbotFactory::InitInventoryTrade()
{
    vector<uint32> ids;
    // Iterate through all item entries and find trade goods that the bot can use
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_TRADE_GOODS || proto->Bonding != NO_BIND)
        {
            continue;
        }

        if (proto->ItemLevel < bot->getLevel())
        {
            continue;
        }

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
        {
            continue;
        }

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
        {
            continue;
        }

        ids.push_back(itemId);
    }

    if (ids.empty())
    {
        sLog.outError("No trade items available for bot %s (%d level)", bot->GetName(), bot->getLevel());
        return;
    }

    // Add a random trade good item to the bot's inventory
    uint32 index = urand(0, ids.size() - 1);
    if (index >= ids.size())
    {
        return;
    }

    uint32 itemId = ids[index];
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto)
    {
        return;
    }

    uint32 count = 1, stacks = 1;
    switch (proto->Quality)
    {
        case ITEM_QUALITY_NORMAL:
            count = proto->GetMaxStackSize();
            stacks = urand(1, 7) / auctionbot.GetRarityPriceMultiplier(proto);
            break;
        case ITEM_QUALITY_UNCOMMON:
            stacks = 1;
            count = urand(1, proto->GetMaxStackSize());
            break;
        case ITEM_QUALITY_RARE:
            stacks = 1;
            count = urand(1, min(uint32(3), proto->GetMaxStackSize()));
            break;
    }

    for (uint32 i = 0; i < stacks; i++)
    {
        StoreItem(itemId, count);
    }
}

/**
 * Initializes the equipment for the player bot.
 */
void PlayerbotFactory::InitInventoryEquip()
{
    vector<uint32> ids;

    // Determine the desired quality of items
    uint32 desiredQuality = itemQuality;
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL)
    {
        desiredQuality--;
    }

    // Iterate through all item entries and find items that the bot can equip
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        // Check if the item is armor or weapon and if it can be equipped by the bot
        if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON || (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_WHEN_USE))
        {
            continue;
        }

        if (proto->Class == ITEM_CLASS_ARMOR && !CanEquipArmor(proto))
        {
            continue;
        }

        if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(proto))
        {
            continue;
        }

        if (!CanEquipItem(proto, desiredQuality))
        {
            continue;
        }

        ids.push_back(itemId);
    }

    // Add a random number of items to the bot's inventory
    int maxCount = urand(0, 3);
    int count = 0;
    for (int attempts = 0; attempts < 15; attempts++)
    {
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        uint32 itemId = ids[index];
        if (StoreItem(itemId, 1) && count++ >= maxCount)
        {
            break;
        }
   }
}

/**
 * Initializes the guild for the player bot.
 */
void PlayerbotFactory::InitGuild()
{
    // Check if the bot is already in a guild
    if (bot->GetGuildId())
    {
        return;
    }

    // Create random guilds if the list is empty
    if (sPlayerbotAIConfig.randomBotGuilds.empty())
    {
        RandomPlayerbotFactory::CreateRandomGuilds();
    }

    vector<uint32> guilds;
    for(list<uint32>::iterator i = sPlayerbotAIConfig.randomBotGuilds.begin(); i != sPlayerbotAIConfig.randomBotGuilds.end(); ++i)
    {
        guilds.push_back(*i);
    }

    if (guilds.empty())
    {
        sLog.outError("No random guilds available");
        return;
    }

    // Add the bot to a random guild
    int index = urand(0, guilds.size() - 1);
    uint32 guildId = guilds[index];
    Guild* guild = sGuildMgr.GetGuildById(guildId);
    if (!guild)
    {
        sLog.outError("Invalid guild %u", guildId);
        return;
    }

    if (guild->GetMemberSize() < 10)
    {
        guild->AddMember(bot->GetObjectGuid(), urand(GR_OFFICER, GR_INITIATE));
    }
}
