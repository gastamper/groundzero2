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

#include    <stddef.h>
#include    "ground0.h"
#include    "ansi.h"
#include    "db.h"
#include    "telnet.h"

/* command procedures needed */
DECLARE_DO_FUN (do_exits);
DECLARE_DO_FUN (do_look);
DECLARE_DO_FUN (do_help);
DECLARE_DO_FUN (do_teamtalk);
void scan_direction(CHAR_DATA *, ROOM_DATA *, int, int, bool);


char *const where_name[] = {
    "<used as light>     ",
    "<worn on finger>    ",
    "<worn on finger>    ",
    "<worn around neck>  ",
    "<worn around neck>  ",
    "<worn on body>      ",
    "<worn on head>      ",
    "<worn on legs>      ",
    "<worn on feet>      ",
    "<worn on hands>     ",
    "<worn on arms>      ",
    "<worn as shield>    ",
    "<worn about body>   ",
    "<worn about waist>  ",
    "<worn around wrist> ",
    "<worn around wrist> ",
    "<wielded>           ",
    "<second hand>       ",
    "<held>              "
};


/* for do_count */
int max_on = 0;



/*
 * Local functions.
 */
char *format_obj_to_char
args ((OBJ_DATA * obj, CHAR_DATA * ch, bool fShort));
void show_list_to_char
args ((OBJ_DATA * list, CHAR_DATA * ch, bool fShort, bool fShowNothing));
void show_char_to_char_0 args((CHAR_DATA * victim, CHAR_DATA * ch));
void show_char_to_char_1 args((CHAR_DATA * victim, CHAR_DATA * ch));
void show_char_to_char args((CHAR_DATA * list, CHAR_DATA * ch));
void show_exits
args ((ROOM_DATA * the_room, CHAR_DATA * ch, char *argument));
bool check_blind (CHAR_DATA * ch, bool);
void send_centerred args((char *argument, CHAR_DATA * ch));

char *
format_obj_to_char (OBJ_DATA * obj, CHAR_DATA * ch, bool fShort)
{
    static char buf[MAX_STRING_LENGTH];

    buf[0] = '\0';

    if ( fShort )
    {
        if ( obj->short_descr != NULL )
            strcat(buf, obj->short_descr);
    }
    else
    {
        if ( obj->description != NULL )
            strcat(buf, obj->description);
    }

    return buf;
}


/*
 * Scan in a single direction for as far as we can.  This is more efficient
 * than the old GZ scan code, so I should use for do_scan() there.  Stuff
 * like this -- written with a slight bit of sense and an elementrary basis
 * in mathematics -- should eventually replace most of Randar's hacks.  Some
 * of Randar's code lends strong credibility to the claim that he coded
 * mostly while intoxicated...
 *
 * -satori
 */

void
scan_direction (struct char_data *ch, struct room_data *rm, int dist,
               int dir, bool autoscan)
{
    char buf[MAX_STRING_LENGTH];/* What to send.              */
    int i;                      /* Iterator distance.         */
    struct char_data *ich;      /* Iterator character.        */
    struct obj_data *iobj;      /* Iterator object.           */

    for ( i = 1; i <= dist; i++ )
    {
        if ( rm->exit[dir] )
            break;

        rm = index_room(rm, dir_cmod[dir][0], dir_cmod[dir][1]);

        for ( ich = rm->people; ich; ich = ich->next_in_room )
            if ( can_see(ch, ich) )
            {
                sprintf(buf, "%s%s%s&n is %d %s.\r\n",
                        (autoscan ? "- " : ""),
                        team_table[ich->team].namecolor, PERS(ich, ch),
                        i, dir_name[dir]);
                send_to_char(buf, ch);
            }

        for ( iobj = rm->contents; iobj; iobj = iobj->next_content )
            if (iobj->item_type == ITEM_TEAM_VEHICLE ||
                iobj->item_type == ITEM_TEAM_ENTRANCE)
            {
                sprintf(buf, "%s&uO%s&n is %d %s.\r\n",
                        (autoscan ? "&X- " : ""),
                        iobj->short_descr, i,
                        dir_name[dir]);
                send_to_char(buf, ch);
            }
    }
}


void
do_scan (CHAR_DATA * ch, char *argument)
{
    if ( ch->in_room->interior_of || ch->in_room->inside_mob )
    {
        exterior_shell(ch, do_scan(ch, ""));
        return;
    }

    send_to_char("You scan your surroundings and see...\r\n\r\n", ch);
    scan_direction(ch, ch->in_room, max_sight(ch), DIR_NORTH, false);
    scan_direction(ch, ch->in_room, max_sight(ch), DIR_EAST, false);
    scan_direction(ch, ch->in_room, max_sight(ch), DIR_SOUTH, false);
    scan_direction(ch, ch->in_room, max_sight(ch), DIR_WEST, false);
}


/*
 * Show a list to a character.
 * Can coalesce duplicated items.
 */

void
show_list_to_char (OBJ_DATA * list, CHAR_DATA * ch, bool fShort,
                  bool fShowNothing)
{
#define MAX_ITEMS_IN_ROOM 500
#define MAX_LENGTH_ITEM_DESC 200
    char buf[MAX_STRING_LENGTH];
    char prgpstrShow[MAX_ITEMS_IN_ROOM][MAX_LENGTH_ITEM_DESC];
    int prgnShow[MAX_ITEMS_IN_ROOM];
    char pstrShow[MAX_LENGTH_ITEM_DESC];
    OBJ_DATA *obj;
    int nShow;
    int iShow;
    int count;
    bool fCombine;

    if ( ch->desc == NULL )
        return;

    /*
     * Alloc space for output lines.
     */
    count = 0;
    for ( obj = list; obj != NULL; obj = obj->next_content )
    {
        if ( count > 10000 )
        {
            send_to_char("Infinite object list, report this bug.\r\n", ch);
            logmesg("skipping infinite obj list (showed to %s).",
                       ch->names);
            return;
        }
        count++;
    }
    nShow = 0;

    /*
     * Format the list of objects.
     */
    for ( obj = list; obj != NULL; obj = obj->next_content )
    {
        if ( obj->wear_loc == WEAR_NONE )
        {
            strcpy(pstrShow, format_obj_to_char(obj, ch, fShort));
            fCombine = FALSE;

            if ( IS_NPC(ch) || IS_SET(ch->comm, COMM_COMBINE) )
            {
                /*
                 * Look for duplicates, case sensitive.
                 * Matches tend to be near end so run loop backwords.
                 */
                for ( iShow = nShow - 1; iShow >= 0; iShow-- )
                {
                    if ( !strcmp(prgpstrShow[iShow], pstrShow) )
                    {
                        prgnShow[iShow]++;
                        fCombine = TRUE;
                        break;
                    }
                }
            }

            /*
             * Couldn't combine, or didn't want to.
             */
            if ( !fCombine )
            {
                strcpy(prgpstrShow[nShow], str_dup(pstrShow));
                prgnShow[nShow] = 1;
                nShow++;
            }
        }
    }

    /*
     * Output the formatted list.
     */
    for ( iShow = 0; iShow < nShow; iShow++ )
    {
        if ( IS_NPC(ch) || IS_SET(ch->comm, COMM_COMBINE) )
        {
            if ( prgnShow[iShow] != 1 )
            {
                sprintf(buf, "(%2d) ", prgnShow[iShow]);
                send_to_char(buf, ch);
            }
            else
            {
                send_to_char("     ", ch);
            }
        }
        send_to_char(prgpstrShow[iShow], ch);
        send_to_char("\r\n", ch);
    }

    if ( fShowNothing && nShow == 0 )
    {
        if ( IS_NPC(ch) || IS_SET(ch->comm, COMM_COMBINE) )
            send_to_char("     ", ch);
        send_to_char("Nothing.\r\n", ch);
    }

    return;
}



void
show_char_to_char_0 (CHAR_DATA * victim, CHAR_DATA * ch)
{
    char buf[MAX_STRING_LENGTH];

    buf[0] = '\0';

    if ( IS_SET(victim->act, PLR_WIZINVIS) )
        strcat(buf, "(&uwWizi&n) ");

    strcat(buf, PERS(victim, ch));
    if ( !IS_NPC(victim) && !IS_SET(ch->comm, COMM_BRIEF) )
        strcat(buf, victim->pcdata->title_line);

    strcat(buf, "&n");

    switch (victim->position)
    {
        case POS_DEAD:
            strcat(buf, " is DEAD!!");
            break;
        case POS_MORTAL:
            strcat(buf, " is mortally wounded.");
            break;
        case POS_INCAP:
            strcat(buf, " is incapacitated.");
            break;
        case POS_STUNNED:
            strcat(buf, " is lying here stunned.");
            break;
        case POS_SLEEPING:
            strcat(buf, " is sleeping here.");
            break;
        case POS_RESTING:
            strcat(buf, " is resting here.");
            break;
        case POS_SITTING:
            strcat(buf, " is sitting here.");
            break;
        case POS_STANDING:
            strcat(buf, " is here.");
            break;
        case POS_FIGHTING:
            strcat(buf, " is here, fighting ");
            if ( victim->fighting == NULL )
                strcat(buf, "thin air??");
            else if ( victim->fighting == ch )
                strcat(buf, "YOU!");
            else if ( victim->in_room == victim->fighting->in_room )
            {
                strcat(buf, PERS(victim->fighting, ch));
                strcat(buf, ".");
            }
            else
                strcat(buf, "somone who left??");
            break;
    }

    strcat(buf, "\r\n");
    buf[0] = UPPER(buf[0]);
    send_to_char(buf, ch);
    return;
}


char *
diagnose_simple (struct char_data *vict)
{
    int perc = PERCENT(vict->hit, vict->max_hit);
    static char *i = "bleeding to death";

    if ( perc >= 100 )
        i = "excellent condition";
    else if ( perc >= 90 )
        i = "a few scratches";
    else if ( perc >= 75 )
        i = "small wounds and bruises";
    else if ( perc >= 50 )
        i = "quite a few wounds";
    else if ( perc >= 30 )
        i = "big nasty wounds";
    else if ( perc >= 15 )
        i = "pretty hurt";
    else if ( perc >= 0 )
        i = "awful condition";

    return i;
}


char *
diagnose (struct char_data *vict)
{
    int perc = PERCENT(vict->hit, vict->max_hit);
    static char *i = " is bleeding to death.";

    if ( perc >= 100 )
        i = " is in excellent condition.";
    else if ( perc >= 90 )
        i = " has a few scratches.";
    else if ( perc >= 75 )
        i = " has some small wounds and bruises.";
    else if ( perc >= 50 )
        i = " has quite a few wounds.";
    else if ( perc >= 30 )
        i = " has some big nasty wounds and scratches.";
    else if ( perc >= 15 )
        i = " looks pretty hurt.";
    else if ( perc >= 0 )
        i = " is in awful condition.";

    return i;
}


void
show_char_to_char_1 (CHAR_DATA * victim, CHAR_DATA * ch)
{
    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    int iWear;
    bool found;

    if ( can_see(victim, ch) )
    {
        if ( ch == victim )
            act("$n looks at $mself.", ch, NULL, NULL, TO_ROOM);
        else
        {
            act("$n looks at you.", ch, NULL, victim, TO_VICT);
            act("$n looks at $N.", ch, NULL, victim, TO_NOTVICT);
        }
    }

    act("$E is ready to kill you like anyone else you would find here.\r\n"
        "But other than that . . .", ch, NULL, victim, TO_CHAR);

    sprintf(buf, "%s%s", PERS(victim, ch), diagnose(victim));
    buf[0] = UPPER(buf[0]);
    send_to_char(buf, ch);

    found = FALSE;
    for ( iWear = 0; iWear < MAX_WEAR; iWear++ )
    {
        if ( (obj = get_eq_char(victim, iWear)) != NULL )
        {
            if ( !found )
            {
                send_to_char("\r\n", ch);
                act("$N is using:", ch, NULL, victim, TO_CHAR);
                found = TRUE;
            }
            send_to_char(where_name[iWear], ch);
            send_to_char(format_obj_to_char(obj, ch, TRUE), ch);
            send_to_char("\r\n", ch);
        }
    }


    if ( IS_IMMORTAL(ch) )
    {
        send_to_char("\r\nYou peek at the inventory:\r\n", ch);
        show_list_to_char(victim->carrying, ch, TRUE, TRUE);
    }
    return;
}



void
show_char_to_char (CHAR_DATA * list, CHAR_DATA * ch)
{
    CHAR_DATA *rch;

    for ( rch = list; rch != NULL; rch = rch->next_in_room )
    {
        if ( rch == ch || !rch->names )
            continue;

        if ( !IS_NPC(rch )
            && IS_SET(rch->act, PLR_WIZINVIS)
            && get_trust(ch) < rch->invis_level)
            continue;

        if ( can_see(ch, rch) )
            show_char_to_char_0(rch, ch);
    }

    return;
}

bool
check_blind (CHAR_DATA * ch, bool mesg)
{
    OBJ_DATA *prot_obj;

    if ( !IS_NPC(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
        return TRUE;

    if ( IS_SET(ch->affected_by, AFF_BLIND) )
    {
        if ( mesg )
            send_to_char("You can't see a thing!\r\n", ch);
        return FALSE;
    }

    if ( IS_SET(ch->in_room->room_flags, ROOM_DARK) &&
         ((prot_obj = get_eq_char(ch, WEAR_HEAD)) == NULL ||
          !IS_SET(prot_obj->general_flags, GEN_SEE_IN_DARK)) )
    {
        if ( mesg )
            send_to_char("The room is too dark to see anything.\r\n", ch);
        return FALSE;
    }

    return TRUE;
}


/* RT does socials */
void
do_socials (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int iSocial;
    int col = 0;
    int k = 0;

    *buf = '\0';

    for ( iSocial = 0; social_table[iSocial].name; iSocial++ )
    {
        k += sprintf(buf + k, "%-12s%s", social_table[iSocial].name,
                     !(++col % 6) ? "\r\n" : "");

        if ( k >= MAX_STRING_LENGTH - 128 )
        {
            page_to_char(buf, ch);
            *buf = '\0';
            k = 0;
        }
    }

    if ( col % 6 != 0 )
    {
        strcat(buf, "\r\n");
        k += 2;
    }

    sprintf(buf + k, "%d socials in all!\r\n", iSocial);
    page_to_char(buf, ch);
}

void
send_centerred (char *argument, CHAR_DATA * ch)
{
    char buf[MAX_INPUT_LENGTH];
    int count, count2, size = strlen(argument);

    for ( count = 0; count < 40 - size / 2 - size % 2; count++ )
        buf[count] = ' ';
    for ( count2 = 0; argument[count2]; count++, count2++ )
        buf[count] = argument[count2];
    buf[count++] = '\n';
    buf[count++] = '\r';
    buf[count] = 0;
    send_to_char(buf, ch);
    return;
}

/* RT Commands to replace news, motd, imotd, etc from ROM */

void
do_motd (CHAR_DATA * ch, char *argument)
{
    do_help(ch, "motd");
}

void
do_imotd (CHAR_DATA * ch, char *argument)
{
    do_help(ch, "imotd");
}

void
do_rules (CHAR_DATA * ch, char *argument)
{
    do_help(ch, "rules");
}

void
do_changes (CHAR_DATA * ch, char *argument)
{
    do_help(ch, "changes");
}

void
do_credits (CHAR_DATA * ch, char *argument)
{
    do_help(ch, "credits");
}


void
do_wizlist (CHAR_DATA * ch, char *argument)
{
    int count;

    send_centerred("The Admins", ch);
    send_centerred("----------", ch);
    for ( count = 0; imp_table[count].rl_name[0]; count++ )
        if ( !imp_table[count].honorary )
            send_centerred(imp_table[count].game_names, ch);
    send_to_char("\r\n", ch);
    return;
}


const int default_palette[MAX_PALETTE_ENTS] = {
    0x000000,
    0xaa0000,
    0x00aa00,
    0xaa5500,
    0x0000aa,
    0xaa00aa,
    0x00aaaa,
    0xaaaaaa,
    0x555555,
    0xff5555,
    0x55ff55,
    0xffff55,
    0x5555ff,
    0xff55ff,
    0x55ffff,
    0xffffff
};

const char *palette_codes[MAX_PALETTE_ENTS] = {
    "\x1B[0;30m",
    "\x1B[0;31m",
    "\x1B[0;32m",
    "\x1B[0;33m",
    "\x1B[0;34m",
    "\x1B[0;35m",
    "\x1B[0;36m",
    "\x1B[0;37m",
    "\x1B[1;30m",
    "\x1B[1;31m",
    "\x1B[1;32m",
    "\x1B[1;33m",
    "\x1B[1;34m",
    "\x1B[1;35m",
    "\x1B[1;36m",
    "\x1B[1;37m"
};

void
do_palette (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char *endptr;
    int num;
    int k;

    argument = one_argument(argument, arg);

    if ( !ch->pcdata || !ch->desc || ch->desc->ttype != TTYPE_LINUX )
    {
        send_to_char("You need the LINUX ttype to do this.\r\n", ch);
        return;
    }
    else if ( !*arg )
    {
        k = sprintf(buf, "Your palette looks like this:\r\n");
        for ( num = 0; num < MAX_PALETTE_ENTS; num++ )
            k += sprintf(buf + k, "%2d. %s%06x\x1B[m\r\n",
                         num, palette_codes[num],
                         ch->pcdata->palette[num]);
        strcat(buf,
               "Use &cPALETTE RESET&n to set the palette to defaults.\r\n"
               "Use &cPALETTE SET <#> <rrggbb>&n to set a color.\r\n");
        send_to_char(buf, ch);
        return;
    }
    else if ( !str_prefix(arg, "reset") )
    {
        for ( num = 0; num < MAX_PALETTE_ENTS; num++ )
            ch->pcdata->palette[num] = default_palette[num];
        send_to_char("\x1B]R\x1B[H\x1B[JPalette reset.\r\n", ch);
        return;
    }
    else if ( !str_prefix(arg, "use") )
    {
        for ( k = 0, num = 0; num < MAX_PALETTE_ENTS; num++ )
            k += sprintf(buf + k, "\x1B]P%x%06x", num,
                         ch->pcdata->palette[num]);
        strcat(buf, "Palette set.\r\n");
        send_to_char(buf, ch);
        return;
    }
    else if ( str_prefix(arg, "set") )
    {
        send_to_char("USAGE: palette [reset|set <#> <rrggbb>]\r\n"
                     "Use PALETTE without arguments to see your current palette.\r\n",
                     ch);
        return;
    }

    argument = one_argument(argument, arg);
    num = strtol(arg, &endptr, 10);

    if ( !*arg || *endptr )
    {
        printf_to_char(ch,
                       "The first argument must be a palette entry (a number between "
                       "between 0 and %d).\r\n", MAX_PALETTE_ENTS);
        return;
    }

    one_argument(argument, arg);
    k = strtol(arg, &endptr, 16);

    if ( !*arg || *endptr )
    {
        send_to_char
            ("The second argument must a set of three hexidecimal doubles"
             "(e.g, AA55CC).\r\n", ch);
        return;
    }

    ch->pcdata->palette[num] = k;
    printf_to_char(ch, "\x1B]P%x%06x%sPalette set.\x1B[m\r\n",
                   num, ch->pcdata->palette[num], palette_codes[num]);
}


void
do_ansi (struct char_data *ch, char *argument)
{
    send_to_char("Use &cTTYPE&n to enable/disable color.\r\n"
                 "Use &cCOLORSET&n to configure your color scheme.\r\n", ch);
}


void
do_colorset (struct char_data *ch, char *argument)
{
    const char *ampCode = "xrgybmcwXRGYBMCW";

    const struct
    {
        const char *name;
        size_t off;
        char uc;
    }
    ColorSettings[] =
    {
        { "combat", offsetof(struct pc_data, color_combat_o), 'A' },
        { "action", offsetof(struct pc_data, color_action), 'a' },
        { "wizi", offsetof(struct pc_data, color_wizi), 'w' },
        { "coordinates", offsetof(struct pc_data, color_xy), 'X' },
        { "level", offsetof(struct pc_data, color_level), 'L' },
        { "hp", offsetof(struct pc_data, color_hp), 'H' },
        { "descriptions", offsetof(struct pc_data, color_desc), 'D' },
        { "objects", offsetof(struct pc_data, color_obj), 'O' },
        { "say", offsetof(struct pc_data, color_say), 's' },
        { "tell", offsetof(struct pc_data, color_tell), 't' },
        { "reply", offsetof(struct pc_data, color_reply), 'r' },
        { "exits", offsetof(struct pc_data, color_exits), 'E' },
        { "scondition", offsetof(struct pc_data, color_combat_condition_s), 'C' },
        { "ocondition", offsetof(struct pc_data, color_combat_condition_o), 'c' },
        { NULL, 0 }
    };

    char arg[MAX_INPUT_LENGTH];
    int color;

    argument = one_argument(argument, arg);

    if ( IS_NPC(ch) )
    {
        send_to_char("No mobiles allowed.\r\n", ch);
        return;
    }
    else if ( !*arg )
    {
        char buf[MAX_STRING_LENGTH];
        int k = 0, i;

        for ( i = 0; ColorSettings[i].name; i++ )
        {
            k += sprintf(buf+k, "&u%c%-16s%s",
                         ColorSettings[i].uc,
                         ColorSettings[i].name,
                         (((i+1) % 3) == 0) ? "\r\n" : "   ");
        }

        if ( i % 3 )
            strcat(buf, "\r\n");

        send_to_char(buf, ch);

        send_to_char("\r\n&nUse &cTTYPE&n to enable/disable color.\r\n"
                     "&BUSAGE&X: &Wcolorset &X<&wname&X> <&wcolor&X>&n\r\n"
                     "<color> is an && code.\r\n", ch);
        return;
    }

    for ( color = 0; ColorSettings[color].name; color++ )
        if ( !str_prefix(arg, ColorSettings[color].name) )
            break;

    if ( ColorSettings[color].name == NULL )
    {
        send_to_char("That's not a valid color setting.\r\n", ch);
        return;
    }

    /* Got a valid color to set, let's get the color code. */
    one_word(argument, arg);
    char p = *arg;

    if ( p == '\0' )
    {
        send_to_char("&BUSAGE&X: &Wcolorset &X<&wname&X> <&wcolor&X>&n\r\n"
                     "<color> is an && code.\r\n", ch);
        return;
    }

    /* Let them use a full &-qualified code, but don't require it. */
    if ( p == '&' )
        p = arg[1];

    int idx = strchr(ampCode, p) - ampCode;

    if ( idx < 0 )
    {
        send_to_char("That's not a valid color.  See &cHELP COLOR&n.\r\n", ch);
        return;
    }

    indirect(ch->pcdata, ColorSettings[color].off, byte) = (byte) idx;

    printf_to_char(ch, "Color set for &u%c%s&n set.\r\n",
                   ColorSettings[color].uc,
                   ColorSettings[color].name);
}


void
do_ttype (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int i;

    extern const char *ttypes[];

    if ( !ch->pcdata || !ch->desc )
    {
        send_to_char("Ix-nay on the TTYPE command, foo'. "
                     "You ain't a playa'.\r\n", ch);
        return;
    }

    one_argument(argument, arg);

    if ( !*arg )
    {
        char buf[MAX_STRING_LENGTH];
        register int k;

        k = sprintf(buf,
                    "Your current terminal type is &W%s%s%s&n.  "
                    "It may be one of the following:\r\n",
                    ch->pcdata->ttype == -1 ? "AUTO (" : "",
                    ttypes[ch->desc->ttype],
                    ch->pcdata->ttype == -1 ? ")" : "");

        for ( i = 0; ttypes[i]; i++ )
            k += sprintf(buf + k, "  %s\r\n", ttypes[i]);

        strcat(buf, "or &cAUTO&n to default to autodetected mode.\r\n");
        send_to_char(buf, ch);
        return;
    }
    else if ( !str_prefix(arg, "auto") )
    {
        ch->pcdata->ttype = -1;
        ch->desc->ttype = TTYPE_ANSI;

        send_to_char
            ("Your terminal type will now be autodetected on login...\r\n",
             ch);

        arg[0] = TELQUAL_SEND;
        IAC_subneg(ch->desc, TELOPT_TTYPE, arg, 1);
        return;
    }

    for ( i = 0; ttypes[i]; i++ )
        if ( !str_prefix(arg, ttypes[i]) )
            break;

    if ( !ttypes[i] )
    {
        send_to_char
            ("Unrecognized terminal type.  See &cHELP TTYPE&n.\r\n", ch);
        return;
    }

    ch->desc->ttype = ch->pcdata->ttype = i;
    printf_to_char(ch, "Terminal type set to &W%s&n.\r\n", ttypes[i]);
}


inline bool
p_tweak_bot (struct char_data *ch)
{
    /* Tweak a bot every so often, just to fuck it up. */
    return ( ch->desc && IS_SET(ch->act, PLR_SUSPECTED_BOT) &&
             !number_range(0, 3) );
}


void
show_battlemap (CHAR_DATA * ch)
{
    int x, y;
    ROOM_DATA *the_room;
    OBJ_DATA *obj;
    CHAR_DATA *ich;
    char buf[MAX_STRING_LENGTH];

    *buf = '\0';

    if ( ch->desc )
    {
        if ( ch->desc->botLagFlag == 1 )
            WAIT_STATE(ch, 3);

        ch->desc->botLagFlag = 2;
    }

    strcat(buf, "&RTHE NAMELESS CITY&n\r\n");

    for ( y = the_city->y_size - 1; y >= 0; y-- )
    {
        if ( ch->desc && IS_SET(ch->act, PLR_SUSPECTED_BOT) )
            sprintf(buf + strlen(buf), "&i%s\r\n%c%c%c\b%c&n",
                    (!number_range(0,2) ? ".." : ""),
                    UPPER(*team_table[number_range(0, g_numTeams-1)].name),
                    ".$FM?VDG "[number_range(0, 8)],
                    ".$FM?VDG."[number_range(0, 8)],
                    ".$FM?VDG "[number_range(0, 8)]);
        else
            strcat(buf, "\r\n&n");

        for ( x = 0; x < the_city->x_size; x++ )
        {
            the_room = index_room(the_city->rooms_on_level, x, y);

            if ( the_room == ch->in_room )
            {
                strcat(buf, "&Y*&n");
                continue;
            }
            else if (the_room->mine && the_room->mine->owner &&
                     (the_room->mine->owner->team == ch->team || ch->trust)
                     && !team_table[ch->team].independent)
            {
                strcat(buf, "&YM&n");
                continue;
            }
            else if ( HasSquareFeature(ch->team, x, y, SQRF_MINE) )
            {
                strcat(buf, "&Y?&n");
                continue;
            }

            for ( ich = the_room->people; ich; ich = ich->next_in_room )
                if ( ich->interior && IS_SET(ich->temp_flags, IS_VEHICLE) )
                {
                    sprintf(buf + strlen(buf), "%sV&n",
                            team_table[ich->team].namecolor);
                    break;
                }

            if ( ich )
                continue;

            for ( obj = the_room->contents; obj; obj = obj->next_content )
                if ( obj->item_type == ITEM_TEAM_VEHICLE )
                {
                    strcat(buf, "&gV&n");
                    break;
                }
                else if ( IS_SET(obj->general_flags, GEN_BURNS_ROOM) )
                {
                    strcat(buf, "&RF&n");
                    break;
                }
                else if ( IS_SET(obj->general_flags, GEN_DARKS_ROOM) )
                {
                    strcat(buf, "&XD&n");
                    break;
                }
                else if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) )
                {
                    strcat(buf, "&GG&n");
                    break;
                }

            if ( obj )
                continue;

            if ( the_room->people )
            {
                sprintf(buf + strlen(buf), "%s%c&n",
                        team_table[the_room->people->team].namecolor,
                        (IS_NPC(the_room->people) || p_tweak_bot(ch)
                         ? LOWER(*team_table[the_room->people->team].name)
                         : UPPER(*team_table[the_room->people->team].
                                 name)));
                continue;
            }

            if ( HasSquareFeature(ch->team, x, y, SQRF_DEPOT) )
            {
                strcat(buf, "&G$&n");
                continue;
            }

            if ( HasSquareFeature(ch->team, x, y, SQRF_DOWN) )
            {
                strcat(buf, "&WD&n");
                continue;
            }


            strcat(buf, ".");
        }
    }

    send_to_char(buf, ch);
}


void
do_look (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    int door;
    int number, count;

    if ( ch->interior )
    {
        ROOM_DATA *room = ch->in_room;
        CHAR_DATA *vict_next;

        char_from_room(ch);
        ch->in_room = room;     /* Makes the exterior_shell work. */

        for ( victim = ch->interior->people; victim; victim = vict_next )
        {
            vict_next = victim->next_in_room;
            exterior_shell(victim, do_look(victim, "auto"));
        }

        ch->in_room = NULL;
        char_to_room(ch, room);
    }

    if ( ch->desc == NULL )
        return;

    if ( ch->position < POS_SLEEPING )
    {
        send_to_char("You can't see anything but stars!\r\n", ch);
        return;
    }
    else if ( ch->position == POS_SLEEPING )
    {
        send_to_char("You can't see anything, you're sleeping!\r\n", ch);
        return;
    }

    if ( !check_blind(ch, true) )
        return;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    number = number_argument(arg1, arg3);
    count = 0;

    if ( arg1[0] == '\0' || !str_cmp(arg1, "auto") )
    {
#if 0
        if ( IS_SET(ch->temp_flags, MAN_HUD) )
        {
            vehicle_hud(ch->in_room->inside_mob, ch);
            return;
        }
#endif

        char twk1_buf[16];
        char twk2_buf[16];

        if ( ch->desc && IS_SET(ch->act, PLR_SUSPECTED_BOT) )
        {
            sprintf(twk1_buf, "&i%d&n", number_range(0, 20));
            sprintf(twk2_buf, "&i%d&n", number_range(0, 20));
        }
        else
        {
            *twk1_buf = '\0';
            *twk2_buf = '\0';
        }

        sprintf(buf, "&uD%s&n [%s&uX%d&n, %s&uX%d&n] L - &uL%d&n\r\n",
                ch->in_room->name,
                twk1_buf, ch->in_room->x,
                twk2_buf, ch->in_room->y,
                ch->in_room->level);

        send_to_char(buf, ch);
        do_exits(ch, "auto");
        show_list_to_char(ch->in_room->contents, ch, FALSE, FALSE);
        show_char_to_char(ch->in_room->people, ch);

        if ( IS_SET(ch->comm, COMM_AUTOSCAN) )
        {
            scan_direction(ch, ch->in_room, max_sight(ch), DIR_NORTH, true);
            scan_direction(ch, ch->in_room, max_sight(ch), DIR_EAST, true);
            scan_direction(ch, ch->in_room, max_sight(ch), DIR_SOUTH, true);
            scan_direction(ch, ch->in_room, max_sight(ch), DIR_WEST, true);
        }

        return;
    }

    if ( (ch->in_room->x == -1 ) &&
        (ch->in_room->y == -1) &&
        (ch->in_room->level == -1) &&
        (ch->in_room != safe_area) &&
        (!ch->in_room->interior_of ||
         (ch->in_room->interior_of->item_type == ITEM_TEAM_ENTRANCE)))
    {
        if ( !str_cmp(arg1, "map") )
        {
            if ( ch->in_room->inside_mob && ch->desc )
                ch->desc->botLagFlag = 0;

            show_battlemap(ch);
            return;
        }
    }

    if ( ch->desc )
        ch->desc->botLagFlag = 0;

    if ( !str_cmp(arg1, "out") )
    {
        if ( !ch->in_room->interior_of && !ch->in_room->inside_mob )
        {
            send_to_char("You are not inside anything to look out of!",
                         ch);
            return;
        }

        exterior_shell(ch, do_look(ch, "auto"));
        return;
    }

    if ( !str_cmp(arg1, "i") || !str_cmp(arg1, "in") )
    {
        /* 'look in' */
        if ( arg2[0] == '\0' )
        {
            send_to_char("Look in what?\r\n", ch);
            return;
        }

        if ( (obj = get_obj_here(ch, arg2)) == NULL )
        {
            send_to_char("You do not see that here.\r\n", ch);
            return;
        }

        if ( IS_SET(obj->general_flags, GEN_NO_AMMO_NEEDED) )
        {
            sprintf(buf, "$p needs no ammunition\r\n");
            act(buf, ch, obj, NULL, TO_CHAR);
            return;
        }
        if ( obj->contains )
        {
            sprintf(buf, "$p is loaded with $P.\r\nThere are %d rounds "
                    "left.\r\n", obj->contains->ammo);
            act(buf, ch, obj, obj->contains, TO_CHAR);
            return;
        }
        else
        {
            send_to_char("There is nothing inside.\r\n", ch);
            return;
        }
    }

    if ( (victim = get_char_room(ch, arg1)) != NULL )
    {
        show_char_to_char_1(victim, ch);
        return;
    }

    for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
    {
        if ( is_name(arg3, obj->name) )
            if ( ++count == number )
            {
                send_to_char(obj->description, ch);
                send_to_char("\r\n", ch);
                return;
            }
    }

    for ( obj = ch->in_room->contents; obj != NULL; obj = obj->next_content )
    {
        if ( is_name(arg3, obj->name) )
            if ( ++count == number )
            {
                send_to_char(obj->description, ch);
                send_to_char("\r\n", ch);
                return;
            }
    }

    if ( count > 0 && count != number )
    {
        if ( count == 1 )
            sprintf(buf, "You only see one %s here.\r\n", arg3);
        else
            sprintf(buf, "You only see %d %s's here.\r\n", count, arg3);

        send_to_char(buf, ch);
        return;
    }

    if ( !str_cmp(arg1, "n") || !str_cmp(arg1, "north") )
        door = 0;
    else if ( !str_cmp(arg1, "e") || !str_cmp(arg1, "east") )
        door = 1;
    else if ( !str_cmp(arg1, "s") || !str_cmp(arg1, "south") )
        door = 2;
    else if ( !str_cmp(arg1, "w") || !str_cmp(arg1, "west") )
        door = 3;
    else if ( !str_cmp(arg1, "u") || !str_cmp(arg1, "up") )
        door = 4;
    else if ( !str_cmp(arg1, "d") || !str_cmp(arg1, "down") )
        door = 5;
    else
    {
        send_to_char("You do not see that here.\r\n", ch);
        return;
    }

    send_to_char("Nothing special there.\r\n", ch);
}


/*
 * Thanks to Zrin for auto-exit part.
 */
void
do_exits (CHAR_DATA * ch, char *argument)
{
    show_exits(ch->in_room, ch, argument);
}

void
show_exits (ROOM_DATA * the_room, CHAR_DATA * ch, char *argument)
{
    ROOM_DATA *room_to;
    extern char *const dir_name[];
    char buf[MAX_STRING_LENGTH];
    int pexit;
    bool found;
    bool fAuto;
    sh_int door;
    int k;

    fAuto = !str_cmp(argument, "auto");

    if ( !check_blind(ch, true) )
        return;

    strcpy(buf, fAuto ? "[Exits:" : "Obvious exits:\r\n");
    k = strlen(buf);

    found = FALSE;
    for ( door = 0; door <= 5; door++ )
    {
        pexit = the_room->exit[door];
        if ( !IS_SET(pexit, EX_ISWALL) )
        {
            found = TRUE;

            if ( fAuto )
            {
                k += sprintf(buf + k, " &uE%s", dir_name[door]);
            }
            else
            {
                room_to = get_to_room(the_room, door);
                k += sprintf(buf + k, "&uE%-5s&n - %s\r\n",
                             capitalize(dir_name[door]), room_to->name);
            }
        }
    }

    if ( !found )
        strcat(buf, fAuto ? " none" : "None.\r\n");
    else
        strcat(buf, "&n");

    if ( fAuto )
        strcat(buf, "]\r\n");

    send_to_char(buf, ch);
}

void
do_score (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int hours;

    if ( IS_NPC(ch) )
        return;

    hours = (int) (ch->played + current_time - ch->logon) / 3600;

    sprintf(buf, "You are %s.\r\n", ch->names);
    send_to_char(buf, ch);
    sprintf(buf, "You have played for %d hour%s.\r\n",
            hours, (hours == 1 ? "" : "s"));
    send_to_char(buf, ch);
    sprintf(buf,
            "Your armor will absorb %d hp from each shot you " "take.\r\n",
            ch->armor);
    send_to_char(buf, ch);

    switch (ch->position)
    {
        case POS_DEAD:
            send_to_char("You are DEAD!!\r\n", ch);
            break;
        case POS_MORTAL:
            send_to_char("You are mortally wounded.\r\n", ch);
            break;
        case POS_INCAP:
            send_to_char("You are incapacitated.\r\n", ch);
            break;
        case POS_STUNNED:
            send_to_char("You are stunned.\r\n", ch);
            break;
        case POS_SLEEPING:
            send_to_char("You are sleeping.\r\n", ch);
            break;
        case POS_RESTING:
            send_to_char("You are resting.\r\n", ch);
            break;
        case POS_STANDING:
            send_to_char("You are standing.\r\n", ch);
            break;
        case POS_FIGHTING:
            send_to_char("You are fighting.\r\n", ch);
            break;
    }

    sprintf(buf,
            "Your Stats This Round: Kills - %d, Deaths - %d, Booms - %d, Suicides - %d\r\n",
            ch->pcdata->gs_kills, ch->pcdata->gs_deaths,
            ch->pcdata->gs_booms, ch->pcdata->gs_lemmings);
    send_to_char(buf, ch);

    /* RT wizinvis and holy light */
    if ( IS_IMMORTAL(ch) )
    {
        send_to_char("Holy Light: ", ch);
        if ( IS_SET(ch->act, PLR_HOLYLIGHT) )
            send_to_char("on", ch);
        else
            send_to_char("off", ch);

        if ( IS_SET(ch->act, PLR_WIZINVIS) )
        {
            sprintf(buf, "  Invisible: level %d", ch->invis_level);
            send_to_char(buf, ch);
        }
        send_to_char("\r\n", ch);
    }
}


void
system_info (struct descriptor_data *d)
{
    char buf[MAX_STRING_LENGTH];
    time_t ct = time(0);
    int k = 0;

    extern char str_boot_time[];
    extern char *scenario_name;
    extern int round_time_left;

    k = sprintf(buf,
                "Scenario              : %s\r\n"
                "Round started         : %s\r"
                "System time           : %s\r",
                scenario_name, str_boot_time, asctime(localtime(&ct)));

    if ( buttontimer != -1 && buttonpresser )
    {
        k += sprintf(buf + k,
                     "Button presser        : %s\r\n"
                     "Time left on button   : %d seconds\r\n",
                     buttonpresser->names, buttontimer);
    }
    else if ( round_time_left >= 0 )
    {
        k += sprintf(buf + k,
                     "Time Left in Round    : %d minute%s\r\n",
                     round_time_left, (round_time_left == 1 ? "" : "s"));
    }

    sprintf(buf + k, "Most players this boot: %d\r\n", max_on);
    write_to_buffer(d, buf, 0);
}


void
do_time (CHAR_DATA * ch, char *argument)
{
    if ( !ch->desc )
        return;
    system_info(ch->desc);
}

void
do_help (CHAR_DATA * ch, char *argument)
{
    HELP_DATA *pHelp;
    char argall[MAX_INPUT_LENGTH], argone[MAX_INPUT_LENGTH],
        buf[MAX_INPUT_LENGTH];
    bool dontPage = FALSE;
    int k = 0;

    argument = one_argument(argument, argone);

    if ( !strcmp(argone, "-") )
    {
        dontPage = TRUE;
        argument = one_argument(argument, argone);
    }

    if ( !*argone )
        strcpy(argall, "summary");
    else
        while ( *argone )
        {
            k += sprintf(argall + k, "%s%s", (k > 0 ? " " : ""), argone);
            argument = one_argument(argument, argone);
        }

    for ( pHelp = help_first; pHelp != NULL; pHelp = pHelp->next )
    {
        if ( pHelp->level > get_trust(ch) )
            continue;

        if ( is_name(argall, pHelp->keyword) )
        {
            if ( pHelp->level >= 0 && str_cmp(argall, "imotd") )
            {
                if ( dontPage )
                {
                    send_to_char(pHelp->keyword, ch);
                    send_to_char("\r\n", ch);
                }
                else
                {
                    page_to_char(pHelp->keyword, ch);
                    page_to_char("\r\n", ch);
                }
            }

            /*
             * Strip leading '.' to allow initial blanks.
             */
            if ( pHelp->text[0] == '.' )
            {
                if ( dontPage )
                    send_to_char(pHelp->text + 1, ch);
                else
                    page_to_char(pHelp->text + 1, ch);
            }
            else
            {
                if ( dontPage )
                    send_to_char(pHelp->text, ch);
                else
                    page_to_char(pHelp->text, ch);
            }

            return;
        }
    }

    sprintf(buf, "%s wanted help on %s.", ch->names, argall);
    logmesg(buf);
    send_to_char("No help on that word.\r\n", ch);
}



void
do_rank (CHAR_DATA * ch, char *argument)
{
    if ( RANK(ch) < RANK_HUNTER )
    {
        send_to_char
            ("Go away, lamer.  Only hunters and badasses can do this.\r\n",
             ch);
        return;
    }

    if ( !IS_SET(ch->act, PLR_RANKONLY) )
    {
        send_to_char
            ("You now only show your rank on 'who' to those of lower rank.\r\n",
             ch);
        SET_BIT(ch->act, PLR_RANKONLY);
    }
    else
    {
        send_to_char
            ("You now show your kills/deaths in 'who' to everyone.\r\n",
             ch);
        REMOVE_BIT(ch->act, PLR_RANKONLY);
    }
}


const char *who_ranks[RANK_MAX] = {
    "&cJOSH",
    "",
    "&BNOVICE",
    "&XMERC",
    "&GHUNTER",
    "&RBADASS"
};


char *
wholine (CHAR_DATA * who, bool justRank, bool fullTitle)
{
    static char who_buf[MAX_STRING_LENGTH];
    int titlespace = 43;        /* NAME:TITLE field length. */
    char title_str[MAX_INPUT_LENGTH];
    int j;

    /*
     * KILLS---DEATHS--TEAM-----RANK-----NAME:TITLE-----------------------------------
     * 10000   10000   TRAITOR  MERC     (Idle) Testing .............................
     */

    if ( justRank )
    {
        j = sprintf(who_buf, "                %s%*c %s%*c  ",
                    team_table[who->team].who_name,
                    (8 - str_len(team_table[who->team].who_name)), ' ',
                    who_ranks[RANK(who)],
                    (7 - str_len(who_ranks[RANK(who)])), ' ');
    }
    else
    {
        j = sprintf(who_buf, "&Y%-6d  %-6d  %s%*c %s%*c  ",
                    who->kills, who->deaths,
                    team_table[who->team].who_name,
                    (8 - str_len(team_table[who->team].who_name)), ' ',
                    who_ranks[RANK(who)],
                    (7 - str_len(who_ranks[RANK(who)])), ' ');
    }

    if ( who->idle > 16 )
    {
        j += sprintf(who_buf + j, "&W(%sIdle&W) ",
                     (who->idle >= 64 ? "&R" :
                      who->idle >= 48 ? "&C" : who->idle >=
                      32 ? "&c" : "&X"));
        titlespace -= 6;
    }

    if ( team_table[who->team].teamleader == who )
    {
        j += sprintf(who_buf + j, "&X(&RL&X) ");
        titlespace -= 5;
    }

    titlespace -= strlen(who->names);
    strn_cpy(title_str, who->pcdata->title_line, titlespace);
    sprintf(who_buf + j, "&n%s%s &n\r\n", who->names,
            (fullTitle ? who->pcdata->title_line : title_str));

    return who_buf;
}


/* whois command */
void
do_whois (CHAR_DATA * ch, char *argument)
{
    char output[4 * MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    DESCRIPTOR_DATA *d;
    bool found = FALSE;
    CHAR_DATA *vch;
    bool justRank;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        send_to_char("You must provide a name.\r\n", ch);
        return;
    }

    output[0] = '\0';

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
        if ( d->connected != CON_PLAYING )
            continue;

        vch = (d->original != NULL) ? d->original : d->character;

        if ( vch->in_room == graveyard_area || vch->in_room == store_area )
            continue;
        if ( IS_SET(vch->act, PLR_WIZINVIS) && ch->trust < vch->invis_level )
            continue;

        if ( !str_prefix(arg, vch->names) )
        {
            found = TRUE;

            justRank = !IS_IMMORTAL(ch) && RANK(vch) != RANK_UNRANKED &&
                RANK(vch) > RANK(ch) && IS_SET(vch->act, PLR_RANKONLY);

            strcat(output, wholine(vch, justRank, TRUE));
        }
    }

    if ( !found )
    {
        send_to_char("No one of that name is playing.\r\n", ch);
        return;
    }

    send_to_char
        ("&cKILLS&n---&cDEATHS&n--&cTEAM&n-----&cRANK&n-----&cNAME&n----\r\n",
         ch);
    send_to_char(output, ch);
}


void
teamscore (CHAR_DATA * ch)
{
    char buf[MAX_STRING_LENGTH];
    int index;
    int k;

    extern int g_numTeams;
    int calculate_worth(void);

    k = sprintf(buf, "&nWorth&X: &g%d &X- &nScores&X: ",
                calculate_worth());

    for ( index = 0; index < g_numTeams; index++ )
    {
        if ( team_table[index].independent || !team_table[index].active )
            continue;

        k += sprintf(buf + k, "&W(%s&W)&n:&W %d&n   ",
                     team_table[index].who_name, team_table[index].score);
    }

    strcat(buf, "\r\n");
    send_to_char(buf, ch);
}


void
do_who (CHAR_DATA * ch, char *argument)
{
    char output[4 * MAX_STRING_LENGTH];
    bool fImmortalOnly, fLinkDeadOnly;
    char arg[MAX_STRING_LENGTH];
    bool justRank = 0;
    CHAR_DATA *vch;
    int nMatch;

    /*
     * Set default arguments.
     */
    fImmortalOnly = FALSE;
    fLinkDeadOnly = FALSE;

    /*
     * Parse arguments.
     */
    argument = one_argument(argument, arg);

    switch (LOWER(*arg))
    {
        case 'i':
            fImmortalOnly = TRUE;
            break;
        case 'd':
            fLinkDeadOnly = TRUE;
            break;
        case 'l':
        default:
            *arg = 'l';
            teamscore(ch);
            send_to_char
                ("\r\n&cKILLS&n---&cDEATHS&n--&cTEAM&n-----&cRANK&n-----&cNAME&n----\r\n",
                 ch);
            break;
    }

    /*
     * Now show matching chars.
     */
    nMatch = 0;
    output[0] = '\0';

    for ( vch = char_list; vch; vch = vch->next )
    {
        /*
         * Check for match against restrictions.
         * Don't use trust as that exposes trusted mortals.
         */
        if ( vch->desc && fLinkDeadOnly )
            continue;
        if ( !vch->desc && !fLinkDeadOnly )
            continue;
        if ( IS_NPC(vch) )
            continue;
        if ( vch->in_room == graveyard_area || vch->in_room == store_area )
            continue;
        if ( IS_SET(vch->act, PLR_WIZINVIS) && ch->trust < vch->invis_level )
            continue;
        if ( fImmortalOnly && !vch->trust )
            continue;

        nMatch++;

        justRank = !IS_IMMORTAL(ch) && RANK(vch) != RANK_UNRANKED &&
            IS_SET(vch->act, PLR_RANKONLY) && RANK(vch) > RANK(ch);

        strcat(output, wholine(vch, justRank, FALSE));
    }

    sprintf(output + strlen(output), "\r\nPlayers found: &R%d&n\r\n",
            nMatch);
    page_to_char(output, ch);

    if ( (arg[0] == 'l') || (arg[0] == 'L') )
    {
        page_to_char
            ("\r\n&X- -- --&n- ----&W=&Y/ &RFREE KILLS &Y/&W=&n---- -&X-- -- -&n\r\n",
             ch);
        do_who(ch, "d");
    }
}


void
do_count (CHAR_DATA * ch, char *argument)
{
    int count;
    DESCRIPTOR_DATA *d;
    char buf[MAX_STRING_LENGTH];

    count = 0;

    for ( d = descriptor_list; d != NULL; d = d->next )
        if ( d->connected == CON_PLAYING && can_see(ch, d->character) )
            count++;

    if ( max_on == count )
        sprintf(buf,
                "There are %d characters on, the most so far this boot.\r\n",
                count);
    else
        sprintf(buf,
                "There are %d characters on, the most on this boot was %d.\r\n",
                count, max_on);

    send_to_char(buf, ch);
}


void
do_inventory (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "You are carrying %d items:\r\n", count_carried(ch));
    send_to_char(buf, ch);
    show_list_to_char(ch->carrying, ch, TRUE, TRUE);
    return;
}



void
do_equipment (CHAR_DATA * ch, char *argument)
{
    OBJ_DATA *obj;
    int iWear;
    bool found;

    send_to_char("You are using:\r\n", ch);
    found = FALSE;
    for ( iWear = 0; iWear < MAX_WEAR; iWear++ )
    {
        if ( (obj = get_eq_char(ch, iWear)) == NULL )
            continue;

        send_to_char(where_name[iWear], ch);
        send_to_char(format_obj_to_char(obj, ch, TRUE), ch);
        send_to_char("\r\n", ch);
        found = TRUE;
    }

    if ( !found )
        send_to_char("Nothing.\r\n", ch);

    return;
}


static int
whereline (struct char_data *ch, struct char_data *who, char *buf, int bpos)
{
    struct room_data *rm = who->in_room;
    int k = 0;

    if ( !can_see(ch, who) || ch == who )
        return (0);

    if ( who->in_room->level != ch->in_room->level )
        return (0);

    if ( ch->team != who->team && p_tweak_bot(ch) )
    {
        int x_max = rm->this_level->x_size - 1;
        int y_max = rm->this_level->y_size - 1;
        int x = rm->x + number_range(-5, 5);
        int y = rm->y + number_range(-5, 5);

        if ( x < 0 ) x = 0;
        if ( x > x_max ) x = x_max - 1;
        if ( y < 0 ) y = 0;
        if ( y > y_max ) y = y_max - 1;

        rm = index_room(rm, x - rm->x, y - rm->y);
    }

    k = sprintf(buf + bpos, "%s%-18s  &n%s",
                team_table[who->team].namecolor,
                without_colors(PERS(who, ch)), rm->name);

    if ( who->team == ch->team && !team_table[ch->team].independent )
        k += sprintf(buf + bpos + k, "  [&uX%d&n, &uX%d&n]", rm->x, rm->y);

    strcat(buf, "\r\n");
    return (k + 2);
}


void
do_where (struct char_data *ch, char *argument)
{
    struct room_data *wasin = NULL;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    struct char_data *ich;
    int bpos = 0;

    if ( !ch->in_room )
    {
        logmesg("%s in do_where in nowhere", ch->names);
        send_to_char("You appear to be in nowhere.  This is a bug.\r\n",
                     ch);
        return;
    }

    one_argument(argument, arg);
    strcpy(buf,
           "You check your low granularity scanners for life signs:\r\n");
    bpos = strlen(buf);

    if ( !ch->in_room->interior_of && ch->in_room->level == -1 )
    {
        strcat(buf, "None.\r\n");
        send_to_char(buf, ch);
        return;
    }
    else if ( ch->in_room->interior_of )
    {
        struct room_data *rm;

        if ( ch->in_room->interior_of->in_room->level == -1 )
        {
            strcat(buf, "None.\r\n");
            send_to_char(buf, ch);
            return;
        }

        wasin = ch->in_room;
        rm = ch->in_room->interior_of->in_room;
        char_from_room(ch);
        char_to_room(ch, rm);
    }
    else if ( ch->in_room->inside_mob )
    {
        struct room_data *rm;

        if ( ch->in_room->inside_mob->in_room->level == -1 )
        {
            strcat(buf, "None.\r\n");
            send_to_char(buf, ch);
            return;
        }

        wasin = ch->in_room;
        rm = ch->in_room->inside_mob->in_room;
        char_from_room(ch);
        char_to_room(ch, rm);
    }

    if ( !*arg || !str_prefix(arg, "players") )
    {
        for ( ich = char_list; ich; ich = ich->next )
        {
            if ( IS_NPC(ich) && !ich->interior )
                continue;
            if ( !IS_NPC(ich ) &&
                (!ich->desc || ich->desc->connected != CON_PLAYING))
                continue;
            bpos += whereline(ch, ich, buf, bpos);
        }
    }
    else if ( !str_cmp(arg, "all") )
    {
        for ( ich = char_list; ich; ich = ich->next )
            bpos += whereline(ch, ich, buf, bpos);
    }
    else
    {
        if ( (ich = get_char_world(ch, arg)) == NULL )
        {
            send_to_char("Who the fuck is that?!\r\n", ch);
            return;
        }

        bpos += whereline(ch, ich, buf, bpos);
    }

    if ( wasin )
    {
        char_from_room(ch);
        char_to_room(ch, wasin);

    }

    if ( ch->desc && !ch->desc->botLagFlag )
        ch->desc->botLagFlag = 1;

    send_to_char(buf, ch);
}


void
set_title (CHAR_DATA * ch, char *title)
{
    char buf[MAX_STRING_LENGTH];

    if ( IS_NPC(ch) )
    {
        logmesg("Set_title: NPC.");
        return;
    }

    if ( ch->trust > 1 && *title == '"' )
        one_argument_pcase(title, buf);
    else if (title[0] != '.' && title[0] != ',' && title[0] != '!' &&
             title[0] != '?')
    {
        buf[0] = ' ';
        strcpy(buf + 1, title);
    }
    else
    {
        strcpy(buf, title);
    }

    buf[30] = '\0';
    free_string(ch->pcdata->title_line);
    ch->pcdata->title_line = str_dup(buf);
}



void
do_title (CHAR_DATA * ch, char *argument)
{
    if ( IS_NPC(ch) )
        return;

    if ( argument[0] == '\0' )
    {
        send_to_char("Change your title to what?\r\n", ch);
        return;
    }

    smash_tilde(argument);
    set_title(ch, argument);
    send_to_char("Ok.\r\n", ch);
}

void
do_report (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_INPUT_LENGTH];

    sprintf(buf, "You say 'I have %d/%d hp.'\r\n", ch->hit, ch->max_hit);
    send_to_char(buf, ch);

    sprintf(buf, "$n says 'I have %d/%d hp.'", ch->hit, ch->max_hit);

    act(buf, ch, NULL, NULL, TO_ROOM);

    return;
}

void
do_password (CHAR_DATA * ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *pArg;
    char *pwdnew;
    char *p;
    char cEnd;

    if ( IS_NPC(ch) )
        return;

    /*
     * Can't use one_argument here because it smashes case.
     * So we just steal all its code.  Bleagh.
     */
    pArg = arg1;
    while ( isspace(*argument) )
        argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
        cEnd = *argument++;

    while ( *argument != '\0' )
    {
        if ( *argument == cEnd )
        {
            argument++;
            break;
        }
        *pArg++ = *argument++;
    }
    *pArg = '\0';

    pArg = arg2;
    while ( isspace(*argument) )
        argument++;

    cEnd = ' ';
    if ( *argument == '\'' || *argument == '"' )
        cEnd = *argument++;

    while ( *argument != '\0' )
    {
        if ( *argument == cEnd )
        {
            argument++;
            break;
        }
        *pArg++ = *argument++;
    }
    *pArg = '\0';

    if ( arg1[0] == '\0' || arg2[0] == '\0' )
    {
        send_to_char("Syntax: password <old> <new>.\r\n", ch);
        return;
    }

    if ( strcmp(crypt(arg1, ch->pcdata->pwd), ch->pcdata->password ) )
    {
        WAIT_STATE(ch, 40);
        send_to_char("Wrong password.  Wait 10 seconds.\r\n", ch);
        return;
    }

    if ( strlen(arg2) < 5 )
    {
        send_to_char
            ("New password must be at least five characters long.\r\n",
             ch);
        return;
    }

    /*
     * No tilde allowed because of player file format.
     */
    pwdnew = crypt(arg2, ch->name);
    for ( p = pwdnew; *p != '\0'; p++ )
    {
        if ( *p == '~' )
        {
            send_to_char("New password not acceptable, try again.\r\n",
                         ch);
            return;
        }
    }

    free_string(ch->pcdata->password);
    ch->pcdata->password = str_dup(pwdnew);
    save_char_obj(ch);
    send_to_char("Ok.\r\n", ch);
    return;
}

void
do_top (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];

    one_argument(argument, arg);
    if ( !arg[0] )
    {
        send_to_char("Syntax: top <kills|deaths|hours>\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "kills") )
    {
        send_to_char("KILLS\r\n", ch);
        show_top_list(ch, top_players_kills, NUM_TOP_KILLS);
    }
    else if ( !str_prefix(arg, "deaths") )
    {
        send_to_char("DEATHS\r\n", ch);
        show_top_list(ch, top_players_deaths, NUM_TOP_DEATHS);
    }
    else if ( !str_prefix(arg, "hours") )
    {
        send_to_char("HOURS\r\n", ch);
        show_top_list(ch, top_players_hours, NUM_TOP_HOURS);
    }
    else
        do_top(ch, "");
}


void
do_kill_message (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if ( !*argument )
    {
        sprintf(buf, "Your kill message is: %s&n\r\n", ch->kill_msg);
        send_to_char(buf, ch);
        return;
    }
    else if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your channel priviliges.\r\n",
                     ch);
        return;
    }

    smash_tilde(argument);

    if ( ch->kill_msg )
        free_string(ch->kill_msg);

    ch->kill_msg = str_dup(argument);
    send_to_char("Ok.\r\n", ch);
}


void
do_suicide_message (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if ( !*argument )
    {
        sprintf(buf, "Your suicide message is: %s&n\r\n", ch->suicide_msg);
        send_to_char(buf, ch);
        return;
    }
    else if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your channel priviliges.\r\n",
                     ch);
        return;
    }

    smash_tilde(argument);

    if ( ch->suicide_msg )
        free_string(ch->suicide_msg);

    ch->suicide_msg = str_dup(argument);
    send_to_char("Ok.\r\n", ch);
}


void
do_team (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char *aptr;

    if ( team_table[ch->team].teamleader != ch )
    {
        do_teamtalk(ch, argument);
        return;
    }

    aptr = one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Syntax:\r\n", ch);
        send_to_char("  team name   <new name>\r\n", ch);
        send_to_char("  team color  <new color code>\r\n", ch);
        send_to_char("  team <message> (to teamtalk)\r\n", ch);
        return;
    }

    if ( !str_cmp(arg, "name") )
    {
        one_argument(aptr, arg);

        if ( str_len(arg) < 3 || str_len(arg) > 7 )
        {
            send_to_char
                ("A team name must be between 3 and 7 visible characters long.\r\n",
                 ch);
            return;
        }

        if ( team_table[ch->team].who_name )
            free_string(team_table[ch->team].who_name);

        team_table[ch->team].who_name = str_dup(aptr);
        return;
    }

    if ( !str_cmp(arg, "color") )
    {
        one_argument(aptr, arg);

        if ( str_len(arg) > 0 )
        {
            send_to_char("It has to be *just* a color code.\r\n", ch);
            return;
        }

        if ( team_table[ch->team].namecolor )
            free_string(team_table[ch->team].namecolor);

        team_table[ch->team].namecolor = str_dup(aptr);
        return;
    }

    do_teamtalk(ch, argument);
}


void
do_repop (CHAR_DATA * ch, char *argument)
{
    if ( !IS_SET(ch->act, PLR_REPOP) )
    {
        send_to_char("You will now be deployed at base.\r\n", ch);
        SET_BIT(ch->act, PLR_REPOP);
        return;
    }
    send_to_char
        ("You will now be deployed randomly on the battlefield.\r\n", ch);
    REMOVE_BIT(ch->act, PLR_REPOP);
}


void
do_history (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int k = 0;
    int i;

    if ( !ch->desc || IS_NPC(ch) )
    {
        send_to_char("You need a descriptor.\r\n", ch);
        return;
    }

    for ( i = 0; i < ch->desc->hpoint; i++ )
        k += sprintf(buf + k, "%4d  %s\r\n", i, ch->desc->inlast[i]);

    send_to_char(buf, ch);
}


void
do_track (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    struct room_data *rm;
    CHAR_DATA *victim;
    int dx, dy;

    if ( !ch->in_room || ch->in_room->level == -1 )
    {
        send_to_char("Track here?!\r\n", ch);
        return;
    }

    send_to_char("You stop to examine your tracking device.\r\n", ch);
    WAIT_STATE(ch, 3);

    if ( !(victim = get_char_world(ch, argument)) )
    {
        send_to_char
            ("Your tracking device cannot pick up a matching signal.\r\n",
             ch);
        return;
    }
    else if ( victim->in_room == ch->in_room )
    {
        printf_to_char(ch, "%s is RIGHT IN THE ROOM WITH YOU!",
                       PERS(victim, ch));
        return;
    }
    else if ( victim->in_room->level != ch->in_room->level )
    {
        act("Your tracking device cannot pick up $S signal.", ch, NULL,
            victim, TO_CHAR);
        return;
    }
    else
    {
        rm = victim->in_room;
        sprintf(buf, "%s is ", PERS(victim, ch));
    }

    dy = rm->y - ch->in_room->y;
    dx = rm->x - ch->in_room->x;

    sprintf(buf + strlen(buf), "%s%s of you.",
            (dy < 0 ? "south" : (dy > 0 ? "north" : "")),
            (dx < 0 ? "west" : (dx > 0 ? "east" : "")));

    send_to_char(buf, ch);
}


void
do_compact (CHAR_DATA * ch, char *argument)
{
    if ( IS_SET(ch->comm, COMM_COMPACT) )
    {
        REMOVE_BIT(ch->comm, COMM_COMPACT);
        send_to_char("Compact removed.\r\n", ch);
    }
    else
    {
        SET_BIT(ch->comm, COMM_COMPACT);
        send_to_char("Compact set.\r\n", ch);
    }
}


void
do_autoscan (struct char_data *ch, char *argument)
{
    if ( IS_SET(ch->comm, COMM_AUTOSCAN) )
    {
        REMOVE_BIT(ch->comm, COMM_AUTOSCAN);
        send_to_char("You will no longer scan while walking.\r\n", ch);
    }
    else
    {
        SET_BIT(ch->comm, COMM_AUTOSCAN);
        send_to_char("You will now scan while walking.\r\n", ch);
    }
}
