/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  Ground ZERO improvements copyright pending 1994, 1995 by James Hilke   *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

#include    "ground0.h"

/* command procedures needed */
DECLARE_DO_FUN (do_look);
DECLARE_DO_FUN (do_recall);
DECLARE_DO_FUN (do_stand);
DECLARE_DO_FUN (do_gecho);
DECLARE_DO_FUN (do_push);


sh_int teleport_time = 0;


const int dir_cmod[][3] = {
    {0, 1, 0},   // north
    {1, 0, 0},   // east
    {0, -1, 0},  // south
    {-1, 0, 0},  // west
    {0, 0, 1},   // up
    {0, 0, -1}   // down
};


char *const dir_name[] = {
    "north",
    "east",
    "south",
    "west",
    "up",
    "down"
};


const sh_int rev_dir[] = {
    DIR_SOUTH,
    DIR_WEST,
    DIR_NORTH,
    DIR_EAST,
    DIR_DOWN,
    DIR_UP
};


/* local functions */
int find_door(CHAR_DATA * ch, char *arg);
int numeric_direction(char *arg);
bool has_key(CHAR_DATA * ch, int key);
int is_aggro(CHAR_DATA * ch, CHAR_DATA * vict);
void hit_mine(ROOM_DATA *, CHAR_DATA *);


/* external functions */
int damage_char(CHAR_DATA *, CHAR_DATA *, sh_int, bool, const char *);
void stop_manning(CHAR_DATA *);
OBJ_DATA *find_eq_char(CHAR_DATA *, int, unsigned int);
CHAR_DATA *find_manning(ROOM_DATA *, int);


ROOM_DATA *
index_room (ROOM_DATA * curr_room, sh_int x, sh_int y)
{
    return &(curr_room[x + curr_room->this_level->x_size * y]);
}


ROOM_DATA *
get_to_room (ROOM_DATA * room, sh_int d)
{
    sh_int max_x, max_y, new_x, new_y;
    ROOM_DATA *ref_room;

    if ( d != DIR_UP && d != DIR_DOWN )
    {
        max_x = room->this_level->x_size - 1;
        max_y = room->this_level->y_size - 1;
        new_x = room->x + dir_cmod[d][0];
        new_y = room->y + dir_cmod[d][1];
        ref_room = room->this_level->rooms_on_level;
    }
    else
    {
        LEVEL_DATA *level =
            (d ==
             DIR_UP ? room->this_level->level_up : room->this_level->
             level_down);

        if ( !level )
            return NULL;

        max_x = level->x_size - 1;
        max_y = level->y_size - 1;
        new_x =
            level->reference_x - (room->this_level->reference_x - room->x);
        new_y =
            level->reference_y - (room->this_level->reference_y - room->y);
        ref_room = level->rooms_on_level;
    }

    /* Wrap X coordinate. */
    if ( new_x < 0 )
        new_x = max_x;
    else if ( new_x > max_x )
        new_x = 0;

    /* Wrap Y coordinate. */
    if ( new_y < 0 )
        new_y = max_y;
    else if ( new_y > max_y )
        new_y = 0;

    return index_room(ref_room, new_x, new_y);
}


ROOM_DATA *
bad_room (CHAR_DATA * ch)
{
    CHAR_DATA *fch = NULL;
    char buf[MAX_INPUT_LENGTH];
    sh_int new_x, new_y;

    if ( IS_SET(ch->act, PLR_BADPOP) )
    {
        for ( fch = char_list; fch; fch = fch->next )
        {
            if ( fch == ch || !fch->in_room || fch->in_room->level != 0 )
                continue;
            if ( !team_table[ch->team].independent && ch->team == fch->team )
                continue;
            if ( !number_range(0, 3) )
                break;
        }
    }

    if ( fch )
    {
        new_x = fch->in_room->x + number_range(-4, 4);
        new_y = fch->in_room->x + number_range(-4, 4);

        if ( new_x < 0 )
            new_x = 0;
        else if ( new_x > the_city->x_size - 1 )
            new_x = the_city->x_size - 1;

        if ( new_y < 0 )
            new_y = 0;
        else if ( new_y > the_city->y_size - 1 )
            new_y = the_city->y_size - 1;

        sprintf(buf, "BADPOP: %s was sent to %s.", ch->names,
                IS_NPC(fch) ? fch->short_descript : fch->names);
        logmesg("%s", buf);
        wiznet(buf, NULL, NULL, WIZ_BADPOP, 0, get_trust(ch));
    }
    else
    {
        new_x = number_range(0, the_city->x_size - 1);
        new_y = number_range(0, the_city->y_size - 1);
    }

    return index_room(the_city->rooms_on_level, new_x, new_y);
}


void
do_enter (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj;

    if ( (obj = get_obj_list(ch, argument, ch->in_room->contents)) == NULL )
    {
        send_to_char("You don't see that here.\n\r", ch);
        return;
    }

    if ( IS_SET(ch->affected_by, AFF_DAZE) )
    {
        send_to_char("You are too dazed to do that.\n\r", ch);
        return;
    }

    if ( !obj->interior )
    {
        send_to_char("That does not seem to have an entrance.\n\r", ch);
        return;
    }

    if ( ch->in_room->inside_mob )
    {
        send_to_char("You cannot enter an object while inside a mob.\n\r",
                     ch);
        return;
    }

    if ( OBJ_CAPACITY(obj) &&
        count_people(obj->interior) > OBJ_CAPACITY(obj))
    {
        send_to_char("There's not enough room in there for you.\r\n", ch);
        return;
    }

    act("You enter $p.", ch, obj, NULL, TO_CHAR);
    act("$n enters $p.", ch, obj, NULL, TO_ROOM);

    char_from_room(ch);
    char_to_room(ch, obj->interior);

    if ( obj->item_type == ITEM_TEAM_VEHICLE )
    {
        SET_BIT(ch->temp_flags, IN_VEHICLE);
        obj->team = ch->team;
    }

    complete_movement(ch, DIR_UP);
}


void
do_teleport (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *rch;
    ROOM_DATA *room;

    if ( (ch->in_room != safe_area ) && ch->in_room != graveyard_area &&
        !teleport_time && ch->desc)
    {
        if (!g_forceAction &&
            (!argument[0] || str_prefix(argument, "emergency")))
        {
            send_to_char
                ("A scratchy voice comes through over your comm badge, "
                 "'&utI'm sorry, but\n\rredeployment systems are offline "
                 "at the moment.  We will notify you when they\n\rare "
                 "back on line.  If you require emergency transport, "
                 "then you may request it by typing 'teleport emergency'"
                 ", be sure you are in excellent physical condition "
                 "if you do this though.&n'\n\r", ch);
            return;
        }
        else
        {
            send_to_char("A scortching ray of light &Rburns&n you as "
                         "transports from above try to pinpoint your "
                         "location.\n\r", ch);
            damage_char(ch, ch->last_hit_by, 2500, TRUE, "teleporters");
        }
    }

    if (ch->desc && ch->in_room != safe_area &&
        ch->in_room != graveyard_area && ch->in_room->level < 0)
    {
        send_to_char("The transports could not reach your location.\r\n",
                     ch);
        return;
    }

    act("A helicopter roars in from overhead kicking up dust and "
        "dirt in a wide radius.\n\rYou are efficiently hustled "
        "aboard.  On your way to the dropoff, the\n\rSergeant in "
        "command briefly wishes you luck but you can see from "
        "the\n\rexpression on his face that he finds your survival "
        "highly questionable.\n\rThen before you know it, you are "
        "dropped off and the helicopter is gone once more.", ch, NULL,
        NULL, TO_CHAR);

    for ( rch = ch->in_room->people; rch; rch = rch->next_in_room )
    {
        act("A helicopter roars in from overhead kicking up dust and dirt in a " "wide radius.\n\r$n is hustled aboard as soldiers cover his " "safe boarding with deadly\n\rweapons of a make you have never " "seen.\n\rWith a roar, the helicopter takes to the sky and is " "quickly lost from view.", ch, NULL, rch, TO_VICT);
    }

    char_from_room(ch);

    if ( !IS_SET(ch->act, PLR_REPOP) )
    {
        room = bad_room(ch);
        char_to_room(ch, room);
    }
    else
    {
        char_to_room(ch, index_room(the_city->rooms_on_level,
                                    (the_city->x_size -
                                     1) * team_table[ch->team].x_mult,
                                    (the_city->y_size -
                                     1) * team_table[ch->team].y_mult));
    }

    for ( rch = ch->in_room->people; rch; rch = rch->next_in_room )
    {
        act("With a roar of displaced air, a helicopter arrives above you.\n\r" "Dust and dirt sting your face as it lands.\n\r$n is quickly pushed " "out and the helicopter speeds away into the sky once more.", ch, NULL, rch, TO_VICT);
    }

    do_look(ch, "auto");
    stop_manning(ch);
    REMOVE_BIT(ch->temp_flags, IN_VEHICLE);
    complete_movement(ch, DIR_UP);
}


void
do_leave (CHAR_DATA * ch, char *argument)
{
    ROOM_DATA *temp_room;
    OBJ_DATA *obj;

    if ( !ch->in_room->interior_of )
    {
        do_teleport(ch, argument);
        return;
    }
    else if ( ch->desc && ch->desc->botLagFlag == 2 )
    {
        WAIT_STATE(ch, 3);
        ch->desc->botLagFlag = 0;
    }

    obj = ch->in_room->interior_of;
    temp_room = obj->in_room;
    act("You leave $p.", ch, obj, NULL, TO_CHAR);
    act("$n has left.", ch, obj, NULL, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, temp_room);
    stop_manning(ch);
    REMOVE_BIT(ch->temp_flags, IN_VEHICLE);
    act("$n clambers out of $p.", ch, obj, NULL, TO_ROOM);
    complete_movement(ch, DIR_UP);
    return;
}


int
is_aggro (CHAR_DATA * ch, CHAR_DATA * vict)
{
    if ( IS_IMMORTAL(vict) )
        return 0;
    if ( ch->owner == vict )
        return 0;
    if ( ch->owner && ch->owner == vict->owner )
        return 0;
    if ( IS_SET(ch->act, PLR_AGGRO_ALL) )
        return 1;
    if ( IS_SET(vict->act, PLR_TRAITOR) )
        return 1;
    if ( IS_SET(ch->act, PLR_AGGRO_OTHER )
        && ((team_table[ch->team].independent && vict->owner != ch)
            || ch->team != vict->team))
        return 1;

    return 0;
}


bool
move_char (CHAR_DATA * ch, int door, bool follow, int walls)
{
    ROOM_DATA *in_room;
    ROOM_DATA *to_room;
    long pexit;
    CHAR_DATA *fch;
    CHAR_DATA *fch_next;

    if ( door < 0 || door > 5 )
    {
        logmesg("Error: do_move: bad door %d.", door);
        return (FALSE);
    }

    if ( ch->in_room->inside_mob && IS_SET(ch->temp_flags, IN_VEHICLE) )
    {
        control_vehicle(ch, ch->in_room->inside_mob, door);
        return (TRUE);
    }

    in_room = ch->in_room;
    pexit = in_room->exit[door];

    if ( IS_SET(pexit, EX_ISWALL) )
    {
        if ( !IS_SET(pexit, EX_ISNOBREAKWALL) && walls != MWALL_NOMINAL )
        {
            if ( walls == MWALL_DESTROY )
            {
                if ( IS_SET(ch->temp_flags, IS_VEHICLE) && ch->interior )
                {
                    CHAR_DATA *tmp = find_manning(ch->interior, MAN_DRIVE);

                    if ( tmp && tmp->in_room == ch->interior )
                    {
                        WAIT_STATE(tmp, 5);
                        act("You crash through a wall!", tmp, NULL, NULL,
                            TO_CHAR);
                        act("$n drives through a wall!", tmp, NULL, NULL,
                            TO_ROOM);
                    }
                }

                falling_wall(in_room, door);
                falling_wall(get_to_room(in_room, door), rev_dir[door]);
            }
        }
        else
        {
            send_to_char("You can't go that way.\r\n", ch);
            return (FALSE);
        }
    }

    if ( IS_SET(ch->affected_by, AFF_DAZE) )
    {
        send_to_char("You are too dazed to move.\r\n", ch);
        return (FALSE);
    }

    to_room = get_to_room(in_room, door);

    if ( room_is_private(to_room) )
    {
        send_to_char("That room is private right now.\r\n", ch);
        return (FALSE);
    }

    if ( ch->position < POS_STANDING )
        do_stand(ch, "");

    if (ch->in_room->mine &&
        IS_SET(ch->in_room->mine->general_flags, GEN_DIRECTIONAL))
        if ( ch->in_room->mine->orientation == door )
            hit_mine(ch->in_room, ch);

    if ( ch->in_room == safe_area )
        return (FALSE);

    char_from_room(ch);
    char_to_room(ch, to_room);
    do_look(ch, "auto");

    if ( in_room == to_room )     /* no circular follows */
        return (TRUE);

    for ( fch = in_room->people; fch != NULL; fch = fch_next )
    {
        fch_next = fch->next_in_room;

        if (fch->in_room == in_room && fch->leader == ch &&
            fch->position == POS_STANDING)
        {
            act("You follow $N.", fch, NULL, ch, TO_CHAR);
            move_char(fch, door, TRUE, MWALL_NOMINAL);
        }
    }

    complete_movement(ch, rev_dir[door]);
    WAIT_STATE(ch, 1);
    return (TRUE);
}


void
hit_mine (ROOM_DATA * room, CHAR_DATA * ch)
{
    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *mine = room->mine;

    if ( ch && find_worn_eq_char(ch, SEARCH_GEN_FLAG, GEN_DETECT_MINE) )
    {
        send_to_char
            ("Your metal detector alerts you to the presence of a mine.\r\n", 
		ch);
        if (room->this_level == the_city && room->mine->owner &&
            room->mine->owner->team != ch->team)
            SetSquareFeature(ch->team, room->x, room->y, SQRF_MINE);
        return;
    }

    /* Special handling of announcing mines. */
    if ( IS_SET(mine->extract_flags, EXTRACT_ANNOUNCE) )
    {
        CHAR_DATA *owner = mine->owner;
        bool doExtract = FALSE;
        bool neverExtract = FALSE;

        if ( ch == NULL )
            return;

        /*
         * HACK!  We initialise neverExtract to TRUE if we don't want it to be
         * extracted here (i.e., if we want it to be bang_obj()'d later).
         * Otherwise, we set it to FALSE and it may be extracted here.  We
         * do this to prevent notification of tripping a trembler mine to
         * the victim (they'd see a message from bang_obj() otherwise) if the
         * mine does nothing else (no damage, etc.).
         */

        neverExtract = (mine->extract_flags != EXTRACT_ANNOUNCE ||
                        mine->damage_char[0] ||
                        mine->damage_char[1] ||
                        mine->damage_char[2] ||
                        mine->damage_structural[0] ||
                        mine->damage_structural[1] ||
                        mine->damage_structural[2]);

        if ( !neverExtract && owner )
            if (owner == ch ||
                (!team_table[owner->team].independent &&
                 owner->team == ch->team))
                return;

        /* Prepare the messages for possible sending later. */
        if ( room->interior_of )
        {
            sprintf(buf,
                    "%s&W(%s&W) &Yhit &B%s&n inside &B%s&n at [&uX%d&n, &uX%d&n] L - &uL%d&n\r\n",
                    ch->names, team_table[ch->team].who_name,
                    mine->short_descr, room->interior_of->short_descr,
                    room->interior_of->in_room->x,
                    room->interior_of->in_room->y,
                    room->interior_of->in_room->level);
        }
        else
        {
            sprintf(buf,
                    "%s&W(%s&W) &Yhit &B%s&n at [&uX%d&n, &uX%d&n] L - &uL%d&n\r\n",
                    ch->names, team_table[ch->team].who_name,
                    mine->short_descr, room->x, room->y, room->level);
        }

        /* Send to appropriate parties, should we extract? */
        if ( owner )
        {
            if ( owner != ch && team_table[owner->team].independent )
            {
                send_to_char(buf, owner);
                doExtract = TRUE;
            }
            else if ( owner->team != ch->team )
            {
                send_to_team(buf, owner->team);
                doExtract = TRUE;
            }
        }
        else
        {
            do_gecho(NULL, buf);
            doExtract = TRUE;
        }

        if ( doExtract && !neverExtract )
        {
            mine->destination = NULL;
            obj_to_room(room->mine, explosive_area);
            extract_obj(room->mine, mine->extract_me);

            if ( ch )
                UnsetSquareFeature(ch->team, room->x, room->y, SQRF_MINE);

            room->mine = NULL;
            return;
        }
    }

    room->mine = NULL;
    mine->destination = NULL;

    if ( ch )
    {
        UnsetSquareFeature(ch->team, room->x, room->y, SQRF_MINE);
        obj_to_char(mine, ch);
    }
    else
        obj_to_room(mine, room);

    bang_obj(mine, 0);
}


bool
IsDepot ( struct room_data *rm, int minObj )
{
    struct obj_data *obj;
    int count = 0;

    if ( rm != ammo_repop[0] && rm != ammo_repop[1] &&
         rm != ammo_repop[2] )
        return false;
    else if ( minObj < 1 )
        return true;

    for ( obj = rm->contents; obj; obj = obj->next_content )
        count++;

    return (count >= minObj);
}


void
complete_movement (CHAR_DATA * ch, int fromDir)
{
    sh_int distance;
    char buf[MAX_INPUT_LENGTH];
    CHAR_DATA *fch;
    int dir;

    if ( !IS_SET(ch->act, PLR_WIZINVIS) )
    {
        for ( fch = char_list; fch; fch = fch->next )
        {
            if ( fch == ch )
                continue;
            if ( !can_see_linear_char(fch, ch, 0, 0) )
                continue;

            distance = 0;

            if ( (distance = ch->in_room->x - fch->in_room->x) != 0 )
                dir = (distance < 0 ? DIR_WEST : DIR_EAST);
            else if ( (distance = ch->in_room->y - fch->in_room->y) != 0 )
                dir = (distance < 0 ? DIR_SOUTH : DIR_NORTH);
            else
                dir = DIR_UP;

            if ( !distance )
            {
                strcpy(buf, "$N has arrived.");
                act(buf, fch, NULL, ch, TO_CHAR);
            }
            else
            {
                sprintf(buf, "$N has arrived %d %s.", ABS(distance),
                        dir_name[dir]);
                act(buf, fch, NULL, ch, TO_CHAR);
            }

            if ( fch->interior )
            {
                act(buf, fch->interior->people, NULL, ch, TO_CHAR);
                act(buf, fch->interior->people, NULL, ch, TO_ROOM);
            }

            if ( is_aggro(fch, ch) )
            {
                fch->fighting = ch;

                if ( IS_NPC(fch) && (fch->ld_behavior == BEHAVIOR_GUARD ) &&
                    (fch->in_room == ch->in_room))
                {
                    int door;

                    /* Ensure that there is an exit, first: */
                    for ( door = 0; door < 4; door++ )
                        if ( !ch->in_room->exit[door] )
                            break;

                    if ( door == 4 )
                    {
                        act("$n activates a teleportation device.",
                            fch, NULL, NULL, TO_ROOM);
                        do_teleport(ch, "emergency");
                    }

                    do
                    {
                        door = number_range(0, 3);
                    }
                    while ( ch->in_room->exit[door] );

                    if ( ch->position < POS_STANDING )
                        do_stand(ch, "");

                    sprintf(buf, "%s %s", ch->names, dir_name[door]);
                    do_push(fch, buf);
                }
            }

            if ( is_aggro(ch, fch) )
                ch->fighting = fch;
        }
    }

    if ( IsDepot(ch->in_room, 5) )
        SetSquareFeature(ch->team, ch->in_room->x, ch->in_room->y,
                         SQRF_DEPOT);

    if ( !ch->in_room->exit[DIR_DOWN] )
        SetSquareFeature(ch->team, ch->in_room->x, ch->in_room->y,
                         SQRF_DOWN);

    if (ch->in_room->mine &&
        (!IS_SET(ch->in_room->mine->general_flags, GEN_DIRECTIONAL) ||
         fromDir == ch->in_room->mine->orientation))
        hit_mine(ch->in_room, ch);
    else if ( !ch->in_room->mine )
        UnsetSquareFeature(ch->team, ch->in_room->x, ch->in_room->y,
                           SQRF_MINE);

    if ( !IS_SET(ch->temp_flags, IS_VEHICLE) )
    {
        char first[MAX_INPUT_LENGTH], second[MAX_INPUT_LENGTH];
        struct obj_data *obj = ch->in_room->contents;
        struct obj_data *next = NULL;
        char *ptr;

        for ( obj = ch->in_room->contents; obj != NULL; obj = next )
        {
            next = obj->next_content;

            if ( IS_SET(obj->general_flags, GEN_WALK_DAMAGE) )
            {
                ptr = one_argument(obj->attack_message, first);
                one_argument(ptr, second);

                act("&YOUCH!&n  You &ua$T $p!", ch, obj, first, TO_CHAR);
                act("$n &ua$T $p, here, and howls in pain.", ch, obj, second,
                    TO_ROOM);

                damage_char(ch, obj->owner, obj->damage_char[0], FALSE,
                            obj->short_descr);

                if ( obj->rounds_delay > 0 )
                {
                    send_to_char("It temporarily slows you down.\r\n", ch);
                    WAIT_STATE(ch, obj->rounds_delay);
                }

                if ( obj->ammo != -1 && --obj->ammo <= 0 )
                {
                    /* It's all used up. */
                    extract_obj(obj, obj->extract_me);
                }
            }
        }
    }
}


void
do_roll (struct char_data *ch, char *argument)
{
    char arg2[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    struct obj_data *obj;
    struct room_data *rm;
    int dist;
    int dir;

    argument = one_argument(argument, arg);
    one_argument(argument, arg2);

    if ( !*arg2 )
    {
        send_to_char("Roll what in what direction?!\r\n", ch);
        return;
    }
    else if ( !(obj = get_obj_list(ch, arg, ch->in_room->contents)) )
    {
        send_to_char("Nothing like that around here.\r\n", ch);
        return;
    }
    else if ( !IS_SET(obj->general_flags, GEN_CAN_PUSH) )
    {
        send_to_char("It won't roll.\r\n", ch);
        return;
    }

    for ( dir = 0; dir < NUM_OF_DIRS; dir++ )
        if ( !str_prefix(arg2, dir_name[dir]) )
            break;

    if ( dir == NUM_OF_DIRS )
    {
        send_to_char("That's not a direction.\r\n", ch);
        return;
    }
    else if ( dir == DIR_UP || dir == DIR_DOWN )
    {
        send_to_char("You can't roll something up or down.\r\n", ch);
        return;
    }

    rm = ch->in_room;
    dist = 0;

    while ( rm && !rm->exit[dir] )
    {
        rm = index_room(rm, dir_cmod[dir][0], dir_cmod[dir][1]);
        dist++;
    }

    act("$n rolls $p $T.", ch, obj, dir_name[dir], TO_ROOM);
    act("You roll $p $T.", ch, obj, dir_name[dir], TO_CHAR);
    send_object(obj, rm, dist);
}


void
do_push (CHAR_DATA * ch, char *argument)
{
    char arg2[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim = NULL;
    OBJ_DATA *obj = NULL;
    ROOM_DATA *to_room;
    int i;

    argument = one_argument(argument, arg);
    one_argument(argument, arg2);

    if ( !*arg || !*arg2 )
    {
        send_to_char("Push who (or what) where?!\r\n", ch);
        return;
    }
    else if ( (victim = get_char_room(ch, arg)) == NULL )
    {
        if ( (obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL )
        {
            send_to_char("That doesn't seem to be here.\r\n", ch);
            return;
        }
    }

    for ( i = DIR_NORTH; i <= DIR_WEST; i++ )
        if ( !str_prefix(arg2, dir_name[i]) )
            break;

    if ( i > DIR_WEST )
    {
        send_to_char("That's not a direction.\r\n", ch);
        return;
    }
    else if ( obj && !IS_SET(obj->general_flags, GEN_CAN_PUSH) )
    {
        send_to_char("You can't move that.\r\n", ch);
        return;
    }
    else if ( victim )
    {
        if ( IS_NPC(victim) && ch != victim->owner && !IS_IMMORTAL(ch) )
        {
            act("$E won't budge.", ch, NULL, victim, TO_CHAR);
            act("$n attempts to push you $t.", ch, dir_name[i], victim,
                TO_VICT);
            act("$n attempts to push $N $t.", ch, dir_name[i], victim,
                TO_NOTVICT);
            return;
        }
        else if ( victim == ch )
        {
            send_to_char("You can't push yourself.\r\n", ch);
            return;
        }
        else if ( victim->position < POS_STANDING )
        {
            act("$E needs to be standing up.", ch, NULL, victim, TO_CHAR);
            return;
        }
    }

    if ( ch->in_room->exit[i] )
    {
        send_to_char("A wall blocks your way.", ch);
        return;
    }

    to_room = index_room(ch->in_room, dir_cmod[i][0], dir_cmod[i][1]);

    if ( room_is_private(to_room) )
    {
        send_to_char("That room is private.\r\n", ch);
        return;
    }

    if ( obj )
    {
        obj_from_room(obj);
        obj_to_room(obj, to_room);
        act("You push $P $t.", ch, dir_name[i], obj, TO_CHAR);
        act("$n pushes $P $t.", ch, dir_name[i], obj, TO_ROOM);

        if ( to_room->mine )
        {
            hit_mine(to_room, NULL);
            UnsetSquareFeature(ch->team, to_room->x, to_room->y,
                               SQRF_MINE);
        }

        return;
    }

    act("$n pushes $N $t.", ch, dir_name[i], victim, TO_NOTVICT);
    act("You push $N $t.", ch, dir_name[i], victim, TO_CHAR);
    act("\r\n$n pushes you $t.\r\n", ch, dir_name[i], victim, TO_VICT);

    char_from_room(victim);
    char_to_room(victim, to_room);

    do_look(victim, "auto");
    complete_movement(victim, rev_dir[i]);

    if ( ch->team != victim->team && !IS_NPC(ch) )
        WAIT_STATE(ch, 1);
}


void
do_north (CHAR_DATA * ch, char *argument)
{
    move_char(ch, DIR_NORTH, FALSE, MWALL_NOMINAL);
}



void
do_east (CHAR_DATA * ch, char *argument)
{
    move_char(ch, DIR_EAST, FALSE, MWALL_NOMINAL);
}


void
do_south (CHAR_DATA * ch, char *argument)
{
    move_char(ch, DIR_SOUTH, FALSE, MWALL_NOMINAL);
}


void
do_west (CHAR_DATA * ch, char *argument)
{
    move_char(ch, DIR_WEST, FALSE, MWALL_NOMINAL);
}


void
do_up (CHAR_DATA * ch, char *argument)
{
    move_char(ch, DIR_UP, FALSE, MWALL_NOMINAL);
}


void
do_down (CHAR_DATA * ch, char *argument)
{
    move_char(ch, DIR_DOWN, FALSE, MWALL_NOMINAL);
}


int
numeric_direction (char *arg)
{
    if ( !str_prefix(arg, "north") )
        return DIR_NORTH;
    else if ( !str_prefix(arg, "east") )
        return DIR_EAST;
    else if ( !str_prefix(arg, "south") )
        return DIR_SOUTH;
    else if ( !str_prefix(arg, "west") )
        return DIR_WEST;
    else if ( !str_prefix(arg, "up") )
        return DIR_UP;
    else if ( !str_prefix(arg, "down") )
        return DIR_DOWN;
    else
        return -1;
}


void
do_stand (CHAR_DATA * ch, char *argument)
{
    switch (ch->position)
    {
        case POS_SLEEPING:
            send_to_char("You wake and stand up.\n\r", ch);
            act("$n wakes and stands up.", ch, NULL, NULL, TO_ROOM);
            ch->position = POS_STANDING;
            break;

        case POS_RESTING:
        case POS_SITTING:
            send_to_char("You stand up.\n\r", ch);
            act("$n stands up.", ch, NULL, NULL, TO_ROOM);
            ch->position = POS_STANDING;
            break;

        case POS_STANDING:
            send_to_char("You are already standing.\n\r", ch);
            break;

        case POS_FIGHTING:
            send_to_char("You are already fighting!\n\r", ch);
            break;
    }

    return;
}



void
do_rest (CHAR_DATA * ch, char *argument)
{
    switch (ch->position)
    {
        case POS_SLEEPING:
            send_to_char("You wake up and start resting.\n\r", ch);
            act("$n wakes up and starts resting.", ch, NULL, NULL,
                TO_ROOM);
            ch->position = POS_RESTING;
            break;

        case POS_RESTING:
            send_to_char("You are already resting.\n\r", ch);
            break;

        case POS_STANDING:
            send_to_char("You rest.\n\r", ch);
            act("$n sits down and rests.", ch, NULL, NULL, TO_ROOM);
            ch->position = POS_RESTING;
            break;

        case POS_SITTING:
            send_to_char("You rest.\n\r", ch);
            act("$n rests.", ch, NULL, NULL, TO_ROOM);
            ch->position = POS_RESTING;
            break;

        case POS_FIGHTING:
            send_to_char("You can't rest while fighting!\n\r", ch);
            break;
    }

    return;
}


void
do_sit (CHAR_DATA * ch, char *argument)
{
    switch (ch->position)
    {
        case POS_SLEEPING:
            send_to_char("You wake and sit up.\n\r", ch);
            act("$n wakes and sits up.\n\r", ch, NULL, NULL, TO_ROOM);
            ch->position = POS_SITTING;
            break;
        case POS_RESTING:
            send_to_char("You stop resting.\n\r", ch);
            ch->position = POS_SITTING;
            break;
        case POS_SITTING:
            send_to_char("You are already sitting down.\n\r", ch);
            break;
        case POS_FIGHTING:
            send_to_char("You can't sit while fighting.\n\r", ch);
            break;
        case POS_STANDING:
            send_to_char("You sit down.\n\r", ch);
            act("$n sits down on the ground.\n\r", ch, NULL, NULL,
                TO_ROOM);
            ch->position = POS_SITTING;
            break;
    }
    return;
}


void
do_sleep (CHAR_DATA * ch, char *argument)
{
    switch (ch->position)
    {
        case POS_SLEEPING:
            send_to_char("You are already sleeping.\n\r", ch);
            break;
        case POS_RESTING:
        case POS_SITTING:
        case POS_STANDING:
            send_to_char("You go to sleep.\n\r", ch);
            act("$n goes to sleep.", ch, NULL, NULL, TO_ROOM);
            ch->position = POS_SLEEPING;
            break;
        case POS_FIGHTING:
            send_to_char("You can't sleep while fighting!\n\r", ch);
            break;
    }

    return;
}



void
do_wake (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        do_stand(ch, argument);
        return;
    }

    if ( !IS_AWAKE(ch) )
    {
        send_to_char("You are asleep yourself!\n\r", ch);
        return;
    }

    if ( (victim = get_char_room(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }
    else if (victim->in_room == safe_area && victim->team != ch->team &&
             !ch->trust)
    {
        send_to_char("You can't do that.\r\n", ch);
        return;
    }

    if ( IS_AWAKE(victim) )
    {
        act("$N is already awake.", ch, NULL, victim, TO_CHAR);
        return;
    }

    victim->position = POS_STANDING;
    act("You wake $M.", ch, NULL, victim, TO_CHAR);
    act("$n wakes you.", ch, NULL, victim, TO_VICT);
    act("$n wakes $N.", ch, NULL, victim, TO_NOTVICT);
    return;
}
