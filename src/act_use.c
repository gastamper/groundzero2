/*
 * GroundZero2 is based upon code from GroundZero by Corey Hilke, et al.
 * GroundZero was based upon code from ROM by Russ Taylor, et al.
 * ROM was based upon code from Merc by Hatchett, Furey, and Kahn.
 * Merc was based upon code from DIKUMUD.
 *
 * DikuMUD was written by Katja Nyboe, Tom Madsen, Hans Henrik Staerfeldt,
 * Michael Seifert, and Sebastian Hammer.
 *
 */

/**
 * NAME:        usecom.c
 * SYNOPSIS:    The use command and its various parts.
 * AUTHOR:      satori
 * CREATED:     18 Feb 2001
 * MODIFIED:    18 Feb 2001
 */

#include "ground0.h"


static bool can_build_wall(struct char_data *, struct room_data *);
static int construct_wall(struct char_data *, struct obj_data *, char *);
static int evac_char(struct char_data *, struct obj_data *, char *);
static int backup_radio(struct char_data *, struct obj_data *, char *);
static int satellite_map(struct char_data *, struct obj_data *, char *);
static int heal_tank(struct char_data *, char *);
int use_kit_person(struct char_data *, struct obj_data *,
                   struct char_data *);
static int use_kit(struct char_data *, struct obj_data *, char *);
static int air_strike(struct char_data *, char *);
int fire_extinguisher(struct room_data *, int);
void show_battlemap(struct char_data *);

void
do_use (struct char_data *ch, char *argument)
{
    char *rest_of_line;
    char arg[MAX_INPUT_LENGTH];
    struct char_data *droid, *vict;
    struct obj_data *obj;

    rest_of_line = one_argument(argument, arg);

    if ( (obj = get_obj_carry(ch, arg)) == NULL )
    {
        send_to_char("You don't have that.\r\n", ch);
        return;
    }

    if ( IS_SET(obj->usage_flags, USE_AIRSTRIKE) )
    {
        act("$n talks quietly into $p.", ch, obj, NULL, TO_ROOM);
        if ( air_strike(ch, rest_of_line) )
            extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_FIRE) )
    {
        act("$n uses a $p.", ch, obj, NULL, TO_ROOM);
        if ( fire_extinguisher(ch->in_room, 1) )
            extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_TANKKIT) )
    {
        act("$n uses a $p.", ch, obj, NULL, TO_ROOM);
        if ( heal_tank(ch, rest_of_line) )
            extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_EVAC) )
    {
        act("$n talks quietly into $p.", ch, obj, NULL, TO_ROOM);
        if ( evac_char(ch, obj, rest_of_line) )
            extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_BACKUP) )
    {
        act("$n talks quietly into $p.", ch, obj, NULL, TO_ROOM);
        if ( backup_radio(ch, obj, rest_of_line) )
            obj_from_char(obj);
    }
    else if ( IS_SET(obj->usage_flags, USE_SATELLITE) )
    {
        if ( satellite_map(ch,obj,rest_of_line) )
            extract_obj(obj,obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_HEAL) )
    {
        if ( use_kit(ch, obj, rest_of_line) )
            extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_TURRET) )
    {
        droid = clone_mobile(turret, FALSE);
        char_from_room(droid);
        char_to_room(droid, ch->in_room);
        act("$n uses $p to assemble a turret.", ch, obj, NULL, TO_ROOM);
        act("You use $p to assemble a turret.", ch, obj, NULL, TO_CHAR);
        act("You carefully program it to attack anything it comes in "
            "contact with.", ch, NULL, NULL, TO_CHAR);
        droid->team = ch->team;
        droid->owner = ch;
        extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_DROID) )
    {
        droid = clone_mobile(pill_box, FALSE);
        char_from_room(droid);
        char_to_room(droid, ch->in_room);
        act("$n uses $p to assemble a guard droid.", ch, obj, NULL,
            TO_ROOM);
        act("You use $p to assemble a guard droid.", ch, obj, NULL,
            TO_CHAR);
        act("You carefully program it to attack anything it comes in "
            "contact with.", ch, NULL, NULL, TO_CHAR);
        droid->team = ch->team;
        droid->owner = ch;
        extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_SEEKER_DROID) )
    {
        one_argument(rest_of_line, arg);
        if ( !arg[0] )
        {
            send_to_char("You must specify a target for the droid to "
                         "attempt to destroy.", ch);
            return;
        }
        if ( (vict = get_char_world(ch, arg)) == NULL )
        {
            send_to_char("Who's that?!\r\n", ch);
            return;
        }
        droid = clone_mobile(pill_box, FALSE);
        char_from_room(droid);
        char_to_room(droid, ch->in_room);
        act("$n uses $p to assemble a seeker droid.", ch, obj, NULL,
            TO_ROOM);
        act("You use $p to assemble a seeker droid.", ch, obj, NULL,
            TO_CHAR);
        act("You carefully program it to hunt down $N.", ch, NULL, vict,
            TO_CHAR);
        droid->ld_behavior = BEHAVIOR_SEEKING_PILLBOX;
        droid->chasing = droid->fighting = vict;
        droid->team = ch->team;
        droid->owner = ch;
        extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_WALL) )
    {
        one_argument(rest_of_line, arg);

        if ( !*arg )
        {
            send_to_char
                ("You must specify what exit to block with the wall.\r\n",
                 ch);
            return;
        }

        if ( construct_wall(ch, obj, arg) )
            extract_obj(obj, obj->extract_me);
    }
    else if ( IS_SET(obj->usage_flags, USE_HEALTH_MONITOR) )
    {
        one_argument(rest_of_line, arg);

        if ( !*arg )
        {
            send_to_char("Get whose condition?\r\n", ch);
            return;
        }
        else if ( !(vict = get_char_world(ch, arg)) )
        {
            send_to_char("Who's that?!\r\n", ch);
            return;
        }

        act("$N$t", ch, diagnose(vict), vict, TO_CHAR);
        act("$n scanned your health.", ch, NULL, vict, TO_VICT);

        if ( --obj->ammo <= 0 )
            extract_obj(obj, obj->extract_me);
        else
        {
            sprintf(arg, "You have %d uses left on $p.", obj->ammo);
            act(arg, ch, obj, NULL, TO_CHAR);
        }
    }
    else if ( IS_SET(obj->usage_flags, USE_BATTLEMAP) )
    {
        show_battlemap(ch);

        if ( --obj->ammo <= 0 )
            extract_obj(obj, obj->extract_me);
        else
        {
            sprintf(arg, "\r\r\n\nYou have %d uses left on $p.",
                    obj->ammo);
            act(arg, ch, obj, NULL, TO_CHAR);
        }
    }
    else if ( IS_SET(obj->usage_flags, USE_MAKE_OBJ_ROOM) )
    {
        struct obj_data *spawn;

        if ( obj->spawnMax != -1 )
        {
            int max = 0;

            for ( spawn = ch->in_room->contents;
                  max < obj->spawnMax && spawn;
                  spawn = spawn->next_content )
            {
                if ( spawn->pIndexData == obj->spawnTrail )
                    max++;
            }

            if ( max >= obj->spawnMax )
            {
                send_to_char("The room's already cluttered with stuff.\r\n", ch);
                return;
            }
        }

        spawn = create_object(obj->spawnTrail, 0);
        spawn->extract_me = 1;

        if ( spawn == NULL )
        {
            act("$p appears to be empty.", ch, obj, NULL, TO_CHAR);
            return;
        }

        act("You &uaset down $p from $P.", ch, spawn, obj, TO_CHAR);
        act("$n &uasets down $p from $P.", ch, spawn, obj, TO_ROOM);

        claimObject(ch, spawn);
        obj_to_room(spawn, ch->in_room);
        extract_obj(obj, obj->extract_me);
    }
    else
        do_wear(ch, argument);
}


static bool
can_build_wall (struct char_data *ch, ROOM_DATA * rm)
{
    int walls = 0;
    int i;

    for ( i = 0; i < NUM_OF_DIRS; i++ )
    {
        if ( !IS_SET(rm->exit[i], EX_ISWALL) )
            continue;
        if ( IS_SET(rm->exit[i], EX_FAKEWALL) )
            break;

        if ( i != DIR_UP && i != DIR_DOWN )
            walls++;
    }

    if ( i != NUM_OF_DIRS )
    {
        send_to_char("You can only build one wall per room.\r\n", ch);
        return (FALSE);
    }
    else if ( walls >= 3 )
    {
        send_to_char("You can't completely close off a room.\r\n", ch);
        return (FALSE);
    }

    return TRUE;
}


static int
construct_wall (struct char_data *ch, struct obj_data *obj, char *arg)
{
    ROOM_DATA *toroom;
    ROOM_DATA *wasin;
    int dir;

    for ( dir = 0; dir < NUM_OF_DIRS; dir++ )
        if ( !str_prefix(arg, dir_name[dir]) )
            break;

    if ( dir == NUM_OF_DIRS )
    {
        send_to_char("That's not a valid direction.\r\n", ch);
        return 0;
    }
    else if ( dir == DIR_UP || dir == DIR_DOWN )
    {
        send_to_char("Sorry, you can't build walls up or down.\r\n", ch);
        return 0;
    }
    else if ( IS_SET(ch->in_room->exit[dir], EX_ISWALL) )
    {
        send_to_char("There's already a wall there.\r\n", ch);
        return 0;
    }
    else if ( !(toroom = get_to_room(ch->in_room, dir)) )
    {
        send_to_char("You can't go that way.\r\n", ch);
        return 0;
    }
    else if ( IS_SET(toroom->exit[rev_dir[dir]], EX_ISWALL) )
    {
        send_to_char
            ("There's already a wall there, on the other side.\r\n", ch);
        logmesg("BUG: One-sided wall connecting (%d, %d) and (%d, %d)",
                   ch->in_room->x, ch->in_room->y, toroom->x, toroom->y);
    }
    else if ( !can_build_wall(ch, ch->in_room ) ||
             !can_build_wall(ch, toroom))
        return 0;

    act("You use a $p to construct a wall to the $T.", ch, obj,
        dir_name[dir], TO_CHAR);
    act("$n uses a $p to construct a wall to the $T.", ch, obj,
        dir_name[dir], TO_ROOM);
    SET_BIT(ch->in_room->exit[dir], EX_ISWALL | EX_FAKEWALL);
    SET_BIT(toroom->exit[rev_dir[dir]], EX_ISWALL | EX_FAKEWALL);

    wasin = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, toroom);
    act("A wall to the $t is erected by $n, from the other side.", ch,
        dir_name[rev_dir[dir]], NULL, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, wasin);

    return 1;
}


static int
evac_char (struct char_data *ch, struct obj_data *obj, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    struct char_data *evacTo;
    ROOM_DATA *dest;

    if ((ch->in_room->interior_of &&
         ch->in_room->interior_of->item_type == ITEM_TEAM_ENTRANCE) ||
        ch->in_room == safe_area)
    {
        send_to_char("The radio signal is too weak here.\r\n", ch);
        return 0;
    }

    one_argument(argument, arg);

    if ( (dest = find_location(ch, argument)) )
    {
        if ( !dest->level )
        {
            act("You talk into $p, requesting transport.\r\nAlmost "
                "instantly, a bright light surrounds you and you are at "
                "your destination.\r\n", ch, obj, NULL, TO_CHAR);
            if ( ch->position < POS_STANDING )
                do_stand(ch, "");
            do_goto(ch, argument);
            complete_movement(ch, DIR_UP);
            return 1;
        }
        else
        {
            send_to_char
                ("Transports cannot reach the saferoom or lower levels.\r\n",
                 ch);
            return 0;
        }
    }
    else if ( (evacTo = get_char_world(ch, arg)) )
    {
        /* To prevent evac booming by low rep chars */
        if ( RANK(ch) < RANK_HUNTER || RANK(evacTo) > RANK(ch) )
        {
            act("You talk into $p, requesting transport.\r\nUnfortunately, the " "officer on the other side informs you that you are of " "insufficient rank for your request to be honored.", ch, obj, NULL, TO_CHAR);

            send_to_char
                ("\r\nThe usual way to use an evac radio is with &cUSE EVAC <x> <y>&n\r\n",
                 ch);
            send_to_char("See &cHELP EVAC&n for more.\r\n", ch);
            return 0;
        }

        if ( evacTo->in_room && evacTo->in_room->level == 0 )
        {
            int x, y;

            act("You talk into $p, requesting transport.\r\nAlmost "
                "instantly, a bright light surrounds you.  It flickers "
                "intently.\r\nYou hear a soft humming sound.  And then "
                "you are at your destination.\r\nOr, at least, near it.",
                ch, obj, NULL, TO_CHAR);

            x = evacTo->in_room->x - number_range(-4, 4);
            y = evacTo->in_room->y - number_range(-4, 4);
            x = UMAX(0, UMIN(evacTo->in_room->this_level->x_size - 1, x));
            y = UMAX(0, UMIN(evacTo->in_room->this_level->y_size - 1, y));

            sprintf(arg, "%d %d 0", x, y);
            if ( ch->position < POS_STANDING )
                do_stand(ch, "");
            do_goto(ch, arg);
            complete_movement(ch, DIR_UP);
            return 1;
        }
        else
        {
            send_to_char("The transports couldn't lock on to that person.\r\n", ch);
            return 0;
        }
    }
    if ( RANK(ch) >= RANK_HUNTER )
        send_to_char("You must specify where you want to go in the form x y "
                 "level, or specify the person you'd like transport to.\r\n", ch);
    else
        send_to_char("You must specify where you want to go in the form x y level.\r\n", ch);
    return 0;
}


static int
backup_radio (struct char_data *ch, struct obj_data *obj, char *argument)
{
    ROOM_DATA *dest;
    struct char_data *mob;
    int i;

    if ((ch->in_room->interior_of &&
         (ch->in_room->interior_of->item_type == ITEM_TEAM_ENTRANCE)) ||
        (ch->in_room == safe_area))
    {
        send_to_char("The radio signal is too weak here.\r\n", ch);
        return 0;
    }

    if ( !*argument )
        dest = ch->in_room;
    else if ( (dest = find_location(ch, argument)) != NULL && !dest->level )
    {
        act("You talk into $p, requesting the assistance of paratroopers.\r\n" "A voice comes back, \"&rRoger, troopers dispatched.&n\"", ch, obj, NULL, TO_CHAR);
    }
    else
    {
        send_to_char("Usage: use radio [coordinates]\r\n"
                     "If you do not specify coordinates, the troopers will follow you.\r\n",
                     ch);
        return 0;
    }

    obj->generic_counter = 3;

    for ( i = 0; i < 3; i++ )
    {
        mob = clone_mobile(trooper, FALSE);

        char_from_room(mob);
        char_to_room(mob, dest);

        mob->team = ch->team;
        mob->owner = ch;
        mob->genned_by = obj;

        if ( ch->in_room == dest )
        {
            add_follower(mob, ch);
            act("$N parachutes down from the sky, landing at your feet.",
                ch, NULL, mob, TO_CHAR);
        }
        else
        {
            mob->ld_behavior = 3;
        }

        act("$n parachutes down from the sky, landing at your feet.", mob,
            NULL, ch, TO_ROOM);
    }

    return 1;
}


static int
heal_tank (struct char_data *ch, char *argument)
{
    struct char_data *tank;

    tank = ch->in_room->inside_mob;
    if ( !tank )
    {
        send_to_char
            ("You must be in a running tank in order to repair it.", ch);
        return 0;
    }

    tank->hit = tank->hit + 5000;
    if ( tank->hit > tank->max_hit )
        tank->hit = tank->max_hit;

    send_to_char
        ("You use your kit to repair the tank.\r\n", ch);
    return 1;                   /* worked out */
}


int
use_kit_person (struct char_data *ch, struct obj_data *obj,
               struct char_data *targ)
{
    act("$n uses $p on you.", ch, obj, targ, TO_VICT);
    act("You use $p on $M.", ch, obj, targ, TO_CHAR);
    act("$n uses $p on $N.", ch, obj, targ, TO_NOTVICT);
    heal_char(targ, obj->damage_char[0]);
    return (!IS_NPC(ch));
}


static int
use_kit (struct char_data *ch, struct obj_data *obj, char *argument)
{
    char who[MAX_INPUT_LENGTH];
    struct char_data *targ;

    one_argument(argument, who);

    if ( !who[0] )
    {
        if ( ch->hit < ch->max_hit )
            act("You use $p.", ch, obj, NULL, TO_CHAR);
        else
            act("You waste $p.", ch, obj, NULL, TO_CHAR);
        act("$n uses $p.", ch, obj, NULL, TO_ROOM);
        heal_char(ch, obj->damage_char[0]);
        return (!IS_NPC(ch));
    }
    else if ( !(targ = get_char_room(ch, who)) )
    {
        send_to_char("They aren't here.\r\n", ch);
        return (0);
    }

    return (use_kit_person(ch, obj, targ));
}


static int
satellite_map (struct char_data *ch, struct obj_data *obj, char *argument)
{
    char who[MAX_INPUT_LENGTH];
    struct char_data *targ;

    void do_topology(struct char_data *, char *);

    one_argument(argument, who);

    if ( !who[0] ) // locate self
    {
        act("$n uses $p.", ch, obj, NULL, TO_ROOM);
        do_topology(ch, "0");
        return (1);
    }
    else if ( !(targ = get_char_room(ch, who)) )
    {
        send_to_char("They aren't here.\r\n", ch);
        return (0);
    }
    send_to_char("You can't use that on others.\r\n",ch);
    return (0);
}


static int
air_strike (struct char_data *ch, char *argument)
{
    ROOM_DATA *start_room, *temp_room;
    struct obj_data *the_napalm;
    sh_int start_x, start_y, end_x, end_y, x, y;
    int radius = 1;

    if ( VNUM_NAPALM == -1 || VNUM_FIRE == -1 )
    {
        send_to_char("Napalm and fire are currently disabled.\r\n", ch);
        return 0;
    }

    if ((ch->in_room->interior_of &&
         (ch->in_room->interior_of->item_type == ITEM_TEAM_ENTRANCE)) ||
        (ch->in_room == safe_area))
    {
        send_to_char("The radio signal is too weak here.\r\n", ch);
        return 0;
    }
    start_room = find_location(ch, argument);
    if ( !start_room )
    {
        send_to_char
            ("You must specify which coordinates you want bombed in "
             "the form x y level.\r\n", ch);
        return 0;
    }
    if ( start_room->level )
    {
        send_to_char("Lower levels cannot be reached by air strikes.\r\n", ch);
        return 0;
    }
    if ( (start_x = start_room->x - radius) < 0 )
        start_x = 0;
    if ( (start_y = start_room->y - radius) < 0 )
        start_y = 0;
    if ((end_x =
         start_room->x + radius) > start_room->this_level->x_size - 1)
        end_x = start_room->this_level->x_size - 1;
    if ((end_y =
         start_room->y + radius) > start_room->this_level->y_size - 1)
        end_y = start_room->this_level->y_size - 1;

    for ( x = start_x; x <= end_x; x++ )
        for ( y = start_y; y <= end_y; y++ )
        {
            temp_room =
                index_room(start_room->this_level->rooms_on_level, x, y);
            obj_to_room(the_napalm =
                        create_object(get_obj_index(VNUM_NAPALM), 0),
                        temp_room);
            claimObject(ch, the_napalm);
            if ( temp_room->people )
            {
                act("A bomber roars overhead dropping $p which lands everwhere!", temp_room->people, the_napalm, NULL, TO_CHAR);
                act("A bomber roars overhead dropping $p which lands everwhere!", temp_room->people, the_napalm, NULL, TO_ROOM);
            }
            bang_obj(the_napalm, 1);
        }
    return 1;
}
