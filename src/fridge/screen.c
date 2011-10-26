/*
 * ============================================================================
 *                              _______ _______ 
 *                             |   _   |   _   |
 *                             |.  |___|___|   |
 *             -= g r o u n d  |.  |   |/  ___/  z e r o   2 =-
 *                             |:  1   |:  1  \ 
 *                             |::.. . |::.. . |
 *                             `-------`-------'
 * ============================================================================
 *
 * Module:      screen.c
 * Synopsis:    Full-screen (ISO DP6429) interface for GZ2.
 * Author:      Satori
 * Created:     13 Aug 2002
 */

#include "ground0.h"


/*
 * TODO:
 *
 * - Make this compile.
 * - See testscreen in interp.c for test command.
 * - Fill-in the weapons/ammo fields on the overlay.
 * - Fix it so on blind, everything turns into ':'s on the map.
 * - Fix it so map shows arena borders with ':' in UpdateScreen().
 * - Add PRETTY command to switch this mode on.
 * - Call UpdateScreen() when necessary.
 * - Make UpdateScreen() do \0337 and \0338.
 */


#define MAP_DIST            4
#define MAP_WIDTH           (MAP_DIST * 2) + 1
#define MAP_HEIGHT          (MAP_DIST * 2) + 1


/*
 * Yet more constants about the location of various things in the screen
 * oriented interface.  In this case, the lengths of various things to help
 * us jump there and draw what we need to.
 */
#define MAP_PAD             2       /* Spaces before map line. */
#define HEADER              2 * MAP_PAD + MAP_WIDTH
#define FIELD_COLUMN        HEADER + 12
#define HEALTH_BAR          HEADER + 13
#define HEALTH_INFO         HEADER + 32


/* Functions defined herein. */
void RedrawScreen (struct char_data *);
void UpdateScreen (struct char_data *);
static char *MapChar (struct char_data *, int, int);
char *meter (int, int);


/* Needed: */
bool check_blind (struct char_data *, bool);


void
RedrawScreen (struct char_data *ch)
{
    char map[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

    if ( ch->in_room->level == -1 || check_blind(ch, false) )
    {
        memset(buf, ':', MAP_WIDTH);
        for ( int line = 0; line < MAP_HEIGHT; line++ )
            sprintf(map, "  &X%s&n\r\n", buf);
    }
    else
    {
        int xs = ch->in_room->this_level->x_size - 1;
        int ys = ch->in_room->this_level->y_size - 1;
        int xp = ch->in_room->x;
        int yp = ch->in_room->y;
        int pos = 0;
        int x, y;

        for ( y = -MAX_DIST; y <= MAX_DIST; y++ )
        {
            pos += sprintf(buf + pos, "  &X");

            for ( x = -MAX_DIST; x <= MAX_DIST; x++ )
                map[pos++] = (x + xp < 0 || x + xp > xs ||
                              y + yp < 0 || y + yp > ys) ? ':' : '.';

            map[pos++] = '&';
            map[pos++] = 'n';
            map[pos++] = '\r';
            map[pos++] = '\n';
        }

        map[pos] = '\0';
    }

    for ( int i = 0; i < 62; i++ )
        buf[i] = '=';

    printf_to_char(ch,
        "\x1B[r"                /* Reset normal scroll region.  */
        "\x1B[H\x1B[J"          /* Clear screen, home cursor.   */
        "\x1B[0;9r"             /* Set top scroll region.       */
        "\x1B[H"                /* Go home again (in case).     */
        "%s"                    /* The map buffer.              */
        "\x1B[%d;1H&n%s"        /* Break line.                  */
        "\x1B[%d;3H&cHealth&X:    [          ]       (    /    )"
        "\x1B[%d;4H&cAbsorb&X:"
        "\x1B[%d;5H&cFighting&X:"
        "\x1B[%d;6H&cPrimary&X:                      (    /    )"
        "\x1B[%d;7H&cSecondary&X:                    (    /    )"
        "\x1B[10;25r"           /* Set bottom scroll region.    */
        "\x1B[0;10H\0337",      /* Hack for UpdateScreen().     */
        map, HEADER, buf, HEADER, HEADER, HEADER, HEADER, HEADER);

    UpdateScreen(ch);
    send_to_char("\x1B[0;10H\0337", ch);
}


void
UpdateScreen (struct char_data *ch)
{
    /*
     * These correspond to DIR_NORTH, etc., in order and move the cursor in
     * the proper direction by one space.  See ECMA-048 or ISO DP6429 for
     * descriptions of \x1B[A ... \x1B[D.
     */
    const char isoScoot[] = "ACBD";

    char room_line[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    struct room_data *rm;
    int pos = 0;

    const int px = MAP_WIDTH / 2 + MAP_PAD;
    const int py = MAP_HEIGHT / 2;


    for ( int dir = 0; dir < DIR_UP; dir++ )
    {
        rm = ch->in_room;
        pos += sprintf(buf+pos, "\x1B[%d;%dH", px, py);

        for ( dist = 1; rm->exit[dir] && dist <= MAX_DIST; dist++ )
        {
            rm = index_room(rm, dir_cmod[dir][0], dir_cmod[dir][1]);
            pos += sprintf(buf+pos, "\x1B[%c%s", isoScoot[dir],
                (blind ? "&X.&n" : MapChar(ch, rm)));
        }
    }

    pos += sprintf(buf+pos, "\x1B[%d;%dH&Y*&n", px, py);

    sprintf(room_line, "&uD%.30s&n [&uX%d&n, &uX%d&n] L - &uL%d&n",
        ch->in_room->name, ch->in_room->x,
        ch->in_room->y, ch->in_room->level);

    pos += sprintf(buf+pos, "\x1B[%d;0H%s%*c(TEAM %s&n)",
        HEADER, buf, 60 - str_len(buf), ' ',
        team_table[ch->team].who_name);

    send_to_char(buf, ch);
    pos = 0;

    pos += sprintf(buf+pos,
        "\x1B[%d;3H&Y%s\x1B[%dH&R%-4d&X/&r%-4d&X)"
        "\x1B[%d;4H&B%d&nhp"
        "\x1B[%d;5H%s%s",
        HEALTH_BAR, meter(ch->hit, ch->max_hit),
        HEALTH_INFO, ch->hit, ch->max_hit,
        FIELD_COLUMN, ch->abs, FIELD_COLUMN,
        (ch->fighting ? team_table[ch->fighting->team].namecolor : "&n"),
        (ch->fighting ? ch->fighting->names : "n/a"));

    strcpy(buf + pos, "\x1B[10;25r\0338");
    send_to_char(buf, ch);
}


/*
 * XXX: This code is common to all maps displayed by GZ2 and thus should be
 * made public and used in show_battlemap() and the mech's HUD code (when
 * it's brought back).
 */
static char *
MapChar (struct char_data *ch, struct room_data *rm)
{
    struct char_data *ich;
    struct obj_data *iobj;
    static char buf[32];

    if ( rm->mine &&
         rm->mine->owner &&
         ((rm->mine->owner->team == ch->team &&
           !team_table[ch->team].independent) ||
          ch->trust) )
    {
        strcpy(buf, "&YM&n");
        return buf;
    }

    if ( HasSquareFeature(ch->team, x, y, SQRF_MINE) )
    {
        strcpy(buf, "&Y?&n");
        return buf;
    }

    for ( ich = rm->people; ich; ich = ich->next_in_room )
        if ( ich->interior && IS_SET(ich->temp_flags, IS_VEHICLE) )
        {
            sprintf(buf, "%sV&n", team_table[ich->team].namecolor);
            break;
        }

    if ( ich )
        return buf;

    for ( iobj = rm->contents; iobj; iobj = iobj->next_content )
        if ( iobj->item_type == ITEM_TEAM_VEHICLE )
        {
            strcpy(buf, "&gV&n");
            break;
        }
        else if ( IS_SET(iobj->general_flags, GEN_BURNS_ROOM) )
        {
            strcpy(buf, "&RF&n");
            break;
        }
        else if ( IS_SET(iobj->general_flags, GEN_DARKS_ROOM) )
        {
            strcpy(buf, "&XD&n");
            break;
        }
        else if ( IS_SET(iobj->general_flags, GEN_CHOKES_ROOM) )
        {
            strcpy(buf, "&GG&n");
            break;
        }

    if ( iobj )
        return buf;

    if ( rm->people )
    {
        sprintf(buf, "%s%c&n", team_table[rm->people->team].namecolor,
                (IS_NPC(rm->people) || p_tweak_bot(ch)
                 ? LOWER(*team_table[rm->people->team].name)
                 : UPPER(*team_table[rm->people->team].
                         name)));
        return buf;
    }

    if ( HasSquareFeature(ch->team, x, y, SQRF_DEPOT) )
    {
        strcpy(buf, "&G$&n");
        return buf;
    }

    if ( HasSquareFeature(ch->team, x, y, SQRF_DOWN) )
    {
        strcpy(buf, "&WD&n");
        return buf;
    }

    return buf;
}


static char *
meter (int cur, int max)
{
    static char buf[11];
    int perc = PERCENT(cur, max);
    int fullChars = perc / 10;
    memset(buf, '#', fullChars);
    strcpy(buf + fullChars, "&X");      /* +2 */
    memset(buf + fullChars + 2, ':', 10 - fullChars);
    *(buf + 10) = '\0';
    return buf;
}
