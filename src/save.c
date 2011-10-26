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

#if defined(macintosh)
#include <types.h>
#else
#include <sys/types.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ground0.h"

#if !defined(macintosh)
extern int _filbuf args((FILE *));
#endif


extern int default_palette[MAX_PALETTE_ENTS];

/*
 * Local functions.
 */
void fwrite_char args((CHAR_DATA * ch, FILE * fp));
void fwrite_obj
args ((CHAR_DATA * ch, OBJ_DATA * obj, FILE * fp, int iNest));
void fread_char args((CHAR_DATA * ch, FILE * fp));
void fread_obj args((CHAR_DATA * ch, FILE * fp));
void set_default_pcdata args((CHAR_DATA * ch));
void set_default_colors args((CHAR_DATA * ch));
void set_char_defaults args((CHAR_DATA * ch));
PC_DATA *alloc_pcdata args(());
CHAR_DATA *alloc_char args(());


/*
 * Save a character and inventory.
 * Would be cool to save NPC's too for quest purposes,
 *   some of the infrastructure is provided.
 */
void
save_char_obj (CHAR_DATA * ch)
{
    char strsave[MAX_INPUT_LENGTH];
    FILE *fp;

    if ( IS_NPC(ch) )
        return;

    if ( !ch || !ch->names )      /* if the don't exsist, why save them? */
        return;

    if ( ch->desc != NULL && ch->desc->original != NULL )
        ch = ch->desc->original;

    sprintf(strsave, "%sPS%s", PLAYER_DIR, capitalize(ch->names));
    if ( (fp = fopen(PLAYER_TEMP, "w")) == NULL )
    {
        logmesg("Save_char_obj: fopen");
        logerr(strsave);
    }
    else
    {
        fwrite_char(ch, fp);
        fprintf(fp, "#END\n");
    }
    fclose(fp);
    rename(PLAYER_TEMP, strsave);
}



/*
 * Write the char.
 */
void
fwrite_char (CHAR_DATA * ch, FILE * fp)
{
    int i;

    fprintf(fp, "#PLAYER\n");

    fprintf(fp, "Name %s~\n", ch->names);
    if ( ch->short_descript && ch->short_descript[0] )
        fprintf(fp, "ShD  %s~\n", ch->short_descript);
    fprintf(fp, "Sex  %d\n", ch->sex);
    fprintf(fp, "nhp_solo %d\n", ch->pcdata->solo_hit);
    if ( ch->kills )
        fprintf(fp, "Kills2 %d\n", ch->kills);
    if ( ch->deaths )
        fprintf(fp, "Killed2 %d\n", ch->deaths);
    if ( IS_SET(ch->act, PLR_TRAITOR) )
        fprintf(fp, "TraitorTime %lu\n", ch->pcdata->time_traitored);
    fprintf(fp, "Plyd %d\n",
            ch->played + (int) (current_time - ch->logon));
    fprintf(fp, "Note %lu\n", (unsigned long) ch->last_note);
    fprintf(fp, "Window %d %d\n", ch->pcdata->columns, ch->pcdata->lines);
    if ( ch->act != 0 )
        fprintf(fp, "Act  %ld\n", ch->act);
    if ( ch->wiznet )
        fprintf(fp, "Wizn %ld\n", ch->wiznet);
    if ( ch->invis_level != 0 )
        fprintf(fp, "Invi %d\n", ch->invis_level);
    fprintf(fp, "Pass %s~\n", ch->pcdata->password);
    if ( ch->kill_msg )
        fprintf(fp, "KillMsg %s~\n", ch->kill_msg);
    if ( ch->suicide_msg )
        fprintf(fp, "SuicideMsg %s~\n", ch->suicide_msg);
    if ( ch->pcdata->poofin_msg && ch->pcdata->poofin_msg[0] )
        fprintf(fp, "Bin  %s~\n", ch->pcdata->poofin_msg);
    if ( ch->pcdata->poofout_msg && ch->pcdata->poofout_msg[0] )
        fprintf(fp, "Bout %s~\n", ch->pcdata->poofout_msg);
    if ( ch->pcdata->created_site )
        fprintf(fp, "CreatedSite %s~\n", ch->pcdata->created_site);
    fprintf(fp, "VotedOn %lu\n", (unsigned long) ch->pcdata->voted_on);

    if ( ch->trust < 2 )
        fprintf(fp, "Titl %s~\n", ch->pcdata->title_line);
    else
        fprintf(fp, "ImmTitle %s\n",
                escape_qstring(ch->pcdata->title_line));

    fprintf(fp, "TType %d\n", ch->pcdata->ttype);

    fprintf(fp, "Comm %ld\n", ch->comm);

    fprintf(fp, "Color %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
            ch->pcdata->color_hp,
            ch->pcdata->color_desc,
            ch->pcdata->color_obj,
            ch->pcdata->color_action,
            ch->pcdata->color_combat_o,
            ch->pcdata->color_combat_condition_s,
            ch->pcdata->color_combat_condition_o,
            ch->pcdata->color_xy,
            ch->pcdata->color_wizi,
            ch->pcdata->color_level,
            ch->pcdata->color_exits,
            ch->pcdata->color_say,
            ch->pcdata->color_tell, ch->pcdata->color_reply);

    if ( memcmp((const char * ) ch->pcdata->palette,
               (const char *) default_palette,
               sizeof(int) * MAX_PALETTE_ENTS) != 0)
    {
        fprintf(fp, "Palette");
        for ( i = 0; i < MAX_PALETTE_ENTS; i++ )
            fprintf(fp, " %X", ch->pcdata->palette[i]);
        fprintf(fp, "\n");
    }

    if ( GET_PAGER_MEMORY(ch) && GET_NO_PAGES(ch) > 0 )
    {
        fprintf(fp, "NoPages %d\n", GET_NO_PAGES(ch));

        for ( i = 0; i < GET_NO_PAGES(ch); i++ )
            fprintf(fp, "Page %s %ld %s~\n",
                    GET_PAGE(ch, i).from, GET_PAGE(ch, i).time,
                    GET_PAGE(ch, i).message);
    }

    fprintf(fp, "End\n\n");
}

CHAR_DATA *
alloc_char ()
{
    CHAR_DATA *temp;

    if ( char_free == NULL )
        return alloc_perm(sizeof(CHAR_DATA));
    else
    {
        temp = char_free;
        char_free = char_free->next;
        return temp;
    }
}

PC_DATA *
alloc_pcdata ()
{
    PC_DATA *temp;

    if ( pcdata_free == NULL )
        return alloc_perm(sizeof(PC_DATA));
    else
    {
        temp = pcdata_free;
        pcdata_free = pcdata_free->next;
        return temp;
    }
}

void
set_char_defaults (CHAR_DATA * ch)
{
    ch->move_delay = 0;
    ch->where_start = NULL;
    ch->valid = VALID_VALUE;
    ch->ld_behavior = 0;
    ch->affected_by = 0;
    ch->temp_flags = 0;
    ch->act = 0;
    ch->comm = COMM_COMBINE | COMM_PROMPT;
    ch->invis_level = 0;
    ch->trust = 0;
    ch->armor = 0;
    ch->last_fight = 0;
    ch->abs = 0;
}

void
set_default_colors (CHAR_DATA * ch)
{
    ch->pcdata->color_combat_condition_s = 3;
    ch->pcdata->color_action = 10;
    ch->pcdata->color_xy = 15;
    ch->pcdata->color_wizi = 5;
    ch->pcdata->color_hp = 6;
    ch->pcdata->color_combat_condition_o = 9;
    ch->pcdata->color_combat_o = 14;
    ch->pcdata->color_level = 9;
    ch->pcdata->color_exits = 10;
    ch->pcdata->color_desc = 11;
    ch->pcdata->color_obj = 12;
    ch->pcdata->color_say = 13;
    ch->pcdata->color_tell = 1;
    ch->pcdata->color_reply = 1;

    memcpy((char *) ch->pcdata->palette,
           (const char *) default_palette, sizeof(int) * MAX_PALETTE_ENTS);
}

void
set_default_pcdata (CHAR_DATA * ch)
{
    ch->pcdata->voted_on = 0;
    ch->pcdata->confirm_delete = FALSE;
    ch->pcdata->password = NULL;
    ch->pcdata->poofin_msg = NULL;
    ch->pcdata->poofin_msg = NULL;
    ch->pcdata->title_line = NULL;
    ch->pcdata->time_traitored = 0;
    ch->pcdata->solo_hit = HIT_POINTS_MORTAL;
    ch->pcdata->created_site = NULL;
    ch->pcdata->listen_data = 0;

    ch->pcdata->ttype = -1;
    ch->pcdata->lines = -1;
    ch->pcdata->columns = -1;

    ch->pcdata->it_is_chardata = 1;
    ch->pcdata->it.cd = NULL;
    ch->pcdata->him = NULL;
    ch->pcdata->her = NULL;

    GET_NO_PAGES(ch) = 0;
    GET_PAGER_MEMORY(ch) = NULL;

    /* Game Stats */
    ch->pcdata->gs_kills = 0;
    ch->pcdata->gs_deaths = 0;
    ch->pcdata->gs_booms = 0;
    ch->pcdata->gs_lemmings = 0;
    ch->pcdata->gs_idle = 0;
    ch->pcdata->gs_hellpts = 0;
    ch->pcdata->gs_teamed_kills = 0;
}

/*
 * Load a char and inventory into a new ch structure.
 */
CHAR_DATA *
load_char_obj (DESCRIPTOR_DATA * d, char *name)
{
    static PC_DATA pcdata_zero;
    char strsave[MAX_INPUT_LENGTH];
    char buf[100];
    CHAR_DATA *ch;
    FILE *fp;
    bool found;

    ch = alloc_char();
    clear_char(ch);
    ch->pcdata = alloc_pcdata();
    *ch->pcdata = pcdata_zero;

    if ( d )
    {
        d->character = ch;
        ch->desc = d;
    }
    else
        ch->desc = NULL;

    ch->names = str_dup(name);
    set_char_defaults(ch);
    set_default_colors(ch);
    set_default_pcdata(ch);

    found = FALSE;

    /* decompress if .gz file exists */
    sprintf(strsave, "%sPS%s%s", PLAYER_DIR, capitalize(name), ".gz");
    if ( (fp = fopen(strsave, "r")) != NULL )
    {
        fclose(fp);
        sprintf(buf, "gzip -dfq %s", strsave);
        system(buf);
    }

    sprintf(strsave, "%sPS%s", PLAYER_DIR, capitalize(name));
    if ( (fp = fopen(strsave, "r")) != NULL )
    {
        found = TRUE;
        /* freed from above, will be found in pfile */
        free_string(ch->names);
        ch->names = NULL;
        for ( ;; )
        {
            char letter;
            char *word;

            letter = fread_letter(fp);
            if ( letter == '*' )
            {
                fread_to_eol(fp);
                continue;
            }

            if ( letter != '#' )
            {
                logmesg("Load_char_obj: # not found.");
                break;
            }

            word = fread_word(fp);
            if ( !str_cmp(word, "PLAYER") )
                fread_char(ch, fp);
            else if ( !str_cmp(word, "END") )
                break;
            else
            {
                logmesg("Load_char_obj: bad section.");
                break;
            }
        }
        fclose(fp);
    }

    if ( IS_SET(ch->comm, COMM_TELNET_GA) && ch->desc )
        ch->desc->sga = 1;

    if ( found )
        return ch;
    else
        return 0;
}



/*
 * Read in a char.
 */

#if defined(KEY)
#undef KEY
#endif

#define KEY( literal, field, value )                    \
                if (  !str_cmp( word, literal )  )        \
                {                                       \
                    field  = value;                     \
                    fMatch = TRUE;                      \
                    break;                              \
                }

#define IGNORE(literal)                                 \
                if (  !str_cmp( word, literal )  )        \
                {                                       \
                    fMatch = TRUE;                      \
                    fread_to_eol (fp);                  \
                    break;                              \
                }


void
fread_char (CHAR_DATA * ch, FILE * fp)
{
    char buf[MAX_STRING_LENGTH];
    int pageIdx = 0;
    char *word;
    bool fMatch;

    for ( ;; )
    {
        word = feof(fp) ? "End" : fread_word(fp);
        fMatch = FALSE;

        switch (UPPER(word[0]))
        {
            case '*':
                fMatch = TRUE;
                fread_to_eol(fp);
                break;

            case 'A':
                KEY("Act", ch->act, fread_number(fp));

            case 'B':
                KEY("Bamfin", ch->pcdata->poofin_msg, fread_string(fp));
                KEY("Bamfout", ch->pcdata->poofout_msg, fread_string(fp));
                KEY("Bin", ch->pcdata->poofin_msg, fread_string(fp));
                KEY("Bout", ch->pcdata->poofout_msg, fread_string(fp));
                break;

            case 'C':
                KEY("Comm", ch->comm, fread_number(fp));
                KEY("CreatedSite", ch->pcdata->created_site,
                    fread_string(fp));
                if ( !str_cmp(word, "Color") )
                {
                    ch->pcdata->color_hp = fread_number(fp);
                    ch->pcdata->color_desc = fread_number(fp);
                    ch->pcdata->color_obj = fread_number(fp);
                    ch->pcdata->color_action = fread_number(fp);
                    ch->pcdata->color_combat_o = fread_number(fp);
                    ch->pcdata->color_combat_condition_s =
                        fread_number(fp);
                    ch->pcdata->color_combat_condition_o =
                        fread_number(fp);
                    ch->pcdata->color_xy = fread_number(fp);
                    ch->pcdata->color_wizi = fread_number(fp);
                    ch->pcdata->color_level = fread_number(fp);
                    ch->pcdata->color_exits = fread_number(fp);
                    ch->pcdata->color_say = fread_number(fp);
                    ch->pcdata->color_tell = fread_number(fp);
                    ch->pcdata->color_reply = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }
                break;

            case 'E':
                if ( !str_cmp(word, "End") )
                    return;
                break;

            case 'H':
                IGNORE("hp_solo");
                /*      KEY ("hp_solo", ch->pcdata->solo_hit, fread_number (fp)); */
                break;

            case 'I':
                KEY("InvisLevel", ch->invis_level, fread_number(fp));
                KEY("Invi", ch->invis_level, fread_number(fp));
                KEY("ImmTitle", ch->pcdata->title_line, fread_qstring(fp));
                break;

            case 'K':
                IGNORE("Killed");
                IGNORE("Kills");
                KEY("Kills2", ch->kills, fread_number(fp));
                KEY("Killed2", ch->deaths, fread_number(fp));
                KEY("KillMsg", ch->kill_msg, fread_string(fp));
                break;

            case 'N':
                KEY("Name", ch->names, fread_string(fp));
                KEY("nhp_solo", ch->pcdata->solo_hit, fread_number(fp));
                KEY("Note", ch->last_note, fread_number(fp));

                if ( !str_cmp(word, "NoPages") )
                {
                    GET_NO_PAGES(ch) = fread_number(fp);
                    GET_PAGER_MEMORY(ch) =
                        calloc(GET_NO_PAGES(ch), sizeof(struct page_data));
                    fMatch = TRUE;
                    break;
                }

                break;

            case 'P':
                if ( !str_cmp(word, "Page") )
                {
                    if ( GET_NO_PAGES(ch) <= 0 || !GET_PAGER_MEMORY(ch) )
                    {
                        logmesg("Got a pager message before NoPages.");
                        break;
                    }
                    GET_PAGE(ch, pageIdx).from = str_dup(fread_word(fp));
                    GET_PAGE(ch, pageIdx).time = fread_number(fp);
                    GET_PAGE(ch, pageIdx).message = fread_string(fp);

                    pageIdx++;

                    fMatch = TRUE;
                    break;
                }

                if ( !str_cmp(word, "Palette") )
                {
                    int i;

                    for ( i = 0; i < MAX_PALETTE_ENTS; i++ )
                    {
                        word = fread_word(fp);
                        sscanf(word, " %x", &ch->pcdata->palette[i]);
                    }

                    fMatch = TRUE;
                    break;
                }

                KEY("Password", ch->pcdata->password, fread_string(fp));
                KEY("Pass", ch->pcdata->password, fread_string(fp));
                KEY("Played", ch->played, fread_number(fp));
                KEY("Plyd", ch->played, fread_number(fp));
                break;

            case 'S':
                KEY("Sex", ch->sex, fread_number(fp));
                KEY("ShortDescr", ch->short_descript, fread_string(fp));
                KEY("ShD", ch->short_descript, fread_string(fp));
                KEY("SuicideMsg", ch->suicide_msg, fread_string(fp));
                break;

            case 'T':
                if ( !str_cmp(word, "TType") )
                {
                    ch->pcdata->ttype = fread_number(fp);
                    if ( ch->pcdata->ttype != -1 && ch->desc )
                        ch->desc->ttype = ch->pcdata->ttype;

                    fMatch = TRUE;
                    break;
                }

                KEY("TraitorTime", ch->pcdata->time_traitored,
                    fread_number(fp));
                KEY("Trust", ch->trust, fread_number(fp));
                KEY("Tru", ch->trust, fread_number(fp));

                if ( !str_cmp(word, "Title") || !str_cmp(word, "Titl") )
                {
                    ch->pcdata->title_line = fread_string(fp);
                    if (ch->pcdata->title_line[0] != '.' &&
                        ch->pcdata->title_line[0] != ',' &&
                        ch->pcdata->title_line[0] != '!' &&
                        ch->pcdata->title_line[0] != '?')
                    {
                        sprintf(buf, " %s", ch->pcdata->title_line);
                        free_string(ch->pcdata->title_line);
                        ch->pcdata->title_line = str_dup(buf);
                    }
                    fMatch = TRUE;
                    break;
                }
                break;
            case 'V':
                KEY("VotedOn", ch->pcdata->voted_on, fread_number(fp));
                break;
            case 'W':
                KEY("Wizn", ch->wiznet, fread_number(fp));

                if ( !str_cmp(word, "Window") )
                {
                    ch->pcdata->columns = fread_number(fp);
                    ch->pcdata->lines = fread_number(fp);

                    if ( ch->pcdata->columns != -1 && ch->desc )
                        ch->desc->columns = ch->pcdata->columns;
                    if ( ch->pcdata->lines != -1 && ch->desc )
                        ch->desc->lines = ch->pcdata->lines;

                    fMatch = TRUE;
                    break;
                }
                break;
        }

        if ( !fMatch )
        {
            logmesg("Fread_char: no match (%s).", word);
            fread_to_eol(fp);
        }
    }
}
