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
#include    "memory.h"
#include    <dirent.h>
#include    <sys/stat.h>

/* command procedures needed */
DECLARE_DO_FUN (do_mstat);
DECLARE_DO_FUN (do_ostat);
DECLARE_DO_FUN (do_rset);
DECLARE_DO_FUN (do_mset);
DECLARE_DO_FUN (do_oset);
DECLARE_DO_FUN (do_sset);
DECLARE_DO_FUN (do_tset);
DECLARE_DO_FUN (do_enfset);
DECLARE_DO_FUN (do_force);
DECLARE_DO_FUN (do_save);
DECLARE_DO_FUN (do_look);
DECLARE_DO_FUN (do_oload);
DECLARE_DO_FUN (do_ofind);
DECLARE_DO_FUN (do_help);
DECLARE_DO_FUN (do_statfreeze);

/* External functions. */
CHAR_DATA *char_file_active(char *);
void stop_manning(CHAR_DATA *);
int mult_argument(char *, char *);
void scatter_objects(void);
void base_purge(void);
void tick_stuff(int);

/* Local functions. */
ROOM_DATA *find_location(CHAR_DATA *, char *);

/* Global (Exported) Variables. */
DESCRIPTOR_DATA *pinger_list = NULL;

extern const char *ttypes[];


void
do_repeat (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int i;

    argument = one_argument(argument, arg);

    if ( !isdigit(*arg) )
    {
        send_to_char("Usage: *<times> <command>\r\n", ch);
        return;
    }

    i = atoi(arg);
    while ( i-- > 0 )
        interpret(ch, argument);
}


void
do_wiznet (CHAR_DATA * ch, char *argument)
{
    int flag;
    char buf[MAX_STRING_LENGTH];

    if ( argument[0] == '\0' )
    {
	send_to_char("Usage: wiznet <on|off|status|show|channel from list>\r\n", ch);
	return;
    }

    if ( !str_cmp(argument, "on") 
		&& !IS_SET(ch->wiznet, WIZ_ON) )
    {
        send_to_char("Welcome to Wiznet!\r\n", ch);
        SET_BIT(ch->wiznet, WIZ_ON);
        return;
    }
    else if ( !str_cmp(argument, "on") 
		&& IS_SET(ch->wiznet, WIZ_ON) )
	{
	send_to_char("Wiznet already on.\r\n", ch);
	return;
	}

    if ( !str_cmp(argument, "off") 
		&& IS_SET(ch->wiznet, WIZ_ON) )
    {
        send_to_char("Signing off of Wiznet.\r\n", ch);
        REMOVE_BIT(ch->wiznet, WIZ_ON);
        return;
    }
    else if ( !str_cmp(argument, "off") && 
		!IS_SET(ch->wiznet, WIZ_ON) )
    {
	send_to_char("Wiznet already off.\r\n", ch);
	return;
    }

    /* show wiznet status */
    if ( !str_prefix(argument, "status") )
    {
        buf[0] = '\0';

        if ( !IS_SET(ch->wiznet, WIZ_ON) )
            strcat(buf, "off ");

        for ( flag = 0; wiznet_table[flag].name != NULL; flag++ )
            if ( IS_SET(ch->wiznet, wiznet_table[flag].flag) )
            {
                strcat(buf, wiznet_table[flag].name);
                strcat(buf, " ");
            }

        strcat(buf, "\r\n");

        send_to_char("Wiznet status:\r\n", ch);
        send_to_char(buf, ch);
        return;
    }

    if ( !str_prefix(argument, "show") )
        /* list of all wiznet options */
    {
        buf[0] = '\0';

        for ( flag = 0; wiznet_table[flag].name != NULL; flag++ )
        {
            if ( wiznet_table[flag].level <= get_trust(ch) )
            {
                strcat(buf, wiznet_table[flag].name);
                strcat(buf, " ");
            }
        }

        strcat(buf, "\r\n");

        send_to_char("Wiznet options available to you are:\r\n", ch);
        send_to_char(buf, ch);
        return;
    }

    flag = wiznet_lookup(argument);

    if ( flag == -1 || get_trust(ch) < wiznet_table[flag].level )
    {
        send_to_char("No such option.\r\n", ch);
        return;
    }

    if ( IS_SET(ch->wiznet, wiznet_table[flag].flag) )
    {
        sprintf(buf, "You will no longer see %s on wiznet.\r\n",
                wiznet_table[flag].name);
        send_to_char(buf, ch);
        REMOVE_BIT(ch->wiznet, wiznet_table[flag].flag);
        return;
    }
    else
    {
        sprintf(buf, "You will now see %s on wiznet.\r\n",
                wiznet_table[flag].name);
        send_to_char(buf, ch);
        SET_BIT(ch->wiznet, wiznet_table[flag].flag);
        return;
    }

}

void
wiznet (char *string, CHAR_DATA * ch, OBJ_DATA * obj, long flag,
       long flag_skip, int min_level)
{
    DESCRIPTOR_DATA *d;

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
        if ( d->connected == CON_PLAYING && IS_IMMORTAL(d->character )
            && IS_SET(d->character->wiznet, WIZ_ON)
            && (!flag || IS_SET(d->character->wiznet, flag))
            && (!flag_skip || !IS_SET(d->character->wiznet, flag_skip))
            && get_trust(d->character) >= min_level && d->character != ch)
        {
            if ( IS_SET(d->character->wiznet, WIZ_PREFIX) )
                send_to_char("&X-&n-&W>&r ", d->character);
            act_new(string, d->character, obj, ch, TO_CHAR, POS_DEAD);
            if ( IS_SET(d->character->wiznet, WIZ_PREFIX) )
                send_to_char("&n", d->character);
        }
    }

    return;
}

/* equips a character */
void
do_outfit (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    bool found = FALSE;
    CHAR_DATA *targ;
    OBJ_DATA *obj;
    sh_int count;
    int i;

    extern int g_numOutfits;
    extern OUTFIT_DATA *g_outfitTable;

    one_argument(argument, arg);

    if ( !*arg )
    {
	send_to_char("You must specify a player to provide items to.\r\n", ch);
	return;
    }
    // The game actually calls do_outfit on any character that logs in, so
    // it's easiest to juts pass a wonky argument than code around the exception.
    else if ( !strcmp(arg, "equip_on_login") )
    {
        targ = ch;
    }
    else
    {
        if ( !(targ = get_char_world(ch, arg)) )
        {
            send_to_char("I see no-one by that name.\r\n", ch);
            return;
        }

        act("$n equips you.", ch, NULL, targ, TO_VICT);
        act("You equip $N.", ch, NULL, targ, TO_CHAR);
    }

    /* First search for team-specific and leader-specific stuff. */
    for ( i = 0; i < g_numOutfits; i++ )
    {
        if ( g_outfitTable[i].team != targ->team )
            continue;
        if (g_outfitTable[i].leader_only &&
            team_table[targ->team].teamleader != targ)
            continue;

        if ( (pObjIndex = get_obj_index(g_outfitTable[i].vnum)) == NULL )
        {
            logmesg("Couldn't outfit character with object #%d",
                       g_outfitTable[i].vnum);
            continue;
        }

        found = TRUE;

        for ( count = 0; count < g_outfitTable[i].quantity; count++ )
        {
            obj = create_object(pObjIndex, 0);
            obj->extract_me = 1;
            obj_to_char(obj, targ);
        }
    }

    /* Didn't find any team-specific outfits or is on an indy team. */
    if ( !found )
    {
        for ( i = 0; i < g_numOutfits; i++ )
        {
            if ( g_outfitTable[i].team != -1 )
                continue;
            if (g_outfitTable[i].leader_only &&
                team_table[targ->team].teamleader != targ)
                continue;
            if ( g_outfitTable[i].quantity < 1 )
                continue;

            if ( (pObjIndex = get_obj_index(g_outfitTable[i].vnum)) == NULL )
            {
                logmesg("Couldn't outfit character with object #%d",
                           g_outfitTable[i].vnum);
                continue;
            }

            for ( count = 0; count < g_outfitTable[i].quantity; count++ )
            {
                obj = create_object(pObjIndex, 0);
                obj->extract_me = 1;
                obj_to_char(obj, targ);
            }
        }
    }
}


/* RT nochannels command, for those spammers */
void
do_nochannels (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Nochannel whom?", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You cannot do that to higher level characters.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->comm, COMM_NOCHANNELS) )
    {
        REMOVE_BIT(victim->comm, COMM_NOCHANNELS);
        send_to_char("The gods have restored your channel priviliges.\r\n",
                     victim);
        send_to_char("NOCHANNELS removed.\r\n", ch);
    }
    else
    {
        SET_BIT(victim->comm, COMM_NOCHANNELS);
        send_to_char("The gods have revoked your channel priviliges.\r\n",
                     victim);
        send_to_char("NOCHANNELS set.\r\n", ch);
    }

    return;
}

void
do_bamfin (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if ( !IS_NPC(ch) )
    {
        smash_tilde(argument);

        if ( argument[0] == '\0' )
        {
            sprintf(buf, "Your poofin is %s\r\n", ch->pcdata->poofin_msg);
            send_to_char(buf, ch);
            return;
        }

        free_string(ch->pcdata->poofin_msg);
        ch->pcdata->poofin_msg = str_dup(argument);

        sprintf(buf, "Your poofin is now %s\r\n", ch->pcdata->poofin_msg);
        send_to_char(buf, ch);
    }
    return;
}



void
do_bamfout (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if ( !IS_NPC(ch) )
    {
        smash_tilde(argument);

        if ( argument[0] == '\0' )
        {
            sprintf(buf, "Your poofout is %s\r\n",
                    ch->pcdata->poofout_msg);
            send_to_char(buf, ch);
            return;
        }

        free_string(ch->pcdata->poofout_msg);
        ch->pcdata->poofout_msg = str_dup(argument);

        sprintf(buf, "Your poofout is now %s\r\n",
                ch->pcdata->poofout_msg);
        send_to_char(buf, ch);
    }
    return;
}

void
do_bringon (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
    CHAR_DATA *the_summonned;

    one_argument(argument, arg);

    if (!*arg)
    {
        send_to_char("Specify a character to spawn.\r\n", ch);
        return;
    }
    if ( char_file_active(arg) )
    {
        send_to_char("That character is in the game at the moment.\r\n", ch);
        return;
    }

    if ( (the_summonned = load_char_obj(NULL, arg)) == NULL )
    {
        send_to_char("That character is not in the player files.\r\n", ch);
        return;
    }
    the_summonned->next = char_list;
    char_list = the_summonned;
    char_to_room(the_summonned, ch->in_room);

    do_outfit(the_summonned, "");

    if ( the_summonned->trust != 10 )
    {
        the_summonned->max_hit = HIT_POINTS_MORTAL;
        the_summonned->hit = HIT_POINTS_MORTAL;
        /* give them trust so they are not transed away from the god that
           summonned them; when the character connects, the trust will be gone;
           see comm.c */
        the_summonned->trust = 1;
    }
    else
    {
        the_summonned->max_hit = HIT_POINTS_IMMORTAL;
        the_summonned->hit = HIT_POINTS_IMMORTAL;
    }
    act("$N appears before you.", ch, NULL, the_summonned, TO_CHAR);
    act("$N appears before $n.", ch, NULL, the_summonned, TO_ROOM);
    sprintf(buf, "%s has summonned %s into the game.\r\n", ch->names, arg);
    logmesg(buf);
}

void
do_deny (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Deny whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't logged in.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("You can't do that to NPCs.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You can't do that to higher level characters.\r\n", ch);
        return;
    }

    if ( !IS_SET(victim->act, PLR_DENY) )
    {
        SET_BIT(victim->act, PLR_DENY);
        send_to_char("You are denied access to the game!\r\n", victim);
        send_to_char("OK.\r\n", ch);
        save_char_obj(victim);
        if ( victim->desc )
            close_socket(victim->desc);

        victim->next_extract = extract_list;
        extract_list = victim;
    }
    else
    {
	// This should never happen - by being denied, they can't be online.
        send_to_char("That character is already denied.\r\n", ch);
    }

    return;
}

void
do_undeny (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Undeny whom?\r\n", ch);
        return;
    }
    if ( (victim = get_char_world(ch, arg)) != NULL )
    {
        if ( IS_NPC(victim) )
        {
            send_to_char("You can't do that to NPCs.\r\n", ch);
            return;
        }
    }

    if ( char_file_active(arg) )
    {
        send_to_char("That character is not denied.\r\n", ch);
        return;
    }

    if ( (victim = load_char_obj(NULL, arg)) == NULL )
    {
        send_to_char("That character is not in the player files.\r\n", ch);
        return;
    }

    victim->next = char_list;
    char_list = victim;
    char_to_room(victim, ch->in_room);
    if ( IS_SET(victim->act, PLR_DENY) )
    {
        REMOVE_BIT(victim->act, PLR_DENY);
        save_char_obj(victim);
	victim->next_extract = extract_list;
	extract_list = victim;
    }
    else
    {
        send_to_char("Loaded character, but they're not denied.\r\n", ch);
	logmesg("Error: loaded an undenied character in do_undeny.\r\n");
    }

    return;
}



void
do_disconnect (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Disconnect whom?\r\n", ch);
        return;
    }
    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim->desc == NULL )
    {
        act("$N doesn't have a descriptor.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if ( victim->trust > ch->trust )
    {
        send_to_char("You cannot do that to higher level characters.\r\n\r\n", ch);
        return;
    }

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
        if ( d == victim->desc )
        {
            // Probably no worth notifying someone they've been disconnected as such.
	    close_socket(d);
            send_to_char("Ok.\r\n", ch);
            return;
        }
    }

    logmesg("Do_disconnect: descriptor not found.");
    send_to_char("Descriptor not found!\r\n", ch);
    return;
}


void
do_gecho (CHAR_DATA * ch, char *argument)
{
    DESCRIPTOR_DATA *d;

    if ( argument[0] == '\0' )
    {
        send_to_char("Global echo what?\r\n", ch);
        return;
    }

    for ( d = descriptor_list; d; d = d->next )
    {
        if ( d->connected == CON_PLAYING )
        {
            if ( ch && get_trust(ch ) &&
                get_trust(d->character) >= get_trust(ch))
                send_to_char("global> ", d->character);
            send_to_char(argument, d->character);
            send_to_char("\r\n", d->character);
        }
    }

    return;
}



void
do_lecho (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *chars_in_room;

    if ( argument[0] == '\0' )
    {
        send_to_char("Local echo what?\r\n", ch);
        return;
    }

    for (chars_in_room = ch->in_room->people; chars_in_room;
         chars_in_room = chars_in_room->next_in_room)
    {
        if ( get_trust(chars_in_room) >= get_trust(ch) )
            send_to_char("local> ", chars_in_room);
        send_to_char(argument, chars_in_room);
        send_to_char("\r\n", chars_in_room);
    }
}

void
do_pecho (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);

    if ( argument[0] == '\0' || arg[0] == '\0' )
    {
        send_to_char("Personal echo what?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Target not found.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
        send_to_char("personal> ", victim);

    send_to_char(argument, victim);
    send_to_char("\r\n", victim);
    send_to_char("personal> ", ch);
    send_to_char(argument, ch);
    send_to_char("\r\n", ch);
}


ROOM_DATA *
find_location (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    LEVEL_DATA *lvl;
    OBJ_DATA *obj;
    bool rand;
    int x = 0;
    int y = 0;

    argument = one_argument(argument, arg);

    if ( !*arg )
        return NULL;

    if ( !str_prefix(arg, "safe") )
        return safe_area;

    if ( !ch || IS_IMMORTAL(ch) || IS_NPC(ch) )
    {
        if ( !str_prefix(arg, "god-general") )
            return god_general_area;
        else if ( !str_prefix(arg, "god-satori" ) ||
                 !str_prefix(arg, "zen-garden"))
            return satori_area;
        else if ( !str_prefix(arg, "god-explode") )
            return explosive_area;
        else if ( !str_prefix(arg, "god-store") )
            return store_area;
        else if ( !str_prefix(arg, "graveyard") )
            return graveyard_area;
        else if ( !str_prefix(arg, "deploy-area") )
            return deploy_area;
        else if ( !str_prefix(arg, "object") && ch )
        {
            argument = one_argument(argument, arg);

            if ( (obj = get_obj_world(ch, arg)) != NULL )
                return obj->in_room;
            else
                return NULL;
        }
        else if ( ch && (victim = get_char_world(ch, arg)) != NULL )
            return victim->in_room;
    }

    rand = !str_prefix(arg, "random");

    if ( !rand )
    {
        if ( !isdigit(*arg) )
        {
            if ( ch )
                send_to_char
                    ("You have to goto to some coordinates or 'random'.\r\n",
                     ch);
            else
                logmesg
                    ("%s %s -- You have to goto to some coordinates or 'random'.\r\n",
                     arg, argument);
            return NULL;
        }

        x = atoi(arg);
        argument = one_argument(argument, arg);
        y = atoi(arg);
    }

    argument = one_argument(argument, arg);

    if ( *arg )
    {
        int lnum = atoi(arg);

        for ( lvl = the_city; lvl; lvl = lvl->level_down )
            if ( lvl->level_number == lnum )
                break;

        if ( !lvl )
        {
            if ( ch )
                send_to_char("Level not found.\r\n", ch);
            else
                logmesg("Level not found (%d).", lnum);
            return NULL;
        }
    }
    else
        lvl = ch->in_room->this_level;

    if ( rand )
    {
        x = number_range(0, lvl->x_size - 1);
        y = number_range(0, lvl->y_size - 1);
    }
    else if ( x < 0 || x > lvl->x_size - 1 || y < 0 || y > lvl->y_size - 1 )
    {
        if ( ch )
            send_to_char
                ("Those coordinates don't exist on that level.\r\n", ch);
        else
            logmesg("Bad coordinates %d %d %d.", x, y,
                       lvl->level_number);
        return NULL;
    }

    return index_room(lvl->rooms_on_level, x, y);
}



void
do_transfer (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    ROOM_DATA *location;
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    argument = one_argument(argument, arg1);

    if ( arg1[0] == '\0' )
    {
        send_to_char("Transfer whom (and where)?\r\n", ch);
        return;
    }

    if ( !str_cmp(arg1, "all") )
    {
        for ( d = descriptor_list; d != NULL; d = d->next )
        {
            if (d->connected == CON_PLAYING
                && d->character != ch && d->character->in_room != NULL &&
                can_see(ch, d->character))
            {
                char buf[MAX_STRING_LENGTH];

                sprintf(buf, "%s %s", d->character->names, argument);
                do_transfer(ch, buf);
            }
        }
        return;
    }

    /*
     * Thanks to Grodyn for the optional location parameter.
     */
    if ( argument[0] == '\0' )
    {
        location = ch->in_room;
    }
    else if ( (location = find_location(ch, argument)) == NULL )
    {
        send_to_char("No such location.\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg1)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim->in_room == NULL )
    {
        send_to_char("They are in limbo.\r\n", ch);
        return;
    }

    if ( victim->fighting != NULL )
    {
        victim->fighting->fighting = NULL;
        victim->fighting = NULL;
    }
    act("A helicopter roars in from overhead kicking up dust and dirt in a " "wide radius.\r\n$n is hustled aboard as soldiers cover his " "safe boarding with deadly weapons of a make you have never seen.\r\n" "With a roar, the helicopter takes to the sky and is quickly lost " "from view.", victim, NULL, NULL, TO_ROOM);
    char_from_room(victim);
    char_to_room(victim, location);
    act("With a roar of displaced air, a helicopter arrives above you.\r\n"
        "Dust and dirt sting your face as it lands.\r\n$n is quickly pushed "
        "out and the helicopter speeds away into the sky once more.",
        victim, NULL, NULL, TO_ROOM);

    if ( ch != victim )
        act("A helicopter roars in from overhead kicking up dust and dirt in " "a wide radius.\r\nYou are efficiently hustled aboard as the " "Sergeant in command briefly mutters something to you about $n's " "orders.\r\nThen with a roar, the helicopter takes to the sky " "and before you know it, you are dropped off and the helicopter " "is gone once more.", ch, NULL, victim, TO_VICT);
    do_look(victim, "auto");
    send_to_char("Ok.\r\n", ch);

    stop_manning(victim);
    REMOVE_BIT(victim->temp_flags, IN_VEHICLE);

    if ( IS_SET(ch->temp_flags, IN_VEHICLE) )
        SET_BIT(victim->temp_flags, IN_VEHICLE);
}

void
do_at (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    ROOM_DATA *location;
    ROOM_DATA *original;
    CHAR_DATA *wch;

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' || argument[0] == '\0' )
    {
        send_to_char("At where what?\r\n", ch);
        return;
    }

    if ( (location = find_location(ch, arg)) == NULL )
    {
        send_to_char("No such location.\r\n", ch);
        return;
    }

    original = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, location);
    interpret(ch, argument);

    /*
     * See if 'ch' still exists before continuing!
     * Handles 'at XXXX quit' case.
     */
    for ( wch = char_list; wch != NULL; wch = wch->next )
    {
        if ( wch == ch )
        {
            char_from_room(ch);
            char_to_room(ch, original);
            break;
        }
    }

    return;
}



void
do_goto (CHAR_DATA * ch, char *argument)
{
    ROOM_DATA *location;
    CHAR_DATA *rch;

    if ( argument[0] == '\0' )
    {
        send_to_char("Syntax: goto <room>\r\n", ch);
	send_to_char("Destination can be either coordinates (X Y Z) or one of the predefined rooms (safe, god, etc.).\r\n", ch);
        return;
    }

    if ( (location = find_location(ch, argument)) == NULL )
    {
        send_to_char("No such location.\r\n", ch);
        return;
    }

    if ( ch->fighting != NULL )
    {
        ch->fighting->fighting = 0;
        ch->fighting = 0;
    }

    for ( rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room )
    {
        if ( get_trust(rch) >= ch->invis_level )
        {
            if (ch->pcdata != NULL && ch->pcdata->poofout_msg &&
                ch->pcdata->poofout_msg[0])
                act("$t", ch, ch->pcdata->poofout_msg, rch, TO_VICT);
            else if ( IS_IMMORTAL(ch) )
                act("$n leaves in a swirling mist.", ch, NULL, rch,
                    TO_VICT);
            else
                act("$n flashes brightly, then is gone.", ch, NULL, rch,
                    TO_VICT);
        }
    }

    char_from_room(ch);
    char_to_room(ch, location);

    for ( rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room )
    {
        if ( get_trust(rch) >= ch->invis_level )
        {
            if (ch->pcdata != NULL && ch->pcdata->poofin_msg &&
                ch->pcdata->poofin_msg[0])
                act("$t", ch, ch->pcdata->poofin_msg, rch, TO_VICT);
            else if ( IS_IMMORTAL(ch) )
                act("$n appears in a swirling mist.", ch, NULL, rch,
                    TO_VICT);
            else
                act("$n appears in a flash of white light!", ch, NULL, rch,
                    TO_VICT);
        }
    }

    stop_manning(ch);
    REMOVE_BIT(ch->temp_flags, IN_VEHICLE);
    do_look(ch, "auto");

    if ((ch->in_room->inside_mob &&
         IS_SET(ch->in_room->inside_mob->temp_flags, IS_VEHICLE)) ||
        (ch->in_room->interior_of &&
         ch->in_room->interior_of->item_type == ITEM_TEAM_VEHICLE))
        SET_BIT(ch->temp_flags, IN_VEHICLE);
}

/* RT to replace the 3 stat commands */

void
do_stat (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *string;
    OBJ_DATA *obj;
    CHAR_DATA *victim;

    string = one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  stat <name>\r\n", ch);
        send_to_char("  stat obj <name>\r\n", ch);
        send_to_char("  stat mob <name>\r\n", ch);
        send_to_char("  stat room <number>\r\n", ch);
        return;
    }

    if ( !str_cmp(arg, "obj") )
    {
        do_ostat(ch, string);
        return;
    }

    if ( !str_cmp(arg, "char") || !str_cmp(arg, "mob") )
    {
        do_mstat(ch, string);
        return;
    }

    /* do it the old way */

    obj = get_obj_world(ch, argument);
    if ( obj != NULL )
    {
        do_ostat(ch, argument);
        return;
    }

    victim = get_char_world(ch, argument);
    if ( victim != NULL )
    {
        do_mstat(ch, argument);
        return;
    }

    send_to_char("Nothing by that name found anywhere.\r\n", ch);
}



void
do_ostat (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Stat what?\r\n", ch);
        return;
    }

    if ( (obj = get_obj_world(ch, arg)) == NULL )
    {
        send_to_char("Nothing like that in hell, earth, or heaven.\r\n",
                     ch);
        return;
    }

    sprintf(buf, "Name(s): %s\r\n", obj->name);
    send_to_char(buf, ch);

    sprintf(buf, "Short description: %s\r\nLong description: %s\r\n",
            obj->short_descr, obj->description);
    send_to_char(buf, ch);

    sprintf(buf, "Vnum: %d  Type: %s%s\r\n",
            obj->pIndexData->vnum, item_type_name(obj),
            IS_SET(obj->general_flags,
                   GEN_NO_AMMO_NEEDED) ? "ammo_less" : "");
    send_to_char(buf, ch);

    sprintf(buf, "Wear bits: %s\r\n", wear_bit_name(obj->wear_flags));
    send_to_char(buf, ch);

    sprintf(buf, "char hp: %d struct hp %d char damages %d/%d/%d "
            "struct damages %d/%d/%d\r\nTimer: %d\r\n", obj->hp_char,
            obj->hp_struct, obj->damage_char[0], obj->damage_char[1],
            obj->damage_char[2], obj->damage_structural[0],
            obj->damage_structural[1], obj->damage_structural[2],
            obj->timer);
    send_to_char(buf, ch);

    sprintf(buf, "Range: %d Rate of Fire: %d Absorb: %d Ammo_type: %d\r\n",
            obj->range, obj->rounds_per_second, obj->armor,
            obj->ammo_type);
    send_to_char(buf, ch);
    sprintf(buf, "In room: (%d, %d, L%d) Carried by: %s  "
            "Wear_loc: %d\r\n", obj->in_room ? obj->in_room->x : 0,
            obj->in_room ? obj->in_room->y : 0,
            obj->in_room ? obj->in_room->level : 0,
            obj->carried_by == NULL ? "(none)" : PERS(obj->carried_by, ch),
            obj->wear_loc);
    send_to_char(buf, ch);

    sprintf(buf, "Owner: %s\r\n", obj->owner ? obj->owner->names : "none");
    send_to_char(buf, ch);
}


#define PCDAUTO(ch, elt, _)  ((ch)->pcdata && (ch)->pcdata->elt == -1 ? (_) : "")

void
do_mstat (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Stat whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, argument)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    sprintf(buf, "Name: %s.\r\n", victim->names);
    send_to_char(buf, ch);

    sprintf(buf, "Sex: %s  Room: (%d, %d, L%d)\r\n",
            victim->sex == SEX_MALE ? "male" :
            victim->sex == SEX_FEMALE ? "female" : "neutral",
            victim->in_room == NULL ? 0 : victim->in_room->x,
            victim->in_room == NULL ? 0 : victim->in_room->y,
            victim->in_room == NULL ? 0 : victim->in_room->level);
    send_to_char(buf, ch);

    sprintf(buf, "Hp: %d/%d\r\n", victim->hit, victim->max_hit);
    send_to_char(buf, ch);

    sprintf(buf, "Trust: %d\r\n", victim->trust);
    send_to_char(buf, ch);

    sprintf(buf, "Position: %d\r\n", victim->position);
    send_to_char(buf, ch);

    sprintf(buf, "Fighting: %s\r\n",
            victim->fighting ? victim->fighting->names : "(none)");
    send_to_char(buf, ch);

    sprintf(buf, "LD Timer: %d\r\n", victim->ld_timer);
    send_to_char(buf, ch);

    sprintf(buf, "Idle Ticks: %d\r\n", victim->idle);
    send_to_char(buf, ch);

    if ( !IS_NPC(victim) )
    {
        if ( victim->desc )
        {
            sprintf(buf,
                    "TType: %s%s%s\r\n"
                    "Screen Dim: [X=%s%d%s, Y=%s%d%s]\r\n",
                    PCDAUTO(victim, ttype, "AUTO ("),
                    ttypes[victim->desc->ttype],
                    PCDAUTO(victim, ttype, ")"),
                    PCDAUTO(victim, columns, "AUTO ("),
                    victim->desc->columns,
                    PCDAUTO(victim, columns, ")"),
                    PCDAUTO(victim, lines, "AUTO ("),
                    victim->desc->lines, PCDAUTO(victim, lines, ")"));
            send_to_char(buf, ch);
        }

        sprintf(buf, "Played: %d Timer: %d\r\n",
                (int) (victim->played + current_time -
                       victim->logon) / 3600, victim->timer);
        send_to_char(buf, ch);
    }
    else
    {
        printf_to_char(ch, "Worth: %d\r\n", victim->worth);
        printf_to_char(ch, "Chasing: %s\r\n",
                       (victim->chasing ? victim->chasing->names : "n/a"));
    }

    sprintf(buf, "Act: %s\r\n", act_bit_name(victim->act));
    send_to_char(buf, ch);

    if ( victim->comm )
    {
        sprintf(buf, "Comm: %s\r\n", comm_bit_name(victim->comm));
        send_to_char(buf, ch);
    }

    sprintf(buf, "Leader: %s\r\n",
            victim->leader ? victim->leader->names : "(none)");
    send_to_char(buf, ch);

    sprintf(buf, "Short description: %s\r\n", victim->short_descript);
    send_to_char(buf, ch);

    if ( !IS_NPC(victim) )
    {
        sprintf(buf, "\r\n&R== &WGame Stats&R ==&n\r\n");
        send_to_char(buf, ch);
        sprintf(buf,
                "Kills: %d\r\nDeaths: %d\r\nBooms: %d\r\nLemmings: %d\r\nIdle: %d\r\nHell pts: %d",
                victim->pcdata->gs_kills, victim->pcdata->gs_deaths,
                victim->pcdata->gs_booms, victim->pcdata->gs_lemmings,
                victim->pcdata->gs_idle, victim->pcdata->gs_hellpts);
        send_to_char(buf, ch);
    }
}

/* ofind and mfind replaced with vnum */

void
do_vnum (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *string;

    string = one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  vnum obj <name>\r\n", ch);
        return;
    }

    if ( !str_cmp(arg, "obj") )
    {
        do_ofind(ch, string);
        return;
    }

    do_ofind(ch, argument);
}

void
do_ofind (CHAR_DATA * ch, char *argument)
{
    extern int top_obj_index;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    int vnum;
    int nMatch;
    bool fAll;
    bool found;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Find what?\r\n", ch);
        return;
    }

    fAll = FALSE || !str_cmp(arg, "all");
    found = FALSE;
    nMatch = 0;

    for ( vnum = 1; vnum < top_obj_index; vnum++ )
    {
        if ( (pObjIndex = get_obj_index(vnum)) != NULL )
        {
            nMatch++;
            if ( fAll || is_name(argument, pObjIndex->name) )
            {
                found = TRUE;
                sprintf(buf, "[%5d](%2d) %s\r\n",
                        pObjIndex->vnum, pObjIndex->number_to_put_in,
                        pObjIndex->short_descr);

                send_to_char(buf, ch);
            }
        }
    }

    if ( !found )
        send_to_char("No objects by that name.\r\n", ch);

    return;
}



void
do_shutdow (CHAR_DATA * ch, char *argument)
{
    send_to_char("If you really want to shutdown the game, type it all the way out.\r\n", ch);
    return;
}


void
do_shutdown (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if ( str_cmp(arg, "now") )
    {
        send_to_char
            ("This will shutdown the Mud and *NOT* reboot it until it's manually\r\n"
             "restarted from the shell.  If you're sure you want to do this, use\r\n"
             "&cSHUTDOWN NOW&n.\r\n", ch);
        return;
    }

    do_reboot(ch, "-X");
}


void
do_reboot (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    bool quiet = FALSE;

    extern bool g_coldboot;

    argument = one_argument(argument, arg);

    if ( !str_cmp(arg, "-q") )
    {
        one_argument(argument, arg);
        quiet = TRUE;
    }

    if ( !str_cmp(arg, "-c") )
    {
        sprintf(buf, "Cold reboot by %s.\r\n", ch->names);
        g_coldboot = TRUE;
    }
    else if ( !str_cmp(arg, "-X") )
    {
        sprintf(buf, "Mud shutdown by %s.\r\n", ch->names);
        g_coldboot = TRUE;
        touch(SHUTDOWN_FILE);
    }
    else
    {
        sprintf(buf, "Warm reboot by %s.\r\n", ch->names);
        g_coldboot = FALSE;
    }

    if ( !quiet )
        do_gecho(NULL, buf);

    if ( g_coldboot )
    {
        DESCRIPTOR_DATA *d_next;
        DESCRIPTOR_DATA *d;

        for ( d = descriptor_list; d != NULL; d = d_next )
        {
            d_next = d->next;
            close_socket(d);
        }
    }
    // In case of shutdown command, should use STATUS_SHUTDOWN instead
    if ( !str_cmp(arg, "-X") ) ground0_down = STATUS_SHUTDOWN;
    else ground0_down = STATUS_REBOOT;
    save_polls();
}


void
do_snoop (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Snoop whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim->desc == NULL )
    {
        send_to_char("No descriptor to snoop.\r\n", ch);
        return;
    }

    if ( victim == ch )
    {
        send_to_char("Cancelling all snoops.\r\n", ch);
        for ( d = descriptor_list; d != NULL; d = d->next )
        {
            if ( d->snoop_by == ch->desc )
                d->snoop_by = NULL;
        }
        return;
    }

    if ( victim->desc->snoop_by != NULL )
    {
        send_to_char("Busy already.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You can't do that to higher level characters.\r\n", ch);
        return;
    }

    if ( ch->desc != NULL )
    {
        for ( d = ch->desc->snoop_by; d != NULL; d = d->snoop_by )
        {
            if ( d->character == victim || d->original == victim )
            {
                send_to_char("No snoop loops.\r\n", ch);
                return;
            }
        }
    }

    victim->desc->snoop_by = ch->desc;
    send_to_char("Ok.\r\n", ch);
    return;
}


#if 0
// do_switch technically works but needs lots of case checking and bugfixing
// so commenting out until that happens
void
do_switch (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);
    one_argument(argument, arg2);

    if ( arg[0] == '\0' )
    {
        send_to_char("Switch into whom?\r\n", ch);
        return;
    }

    if ( ch->desc == NULL )
        return;

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim == ch )
    {
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( victim->desc != NULL )
    {
        send_to_char("Character in use.\r\n", ch);
        return;
    }

    if ( strcmp(argument, victim->pcdata->password ))
    {
        send_to_char("Wrong Password.\r\n", ch);
        return;
    }

    ch->desc->character = victim;
    ch->desc->original = ch;
    victim->desc = ch->desc;
    ch->desc = NULL;
    /* change communications to match */
    victim->comm = ch->comm;
    send_to_char("Ok.\r\n", victim);
    return;
}
#endif 

/* trust levels for load and clone */

/* for clone, to insure that cloning goes many levels deep */
void
recursive_clone (OBJ_DATA * obj, OBJ_DATA * clone)
{
    OBJ_DATA *c_obj, *t_obj;

    for ( c_obj = obj->contains; c_obj != NULL; c_obj = c_obj->next_content )
    {
        t_obj = create_object(c_obj->pIndexData, 0);
        clone_object(c_obj, t_obj);
        obj_to_obj(t_obj, clone);
        recursive_clone(c_obj, t_obj);
    }
}


void
do_clone (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char id[MAX_INPUT_LENGTH];
    int number, i;

    argument = one_argument(argument, arg);
    number = mult_argument(argument, id);

    if ( !*arg )
    {
        send_to_char
            ("Usage: &cclone &yobject&X|&ymobile &X[&gnumber&Y*&X]&gname&n\r\n",
             ch);
        return;
    }
    else if ( !str_prefix(arg, "object") )
    {
        OBJ_DATA *clone = NULL;
        OBJ_DATA *obj;

        obj = get_obj_here(ch, id);
        if ( obj == NULL )
        {
            send_to_char("Clone only works on objects in the current room.\r\n", ch);
            return;
        }

        for ( i = 0; i < number; i++ )
        {
            clone = create_object(obj->pIndexData, 0);
            clone_object(obj, clone);
            if ( obj->carried_by != NULL )
                obj_to_char(clone, ch);
            else
                obj_to_room(clone, ch->in_room);
            if ( ch->trust )
                recursive_clone(obj, clone);
        }

        if ( !clone )
        {
            send_to_char("Hrm?!\r\n", ch);
            return;
        }

        sprintf(buf, "$n has created $p[%d].", number);
        act(buf, ch, clone, NULL, TO_ROOM);
        sprintf(buf, "You clone $p[%d].", number);
        act(buf, ch, clone, NULL, TO_CHAR);
        return;
    }
    else if ( !str_prefix(arg, "mobile") )
    {
        CHAR_DATA *clone = NULL;
        CHAR_DATA *mob;

        mob = get_char_room(ch, id);
        if ( mob == NULL )
        {
            send_to_char("Clone only works on mobiles in the current room.\r\n", ch);
            return;
        }
        else if ( !IS_NPC(mob) )
        {
            send_to_char("'clone mobile' only works on mobiles.\r\n", ch);
            return;
        }

        for ( i = 0; i < number; i++ )
        {
            clone = clone_mobile(mob, FALSE);
        }

        if ( !clone )
        {
            send_to_char("Hrm?!\r\n", ch);
            return;
        }

        sprintf(buf, "$n has created $N[%d].", number);
        act(buf, ch, NULL, clone, TO_ROOM);
        sprintf(buf, "You clone $N[%d].", number);
        act(buf, ch, NULL, clone, TO_CHAR);
        return;
    }

    do_clone(ch, "");
}


void
do_oload (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    OBJ_INDEX_DATA *pObjIndex;
    OBJ_DATA *obj;
    int vnum;

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if ( arg1[0] == '\0' || !is_number(arg1) )
    {
        send_to_char("Syntax: create <vnum>.\r\n", ch);
        return;
    }

    vnum = atoi(arg1);

    if ( vnum == VNUM_HQ )
    {
        send_to_char("You cannot load an HQ.\r\n", ch);
        return;
    }

    if ( (pObjIndex = get_obj_index(vnum)) == NULL )
    {
        send_to_char("No object has that vnum.\r\n", ch);
        return;
    }

    obj = create_object(pObjIndex, 0);
    if ( CAN_WEAR(obj, ITEM_TAKE) )
        obj_to_char(obj, ch);
    else
        obj_to_room(obj, ch->in_room);
    act("$n has created $p!", ch, obj, NULL, TO_ROOM);
    send_to_char("Ok.\r\n", ch);
    return;
}


void
do_destroy (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    for ( obj = ch->in_room->contents; obj != NULL; obj = obj_next )
    {
        obj_next = obj->next_content;
        if ( obj->item_type == ITEM_TEAM_ENTRANCE )
            continue;
        extract_obj(obj, 1);
    }

    act("$n destroys everything in the room!", ch, NULL, NULL, TO_ROOM);
    send_to_char("Ok.\r\n", ch);
}


void
do_purge (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[100];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    DESCRIPTOR_DATA *d;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        /* 'purge' */
        OBJ_DATA *obj_next;

        for ( obj = ch->in_room->contents; obj != NULL; obj = obj_next )
        {
            obj_next = obj->next_content;
            if ( obj->item_type == ITEM_TEAM_ENTRANCE )
                continue;
            extract_obj(obj, obj->extract_me);
        }

        act("$n purges the room!", ch, NULL, NULL, TO_ROOM);
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( !IS_NPC(victim) )
    {

        if ( ch == victim )
        {
            send_to_char("Purging yourself is a bad idea.\r\n", ch);
            return;
        }

        if ( get_trust(ch) <= get_trust(victim) )
        {
            send_to_char("You cannot purge higher level immortals.\r\n", ch);
            sprintf(buf, "%s tried to purge you!\r\n", ch->names);
            send_to_char(buf, victim);
            return;
        }

        act("$n disintegrates $N.", ch, 0, victim, TO_NOTVICT);
	sprintf(buf, "You purged %s.\r\n", victim->names);
	send_to_char(buf, ch);

        save_char_obj(victim);
        d = victim->desc;

        victim->next_extract = extract_list;
        extract_list = victim;

        if ( d != NULL )
            close_socket(d);

        return;
    }
    else if (victim->in_room == store_area || victim->in_room == graveyard_area)
    {
        send_to_char("Victim is in a protected area.\r\n", ch);
        return;
    }
    else if (victim == enforcer)
    {
	send_to_char("You cannot purge enforcers.\r\n", ch);
        return;
    }

    act("$n purges $N.", ch, NULL, victim, TO_NOTVICT);
    extract_char(victim, TRUE);
}


void
do_trust (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[100];
    CHAR_DATA *victim;
    int level;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if ( arg1[0] == '\0' || arg2[0] == '\0' || !is_number(arg2) )
    {
        send_to_char("Syntax: trust <char> <level>.\r\n", ch);
        return;
    }

    if ( (victim = get_char_room(ch, arg1)) == NULL )
    {
        send_to_char("That player is not here.\r\n", ch);
        return;
    }

    if ( (level = atoi(arg2)) < 0 || level > MAX_TRUST )
    {
	sprintf(buf, "Level must be 0 (reset) or between 1 and %d.\r\n", MAX_TRUST);
        send_to_char(buf, ch);
        return;
    }

    if ( level > get_trust(ch) )
    {
        send_to_char("Limited to your trust.\r\n", ch);
        return;
    }
    if ( ch->trust <= victim->trust && ch != victim )
    {
        send_to_char("You can only lower your own trust.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->act, PLR_WIZINVIS) )
    {
        if ( level < victim->invis_level )
            victim->invis_level = level;

        if ( level == 0 )
        {
            act("$n slowly fades into existence.", victim, NULL, NULL,
                TO_ROOM);
            send_to_char("You slowly fade back into existence.\r\n",
                         victim);
        }

        REMOVE_BIT(victim->act, PLR_HOLYLIGHT);
        REMOVE_BIT(victim->act, PLR_WIZINVIS);
    }

    victim->trust = level;
    sprintf(buf, "%s has set trust of %s to %d.", ch->names, victim->names, level);
    logmesg(buf);
    wiznet(buf, NULL, NULL, WIZ_ON, 0, 1);
    return;
}



void
do_restore (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *vch;
    DESCRIPTOR_DATA *d;

    one_argument(argument, arg);
    if ( arg[0] == '\0' || !str_cmp(arg, "room") )
    {
        /* cure room */

        for (vch = ch->in_room->people; vch != NULL;
             vch = vch->next_in_room)
        {
            vch->hit = vch->max_hit;
	    if ( !can_see(vch, ch) )
	      act("Someone has restored you.", ch, NULL, vch, TO_VICT);
	    else 
	      act("$n has restored you.", ch, NULL, vch, TO_VICT);
        }

        send_to_char("Room restored.\r\n", ch);
        return;

    }

    if ( !str_cmp(arg, "all") )
    {
        /* cure all */

        for ( d = descriptor_list; d != NULL; d = d->next )
        {
            victim = d->character;

            if ( victim == NULL || IS_NPC(victim) )
                continue;

            victim->hit = victim->max_hit;
            if ( victim->in_room != NULL ) 
	    {
		if ( !can_see(victim, ch) )
		  act("Someone has restored you.", ch, NULL, victim, TO_VICT);
                else 
	          act("$n has restored you.", ch, NULL, victim, TO_VICT);
	    }
        }
        send_to_char("All active players restored.\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    /* single target */
    victim->hit = victim->max_hit;
    if ( !can_see(victim, ch) )
      act("Someone has restored you.", ch, NULL, victim, TO_VICT);
    else 
      act("$n has restored you.", ch, NULL, victim, TO_VICT);
    send_to_char("Ok.\r\n", ch);
    return;
}


void
do_freeze (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Freeze whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("You can't do that to NPCs.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You cannot do that to higher level characters.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->act, PLR_FREEZE) )
    {
        REMOVE_BIT(victim->act, PLR_FREEZE);
        send_to_char("You can play again.\r\n", victim);
        send_to_char("FREEZE removed.\r\n", ch);
	wizlog(0, 0, WIZ_ON, 0, get_trust(ch), "%s unfrozen by %s.",
		victim->names, ch->names);
    }
    else
    {
        SET_BIT(victim->act, PLR_FREEZE);
        send_to_char("You can't do ANYthing!\r\n", victim);
        send_to_char("FREEZE set.\r\n", ch);
	wizlog(0, 0, WIZ_ON, 0, get_trust(ch), "%s frozen by %s.",
		victim->names, ch->names);
    }

    save_char_obj(victim);

    return;
}



void
do_log (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Log whom?\r\n", ch);
        return;
    }

    if ( !str_cmp(arg, "all") )
    {
        if ( fLogAll )
        {
            fLogAll = FALSE;
            send_to_char("Log ALL off.\r\n", ch);
        }
        else
        {
            fLogAll = TRUE;
            send_to_char("Log ALL on.\r\n", ch);
        }
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("You can't do that to NPCs.\r\n", ch);
        return;
    }

    /*
     * No level check, gods can log anyone.
     */
    if ( IS_SET(victim->act, PLR_LOG) )
    {
        REMOVE_BIT(victim->act, PLR_LOG);
        send_to_char("LOG removed.\r\n", ch);
    }
    else
    {
        SET_BIT(victim->act, PLR_LOG);
        send_to_char("LOG set.\r\n", ch);
    }

    return;
}



void
do_noemote (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Noemote whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }


    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You can't do that to higher level characters.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->comm, COMM_NOEMOTE) )
    {
        REMOVE_BIT(victim->comm, COMM_NOEMOTE);
        send_to_char("You can emote again.\r\n", victim);
        send_to_char("NOEMOTE removed.\r\n", ch);
    }
    else
    {
        SET_BIT(victim->comm, COMM_NOEMOTE);
        send_to_char("You can't emote!\r\n", victim);
        send_to_char("NOEMOTE set.\r\n", ch);
    }

    return;
}



void
do_noshout (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Noshout whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("You can't do that to NPCs.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You can't do that to higher level characters.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->comm, COMM_NOSHOUT) )
    {
        REMOVE_BIT(victim->comm, COMM_NOSHOUT);
        send_to_char("You can shout again.\r\n", victim);
        send_to_char("NOSHOUT removed.\r\n", ch);
    }
    else
    {
        SET_BIT(victim->comm, COMM_NOSHOUT);
        send_to_char("You can't shout!\r\n", victim);
        send_to_char("NOSHOUT set.\r\n", ch);
    }

    return;
}



void
do_notell (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Notell whom?", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("You can't do that to NPCs.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You can't do that to higher level characters.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->comm, COMM_NOTELL) )
    {
        REMOVE_BIT(victim->comm, COMM_NOTELL);
        send_to_char("You can tell again.\r\n", victim);
        send_to_char("NOTELL removed.\r\n", ch);
    }
    else
    {
        SET_BIT(victim->comm, COMM_NOTELL);
        send_to_char("You can't tell!\r\n", victim);
        send_to_char("NOTELL set.\r\n", ch);
    }

    return;
}



void
do_peace (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *rch;

    for ( rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room )
    {
        if ( rch->fighting != NULL )
        {
            rch->fighting->fighting = 0;
            rch->fighting = 0;
        }
    }

    send_to_char("Ok.\r\n", ch);
    return;
}


// Active ban code is in ban.c
#if 0
void
do_ban (CHAR_DATA * ch, char *argument)
{
    int newbie;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    BAN_DATA *pban;

    if ( ch && IS_NPC(ch) )
        return;

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        strcpy(buf, "Fully banned sites:\r\n");
        for ( pban = ban_list; pban != NULL; pban = pban->next )
        {
            strcat(buf, pban->name);
            strcat(buf, "\r\n");
            strcat(buf, pban->reason);
            strcat(buf, "\r\n");
        }
        send_to_char(buf, ch);
        strcpy(buf, "\r\n\r\nNewbie banned sites:\r\n");
        for ( pban = nban_list; pban != NULL; pban = pban->next )
        {
            strcat(buf, pban->name);
            strcat(buf, "\r\n");
            strcat(buf, pban->reason);
            strcat(buf, "\r\n");
        }
        send_to_char(buf, ch);
        return;
    }

    if ( !arg[0] || (str_prefix(arg, "newbie") && str_prefix(arg, "full")) )
    {
        send_to_char("Syntax: ban <newbie/full> <address> <reason>\r\n",
                     ch);
        return;
    }

    if ( !str_prefix(arg, "newbie") )
        newbie = 1;
    else
        newbie = 0;

    argument = one_argument(argument, arg);

    if ( !arg[0] || !argument[0] )
    {
        send_to_char("Syntax: ban <newbie/full> <address> <reason>\r\n",
                     ch);
        return;
    }

    for (pban = newbie ? nban_list : ban_list; pban != NULL;
         pban = pban->next)
    {
        if ( !str_cmp(arg, pban->name) )
        {
            send_to_char("Reason for banning was updated.\r\n", ch);
            strcpy(pban->reason, argument);
            return;
        }
    }

    if ( ban_free == NULL )
    {
        pban = alloc_perm(sizeof(*pban));
    }
    else
    {
        pban = ban_free;
        ban_free = ban_free->next;
    }

    strcpy(pban->name, arg);
    strcpy(pban->reason, argument);
    pban->next = newbie ? nban_list : ban_list;
    if ( newbie )
        nban_list = pban;
    else
        ban_list = pban;
    if ( ch )
        send_to_char("Ok.\r\n", ch);
    return;
}


void
do_saveban (CHAR_DATA * ch, char *argument)
{
    FILE *fp;
    BAN_DATA *pban;

    send_to_char("Saving full bans.\r\n", ch);

    if ( (fp = fopen(BAN_LIST, "w")) == NULL )
    {
        send_to_char("Open write error, aborting.\n", ch);
        return;
    }

    fprintf(fp, "#BANS\n");

    for ( pban = ban_list; pban != NULL; pban = pban->next )
    {
        fprintf(fp, "%s~\n", pban->name);
        fprintf(fp, "%s~\n", pban->reason);
    }

    fprintf(fp, "$~\n\n#$\n");
    fclose(fp);

    send_to_char("Saving newbie bans.\r\n", ch);

    if ( (fp = fopen(NBAN_LIST, "w")) == NULL )
    {
        send_to_char("Open write error, aborting.\n", ch);
        return;
    }

    fprintf(fp, "#NBANS\n");

    for ( pban = nban_list; pban != NULL; pban = pban->next )
    {
        fprintf(fp, "%s~\n", pban->name);
        fprintf(fp, "%s~\n", pban->reason);
    }

    fprintf(fp, "$~\n\n#$\n");
    fclose(fp);
}


void
do_allow (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    BAN_DATA *prev;
    BAN_DATA *curr;
    int count, found = 0;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Remove which site from the ban list?\r\n", ch);
        return;
    }

    for ( count = 0; count < 2; count++ )
    {
        prev = NULL;
        for (curr = count ? ban_list : nban_list; curr != NULL;
             prev = curr, curr = curr->next)
        {
            if ( !str_cmp(arg, curr->name) )
            {
                if ( prev == NULL )
                    if ( count )
                        ban_list = ban_list->next;
                    else
                        nban_list = nban_list->next;
                else
                    prev->next = curr->next;

                curr->next = ban_free;
                ban_free = curr;
                if ( !count )
                    send_to_char("Newbie ban removed.\r\n", ch);
                else
                    send_to_char("Perm ban removed.\r\n", ch);
                found = 1;
                break;
            }
        }
    }
    if ( !found )
        send_to_char("Site is not banned.\r\n", ch);
}
#endif



void
do_wizlock (CHAR_DATA * ch, char *argument)
{
    extern bool wizlock;

    wizlock = !wizlock;

    if ( wizlock )
        send_to_char("Game wizlocked.\r\n", ch);
    else
        send_to_char("Game un-wizlocked.\r\n", ch);

    return;
}

/* RT anti-newbie code */

void
do_newlock (CHAR_DATA * ch, char *arg)
{
    char buf[MAX_STRING_LENGTH];
    extern bool newlock;

    if ( !str_cmp(arg, "on") )
	{
	newlock = TRUE;
	send_to_char("&YThe game has been locked to new characters.&n\r\n", ch);
	wizlog(0, 0, WIZ_ON, 0, get_trust(ch), "&YGame newlocked by %s.&n",
		ch->names);
	return;
	}
    else if ( !str_cmp(arg, "off") )
	{
	newlock = FALSE;
	send_to_char("&YNewlock has been removed.&n\r\n", ch);
	wizlog(0, 0, WIZ_ON, 0, get_trust(ch), "&YNewlock removed by %s.&n",
		ch->names);
	return;
	}
    sprintf(buf, "USAGE: newlock <on|off>\r\n\r\n"
		 "&YNewlock is currently: %s&n\r\n", 
		(newlock) ? "on" : "off");
    send_to_char(buf, ch);
    return;
}



/* RT set replaces sset, mset, oset, and rset */

void
do_set (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  set mob   <name> <field> <value>\r\n", ch);
        send_to_char("  set obj   <name> <field> <value>\r\n", ch);
        send_to_char("  set team  <team> <field> <value>\r\n", ch);
        send_to_char("  set enf   <field> <value>\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "mobile") || !str_prefix(arg, "character") )
        do_mset(ch, argument);
    else if ( !str_prefix(arg, "object") )
        do_oset(ch, argument);
    else if ( !str_prefix(arg, "team") )
        do_tset(ch, argument);
    else if ( !str_prefix(arg, "enforcer") )
        do_enfset(ch, argument);
    else
        do_set(ch, "");
}


void
do_enfset (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    extern bool spamkill_protection;
    extern bool time_protection;

    one_argument(argument, arg);

    if ( !str_prefix(arg + 1, "spamprot") )
    {
        switch (arg[0])
        {
            case '+':
                spamkill_protection = TRUE;
                send_to_char("Spamkill protection enabled.\r\n", ch);
                break;
            case '-':
                spamkill_protection = FALSE;
                send_to_char("Spamkill protection disabled.\r\n", ch);
                break;
            case '?':
                sprintf(arg, "Spamikill protection: %s\r\n",
                        (spamkill_protection ? "ON" : "OFF"));
                send_to_char(arg, ch);
                break;
            default:
                /* Send usage message. */
                do_enfset(ch, "");
                break;
        }
        return;
    }
    else if ( !str_prefix(arg + 1, "timeprot") )
    {
        switch (arg[0])
        {
            case '+':
                time_protection = TRUE;
                send_to_char("Round time protection enabled.\r\n", ch);
                break;
            case '-':
                time_protection = FALSE;
                send_to_char("Round time protection disabled.\r\n", ch);
                break;
            case '?':
                sprintf(arg, "Round time protection: %s\r\n",
                        (time_protection ? "ON" : "OFF"));
                send_to_char(arg, ch);
                break;
            default:
                /* Send usage message. */
                do_enfset(ch, "");
                break;
        }
        return;
    }

    send_to_char("Syntax:\r\n", ch);
    send_to_char("  set enf [+|-|?]<field>\r\n", ch);
    send_to_char("Available fields:\r\n", ch);
    send_to_char("  spamprot timeprot\r\n", ch);
}


void
do_mset (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int value;

    smash_tilde(argument);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    strcpy(arg3, argument);

    if ( arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  set char <name> <field> <value>\r\n", ch);
        send_to_char("  Field being one of:\r\n", ch);
        send_to_char("    hp permhp kp kills deaths sex team\r\n"
                     "    mdelay tweakbot blind dazed\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg1)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    /*
     * Snarf the value (which need not be numeric).
     */
    value = is_number(arg3) ? atoi(arg3) : -1;

    if ( !str_prefix(arg2, "mdelay") )
    {
        if ( value < 0 || value > 10 )
        {
            send_to_char("That's too slow.\r\n", ch);
            return;
        }
        else if ( !IS_NPC(victim) )
        {
            send_to_char("You can't set move delays on players.\r\n", ch);
            return;
        }

        victim->move_delay = value;
        return;
    }


    if ( !str_prefix(arg2, "kills") )
    {
        if ( value < 0 || value > 99999 )
        {
            send_to_char("Kill range is between 0 and 99,999.\r\n", ch);
            return;
        }
        if ( IS_NPC(victim) )
        {
            send_to_char("You can't set kills on mobs.\r\n", ch);
            return;
        }
        victim->kills = value;
        return;
    }

    if ( !str_prefix(arg2, "deaths") )
    {
        if ( value < 0 || value > 99999 )
        {
            send_to_char("KP range is between 0 and 99,999.\r\n", ch);
            return;
        }
        if ( IS_NPC(victim) )
        {
            send_to_char("You can't set deaths on mobs.\r\n", ch);
            return;
        }
        victim->deaths = value;
        return;
    }

    if ( !str_prefix(arg2, "team") )
    {
        if ( (value = lookup_team(arg3)) == -1 )
        {
            send_to_char
                ("That's not a team.  Type 'teamstats' to see them.\r\n",
                 ch);
            return;
        }

        if ( !team_table[victim->team].independent )
            team_table[victim->team].players--;

        if ( team_table[victim->team].teamleader == victim )
        {
            team_table[victim->team].teamleader = NULL;
            autoleader(victim->team, NULL, victim);
        }

        victim->team = value;

        if ( !team_table[victim->team].independent )
            team_table[victim->team].players++;

        return;
    }

    if ( !str_prefix(arg2, "sex") )
    {

        if ( value < 0 || value > 2 )
        {
            send_to_char("Sex range is 0 to 2.\r\n", ch);
            return;
        }
        victim->sex = value;
        if ( !IS_NPC(victim) )
            victim->sex = value;
        return;
    }

    if ( !str_prefix(arg2, "hp") )
    {
        victim->max_hit = value;
        return;
    }

    if ( !str_prefix(arg2, "permhp") )
    {
        if ( value < -10 || value > 30000 )
        {
            send_to_char("Permanent HP range is -10 to 30,000.\r\n", ch);
            return;
        }
        else if ( IS_NPC(victim) || !victim->pcdata )
        {
            send_to_char("Mobiles don't have permanent HP.\r\n", ch);
            return;
        }

        victim->pcdata->solo_hit = victim->max_hit = value;
        return;
    }

    if ( !str_prefix(arg2, "tweakbot") )
    {
        if ( !str_cmp(arg3, "on") || !str_cmp(arg3, "yes") ||
             !str_cmp(arg3, "true") || !str_cmp(arg3, "on") )
        {
            act("Enabled suspected bot tweaks on $M.", ch, NULL, victim, TO_CHAR);
            SET_BIT(victim->act, PLR_SUSPECTED_BOT);
        }
        else
        {
            act("Disabled suspected bot tweaks on $M.", ch, NULL, victim, TO_CHAR);
            REMOVE_BIT(victim->act, PLR_SUSPECTED_BOT);
        }
        return;
    }

    if ( !str_prefix(arg2, "blind") )
    {
        if ( !str_cmp(arg3, "on") || !str_cmp(arg3, "yes") ||
             !str_cmp(arg3, "true") )
        {
            act("You set a blindness upon $M.", ch, NULL, victim, TO_CHAR);
            SET_BIT(victim->affected_by, AFF_BLIND);
            send_to_char("Your vision fails.\r\n", victim);
        }
        else
        {
            act("You lift the blindness from $M.", ch, NULL, victim, TO_CHAR);
            REMOVE_BIT(victim->affected_by, AFF_BLIND);
            send_to_char("You can see again!\r\n", victim);
        }

        return;
    }

    if ( !str_prefix(arg2, "dazed") )
    {
        if ( !str_cmp(arg3, "on") || !str_cmp(arg3, "yes") ||
             !str_cmp(arg3, "true") )
        {
            act("You daze $M.", ch, NULL, victim, TO_CHAR);
            SET_BIT(victim->affected_by, AFF_DAZE);
            send_to_char("You find yourself suddenly paralyzed.\r\n", victim);
        }
        else
        {
            act("You un-daze $M.", ch, NULL, victim, TO_CHAR);
            REMOVE_BIT(victim->affected_by, AFF_DAZE);
            send_to_char("You are no longer dazed!\r\n", victim);
        }

        return;
    }

    /*
     * Generate usage message.
     */
    do_mset(ch, "");
    return;
}

void
do_string (CHAR_DATA * ch, char *argument)
{
    char type[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *obj;

    smash_tilde(argument);
    argument = one_argument(argument, type);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    strcpy(arg3, argument);

    if (type[0] == '\0' || arg1[0] == '\0' || arg2[0] == '\0' ||
        arg3[0] == '\0')
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  string char <name> <field> <string>\r\n", ch);
        send_to_char("    fields: short long desc title spec\r\n", ch);
        send_to_char("  string obj  <name> <field> <string>\r\n", ch);
        send_to_char("    fields: name short long extended\r\n", ch);
        return;
    }

    if ( !str_prefix(type, "character") || !str_prefix(type, "mobile") )
    {
        if ( (victim = get_char_world(ch, arg1)) == NULL )
        {
            send_to_char("They aren't here.\r\n", ch);
            return;
        }

        /* string something */

        if ( !str_prefix(arg2, "name") )
        {
            if ( !IS_NPC(victim) )
            {
                send_to_char("You can't do that to players.\r\n", ch);
                return;
            }

            free_string(victim->names);
            victim->names = str_dup(arg3);
            return;
        }

        if ( !str_prefix(arg2, "short") )
        {
            free_string(victim->short_descript);
            victim->short_descript = str_dup(arg3);
            return;
        }

        if ( !str_prefix(arg2, "title") )
        {
            if ( IS_NPC(victim) )
            {
                send_to_char("You can't do that to NPCs.\r\n", ch);
                return;
            }

            set_title(victim, arg3);
            return;
        }

        if ( !str_prefix(arg2, "spec") )
        {
            if ( !IS_NPC(victim) )
            {
                send_to_char("You can't do that to players.\r\n", ch);
                return;
            }
            return;
        }
    }

    if ( !str_prefix(type, "object") )
    {
        /* string an obj */

        if ( (obj = get_obj_world(ch, arg1)) == NULL )
        {
            send_to_char("Nothing like that in heaven or earth.\r\n", ch);
            return;
        }

        if ( !str_prefix(arg2, "name") )
        {
            free_string(obj->name);
            obj->name = str_dup(arg3);
            return;
        }

        if ( !str_prefix(arg2, "short") )
        {
            free_string(obj->short_descr);
            obj->short_descr = str_dup(arg3);
            return;
        }

        if ( !str_prefix(arg2, "long") )
        {
            free_string(obj->description);
            obj->description = str_dup(arg3);
            return;
        }
    }


    /* echo bad use message */
    do_string(ch, "");
}


void
do_oset (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int value, value2;

    smash_tilde(argument);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if ( arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  set obj <object> <field> <value>\r\n", ch);
        send_to_char("  Field being one of:\r\n", ch);
        send_to_char("    char_dmg <0-2> struct_dmg <0-2>\r\n", ch);
        send_to_char("    extra wear level weight cost timer\r\n", ch);
        return;
    }

    if ( (obj = get_obj_world(ch, arg1)) == NULL )
    {
        send_to_char("Nothing like that in heaven or earth.\r\n", ch);
        return;
    }

    /*
     * Snarf the value (which need not be numeric).
     */
    if ( is_number(arg3) )
        value = atoi(arg3);
    else
        value = 0;

    /*
     * Set something.
     */

    if ( !str_prefix(arg2, "wear") )
    {
        obj->wear_flags = value;
        return;
    }

    if ( !str_prefix(arg2, "weight") )
    {
        obj->weight = value;
        return;
    }

    if ( !str_prefix(arg2, "timer") )
    {
        obj->timer = value;
        return;
    }

    if ( !str_prefix(arg2, "char_dmg") )
    {
        argument = one_argument(argument, arg4);
        if ( is_number(arg4) )
            value2 = atoi(arg4);
        else
            value2 = 0;
        if ( (value2 > -MAX_INT) && (value2 < MAX_INT) && (value > -1 ) &&
            (value < 3))
        {
            obj->damage_char[value] = value2;
            return;
        }
    }

    if ( !str_prefix(arg2, "struct_dmg") )
    {
        argument = one_argument(argument, arg4);
        if ( is_number(arg4) )
            value2 = atoi(arg4);
        else
            value2 = 0;
        if ( (value2 > -MAX_INT) && (value2 < MAX_INT) && (value > -1 ) &&
            (value < 3))
        {
            obj->damage_structural[value] = value2;
            return;
        }
    }

    /*
     * Generate usage message.
     */
    do_oset(ch, "");
    return;
}


void
do_tset (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *vict;
    int value, team;

    smash_tilde(argument);
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if ( arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  set team <team> <field> <value>\r\n", ch);
        send_to_char("Where field is one of:\r\n", ch);
        send_to_char("  score leader players active independent\r\n", ch);
        return;
    }

    if ( (team = lookup_team(arg1)) == -1 )
    {
        send_to_char("That team does not exist.\r\n", ch);
        return;
    }

    if ( is_number(arg3) )
        value = atoi(arg3);
    else
        value = 0;

    /*
     * Set something.
     */

    if ( !str_prefix(arg2, "score") )
    {
        team_table[team].score = value;
        return;
    }

    if ( !str_prefix(arg2, "leader") )
    {
        if ( (vict = get_char_world(ch, arg3)) == NULL )
        {
            send_to_char("Who's that?\r\n", ch);
            return;
        }
        else if ( vict->team != team )
        {
            send_to_char
                ("Can't set a player to leader of team they're not on.\r\n",
                 ch);
            return;
        }

        if ( team_table[team].teamleader )
        {
            sprintf(buf,
                    "%s%s&n has been removed from %s&n's leadership by %s%s&n",
                    team_table[team].namecolor,
                    team_table[team].teamleader->names,
                    team_table[team].who_name,
                    team_table[ch->team].namecolor, ch->names);
            do_gecho(NULL, buf);
        }

        sprintf(buf,
                "%s%s&n has been promoted to %s&n's leadership by %s%s&n",
                team_table[team].namecolor, vict->names,
                team_table[team].who_name, team_table[ch->team].namecolor,
                ch->names);
        do_gecho(NULL, buf);
        team_table[team].teamleader = vict;
        return;
    }

    if ( !str_prefix(arg2, "players") )
    {
        team_table[team].players = value;
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg2, "active") )
    {
        if ( is_number(arg3) )
            team_table[team].active = value;
        else if ( !str_prefix(arg3, "true") )
            team_table[team].active = TRUE;
        else if ( !str_prefix(arg3, "false") )
            team_table[team].active = FALSE;
        else
            send_to_char
                ("Bad argument.  Must be either 0 (FALSE) or 1 (TRUE).\r\n",
                 ch);
        return;
    }

    if ( !str_prefix(arg2, "independent") )
    {
        if ( is_number(arg3) )
            team_table[team].independent = value;
        else if ( !str_prefix(arg3, "true") )
            team_table[team].independent = TRUE;
        else if ( !str_prefix(arg3, "false") )
            team_table[team].independent = FALSE;
        else
        {
            send_to_char
                ("Bad argument.  Must be either 0 (FALSE) or 1 (TRUE).\r\n",
                 ch);
            return;
        }

        if ( team_table[team].independent )
        {
            sprintf(buf,
                    "Team %s&n has been disbanded by the order of %s%s&W(%s&W)&n!\r\n",
                    team_table[team].who_name,
                    team_table[ch->team].namecolor, ch->names,
                    team_table[ch->team].who_name);
            do_gecho(NULL, buf);
        }
        else
        {
            sprintf(buf,
                    "Team %s&n has been united on the orders of %s%s&W(%s&W)&n!\r\n",
                    team_table[team].who_name,
                    team_table[ch->team].namecolor, ch->names,
                    team_table[ch->team].who_name);
            do_gecho(NULL, buf);
        }
        send_to_char("Ok.\r\n", ch);
        return;
    }

    /* Generate usage message if we get to this point. */
    do_tset(ch, "");
}


const char *connected_states[] = {
    "PLAYING",
    "GET HOST",
    "QANSI",
    "GET NAME",
    "PASSWORD",
    "CONF NEW",
    "ADM PWD",
    "NEW PWD",
    "CONF PWD",
    "ADM CHAR",
    "QSEX",
    "DEFAULT",
    "QSOLO",
    "IMOTD",
    "MOTD",
    "DISCON",
    "NEWBIE",
    "RELOG",
    "ISO6429",
    "\n"
};


void
do_sockets (CHAR_DATA * ch, char *argument)
{
    char buf[2 * MAX_STRING_LENGTH];
    char site[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    CHAR_DATA *who;
    int count = 0;

    one_argument(argument, arg);

    strcpy(buf,
           "&CSOCK&X-&CNAME&X------------&CSTATE&X----&CHOURS&X-&CSITE&X----&n\r\n");

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
        who = (d->original ? d->original : d->character);

        if ( !who )
            continue;
        if ( !can_see(ch, who) )
            continue;
        if ( *arg && !is_name(arg, who->names) )
            continue;
        if ( !ch->trust && d->connected != CON_PLAYING )
            continue;

        count++;
        memset(site, 0, MAX_INPUT_LENGTH);

        if ( !ch->trust && !who->trust )
        {
            register char *rptr = d->host;
            register char *wptr = site;
            char *eptr = d->host + 37;

            while ( rptr < eptr )
            {
                if ( isdigit(*rptr) )
                {
                    *(wptr++) = '&';
                    *(wptr++) = 'X';

                    while ( rptr < eptr && isdigit(*rptr) )
                    {
                        *(wptr++) = 'X';
                        rptr++;
                    }

                    *(wptr++) = '&';
                    *(wptr++) = 'r';
                }

                if ( !(*(wptr++) = *(rptr++)) )
                {
                    if ( isdigit(*(wptr - 1)) )
                        *(--wptr) = '\0';
                    break;
                }
            }
        }
        else
        {
            if ( ch->trust )
                strncpy(site, d->host, 36);
            else
                strncpy(site, "-", 36);
        }

        sprintf(buf + strlen(buf),
                "&B%-4d &W%-15s &c%-8s &n%-5ld &r%s&n\r\n", d->descriptor,
                (!who->names ? "[unnamed]" : who->names),
                connected_states[d->connected], HOURS_PLAYED(who), site);
    }

    if ( count == 0 && *arg )
    {
        send_to_char("No one by that name is connected.\r\n", ch);
        return;
    }

    sprintf(buf + strlen(buf), "%d user%s\r\n", count,
            (count == 1 ? "" : "s"));
    page_to_char(buf, ch);
}



/*
 * Thanks to Grodyn for pointing out bugs in this function.
 */
void
do_force (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' || argument[0] == '\0' )
    {
        send_to_char("Force whom to do what?\r\n", ch);
        return;
    }

    one_argument(argument, arg2);
    
    if ( !str_cmp(arg2, "delete") || !str_cmp(arg2, "password") )
    {
        send_to_char("You can't force players to do that.\r\n", ch);
        return;
    }

    sprintf(buf, "$n forces you to '%s'.", argument);

    if ( !str_cmp(arg, "all") )
    {
        CHAR_DATA *vch;
        CHAR_DATA *vch_next;

        for ( vch = char_list; vch != NULL; vch = vch_next )
        {
            vch_next = vch->next;

            if ( !IS_NPC(vch) && get_trust(vch) < get_trust(ch) )
            {
                act(buf, ch, NULL, vch, TO_VICT);
                interpret(vch, argument);
            }
        }
    }
    else if ( !str_cmp(arg, "players") )
    {
        CHAR_DATA *vch;
        CHAR_DATA *vch_next;

        for ( vch = char_list; vch != NULL; vch = vch_next )
        {
            vch_next = vch->next;

            if ( !IS_NPC(vch) && get_trust(vch) < get_trust(ch) )
            {
                act(buf, ch, NULL, vch, TO_VICT);
                interpret(vch, argument);
            }
        }
    }
    else if ( !str_cmp(arg, "gods") )
    {
        CHAR_DATA *vch;
        CHAR_DATA *vch_next;

        for ( vch = char_list; vch != NULL; vch = vch_next )
        {
            vch_next = vch->next;

            if ( !IS_NPC(vch) && get_trust(vch) < get_trust(ch) )
            {
                act(buf, ch, NULL, vch, TO_VICT);
                interpret(vch, argument);
            }
        }
    }
    else
    {
        CHAR_DATA *victim;

        if ( (victim = get_char_world(ch, arg)) == NULL )
        {
            send_to_char("They aren't here.\r\n", ch);
            return;
        }

        if ( victim == ch )
        {
            send_to_char("Aye aye, right away!\r\n", ch);
            return;
        }

        if ( get_trust(victim) >= get_trust(ch) )
        {
            send_to_char("Do it yourself!\r\n", ch);
            return;
        }

        act(buf, ch, NULL, victim, TO_VICT);
        interpret(victim, argument);
    }

    send_to_char("Ok.\r\n", ch);
    return;
}

void
do_penalize (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    CHAR_DATA *vch;

    argument = one_argument(argument, arg);

    /* RELEASE: This is a facility to allow mortals to have limited punitive
       powers.  On a game like GZ where the imms weren't on much *duck* this
       was very useful and was the first measure of trust I gave to a mortal
       before later giving them an imm if they did well at dealing with
       spammers, etc. */
//    if ( !is_name(ch->names, "tireg euthenasia lalmec") )
//    if ( 1 )
//    {
//        send_to_char("Huh?!\n", ch);
//        return;
    }

    if ( !arg[0] )
    {
        send_to_char("USAGE: penalize <character name>\r\n", ch);
        return;
    }

    if ( (vch = get_char_world(NULL, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( vch == ch )
    {
        send_to_char("You can't penalize yourself.\r\n", ch);
        return;
    }

    if ( IS_NPC(vch) )
    {
        send_to_char("If mobs are causing problems, penalizing them won't help.\r\n", ch);
        return;
    }

    one_argument(argument, arg);
    if ( enforcer && (enforcer->desc) )
        if ( !arg[0] )
        {
            send_to_char("The enforcer position is already being used.\r\n"
                         "'penalize <name> f' to kick them out.\r\n", ch);
            return;
        }

    sprintf(log_buf, "%s ENTERS PENAL mode for %s.", ch->names,
            vch->names);
    logmesg(log_buf);
    wiznet(log_buf, NULL, NULL, WIZ_ON, 0, 1);
    save_char_obj(ch);

    if ( enforcer->desc )
    {
        CHAR_DATA *tmp;

        sprintf(log_buf, "Old enforcer %s is ousted for %s.",
                enforcer->names, ch->names);
        logmesg(log_buf);
        act("$n takes over the enforcer position.\r\n", ch, NULL, enforcer,
            TO_VICT);
        tmp = enforcer->desc->original;
        tmp->desc = enforcer->desc;
        tmp->desc->character = tmp;
        tmp->desc->original = NULL;
        if ( tmp->trust == 1 )
            tmp->trust = 0;
        enforcer->desc = NULL;
    }

    enforcer->hit = enforcer->max_hit = HIT_POINTS_IMMORTAL;
    enforcer->kills = 0;
    enforcer->trust = 2;
    if ( !ch->trust )
        ch->trust = 1;
    char_from_room(ch);
    char_to_room(ch, safe_area);

    ch->desc->character = enforcer;
    ch->desc->original = ch;
    enforcer->desc = ch->desc;
    ch->desc = NULL;
    /* change communications to match */
    enforcer->comm = ch->comm;
    do_look(enforcer, "auto");
    send_to_char("You are now in penal mode.  'Done' will return you to the game.\r\n", enforcer);
}

void
do_done (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *tmp;

    if ( ch != enforcer )
    {
        send_to_char("Huh?!\r\n", ch);
        return;
    }

    sprintf(log_buf, "%s LEAVES PENAL mode.", ch->desc->original->names);
    logmesg(log_buf);
    tmp = enforcer->desc->original;
    tmp->desc = enforcer->desc;
    tmp->desc->character = tmp;
    tmp->desc->original = NULL;
    enforcer->desc = NULL;
    if ( tmp->trust == 1 )
        tmp->trust = 0;
    do_look(tmp, "auto");
    send_to_char("You return to the game.\r\n", tmp);
}

/*
 * New routines by Dionysos.
 */
void
do_invis (CHAR_DATA * ch, char *argument)
{
    int level;
    char arg[MAX_STRING_LENGTH];

    if ( IS_NPC(ch) )
        return;

    /* RT code for taking a level argument */
    one_argument(argument, arg);

    if ( arg[0] == '\0' )
        /* take the default path */

        if ( IS_SET(ch->act, PLR_WIZINVIS) )
        {
            REMOVE_BIT(ch->act, PLR_WIZINVIS);
            ch->invis_level = 0;
            act("$n slowly fades into existence.", ch, NULL, NULL,
                TO_ROOM);
            send_to_char("You slowly fade back into existence.\r\n", ch);
        }
        else
        {
            SET_BIT(ch->act, PLR_WIZINVIS);
            ch->invis_level = get_trust(ch);
            act("$n slowly fades into thin air.", ch, NULL, NULL, TO_ROOM);
            send_to_char("You slowly vanish into thin air.\r\n", ch);
        }
    else
        /* do the level thing */
    {
        level = atoi(arg);
        if ( level < 2 || level > get_trust(ch) )
        {
            send_to_char
                ("Invis level must be between 2 and your level.\r\n", ch);
            return;
        }
        else
        {
            ch->reply = NULL;
            SET_BIT(ch->act, PLR_WIZINVIS);
            ch->invis_level = level;
            act("$n slowly fades into thin air.", ch, NULL, NULL, TO_ROOM);
            send_to_char("You slowly vanish into thin air.\r\n", ch);
        }
    }

    return;
}



void
do_holylight (CHAR_DATA * ch, char *argument)
{
    if ( IS_NPC(ch) )
        return;

    if ( IS_SET(ch->act, PLR_HOLYLIGHT) )
    {
        REMOVE_BIT(ch->act, PLR_HOLYLIGHT);
        send_to_char("Holy light mode off.\r\n", ch);
    }
    else
    {
        SET_BIT(ch->act, PLR_HOLYLIGHT);
        send_to_char("Holy light mode on.\r\n", ch);
    }

    return;
}


void
do_noleader (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Noleader whom?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("You can't do that to NPCs.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You can't do that to a higher level character.\r\n", ch);
        return;
    }

    if ( !IS_SET(victim->act, PLR_NOLEADER) )
    {
        SET_BIT(victim->act, PLR_NOLEADER);
        send_to_char("You are no longer allowed to be a leader!\r\n",
                     victim);
        act("$N can no longer be a leader.\r\n", ch, NULL, victim,
            TO_CHAR);
        save_char_obj(victim);
        return;
    }

    send_to_char("You may be a leader once more!\r\n", victim);
    act("$N can now be a leader once more.\r\n", ch, NULL, victim,
        TO_CHAR);
    REMOVE_BIT(victim->act, PLR_NOLEADER);
    save_char_obj(victim);
    return;
}

void
do_tick (CHAR_DATA * ch, char *argument)
{
    tick_stuff(1);
    send_to_char("Tick.\r\n", ch);
}


void
do_expand (CHAR_DATA * ch, char *argument)
{
    if ( !g_forceAction )
    {
        send_to_char("Type &WEXPAND!&n to expand the city.\r\n", ch);
        return;
    }
    else if ( expansions >= MAX_EXPANSIONS )
    {
        send_to_char("The game can't expand any further.\r\n", ch);
        return;
    }

    expand_city();
    expansions++;
    scatter_objects();
    send_to_char("Done.\r\n", ch);
}

void
do_grab (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if ( (arg1[0] == '\0') || (arg2[0] == '\0') )
    {
        send_to_char("Syntax : grab <object> <player>\r\n", ch);
        return;
    }

    if ( !(victim = get_char_world(ch, arg2)) )
    {
        send_to_char("They are not here!\r\n", ch);
        return;
    }

    if ( !(obj = get_obj_list(ch, arg1, victim->carrying)) )
    {
        send_to_char("They do not have that item.\r\n", ch);
        return;
    }

    if ( victim->trust >= ch->trust )
    {
        send_to_char("You can't do that to higher level characters.\r\n", ch);
        return;
    }

    if ( obj->wear_loc != WEAR_NONE )
        unequip_char(victim, obj);

    obj_from_char(obj);
    obj_to_char(obj, ch);

    act("You grab $p from $N.", ch, obj, victim, TO_CHAR);
    if ( arg3[0] == '\0' || !str_cmp(arg3, "yes") || !str_cmp(arg3, "true") )
        act("You are no longer worthy of $p.", ch, obj, victim, TO_VICT);

    return;
}


const char *
name_expand (CHAR_DATA * ch)
{
    int count = 1;
    CHAR_DATA *rch;
    char name[MAX_INPUT_LENGTH];        /*  HOPEFULLY no mob has a name longer than THAT */

    static char outbuf[MAX_INPUT_LENGTH];

    if ( !IS_NPC(ch) )
        return ch->names;

    one_argument(ch->names, name);      /* copy the first word into name */

    if ( !name[0] )
    {                           /* weird mob .. no keywords */
        strcpy(outbuf, "");     /* Do not return NULL, just an empty buffer */
        return outbuf;
    }

    for ( rch = ch->in_room->people; rch && (rch != ch );
         rch = rch->next_in_room)
        if ( is_name(name, rch->names) )
            count++;


    sprintf(outbuf, "%d.%s", count, name);
    return outbuf;
}


void
do_for (CHAR_DATA * ch, char *argument)
{
    char range[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    bool fGods = FALSE, fMortals = FALSE, fMobs = FALSE, fEverywhere =
        FALSE, found;
    ROOM_DATA *room, *old_room;
    CHAR_DATA *p;
    CHAR_DATA *p_next;
    int x, y;

    argument = one_argument(argument, range);

    if ( !range[0] || !argument[0] )
    {                           /* invalid usage? */
        do_help(ch, "for");
        return;
    }

    if ( !str_prefix("quit", argument) )
    {
        send_to_char("Are you trying to crash the MUD or something?\r\n",
                     ch);
        return;
    }


    if ( !str_cmp(range, "all") )
    {
        fMortals = TRUE;
        fGods = TRUE;
    }
    else if ( !str_cmp(range, "gods") )
        fGods = TRUE;
    else if ( !str_cmp(range, "mortals") )
        fMortals = TRUE;
    else if ( !str_cmp(range, "mobs") )
        fMobs = TRUE;
    else if ( !str_cmp(range, "everywhere") )
        fEverywhere = TRUE;
    else
        do_help(ch, "for");     /* show syntax */

    /* do not allow # to make it easier */
    if ( fEverywhere && strchr(argument, '#') )
    {
        send_to_char("Cannot use FOR EVERYWHERE with the # thingie.\r\n",
                     ch);
        return;
    }

    if ( strchr(argument, '#') )
    {                           /* replace # ? */
        for ( p = char_list; p; p = p_next )
        {
            p_next = p->next;   /* In case someone DOES try to AT MOBS SLAY # */
            found = FALSE;

            if ( !(p->in_room) || room_is_private(p->in_room) || (p == ch) )
                continue;

            if ( IS_NPC(p) && fMobs )
                found = TRUE;
            else if ( !IS_NPC(p) && p->trust >= 1 && fGods )
                found = TRUE;
            else if ( !IS_NPC(p) && p->trust < 1 && fMortals )
                found = TRUE;

            /* It looks ugly to me.. but it works :) */
            if ( found )
            {                   /* p is 'appropriate' */
                char *pSource = argument;       /* head of buffer to be parsed */
                char *pDest = buf;      /* parse into this */

                while ( *pSource )
                {
                    if ( *pSource == '#' )
                    {           /* Replace # with name of target */
                        const char *namebuf = name_expand(p);

                        if ( namebuf )    /* in case there is no mob name ?? */
                            while ( *namebuf )    /* copy name over */
                                *(pDest++) = *(namebuf++);

                        pSource++;
                    }
                    else
                        *(pDest++) = *(pSource++);
                }               /* while */
                *pDest = '\0';  /* Terminate */

                /* Execute */
                old_room = ch->in_room;
                char_from_room(ch);
                char_to_room(ch, p->in_room);
                interpret(ch, buf);
                char_from_room(ch);
                char_to_room(ch, old_room);

            }                   /* if found */
        }                       /* for every char */
    }
    else
        /* just for every room with the appropriate people in it */
    {
        for ( y = the_city->y_size - 1; y >= 0; y-- )
            for ( x = 0; x < the_city->x_size; x++ )
            {
                room = index_room(the_city->rooms_on_level, x, y);


                found = FALSE;

                /* Anyone in here at all? */
                if ( fEverywhere )        /* Everywhere executes always */
                    found = TRUE;
                else if ( !room->people ) /* Skip it if room is empty */
                    continue;


                /* Check if there is anyone here of the requried type */
                /* Stop as soon as a match is found or there are no more ppl in room */
                for ( p = room->people; p && !found; p = p->next_in_room )
                {

                    if ( p == ch )        /* do not execute on oneself */
                        continue;

                    if ( IS_NPC(p) && fMobs )
                        found = TRUE;
                    else if ( !IS_NPC(p) && (p->trust >= 1) && fGods )
                        found = TRUE;
                    else if ( !IS_NPC(p) && (p->trust < 1) && fMortals )
                        found = TRUE;
                }               /* for everyone inside the room */

                if ( found && !room_is_private(room) )
                {               /* Any of the required type here AND room not private? */
                    /* This may be ineffective. Consider moving character out of old_room
                       once at beginning of command then moving back at the end.
                       This however, is more safe?
                     */

                    old_room = ch->in_room;
                    char_from_room(ch);
                    char_to_room(ch, room);
                    interpret(ch, argument);
                    char_from_room(ch);
                    char_to_room(ch, old_room);
                }               /* if found */
            }                   /* for every room in a bucket */
    }                           /* if strchr */
}                               /* do_for */


void
do_teamstats (CHAR_DATA * ch, char *argument)
{
    int index = 0;
    char buf[MAX_INPUT_LENGTH] = "";

    sprintf(buf, "%-7s %-7s %-5s %-10s\r\n", "Team", "Players", "Score",
            "Leader");
    send_to_char(buf, ch);

    for ( index = 0; index < g_numTeams; index++ )
    {
        sprintf(buf, "%-7s", team_table[index].name);
        send_to_char(buf, ch);
        sprintf(buf, " %-7d", team_table[index].players);
        send_to_char(buf, ch);
        sprintf(buf, " %-5d", team_table[index].score);
        send_to_char(buf, ch);

        if ( team_table[index].teamleader )
        {
            sprintf(buf, " %-10s", team_table[index].teamleader->names);
            send_to_char(buf, ch);
        }
        send_to_char("\r\n", ch);
    }
}


void
do_badpop (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if ( arg[0] == '\0' )
    {
        send_to_char("Cause who to suffer from unlucky teleports?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( IS_NPC(victim) )
    {
        send_to_char("Not on NPCs.\r\n", ch);
        return;
    }

    if ( !IS_SET(victim->act, PLR_BADPOP) )
    {
        SET_BIT(victim->act, PLR_BADPOP);
        act("$N now has bad luck with teleports.\r\n", ch, NULL, victim,
            TO_CHAR);
        return;
    }

    act("$N's curse of bad pops has been lifted.\r\n", ch, NULL, victim,
        TO_CHAR);
    REMOVE_BIT(victim->act, PLR_BADPOP);
    return;
}


void
do_addlag (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;
    char arg1[MAX_STRING_LENGTH];
    int x;

    argument = one_argument(argument, arg1);

    if ( arg1[0] == '\0' )
    {
        send_to_char("Add artificial lag to who?\r\n", ch);
        return;
    }
    if ( (victim = get_char_world(ch, arg1)) == NULL )
    {
        send_to_char("That character isn't online.\r\n", ch);
        return;
    }

    if ( victim->trust >= ch->trust )
    {
        send_to_char
            ("You can only addlag to someone that is below your trust.\r\n",
             ch);
        return;
    }

    if ( (x = atoi(argument)) <= 0 )
    {
        send_to_char("Lag cannot be less than or equal to zero.\r\n", ch);
        return;
    }

    if ( x > 128 )
    {
        send_to_char("Added lag cannot exceed 128.\r\n",
                     ch);
        return;
    }

    WAIT_STATE(victim, x);
    send_to_char("Lag added to target.\r\n", ch);
    return;
}


void
do_gemote (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;

    while ( isspace(*argument) )
        argument++;

    if ( !IS_NPC(ch) && IS_SET(ch->comm, COMM_NOEMOTE | COMM_NOSOCIALS) )
    {
        send_to_char("You can't show your emotions.\r\n", ch);
        return;
    }
    else if ( argument[0] == '\0' )
    {
        send_to_char("Syntax: gemote <message>\r\n", ch);
        return;
    }

    act_new("&BGocial:&n $n $t&n", ch, argument, NULL, TO_CHAR, POS_DEAD);

    for ( victim = char_list; victim != NULL; victim = victim->next )
    {
        if ( IS_SET(victim->comm, COMM_NOBOUNTY) ||
             IS_SET(victim->comm, COMM_QUIET) ||
             IS_SET(victim->comm, COMM_NOSOCIALS) )
            continue;
        act_new("&BGocial:&n $n $t&n", ch, argument, victim, TO_VICT,
                POS_DEAD);
    }
}


void
do_ptell (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *findimm;

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' || argument[0] == '\0' )
    {
        send_to_char("ptell whom what?\r\n", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Can't find em...\r\n", ch);
        return;
    }
    act_new("&R$n&n answers your prayer '&Y$t&n'", ch, argument, victim,
            TO_VICT, POS_DEAD);
    sprintf(buf, "&R%s&n ptells &r%s&n '&Y%s&n'\r\n", ch->names,
            victim->names, argument);

    for ( findimm = char_list; findimm != NULL; findimm = findimm->next )
    {
        if ( IS_SET(findimm->comm, COMM_QUIET) || !get_trust(findimm ) ||
            IS_NPC(findimm) || findimm == victim)
            continue;
        send_to_char(buf, findimm);
    }
}

void
do_basepurge (CHAR_DATA * ch, char *argument)
{
    base_purge();
    send_to_char("Ok.", ch);
}

void
do_nonote (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("Nonote whom?", ch);
        return;
    }

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( get_trust(victim) >= get_trust(ch) )
    {
        send_to_char("You failed.\r\n", ch);
        return;
    }

    if ( IS_SET(victim->act, PLR_NONOTE) )
    {
        REMOVE_BIT(victim->act, PLR_NONOTE);
        send_to_char("The gods have restored your note priviliges.\r\n",
                     victim);
        send_to_char("NONOTE removed.\r\n", ch);
    }
    else
    {
        SET_BIT(victim->act, PLR_NONOTE);
        send_to_char("The gods have revoked your note priviliges.\r\n",
                     victim);
        send_to_char("NONOTE set.\r\n", ch);
    }

    return;
}


/* Coded by Lalmec. */
void
do_ping (CHAR_DATA * ch, char *argument)
{
    char strhost[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( !ch->desc )
    {
        send_to_char("You're not a character.\r\n", ch);
        return;
    }
    else if ( arg[0] == '\0' )
    {
        send_to_char("Ping who?\r\n", ch);
        return;
    }
    else if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }
    else if ( !victim->desc )
    {
        send_to_char("It must have a connection.\r\n", ch);
        return;
    }
    else if ( ch->desc->ping_pipe )
    {
        send_to_char("You're already pinging someone.\r\n", ch);
        return;
    }

    sprintf(strhost, "/bin/ping -n -c 5 %s", victim->desc->host);

    if ( (ch->desc->ping_pipe = popen(strhost, "r")) == NULL )
    {
        send_to_char("Error opening ping_pipe.\r\n", ch);
        return;
    }

    ch->desc->ping_next = pinger_list;
    pinger_list = ch->desc;
    ch->desc->ping_wait = 60;
}


void
do_savehelps (CHAR_DATA * ch, char *argument)
{
    FILE *fp;
    HELP_DATA *pHelp;

    if ( (fp = fopen("../helpfiles.txt", "w")) == NULL )
    {
        if ( ch )
            send_to_char("Error openning file helpfiles.txt", ch);
        return;
    }

    for ( pHelp = help_first; pHelp != NULL; pHelp = pHelp->next )
    {
        fprintf(fp, "%d %s~\n%s~\n\n",
                pHelp->level, pHelp->keyword, pHelp->text);
    }
}


void
do_setscenario (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char curscen_link[MAX_INPUT_LENGTH];
    char tmp[MAX_INPUT_LENGTH];

    one_argument(argument, arg);
    strcpy(curscen_link, CUR_SCEN_DIR);
    *(curscen_link + strlen(curscen_link) - 1) = '\0';

    if ( !*arg )
    {
        struct dirent *de;
        char curscen[256];
        struct stat sbuf;
        DIR *scndir;
        int k = 0;

        if ( (scndir = opendir(SCEN_DIR)) == NULL )
        {
            printf_to_char(ch, "%s: %s\r\n", SCEN_DIR, strerror(errno));
            logerr(SCEN_DIR);
            return;
        }

        if ( (k = readlink(curscen_link, curscen, sizeof(curscen))) == -1 )
        {
            send_to_char("Could not read current scenario link.\r\n", ch);
            logerr(curscen_link);
            return;
        }

        *(curscen + k) = '\0';
        k = sprintf(buf, "The following scenarios exist in %s:\r\n",
                    SCEN_DIR);

        while ( (de = readdir(scndir)) != NULL )
        {
            sprintf(tmp, SCEN_DIR "%s", de->d_name);
            if ( lstat(tmp, &sbuf) == -1 )
            {
                printf_to_char(ch, "%s: %s\r\n", de->d_name,
                               strerror(errno));
                logerr(de->d_name);
                return;
            }
            else if ( *de->d_name == '.' || !S_ISDIR(sbuf.st_mode ) ||
                     S_ISLNK(sbuf.st_mode))
                continue;

            k += sprintf(buf + k, "    %-25s%s\r\n",
                         de->d_name,
                         (!strcmp(curscen, de->d_name) ? " &X(&Y*&X)&n" :
                          ""));
        }

        closedir(scndir);
        send_to_char(buf, ch);
        return;
    }
    else if ( !str_cmp(arg, "default") )
    {
        unlink(curscen_link);

        if ( symlink(DFLT_SCEN_DIR, curscen_link) == -1 )
        {
            send_to_char
                ("Could not reset to default scenario.  You'll have to use the shell to fix it.\r\n",
                 ch);
            logerr(curscen_link);
            return;
        }

        send_to_char("Scenario set to " DFLT_SCEN_DIR
                     " for next boot.\r\n", ch);
        return;
    }

    sprintf(buf, SCEN_DIR "%s" SLASH, arg);

    if ( access(buf, R_OK | X_OK) == -1 )
    {
        printf_to_char(ch, "%s: %s\r\n", buf, strerror(errno));
        logerr(buf);
        return;
    }
    else if ( unlink(curscen_link) == -1 )
    {
        printf_to_char(ch, "%s: %s\r\n", curscen_link, strerror(errno));
        logerr(curscen_link);
        return;
    }
    else if ( symlink(arg, curscen_link) == -1 )
    {
        printf_to_char(ch, "%s: %s\r\n", buf, strerror(errno));
        logerr(buf);

        if ( symlink(DFLT_SCEN_DIR, curscen_link) == -1 )
        {
            send_to_char
                ("Could not reset to default scenario.  You'll have to use the shell to fix it.\r\n",
                 ch);
            logerr(DFLT_SCEN_DIR);
        }

        return;
    }

    printf_to_char(ch, "Scenario set to %s for next boot.\r\n", arg);
}


void
do_listenin (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int i;

    if ( IS_NPC(ch) )
    {
        send_to_char("No.\r\n", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if ( !*arg )
    {
        int k = sprintf(buf, "You are listening to: ");

        for ( i = 0; i < g_numTeams; i++ )
            if ( IS_SET(ch->pcdata->listen_data, (1 << i)) )
                k += sprintf(buf + k, "%s ", team_table[i].who_name);

        strcat(buf, "&n\r\n");
        send_to_char(buf, ch);
        return;
    }

    while ( *arg )
    {
        if ( (i = lookup_team(arg)) == -1 )
        {
            printf_to_char(ch, "What team is %s?!\r\n", arg);
            return;
        }

        if ( IS_SET(ch->pcdata->listen_data, (1 << i)) )
        {
            printf_to_char(ch, "No longer listening to %s&n teamtalk.\r\n",
                           team_table[i].who_name);
            REMOVE_BIT(ch->pcdata->listen_data, (1 << i));
        }
        else
        {
            printf_to_char(ch, "Now listening to %s&n teamtalk.\r\n",
                           team_table[i].who_name);
            SET_BIT(ch->pcdata->listen_data, (1 << i));
        }

        argument = one_argument(argument, arg);
    }
}


void
do_novote (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *vict;
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Novote who?\r\n", ch);
        return;
    }
    else if ( (vict = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("He or she isn't here.\r\n", ch);
        return;
    }
    else if ( vict->trust >= ch->trust )
    {
        send_to_char("You failed.\r\n", ch);
        return;
    }

    if ( IS_SET(vict->act, PLR_NOVOTE) )
    {
        REMOVE_BIT(vict->act, PLR_NOVOTE);
        send_to_char("You have been re-enfranchised.\r\n", vict);
        send_to_char("NOVOTE removed.\r\n", ch);
    }
    else
    {
        SET_BIT(vict->act, PLR_NOVOTE);
        send_to_char("You have been disenfranchised.\r\n", vict);
        send_to_char("NOVOTE set.\r\n", ch);
    }
}


void
do_as (struct char_data *ch, char *argument)
{
    struct descriptor_data *dvict;
    char arg[MAX_INPUT_LENGTH];
    struct char_data *vict;
    PC_DATA *pdata;

    argument = one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Do what, as who?\r\n", ch);
        return;
    }
    else if ( (vict = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Who's that?\r\n", ch);
        return;
    }
    else if ( vict->trust >= ch->trust )
    {
        send_to_char("No.\r\n", ch);
        return;
    }
    else if ( !str_prefix(argument, "delete") )
    {
        send_to_char("No.\r\n", ch);
        return;
    }

    /* Store their old descriptor, set the new one to us. */
    dvict = vict->desc;
    vict->desc = ch->desc;
    pdata = vict->pcdata;
    vict->pcdata = ch->pcdata;

    /* Run the command with us as their descriptor. */
    interpret(vict, argument);

    /* Restore their old descriptor. */
    vict->desc = dvict;
    vict->pcdata = pdata;
}


void
do_zombies (struct char_data *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    struct char_data *zch;
    struct obj_data *obj;
    int count;
    int k = 0;

    one_argument(argument, arg);

    if ( !*arg || !str_prefix(arg, "list") )
    {
        k = sprintf(buf, "The list of zombies in %s&n:\r\n",
                    graveyard_area->name);

        for ( zch = graveyard_area->people; zch; zch = zch->next_in_room )
        {
            if ( !can_see(ch, zch) )
                continue;

            count = 0;
            obj = zch->owned_objs;

            while ( obj )
            {
                count++;
                obj = obj->next_owned;
            }

            k += sprintf(buf + k, "  &Y*&n &m%-32s&X [&c%02d&X]&n\r\n",
                         without_colors(PERS(zch, ch)), count);
        }

        send_to_char(buf, ch);
        return;
    }

    for ( zch = graveyard_area->people; zch; zch = zch->next_in_room )
        if ( can_see(ch, zch) && is_name(arg, zch->names) )
            break;

    if ( !zch )
    {
        send_to_char("Didn't find any zombies named that.\r\n", ch);
        return;
    }

    k = sprintf(buf, "The zombie formerly known as %s&n owns:\r\n",
                PERS(zch, ch));
    count = 0;

    for ( obj = zch->owned_objs; obj; obj = obj->next_owned )
    {
        k += sprintf(buf + k, "  &Y*&n &m%s&n ",
                     without_colors(obj->short_descr));
        count++;

        if ( obj->destination )
        {
            k += sprintf(buf + k,
                         " going to [&uX%d&n, &uX%d&n] L - &uL%d&n\r\n",
                         obj->destination->x, obj->destination->y,
                         obj->destination->level);
        }
        else if ( obj->in_room )
        {
            if ( obj->in_room->level == -1 )
            {
                if ( obj->in_room->interior_of )
                {
                    k += sprintf(buf + k,
                                 " inside %s at [&uX%d&n, &uX%d&n] L - &uL%d&n\r\n",
                                 without_colors(obj->in_room->interior_of->
                                                short_descr),
                                 obj->in_room->interior_of->in_room->x,
                                 obj->in_room->interior_of->in_room->y,
                                 obj->in_room->interior_of->in_room->
                                 level);
                }
                else
                {
                    k += sprintf(buf + k, " at %s&n\r\n",
                                 obj->in_room->name);
                }
            }
            else
            {
                k += sprintf(buf + k,
                             " at [&uX%d&n, &uX%d&n] L - &uL%d&n\r\n",
                             obj->in_room->x, obj->in_room->y,
                             obj->in_room->level);
            }
        }
        else if ( obj->carried_by )
        {
            k += sprintf(buf + k, " carried by %s\r\n",
                         obj->carried_by->names);
        }
    }

    if ( !count )
        strcat(buf, "  &YNothing!&n  Hrm...\r\n");
    else
        sprintf(buf + k, "\r\n%d objects.\r\n", count);

    send_to_char(buf, ch);
}


void
do_statfreeze (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    struct char_data *vict;

    one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Usage: statfreeze <who>\r\n", ch);
        return;
    }
    else if ( (vict = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Ehm?  Who?\r\n", ch);
        return;
    }
    else if ( IS_NPC(vict) )
    {
        send_to_char("NPC's don't have stats, really.\r\n", ch);
        return;
    }
    else if ( vict->trust >= ch->trust )
    {
        send_to_char("No.\r\n", ch);
        return;
    }
    else if ( IS_SET(vict->act, PLR_FREEZE_STATS) )
    {
        send_to_char("Stats unfrozen.\r\n", ch);
        REMOVE_BIT(vict->act, PLR_FREEZE_STATS);
        return;
    }

    send_to_char("Stats frozen.\r\n", ch);
    SET_BIT(vict->act, PLR_FREEZE_STATS);
}


void
do_slay (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *vict;
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    one_argument(argument, arg);

    if ( (vict = get_char_room(ch, arg)) != NULL )
    {
        if ( vict->trust > ch->trust )
        {
            send_to_char
                ("Yea right pal.  Slay someone less than your trust level.\r\n",
                 ch);
            return;
        }

        if ( IS_NPC(vict ) &&
            (!str_cmp(vict->where_start, "\"god-store\"") ||
             vict == enforcer))
        {
            sprintf(buf,
                    "You can't slay %s, the mud would have a heart-attack!\r\n",
                    vict->short_descript);
            send_to_char(buf, ch);
            return;
        }

        act("$n &uacrams&n &uOa grenade&n down $N's throat.  You are covered in &Rblood&n.", ch, NULL, vict, TO_NOTVICT);
        act("You &uacram&n &uOa grenade&n down $N's throat.  You are covered in &Rblood&n.", ch, NULL, vict, TO_CHAR);
        act("$n &uacrams&n &uOa grenade&n down your throat.  You feel a tremendous\r\n" "&Rpain&n in your stomach before you become a pasty film coverring everything\r\n" "in the room.\r\n", ch, NULL, vict, TO_VICT);

        vict->last_hit_by = ch;
        char_death(vict, "slayed");
    }
    else
        send_to_char("They aren't here.\n\r", ch);
}


void
do_makechar (CHAR_DATA * ch, char *argument)
{
    static PC_DATA pcdata_zero;

    char name[MAX_INPUT_LENGTH];
    char passwd[MAX_INPUT_LENGTH];
    char sex[MAX_INPUT_LENGTH];
    CHAR_DATA *newbie;
    char *pwd;
    int geni;

    CHAR_DATA *alloc_char();
    PC_DATA *alloc_pcdata();
    void set_default_pcdata(CHAR_DATA *);
    void set_default_colors(CHAR_DATA *);
    void set_char_defaults(CHAR_DATA *);

    argument = one_argument_pcase(argument, name);
    argument = one_argument_pcase(argument, passwd);
    argument = one_argument(argument, sex);

    if ( !*name || !*passwd || !*sex || !*argument )
    {
        send_to_char
            ("Usage: makechar <name> <passwd> <sex> <comment/e-mail>\r\n\n"
             "The <comment> field is required. Use it to identify who you created the\r\n"
             "character for (like an AOLer's e-mail address).\r\n", ch);
        return;
    }
    else if ( player_exists(name) )
    {
        send_to_char("A player by that name already exists.\r\n", ch);
        return;
    }

    if ( !str_prefix(sex, "male") )
        geni = SEX_MALE;
    else if ( !str_prefix(sex, "female") )
        geni = SEX_FEMALE;
    else if ( !str_prefix(sex, "neuter") )
        geni = SEX_NEUTRAL;
    else
    {
        send_to_char("What kinda' gender is that, genius?\r\n", ch);
        return;
    }

    pwd = crypt(passwd, passwd);

    if ( strchr(pwd, '~') )
    {
        send_to_char("That's an invalid password.\r\n", ch);
        return;
    }

    newbie = alloc_char();
    clear_char(newbie);
    newbie->pcdata = alloc_pcdata();
    *newbie->pcdata = pcdata_zero;
    newbie->desc = NULL;

    newbie->names = str_dup(name);
    set_char_defaults(newbie);
    set_default_colors(newbie);
    set_default_pcdata(newbie);

    newbie->sex = geni;
    newbie->pcdata->password = str_dup(pwd);
    newbie->pcdata->created_site = str_dup("localhost");
    set_title(newbie, "is &YFRESH MEAT&n");
    newbie->kills = 0;
    newbie->team = -1;

    logmesg("%s created newbie '%s': %s", ch->names, newbie->names,
               argument);
    save_char_obj(newbie);
    free_char(newbie);

    send_to_char("OK.\r\n", ch);
}


void
do_rename (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *vict;
    bool extract_them = false;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if ( !*arg1 || !*arg2 )
    {
        send_to_char("Syntax: rename <old_name> <new_name>\r\n", ch);
        return;
    }
    else if ( player_exists(arg2) )
    {
        send_to_char("A player by that name already exists.\r\n", ch);
        return;
    }
    else if ( !(vict = char_file_active(arg1)) )
    {
        if ( !(vict = load_char_obj(NULL, arg1)) )
        {
            send_to_char("Nobody by that name exists.\r\n", ch);
            return;
        }

        extract_them = true;
        char_to_room(vict, safe_area);
    }
    else if ( get_trust(ch) <= get_trust(vict) )
    {
        send_to_char("You failed.\r\n", ch);
        return;
    }

    free_string(vict->names);
    vict->names = str_dup(capitalize(arg2));
    save_char_obj(vict);

    if ( extract_them )
        extract_char(vict, TRUE);

    sprintf(buf, "You are now %s.\r\n", capitalize(arg2));
    send_to_char(buf, vict);

    if ( ch != vict )
        send_to_char("Renamed.\r\n", ch);

    backup_pfile(arg1);
    rename_pfile(arg1, arg2);
}


void
do_topology (CHAR_DATA * ch, char *argument)
{
    register struct room_data *room;
    char buf[MAX_STRING_LENGTH * 2];
    char arg[MAX_INPUT_LENGTH];
    struct level_data *lvl;
    register char *ptr;
    register int y, x;

    extern struct level_data *the_city;

    one_argument(argument, arg);

    if ( !*arg )
        x = ch->in_room->level;
    else if ( isdigit(*arg) )
        x = atoi(arg);
    else
    {
        send_to_char("Usage: topology [level]\r\n", ch);
        return;
    }

    for ( lvl = the_city; x > 0 && lvl; lvl = lvl->level_down )
        x--;

    if ( x < 0 || lvl == NULL )
    {
        send_to_char("Topological data not available for that level.\r\n",
                     ch);
        return;
    }

    ptr = buf;
    ptr +=
        sprintf(buf, "Topology of Level %d\r\n\r\n ", lvl->level_number);

    /* Draw very top of topological map. */
    for ( x = 0; x < lvl->x_size; x++ )
    {
        *(ptr++) = '_';
        *(ptr++) = '_';
    }

    /* Overwrite trailing _ for appearance purposes. */
    ptr--;

    /* Draw the inside of the map. */
    for ( y = lvl->y_size - 1; y > 0; y-- )
    {
        room = index_room(lvl->rooms_on_level, 0, y);
        *(ptr++) = '\r';
        *(ptr++) = '\n';
        *(ptr++) = '|';

        for ( x = 0; x < lvl->x_size; x++ )
        {
            if ( ch->in_room == room )
            {
                *(ptr++) = '&';
                *(ptr++) = 'Y';
                *(ptr++) = '*';
                *(ptr++) = '&';
                *(ptr++) = 'n';
            }
            else
            {
                if ( ch->trust && (!room->exit[DIR_UP] ||
                                   !room->exit[DIR_DOWN]) )
                {
                    *(ptr++) = '&';
                    *(ptr++) = 'W';
                }

                *(ptr++) = (room->exit[DIR_SOUTH] ? '_' : '.');

                if ( ch->trust && (!room->exit[DIR_UP] ||
                                   !room->exit[DIR_DOWN]) )
                {
                    *(ptr++) = '&';
                    *(ptr++) = 'n';
                }
            }

            *(ptr++) = (room->exit[DIR_EAST] ? '|' : ' ');
            room++;
        }
    }

    /* Draw the very bottom of the map. */
    *(ptr++) = '\r';
    *(ptr++) = '\n';
    *(ptr++) = '|';
    room = lvl->rooms_on_level;

    for ( x = 0; x < lvl->x_size; x++ )
    {
        if ( ch->in_room == room )
        {
            *(ptr++) = '&';
            *(ptr++) = 'Y';
            *(ptr++) = '*';
            *(ptr++) = '&';
            *(ptr++) = 'n';
        }
        else
        {
            if ( ch->trust && (!room->exit[DIR_UP] || !room->exit[DIR_DOWN]) )
            {
                *(ptr++) = '&';
                *(ptr++) = 'W';
            }

            *(ptr++) = '_';

            if ( ch->trust && (!room->exit[DIR_UP] || !room->exit[DIR_DOWN]) )
            {
                *(ptr++) = '&';
                *(ptr++) = 'n';
            }
        }

        *(ptr++) = (room->exit[DIR_EAST] ? '|' : '_');
        room++;
    }

    *(ptr++) = '\r';
    *(ptr++) = '\n';
    *ptr = '\0';

    send_to_char(buf, ch);
}


void
do_dub (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    struct char_data *vict;

    one_argument(argument, arg);

    if ( !*arg )
    {
        if ( ch->dubbing )
        {
            act("You stop dubbing $N.", ch, NULL, ch->dubbing, TO_CHAR);
            ch->dubbing = NULL;
            return;
        }

        send_to_char("&WUsage&X: &cDUB &X<&nvictim&X>&n\r\n", ch);
        return;
    }
    else if ( !(vict = get_char_world(ch, arg)) )
    {
        send_to_char("Who's that?\r\n", ch);
        return;
    }
    else if ( vict->trust >= ch->trust )
    {
        send_to_char("No.\r\n", ch);
        printf_to_char(vict, "%s tried to dub you.\r\n", ch->names);
        return;
    }

    if ( ch->dubbing )
        act("You stop dubbing $N.", ch, NULL, ch->dubbing, TO_CHAR);

    ch->dubbing = vict;
    act("You begin dubbing $N.", ch, NULL, ch->dubbing, TO_CHAR);
}


void
do_whack (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    struct char_data *vict;

    one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("&WUsage&X: &cWHACK &X<&nvictim&X>\r\n", ch);
        return;
    }
    else if ( !(vict = get_char_world(ch, arg)) )
    {
        send_to_char("Who's that?\r\n", ch);
        return;
    }
    else if ( vict == ch )
    {
        send_to_char("...it's not that kind of 'whack'.\r\n", ch);
        return;
    }
    else if ( IS_NPC(vict) )
    {
        send_to_char("You can't whack an NPC.\r\n", ch);
        return;
    }
    else if ( vict->trust >= ch->trust )
    {
        send_to_char("No.\r\n", ch);
        printf_to_char(vict, "%s tried to whack you.\r\n", ch->names);
        return;
    }
    else if ( !g_forceAction )
    {
        printf_to_char(ch,
                       "Careful!  You cannot undo deletion!\r\n"
                       "Use &cWHACK %s!&n if you're absolutely sure "
                       "this is what you want to do.\r\n", arg);
        return;
    }

    act("You load a &Ws&Xi&Wl&Xv&We&Xr&n bullet into your gat and take aim at $N...", ch, NULL, vict, TO_CHAR);
    act("$n loads a &Ws&Xi&Wl&Xv&We&Xr&n bullet into his gat and takes aim at you...", ch, NULL, vict, TO_VICT);

    if ( vict->desc )
        close_socket(vict->desc);

    backup_pfile(vict->names);
    remove_pfile(vict->names);
    extract_char(vict, TRUE);
}
