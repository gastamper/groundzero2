/*
 * GroundZero2 is based upon code from GroundZero by Corey Hilke, et al.
 * GroundZero was based upon code from ROM by Russ Taylor, et al.
 * ROM was based upon code from Merc by Hatchett, Furey, and Kahn.
 * Merc was based upon code from DIKUMUD.
 *
 * DikuMUD was written by Katja Nyboe, Tom Madsen, Hans Henrik Staerfeldt,
s * Michael Seifert, and Sebastian Hammer.
 *
 */

/**
 * NAME:        act_combat.c
 * SYNOPSIS:    Commands for fighting, etc.
 * AUTHOR:      satori
 * CREATED:     18 Feb 2001
 * MODIFIED:    18 Feb 2001
 */

#include "ground0.h"


void
do_bury (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *dirname = NULL;
    OBJ_DATA *obj;
    int dir = -1;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if ( !*arg1 )
    {
        for ( obj = ch->in_room->contents; obj; obj = obj->next_content )
            if ( IS_SET(obj->general_flags, GEN_CAN_BURY) )
                break;

        if ( !obj )
            for ( obj = ch->carrying; obj; obj = obj->next_content )
                if (obj->wear_loc == WEAR_NONE &&
                    IS_SET(obj->general_flags, GEN_CAN_BURY))
                    break;
    }
    else if ( !*arg2 )
    {
        /* Can be either "bury <dir>" or "bury <mine>", figure out which. */
        if ( !(obj = get_obj_here(ch, arg1)) )
        {
            for ( obj = ch->in_room->contents; obj; obj = obj->next_content )
                if ( IS_SET(obj->general_flags, GEN_CAN_BURY ) &&
                    IS_SET(obj->general_flags, GEN_DIRECTIONAL))
                    break;

            if ( !obj )
            {
                for ( obj = ch->carrying; obj; obj = obj->next_content )
                    if (obj->wear_loc == WEAR_NONE &&
                        IS_SET(obj->general_flags, GEN_CAN_BURY) &&
                        IS_SET(obj->general_flags, GEN_DIRECTIONAL))
                        break;

                dirname = arg1;
                for ( dir = 0; dir <= DIR_WEST; dir++ )
                    if ( !str_prefix(arg1, dir_name[dir]) )
                        break;
            }
        }
    }
    else if ( (obj = get_obj_here(ch, arg1)) )
        dirname = arg2;

    if ( !obj )
    {
        send_to_char("You don't see that here.\r\n", ch);
        return;
    }
    else if ( !IS_SET(obj->general_flags, GEN_CAN_BURY) )
    {
        send_to_char("You can't bury that.\r\n", ch);
        return;
    }
    else if ( obj->timer > 0 )
    {
        send_to_char("You cannot bury a live object.\r\n", ch);
        return;
    }
    else if ( IS_SET(obj->general_flags, GEN_DIRECTIONAL) )
    {
        if ( !dirname )
        {
            send_to_char
                ("You need to specify what direction to bury that in.\r\n",
                 ch);
            return;
        }

        for ( dir = 0; dir <= DIR_WEST; dir++ )
            if ( !str_prefix(dirname, dir_name[dir]) )
                break;

        if ( dir > DIR_WEST )
        {
            send_to_char("You can't bury a mine in that direction.\r\n",
                         ch);
            return;
        }
        else if ( ch->in_room->exit[dir] )
        {
            send_to_char("There's a wall there.\r\n", ch);
            return;
        }

        obj->orientation = dir;
    }

    if ( ch->in_room->mine )
    {
        send_to_char("Oops.  Someone beat you to it!\n\r", ch);
        extract_obj(obj, obj->extract_me);
        obj = ch->in_room->mine;

        obj->destination = NULL;
        ch->in_room->mine = NULL;
        obj_to_char(obj, ch);
        bang_obj(obj, 0);
        return;
    }

    if ( IS_SET(obj->general_flags, GEN_DIRECTIONAL) )
    {
        act("$n buries $p to the $T.", ch, obj, dir_name[dir], TO_ROOM);
        act("You bury $p to the $T.", ch, obj, dir_name[dir], TO_CHAR);
    }
    else
    {
        act("$n buries $p.", ch, obj, NULL, TO_ROOM);
        act("You bury $p.", ch, obj, NULL, TO_CHAR);
    }

    if ( obj->in_room )
        obj_from_room(obj);
    else if ( obj->carried_by )
        obj_from_char(obj);
    else
        logmesg("do_bury: can't figure out where target object is.");

    obj->destination = ch->in_room;
    ch->in_room->mine = obj;
    claimObject(ch, obj);

    /* Annnouncing it to teammates if not independent */
    if ( !team_table[ch->team].independent )
    {
        if ( ch->in_room->interior_of )
        {
            OBJ_DATA *interior = ch->in_room->interior_of;

            sprintf(buf,
                    "%s %s&n: 'Buried &B%s&n inside of &B%s&n at [&uX%d&n, &uX%d&n] L - &uL%d&n'\r\n",
                    ch->names, team_table[ch->team].who_name,
                    obj->short_descr, interior->short_descr,
                    interior->in_room->x, interior->in_room->y,
                    interior->in_room->level);
        }
        else
        {
            sprintf(buf,
                    "%s %s&n: 'Buried &B%s&n at [&uX%d&n, &uX%d&n] L - &uL%d&n'\r\n",
                    ch->names, team_table[ch->team].who_name,
                    obj->short_descr, ch->in_room->x, ch->in_room->y,
                    ch->in_room->level);
        }

        send_to_team(buf, ch->team);
    }
}


void
do_kill (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *obj;

    if ( ch->in_room == safe_area )
    {
        send_to_char("You cannot attack others in safe areas.\r\n", ch);
        return;
    }
    else if ( ch->in_room == graveyard_area )
    {
        send_to_char("You cannot attack zombies.\r\n", ch);
        return;
    }
    else if ( ch->in_room->inside_mob )
    {
        send_to_char
            ("There is not enough space in here to be violent.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if ( !arg[0] )
    {
        send_to_char("You will now not attack anyone unless they attack "
                     "you.\n\r", ch);
        if ( IS_SET(ch->act, PLR_AGGRO_OTHER) )
            REMOVE_BIT(ch->act, PLR_AGGRO_OTHER);
        ch->fighting = NULL;
        return;
    }

    if ( !str_prefix(arg, "all") )
    {
        send_to_char("You will now attack anything that isn't on your team.\r\n", ch);
        SET_BIT(ch->act, PLR_AGGRO_OTHER);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("That doesn't exist.\r\n", ch);
        return;
    }
    else if ( victim == ch )
    {
        send_to_char("I'm sure you can find a more creative way to kill "
                     "yourself.\r\n", ch);
        return;
    }
    else if ( ch->fighting == victim )
    {
        send_to_char("You're aleady attacking them.\r\n", ch);
        return;
    }
    else if ( !team_table[ch->team].independent && ch->team == victim->team )
    {
        if ( !g_forceAction )
        {
            act("But $E's a teammate!  Use &WKILL $t!&n if you really want to kill them.", ch, arg, victim, TO_CHAR);
            return;
        }

        send_to_char("Well, okay...\r\n", ch);
    }

    ch->fighting = victim;
    obj = NULL;

    if ( can_see_linear_char(ch, victim, 1, 0) )
    {
        obj = get_eq_char(ch, WEAR_WIELD);
        if ( !shoot_char(ch, victim, obj, 1) )
            act("You will shoot $M as soon as you have $t working properly.", ch, (obj ? obj->short_descr : "your weapon"), victim, TO_CHAR);
    }

    if ( victim && !test_char_alive(victim ) &&
        can_see_linear_char(ch, victim, 1, 1))
    {
        obj = get_eq_char(ch, WEAR_SEC);
        if ( !shoot_char(ch, victim, obj, 1) )
            act("You will shoot $M as soon as you have $t working properly.", ch, (obj ? obj->short_descr : "your weapon"), victim, TO_CHAR);
    }

    if ( !obj )
        act("You will now attack $N (and only $M!) on sight.", ch, NULL,
            victim, TO_CHAR);

    if ( IS_SET(ch->act, PLR_AGGRO_OTHER) )
    {
        send_to_char("Kill all removed.\n\r", ch);
        REMOVE_BIT(ch->act, PLR_AGGRO_OTHER);
    }
}


void
pull_obj (OBJ_DATA * obj, CHAR_DATA * ch, int silent, char *rest)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    if ( !obj->extract_flags )
    {
        if ( !silent )
            send_to_char("There is no pin on that.\n\r", ch);
        return;
    }

    if ( obj->timer > 0 )
    {
        if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) )
        {
            if ( !silent )
                send_to_char("It has already been lit.\n\r", ch);
            return;
        }
        else
        {
            if ( !silent )
                send_to_char("It has already been pulled.\n\r", ch);
            return;
        }
    }

    one_argument(rest, arg);

    if ( IS_SET(obj->general_flags, GEN_TIMER_ITEM) && isdigit(*arg) )
    {
        int t = atoi(arg);

        if ( t < 5 || t > 60 )
        {
            if ( !silent )
                send_to_char
                    ("The timer has a range of 6 to 60 seconds.\r\n", ch);
            return;
        }

        obj->timer = t;

        if ( !silent )
        {
            sprintf(buf,
                    "You program &uOthe timer&n on $p to detonate in %d seconds...",
                    obj->timer);
            act(buf, ch, obj, NULL, TO_CHAR);
        }

        act("$n presses some buttons on the face of $p.", ch, obj, NULL,
            TO_ROOM);
    }
    else
    {
        if ( IS_SET(obj->general_flags, GEN_TIMER_ITEM) && !silent )
        {
            sprintf(buf,
                    "You restart &uOthe timer&n on $p at %d seconds...",
                    obj->cooked_timer);
            act(buf, ch, obj, NULL, TO_CHAR);
        }

        obj->timer = obj->cooked_timer;
    }

    claimObject(ch, obj);

    if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) )
    {
        if ( !silent )
            act("You &ualight&n &uOthe rag&n on " "from $p.\n\rBetter get rid of that pal.  &YLooks to be " "&Rburning&n pretty fast there!&n", ch, obj, NULL, TO_CHAR);
        act("$n &ualights&n &uOthe rag&n on $p.",
            ch, obj, NULL, TO_ROOM);
    }
    else
    {
        if ( !silent )
            act("You &uapull&n &uOthe pin&n from $p.\n\rBetter get rid of that " "pal.  &YYou've only got a few seconds!&n", ch, obj, NULL, TO_CHAR);
        act("$n &uapulls&n &uOthe pin&n from $p.", ch, obj, NULL, TO_ROOM);
    }
}


void
do_pull (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj;
    char arg[MAX_INPUT_LENGTH];
    bool found = FALSE;

    if ( ch->in_room == safe_area )
    {
        send_to_char("You cannot engage in combat in safe rooms.\r\n", ch);
        return;
    }
    else if ( ch->in_room == graveyard_area )
    {
        send_to_char("You cannot kill zombies.\r\n", ch);
        return;
    }

    if ( ch->in_room->inside_mob )
    {
        send_to_char
            ("There is not enough space in here to be violent.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if ( !str_cmp(arg, "all") )
    {
        for ( obj = ch->carrying; obj; obj = obj->next_content )
        {
            if ( obj->wear_loc == WEAR_NONE )
            {
                pull_obj(obj, ch, 1, argument);
                found = TRUE;
            }
        }
    }
    else if ( !str_prefix("all.", arg) )
    {
        for ( obj = ch->carrying; obj; obj = obj->next_content )
        {
            if (obj->wear_loc == WEAR_NONE &&
                is_name_obj(arg + 4, obj->name))
            {
                pull_obj(obj, ch, 1, argument);
                found = TRUE;
            }
        }
    }
    else if ( (obj = get_obj_carry(ch, arg)) )
    {
        pull_obj(obj, ch, 0, argument);
        found = TRUE;
    }

    if ( !found )
    {
        send_to_char("Pull what?!\r\n", ch);
        return;
    }

}


void
do_unpull (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Unpull what?\r\n", ch);
        return;
    }
    else if ( !(obj = get_obj_carry(ch, arg)) )
    {
        send_to_char("You don't have that.\r\n", ch);
        return;
    }
    else if ( obj->owner != ch && !IS_IMMORTAL(ch) )
    {
        send_to_char("You didn't pull it...\r\n", ch);
        return;
    }
    else if ( obj->timer <= 0 )
    {
        act("$p&n isn't live.\r\n", ch, obj, NULL, TO_CHAR);
        return;
    }
    else if ( obj->timer <= 3 )
    {
        send_to_char("It's too late to turn back now.  Get rid of it!\r\n",
                     ch);
        return;
    }
    else if ( !number_range(0, 29) )
    {
        if ( !number_range(0, 199) )
        {
            if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) )
            {
                act("Ow!  You burn yourself on &uOthe rag&n and fumble $p to the ground.", ch, obj, NULL, TO_CHAR);
                act("$n yelps in pain as he burns himself on &uOthe rag&n of $p\r\n" "and drops it.", ch, obj, NULL, TO_ROOM);
            }
            else if ( IS_SET(obj->general_flags, GEN_TIMER_ITEM) )
            {
                act("Oops... you yanked the &uOdetonator wire&n on $p.  Wonder what that'll do...", ch, obj, NULL, TO_CHAR);
                act("$n fumbles with $p and yanks out a wire.  That doesn't look good.", ch, obj, NULL, TO_ROOM);
            }
            else
            {
                act("You fumble with &uOthe pin&n, wedging it hard inside $p.\r\n" "You've seemed to have made a critical mistake...", ch, obj, NULL, TO_CHAR);
                act("$n fumbles with &uOthe pin&n of $p.", ch, obj, NULL, TO_ROOM);
            }

            bang_obj(obj, obj->extract_me);
            return;
        }

        send_to_char
            ("Best to either get rid of it or try it again.\r\n",
             ch);
        return;
    }

    if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) )
    {
        act("You extinguish &uOthe rag&n on $p.", ch, obj, NULL,
            TO_CHAR);
        act("$n extinguishes &uOthe rag&n on $p.", ch, obj, NULL,
            TO_ROOM);
    }
    else if ( IS_SET(obj->general_flags, GEN_TIMER_ITEM) )
    {
        act("You de-activate &uOthe timer&n on $p.", ch, obj,
            NULL, TO_CHAR);
        act("$n de-actives &uOthe timer&n on $p.", ch, obj, NULL,
            TO_ROOM);
    }
    else
    {
        act("You slip &uOthe pin&n back into $p.", ch, obj,
            NULL, TO_CHAR);
        act("$n slips &uOthe pin&n back into $p.", ch, obj, NULL,
            TO_ROOM);
    }

    disownObject(obj);
    obj->cooked_timer = obj->timer;
    obj->timer = -1;
}


void
do_explode (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj, *next_obj;
    int deaths = ch->deaths;

    if ( IS_NPC(ch) )
        return;

    if ( argument )
    {
        if ( RANK(ch) < RANK_BADASS )
        {
            send_to_char("You aren't high enough to do this.\r\n", ch);
            return;
        }
    }

    if ( ch->in_room == safe_area )
    {
        send_to_char("You cannot engage in combat in safe rooms.", ch);
        return;
    }
    else if ( ch->in_room == graveyard_area )
    {
        send_to_char("Not here.\r\n", ch);
        return;
    }

    if ( ch->in_room->inside_mob )
    {
        send_to_char
            ("There is not enough space in here to be violent.\n\r", ch);
        return;
    }

    if ( ch->hit < ch->max_hit && ch->trust == 0 )
    {
        send_to_char
            ("You must be at full health to use this command.\n\r", ch);
        return;
    }

    act("$n activates a small device that causes every item in his inventory " "to &REXPLODE&n!", ch, NULL, NULL, TO_ROOM);
    send_to_char
        ("You activate your self destruct device, setting off every "
         "explosive item in your inventory!\n\r", ch);

    for ( obj = ch->carrying; obj; obj = next_obj )
    {
        next_obj = obj->next_content;
        if ( (obj->item_type == ITEM_EXPLOSIVE ) ||
            (obj->item_type == ITEM_AMMO))
        {
            claimObject(ch, obj);
            bang_obj(obj, 0);

            if ( !ch || deaths != ch->deaths )
                break;
        }
    }

    if ( ch && deaths != ch->deaths )
        save_char_obj(ch);
}


void
do_boom (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];
    int deaths;

    if ( IS_NPC(ch) )
        return;

    if ( RANK(ch) < RANK_MERC )
    {
        send_to_char("You are not high enough to do this.\r\n", ch);
        return;
    }

    if ( ch->in_room == safe_area )
    {
        send_to_char("You cannot engage in combat in safe rooms.", ch);
        return;
    }
    else if ( ch->in_room == graveyard_area )
    {
        send_to_char("No.\r\n", ch);
        return;
    }

    if ( ch->in_room->inside_mob )
    {
        send_to_char
            ("There is not enough space in here to be violent.\n\r", ch);
        return;
    }
    else if ( !IS_IMMORTAL(ch) )
    {
        if ( buttontimer >= 0 )
        {
            send_to_char
                ("The button must be pressed to do this.\r\n", ch);
            return;
        }
        else if ( ch->hit < ch->max_hit )
        {
            send_to_char
                ("You must be at full health to use this command.\r\n", ch);
            return;
        }
    }

    act("$n has self destructed.", ch, NULL, NULL, TO_ROOM);
    sprintf(buf, "%s has boomed out.", ch->names);
    logmesg(buf);

    send_to_char("You activate your self-destruct device...\r\n",ch);

    if ( ch->desc )
        close_socket(ch->desc);

    deaths = ch->deaths;
    do_explode(ch, NULL);

    if ( deaths == ch->deaths )
    {
        ch->last_hit_by = ch;
        char_death(ch, "self-destruct");
        save_char_obj(ch);
    }
}

void
do_fire (struct char_data *ch, char *argument)
{
    void vehicle_fire (struct char_data *, char *);

    if ( IS_SET(ch->temp_flags, IN_VEHICLE | MAN_TURRET | MAN_HUD) )
    {
        vehicle_fire(ch, argument);
        return;
    }

    send_to_char("You're not in a vehicle.\r\n", ch);
}
