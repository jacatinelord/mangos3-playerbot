#pragma once

#include "../Action.h"
#include "player.h"

class Player;
class Group;

namespace ai
{
    class AcceptInvitationAction : public Action {
    public:
        AcceptInvitationAction(PlayerbotAI* ai) : Action(ai, "accept invitation") {}

        virtual bool Execute(Event event)
        {
            Player* master = GetMaster();

            Group* grp = bot->GetGroupInvite();
            if (!grp)
            {
                return false;
            }

            Player* inviter = sObjectMgr.GetPlayer(grp->GetLeaderGuid());
            if (!inviter)
            {
                return false;
            }

            if (!ai->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, inviter))
            {
                WorldPacket data(SMSG_GROUP_DECLINE, 10);
                data << bot->GetName();
                inviter->GetSession()->SendPacket(&data);
                bot->UninviteFromGroup();
                return false;
            }

            WorldPacket p;
            uint32 roles_mask = 0;
            p << roles_mask;
            bot->GetSession()->HandleGroupInviteResponseOpcode(p);

            if (sRandomPlayerbotMgr.IsRandomBot(bot))
            {
                bot->GetPlayerbotAI()->SetMaster(inviter);
            }

            ai->ResetStrategies();
            ai->TellMaster("Hello");
            return true;
        }
    };

}
