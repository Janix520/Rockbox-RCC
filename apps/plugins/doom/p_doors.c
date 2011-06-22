/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *   Door animation code (opening/closing)
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "p_spec.h"
#include "p_tick.h"
#include "s_sound.h"
#include "sounds.h"
#include "r_main.h"
#include "dstrings.h"
#include "d_deh.h"  // Ty 03/27/98 - externalized

#include "rockmacros.h"

///////////////////////////////////////////////////////////////
//
// Door action routines, called once per tick
//
///////////////////////////////////////////////////////////////

//
// T_VerticalDoor
//
// Passed a door structure containing all info about the door.
// See P_SPEC.H for fields.
// Returns nothing.
//
// jff 02/08/98 all cases with labels beginning with gen added to support
// generalized line type behaviors.

void T_VerticalDoor (vldoor_t* door)
{
   result_e  res;

   // Is the door waiting, going up, or going down?
   switch(door->direction)
   {
   case 0:
      // Door is waiting
      if (!--door->topcountdown)  // downcount and check
      {
         switch(door->type)
         {
         case blazeRaise:
         case genBlazeRaise:
            door->direction = -1; // time to go back down
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdcls);
            break;

         case normal:
         case genRaise:
            door->direction = -1; // time to go back down
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_dorcls);
            break;

         case close30ThenOpen:
         case genCdO:
            door->direction = 1;  // time to go back up
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
            break;

         case genBlazeCdO:
            door->direction = 1;  // time to go back up
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
            break;

         default:
            break;
         }
      }
      break;

   case 2:
      // Special case for sector type door that opens in 5 mins
      if (!--door->topcountdown)  // 5 minutes up?
      {
         switch(door->type)
         {
         case raiseIn5Mins:
            door->direction = 1;  // time to raise then
            door->type = normal;  // door acts just like normal 1 DR door now
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
            break;

         default:
            break;
         }
      }
      break;

   case -1:
      // Door is moving down
      res = T_MovePlane
            (
               door->sector,
               door->speed,
               door->sector->floorheight,
               false,
               1,
               door->direction
            );

      /* killough 10/98: implement gradual lighting effects */
      if (door->lighttag && door->topheight - door->sector->floorheight)
         EV_LightTurnOnPartway(door->line,
                               FixedDiv(door->sector->ceilingheight -
                                        door->sector->floorheight,
                                        door->topheight -
                                        door->sector->floorheight));

      // handle door reaching bottom
      if (res == pastdest)
      {
         switch(door->type)
         {
            // regular open and close doors are all done, remove them
         case blazeRaise:
         case blazeClose:
         case genBlazeRaise:
         case genBlazeClose:
            door->sector->ceilingdata = NULL;  //jff 2/22/98
            P_RemoveThinker (&door->thinker);  // unlink and free
            // killough 4/15/98: remove double-closing sound of blazing doors
            if (comp[comp_blazing])
               S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdcls);
            break;

         case normal:
         case p_close:
         case genRaise:
         case genClose:
            door->sector->ceilingdata = NULL; //jff 2/22/98
            P_RemoveThinker (&door->thinker);  // unlink and free
            break;

            // close then open doors start waiting
         case close30ThenOpen:
            door->direction = 0;
            door->topcountdown = TICRATE*30;
            break;

         case genCdO:
         case genBlazeCdO:
            door->direction = 0;
            door->topcountdown = door->topwait; // jff 5/8/98 insert delay
            break;

         default:
            break;
         }
      }
      /* jff 1/31/98 turn lighting off in tagged sectors of manual doors
       * killough 10/98: replaced with gradual lighting code
       */
      else if (res == crushed) // handle door meeting obstruction on way down
      {
         switch(door->type)
         {
         case genClose:
         case genBlazeClose:
         case blazeClose:
         case p_close:          // Close types do not bounce, merely wait
            break;

         case blazeRaise:
         case genBlazeRaise:
            door->direction = 1;
            if (!comp[comp_blazing]) {
               S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
               break;
            }

         default:             // other types bounce off the obstruction
            door->direction = 1;
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
            break;
         }
      }
      break;

   case 1:
      // Door is moving up
      res = T_MovePlane
            (
               door->sector,
               door->speed,
               door->topheight,
               false,
               1,
               door->direction
            );

      /* killough 10/98: implement gradual lighting effects */
      if (door->lighttag && door->topheight - door->sector->floorheight)
         EV_LightTurnOnPartway(door->line,
                               FixedDiv(door->sector->ceilingheight -
                                        door->sector->floorheight,
                                        door->topheight -
                                        door->sector->floorheight));

      // handle door reaching the top
      if (res == pastdest)
      {
         switch(door->type)
         {
         case blazeRaise:       // regular open/close doors start waiting
         case normal:
         case genRaise:
         case genBlazeRaise:
            door->direction = 0; // wait at top with delay
            door->topcountdown = door->topwait;
            break;

         case close30ThenOpen:  // close and close/open doors are done
         case blazeOpen:
         case p_open:
         case genBlazeOpen:
         case genOpen:
         case genCdO:
         case genBlazeCdO:
            door->sector->ceilingdata = NULL; //jff 2/22/98
            P_RemoveThinker (&door->thinker); // unlink and free
            break;

         default:
            break;
         }

         /* jff 1/31/98 turn lighting on in tagged sectors of manual doors
         * killough 10/98: replaced with gradual lighting code */
      }
      break;
   }
}

///////////////////////////////////////////////////////////////
//
// Door linedef handlers
//
///////////////////////////////////////////////////////////////

//
// EV_DoLockedDoor
//
// Handle opening a tagged locked door
//
// Passed the line activating the door, the type of door,
// and the thing that activated the line
// Returns true if a thinker created
//
int EV_DoLockedDoor
( line_t* line,
  vldoor_e  type,
  mobj_t* thing )
{
   player_t* p;

   // only players can open locked doors
   p = thing->player;
   if (!p)
      return 0;

   // check type of linedef, and if key is possessed to open it
   switch(line->special)
   {
   case 99:  // Blue Lock
   case 133:
      if (!p->cards[it_bluecard] && !p->cards[it_blueskull])
      {
         p->message = s_PD_BLUEO;             // Ty 03/27/98 - externalized
         S_StartSound(p->mo,sfx_oof);         // killough 3/20/98
         return 0;
      }
      break;

   case 134: // Red Lock
   case 135:
      if (!p->cards[it_redcard] && !p->cards[it_redskull])
      {
         p->message = s_PD_REDO;              // Ty 03/27/98 - externalized
         S_StartSound(p->mo,sfx_oof);         // killough 3/20/98
         return 0;
      }
      break;

   case 136: // Yellow Lock
   case 137:
      if (!p->cards[it_yellowcard] && !p->cards[it_yellowskull])
      {
         p->message = s_PD_YELLOWO;           // Ty 03/27/98 - externalized
         S_StartSound(p->mo,sfx_oof);         // killough 3/20/98
         return 0;
      }
      break;
   }

   // got the key, so open the door
   return EV_DoDoor(line,type);
}


//
// EV_DoDoor
//
// Handle opening a tagged door
//
// Passed the line activating the door and the type of door
// Returns true if a thinker created
//
int EV_DoDoor
( line_t* line,
  vldoor_e  type )
{
   int   secnum,rtn;
   sector_t* sec;
   vldoor_t* door;

   secnum = -1;
   rtn = 0;

   // open all doors with the same tag as the activating line
   while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      sec = &sectors[secnum];
      // if the ceiling already moving, don't start the door action
      if (P_SectorActive(ceiling_special,sec)) //jff 2/22/98
         continue;

      // new door thinker
      rtn = 1;
      door = Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);
      P_AddThinker (&door->thinker);
      sec->ceilingdata = door; //jff 2/22/98

      door->thinker.function = T_VerticalDoor;
      door->sector = sec;
      door->type = type;
      door->topwait = VDOORWAIT;
      door->speed = VDOORSPEED;
      door->line = line; // jff 1/31/98 remember line that triggered us
      door->lighttag = 0; /* killough 10/98: no light effects with tagged doors */

      // setup door parameters according to type of door
      switch(type)
      {
      case blazeClose:
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->direction = -1;
         door->speed = VDOORSPEED * 4;
         S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdcls);
         break;

      case p_close:
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->direction = -1;
         S_StartSound((mobj_t *)&door->sector->soundorg,sfx_dorcls);
         break;

      case close30ThenOpen:
         door->topheight = sec->ceilingheight;
         door->direction = -1;
         S_StartSound((mobj_t *)&door->sector->soundorg,sfx_dorcls);
         break;

      case blazeRaise:
      case blazeOpen:
         door->direction = 1;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         door->speed = VDOORSPEED * 4;
         if (door->topheight != sec->ceilingheight)
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_bdopn);
         break;

      case normal:
      case p_open:
         door->direction = 1;
         door->topheight = P_FindLowestCeilingSurrounding(sec);
         door->topheight -= 4*FRACUNIT;
         if (door->topheight != sec->ceilingheight)
            S_StartSound((mobj_t *)&door->sector->soundorg,sfx_doropn);
         break;

      default:
         break;
      }
   }
   return rtn;
}


//
// EV_VerticalDoor
//
// Handle opening a door manually, no tag value
//
// Passed the line activating the door and the thing activating it
// Returns true if a thinker created
//
// jff 2/12/98 added int return value, fixed all returns
//
int EV_VerticalDoor
( line_t* line,
  mobj_t* thing )
{
   player_t* player;
   sector_t* sec;
   vldoor_t* door;

   //  Check for locks
   player = thing->player;

   switch(line->special)
   {
   case 26: // Blue Lock
   case 32:
      if ( !player )
         return 0;
      if (!player->cards[it_bluecard] && !player->cards[it_blueskull])
      {
         player->message = s_PD_BLUEK;         // Ty 03/27/98 - externalized
         S_StartSound(player->mo,sfx_oof);     // killough 3/20/98
         return 0;
      }
      break;

   case 27: // Yellow Lock
   case 34:
      if ( !player )
         return 0;
      if (!player->cards[it_yellowcard] && !player->cards[it_yellowskull])
      {
         player->message = s_PD_YELLOWK;       // Ty 03/27/98 - externalized
         S_StartSound(player->mo,sfx_oof);     // killough 3/20/98
         return 0;
      }
      break;

   case 28: // Red Lock
   case 33:
      if ( !player )
         return 0;
      if (!player->cards[it_redcard] && !player->cards[it_redskull])
      {
         player->message = s_PD_REDK;          // Ty 03/27/98 - externalized
         S_StartSound(player->mo,sfx_oof);     // killough 3/20/98
         return 0;
      }
      break;

   default:
      break;
   }

   // if the wrong side of door is pushed, give oof sound
   if (line->sidenum[1]==-1)                     // killough
   {
      S_StartSound(player->mo,sfx_oof);           // killough 3/20/98
      return 0;
   }

   // get the sector on the second side of activating linedef
   sec = sides[line->sidenum[1]].sector;

   /* if door already has a thinker, use it
    * cph 2001/04/05 -
    * Ok, this is a disaster area. We're assuming that sec->ceilingdata
    *  is a vldoor_t! What if this door is controlled by both DR lines
    *  and by switches? I don't know how to fix that.
    * Secondly, original Doom didn't distinguish floor/lighting/ceiling
    *  actions, so we need to do the same in demo compatibility mode.
    */
   door = sec->ceilingdata;
   if (demo_compatibility) {
      if (!door) door = sec->floordata;
      if (!door) door = sec->lightingdata;
   }
   if (door)
   {
      switch(line->special)
      {
      case  1: // only for "raise" doors, not "open"s
      case  26:
      case  27:
      case  28:
      case  117:
         if (door->direction == -1)
            door->direction = 1;  // go back up
         else
         {
            if (!thing->player)
               return 0;           // JDC: bad guys never close doors

            door->direction = -1; // start going down immediately
         }
         return 1;
      }
   }

   // emit proper sound
   switch(line->special)
   {
   case 117: // blazing door raise
   case 118: // blazing door open
      S_StartSound((mobj_t *)&sec->soundorg,sfx_bdopn);
      break;

   case 1:   // normal door sound
   case 31:
      S_StartSound((mobj_t *)&sec->soundorg,sfx_doropn);
      break;

   default:  // locked door sound
      S_StartSound((mobj_t *)&sec->soundorg,sfx_doropn);
      break;
   }

   // new door thinker
   door = Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);
   P_AddThinker (&door->thinker);
   sec->ceilingdata = door; //jff 2/22/98
   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 1;
   door->speed = VDOORSPEED;
   door->topwait = VDOORWAIT;
   door->line = line; // jff 1/31/98 remember line that triggered us

   /* killough 10/98: use gradual lighting changes if nonzero tag given */
   door->lighttag = comp[comp_doorlight] ? 0 : line->tag;

   // set the type of door from the activating linedef type
   switch(line->special)
   {
   case 1:
   case 26:
   case 27:
   case 28:
      door->type = normal;
      break;

   case 31:
   case 32:
   case 33:
   case 34:
      door->type = p_open;
      line->special = 0;
      break;

   case 117: // blazing door raise
      door->type = blazeRaise;
      door->speed = VDOORSPEED*4;
      break;
   case 118: // blazing door open
      door->type = blazeOpen;
      line->special = 0;
      door->speed = VDOORSPEED*4;
      break;

   default:
      door->lighttag = 0;   // killough 10/98
      break;
   }

   // find the top and bottom of the movement range
   door->topheight = P_FindLowestCeilingSurrounding(sec);
   door->topheight -= 4*FRACUNIT;
   return 1;
}


///////////////////////////////////////////////////////////////
//
// Sector type door spawners
//
///////////////////////////////////////////////////////////////

//
// P_SpawnDoorCloseIn30()
//
// Spawn a door that closes after 30 seconds (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing
//
void P_SpawnDoorCloseIn30 (sector_t* sec)
{
   vldoor_t* door;

   door = Z_Malloc ( sizeof(*door), PU_LEVSPEC, 0);

   P_AddThinker (&door->thinker);

   sec->ceilingdata = door; //jff 2/22/98
   sec->special = 0;

   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 0;
   door->type = normal;
   door->speed = VDOORSPEED;
   door->topcountdown = 30 * 35;
   door->line = NULL; // jff 1/31/98 remember line that triggered us
   door->lighttag = 0; /* killough 10/98: no lighting changes */
}

//
// P_SpawnDoorRaiseIn5Mins()
//
// Spawn a door that opens after 5 minutes (called at level init)
//
// Passed the sector of the door, whose type specified the door action
// Returns nothing
//
void P_SpawnDoorRaiseIn5Mins
( sector_t* sec,
  int   secnum )
{
   (void)secnum;
   vldoor_t* door;

   door = Z_Malloc ( sizeof(*door), PU_LEVSPEC, 0);

   P_AddThinker (&door->thinker);

   sec->ceilingdata = door; //jff 2/22/98
   sec->special = 0;

   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 2;
   door->type = raiseIn5Mins;
   door->speed = VDOORSPEED;
   door->topheight = P_FindLowestCeilingSurrounding(sec);
   door->topheight -= 4*FRACUNIT;
   door->topwait = VDOORWAIT;
   door->topcountdown = 5 * 60 * 35;
   door->line = NULL; // jff 1/31/98 remember line that triggered us
   door->lighttag = 0; /* killough 10/98: no lighting changes */
}
