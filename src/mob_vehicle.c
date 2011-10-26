/* GroundZero2 is based upon code from GroundZero by Corey Hilke, et al.
 * GroundZero was based upon code from ROM by Russ Taylor, et al.
 * ROM was based upon code from Merc by Hatchett, Furey, and Kahn.
 * Merc was based upon code from DIKUMUD.
 *
 * DikuMUD was written by Katja Nyboe, Tom Madsen, Hans Henrik Staerfeldt,
 * Michael Seifert, and Sebastian Hammer.
 *
 */

/**
 * NAME:        mob_vehicle.c
 * SYNOPSIS:    GZ2 (new-style) vehicles
 * AUTHOR:      satori
 * CREATED:     07 Apr 2001
 * MODIFIED:    07 Apr 2001
 */

#include "ground0.h"


struct
{
    char *name;
    int pos;
}
man_positions[] =
{
    { "shield", MAN_SHIELD },
    { "turret", MAN_TURRET },
    { "drive", MAN_DRIVE },
    { "gun", MAN_GUN },
    { "drive", MAN_HUD },
    { NULL, 0 }
};


/*
 * No-one really liked GZ1 (old-style) object vehicles when I put them back in
 * even though people begged for them.  So here's a return to GZ2 (new-style)
 * mobile vehicles, with vast improvements to the code.
 *
 * A quick guide -- the general functions:
 *   - convert_vehicle() converts a vehicle object to a mobile.
 *   - control_vehicle() controls a mobile, depending upon what position
 *     you're manning.
 *   - man_position() mans a position within the vehicle.
 *   - verify_vehicle() verifies that all mandatory positions are manned.
 *   - revert_vehicle() reverts a vehicle mobile to an object.
 *
 */

/*
 * convert_vehicle() --
 *
 * Turn a vehicle object into a mobile.  The mobile has a few interesting
 * attributes: the old vehicle object is in its interior (that is, ironically,
 * the object is now in the room that used to be inside it); the mobile has
 * the hp and abs of the object; the mobile's abs are damageable according to
 * the ACT_DAMAGEABLE_ABS flag; the mobile's team is that of the driver, or if
 * there's the no driver, the last person that entered the vehicle.
 */

struct char_data *
convert_vehicle (struct obj_data *vehicle)
{
    struct char_data *mob = NULL;
    struct char_data *ch;

    logmesg("converting vehicle to mobile");

    for ( ch = store_area->people; ch; ch = ch->next_in_room )
        if ( is_name(OBJ_TOMOB(vehicle), ch->names) )
            break;

    if ( ch == NULL || (mob = clone_mobile(ch, FALSE)) == NULL )
    {
        logmesg
            ("failed conversion: OBJ_TOMOB(vehicle)=\"%s\" (%p), ch=%p, mob=%p",
             OBJ_TOMOB(vehicle), ch, mob);
        return (NULL);
    }

    mob->interior = vehicle->interior;
    vehicle->interior = NULL;
    mob->interior->interior_of = NULL;
    mob->interior->inside_mob = mob;

    char_from_room(mob);
    char_to_room(mob, vehicle->in_room);

    obj_from_room(vehicle);
    obj_to_char(vehicle, mob);

    mob->hit = vehicle->hp_char;
    mob->abs = vehicle->hp_struct;
    ch = mob->interior->people;

    if ( IS_SET(OBJ_POSITIONS(vehicle), MAN_DRIVE | MAN_HUD) )
        for ( ; ch; ch = ch->next_in_room )
            if ( IS_SET(ch->temp_flags, MAN_DRIVE | MAN_HUD) )
                break;

    mob->turret_dir = mob->shield_dir = mob->gun_dir = DIR_NORTH;
    mob->team = (ch ? ch->team : TEAM_MOB);
    SET_BIT(mob->temp_flags, IS_VEHICLE | OBJ_POSITIONS(vehicle));
    return (mob);
}


struct obj_data *
revert_vehicle (struct char_data *mob)
{
    struct obj_data *obj;

    if ( !(obj = find_eq_char(mob, SEARCH_ITEM_TYPE, ITEM_TEAM_VEHICLE)) )
    {
        logmesg("revert_vehicle: Lost vehicle object in mob.");
        return (NULL);
    }

    logmesg("reverting vehicle to object");
    obj->interior = mob->interior;
    mob->interior = NULL;
    obj->interior->interior_of = obj;
    obj->interior->inside_mob = NULL;

    obj_from_char(obj);
    obj_to_room(obj, mob->in_room);

    obj->hp_char = mob->hit;
    obj->hp_struct = mob->abs;

    REMOVE_BIT(mob->temp_flags, IS_VEHICLE);
    extract_char(mob, FALSE);
    mob->next_extract = extract_list;
    extract_list = mob;

    return (obj);
}


int
control_vehicle (struct char_data *ch, struct char_data *mob, int dir)
{
    if ( IS_SET(ch->temp_flags, MAN_SHIELD) )
    {
        if ( dir < DIR_NORTH || dir > DIR_WEST )
        {
            send_to_char("You can't turn the shields that direction.\r\n",
                         ch);
            return (0);
        }
        else if ( mob->shield_dir == dir )
        {
            send_to_char("The shields are already facing that way.\r\n",
                         ch);
            return (0);
        }

        act("You turn the shields to the $t.", ch, dir_name[dir], 0,
            TO_CHAR);
        act("$n turns the shields to the $t.", ch, dir_name[dir], 0,
            TO_ROOM);
        mob->shield_dir = dir;
        return (1);
    }

    if ( IS_SET(ch->temp_flags, MAN_TURRET) )
    {
        if ( dir < DIR_NORTH || dir > DIR_WEST )
        {
            send_to_char("You can't turn the turret that direction.\r\n",
                         ch);
            return (0);
        }
        else if ( mob->turret_dir == dir )
        {
            send_to_char("The turret's already facing that way.\r\n", ch);
            return (0);
        }

        act("You turn the turret to point to the $t.", ch, dir_name[dir],
            0, TO_CHAR);
        act("$n turns the turret to point to the $t.", ch, dir_name[dir],
            0, TO_ROOM);
        mob->turret_dir = dir;
        return (1);
    }

    if ( IS_SET(ch->temp_flags, MAN_GUN) )
    {
        if ( dir == DIR_UP )
        {
            send_to_char("You can't turn the gun in that direction.\r\n",
                         ch);
            return (0);
        }
        else if ( mob->gun_dir == dir )
        {
            send_to_char
                ("The gun is already pointing in that direction.\r\n", ch);
            return (0);
        }
        else if ( dir == DIR_DOWN )
        {
            act("You angle the gun down to hit the current room.", ch, 0,
                0, TO_CHAR);
            act("$n angles the gun down to hit the current room.", ch, 0,
                0, TO_ROOM);
            mob->gun_dir = DIR_DOWN;
            return (1);
        }

        act("You turn the gun to point to the $t.", ch, dir_name[dir], 0,
            TO_CHAR);
        act("$n turns the gun to point to the $t.", ch, dir_name[dir], 0,
            TO_ROOM);
        mob->gun_dir = dir;
        return (1);
    }

    if ( IS_SET(ch->temp_flags, MAN_DRIVE) )
    {
        if ( dir < DIR_NORTH || dir > DIR_WEST )
        {
            send_to_char("You can't drive the tank in that direction.\r\n",
                         ch);
            return (0);
        }
        else if ( g_forceAction )
        {
            if ( move_char(mob, dir, 0, MWALL_DESTROY) == FALSE )
            {
                send_to_char("You couldn't go that way.\r\n", ch);
                return (0);
            }
        }
        else if ( move_char(mob, dir, 0, MWALL_NOMINAL) == FALSE )
        {
            printf_to_char(ch,
                           "You couldn't go that way.  Use &W%c!&n to try driving through a wall.",
                           UPPER(*dir_name[dir]));
            return (0);
        }

        return (1);
    }

    return (0);
}


const char *
man_name (int pos)
{
    switch (pos)
    {
        case MAN_SHIELD:
            return "shield controls";
        case MAN_TURRET:
            return "turret controls";
        case MAN_DRIVE:
            return "drive controls";
        case MAN_GUN:
            return "gun";
        case MAN_HUD:
            return "controls";
        default:
            logmesg("man_name(%d): unrecognized station for manning",
                       pos);
            return "<BUG-BUG-BUG>";
    }
}


struct char_data *
find_manning (struct room_data *rm, int pos)
{
    struct char_data *ch;

    for ( ch = rm->people; ch; ch = ch->next_in_room )
        if ( IS_SET(ch->temp_flags, pos) )
            break;

    return (ch);
}


int
man_position (struct char_data *ch, int pos)
{
    struct obj_data *veh;
    struct char_data *it;
    int i;

    if ( !IS_SET(ch->temp_flags, IN_VEHICLE) ||
         !(veh = ch->in_room->interior_of) )
    {
        if ( ch->in_room->inside_mob )
            send_to_char("You can't switch stations while the vehicle's on!\r\n", ch);
        else
            send_to_char("You're not in a vehicle.\r\n", ch);

        return (0);
    }
    else if ( !IS_SET(OBJ_POSITIONS(veh), pos) )
    {
        send_to_char("I don't see any station like that, here.\r\n", ch);
        return (0);
    }
    else if ( (it = find_manning(ch->in_room, pos)) && it != ch )
    {
        act("$N is already manning that station.", ch, 0, it, TO_CHAR);
        return (0);
    }
    else if ( it == ch )
    {
        send_to_char("You're already manning that station.\r\n", ch);
        return (0);
    }

    for ( i = 0; man_positions[i].name; i++ )
    {
        if ( IS_SET(ch->temp_flags, man_positions[i].pos) )
        {
            act("You stop manning the $t controls.", ch,
                man_positions[i].name, NULL, TO_CHAR);
            act("$n stops manning the $t controls.", ch,
                man_positions[i].name, NULL, TO_ROOM);
            REMOVE_BIT(ch->temp_flags, man_positions[i].pos);
        }
    }

    SET_BIT(ch->temp_flags, pos);
    act("You man the $t.", ch, man_name(pos), 0, TO_CHAR);
    act("$n mans the $t.", ch, man_name(pos), 0, TO_ROOM);
    return (1);
}


void
stop_manning (struct char_data *ch)
{
    struct obj_data *veh;

    if ( ch->in_room && ch->in_room->inside_mob )
    {
        veh = find_eq_char(ch->in_room->inside_mob, SEARCH_ITEM_TYPE,
                           ITEM_TEAM_VEHICLE);

        if ( !veh )
        {
            logmesg("stop_manning: Lost original object in mobile vehicle.");
            return;
        }

        if ( IS_SET(OBJ_R_POSITIONS(veh), ch->temp_flags) )
            veh = revert_vehicle(ch->in_room->inside_mob);
    }

    REMOVE_BIT(ch->temp_flags,
               MAN_DRIVE | MAN_SHIELD | MAN_TURRET | MAN_GUN | MAN_HUD);
}


struct obj_data *
get_vehicle ( struct room_data *rm )
{
    if ( rm->inside_mob == NULL )
        return rm->interior_of;

    return find_eq_char(rm->inside_mob, SEARCH_ITEM_TYPE,
                        ITEM_TEAM_VEHICLE);
}


int
verify_vehicle (struct room_data *rm)
{
    struct obj_data *veh;
    int i;
 
    if ( (veh = get_vehicle(rm)) == NULL )
    {
        logmesg("verify_vehicle: Lost vehicle object in mob.");
        return (-1);
    }
    else if ( !(veh = rm->interior_of) )
    {
        logmesg("verify_vehicle: Room not interior of vehicle mob or object.");
        return (-1);
    }
 
    for ( i = 0; man_positions[i].name; i++ )
        if ( IS_SET(man_positions[i].pos, OBJ_R_POSITIONS(veh) ) &&
             !find_manning(rm, man_positions[i].pos))
            break;
    
    return (i);
}


bool
man_heading (struct char_data *veh, int dir, int flags)
{
    int myDir = DIR_NORTH;

    if ( veh->interior == NULL || !IS_SET(veh->temp_flags, IS_VEHICLE) )
        return FALSE;
    else if ( find_manning(veh->interior, flags) == NULL )
        return FALSE;

    if ( IS_SET(flags, MAN_TURRET) )
        myDir = veh->turret_dir;
    else if ( IS_SET(flags, MAN_GUN) )
        myDir = veh->gun_dir;
    else if ( IS_SET(flags, MAN_SHIELD) )
        myDir = veh->shield_dir;

    return (myDir == dir);
}


void
vehicle_fire (struct char_data *ch, char *argument)
{
    struct char_data *tank = ch->in_room->inside_mob;
    struct obj_data *veh = get_vehicle(ch->in_room);

    if ( tank == NULL || veh == NULL )
    {
        send_to_char("You don't appear to really be inside a vehicle...\r\n", ch);
        return;
    }

    struct char_data *gunner = find_manning(ch->in_room, MAN_TURRET | MAN_HUD);

    if ( gunner && gunner != ch )
    {
        send_to_char("You have someone manning the turret already!\r\n", ch);
        return;
    }

    char arg[MAX_INPUT_LENGTH];
    int dist = (OBJ_TURRET_HIGH(veh) + OBJ_TURRET_LOW(veh)) / 2;

    one_argument(argument, arg);

    if ( *arg )
    {
        if ( !is_number(arg) )
        {
            send_to_char("&BUSAGE&X: &Wfire &X[&ndistance&X]&n\r\n", ch);
            return;
        }

        dist = atoi(arg);

        if ( dist < OBJ_TURRET_LOW(veh) || dist > OBJ_TURRET_HIGH(veh) )
        {
            printf_to_char(ch, "The turret only has an effective range between %d and %d squares.\r\n",
                           OBJ_TURRET_LOW(veh), OBJ_TURRET_HIGH(veh));
            return;
        }
    }

    struct room_data *target = tank->in_room;
    int d = tank->turret_dir;

    while ( dist-- && !target->exit[d] )
        target = index_room(target, dir_cmod[d][0], dir_cmod[d][1]);

    struct obj_data *wpn = get_eq_char(tank, WEAR_WIELD);
    struct obj_data *ammo = (wpn ? find_ammo(wpn) : NULL);
    struct obj_data *clone;

    if ( wpn == NULL )
    {
        send_to_char("Your vehicle doesn't have a turret to fire!\r\n", ch);
        return;
    }

    if ( IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED) )
    {
        clone = create_object(wpn->pIndexData, 0);
        clone_object(wpn, clone);
    }
    else
    {
        if ( ammo == NULL || ammo->ammo <= 0 )
        {
            send_to_char("You've run out of ammunition.\r\n", ch);
            return;
        }

        clone = create_object(ammo->pIndexData, 0);
        clone_object(ammo, clone);
    }

    act("You are momentarily deafened as the turret &Rfires&n $t.",
        tank->interior->people, dir_name[d], NULL, TO_CHAR);
    act("You are momentarily deafened as the turret &Rfires&n $t.",
        tank->interior->people, dir_name[d], NULL, TO_ROOM);

    if ( target->people )
    {
        char buf[256];
        sprintf(buf, "$p fires $P into the room from just %s of here!", dir_name[rev_dir[d]]);
        act(buf, target->people, veh, clone, TO_CHAR);
        act(buf, target->people, veh, clone, TO_ROOM);
    }

    if ( !gunner )
        WAIT_STATE(ch, 8);
    else
        WAIT_STATE(ch, 5);

    obj_to_room(clone, target);
    claimObject(tank, clone);
    bang_obj(clone, 1);
}


/*****************************************/
/*  C O M M A N D   P R O C E D U R E S  */
/*****************************************/

void
do_man (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int i;

    one_argument(argument, arg);

    if ( !*arg )
    {
        struct char_data *mob = ch->in_room->inside_mob;
        struct obj_data *veh;

        if ( IS_SET(ch->temp_flags, IN_VEHICLE) )
        {
            for ( i = 0; man_positions[i].name; i++ )
                if ( IS_SET(ch->temp_flags, man_positions[i].pos) )
                    break;

            if ( man_positions[i].name )
            {
                act("You stop manning the $t controls.", ch,
                    man_positions[i].name, NULL, TO_CHAR);
                act("$n stops manning the $t controls.", ch,
                    man_positions[i].name, NULL, TO_ROOM);

                if ( mob && IS_SET(mob->temp_flags, IS_VEHICLE) )
                {
                    if ((veh =
                         find_eq_char(mob, SEARCH_ITEM_TYPE,
                                      ITEM_TEAM_VEHICLE)) == NULL)
                    {
                        send_to_char("Uhm, shit, you're stuck because:\r\n"
                                     "Game lost the original vehicle.  Report this to the imms!\r\n",
                                     ch);
                        logmesg("unman: lost original vehicle in mob.");
                        return;
                    }

                    if ( IS_SET(OBJ_R_POSITIONS(veh), man_positions[i].pos) )
                        revert_vehicle(mob);
                }

                REMOVE_BIT(ch->temp_flags, man_positions[i].pos);
                return;
            }

            send_to_char("&cUSAGE: &Wman&X <&nposition&X>&n\r\n", ch);

            if ( mob )
                veh = find_eq_char(mob, SEARCH_ITEM_TYPE, ITEM_TEAM_VEHICLE);
            else
                veh = ch->in_room->interior_of;

            if ( veh )
                for ( i = 0; man_positions[i].name; i++ )
                {
                    if ( IS_SET(OBJ_POSITIONS(veh), man_positions[i].pos) )
                    {
                        strcat(arg, man_positions[i].name);
                        if (IS_SET
                            (OBJ_R_POSITIONS(veh), man_positions[i].pos))
                            strcat(arg, "* ");
                        strcat(arg, " ");
                    }
                }
            else
            {
                send_to_char("You're not in a vehicle or your vehicle is broken.\r\n", ch);
                logmesg("%s had IN_VEHICLE while not actually in vehicle.", ch->names);
                return;
            }

            printf_to_char(ch,
                           "The following stations are available, here: %s\r\n",
                           *arg ? arg : "&WNone!&n");
            send_to_char
                ("Stations marked with an asterisk are required for the "
                 "vehicle to be\r\noperational.\r\n", ch);
            return;
        }

        send_to_char("&cUSAGE: &Wman&X <&nposition&X>&n\r\n", ch);
        return;
    }

    for ( i = 0; man_positions[i].name; i++ )
        if ( !str_prefix(arg, man_positions[i].name) )
            break;

    if ( !man_positions[i].name )
    {
        send_to_char("That's not a valid position.\r\n", ch);
        return;
    }

    man_position(ch, man_positions[i].pos);
}


void
do_start (struct char_data *ch, char *argument)
{
    struct char_data *mob;
    struct obj_data *veh;
    int i;

    if ( !IS_SET(ch->temp_flags, IN_VEHICLE ) ||
         !(veh = ch->in_room->interior_of))
    {
        send_to_char("You're not in a vehicle.\r\n", ch);
        return;
    }

    i = verify_vehicle(ch->in_room);

    if ( i == -1 )
    {
        send_to_char("The vehicle's broken.  Hrm.\r\n", ch);
        return;
    }
    else if ( man_positions[i].name )
    {
        act("You need to have someone manning the $t controls before you can start the vehicle.", ch, man_positions[i].name, NULL, TO_CHAR);
        return;
    }

    /*
     * The vehicle is good to go, convert to a mobile and prepare to rock and roll.
     * Whee!
     */
    if ( !(mob = convert_vehicle(veh)) )
    {
        send_to_char("Ack!  Couldn't convert the vehicle because:\r\n"
                     "Something gone and fucked up -- report this to the imms!\r\n",
                     ch);
        return;
    }

    if ( OBJ_START_MESSAGE_DP(veh) )
    {
        act(OBJ_START_MESSAGE_DP(veh), ch, veh, NULL, TO_CHAR);
        act(OBJ_START_MESSAGE_DO(veh), ch, veh, NULL, TO_ROOM);
        act(OBJ_START_MESSAGE_IO(veh), mob, veh, NULL, TO_ROOM);
    }
    else
    {
        act("You fire $p up!  It's time to ride!", ch, veh, NULL, TO_CHAR);
        act("$n fires $p up!  It's time to ride!", ch, veh, NULL, TO_ROOM);
        act("Hrm... $p just fired up.  Looks like someone's in for some trouble!", mob, veh, NULL, TO_ROOM);
    }
}


void
do_stop (struct char_data *ch, char *argument)
{
    struct char_data *mob;
    struct obj_data *veh;

    if ( !(mob = ch->in_room->inside_mob) ||
         !IS_SET(mob->temp_flags, IS_VEHICLE) ||
         !IS_SET(ch->temp_flags, IN_VEHICLE) )
    {
        send_to_char("You're not in an active vehicle.\r\n", ch);
        return;
    }

    veh = find_eq_char(ch->in_room->inside_mob, SEARCH_ITEM_TYPE,
                       ITEM_TEAM_VEHICLE);

    if ( !veh )
    {
        send_to_char("Ack.  Something broke.\r\n", ch);
        logmesg("stop_vehicle: Lost original object in mobile.");
        return;
    }

    if ( OBJ_STOP_MESSAGE_DP(veh) )
    {
        act(OBJ_STOP_MESSAGE_DP(veh), ch, veh, NULL, TO_CHAR);
        act(OBJ_STOP_MESSAGE_DO(veh), ch, veh, NULL, TO_ROOM);
        act(OBJ_STOP_MESSAGE_IO(veh), mob, veh, NULL, TO_ROOM);
    }
    else
    {
        act("You stop $p and kill the engine.", ch, veh, NULL, TO_CHAR);
        act("$n stops $p and kills its engine.", ch, veh, NULL, TO_ROOM);
        act("$p stops as its engine is killed.", mob, veh, NULL, TO_ROOM);
    }

    if ( !(veh = revert_vehicle(mob)) )
        send_to_char("Ack! Couldn't convert the vehicle because:\r\n"
                     "Something gone and fucked up -- report this to the imms!\r\n",
                     ch);
}


void
do_turn (struct char_data *ch, char *argument)
{
    struct char_data *tank = ch->in_room->inside_mob;
    struct obj_data *veh = get_vehicle(ch->in_room);

    if ( tank == NULL || veh == NULL )
    {
        send_to_char("You must be inside an active vehicle.\r\n", ch);
        return;
    }

    struct char_data *gunner = find_manning(ch->in_room, MAN_TURRET | MAN_HUD);

    if ( gunner && gunner != ch )
    {
        send_to_char("You have someone manning the turret already!\r\n", ch);
        return;
    }

    char arg[MAX_INPUT_LENGTH];
    int door;

    one_argument(argument, arg);

    for ( door = 0; door < DIR_UP; door++ )
        if ( !str_prefix(arg, dir_name[door]) )
            break;

    if ( door == DIR_UP )
    {
        send_to_char("You can't turn the turret in that direction.\r\n", ch);
        return;
    }

    if ( !gunner )
        WAIT_STATE(ch, 4);
    else
        WAIT_STATE(ch, 2);

    act("You turn the turret to point to the $t.", ch, dir_name[door], 0, TO_CHAR);
    act("$n turns the turret to point to the $t.", ch, dir_name[door], 0, TO_ROOM);
    tank->turret_dir = door;
}
