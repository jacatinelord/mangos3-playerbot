﻿/**
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

#ifndef DEF_AHNKAHET_H
#define DEF_AHNKAHET_H
/* Encounters
 * Elder Nadox         = 1
 * Prince Taldram      = 2
 * Jedoga Shadowseeker = 3
 * Herald Volazj       = 4
 * Amanitar            = 5
*/
enum
{
    MAX_ENCOUNTER               = 5,
    MAX_INITIATES               = 15,

    TYPE_NADOX                  = 0,
    TYPE_TALDARAM               = 1,
    TYPE_JEDOGA                 = 2,
    TYPE_VOLAZJ                 = 3,
    TYPE_AMANITAR               = 4,
    TYPE_DO_JEDOGA              = MAX_ENCOUNTER,
    TYPE_DO_NADOX               = MAX_ENCOUNTER + 1,
    TYPE_DO_TALDARAM            = MAX_ENCOUNTER + 2,

    DATA_INSANITY_PLAYER        = 1,

    GO_DOOR_TALDARAM            = 192236,
    GO_ANCIENT_DEVICE_L         = 193093,
    GO_ANCIENT_DEVICE_R         = 193094,
    GO_VORTEX                   = 193564,

    NPC_ELDER_NADOX             = 29309,
    NPC_TALDARAM                = 29308,
    NPC_JEDOGA_SHADOWSEEKER     = 29310,
    NPC_AHNKAHAR_GUARDIAN_EGG   = 30173,
    NPC_AHNKAHAR_SWARM_EGG      = 30172,
    NPC_JEDOGA_CONTROLLER       = 30181,
    NPC_TWILIGHT_INITIATE       = 30114,

    NPC_HERALD_VOLAZJ           = 29311,
    NPC_TWISTED_VISAGE_1        = 30621,
    NPC_TWISTED_VISAGE_2        = 30622,
    NPC_TWISTED_VISAGE_3        = 30623,
    NPC_TWISTED_VISAGE_4        = 30624,
    NPC_TWISTED_VISAGE_5        = 30625,

    SPELL_TWISTED_VISAGE_DEATH  = 57555,
    SPELL_INSANITY_SWITCH       = 57538,
    SPELL_INSANITY_CLEAR        = 57558,

    SPELL_INSANITY_PHASE_16     = 57508,
    SPELL_INSANITY_PHASE_32     = 57509,
    SPELL_INSANITY_PHASE_64     = 57510,
    SPELL_INSANITY_PHASE_128    = 57511,
    SPELL_INSANITY_PHASE_256    = 57512,

    ACHIEV_START_VOLAZJ_ID      = 20382,

    ACHIEV_CRIT_RESPECT_ELDERS  = 7317,             // Nadox, achiev 2038
    ACHIEV_CRIT_VOLUNTEER_WORK  = 7359,             // Jedoga, achiev 2056
};

static const float aTaldaramLandingLoc[4] = {528.734f, -845.998f, 11.54f, 0.68f};
static const float aJedogaLandingLoc[4] = {375.4977f, -707.3635f, -16.094f, 5.42f};

#endif
