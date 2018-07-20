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
 * Module:      top.c
 * Synopsis:    Top junk.
 * Author:      satori
 * Created:     19 Feb 2002
 *
 * NOTES:
 */

#include "ground0.h"

TOP_DATA top_players_kills[NUM_TOP_KILLS];
TOP_DATA top_players_deaths[NUM_TOP_DEATHS];
TOP_DATA top_players_hours[NUM_TOP_HOURS];


void
show_top_list (CHAR_DATA * ch, TOP_DATA * topv, size_t sz)
{
    char buf[MAX_STRING_LENGTH];
    int i;

    for ( i = 0; i < sz; i++ )
    {
        if ( topv[i].val != 0 )
        {
            sprintf(buf, "%2d. [&Y%d&n] &R%s&n\r\n", i + 1, topv[i].val,
                topv[i].name);
            send_to_char(buf, ch);
        }
        else break;
    }
}


void
initialise_top (TOP_DATA * topv, size_t sz)
{
    memset((char *) topv, 0, sizeof(TOP_DATA) * sz);
}


static inline int
remove_entry (const char *name, TOP_DATA * topv, size_t sz)
{
    int wasIn = FALSE;
    int i, j;

    for ( i = 0; i < sz; i++ )
    {
        if ( !strcmp(name, topv[i].name) )
        {
            for ( j = i + 1; j < sz && *topv[j].name; j++ )
            {
                /* We need all the optimization we can get. */
                memcpy((char *) &topv[j - 1], (const char *) &topv[j],
                       sizeof(TOP_DATA));
            }

            strcpy(topv[j - 1].name, "");
            topv[j - 1].val = 0;
            wasIn = TRUE;
            i--;
        }
    }

    return (wasIn);
}


static int
insert_into_top (const char *name, int val, TOP_DATA * topv, size_t sz)
{
    int insertAt = 0;
    int i, rv;

    rv = remove_entry(name, topv, sz);

    while ( insertAt < sz && topv[insertAt].val > val )
    {
        /* Find best entry with value lesser than or equal to val. */
        insertAt++;
    }

    if ( insertAt >= sz )
    {
        logmesg("Bad call to insert_into_top()");
        return (0);
    }
    else if ( insertAt != (sz - 1) )
        for ( i = (sz - 1); i > insertAt; i-- )
            memcpy((char *) &topv[i], (const char *) &topv[i - 1],
                   sizeof(TOP_DATA));

    strncpy(topv[insertAt].name, name, MAX_TOPNAME_LEN);
    topv[insertAt].name[MAX_TOPNAME_LEN] = '\0';
    topv[insertAt].val = val;
    return (!rv);
}


static void
write_top (const char *fname, TOP_DATA * topv, size_t sz)
{
    FILE *fh;

    if ( (fh = fopen(fname, "wb")) == NULL )
    {
        logmesg("Error opening top file %s for writing", fname);
        return;
    }

    fwrite((char *) topv, sizeof(TOP_DATA), sz, fh);
    fclose(fh);
}


void
read_top (const char *fname, TOP_DATA * topv, size_t sz)
{
    FILE *fh;

    if ( (fh = fopen(fname, "rb")) == NULL )
    {
        logmesg("Error opening top file %s for reading", fname);
        fclose(fh);
        return;
    }

    if (!fread((char *) topv, sizeof(TOP_DATA), sz, fh)) {
        logmesg ("Error: Could not load top data from %s!", fname);
        fclose(fh);
        return;
    }
    
    fclose(fh);
}


void
check_top_kills (CHAR_DATA * ch)
{
    if ( IS_IMMORTAL(ch) || IS_NPC(ch) || !ch->names )
        return;

    if ( ch->kills > top_players_kills[NUM_TOP_KILLS - 1].val )
    {
        if (insert_into_top
            (ch->names, ch->kills, top_players_kills, NUM_TOP_KILLS))
            send_to_char
                ("Your name has been added to the &Rtop kills&n list!\r\n",
                 ch);

        write_top(TOP_FILE, top_players_kills, NUM_TOP_KILLS);
    }
}


void
check_top_deaths (CHAR_DATA * ch)
{
    if ( IS_IMMORTAL(ch) || IS_NPC(ch) || !ch->names )
        return;

    if ( ch->deaths > top_players_deaths[NUM_TOP_DEATHS - 1].val )
    {
        if (insert_into_top
            (ch->names, ch->deaths, top_players_deaths, NUM_TOP_DEATHS))
            send_to_char
                ("Your name has been added to the &Rtop deaths&n list!\r\n",
                 ch);

        write_top(TOP_DEATH_FILE, top_players_deaths, NUM_TOP_DEATHS);
    }
}


void
check_top_hours (CHAR_DATA * ch)
{
    if ( IS_IMMORTAL(ch) || IS_NPC(ch) || !ch->names )
        return;

    int hours = (ch->played + time(0) - ch->logon) / 3600;

    if ( hours > top_players_hours[NUM_TOP_HOURS - 1].val )
    {
        if (insert_into_top
            (ch->names, hours, top_players_hours, NUM_TOP_HOURS))
            send_to_char
                ("Your name has been added to the &Rtop hours&n list!\r\n",
                 ch);

        write_top(TOP_HOURS_FILE, top_players_hours, NUM_TOP_HOURS);
    }
}
