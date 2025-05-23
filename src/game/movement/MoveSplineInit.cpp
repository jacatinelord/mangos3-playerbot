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

#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "packet_builder.h"
#include "Unit.h"
#include "TransportSystem.h"

namespace Movement
{
    /**
     * @brief Selects the appropriate speed type based on movement flags.
     * @param moveFlags The movement flags.
     * @return The selected UnitMoveType.
     */
    UnitMoveType SelectSpeedType(uint32 moveFlags)
    {
        if (moveFlags & MOVEFLAG_FLYING)
        {
            if (moveFlags & MOVEFLAG_BACKWARD /*&& speed_obj.flight >= speed_obj.flight_back*/)
            {
                return MOVE_FLIGHT_BACK;
            }
            else
            {
                return MOVE_FLIGHT;
            }
        }
        else if (moveFlags & MOVEFLAG_SWIMMING)
        {
            if (moveFlags & MOVEFLAG_BACKWARD /*&& speed_obj.swim >= speed_obj.swim_back*/)
            {
                return MOVE_SWIM_BACK;
            }
            else
            {
                return MOVE_SWIM;
            }
        }
        else if (moveFlags & MOVEFLAG_WALK_MODE)
        {
            // if ( speed_obj.run > speed_obj.walk )
            return MOVE_WALK;
        }
        else if (moveFlags & MOVEFLAG_BACKWARD /*&& speed_obj.run >= speed_obj.run_back*/)
        {
            return MOVE_RUN_BACK;
        }

        return MOVE_RUN;
    }

    /**
     * @brief Final pass of initialization that launches spline movement.
     * @return int32 duration - estimated travel time
     */
    int32 MoveSplineInit::Launch()
    {
        MoveSpline& move_spline = *unit.movespline;
        TransportInfo* transportInfo = unit.GetTransportInfo();

        Location real_position(unit.GetPositionX(), unit.GetPositionY(), unit.GetPositionZ(), unit.GetOrientation());

        // If boarded use current local position
        if (transportInfo)
        {
            transportInfo->GetLocalPosition(real_position.x, real_position.y, real_position.z, real_position.orientation);
        }

        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        if (!move_spline.Finalized() && !transportInfo)
        {
            real_position = move_spline.ComputePosition();
        }

        if (args.path.empty())
        {
            // should i do the things that user should do?
            MoveTo(real_position);
        }

        // correct first vertex
        args.path[0] = real_position;
        args.initialOrientation = real_position.orientation;

        uint32 moveFlags = unit.m_movementInfo.GetMovementFlags();
        if (args.flags.walkmode)
        {
            moveFlags |= MOVEFLAG_WALK_MODE;
        }
        else
        {
            moveFlags &= ~MOVEFLAG_WALK_MODE;
        }

        moveFlags |= MOVEFLAG_FORWARD;

        if (args.velocity == 0.f)
        {
            args.velocity = unit.GetSpeed(SelectSpeedType(moveFlags));
        }

        if (!args.Validate(&unit))
        {
            return 0;
        }

        unit.m_movementInfo.SetMovementFlags((MovementFlags)moveFlags);
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (transportInfo)
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << transportInfo->GetTransportGuid().WriteAsPacked();
            data << int8(transportInfo->GetTransportSeat());
        }

        PacketBuilder::WriteMonsterMove(move_spline, data);
        unit.SendMessageToSet(&data, true);

        return move_spline.Duration();
    }

    /**
     * @brief Stops any creature movement.
     */
    void MoveSplineInit::Stop()
    {
        MoveSpline& move_spline = *unit.movespline;

        // No need to stop if we are not moving
        if (move_spline.Finalized())
        {
            return;
        }

        TransportInfo* transportInfo = unit.GetTransportInfo();

        Location real_position(unit.GetPositionX(), unit.GetPositionY(), unit.GetPositionZ(), unit.GetOrientation());

        // If boarded use current local position
        if (transportInfo)
        {
            transportInfo->GetLocalPosition(real_position.x, real_position.y, real_position.z, real_position.orientation);
        }

        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        if (!move_spline.Finalized() && !transportInfo)
        {
            real_position = move_spline.ComputePosition();
        }

        if (args.path.empty())
        {
            // should i do the things that user should do?
            MoveTo(real_position);
        }

        // current first vertex
        args.path[0] = real_position;

        args.flags = MoveSplineFlag::Done;
        unit.m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_FORWARD | MOVEFLAG_CAN_FLY));
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (transportInfo)
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << transportInfo->GetTransportGuid().WriteAsPacked();
            data << int8(transportInfo->GetTransportSeat());
        }

        data << uint8(0);
        data << real_position.x << real_position.y << real_position.z;
        data << move_spline.GetId();
        data << uint8(MonsterMoveStop);
        unit.SendMessageToSet(&data, true);
    }

    /**
     * @brief Constructor that initializes the MoveSplineInit with a reference to a Unit.
     * @param m Reference to the Unit to be moved.
     */
    MoveSplineInit::MoveSplineInit(Unit& m) : unit(m)
    {
        // mix existing state into new
        args.flags.walkmode = unit.m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
        args.flags.flying = unit.m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_LEVITATING));
    }

    /**
     * @brief Sets unit's facing to a specified target after all path done.
     * @param target The target to face.
     */
    void MoveSplineInit::SetFacing(const Unit* target)
    {
        args.flags.EnableFacingTarget();
        args.facing.target = target->GetObjectGuid().GetRawValue();
    }

    /**
     * @brief Adds final facing animation.
     * Sets unit's facing to specified point/angle after all path done.
     * You can have only one final facing: previous will be overridden.
     * @param angle The angle to face.
     */
    void MoveSplineInit::SetFacing(float angle)
    {
        args.facing.angle = G3D::wrap(angle, 0.f, (float)G3D::twoPi());
        args.flags.EnableFacingAngle();
    }
}
