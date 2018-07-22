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
DECLARE_DO_FUN (do_say);
DECLARE_DO_FUN (do_wear);
DECLARE_DO_FUN (do_goto);
DECLARE_DO_FUN (do_stand);
DECLARE_DO_FUN (do_gecho);
char *npc_parse(const char *, struct char_data *,
                struct char_data *, struct obj_data *, const char *);


/*
 * Local functions.
 */
bool remove_obj(struct char_data *ch, int iWear, bool fReplace,
                int wearing);
bool wear_obj(struct char_data *, struct obj_data *, int, bool, bool,
              bool);
void fire_extinguisher(struct room_data *room, int recurs_depth);
OBJ_DATA *find_eq_char(struct char_data *, int, unsigned int);
void bug_object_state(OBJ_DATA *);
void claimObject(CHAR_DATA *, OBJ_DATA *);


void
send_object (OBJ_DATA * obj, ROOM_DATA * dest_room, sh_int travel_time)
{
    if ( obj->carried_by )
    {
        obj->startpoint = obj->carried_by->in_room;
        obj_from_char(obj);
    }
    else if ( obj->in_room )
    {
        obj->startpoint = obj->in_room;
        obj_from_room(obj);
    }
    else
    {
        logmesg("REALBUG in send_object");
        return;
    }

    obj_to_room(obj, explosive_area);
    obj->arrival_time = travel_time;
    obj->destination = dest_room;
}

void
do_toss (struct char_data *ch, char *argument)
{
    char *const dir_name[] = {
        "north", "east", "south", "west", "up", "down"
    };

    char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
    sh_int n_dir, num_squares;
    struct char_data *thrown_at;
    ROOM_DATA *dest_room = NULL;
    OBJ_DATA *obj;

    thrown_at = NULL;
    argument = one_argument(argument, arg);

    if ( !arg[0] )
    {
        send_to_char("Toss what?", ch);
        return;
    }
    obj = get_obj_carry(ch, arg);
    if ( obj == NULL )
    {
        send_to_char("You don't have that.", ch);
        return;
    }
    if ( ch->in_room->level < 0 )
    {
        send_to_char("No where to throw it to.", ch);
        return;
    }
    argument = one_argument(argument, arg);
    if ( !arg[0] )
    {
        send_to_char("Toss it where?", ch);
        return;
    }

    num_squares = 0;
    if ( is_number(arg) )
    {
        num_squares = atoi(arg);
        if ( num_squares < 1 )
        {
            send_to_char("The number must be positive.", ch);
            return;
        }
        one_argument(argument, arg);
        if ( !arg[0] )
        {
            send_to_char("Yes, but in what direction?", ch);
            return;
        }
    }
    else
        num_squares = 1;

    if ( (n_dir = numeric_direction(arg)) == -1 )
    {
        thrown_at = get_char_world(ch, arg);
        if ( !thrown_at )
        {
            send_to_char("That is not a valid direction or player's name.",
                         ch);
            return;
        }
        if ( !can_see_linear_char(ch, thrown_at, 0, 0 ) ||
            (thrown_at->in_room->x - ch->in_room->x > 2) ||
            (thrown_at->in_room->x - ch->in_room->x < -2) ||
            (thrown_at->in_room->y - ch->in_room->y > 2) ||
            (thrown_at->in_room->y - ch->in_room->y < -2))
        {
            send_to_char("They are not in range to throw it at them.", ch);
            return;
        }
        if ( ch->in_room->x - thrown_at->in_room->x > 0 )
            n_dir = DIR_WEST;
        else if ( ch->in_room->x - thrown_at->in_room->x < 0 )
            n_dir = DIR_EAST;
        else if ( ch->in_room->y - thrown_at->in_room->y > 0 )
            n_dir = DIR_SOUTH;
        else
            n_dir = DIR_NORTH;
    }
    else
    {
        if ( (n_dir == DIR_UP) || (n_dir == DIR_DOWN) )
        {
            send_to_char("You cannot throw an item up or down.\r\n", ch);
            return;
        }
        if ( num_squares >= 2 )
        {
            send_to_char("You &uathrow&n it as hard as you can!\r\n", ch);
            num_squares = 2;
        }
    }

    if ( num_squares == 0 )
        num_squares = 1;

    if ( !thrown_at )
    {
        switch (n_dir)
        {
            case 0:
                if (ch->in_room->y + num_squares >
                    ch->in_room->this_level->y_size - 1)
                    num_squares =
                        ch->in_room->this_level->y_size - ch->in_room->y -
                        1;
                dest_room = index_room(ch->in_room, 0, num_squares);
                break;
            case 1:
                if (ch->in_room->x + num_squares >
                    ch->in_room->this_level->x_size - 1)
                    num_squares =
                        ch->in_room->this_level->x_size - ch->in_room->x -
                        1;
                dest_room = index_room(ch->in_room, num_squares, 0);
                break;
            case 2:
                if ( ch->in_room->y - num_squares < 0 )
                    num_squares = ch->in_room->y;
                dest_room = index_room(ch->in_room, 0, -num_squares);
                break;
            case 3:
                if ( ch->in_room->x - num_squares < 0 )
                    num_squares = ch->in_room->x;
                dest_room = index_room(ch->in_room, -num_squares, 0);
                break;
        }
    }
    else
        dest_room = thrown_at->in_room;

    if ( (ch->in_room->level > 0) && !can_see_linear_room(ch, dest_room) )
    {
        send_to_char
            ("You aren't on the top level of the city, so cannot throw "
             "objects over walls.", ch);
        return;
    }

    sprintf(buf, "$n &uathrows&n $p &uE%s&n.", dir_name[n_dir]);
    act(buf, ch, obj, NULL, TO_ROOM);
    sprintf(buf, "You &uathrow&n $p &uE%s&n.", dir_name[n_dir]);
    act(buf, ch, obj, NULL, TO_CHAR);

    send_object(obj, dest_room, num_squares);
    if ( !obj->owner || (obj->owner != ch) )
        claimObject(ch, obj);
}


static void
helper_unload (struct char_data *ch, OBJ_DATA * wpn, int sec)
{
    char buf[MAX_INPUT_LENGTH];
    OBJ_DATA *ammo;

    if ( !wpn )
    {
        sprintf(buf, "You don't have a %s weapon.\r\n",
                (sec ? "secondary" : "primary"));
        send_to_char(buf, ch);
        return;
    }
    else if ( IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED) )
    {
        sprintf(buf, "Your %s weapon does not require ammunition.\r\n",
                (sec ? "secondary" : "primary"));
        send_to_char(buf, ch);
        return;
    }
    else if ( !(ammo = find_ammo(wpn)) )
    {
        send_to_char("The weapon is already empty.\r\n", ch);
        return;
    }
    else if ( count_carried(ch) >= MAX_NUMBER_CARRY - 1 )
    {
        send_to_char("You can't carry that many items.\r\n", ch);
        return;
    }

    act("You &uaremove&n $P from $p.", ch, wpn, ammo, TO_CHAR);
    act("$n &uaremoves&n $P from $p.", ch, wpn, ammo, TO_ROOM);
    obj_from_obj(ammo);
    obj_to_char(ammo, ch);
}


void
do_unload (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if ( *arg )
    {
        if ( !str_prefix(arg, "primary") )
        {
            helper_unload(ch, get_eq_char(ch, WEAR_WIELD), 0);
            return;
        }
        else if ( !str_prefix(arg, "secondary") )
        {
            helper_unload(ch, get_eq_char(ch, WEAR_SEC), 0);
            return;
        }
    }

    helper_unload(ch, get_eq_char(ch, WEAR_WIELD), 0);
    helper_unload(ch, get_eq_char(ch, WEAR_SEC), 1);
}


static void
helper_load (struct char_data *ch, OBJ_DATA * wpn, char *arg, int sec)
{
    char buf[MAX_INPUT_LENGTH];
    OBJ_DATA *ammo;

    if ( !wpn || wpn->item_type != ITEM_WEAPON )
        return;

    if ( IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED) )
    {
        sprintf(buf, "Your %s weapon does not require ammunition\r\n",
                (sec ? "secondary" : "primary"));
        send_to_char(buf, ch);
        return;
    }

    if ( (ammo = find_ammo(wpn)) != NULL )
    {
        if ( ammo->ammo > 0 )
        {
            sprintf(buf, "There %s still %d round%s in your %s "
                    "weapon.\r\n", (ammo->ammo == 1 ? "is" : "are"),
                    ammo->ammo, (ammo->ammo == 1 ? "" : "s"),
                    (sec ? "secondary" : "primary"));
            send_to_char(buf, ch);
            return;
        }
        else
        {
            obj_from_obj(ammo);
            extract_obj(ammo, ammo->extract_me);
        }
    }

    if ( !arg )
    {
        if ( !(ammo = find_eq_char(ch, SEARCH_AMMO_TYPE, wpn->ammo_type)) )
        {
            sprintf(buf, "You don't have ammo for your %s weapon.\r\n",
                    (sec ? "secondary" : "primary"));
            send_to_char(buf, ch);
            return;
        }
    }
    else
    {
        if ( (ammo = get_obj_carry(ch, arg)) == NULL )
        {
            send_to_char("You don't have that item.\r\n", ch);
            return;
        }
        else if (ammo->item_type != ITEM_AMMO ||
                 ammo->ammo_type != wpn->ammo_type)
        {
            act("$p is not ammo for $P.", ch, ammo, wpn, TO_CHAR);
            return;
        }
    }

    /*
     * Do we really need to do this?  If not, we resolve a big graveyard problem
     * and can be quite happy about eliminating (null) and related crashes.

     claimObject(ch, ammo);

     */

    obj_from_char(ammo);
    obj_to_obj(ammo, wpn);
    act("You &uaload&n $P into $p.\r\n&YCHCK-CHCK . . . CLUNK&n\r\n"
        "&RGrrrrrrrrr. . .&n", ch, wpn, ammo, TO_CHAR);
    act("$n &ualoads&n $P into $p.\r\n&YCHCK-CHCK . . . CLUNK&n", ch, wpn,
        ammo, TO_ROOM);
}


void
do_load (struct char_data *ch, char *argument)
{
    OBJ_DATA *the_weapon, *sec_weapon;
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

    the_weapon = get_eq_char(ch, WEAR_WIELD);
    sec_weapon = get_eq_char(ch, WEAR_SEC);

    if ( !the_weapon && !sec_weapon )
    {
        send_to_char("You aren't wielding anything to load.\r\n", ch);
        return;
    }

    one_argument(one_argument(argument, arg), arg2);
    helper_load(ch, the_weapon, !arg[0] ? NULL : arg, FALSE);
    helper_load(ch, sec_weapon, !arg2[0] ? NULL : arg2, TRUE);
}


void
get_obj (struct char_data *ch, OBJ_DATA * obj, OBJ_DATA * container)
{
    if ( !CAN_WEAR(obj, ITEM_TAKE) )
    {
        send_to_char("You can't take that.\r\n", ch);
        return;
    }

    if ( count_carried(ch) >= MAX_NUMBER_CARRY - 1 && !get_trust(ch) )
    {
        send_to_char("You can't carry that many items.\r\n", ch);
        return;
    }

    act("You get $p.", ch, obj, NULL, TO_CHAR);
    act("$n gets $p.", ch, obj, NULL, TO_ROOM);

    obj_from_room(obj);
    obj_to_char(obj, ch);
}


void
do_get (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    bool found;

    one_argument(argument, arg);

    /* Get type. */
    if ( arg[0] == '\0' )
    {
        send_to_char("Get what?\r\n", ch);
        return;
    }

    if ( str_cmp(arg, "all") && str_prefix("all.", arg) )
    {
        /* 'get obj' */
        obj = get_obj_list(ch, arg, ch->in_room->contents);

        if ( obj == NULL )
        {
            act("I see no '$T' here.", ch, NULL, arg, TO_CHAR);
            return;
        }

        get_obj(ch, obj, NULL);
    }
    else
    {
        /* 'get all' or 'get all.obj' */
        found = FALSE;

        for ( obj = ch->in_room->contents; obj != NULL; obj = obj_next )
        {
            obj_next = obj->next_content;

            if ( (arg[3] == '\0' || is_name_obj(&arg[4], obj->name)) )
            {
                found = TRUE;
                get_obj(ch, obj, NULL);
            }
        }

        if ( !found )
        {
            if ( arg[3] == '\0' )
                send_to_char("I see nothing here.\r\n", ch);
            else
                act("I see no '$T' here.", ch, NULL, &arg[4], TO_CHAR);
        }
    }
}


void
drop_obj (struct char_data *ch, struct obj_data *obj)
{
    act("$n drops $p.", ch, obj, NULL, TO_ROOM);
    act("You drop $p.", ch, obj, NULL, TO_CHAR);

    obj_from_char(obj);
    obj_to_room(obj, ch->in_room);
}


void
do_drop (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    bool found;

    strcpy(arg, argument);

    if ( arg[0] == '\0' )
    {
        send_to_char("Drop what?\r\n", ch);
        return;
    }

    if ( str_cmp(arg, "all") && str_prefix("all.", arg) )
    {
        /* 'drop obj' */
        if ( (obj = get_obj_carry(ch, arg)) == NULL )
        {
            send_to_char("You do not have that item.\r\n", ch);
            return;
        }

        drop_obj(ch, obj);
        return;
    }

    /* 'drop all' or 'drop all.obj' */
    found = FALSE;

    for ( obj = ch->carrying; obj != NULL; obj = obj_next )
    {
        obj_next = obj->next_content;

        if ( obj->carried_by != ch )
            logmesg("do_drop: carried_by is not ch");
        if ( (arg[3] == '\0' || is_name_obj(&arg[4], obj->name) ) &&
            obj->wear_loc == WEAR_NONE)
        {
            found = TRUE;
            drop_obj(ch, obj);
        }
    }

    if ( !found )
    {
        if ( arg[3] == '\0' )
            act("You are not carrying anything.", ch, NULL, arg, TO_CHAR);
        else
            act("You are not carrying any '$T'.", ch, NULL, &arg[4],
                TO_CHAR);
    }
}



void
do_give (struct char_data *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    struct char_data *victim;
    OBJ_DATA *obj;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if ( arg1[0] == '\0' || arg2[0] == '\0' )
    {
        send_to_char("Give what to whom?\r\n", ch);
        return;
    }

    if ( (obj = get_obj_carry(ch, arg1)) == NULL )
    {
        send_to_char("You do not have that item.\r\n", ch);
        return;
    }

    if ( obj->wear_loc != WEAR_NONE )
    {
        send_to_char("You must remove it first.\r\n", ch);
        return;
    }

    if ( obj->timer > 0 )
    {
        send_to_char("It's live, toss it or drop it.\r\n", ch);
        return;
    }

    if ( (victim = get_char_room(ch, arg2)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) && !IS_IMMORTAL(ch) )
    {
        send_to_char
            ("They don't seem to want it and you can't find anywhere "
             "to put it on them.", ch);
        return;
    }

    if ( count_carried(victim) >= MAX_NUMBER_CARRY - 1 )
    {
        send_to_char("They can't carry that many things.\r\n", ch);
        return;
    }

    obj_from_char(obj);
    obj_to_char(obj, victim);

    act("$n &uagives&n $p to $N.", ch, obj, victim, TO_NOTVICT);
    act("$n &uagives&n you $p.", ch, obj, victim, TO_VICT);
    act("You &uagive&n $p to $N.", ch, obj, victim, TO_CHAR);

    if ( (obj->timer > 0) && (obj->item_type == ITEM_EXPLOSIVE) )
    {
        send_to_char("Watch out!  It's LIVE!!", victim);
        send_to_char("A live one.", ch);
    }
}


/*
 * Remove an object.
 */
bool
remove_obj (struct char_data *ch, int iWear, bool fReplace, int wearing)
{
    OBJ_DATA *obj;

    if ( (obj = get_eq_char(ch, iWear)) == NULL )
        return TRUE;

    if ( !fReplace )
        return FALSE;

    if ( !wearing && count_carried(ch) >= MAX_NUMBER_CARRY - 1 )
    {
        send_to_char("You can't carry that many items.\r\n", ch);
        return FALSE;
    }

    unequip_char(ch, obj);
    act("$n &uastops using&n $p.", ch, obj, NULL, TO_ROOM);
    act("You &uastop using&n $p.", ch, obj, NULL, TO_CHAR);
    return TRUE;
}


/*
 * Wear one object on a single location.
 * This is a clean-up of the old wear_obj() fiasco.
 * -raj
 */
bool
wear_obj (struct char_data * ch, OBJ_DATA * obj, int loc, bool fReplace,
         bool silent, bool adj)
{
    int mapToWearFlag[MAX_WEAR] = {
        0,                      /* Remove this, later.  WEAR_LIGHT */
        ITEM_WEAR_FINGER,
        ITEM_WEAR_FINGER,
        ITEM_WEAR_NECK,
        ITEM_WEAR_NECK,
        ITEM_WEAR_BODY,
        ITEM_WEAR_HEAD,
        ITEM_WEAR_LEGS,
        ITEM_WEAR_FEET,
        ITEM_WEAR_HANDS,
        ITEM_WEAR_ARMS,
        ITEM_WEAR_SHIELD,
        ITEM_WEAR_ABOUT,
        ITEM_WEAR_WAIST,
        ITEM_WEAR_WRIST,
        ITEM_WEAR_WRIST,
        ITEM_WIELD,
        ITEM_WIELD,
        ITEM_HOLD
    };

    const char *wear_messages[MAX_WEAR][2] = {
        {"error", "error"},
        {"You &uawear&n $p on your left finger.",
         "$n &uawears&n $p on $s left finger."},
        {"You &uawear&n $p on your right finger.",
         "$n &uawears&n $p on $s left finger."},
        {"You &uawear&n $p on your neck.",
         "$n &uawears&n $p on $s neck."},
        {"You &uawear&n $p around your neck.",
         "$n &uawears&n $p around $s neck."},
        {"You &uawear&n $p on your body.",
         "$n &uawears&n $p on $s body."},
        {"You &uawear&n $p on your head.",
         "$n &uawears&n $p on $s head."},
        {"You &uawear&n $p on your legs.",
         "$n &uawears&n $p on $s legs."},
        {"You &uawear&n $p on your feet.",
         "$n &uawears&n $p on $s feet."},
        {"You &uawear&n $p on your hands.",
         "$n &uawears&n $p on $s hands."},
        {"You &uawear&n $p on your arms.",
         "$n &uawears&n $p on $s arms."},
        {"You &uawear&n $p as a shield.",
         "$n &uawears&n $p as a shield."},
        {"You &uawear&n $p about your body.",
         "$n &uawears&n $p about $s body."},
        {"You &uawear&n $p about your waist.",
         "$n &uawears&n $p about $s waist."},
        {"You &uawear&n $p on your left wrist.",
         "$n &uawears&n $p on $s left wrist."},
        {"You &uawear&n $p on your right wrist.",
         "$n &uawears&n $p on $s right wrist."},
        {"You &uawield&n $p &min your primary hand&n.",
         "$n &uawields&n $p &min $s primary hand&n."},
        {"You &uawield&n $p &min your secondary hand&n.",
         "$n &uawields&n $p &min $s secondary hand&n."},
        {"You &uahold&n $p.",
         "$n &uaholds&n $p."}
    };

    if ( loc < 0 || loc >= MAX_WEAR )
    {
        logmesg
            ("WARNING: Invalid wear location %d in wear_obj_on for '%s'.",
             loc, obj->name);
        send_to_char("Something just went fucky -- don't repeat that.\r\n",
                     ch);
        return (FALSE);
    }
    else if ( !CAN_WEAR(obj, mapToWearFlag[loc]) )
    {
        if ( !silent )
            act("You can't wear $p there...", ch, obj, NULL, TO_CHAR);

        return (FALSE);
    }

    /*
     * TODO: This is sort of fucky.  The promotion to a secondary location should
     * probably happen if the secondary space is used and the primary weapon can
     * not be removed.  Right now, we promote if the primary space is used and
     * then replace the secondary if necessary.  This is because we can't remove
     * the primary, here, for things like weapons, but can't promote later since
     * the weapons need to check the other hands.
     *
     * I'll figure something out eventually.  I'm tired right now and have the
     * feeling I'm overlooking some obvious solution, so I'll leave it be.
     *
     * -satori
     */
    if ( adj )
    {
        if ( loc == WEAR_FINGER_R || loc == WEAR_NECK_2 ||
             loc == WEAR_WRIST_R || loc == WEAR_SEC )
        {
            return (FALSE);
        }

        if ( (loc == WEAR_FINGER_L || loc == WEAR_NECK_1 ||
              loc == WEAR_WRIST_L || loc == WEAR_WIELD) &&
             get_eq_char(ch, loc) )
        {
            loc++;
        }
    }

    if ( loc == WEAR_WIELD || loc == WEAR_SEC || loc == WEAR_HOLD )
    {
        OBJ_DATA *sec =
            get_eq_char(ch, (loc == WEAR_WIELD ? WEAR_SEC : WEAR_WIELD));
        OBJ_DATA *hld =
            get_eq_char(ch, (loc == WEAR_HOLD ? WEAR_SEC : WEAR_HOLD));

        if ( (sec && hld) ||
             (hld && CAN_WEAR(hld, ITEM_TWO_HANDS)) ||
             (sec && CAN_WEAR(sec, ITEM_TWO_HANDS)) ||
             (loc == WEAR_HOLD && CAN_WEAR(obj, ITEM_TWO_HANDS)) ||
             (loc != WEAR_HOLD &&
              CAN_WEAR(obj, ITEM_TWO_HANDS) &&
              !remove_obj(ch, (loc == WEAR_WIELD
                                    ? WEAR_SEC : WEAR_WIELD), fReplace,
                               0)) )
        {
            if ( !silent )
                send_to_char("Your hands are full.\r\n", ch);

            return (FALSE);
        }

        /* Make sec the other object they're holding/wielding, if any. */
        sec = (sec ? sec : hld);

        if ( sec && !CAN_WEAR(obj, ITEM_SEC_HAND ) &&
            !CAN_WEAR(sec, ITEM_SEC_HAND))
        {
            if ( !silent )
                send_to_char("You can't use that as a secondary item.\r\n",
                             ch);

            return (FALSE);
        }
    }

    if ( !remove_obj(ch, loc, fReplace, 1) )
    {
        if ( !silent )
            send_to_char("You're already wearing something there.\r\n",
                         ch);

        return (FALSE);
    }

    act(wear_messages[loc][0], ch, obj, NULL, TO_CHAR);
    act(wear_messages[loc][1], ch, obj, NULL, TO_ROOM);
    equip_char(ch, obj, loc);
    return (TRUE);
}


void
do_wear (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char tmp[MAX_INPUT_LENGTH];
    struct obj_data *obj;
    bool found = FALSE;
    int num;
    int loc;
    int i;

    const char *wear_locations[MAX_WEAR] = {
        "light",
        "finger",
        "",
        "neck",
        "",
        "body",
        "head",
        "legs",
        "feet",
        "hands",
        "arms",
        "shield",
        "about",
        "waist",
        "wrist",
        "",
        "wield",
        "",
        "hold"
    };

    argument = one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char
            ("&WUSAGE&X: &cwear &X<&Ball&X|&Ball.&nobj&X|&n#&B.&nobj&X|&nobj&X> [&nlocation&X]&n\r\n",
             ch);
        return;
    }

    if ( !str_prefix(arg, "all") && (arg[3] == '\0' || arg[3] == '.') )
    {
        /* Parse for 'wear all' or 'wear all.keyword'. */

        for ( obj = ch->carrying; obj; obj = obj->next_content )
        {
            if ( obj->wear_loc != WEAR_NONE || !obj->wear_flags )
                continue;
            if ( arg[3] && !is_name_obj((arg + 4), obj->name) )
                continue;

            /*
             * This ugly little bit checks to see if it can be worn anywhere.
             * wear_obj() is called repeatedly, which is not the best approach,
             * but it handles mapping wear locations to flags and does the
             * appropriate thing if the object can be worn, so we call it once
             * for every object that can be worn.
             */

            for ( loc = 0; loc < MAX_WEAR; loc++ )
                if ( wear_obj(ch, obj, loc, FALSE, TRUE, TRUE) )
                {
                    found = TRUE;
                    break;
                }
        }

        if ( !found )
            send_to_char
                ("You don't have anything else you could possibly put on!\r\n",
                 ch);

        return;
    }

    num = number_argument(arg, tmp);
    one_argument(argument, arg);
    i = 0;

    for ( obj = ch->carrying; obj; obj = obj->next_content )
    {
        if ( obj->wear_loc != WEAR_NONE || !obj->wear_flags )
            continue;
        if ( is_name_obj(tmp, obj->name) && (++i >= num) )
            break;
    }

    if ( !obj )
    {
        send_to_char("You don't have anything wearable by that name.\r\n",
                     ch);
        return;
    }

    if ( *arg )
    {
        /* They provided a location to wear it on, so just try there. */
        for ( loc = 0; loc < MAX_WEAR; loc++ )
            if ( !str_prefix(arg, wear_locations[loc]) )
                break;

        if ( loc == MAX_WEAR )
        {
            send_to_char("That's not a valid wear location.\r\n", ch);
            return;
        }

        wear_obj(ch, obj, loc, TRUE, FALSE, TRUE);
    }
    else
    {
        /* They did not provide a location to wear it on, so try everything. */
        for ( loc = 0; loc < MAX_WEAR; loc++ )
            if (wear_obj
                (ch, obj, loc, TRUE, TRUE,
                 (loc != WEAR_WIELD && loc != WEAR_SEC)))
                break;
    }
}


void
do_primary (struct char_data *ch, char *argument)
{
    char line[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    struct obj_data *obj;
    int num;
    int i;

    one_argument(argument, line);
    num = number_argument(line, arg);
    i = 0;

    for ( obj = ch->carrying; obj; obj = obj->next_content )
    {
        if ( obj->wear_loc != WEAR_NONE || !CAN_WEAR(obj, ITEM_WIELD) )
            continue;

        if ( is_name_obj(arg, obj->name) && (++i >= num) )
        {
            wear_obj(ch, obj, WEAR_WIELD, TRUE, FALSE, FALSE);
            return;
        }
    }

    act("You don't have a weapon by the name of '$t'.", ch, line, NULL,
        TO_CHAR);
}


void
do_secondary (struct char_data *ch, char *argument)
{
    char line[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    struct obj_data *obj;
    int num;
    int i;

    one_argument(argument, line);
    num = number_argument(line, arg);
    i = 0;

    for ( obj = ch->carrying; obj; obj = obj->next_content )
    {
        if ( obj->wear_loc != WEAR_NONE || !CAN_WEAR(obj, ITEM_WIELD) )
            continue;

        if ( is_name_obj(arg, obj->name) && (++i >= num) )
        {
            wear_obj(ch, obj, WEAR_SEC, TRUE, FALSE, FALSE);
            return;
        }
    }

    act("You don't have a weapon by the name of '$t'.", ch, line, NULL,
        TO_CHAR);
}


void
do_wield (struct char_data *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char tmp[MAX_INPUT_LENGTH];
    OBJ_DATA *second = NULL;
    OBJ_DATA *wield = NULL;
    int num;
    int i;

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if ( !*arg1 )
    {
        send_to_char
            ("&WUSAGE&X: &cwield &X<&B_&X|&nprimary&X> &X[&B_&X|&nsecondary&X]&n\r\n",
             ch);
        return;
    }

    if ( str_cmp(arg1, "_") )
    {
        num = number_argument(arg1, tmp);

        for (i = 0, wield = ch->carrying; wield;
             wield = wield->next_content)
            if ( wield->wear_loc == WEAR_NONE && CAN_WEAR(wield, ITEM_WIELD )
                && is_name_obj(tmp, wield->name) && (++i) == num)
                break;

        if ( !wield )
        {
            act("You don't appear to have a weapon named '$t'.", ch, arg1,
                NULL, TO_CHAR);
            return;
        }
    }

    if ( *arg2 && str_cmp(arg2, "_") )
    {
        num = number_argument(arg2, tmp);

        for (i = 0, second = ch->carrying; second;
             second = second->next_content)
            if (second->wear_loc == WEAR_NONE &&
                CAN_WEAR(second, ITEM_WIELD) &&
                is_name_obj(tmp, second->name) && (++i) == num)
                break;

        if ( !second )
        {
            act("You don't appear to have a weapon named '$t'.", ch, arg2,
                NULL, TO_CHAR);
            return;
        }
    }

    if ( wield )
        wear_obj(ch, wield, WEAR_WIELD, TRUE, FALSE, FALSE);

    if ( second )
        wear_obj(ch, second, WEAR_SEC, TRUE, FALSE, FALSE);
}


void
donate (struct char_data *ch, OBJ_DATA * obj)
{
    if ( obj->timer > 0 )
    {
        send_to_char("You can't donate that.  It's live!\r\n", ch);
        return;
    }

    if ( !IS_SET(obj->wear_flags, ITEM_TAKE) )
    {
        send_to_char("You can't donate that!\r\n", ch);
        return;
    }

    if ( (ch->donate_num == 4) && (!IS_IMMORTAL(ch)) )
    {
        send_to_char("Your donation device needs to recharge first.\r\n",
                     ch);
        return;
    }

    act("$p flashes for a moment, then is gone.", ch, obj, NULL, TO_ROOM);
    act("$p flashes for a moment, then is gone.", ch, obj, NULL, TO_CHAR);

    if ( !obj->in_room )
    {
        send_to_char("This is a bug, please report.\r\n", ch);
        logmesg("donate: not in a room");
        bug_object_state(obj);
        return;
    }

    obj_from_room(obj);

    if ( team_table[ch->team].hq )
        obj_to_room(obj, team_table[ch->team].hq->interior);
    else
        obj_to_room(obj, index_room(the_city->rooms_on_level,
                                    the_city->x_size / 2,
                                    the_city->y_size / 2));

    if ( obj->in_room->people )
    {
        act("$p appears in a &Ybright&n flash!", obj->in_room->people, obj,
            NULL, TO_ROOM);
        act("$p appears in a &Ybright&n flash!", obj->in_room->people, obj,
            NULL, TO_CHAR);
    }

    ch->donate_num++;
}

void
do_donate (struct char_data *ch, char *argument)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj_next;
    bool found;
    int max_don = 0;

    strcpy(arg, argument);

    if ( arg[0] == '\0' )
    {
        send_to_char("What do you want to donate?\r\n", ch);
        return;
    }

    if ( ch->in_room == safe_area )
    {
        send_to_char("You cannot donate anything from the safe area.\r\n",
                     ch);
        return;
    }

    if ( str_cmp(arg, "all") && str_prefix("all.", arg) )
    {
        obj = get_obj_list(ch, arg, ch->in_room->contents);
        if ( obj == NULL )
        {
            send_to_char("You don't see that.\r\n", ch);
            return;
        }
        donate(ch, obj);
    }
    else
    {
        found = FALSE;
        for ( obj = ch->in_room->contents; (obj != NULL) && (max_don > 0 );
             obj = obj_next)
        {
            obj_next = obj->next_content;
            if ( (arg[3] == '\0') || is_name_obj(&arg[4], obj->name) )
            {
                found = TRUE;
                donate(ch, obj);
                max_don--;
            }
        }
        if ( !found )
            send_to_char("I don't see that here.\r\n", ch);
    }
}

void
do_remove (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    bool found;
    int x;

    strcpy(arg, argument);

    if ( arg[0] == '\0' )
    {
        send_to_char("Remove what?\r\n", ch);
        return;
    }

    if ( str_cmp(arg, "all") && str_prefix("all.", arg) )
    {
        if ( (obj = get_obj_wear(ch, arg)) == NULL )
        {
            send_to_char("You do not have that item.\r\n", ch);
            return;
        }
        remove_obj(ch, obj->wear_loc, TRUE, 0);
    }
    else
    {
        /* 'remove all' or 'remove all.obj' */
        found = FALSE;
        for ( x = 1; x < MAX_WEAR; x++ )
        {
            if ( (obj = get_eq_char(ch, x)) != NULL )
                if ( (arg[3] == '\0' || is_name_obj(arg + 4, obj->name)) )
                    if ( remove_obj(ch, x, TRUE, 0) )
                        found = TRUE;
        }

        if ( !found )
        {
            if ( arg[3] == '\0' )
                act("You are not wearing anything.", ch, NULL, arg,
                    TO_CHAR);
            else
                act("You are not wearing any '$T'.", ch, NULL, arg + 4,
                    TO_CHAR);
        }
    }

    return;
}

void
fire_extinguisher (ROOM_DATA * room, int recurs_depth)
{
    OBJ_DATA *obj, *obj_next;
    int dir;

    for ( obj = room->contents; obj != NULL; obj = obj_next )
    {
        obj_next = obj->next_content;

        if ( IS_SET(obj->general_flags, GEN_BURNS_ROOM) )
            extract_obj(obj, 1);
    }

    if ( room->people )
    {
        act("The room is temporarily filled with foam.", room->people,
            NULL, NULL, TO_CHAR);
        act("The room is temporarily filled with foam.", room->people,
            NULL, NULL, TO_ROOM);
    }

    if ( !recurs_depth )
        return;

    for ( dir = 0; dir < 4; dir++ )
        if ( !room->exit[dir] )
            fire_extinguisher(get_to_room(room, dir), recurs_depth - 1);
}


void
do_attach (struct char_data *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    struct obj_data *scope;
    struct obj_data *obj;
    struct obj_data *io;

    one_argument(one_argument(argument, arg1), arg2);

    if ( !*arg1 || !*arg2 )
    {
        send_to_char("Attach what to what?\r\n", ch);
        return;
    }
    else if ( !(scope = get_obj_carry(ch, arg1) ) ||
             scope->item_type != ITEM_SCOPE)
    {
        send_to_char("You don't have a scope by that name.\r\n", ch);
        return;
    }
    else if ( !(obj = get_obj_carry(ch, arg2) ) ||
             obj->item_type != ITEM_WEAPON)
    {
        send_to_char("You don't have a weapon by that name.\r\n", ch);
        return;
    }
    else if ( scope->scope_type != obj->scope_type )
    {
        send_to_char("There's nowhere to attach it at.\r\n", ch);
        return;
    }

    for ( io = obj->contains; io; io = io->next_content )
        if ( io->item_type == ITEM_SCOPE )
            break;

    if ( io )
    {
        send_to_char
            ("There's already something attached to it.  Use 'detach'.\r\n",
             ch);
        return;
    }

    obj_from_char(scope);
    obj_to_obj(scope, obj);
    act("You attach $p to $P.", ch, scope, obj, TO_CHAR);
    act("$n attaches $p to $P.", ch, scope, obj, TO_ROOM);
}


void
do_detach (struct char_data *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    struct obj_data *io = NULL;
    struct obj_data *from;
    bool fAll = FALSE;

    one_argument(one_argument(argument, arg1), arg2);

    if ( !*arg1 )
    {
        send_to_char("Detach what from what?\r\n", ch);
        return;
    }
    else if ( !*arg2 )
    {
        from = get_obj_carry(ch, arg1);

        if ( !from )
        {
            act("You don't appear to be carrying a '$t'.", ch, arg1, NULL,
                TO_CHAR);
            return;
        }

        io = from->contains;
    }
    else
    {
        from = get_obj_carry(ch, arg2);

        if ( !from )
        {
            act("You don't appear to be carrying a '$t'.", ch, arg2, NULL,
                TO_CHAR);
            return;
        }

        if ( !str_cmp(arg1, "all") )
        {
            int carrying = count_carried(ch);
            bool fDetached = FALSE;

            fAll = TRUE;

            while ( (io = from->contains) != NULL )
            {
                if ( io->item_type == ITEM_AMMO )
                    continue;

                if ( carrying >= MAX_NUMBER_CARRY - 1 && !ch->trust )
                {
                    send_to_char
                        ("You didn't have room enough to detach everything.\r\n",
                         ch);
                    return;
                }

                obj_from_obj(io);
                obj_to_char(io, ch);
                act("You detach $p from $P.", ch, io, from, TO_CHAR);
                act("$n detaches $p from $P.", ch, io, from, TO_ROOM);
                fDetached = TRUE;
                carrying++;
            }

            if ( !fDetached )
            {
                send_to_char("There wasn't anything attached to it.\r\n",
                             ch);
                return;
            }
        }
        else
            io = get_obj_list(ch, arg1, from->contains);
    }

    if ( !io )
    {
        act("There doesn't appear to be a $T attached to $p.", ch, from,
            (*arg2 ? arg1 : "thing"), TO_CHAR);
        return;
    }
    else if ( io->item_type == ITEM_AMMO )
    {
        send_to_char("Use 'unload' to remove ammo from the weapon.\r\n",
                     ch);
        return;
    }
    else if ( count_carried(ch) >= MAX_NUMBER_CARRY - 1 && !ch->trust )
    {
        send_to_char("You don't have enough room to detach that.\r\n", ch);
        return;
    }

    obj_from_obj(io);
    obj_to_char(io, ch);

    act("You detach $p from $P.", ch, io, from, TO_CHAR);
    act("$n detaches $p from $P.", ch, io, from, TO_ROOM);
}


void
do_depress (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj;
    char buf[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;

    for ( obj = ch->in_room->contents; obj; obj = obj->next_content )
        if ( IS_SET(obj->general_flags, GEN_DEPRESS) )
            break;

    if ( !obj && ch != enforcer )
    {
        send_to_char("You don't see that in this room.", ch);
        return;
    }

    one_argument(argument, arg);
    if ( buttontimer >= 0 )
    {
        if ( buttonpresser == ch )
        {
            send_to_char
                ("You just started it, why on earth would you wanna stop it?\r\n",
                 ch);
            return;
        }
        if (buttonpresser->team == ch->team &&
            !team_table[ch->team].independent && ch != enforcer)
        {
            sprintf(buf, "%s%s&n already depressed for your team.\r\n",
                    team_table[ch->team].namecolor, buttonpresser->names);
            send_to_char(buf, ch);
            return;
        }
        sprintf(buf,
                "%s has stopped the button countdown at %d second%s!\r\n",
                ch->names, buttontimer, (buttontimer == 1 ? "" : "s"));
        for ( d = descriptor_list; d; d = d->next )
        {
            if ( d->connected == CON_PLAYING )
            {
                send_to_char(buf, d->character);
            }
        }
        buttontimer = -1;
        buttonpresser = NULL;
    }
    else
    {
        if ( ch != enforcer )
        {
            for ( struct char_data * ech = char_list; ech; ech = ech->next )
            {
                if ( ech->npc && ech->npc->depress.head )
                {
                    struct cmd_node *node = ech->npc->depress.head;
                    char *hold;

                    while ( node != NULL )
                    {
                        hold = npc_parse(node->cmd, ech, ch, obj, NULL);
                        if ( !hold )
                            continue;
                        interpret(ech, hold);
                        node = node->next;
                    }
                }
            }
        }

        buttonpresser = ch;
        sprintf(buf, "Oh no! %s has depressed the button!\r\n", ch->names);
        for ( d = descriptor_list; d; d = d->next )
        {
            if ( d->connected == CON_PLAYING )
            {
                send_to_char(buf, d->character);
            }
        }
        buttontimer = 300;
    }
}


void
do_compare (CHAR_DATA * ch, char *argument)
{
    char this_arg[MAX_INPUT_LENGTH];
    char that_arg[MAX_INPUT_LENGTH];
    struct obj_data *this;
    struct obj_data *that;
    int diff;

    argument = one_argument(argument, this_arg);
    one_argument(argument, that_arg);

    if ( !*this_arg && !*that_arg )
    {
        send_to_char("Usage&X: &ccompare &X<&nthis&X> <&nthat&X>&n\r\n",
                     ch);
        return;
    }
    else if ( !(this = get_obj_carry(ch, this_arg)) )
    {
        printf_to_char(ch, "You don't have a '%s', sparky.\r\n", this_arg);
        return;
    }
    else if ( !(that = get_obj_carry(ch, that_arg)) )
    {
        printf_to_char(ch, "You don't have a '%s', sparky.\r\n", that_arg);
        return;
    }
    else if (this->item_type != ITEM_ARMOR ||
             that->item_type != ITEM_ARMOR)
    {
        send_to_char("You can only compare armor.\r\n", ch);
        return;
    }

    diff = this->armor - that->armor;

    if ( !diff )
    {
        send_to_char("They provide equal damage protection.\r\n", ch);
        return;
    }

    sprintf(this_arg, "%s provides %d more abs than %s.",
            (diff < 0 ? "$P" : "$p"), ABS(diff), (diff < 0 ? "$p" : "$P"));
    act(this_arg, ch, this, that, TO_CHAR);
}


//useage: combine (obj1) (obj2) (obj3)
void
do_combine (CHAR_DATA * ch, char *argument)
{
//lookup the objects, make sure the user has all of them in his inventory
//lookup the objects in a table, extract and create the third object

}
