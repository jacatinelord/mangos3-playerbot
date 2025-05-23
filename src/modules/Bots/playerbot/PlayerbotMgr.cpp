#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotDbStore.h"
#include "PlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"

class LoginQueryHolder;
class CharacterHandler;

/**
 * @brief Constructor for PlayerbotHolder.
 */
PlayerbotHolder::PlayerbotHolder() : PlayerbotAIBase()
{
    for (uint32 spellId = 0; spellId < sSpellStore.GetNumRows(); spellId++)
    {
        sSpellStore.LookupEntry(spellId);
    }
}

/**
 * @brief Destructor for PlayerbotHolder.
 */
PlayerbotHolder::~PlayerbotHolder()
{
    LogoutAllBots();
}

/**
 * @brief Updates the AI internal state.
 * @param elapsed Time elapsed since the last update.
 */
void PlayerbotHolder::UpdateAIInternal(uint32 elapsed)
{
}

/**
 * @brief Updates the sessions for all player bots.
 * @param elapsed Time elapsed since the last update.
 */
void PlayerbotHolder::UpdateSessions(uint32 elapsed)
{
    for (PlayerBotMap::const_iterator itr = GetPlayerBotsBegin(); itr != GetPlayerBotsEnd(); ++itr)
    {
        Player* const bot = itr->second;
        if (bot->IsBeingTeleported())
        {
            bot->GetPlayerbotAI()->HandleTeleportAck();
        }
        else if (bot->IsInWorld())
        {
            bot->GetSession()->HandleBotPackets();
        }
    }
}

/**
 * @brief Logs out all player bots.
 */
void PlayerbotHolder::LogoutAllBots()
{
    while (true)
    {
        PlayerBotMap::const_iterator itr = GetPlayerBotsBegin();
        if (itr == GetPlayerBotsEnd())
        {
            break;
        }
        Player* bot = itr->second;
        LogoutPlayerBot(bot->GetObjectGuid().GetRawValue());
    }
}

/**
 * @brief Logs out a specific player bot.
 * @param guid The GUID of the player bot to log out.
 */
void PlayerbotHolder::LogoutPlayerBot(uint64 guid)
{
    Player* bot = GetPlayerBot(guid);
    if (bot)
    {
        bot->GetPlayerbotAI()->TellMaster("Goodbye!");
        sPlayerbotDbStore.Save(bot->GetPlayerbotAI());
        sLog.outString("Bot %s logged out", bot->GetName());

        WorldSession* botWorldSessionPtr = bot->GetSession();
        playerBots.erase(guid);    // deletes bot player ptr inside this WorldSession PlayerBotMap
        botWorldSessionPtr->LogoutPlayer(true); // this will delete the bot Player object and PlayerbotAI object
        delete botWorldSessionPtr;  // finally delete the bot's WorldSession
    }
}

/**
 * @brief Gets a player bot by its GUID.
 * @param playerGuid The GUID of the player bot.
 * @return Pointer to the player bot, or nullptr if not found.
 */
Player* PlayerbotHolder::GetPlayerBot(uint64 playerGuid) const
{
    PlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? nullptr : it->second;
}

/**
 * @brief Handles the login of a player bot.
 * @param bot Pointer to the player bot.
 */
void PlayerbotHolder::OnBotLogin(Player* const bot)
{
    PlayerbotAI* ai = new PlayerbotAI(bot);
    bot->SetPlayerbotAI(ai);
    OnBotLoginInternal(bot);

    playerBots[bot->GetObjectGuid().GetRawValue()] = bot;

    Player* master = ai->GetMaster();
    if (master)
    {
        ObjectGuid masterGuid = master->GetObjectGuid();
        if (master->GetGroup() &&
            !master->GetGroup()->IsLeader(masterGuid))
            master->GetGroup()->ChangeLeader(masterGuid);
    }

    Group* group = bot->GetGroup();
    if (group)
    {
        bool groupValid = false;
        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::MemberSlotList::const_iterator i = slots.begin(); i != slots.end(); ++i)
        {
            ObjectGuid member = i->guid;
            uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(member);
            if (!sPlayerbotAIConfig.IsInRandomAccountList(account))
            {
                groupValid = true;
                break;
            }
        }

        if (!groupValid)
        {
            WorldPacket p;
            string member = bot->GetName();
            p << uint32(PARTY_OP_LEAVE) << member << uint32(0);
            bot->GetSession()->HandleGroupDisbandOpcode(p);
        }
    }

    ai->ResetStrategies();
    ai->TellMaster("Hello!");

    uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(bot->GetObjectGuid());
    if (sPlayerbotAIConfig.IsInRandomAccountList(account))
    {
        sLog.outString("%d/%d Bot %s logged in", playerBots.size(), sRandomPlayerbotMgr.GetMaxAllowedBotCount(), bot->GetName());
        if (sConfig.GetBoolDefault("BeepAtStart", true) && playerBots.size() == sRandomPlayerbotMgr.GetMaxAllowedBotCount())
        {
            printf("\a");
        }
    }
    else sLog.outString("Bot %s logged in", bot->GetName());
}

/**
 * @brief Processes a bot command.
 * @param cmd The command to process.
 * @param guid The GUID of the bot.
 * @param admin Whether the command is from an admin.
 * @param masterAccountId The account ID of the master.
 * @param masterGuildId The guild ID of the master.
 * @return The result of the command.
 */
string PlayerbotHolder::ProcessBotCommand(string cmd, ObjectGuid guid, bool admin, uint32 masterAccountId, uint32 masterGuildId)
{
    if (!sPlayerbotAIConfig.enabled || guid.IsEmpty())
    {
        return "bot system is disabled";
    }

    uint32 botAccount = sObjectMgr.GetPlayerAccountIdByGUID(guid);
    bool isRandomBot = sRandomPlayerbotMgr.IsRandomBot(guid);
    bool isRandomAccount = sPlayerbotAIConfig.IsInRandomAccountList(botAccount);
    bool isMasterAccount = (masterAccountId == botAccount);

    if (isRandomAccount && !isRandomBot && !admin)
    {
        Player* bot = sObjectMgr.GetPlayer(guid);
        if (bot->GetGuildId() != masterGuildId)
        {
            return "not in your guild";
        }
    }

    if (!isRandomAccount && !isMasterAccount && !admin)
    {
        return "not in your account";
    }

    if (cmd == "add" || cmd == "login")
    {
        if (sObjectMgr.GetPlayer(guid))
        {
            return "player already logged in";
        }

        AddPlayerBot(guid.GetRawValue(), masterAccountId);
        return "ok";
    }
    else if (cmd == "remove" || cmd == "logout" || cmd == "rm")
    {
        if (!sObjectMgr.GetPlayer(guid))
        {
            return "player is offline";
        }

        if (!GetPlayerBot(guid.GetRawValue()))
        {
            return "not your bot";
        }

        LogoutPlayerBot(guid.GetRawValue());
        return "ok";
    }

    if (admin)
    {
        Player* bot = GetPlayerBot(guid.GetRawValue());
        if (!bot)
        {
            return "bot not found";
        }

        Player* master = bot->GetPlayerbotAI()->GetMaster();
        if (master)
        {
            if (cmd == "init=white" || cmd == "init=common")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_NORMAL);
                factory.CleanRandomize();
                return "ok";
            }
            else if (cmd == "init=green" || cmd == "init=uncommon")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_UNCOMMON);
                factory.CleanRandomize();
                return "ok";
            }
            else if (cmd == "init=blue" || cmd == "init=rare")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_RARE);
                factory.CleanRandomize();
                return "ok";
            }
            else if (cmd == "init=epic" || cmd == "init=purple")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_EPIC);
                factory.CleanRandomize();
                return "ok";
            }
        }

        if (cmd == "update")
        {
            PlayerbotFactory factory(bot, bot->getLevel());
            factory.Refresh();
            return "ok";
        }
        else if (cmd == "random")
        {
            sRandomPlayerbotMgr.Randomize(bot);
            return "ok";
        }
    }

    return "unknown command";
}

/**
 * @brief Handles the player bot manager command.
 * @param handler Pointer to the chat handler.
 * @param args The command arguments.
 * @return True if the command was handled successfully, false otherwise.
 */
bool PlayerbotMgr::HandlePlayerbotMgrCommand(ChatHandler* handler, char const* args)
{
    if (!sPlayerbotAIConfig.enabled)
    {
        handler->PSendSysMessage("|cffff0000Playerbot system is currently disabled!");
        return false;
    }

    WorldSession* m_session = handler->GetSession();

    if (!m_session)
    {
        handler->PSendSysMessage("You may only add bots from an active session");
        return false;
    }

    Player* player = m_session->GetPlayer();
    PlayerbotMgr* mgr = player->GetPlayerbotMgr();
    if (!mgr)
    {
        handler->PSendSysMessage("you cannot control bots yet");
        return false;
    }

    list<string> messages = mgr->HandlePlayerbotCommand(args, player);
    if (messages.empty())
    {
        return true;
    }

    for (list<string>::iterator i = messages.begin(); i != messages.end(); ++i)
    {
        handler->PSendSysMessage(i->c_str());
    }

    return true;
}

/**
 * @brief Handles the player bot command.
 * @param args The command arguments.
 * @param master Pointer to the master player.
 * @return A list of messages resulting from the command.
 */
list<string> PlayerbotHolder::HandlePlayerbotCommand(char const* args, Player* master)
{
    list<string> messages;

    if (!*args)
    {
        messages.push_back("usage: list or add/init/remove PLAYERNAME");
        return messages;
    }

    char* cmd = strtok((char*)args, " ");
    char* charname = strtok(NULL, " ");
    if (!cmd)
    {
        messages.push_back("usage: list or add/init/remove PLAYERNAME");
        return messages;
    }

    if (!strcmp(cmd, "list"))
    {
        messages.push_back(ListBots(master));
        return messages;
    }

    if (!charname)
    {
        messages.push_back("usage: list or add/init/remove PLAYERNAME");
        return messages;
    }

    std::string cmdStr = cmd;
    std::string charnameStr = charname;

    set<string> bots;
    if (charnameStr == "*" && master)
    {
        Group* group = master->GetGroup();
        if (!group)
        {
            messages.push_back("you must be in group");
            return messages;
        }

        Group::MemberSlotList slots = group->GetMemberSlots();
        for (Group::member_citerator i = slots.begin(); i != slots.end(); i++)
        {
            ObjectGuid member = i->guid;

            if (member.GetRawValue() == master->GetObjectGuid().GetRawValue())
            {
                continue;
            }

            string bot;
            if (sObjectMgr.GetPlayerNameByGUID(member, bot))
            {
                bots.insert(bot);
            }
        }
    }

    if (charnameStr == "!" && master && master->GetSession()->GetSecurity() > SEC_GAMEMASTER)
    {
        for (PlayerBotMap::const_iterator i = GetPlayerBotsBegin(); i != GetPlayerBotsEnd(); ++i)
        {
            Player* bot = i->second;
            if (bot && bot->IsInWorld())
            {
                bots.insert(bot->GetName());
            }
        }
    }

    vector<string> chars = split(charnameStr, ',');
    for (vector<string>::iterator i = chars.begin(); i != chars.end(); i++)
    {
        string s = *i;

        uint32 accountId = GetAccountId(s);
        if (!accountId)
        {
            bots.insert(s);
            continue;
        }

        QueryResult* results = CharacterDatabase.PQuery(
            "SELECT `name` FROM `characters` WHERE `account` = '%u'",
            accountId);
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                string charName = fields[0].GetString();
                bots.insert(charName);
            } while (results->NextRow());

            delete results;
        }
    }

    for (set<string>::iterator i = bots.begin(); i != bots.end(); ++i)
    {
        string bot = *i;
        ostringstream out;
        out << cmdStr << ": " << bot << " - ";

        ObjectGuid member = sObjectMgr.GetPlayerGuidByName(bot);
        if (!member)
        {
            out << "character not found";
        }
        else if (master && member.GetRawValue() != master->GetObjectGuid().GetRawValue())
        {
            out << ProcessBotCommand(cmdStr, member,
                master->GetSession()->GetSecurity() >= SEC_GAMEMASTER,
                master->GetSession()->GetAccountId(),
                master->GetGuildId());
        }
        else if (!master)
        {
            out << ProcessBotCommand(cmdStr, member, true, -1, -1);
        }

        messages.push_back(out.str());
    }

    return messages;
}

/**
 * @brief Gets the account ID for a given name.
 * @param name The name of the account.
 * @return The account ID.
 */
uint32 PlayerbotHolder::GetAccountId(string name)
{
    uint32 accountId = 0;

    QueryResult* results = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", name.c_str());
    if (results)
    {
        Field* fields = results->Fetch();
        accountId = fields[0].GetUInt32();
        delete results;
    }

    return accountId;
}

/**
 * @brief Lists the bots for a given master.
 * @param master Pointer to the master player.
 * @return A string containing the list of bots.
 */
string PlayerbotHolder::ListBots(Player* master)
{
    set<string> bots;
    map<uint8, string> classNames;
    classNames[CLASS_DRUID] = "Druid";
    classNames[CLASS_HUNTER] = "Hunter";
    classNames[CLASS_MAGE] = "Mage";
    classNames[CLASS_PALADIN] = "Paladin";
    classNames[CLASS_PRIEST] = "Priest";
    classNames[CLASS_ROGUE] = "Rogue";
    classNames[CLASS_SHAMAN] = "Shaman";
    classNames[CLASS_WARLOCK] = "Warlock";
    classNames[CLASS_WARRIOR] = "Warrior";

    map<string, string> online;
    list<string> names;
    map<string, string> classes;

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        string name = bot->GetName();
        bots.insert(name);

        names.push_back(name);
        online[name] = "+";
        classes[name] = classNames[bot->getClass()];
    }

    if (master)
    {
        QueryResult* results = CharacterDatabase.PQuery("SELECT `class`,`name` FROM `characters` where `account` = '%u'",
            master->GetSession()->GetAccountId());
        if (results != NULL)
        {
            do
            {
                Field* fields = results->Fetch();
                uint8 cls = fields[0].GetUInt8();
                string name = fields[1].GetString();
                if (bots.find(name) == bots.end() && name != master->GetSession()->GetPlayerName())
                {
                    names.push_back(name);
                    online[name] = "-";
                    classes[name] = classNames[cls];
                }
            } while (results->NextRow());
            delete results;
        }
    }

    names.sort();

    ostringstream out;
    bool first = true;
    out << "Bot roster: ";
    for (list<string>::iterator i = names.begin(); i != names.end(); ++i)
    {
        if (first)
        {
            first = false;
        }
        else
        {
            out << ", ";
        }
        string name = *i;
        out << online[name] << name << " " << classes[name];
    }

    return out.str();
}

/**
 * @brief Constructor for PlayerbotMgr.
 * @param master Pointer to the master player.
 */
PlayerbotMgr::PlayerbotMgr(Player* const master) : PlayerbotHolder(), master(master)
{
}

/**
 * @brief Destructor for PlayerbotMgr.
 */
PlayerbotMgr::~PlayerbotMgr()
{
}

/**
 * @brief Updates the AI internal state.
 * @param elapsed Time elapsed since the last update.
 */
void PlayerbotMgr::UpdateAIInternal(uint32 elapsed)
{
    SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
}

/**
 * @brief Handles a command from the master.
 * @param type The type of the command.
 * @param text The text of the command.
 */
void PlayerbotMgr::HandleCommand(uint32 type, const string& text)
{
    Player* master = GetMaster();
    if (!master)
    {
        return;
    }

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleCommand(type, text, *master);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == master)
        {
            bot->GetPlayerbotAI()->HandleCommand(type, text, *master);
        }
    }
}

/**
 * @brief Handles an incoming packet from the master.
 * @param packet The incoming packet.
 */
void PlayerbotMgr::HandleMasterIncomingPacket(const WorldPacket& packet)
{
    // Iterate through all player bots and handle the incoming packet
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleMasterIncomingPacket(packet);
    }

    // Iterate through all random player bots and handle the incoming packet if the master matches
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
        {
            bot->GetPlayerbotAI()->HandleMasterIncomingPacket(packet);
        }
    }

    // Handle specific packet opcodes
    switch (packet.GetOpcode())
    {
        // If the master is logging out, log out all bots
        case CMSG_LOGOUT_REQUEST:
        {
            LogoutAllBots();
            return;
        }
    }
}

/**
 * @brief Handles an outgoing packet to the master.
 * @param packet The outgoing packet.
 */
void PlayerbotMgr::HandleMasterOutgoingPacket(const WorldPacket& packet)
{
    // Iterate through all player bots and handle the outgoing packet
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleMasterOutgoingPacket(packet);
    }

    // Iterate through all random player bots and handle the outgoing packet if the master matches
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
        {
            bot->GetPlayerbotAI()->HandleMasterOutgoingPacket(packet);
        }
    }
}

/**
 * @brief Saves all player bots to the database.
 */
void PlayerbotMgr::SaveToDB()
{
    // Iterate through all player bots and save them to the database
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->SaveToDB();
    }

    // Iterate through all random player bots and save them to the database if the master matches
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
        {
            bot->SaveToDB();
        }
    }
}

/**
 * @brief Internal handler for bot login.
 * @param bot Pointer to the player bot.
 */
void PlayerbotMgr::OnBotLoginInternal(Player * const bot)
{
    // Set the master for the bot and reset its strategies
    bot->GetPlayerbotAI()->SetMaster(master);
    bot->GetPlayerbotAI()->ResetStrategies();
}

/**
 * @brief Handles player login and auto-logins bots if configured.
 * @param player Pointer to the player.
 */
void PlayerbotMgr::OnPlayerLogin(Player* player)
{
    // Check if bot auto-login is enabled
    if (!sPlayerbotAIConfig.botAutologin)
    {
        return;
    }

    // Get the account ID of the player
    uint32 accountId = player->GetSession()->GetAccountId();
    QueryResult* results = CharacterDatabase.PQuery(
        "SELECT `name` FROM `characters` WHERE `account` = '%u'",
        accountId);
    if (results)
    {
        // Construct the add command for all characters in the account
        ostringstream out; out << "add ";
        bool first = true;
        do
        {
            Field* fields = results->Fetch();
            if (first) first = false; else out << ",";
            {
                out << fields[0].GetString();
            }
        } while (results->NextRow());

        delete results;

        // Handle the add command for the player
        HandlePlayerbotCommand(out.str().c_str(), player);
    }
}
