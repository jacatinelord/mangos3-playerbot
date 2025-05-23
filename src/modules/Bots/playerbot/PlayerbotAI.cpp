#include "../botpch.h"
#include "PlayerbotMgr.h"
#include "playerbot.h"

#include "AiFactory.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "strategy/values/LastMovementValue.h"
#include "strategy/actions/LogLevelAction.h"
#include "strategy/values/LastSpellCastValue.h"
#include "LootObjectStack.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "PlayerbotSecurity.h"
#include "Util.h"
#include "Group.h"
#include "Pet.h"
#include "SpellAuras.h"
#include "../ahbot/AhBot.h"
#include "GuildTaskMgr.h"
#include "PlayerbotDbStore.h"

using namespace ai;
using namespace std;

// Function declarations
vector<string> &split(const string &s, char delim, vector<string> &elems);
vector<string> split(const string &s, char delim);
char *strstri(string str1, string str2);
uint64 extractGuid(WorldPacket &packet);

/**
 * Extracts the quest ID from a string.
 * @param str The input string.
 * @return The extracted quest ID.
 */
uint32 PlayerbotChatHandler::extractQuestId(string str)
{
    char *source = (char *)str.c_str();
    char *cId = ExtractKeyFromLink(&source, "Hquest");
    return cId ? atol(cId) : 0;
}

/**
 * Adds a packet handler for a specific opcode.
 * @param opcode The opcode.
 * @param handler The handler name.
 */
void PacketHandlingHelper::AddHandler(uint16 opcode, string handler)
{
    handlers[opcode] = handler;
}

/**
 * Handles packets using the provided ExternalEventHelper.
 * @param helper The ExternalEventHelper instance.
 */
void PacketHandlingHelper::Handle(ExternalEventHelper &helper)
{
    while (!queue.empty())
    {
        helper.HandlePacket(handlers, queue.top());
        queue.pop();
    }
}

/**
 * Adds a packet to the queue for handling.
 * @param packet The packet to add.
 */
void PacketHandlingHelper::AddPacket(const WorldPacket &packet)
{
    if (handlers.find(packet.GetOpcode()) != handlers.end())
    {
        queue.push(WorldPacket(packet));
    }
}

/**
 * Default constructor for PlayerbotAI.
 */
PlayerbotAI::PlayerbotAI() : PlayerbotAIBase(), bot(NULL), aiObjectContext(NULL),
                             currentEngine(NULL), chatHelper(this), chatFilter(this), accountId(0), security(NULL), master(NULL), currentState(BOT_STATE_NON_COMBAT)
{
    for (int i = 0; i < BOT_STATE_MAX; i++)
    {
        engines[i] = NULL;
    }
}

/**
 * Constructor for PlayerbotAI with a bot parameter.
 * @param bot The player bot.
 */
PlayerbotAI::PlayerbotAI(Player *bot) : PlayerbotAIBase(), chatHelper(this), chatFilter(this), security(bot), master(NULL)
{
    this->bot = bot;

    accountId = sObjectMgr.GetPlayerAccountIdByGUID(bot->GetObjectGuid());

    aiObjectContext = AiFactory::createAiObjectContext(bot, this);

    engines[BOT_STATE_COMBAT] = AiFactory::createCombatEngine(bot, this, aiObjectContext);
    engines[BOT_STATE_NON_COMBAT] = AiFactory::createNonCombatEngine(bot, this, aiObjectContext);
    engines[BOT_STATE_DEAD] = AiFactory::createDeadEngine(bot, this, aiObjectContext);
    currentEngine = engines[BOT_STATE_NON_COMBAT];
    currentState = BOT_STATE_NON_COMBAT;

    masterIncomingPacketHandlers.AddHandler(CMSG_GAMEOBJ_USE, "use game object");
    masterIncomingPacketHandlers.AddHandler(CMSG_AREATRIGGER, "area trigger");
    masterIncomingPacketHandlers.AddHandler(CMSG_GAMEOBJ_USE, "use game object");
    masterIncomingPacketHandlers.AddHandler(CMSG_LOOT_ROLL, "loot roll");
    masterIncomingPacketHandlers.AddHandler(CMSG_GOSSIP_HELLO, "gossip hello");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_HELLO, "gossip hello");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_COMPLETE_QUEST, "complete quest");
    masterIncomingPacketHandlers.AddHandler(CMSG_QUESTGIVER_ACCEPT_QUEST, "accept quest");
    masterIncomingPacketHandlers.AddHandler(CMSG_ACTIVATETAXI, "activate taxi");
    masterIncomingPacketHandlers.AddHandler(CMSG_ACTIVATETAXIEXPRESS, "activate taxi");
    masterIncomingPacketHandlers.AddHandler(CMSG_MOVE_SPLINE_DONE, "taxi done");
    masterIncomingPacketHandlers.AddHandler(CMSG_GROUP_UNINVITE_GUID, "uninvite");
    masterIncomingPacketHandlers.AddHandler(CMSG_PUSHQUESTTOPARTY, "quest share");
    masterIncomingPacketHandlers.AddHandler(CMSG_GUILD_INVITE, "guild invite");

    botOutgoingPacketHandlers.AddHandler(SMSG_GROUP_INVITE, "group invite");
    botOutgoingPacketHandlers.AddHandler(BUY_ERR_NOT_ENOUGHT_MONEY, "not enough money");
    botOutgoingPacketHandlers.AddHandler(BUY_ERR_REPUTATION_REQUIRE, "not enough reputation");
    botOutgoingPacketHandlers.AddHandler(SMSG_GROUP_SET_LEADER, "group set leader");
    botOutgoingPacketHandlers.AddHandler(SMSG_FORCE_RUN_SPEED_CHANGE, "check mount state");
    botOutgoingPacketHandlers.AddHandler(SMSG_RESURRECT_REQUEST, "resurrect request");
    botOutgoingPacketHandlers.AddHandler(SMSG_INVENTORY_CHANGE_FAILURE, "cannot equip");
    botOutgoingPacketHandlers.AddHandler(SMSG_TRADE_STATUS, "trade status");
    botOutgoingPacketHandlers.AddHandler(SMSG_LOOT_RESPONSE, "loot response");
    botOutgoingPacketHandlers.AddHandler(SMSG_QUESTUPDATE_ADD_KILL, "quest objective completed");
    botOutgoingPacketHandlers.AddHandler(SMSG_ITEM_PUSH_RESULT, "item push result");
    botOutgoingPacketHandlers.AddHandler(SMSG_PARTY_COMMAND_RESULT, "party command");
    botOutgoingPacketHandlers.AddHandler(SMSG_CAST_FAILED, "cast failed");
    botOutgoingPacketHandlers.AddHandler(SMSG_DUEL_REQUESTED, "duel requested");
    botOutgoingPacketHandlers.AddHandler(SMSG_INVENTORY_CHANGE_FAILURE, "inventory change failure");

    masterOutgoingPacketHandlers.AddHandler(SMSG_PARTY_COMMAND_RESULT, "party command");
    masterOutgoingPacketHandlers.AddHandler(MSG_RAID_READY_CHECK, "ready check");
    masterOutgoingPacketHandlers.AddHandler(MSG_RAID_READY_CHECK_FINISHED, "ready check finished");
}

/**
 * Destructor for PlayerbotAI.
 */
PlayerbotAI::~PlayerbotAI()
{
    for (int i = 0; i < BOT_STATE_MAX; i++)
    {
        if (engines[i])
        {
            delete engines[i];
        }
    }

    if (aiObjectContext)
    {
        delete aiObjectContext;
    }
}

/**
 * Internal update method for PlayerbotAI.
 * @param elapsed The elapsed time since the last update.
 */
void PlayerbotAI::UpdateAIInternal(uint32 elapsed)
{
    if (bot->IsBeingTeleported())
    {
        return;
    }

    ExternalEventHelper helper(aiObjectContext);
    while (!chatCommands.empty())
    {
        ChatCommandHolder holder = chatCommands.front();
        string command = holder.GetCommand();
        Player *owner = holder.GetOwner();
        if (!helper.ParseChatCommand(command, owner) && holder.GetType() == CHAT_MSG_WHISPER)
        {
            ostringstream out;
            out << "Unknown command " << command;
            TellMaster(out);
            helper.ParseChatCommand("help");
        }
        chatCommands.pop();
    }

    botOutgoingPacketHandlers.Handle(helper);
    masterIncomingPacketHandlers.Handle(helper);
    masterOutgoingPacketHandlers.Handle(helper);

    DoNextAction();
}

/**
 * Handles teleport acknowledgment for the bot.
 */
void PlayerbotAI::HandleTeleportAck()
{
    bot->GetMotionMaster()->Clear(true);
    bot->InterruptMoving(1);
    if (bot->IsBeingTeleportedNear())
    {
        WorldPacket p = WorldPacket(CMSG_MOVE_TELEPORT_ACK, 8 + 4 + 4);
        p << bot->GetObjectGuid();
        p << (uint32)0;       // supposed to be flags? not used currently
        p << (uint32)time(0); // time - not currently used
        bot->GetSession()->HandleMoveTeleportAckOpcode(p);
    }
    else if (bot->IsBeingTeleportedFar())
    {
        bot->GetSession()->HandleMoveWorldportAckOpcode();
        SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
    }
}

/**
 * Resets the bot's state and strategies.
 */
void PlayerbotAI::Reset()
{
    if (bot->IsTaxiFlying())
    {
        return;
    }

    currentEngine = engines[BOT_STATE_NON_COMBAT];
    nextAICheckDelay = 0;
    whispers.clear();

    aiObjectContext->GetValue<Unit *>("old target")->Set(NULL);
    aiObjectContext->GetValue<Unit *>("current target")->Set(NULL);
    aiObjectContext->GetValue<LootObject>("loot target")->Set(LootObject());
    aiObjectContext->GetValue<uint32>("lfg proposal")->Set(0);

    LastSpellCast &lastSpell = aiObjectContext->GetValue<LastSpellCast &>("last spell cast")->Get();
    lastSpell.Reset();

    aiObjectContext->GetValue<LastMovement &>("last movement")->Get().Set(NULL);
    aiObjectContext->GetValue<LastMovement &>("last area trigger")->Get().Set(NULL);
    aiObjectContext->GetValue<LastMovement &>("last taxi")->Get().Set(NULL);

    bot->GetMotionMaster()->Clear();
    bot->m_taxi.ClearTaxiDestinations();
    InterruptSpell();

    for (int i = 0; i < BOT_STATE_MAX; i++)
    {
        engines[i]->Init();
    }
}

map<string, ChatMsg> chatMap;

/**
 * Handles a command from the master.
 * @param type The type of the command.
 * @param text The command text.
 * @param fromPlayer The player who sent the command.
 */
void PlayerbotAI::HandleCommand(uint32 type, const string &text, Player &fromPlayer)
{
    if (!GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, type != CHAT_MSG_WHISPER, &fromPlayer))
    {
        return;
    }

    if (type == CHAT_MSG_ADDON)
    {
        return;
    }

    string filtered = text;
    if (!sPlayerbotAIConfig.commandPrefix.empty())
    {
        if (filtered.find(sPlayerbotAIConfig.commandPrefix) != 0)
        {
            return;
        }

        filtered = filtered.substr(sPlayerbotAIConfig.commandPrefix.size());
    }

    if (chatMap.empty())
    {
        chatMap["#w "] = CHAT_MSG_WHISPER;
        chatMap["#p "] = CHAT_MSG_PARTY;
        chatMap["#r "] = CHAT_MSG_RAID;
        chatMap["#a "] = CHAT_MSG_ADDON;
        chatMap["#g "] = CHAT_MSG_GUILD;
    }
    currentChat = pair<ChatMsg, time_t>(CHAT_MSG_WHISPER, 0);
    for (map<string, ChatMsg>::iterator i = chatMap.begin(); i != chatMap.end(); ++i)
    {
        if (filtered.find(i->first) == 0)
        {
            filtered = filtered.substr(3);
            currentChat = pair<ChatMsg, time_t>(i->second, time(0) + 2);
            break;
        }
    }

    filtered = chatFilter.Filter(trim((string &)filtered));
    if (filtered.empty())
    {
        return;
    }

    if (filtered.substr(0, 6) == "debug ")
    {
        string response = HandleRemoteCommand(filtered.substr(6));
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_ADDON, response.c_str(), LANG_ADDON,
                                     CHAT_TAG_NONE, bot->GetObjectGuid(), bot->GetName());
        fromPlayer.GetSession()->SendPacket(&data);
        return;
    }

    if (filtered.find("who") != 0 && !GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_ALLOW_ALL, type != CHAT_MSG_WHISPER, &fromPlayer))
    {
        return;
    }

    if (type == CHAT_MSG_RAID_WARNING && filtered.find(bot->GetName()) != string::npos && filtered.find("award") == string::npos)
    {
        ChatCommandHolder cmd("warning", &fromPlayer, type);
        chatCommands.push(cmd);
        return;
    }

    if (filtered.size() > 2 && filtered.substr(0, 2) == "d " || filtered.size() > 3 && filtered.substr(0, 3) == "do ")
    {
        std::string action = filtered.substr(filtered.find(" ") + 1);
        DoSpecificAction(action);
    }
    else if (filtered == "reset")
    {
        Reset();
    }
    else
    {
        ChatCommandHolder cmd(filtered, &fromPlayer, type);
        chatCommands.push(cmd);
    }
}

/**
 * Handles outgoing packets from the bot.
 * @param packet The packet to handle.
 */
void PlayerbotAI::HandleBotOutgoingPacket(const WorldPacket &packet)
{
    switch (packet.GetOpcode())
    {
    case SMSG_SPELL_FAILURE:
    {
        WorldPacket p(packet);
        p.rpos(0);
        ObjectGuid casterGuid;
        p >> casterGuid.ReadAsPacked();
        if (casterGuid != bot->GetObjectGuid())
        {
            return;
        }

        uint32 spellId;
        p >> spellId;
        SpellInterrupted(spellId);
        return;
    }
    case SMSG_SPELL_DELAYED:
    {
        WorldPacket p(packet);
        p.rpos(0);
        ObjectGuid casterGuid;
        p >> casterGuid.ReadAsPacked();

        if (casterGuid != bot->GetObjectGuid())
        {
            return;
        }

        uint32 delaytime;
        p >> delaytime;
        if (delaytime <= 1000)
        {
            IncreaseNextCheckDelay(delaytime);
        }
        return;
    }
    default:
        botOutgoingPacketHandlers.AddPacket(packet);
    }
}

/**
 * Handles spell interruption for the bot.
 * @param spellid The ID of the interrupted spell.
 */
void PlayerbotAI::SpellInterrupted(uint32 spellid)
{
    LastSpellCast &lastSpell = aiObjectContext->GetValue<LastSpellCast &>("last spell cast")->Get();
    if (!spellid || lastSpell.id != spellid)
    {
        return;
    }

    lastSpell.Reset();

    time_t now = time(0);
    if (now <= lastSpell.time)
    {
        return;
    }

    uint32 castTimeSpent = 1000 * (now - lastSpell.time);

    int32 globalCooldown = CalculateGlobalCooldown(lastSpell.id);
    if (castTimeSpent < globalCooldown)
    {
        SetNextCheckDelay(globalCooldown - castTimeSpent);
    }
    else
    {
        SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
    }

    lastSpell.id = 0;
}

/**
 * Calculates the global cooldown for a spell.
 * @param spellid The ID of the spell.
 * @return The global cooldown in milliseconds.
 */
int32 PlayerbotAI::CalculateGlobalCooldown(uint32 spellid)
{
    if (!spellid)
    {
        return 0;
    }

    if (bot->HasSpellCooldown(spellid))
    {
        return sPlayerbotAIConfig.globalCoolDown;
    }

    return sPlayerbotAIConfig.reactDelay;
}

/**
 * Handles incoming packets from the master.
 * @param packet The packet to handle.
 */
void PlayerbotAI::HandleMasterIncomingPacket(const WorldPacket &packet)
{
    masterIncomingPacketHandlers.AddPacket(packet);
}

/**
 * Handles outgoing packets to the master.
 * @param packet The packet to handle.
 */
void PlayerbotAI::HandleMasterOutgoingPacket(const WorldPacket &packet)
{
    masterOutgoingPacketHandlers.AddPacket(packet);
}

/**
 * Changes the current engine to the specified type.
 * @param type The type of the engine.
 */
void PlayerbotAI::ChangeEngine(BotState type)
{
    Engine *engine = engines[type];

    if (currentEngine != engine)
    {
        currentEngine = engine;
        currentState = type;
        ReInitCurrentEngine();

        switch (type)
        {
        case BOT_STATE_COMBAT:
            sLog.outDebug("=== %s COMBAT ===", bot->GetName());
            break;
        case BOT_STATE_NON_COMBAT:
            sLog.outDebug("=== %s NON-COMBAT ===", bot->GetName());
            break;
        case BOT_STATE_DEAD:
            sLog.outDebug("=== %s DEAD ===", bot->GetName());
            break;
        }
    }
}

/**
 * Executes the next action for the bot.
 */
void PlayerbotAI::DoNextAction()
{
    if (bot->IsBeingTeleported() || (GetMaster() && GetMaster()->IsBeingTeleported()))
    {
        return;
    }

    currentEngine->DoNextAction(NULL);

    if (currentEngine != engines[BOT_STATE_DEAD] && !bot->IsAlive())
    {
        ChangeEngine(BOT_STATE_DEAD);
    }

    if (currentEngine == engines[BOT_STATE_DEAD] && bot->IsAlive())
    {
        ChangeEngine(BOT_STATE_NON_COMBAT);
    }

    Group *group = bot->GetGroup();
    if (!master && group)
    {
        for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player *member = gref->getSource();
            PlayerbotAI *ai = bot->GetPlayerbotAI();
            if (member && member->IsInWorld() && !member->GetPlayerbotAI() && (!master || master->GetPlayerbotAI()))
            {
                ai->SetMaster(member);
                ai->ResetStrategies();
                ai->TellMaster("Hello");
                break;
            }
        }
    }
}

/**
 * Reinitializes the current engine.
 */
void PlayerbotAI::ReInitCurrentEngine()
{
    InterruptSpell();
    currentEngine->Init();
}

/**
 * Changes the strategy for the specified engine type.
 * @param names The names of the strategies.
 * @param type The type of the engine.
 */
void PlayerbotAI::ChangeStrategy(string names, BotState type) const
{
    Engine *e = engines[type];
    if (!e)
    {
        return;
    }

    e->ChangeStrategy(names);
}

/**
 * Clears all strategies for the specified engine type.
 * @param type The type of the engine.
 */
void PlayerbotAI::ClearStrategies(BotState type) const
{
    Engine *e = engines[type];
    if (!e)
    {
        return;
    }

    e->removeAllStrategies();
}

/**
 * Retrieves the list of strategies for the specified engine type.
 * @param type The type of the engine.
 * @return The list of strategies.
 */
list<string> PlayerbotAI::GetStrategies(BotState type) const
{
    Engine *e = engines[type];
    if (!e)
    {
        return list<string>();
    }

    return e->GetStrategies();
}

/**
 * Executes a specific action for the bot.
 * @param name The name of the action.
 */
void PlayerbotAI::DoSpecificAction(string name)
{
    for (int i = 0; i < BOT_STATE_MAX; i++)
    {
        ostringstream out;
        ActionResult res = engines[i]->ExecuteAction(name);
        switch (res)
        {
        case ACTION_RESULT_UNKNOWN:
            continue;
        case ACTION_RESULT_OK:
            out << name << ": done";
            TellMaster(out);
            PlaySound(TEXTEMOTE_NOD);
            return;
        case ACTION_RESULT_IMPOSSIBLE:
            out << name << ": impossible";
            TellMaster(out);
            PlaySound(TEXTEMOTE_NO);
            return;
        case ACTION_RESULT_USELESS:
            out << name << ": useless";
            TellMaster(out);
            PlaySound(TEXTEMOTE_NO);
            return;
        case ACTION_RESULT_FAILED:
            out << name << ": failed";
            TellMaster(out);
            return;
        }
    }
    ostringstream out;
    out << name << ": unknown action";
    TellMaster(out);
}

/**
 * Plays a sound for the bot.
 * @param emote The emote ID.
 * @return True if the sound was played, false otherwise.
 */
bool PlayerbotAI::PlaySound(uint32 emote)
{
    // if (EmotesTextSoundEntry const* soundEntry = FindTextSoundEmoteFor(emote, bot->getRace(), bot->getGender()))
    //{
    //     bot->PlayDistanceSound(soundEntry->SoundId);
    //     return true;
    // }

    return false;
}

/**
 * Checks if the bot contains a specific strategy.
 * @param type The type of the strategy.
 * @return True if the strategy is contained, false otherwise.
 */
bool PlayerbotAI::ContainsStrategy(StrategyType type)
{
    for (int i = 0; i < BOT_STATE_MAX; i++)
    {
        if (engines[i]->ContainsStrategy(type))
        {
            return true;
        }
    }
    return false;
}

/**
 * Checks if the bot has a specific strategy.
 * @param name The name of the strategy.
 * @param type The type of the engine.
 * @return True if the strategy is present, false otherwise.
 */
bool PlayerbotAI::HasStrategy(string name, BotState type)
{
    return engines[type]->HasStrategy(name);
}

/**
 * Resets the strategies for the bot.
 */
void PlayerbotAI::ResetStrategies()
{
    for (int i = 0; i < BOT_STATE_MAX; i++)
    {
        engines[i]->removeAllStrategies();
    }

    AiFactory::AddDefaultCombatStrategies(bot, this, engines[BOT_STATE_COMBAT]);
    AiFactory::AddDefaultNonCombatStrategies(bot, this, engines[BOT_STATE_NON_COMBAT]);
    AiFactory::AddDefaultDeadStrategies(bot, this, engines[BOT_STATE_DEAD]);
    sPlayerbotDbStore.Load(this);
}

/**
 * Checks if the player is a ranged class.
 * @param player The player to check.
 * @return True if the player is a ranged class, false otherwise.
 */
bool PlayerbotAI::IsRanged(Player *player)
{
    PlayerbotAI *botAi = player->GetPlayerbotAI();
    if (botAi)
    {
        return botAi->ContainsStrategy(STRATEGY_TYPE_RANGED);
    }

    switch (player->getClass())
    {
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
    case CLASS_ROGUE:
        return false;
    case CLASS_DRUID:
        return !HasAnyAuraOf(player, "cat form", "bear form", "dire bear form", NULL);
    }
    return true;
}

/**
 * Checks if the player is a tank class.
 * @param player The player to check.
 * @return True if the player is a tank class, false otherwise.
 */
bool PlayerbotAI::IsTank(Player *player)
{
    PlayerbotAI *botAi = player->GetPlayerbotAI();
    if (botAi)
    {
        return botAi->ContainsStrategy(STRATEGY_TYPE_TANK);
    }

    switch (player->getClass())
    {
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
        return true;
    case CLASS_DRUID:
        return HasAnyAuraOf(player, "bear form", "dire bear form", NULL);
    }
    return false;
}

/**
 * Checks if the player is a healer class.
 * @param player The player to check.
 * @return True if the player is a healer class, false otherwise.
 */
bool PlayerbotAI::IsHeal(Player *player)
{
    PlayerbotAI *botAi = player->GetPlayerbotAI();
    if (botAi)
    {
        return botAi->ContainsStrategy(STRATEGY_TYPE_HEAL);
    }

    switch (player->getClass())
    {
    case CLASS_PRIEST:
        return true;
    case CLASS_DRUID:
        return HasAnyAuraOf(player, "tree of life form", NULL);
    }
    return false;
}

namespace MaNGOS
{

    /**
     * Checks if a unit is within range based on its GUID.
     */
    class UnitByGuidInRangeCheck
    {
    public:
        UnitByGuidInRangeCheck(WorldObject const *obj, ObjectGuid guid, float range) : i_obj(obj), i_range(range), i_guid(guid) {}
        WorldObject const &GetFocusObject() const { return *i_obj; }
        bool operator()(Unit *u)
        {
            return u->GetObjectGuid() == i_guid && i_obj->IsWithinDistInMap(u, i_range);
        }

    private:
        WorldObject const *i_obj;
        float i_range;
        ObjectGuid i_guid;
    };

    /**
     * Checks if a game object is within range based on its GUID.
     */
    class GameObjectByGuidInRangeCheck
    {
    public:
        GameObjectByGuidInRangeCheck(WorldObject const *obj, ObjectGuid guid, float range) : i_obj(obj), i_range(range), i_guid(guid) {}
        WorldObject const &GetFocusObject() const { return *i_obj; }
        bool operator()(GameObject *u)
        {
            if (u && i_obj->IsWithinDistInMap(u, i_range) && u->isSpawned() && u->GetGOInfo() && u->GetObjectGuid() == i_guid)
            {
                return true;
            }

            return false;
        }

    private:
        WorldObject const *i_obj;
        float i_range;
        ObjectGuid i_guid;
    };

};

/**
 * Retrieves a unit based on its GUID.
 * @param guid The GUID of the unit.
 * @return The unit, or NULL if not found.
 */
Unit *PlayerbotAI::GetUnit(ObjectGuid guid)
{
    if (!guid)
    {
        return NULL;
    }

    Map *map = bot->GetMap();
    if (!map)
    {
        return NULL;
    }

    return sObjectAccessor.GetUnit(*bot, guid);
}

/**
 * Retrieves a creature based on its GUID.
 * @param guid The GUID of the creature.
 * @return The creature, or NULL if not found.
 */
Creature *PlayerbotAI::GetCreature(ObjectGuid guid)
{
    if (!guid)
    {
        return NULL;
    }

    Map *map = bot->GetMap();
    if (!map)
    {
        return NULL;
    }

    return map->GetCreature(guid);
}

/**
 * Retrieves a game object based on its GUID.
 * @param guid The GUID of the game object.
 * @return The game object, or NULL if not found.
 */
GameObject *PlayerbotAI::GetGameObject(ObjectGuid guid)
{
    if (!guid)
    {
        return NULL;
    }

    Map *map = bot->GetMap();
    if (!map)
    {
        return NULL;
    }

    return map->GetGameObject(guid);
}

/**
 * Sends a message to the master without facing the master.
 * @param text The message text.
 * @param securityLevel The required security level.
 * @return True if the message was sent, false otherwise.
 */
bool PlayerbotAI::TellMasterNoFacing(string text, PlayerbotSecurityLevel securityLevel)
{
    Player *master = GetMaster();
    if (!master || master->IsBeingTeleported())
    {
        return false;
    }

    if (!GetSecurity()->CheckLevelFor(securityLevel, true, master))
    {
        return false;
    }

    if (sPlayerbotAIConfig.whisperDistance && !bot->GetGroup() && sRandomPlayerbotMgr.IsRandomBot(bot) &&
        master->GetSession()->GetSecurity() < SEC_GAMEMASTER &&
        (bot->GetMapId() != master->GetMapId() || bot->GetDistance(master) > sPlayerbotAIConfig.whisperDistance))
    {
        return false;
    }

    time_t lastSaid = whispers[text];
    if (!lastSaid || (time(0) - lastSaid) >= sPlayerbotAIConfig.repeatDelay / 1000)
    {
        whispers[text] = time(0);

        ChatMsg type = CHAT_MSG_WHISPER;
        if (currentChat.second - time(0) >= 1)
        {
            type = currentChat.first;
        }

        WorldPacket data;
        ChatHandler::BuildChatPacket(data,
                                     type == CHAT_MSG_ADDON ? CHAT_MSG_PARTY : type,
                                     text.c_str(),
                                     type == CHAT_MSG_ADDON ? LANG_ADDON : LANG_UNIVERSAL,
                                     CHAT_TAG_NONE, bot->GetObjectGuid(), bot->GetName());
        master->GetSession()->SendPacket(&data);
    }
    return true;
}

/**
 * Sends a message to the master.
 * @param text The message text.
 * @param securityLevel The required security level.
 * @return True if the message was sent, false otherwise.
 */
bool PlayerbotAI::TellMaster(string text, PlayerbotSecurityLevel securityLevel)
{
    if (!TellMasterNoFacing(text, securityLevel))
    {
        return false;
    }

    if (!bot->isMoving() && !bot->IsInCombat() && bot->GetMapId() == master->GetMapId() && !bot->IsTaxiFlying())
    {
        if (!bot->IsInFront(master, sPlayerbotAIConfig.sightDistance, CAST_ANGLE_IN_FRONT))
        {
            bot->SetFacingTo(bot->GetAngle(master));
        }

        bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
    }

    return true;
}

/**
 * Checks if an aura is real.
 * @param bot The bot.
 * @param aura The aura to check.
 * @param unit The unit with the aura.
 * @return True if the aura is real, false otherwise.
 */
static bool IsRealAura(Player *bot, Aura const *aura, Unit *unit)
{
    if (!aura)
    {
        return false;
    }

    if (!unit->IsHostileTo(bot))
    {
        return true;
    }

    uint32 stacks = aura->GetStackAmount();
    if (stacks >= aura->GetSpellProto()->GetStackAmount())
    {
        return true;
    }

    if (aura->GetCaster() == bot || IsPositiveSpell(aura->GetSpellProto()) || aura->IsAreaAura())
    {
        return true;
    }

    return false;
}

/**
 * Checks if a unit has a specific aura.
 * @param name The name of the aura.
 * @param unit The unit to check.
 * @return True if the unit has the aura, false otherwise.
 */
bool PlayerbotAI::HasAura(string name, Unit *unit)
{
    if (!unit)
    {
        return false;
    }

    wstring wnamepart;
    if (!Utf8toWStr(name, wnamepart))
    {
        return 0;
    }

    wstrToLower(wnamepart);

    for (uint32 auraType = SPELL_AURA_BIND_SIGHT; auraType < TOTAL_AURAS; auraType++)
    {
        Unit::AuraList const &auras = unit->GetAurasByType((AuraType)auraType);
        for (Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); i++)
        {
            Aura *aura = *i;
            if (!aura)
            {
                continue;
            }

            const string auraName = aura->GetSpellProto()->SpellName[0];
            if (auraName.empty() || auraName.length() != wnamepart.length() || !Utf8FitTo(auraName, wnamepart))
            {
                continue;
            }

            if (IsRealAura(bot, aura, unit))
            {
                return true;
            }
        }
    }

    return false;
}

/**
 * Checks if a unit has a specific aura.
 * @param spellId The ID of the aura.
 * @param unit The unit to check.
 * @return True if the unit has the aura, false otherwise.
 */
bool PlayerbotAI::HasAura(uint32 spellId, const Unit *unit)
{
    if (!spellId || !unit)
    {
        return false;
    }

    for (uint32 effect = EFFECT_INDEX_0; effect <= EFFECT_INDEX_2; effect++)
    {
        Aura *aura = ((Unit *)unit)->GetAura(spellId, (SpellEffectIndex)effect);

        if (IsRealAura(bot, aura, (Unit *)unit))
        {
            return true;
        }
    }

    return false;
}

/**
 * Checks if a unit has any of the specified auras.
 * @param player The unit to check.
 * @param ... The list of aura names.
 * @return True if the unit has any of the auras, false otherwise.
 */
bool PlayerbotAI::HasAnyAuraOf(Unit *player, ...)
{
    if (!player)
    {
        return false;
    }

    va_list vl;
    va_start(vl, player);

    const char *cur;
    do
    {
        cur = va_arg(vl, const char *);
        if (cur && HasAura(cur, player))
        {
            {
                va_end(vl);
            }
            return true;
        }
    } while (cur);

    va_end(vl);
    return false;
}

/**
 * Checks if the bot can cast a spell on a target.
 * @param name The name of the spell.
 * @param target The target unit.
 * @return True if the spell can be cast, false otherwise.
 */
bool PlayerbotAI::CanCastSpell(string name, Unit *target)
{
    return CanCastSpell(aiObjectContext->GetValue<uint32>("spell id", name)->Get(), target);
}

/**
 * Checks if the bot can cast a spell on a target.
 * @param spellid The ID of the spell.
 * @param target The target unit.
 * @param checkHasSpell Whether to check if the bot has the spell.
 * @return True if the spell can be cast, false otherwise.
 */
bool PlayerbotAI::CanCastSpell(uint32 spellid, Unit *target, bool checkHasSpell)
{
    if (!spellid)
    {
        return false;
    }

    if (!target)
    {
        target = bot;
    }

    Pet *pet = bot->GetPet();
    if (pet && pet->HasSpell(spellid))
    {
        return true;
    }

    if (checkHasSpell && !bot->HasSpell(spellid))
    {
        return false;
    }

    if (bot->HasSpellCooldown(spellid))
    {
        return false;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
    {
        return false;
    }

    bool positiveSpell = IsPositiveSpell(spellInfo);
    if (positiveSpell && bot->IsHostileTo(target))
    {
        return false;
    }

    if (!positiveSpell && bot->IsFriendlyTo(target))
    {
        return false;
    }

    if (target->IsImmuneToSpell(spellInfo, false))
    {
        return false;
    }

    if (bot != target && bot->GetDistance(target) > sPlayerbotAIConfig.sightDistance)
    {
        return false;
    }

    ObjectGuid oldSel = bot->GetSelectionGuid();
    bot->SetSelectionGuid(target->GetObjectGuid());
    Spell *spell = new Spell(bot, spellInfo, false);

    spell->m_targets.setUnitTarget(target);
    spell->m_CastItem = aiObjectContext->GetValue<Item *>("item for spell", spellid)->Get();
    spell->m_targets.setItemTarget(spell->m_CastItem);
    SpellCastResult result = spell->CheckCast(false);
    delete spell;
    if (oldSel)
    {
        bot->SetSelectionGuid(oldSel);
    }

    switch (result)
    {
    case SPELL_FAILED_NOT_INFRONT:
    case SPELL_FAILED_NOT_STANDING:
    case SPELL_FAILED_UNIT_NOT_INFRONT:
    case SPELL_FAILED_MOVING:
    case SPELL_FAILED_TRY_AGAIN:
    case SPELL_FAILED_BAD_IMPLICIT_TARGETS:
    case SPELL_FAILED_BAD_TARGETS:
    case SPELL_CAST_OK:
        return true;
    default:
        return false;
    }
}

/**
 * Casts a spell on a target.
 * @param name The name of the spell.
 * @param target The target unit.
 * @return True if the spell was cast, false otherwise.
 */
bool PlayerbotAI::CastSpell(string name, Unit *target)
{
    bool result = CastSpell(aiObjectContext->GetValue<uint32>("spell id", name)->Get(), target);
    if (result)
    {
        aiObjectContext->GetValue<time_t>("last spell cast time", name)->Set(time(0));
    }

    return result;
}

/**
 * Casts a spell on a target.
 * @param spellId The ID of the spell.
 * @param target The target unit.
 * @return True if the spell was cast, false otherwise.
 */
bool PlayerbotAI::CastSpell(uint32 spellId, Unit *target)
{
    if (!spellId)
    {
        return false;
    }

    if (!target)
    {
        target = bot;
    }

    Pet *pet = bot->GetPet();
    SpellEntry const *pSpellInfo = sSpellStore.LookupEntry(spellId);
    if (pet && pet->HasSpell(spellId))
    {
        bool autocast = false;
        for (AutoSpellList::iterator i = pet->m_autospells.begin(); i != pet->m_autospells.end(); ++i)
        {
            if (*i == spellId)
            {
                autocast = true;
                break;
            }
        }

        pet->ToggleAutocast(spellId, !autocast);
        ostringstream out;
        out << (autocast ? "|cffff0000|Disabling" : "|cFF00ff00|Enabling") << " pet auto-cast for ";
        out << chatHelper.formatSpell(pSpellInfo);
        TellMaster(out);
        return true;
    }

    aiObjectContext->GetValue<LastMovement &>("last movement")->Get().Set(NULL);

    MotionMaster &mm = *bot->GetMotionMaster();

    if (bot->IsFlying())
    {
        return false;
    }

    bot->clearUnitState(UNIT_STAT_CHASE);
    bot->clearUnitState(UNIT_STAT_FOLLOW);

    ObjectGuid oldSel = bot->GetSelectionGuid();
    bot->SetSelectionGuid(target->GetObjectGuid());

    Spell *spell = new Spell(bot, pSpellInfo, false);
    if (bot->isMoving() && spell->GetCastTime())
    {
        delete spell;
        spell->cancel();
        return false;
    }

    SpellCastTargets targets;
    WorldObject *faceTo = target;

    if (pSpellInfo->GetTargets() & TARGET_FLAG_ITEM)
    {
        spell->m_CastItem = aiObjectContext->GetValue<Item *>("item for spell", spellId)->Get();
        targets.setItemTarget(spell->m_CastItem);
    }
    else if (pSpellInfo->GetTargets() & TARGET_FLAG_DEST_LOCATION)
    {
        WorldLocation aoe = aiObjectContext->GetValue<WorldLocation>("aoe position")->Get();
        targets.setDestination(aoe.coord_x, aoe.coord_y, aoe.coord_z);
    }
    else if (pSpellInfo->GetTargets() & TARGET_FLAG_SOURCE_LOCATION)
    {
        targets.setDestination(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
    }
    else
    {
        targets.setUnitTarget(target);
    }

    const SpellEffectEntry *spelleffect = pSpellInfo->GetSpellEffect(SpellEffectIndex(0));
    if (spelleffect->Effect == SPELL_EFFECT_OPEN_LOCK ||
        spelleffect->Effect == SPELL_EFFECT_SKINNING)
    {
        LootObject loot = *aiObjectContext->GetValue<LootObject>("loot target");
        if (!loot.IsLootPossible(bot))
        {
            spell->cancel();
            delete spell;
            return false;
        }

        GameObject *go = GetGameObject(loot.guid);
        if (go && go->isSpawned())
        {
            WorldPacket *const packetgouse = new WorldPacket(CMSG_GAMEOBJ_USE, 8);
            *packetgouse << loot.guid;
            bot->GetSession()->QueuePacket(packetgouse);
            targets.setGOTarget(go);
            faceTo = go;
        }
        else
        {
            Unit *creature = GetUnit(loot.guid);
            if (creature)
            {
                targets.setUnitTarget(creature);
                faceTo = creature;
            }
        }
    }

    if (!bot->IsInFront(faceTo, sPlayerbotAIConfig.sightDistance, CAST_ANGLE_IN_FRONT) && !bot->IsTaxiFlying())
    {
        bot->SetFacingTo(bot->GetAngle(faceTo));
        spell->cancel();
        delete spell;
        SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
        return false;
    }

    spell->SpellStart(&targets);
    WaitForSpellCast(spell);
    aiObjectContext->GetValue<LastSpellCast &>("last spell cast")->Get().Set(spellId, target->GetObjectGuid(), time(0));

    if (oldSel)
    {
        bot->SetSelectionGuid(oldSel);
    }

    return true;
}

/**
 * Waits for the spell cast to complete.
 * @param spell The spell being cast.
 */
void PlayerbotAI::WaitForSpellCast(Spell *spell)
{
    const SpellEntry *const pSpellInfo = spell->m_spellInfo;

    float castTime = spell->GetCastTime();
    if (IsChanneledSpell(pSpellInfo))
    {
        int32 duration = GetSpellDuration(pSpellInfo);
        if (duration > 0)
        {
            castTime += duration;
        }
    }

    castTime = ceil(castTime);

    uint32 globalCooldown = CalculateGlobalCooldown(pSpellInfo->Id);
    if (castTime < globalCooldown)
    {
        castTime = globalCooldown;
    }

    SetNextCheckDelay(castTime + sPlayerbotAIConfig.reactDelay);
}

/**
 * Interrupts the current spell being cast by the bot.
 */
void PlayerbotAI::InterruptSpell()
{
    for (int type = CURRENT_MELEE_SPELL; type < CURRENT_CHANNELED_SPELL; type++)
    {
        Spell *spell = bot->GetCurrentSpell((CurrentSpellTypes)type);
        if (!spell)
        {
            continue;
        }

        if (IsPositiveSpell(spell->m_spellInfo))
        {
            continue;
        }

        bot->InterruptSpell((CurrentSpellTypes)type);

        WorldPacket data(SMSG_SPELL_FAILURE, size_t(8 + 1 + 4 + 1));
        data.appendPackGUID(bot->GetObjectGuid().GetRawValue());
        data << uint8(1);
        data << uint32(spell->m_spellInfo->Id);
        data << uint8(0);
        bot->SendMessageToSet(&data, true);

        data.Initialize(SMSG_SPELL_FAILED_OTHER, size_t(8 + 1 + 4 + 1));
        data.appendPackGUID(bot->GetObjectGuid().GetRawValue());
        data << uint8(1);
        data << uint32(spell->m_spellInfo->Id);
        data << uint8(0);
        bot->SendMessageToSet(&data, true);

        SpellInterrupted(spell->m_spellInfo->Id);
    }
}

/**
 * Removes an aura from the bot.
 * @param name The name of the aura.
 */
void PlayerbotAI::RemoveAura(string name)
{
    uint32 spellid = aiObjectContext->GetValue<uint32>("spell id", name)->Get();
    if (spellid && HasAura(spellid, bot))
    {
        bot->RemoveAurasDueToSpell(spellid);
    }
}

/**
 * Checks if a spell being cast by a target can be interrupted.
 * @param target The target unit.
 * @param spell The name of the spell.
 * @return True if the spell can be interrupted, false otherwise.
 */
bool PlayerbotAI::IsInterruptableSpellCasting(Unit *target, string spell)
{
    uint32 spellid = aiObjectContext->GetValue<uint32>("spell id", spell)->Get();
    if (!spellid || !target->IsNonMeleeSpellCasted(true))
    {
        return false;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
    {
        return false;
    }

    if (target->IsImmuneToSpell(spellInfo, false))
    {
        return false;
    }

    for (int32 i = EFFECT_INDEX_0; i <= EFFECT_INDEX_2; i++)
    {
        if ((spellInfo->GetInterruptFlags() & SPELL_INTERRUPT_FLAG_INTERRUPT) && spellInfo->GetPreventionType() == SPELL_PREVENTION_TYPE_SILENCE)
        {
            return true;
        }
        const SpellEffectEntry *spelleffect = spellInfo->GetSpellEffect(SpellEffectIndex(i));
        if ((spelleffect->Effect == SPELL_EFFECT_INTERRUPT_CAST) &&
            !target->IsImmuneToSpellEffect(spellInfo, (SpellEffectIndex)i, true))
        {
            return true;
        }
    }

    return false;
}

/**
 * Checks if a unit has an aura that can be dispelled.
 * @param target The target unit.
 * @param dispelType The type of dispel.
 * @return True if the unit has an aura that can be dispelled, false otherwise.
 */
bool PlayerbotAI::HasAuraToDispel(Unit *target, uint32 dispelType)
{
    // Iterate through all aura types
    for (uint32 type = SPELL_AURA_NONE; type < TOTAL_AURAS; ++type)
    {
        // Get the list of auras of the current type
        Unit::AuraList const &auras = target->GetAurasByType((AuraType)type);
        for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            const Aura *aura = *itr;
            const SpellEntry *entry = aura->GetSpellProto();
            uint32 spellId = entry->Id;

            // Check if the spell is positive or negative
            bool isPositiveSpell = IsPositiveSpell(spellId);
            if (isPositiveSpell && bot->IsFriendlyTo(target))
            {
                continue;
            }

            if (!isPositiveSpell && bot->IsHostileTo(target))
            {
                continue;
            }

            // Check if the aura duration is less than the configured dispel duration
            if (sPlayerbotAIConfig.dispelAuraDuration && aura->GetAuraDuration() && aura->GetAuraDuration() < (int32)sPlayerbotAIConfig.dispelAuraDuration)
            {
                return false;
            }

            // Check if the spell can be dispelled
            if (canDispel(entry, dispelType))
            {
                return true;
            }
        }
    }
    return false;
}

#ifndef WIN32
/**
 * Case-insensitive string comparison.
 * @param s1 The first string.
 * @param s2 The second string.
 * @return The difference between the first non-matching characters.
 */
inline int strcmpi(const char *s1, const char *s2)
{
    for (; *s1 && *s2 && (toupper(*s1) == toupper(*s2)); ++s1, ++s2)
        ;
    {
        return *s1 - *s2;
    }
}
#endif

/**
 * Checks if a spell can be dispelled.
 * @param entry The spell entry.
 * @param dispelType The type of dispel.
 * @return True if the spell can be dispelled, false otherwise.
 */
bool PlayerbotAI::canDispel(const SpellEntry *entry, uint32 dispelType)
{
    if (entry->GetDispel() != dispelType)
    {
        return false;
    }

    // Check if the spell name matches any of the known non-dispellable spells
    return !entry->SpellName[0] ||
           (strcmpi((const char *)entry->SpellName[0], "demon skin") &&
            strcmpi((const char *)entry->SpellName[0], "mage armor") &&
            strcmpi((const char *)entry->SpellName[0], "frost armor") &&
            strcmpi((const char *)entry->SpellName[0], "wavering will") &&
            strcmpi((const char *)entry->SpellName[0], "chilled") &&
            strcmpi((const char *)entry->SpellName[0], "ice armor"));
}

/**
 * Checks if a race is part of the Alliance faction.
 * @param race The race to check.
 * @return True if the race is part of the Alliance, false otherwise.
 */
bool IsAlliance(uint8 race)
{
    return race == RACE_HUMAN || race == RACE_DWARF || race == RACE_NIGHTELF
#if !defined(CLASSIC)
           || race == RACE_DRAENEI || race == RACE_BLOODELF
#endif
           || race == RACE_GNOME;
}

/**
 * Checks if a player is from an opposing faction.
 * @param player The player to check.
 * @return True if the player is from an opposing faction, false otherwise.
 */
bool PlayerbotAI::IsOpposing(Player *player)
{
    return IsOpposing(player->getRace(), bot->getRace());
}

/**
 * Checks if two races are from opposing factions.
 * @param race1 The first race.
 * @param race2 The second race.
 * @return True if the races are from opposing factions, false otherwise.
 */
bool PlayerbotAI::IsOpposing(uint8 race1, uint8 race2)
{
    return (IsAlliance(race1) && !IsAlliance(race2)) || (!IsAlliance(race1) && IsAlliance(race2));
}

/**
 * Removes all shapeshift forms from the bot.
 */
void PlayerbotAI::RemoveShapeshift()
{
    RemoveAura("bear form");
    RemoveAura("dire bear form");
    RemoveAura("moonkin form");
    RemoveAura("travel form");
    RemoveAura("cat form");
    RemoveAura("flight form");
    RemoveAura("swift flight form");
    RemoveAura("aquatic form");
    RemoveAura("ghost wolf");
    RemoveAura("tree of life");
}

/**
 * Calculates the gear score of a player.
 * @param player The player to calculate the gear score for.
 * @param withBags Whether to include items in bags.
 * @param withBank Whether to include items in the bank.
 * @return The gear score.
 */
uint32 PlayerbotAI::GetEquipGearScore(Player *player, bool withBags, bool withBank)
{
    std::vector<uint32> gearScore(EQUIPMENT_SLOT_END);
    uint32 twoHandScore = 0;

    // Calculate gear score for equipped items
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item *item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            _fillGearScoreData(player, item, &gearScore, twoHandScore);
        }
    }

    // Calculate gear score for items in bags
    if (withBags)
    {
        // Check inventory
        for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item *item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                _fillGearScoreData(player, item, &gearScore, twoHandScore);
            }
        }

        // Check bags
        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag *pBag = (Bag *)player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (Item *item2 = pBag->GetItemByPos(j))
                    {
                        _fillGearScoreData(player, item2, &gearScore, twoHandScore);
                    }
                }
            }
        }
    }

    // Calculate gear score for items in the bank
    if (withBank)
    {
        for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            if (Item *item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                _fillGearScoreData(player, item, &gearScore, twoHandScore);
            }
        }

        for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Item *item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (item->IsBag())
                {
                    Bag *bag = (Bag *)item;
                    for (uint8 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        if (Item *item2 = bag->GetItemByPos(j))
                        {
                            _fillGearScoreData(player, item2, &gearScore, twoHandScore);
                        }
                    }
                }
            }
        }
    }

    uint8 count = EQUIPMENT_SLOT_END - 2; // Ignore body and tabard slots
    uint32 sum = 0;

    // Check if 2H weapon is higher level than main hand + off hand
    if (gearScore[EQUIPMENT_SLOT_MAINHAND] + gearScore[EQUIPMENT_SLOT_OFFHAND] < twoHandScore * 2)
    {
        gearScore[EQUIPMENT_SLOT_OFFHAND] = 0; // Off hand is ignored in calculations if 2H weapon has higher score
        --count;
        gearScore[EQUIPMENT_SLOT_MAINHAND] = twoHandScore;
    }

    // Sum up the gear scores
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        sum += gearScore[i];
    }

    // Calculate the average gear score
    if (count)
    {
        uint32 res = uint32(sum / count);
        return res;
    }
    else
    {
        return 0;
    }
}

/**
 * Fills the gear score data for a given item.
 * @param player The player.
 * @param item The item.
 * @param gearScore The gear score data.
 * @param twoHandScore The two-hand weapon score.
 */
void PlayerbotAI::_fillGearScoreData(Player *player, Item *item, std::vector<uint32> *gearScore, uint32 &twoHandScore)
{
    if (!item)
    {
        return;
    }

    if (player->CanUseItem(item->GetProto()) != EQUIP_ERR_OK)
    {
        return;
    }

    uint8 type = item->GetProto()->InventoryType;
    uint32 level = item->GetProto()->ItemLevel;

    // Update the gear score based on the item type
    switch (type)
    {
    case INVTYPE_2HWEAPON:
        twoHandScore = std::max(twoHandScore, level);
        break;
    case INVTYPE_WEAPON:
    case INVTYPE_WEAPONMAINHAND:
        (*gearScore)[SLOT_MAIN_HAND] = std::max((*gearScore)[SLOT_MAIN_HAND], level);
        break;
    case INVTYPE_SHIELD:
    case INVTYPE_WEAPONOFFHAND:
        (*gearScore)[EQUIPMENT_SLOT_OFFHAND] = std::max((*gearScore)[EQUIPMENT_SLOT_OFFHAND], level);
        break;
    case INVTYPE_THROWN:
    case INVTYPE_RANGEDRIGHT:
    case INVTYPE_RANGED:
    case INVTYPE_QUIVER:
    case INVTYPE_RELIC:
        (*gearScore)[EQUIPMENT_SLOT_RANGED] = std::max((*gearScore)[EQUIPMENT_SLOT_RANGED], level);
        break;
    case INVTYPE_HEAD:
        (*gearScore)[EQUIPMENT_SLOT_HEAD] = std::max((*gearScore)[EQUIPMENT_SLOT_HEAD], level);
        break;
    case INVTYPE_NECK:
        (*gearScore)[EQUIPMENT_SLOT_NECK] = std::max((*gearScore)[EQUIPMENT_SLOT_NECK], level);
        break;
    case INVTYPE_SHOULDERS:
        (*gearScore)[EQUIPMENT_SLOT_SHOULDERS] = std::max((*gearScore)[EQUIPMENT_SLOT_SHOULDERS], level);
        break;
    case INVTYPE_BODY:
        (*gearScore)[EQUIPMENT_SLOT_BODY] = std::max((*gearScore)[EQUIPMENT_SLOT_BODY], level);
        break;
    case INVTYPE_CHEST:
        (*gearScore)[EQUIPMENT_SLOT_CHEST] = std::max((*gearScore)[EQUIPMENT_SLOT_CHEST], level);
        break;
    case INVTYPE_WAIST:
        (*gearScore)[EQUIPMENT_SLOT_WAIST] = std::max((*gearScore)[EQUIPMENT_SLOT_WAIST], level);
        break;
    case INVTYPE_LEGS:
        (*gearScore)[EQUIPMENT_SLOT_LEGS] = std::max((*gearScore)[EQUIPMENT_SLOT_LEGS], level);
        break;
    case INVTYPE_FEET:
        (*gearScore)[EQUIPMENT_SLOT_FEET] = std::max((*gearScore)[EQUIPMENT_SLOT_FEET], level);
        break;
    case INVTYPE_WRISTS:
        (*gearScore)[EQUIPMENT_SLOT_WRISTS] = std::max((*gearScore)[EQUIPMENT_SLOT_WRISTS], level);
        break;
    case INVTYPE_HANDS:
        (*gearScore)[EQUIPMENT_SLOT_HEAD] = std::max((*gearScore)[EQUIPMENT_SLOT_HEAD], level);
        break;
    // Equipped gear score check uses both rings and trinkets for calculation, assume that for bags/banks it is the same
    // with keeping second highest score at second slot
    case INVTYPE_FINGER:
    {
        if ((*gearScore)[EQUIPMENT_SLOT_FINGER1] < level)
        {
            (*gearScore)[EQUIPMENT_SLOT_FINGER2] = (*gearScore)[EQUIPMENT_SLOT_FINGER1];
            (*gearScore)[EQUIPMENT_SLOT_FINGER1] = level;
        }
        else if ((*gearScore)[EQUIPMENT_SLOT_FINGER2] < level)
        {
            (*gearScore)[EQUIPMENT_SLOT_FINGER2] = level;
        }
        break;
    }
    case INVTYPE_TRINKET:
    {
        if ((*gearScore)[EQUIPMENT_SLOT_TRINKET1] < level)
        {
            (*gearScore)[EQUIPMENT_SLOT_TRINKET2] = (*gearScore)[EQUIPMENT_SLOT_TRINKET1];
            (*gearScore)[EQUIPMENT_SLOT_TRINKET1] = level;
        }
        else if ((*gearScore)[EQUIPMENT_SLOT_TRINKET2] < level)
        {
            (*gearScore)[EQUIPMENT_SLOT_TRINKET2] = level;
        }
        break;
    }
    case INVTYPE_CLOAK:
        (*gearScore)[EQUIPMENT_SLOT_BACK] = std::max((*gearScore)[EQUIPMENT_SLOT_BACK], level);
        break;
    default:
        break;
    }
}

/**
 * Handles remote commands for the bot.
 * @param command The command to handle.
 * @return The result of the command.
 */
string PlayerbotAI::HandleRemoteCommand(string command)
{
    if (command == "state")
    {
        switch (currentState)
        {
        case BOT_STATE_COMBAT:
            return "combat";
        case BOT_STATE_DEAD:
            return "dead";
        case BOT_STATE_NON_COMBAT:
            return "non-combat";
        default:
            return "unknown";
        }
    }
    else if (command == "position")
    {
        ostringstream out;
        out << bot->GetPositionX() << " " << bot->GetPositionY() << " " << bot->GetPositionZ() << " " << bot->GetMapId() << " " << bot->GetOrientation();
        uint32 area = bot->GetAreaId();
        if (const AreaTableEntry *entry = sAreaStore.LookupEntry(area))
        {
            out << " |" << entry->area_name[0] << "|";
        }
        return out.str();
    }
    else if (command == "tpos")
    {
        Unit *target = *GetAiObjectContext()->GetValue<Unit *>("current target");
        if (!target)
        {
            return "";
        }

        ostringstream out;
        out << target->GetPositionX() << " " << target->GetPositionY() << " " << target->GetPositionZ() << " " << target->GetMapId() << " " << target->GetOrientation();
        return out.str();
    }
    else if (command == "movement")
    {
        LastMovement &data = *GetAiObjectContext()->GetValue<LastMovement &>("last movement");
        ostringstream out;
        out << data.lastMoveToX << " " << data.lastMoveToY << " " << data.lastMoveToZ << " " << bot->GetMapId() << " " << data.lastMoveToOri;
        return out.str();
    }
    else if (command == "target")
    {
        Unit *target = *GetAiObjectContext()->GetValue<Unit *>("current target");
        if (!target)
        {
            return "";
        }

        return target->GetName();
    }
    else if (command == "hp")
    {
        int pct = (int)((static_cast<float>(bot->GetHealth()) / bot->GetMaxHealth()) * 100);
        ostringstream out;
        out << pct << "%";

        Unit *target = *GetAiObjectContext()->GetValue<Unit *>("current target");
        if (!target)
        {
            return out.str();
        }

        pct = (int)((static_cast<float>(target->GetHealth()) / target->GetMaxHealth()) * 100);
        out << " / " << pct << "%";
        return out.str();
    }
    else if (command == "strategy")
    {
        return currentEngine->ListStrategies();
    }
    else if (command == "action")
    {
        return currentEngine->GetLastAction();
    }
    else if (command == "values")
    {
        return GetAiObjectContext()->FormatValues();
    }
    ostringstream out;
    out << "invalid command: " << command;
    return out.str();
}

/**
 * Checks if the bot has a specific skill.
 * @param skill The skill to check.
 * @return True if the bot has the skill, false otherwise.
 */
bool PlayerbotAI::HasSkill(SkillType skill)
{
    return bot->HasSkill(skill) && bot->GetBaseSkillValue(skill) > 1;
}

/**
 * Handles the player bot command.
 * @param args The command arguments.
 * @return True if the command was handled, false otherwise.
 */
bool ChatHandler::HandlePlayerbotCommand(char *args)
{
    return PlayerbotMgr::HandlePlayerbotMgrCommand(this, args);
}

/**
 * Handles the random player bot command.
 * @param args The command arguments.
 * @return True if the command was handled, false otherwise.
 */
bool ChatHandler::HandleRandomPlayerbotCommand(char *args)
{
    return RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(this, args);
}

/**
 * Handles the auction house bot command.
 * @param args The command arguments.
 * @return True if the command was handled, false otherwise.
 */
bool ChatHandler::HandleAhBotCommand(char *args)
{
    return ahbot::AhBot::HandleAhBotCommand(this, args);
}

/**
 * Handles the guild task command.
 * @param args The command arguments.
 * @return True if the command was handled, false otherwise.
 */
bool ChatHandler::HandleGuildTaskCommand(char *args)
{
    return GuildTaskMgr::HandleConsoleCommand(this, args);
}
