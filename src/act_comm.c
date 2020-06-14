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

#include "ground0.h"
#include "telnet.h"
#include "imp_table.h"

extern char *guild_list[];

/* command procedures needed */
DECLARE_DO_FUN (do_quit);


void clear_screen(struct char_data *ch);



/*
 * Local functions.
 */
void note_attach(CHAR_DATA * ch);
void note_remove(CHAR_DATA * ch, NOTE_DATA * pnote);
void note_delete(NOTE_DATA * pnote);



bool
is_note_to (CHAR_DATA * ch, NOTE_DATA * pnote)
{
    if ( !str_cmp(ch->names, pnote->sender) )
        return TRUE;

    if ( is_name_exact("all", pnote->to_list) )
        return TRUE;

    if ( IS_HERO(ch ) &&
        (is_name("immortal", pnote->to_list) ||
         is_name_exact("imm", pnote->to_list)))
        return TRUE;

    if ( is_name_exact(ch->names, pnote->to_list) )
        return TRUE;

    if ( IS_IMP(ch) )
        return TRUE;

    return FALSE;
}



void
note_attach (CHAR_DATA * ch)
{
    NOTE_DATA *pnote;

    if ( ch->pnote != NULL )
        return;

    if ( note_free == NULL )
    {
        pnote = alloc_perm(sizeof(*ch->pnote));
    }
    else
    {
        pnote = note_free;
        note_free = note_free->next;
    }

    pnote->next = NULL;
    pnote->sender = str_dup(ch->names);
    pnote->date = str_dup("");
    pnote->to_list = str_dup("");
    pnote->subject = str_dup("");
    pnote->text = str_dup("");
    pnote->flags = 0L;
    ch->pnote = pnote;
    return;
}



void
note_remove (CHAR_DATA * ch, NOTE_DATA * pnote)
{
    char to_new[MAX_INPUT_LENGTH];
    char to_one[MAX_INPUT_LENGTH];
    FILE *fp;
    NOTE_DATA *prev;
    char *to_list;

    /*
     * Build a new to_list.
     * Strip out this recipient.
     */
    to_new[0] = '\0';
    to_list = pnote->to_list;
    while ( *to_list != '\0' )
    {
        to_list = one_argument(to_list, to_one);
        if ( to_one[0] != '\0' && str_cmp(ch->names, to_one) )
        {
            strcat(to_new, " ");
            strcat(to_new, to_one);
        }
    }

    /*
     * Just a simple recipient removal?
     */
    if ( str_cmp(ch->names, pnote->sender) && to_new[0] != '\0' )
    {
        free_string(pnote->to_list);
        pnote->to_list = str_dup(to_new + 1);
        return;
    }

    /*
     * Remove note from linked list.
     */
    if ( pnote == note_list )
    {
        note_list = pnote->next;
    }
    else
    {
        for ( prev = note_list; prev != NULL; prev = prev->next )
        {
            if ( prev->next == pnote )
                break;
        }

        if ( prev == NULL )
        {
            logmesg("Error: Note_remove: pnote not found.");
            return;
        }

        prev->next = pnote->next;
    }

    free_string(pnote->text);
    free_string(pnote->subject);
    free_string(pnote->to_list);
    free_string(pnote->date);
    free_string(pnote->sender);
    pnote->next = note_free;
    note_free = pnote;

    if ( (fp = fopen(NOTE_FILE, "w")) == NULL )
    {
        logerr(NOTE_FILE);
    }
    else
    {
        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            fprintf(fp, "Sender  %s~\n", pnote->sender);
            fprintf(fp, "Date    %s~\n", pnote->date);
            fprintf(fp, "Stamp   %lu\n",
                    (unsigned long) pnote->date_stamp);
            fprintf(fp, "Flags   %lu\n", pnote->flags);
            fprintf(fp, "To      %s~\n", pnote->to_list);
            fprintf(fp, "Subject %s~\n", pnote->subject);
            fprintf(fp, "Text\n%s~\n", pnote->text);
        }
        fclose(fp);
    }
}

/* used by imps to nuke a note for good */
void
note_delete (NOTE_DATA * pnote)
{
    FILE *fp;
    NOTE_DATA *prev;

    /*
     * Remove note from linked list.
     */
    if ( pnote == note_list )
    {
        note_list = pnote->next;
    }
    else
    {
        for ( prev = note_list; prev != NULL; prev = prev->next )
        {
            if ( prev->next == pnote )
                break;
        }

        if ( prev == NULL )
        {
            logmesg("Error: note_delete: pnote not found.");
            return;
        }

        prev->next = pnote->next;
    }

    free_string(pnote->text);
    free_string(pnote->subject);
    free_string(pnote->to_list);
    free_string(pnote->date);
    free_string(pnote->sender);
    pnote->flags = 0L;
    pnote->next = note_free;
    note_free = pnote;

    if ( (fp = fopen(NOTE_FILE, "w")) == NULL )
    {
        logerr(NOTE_FILE);
    }
    else
    {
        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            fprintf(fp, "Sender  %s~\n", pnote->sender);
            fprintf(fp, "Date    %s~\n", pnote->date);
            fprintf(fp, "Stamp   %lu\n",
                    (unsigned long) pnote->date_stamp);
            fprintf(fp, "Flags   %lu\n", pnote->flags);
            fprintf(fp, "To      %s~\n", pnote->to_list);
            fprintf(fp, "Subject %s~\n", pnote->subject);
            fprintf(fp, "Text\n%s~\n", pnote->text);
        }
        fclose(fp);
    }
}


struct
{
    char *name;
    int bit;
    int trust;
    bool show;
}
note_flags[] =
{
    { "nodelete", PNOTE_NODEL, 1, TRUE },
    { "!delete", PNOTE_NODEL, 1, FALSE },
    { "urgent", PNOTE_URGENT, 10, TRUE },
    { "stupid", PNOTE_STUPID, 0, TRUE },
    { "smart", PNOTE_SMART, 0, TRUE },
    { "qotd", PNOTE_QOTD, 0, TRUE },
    { "sotd", PNOTE_SOTD, 0, TRUE },

    { NULL, 0 }
};


void
do_note (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *ptr;
    FILE *fp;

    CHAR_DATA *victim;
    NOTE_DATA *pnote;
    int vnum;
    int anum;

    if ( IS_NPC(ch) )
        return;

    argument = one_argument(argument, arg);
    smash_tilde(argument);

    if ( arg[0] == '\0' )
    {
        do_note(ch, "read");
        return;
    }

    if ( !str_prefix(arg, "flags") && ch->trust )
    {
        argument = one_argument(argument, arg2);

        if ( !*arg2 || !isdigit(*arg2) )
        {
            strcpy(buf, "USAGE: note flags <note #> <[+|-]flags>\r\n\n"
                   "You can set or remove the following flags:\r\n");

            for ( vnum = 1, anum = 0; note_flags[anum].name; anum++ )
            {
                if (note_flags[anum].show &&
                    ch->trust >= note_flags[anum].trust &&
                    note_flags[anum].bit)
                {

                    sprintf(buf + strlen(buf), "%-12s%s",
                            note_flags[anum].name,
                            (!((vnum++) % 5) ? "\r\n" : "   "));
                }
            }

            if ( (vnum % 5) )
                strcat(buf, "\r\n");
            send_to_char(buf, ch);
            return;
        }

        anum = atoi(arg2);
        vnum = 0;

        for ( pnote = note_list; anum != vnum && pnote; pnote = pnote->next )
            if ( is_note_to(ch, pnote) )
                vnum++;

        if ( !pnote )
        {
            send_to_char("No such note.\r\n", ch);
            return;
        }

        do
        {
            argument = one_argument(argument, arg2);

            if ( !*arg2 )
                break;

            if ( *arg2 == '+' || *arg2 == '-' )
            {
                anum = (*arg2 == '+') ? 2 : 1;
                ptr = arg2 + 1;
            }
            else
            {
                anum = 0;
                ptr = arg2;
            }

            for ( vnum = 0; note_flags[vnum].name; vnum++ )
                if (ch->trust >= note_flags[vnum].trust &&
                    !str_prefix(ptr, note_flags[vnum].name))
                    break;

            if ( !note_flags[vnum].name )
            {
                printf_to_char(ch, "%s: Unrecognized note flag.\r\n", ptr);
                continue;
            }
            else if ( !note_flags[vnum].bit )
            {
                printf_to_char(ch,
                               "%s: Unimplemented or reserved.  Cannot set.\r\n",
                               note_flags[vnum].name);
                continue;
            }

            switch (anum)
            {
                default:
                case 0: /* Toggle. */
                    pnote->flags ^= note_flags[vnum].bit;
                    break;
                case 1: /* Remove. */
                    REMOVE_BIT(pnote->flags, note_flags[vnum].bit);
                    break;
                case 2: /* Set. */
                    SET_BIT(pnote->flags, note_flags[vnum].bit);
                    break;
            }

            printf_to_char(ch, "%s: %s.", note_flags[vnum].name,
                           IS_SET(pnote->flags,
                                  note_flags[vnum].
                                  bit) ? "Set" : "Removed");
        }
        while ( *argument );

        if ( !(fp = fopen(NOTE_FILE, "w")) )
        {
            logerr(NOTE_FILE);
            printf_to_char(ch,
                           "Flag changes could not be saved: %s: %s\r\n",
                           NOTE_FILE, strerror(errno));
            return;
        }

        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            fprintf(fp, "Sender  %s~\n", pnote->sender);
            fprintf(fp, "Date    %s~\n", pnote->date);
            fprintf(fp, "Stamp   %lu\n",
                    (unsigned long) pnote->date_stamp);
            fprintf(fp, "Flags   %lu\n", pnote->flags);
            fprintf(fp, "To      %s~\n", pnote->to_list);
            fprintf(fp, "Subject %s~\n", pnote->subject);
            fprintf(fp, "Text\n%s~\n", pnote->text);
        }

        fclose(fp);

        return;
    }

    if ( !str_prefix(arg, "list") )
    {
        int bitBlock = PNOTE_STUPID;
        bool fNewOnly = FALSE;

        argument = one_argument(argument, arg2);

        while ( *arg2 == '-' )
        {
            switch (*(arg2 + 1))
            {
                case 'a':       /* All (default). */
                    bitBlock = 0;
                    break;
                case 'z':       /* None. */
                    bitBlock = ~0;
                    break;
                case 'n':       /* Do not display: */
                    for ( ptr = arg2 + 2; *ptr; ptr++ )
                        switch (LOWER(*ptr))
                        {
                            case 'u':
                                bitBlock |= PNOTE_URGENT;
                                break;
                            case 's':
                                bitBlock |= PNOTE_SOTD;
                                break;
                            case 'q':
                                bitBlock |= PNOTE_QOTD;
                                break;
                            case 'x':
                                bitBlock |= PNOTE_NODEL;
                                break;
                            case '-':
                                bitBlock |= PNOTE_STUPID;
                                break;
                            case '+':
                                bitBlock |= PNOTE_SMART;
                                break;
                            case 'g':
                                bitBlock |= PNOTE_GENERAL;
                                break;
                            case 'o':
                                fNewOnly = TRUE;
                                break;
                            case 'n':
                                fNewOnly = FALSE;
                                break;
                            default:
                                /* Do nothing with unrecognized flags. */
                                break;
                        }
                    break;
                case 'd':       /* Display: */
                    for ( ptr = arg2 + 2; *ptr; ptr++ )
                        switch (LOWER(*ptr))
                        {
                            case 'u':
                                bitBlock &= ~PNOTE_URGENT;
                                break;
                            case 's':
                                bitBlock &= ~PNOTE_SOTD;
                                break;
                            case 'q':
                                bitBlock &= ~PNOTE_QOTD;
                                break;
                            case 'x':
                                bitBlock &= ~PNOTE_NODEL;
                                break;
                            case '-':
                                bitBlock &= ~PNOTE_STUPID;
                                break;
                            case '+':
                                bitBlock &= ~PNOTE_SMART;
                                break;
                            case 'g':
                                bitBlock &= ~PNOTE_GENERAL;
                                break;
                            case 'o':
                                fNewOnly = FALSE;
                                break;
                            case 'n':
                                fNewOnly = TRUE;
                                break;
                            default:
                                /* Do nothing with unrecognized flags. */
                                break;
                        }
                    break;
                default:
                    /* Do nothing with unrecognized flags. */
                    break;
            }

            argument = one_argument(argument, arg2);
        }

        vnum = 0;
        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            if ( is_note_to(ch, pnote) )
            {
                vnum++;

                if ( pnote->flags <= 0 && (bitBlock & PNOTE_GENERAL) )
                    continue;
                else if ( fNewOnly && pnote->date_stamp <= ch->last_note )
                    continue;
                else if ( pnote->flags & bitBlock )
                    continue;
                else if ( *arg2 && str_infix(arg2, pnote->subject ) &&
                         str_prefix(arg2, pnote->sender))
                    continue;

                sprintf(buf, "[%3d%c] %s %s: %s&?&n\r\n",
                        vnum - 1,
                        (pnote->date_stamp > ch->last_note
                         && str_cmp(pnote->sender, ch->names)) ? 'N' : ' ',
                        (IS_SET(pnote->flags, PNOTE_URGENT) ? "&RURGENT!&n"
                         : IS_SET(pnote->flags,
                                  PNOTE_QOTD) ? "&BQOTD   &n" :
                         IS_SET(pnote->flags,
                                PNOTE_SOTD) ? "&cSOTD   &n" :
                         IS_SET(pnote->flags,
                                PNOTE_STUPID) ? "&mSTUPID &n" :
                         IS_SET(pnote->flags,
                                PNOTE_SMART) ? "&WSMART  &n" : "       "),
                        pnote->sender, pnote->subject);
                send_to_char(buf, ch);
            }
        }

        return;
    }

    if ( !str_prefix(arg, "catchup") )
    {
        for ( pnote = note_list; pnote; pnote = pnote->next )
            ch->last_note = UMAX(ch->last_note, pnote->date_stamp);
        send_to_char("Caught up.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "read") )
    {
        bool fAll;

        if ( !str_cmp(argument, "all") )
        {
            fAll = TRUE;
            anum = 0;
        }

        else if ( argument[0] == '\0' || !str_prefix(argument, "next") )
            /* read next unread note */
        {
            vnum = 0;
            for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
            {
                if ( is_note_to(ch, pnote ) &&
                    str_cmp(ch->names, pnote->sender) &&
                    ch->last_note < pnote->date_stamp)
                {
                    sprintf(buf, "[%3d] %s: %s\r\n%s\r\n",
                            vnum, pnote->sender, pnote->subject,
                            pnote->date);

                    if ( !ch->trust )
                    {
                        sprintf(buf + strlen(buf), "Type: %s\r\n",
                                (IS_SET(pnote->flags, PNOTE_URGENT) ?
                                 "&RURGENT!&n" : IS_SET(pnote->flags,
                                                        PNOTE_SOTD) ?
                                 "SOTD" : IS_SET(pnote->flags,
                                                 PNOTE_QOTD) ? "QOTD" :
                                 "General"));
                    }
                    else
                    {
                        int i;

                        strcat(buf, "Flags: ");

                        for ( i = 0; note_flags[i].name; i++ )
                        {
                            if (note_flags[i].show &&
                                IS_SET(pnote->flags, note_flags[i].bit))
                            {
                                strcat(buf, note_flags[i].name);
                                strcat(buf, " ");
                            }
                        }

                        strcat(buf, "\r\n");
                    }

                    sprintf(buf + strlen(buf), "To: %s\r\n",
                            pnote->to_list);
                    send_to_char(buf, ch);
                    page_to_char(pnote->text, ch);
                    ch->last_note = UMAX(ch->last_note, pnote->date_stamp);
                    return;
                }
                else if ( is_note_to(ch, pnote) )
                    vnum++;
            }
            send_to_char("You have no unread notes.\r\n", ch);
            return;
        }

        else if ( is_number(argument) )
        {
            fAll = FALSE;
            anum = atoi(argument);
        }
        else
        {
            send_to_char("Note read which number?\r\n", ch);
            return;
        }

        vnum = 0;
        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            if ( is_note_to(ch, pnote) && (vnum++ == anum || fAll) )
            {
                sprintf(buf, "[%3d] %s: %s\r\n%s\r\n",
                        vnum - 1, pnote->sender, pnote->subject,
                        pnote->date);

                if ( !ch->trust )
                {
                    sprintf(buf + strlen(buf), "Type: %s\r\n",
                            (IS_SET(pnote->flags, PNOTE_URGENT) ?
                             "&RURGENT!&n" : IS_SET(pnote->flags,
                                                    PNOTE_SOTD) ? "SOTD" :
                             IS_SET(pnote->flags,
                                    PNOTE_QOTD) ? "QOTD" : "General"));
                }
                else
                {
                    int i;

                    strcat(buf, "Flags: ");

                    for ( i = 0; note_flags[i].name; i++ )
                    {
                        if (note_flags[i].show &&
                            IS_SET(pnote->flags, note_flags[i].bit))
                        {
                            strcat(buf, note_flags[i].name);
                            strcat(buf, " ");
                        }
                    }

                    strcat(buf, "\r\n");
                }

                sprintf(buf + strlen(buf), "To: %s\r\n", pnote->to_list);
                send_to_char(buf, ch);
                send_to_char(pnote->text, ch);
                ch->last_note = UMAX(ch->last_note, pnote->date_stamp);
                return;
            }
        }

        send_to_char("No such note.\r\n", ch);
        return;
    }

    if ( IS_SET(ch->act, PLR_NONOTE) )
    {
        send_to_char("Your note privileges have been taken away.\r\n", ch);
        return;

    }

    if ( !str_cmp(arg, "revise") )
    {
        struct note_data *prev;

        if ( ch->pnote )
        {
            send_to_char("You're already editing a note.\r\n", ch);
            return;
        }

        one_argument(argument, arg2);
        anum = atoi(arg2);
        vnum = 0;
        prev = NULL;

        for ( pnote = note_list; pnote; prev = pnote, pnote = pnote->next )
            if ( is_note_to(ch, pnote) && (vnum++) == anum )
                break;

        if ( !pnote )
        {
            send_to_char("That's not a note.\r\n", ch);
            return;
        }
        else if ( str_cmp(pnote->sender, ch->names) )
        {
            if ( ch->trust )
            {
                int i;
                extern const struct god_type imp_table[];

                for ( i = 0; *imp_table[i].game_names; i++ )
                    if (is_name_exact
                        (pnote->sender, imp_table[i].game_names))
                        break;

                if ( *imp_table[i].game_names )
                {
                    if ( ch->trust <= imp_table[i].trust )
                    {
                        send_to_char
                            ("You can't revise a note by an admin of equal or greater power.\r\n",
                             ch);
                        return;
                    }
                }
            }
            else
            {
                send_to_char("You didn't write that note.\r\n", ch);
                return;
            }
        }

        if ( !prev )
            note_list = note_list->next;
        else
            prev->next = pnote->next;

        pnote->next = NULL;
        ch->pnote = pnote;

        if ( (fp = fopen(NOTE_FILE, "w")) == NULL )
        {
            logerr(NOTE_FILE);
        }
        else
        {
            for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
            {
                fprintf(fp, "Sender  %s~\n", pnote->sender);
                fprintf(fp, "Date    %s~\n", pnote->date);
                fprintf(fp, "Stamp   %lu\n",
                        (unsigned long) pnote->date_stamp);
                fprintf(fp, "Flags   %lu\n", pnote->flags);
                fprintf(fp, "To      %s~\n", pnote->to_list);
                fprintf(fp, "Subject %s~\n", pnote->subject);
                fprintf(fp, "Text\n%s~\n", pnote->text);
            }
            fclose(fp);
        }

        return;
    }

    if ( !str_cmp(arg, "block") )
    {
        note_attach(ch);

        if ( strlen(ch->pnote->text) >= MAX_STRING_LENGTH - MAX_INPUT_LENGTH )
        {
// TODO: let player know how long a note can be.
            send_to_char("Note too long.\r\n", ch);
            return;
        }

        send_to_char("Add your text below in a block.  Put &W~&n on a blank line to end.\r\n", ch);
        SET_BIT(ch->temp_flags, TEMP_BLOCK_ED);
        return;
    }

    if ( !str_cmp(arg, "+") )
    {
        note_attach(ch);
        strcpy(buf, ch->pnote->text);
        if ( strlen(buf) + strlen(argument) >= MAX_STRING_LENGTH - MAX_INPUT_LENGTH )
        {
// TODO: let player know how long a note can be.
            send_to_char("Note too long.\r\n", ch);
            return;
        }

        strcat(buf, argument);
        strcat(buf, "\r\n");
        free_string(ch->pnote->text);
        ch->pnote->text = str_dup(buf);
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_cmp(arg, "-") )
    {
        if (ch->pnote == NULL || !ch->pnote->text ||
            strlen(ch->pnote->text) < 2)
        {
            send_to_char("You have to write something first!\r\n", ch);
            return;
        }

        strcpy(buf, ch->pnote->text);
        free_string(ch->pnote->text);
        ptr = buf + strlen(buf) - 2;
        *ptr = '\0';

        ptr = strrchr(buf, '\n');

        if ( !ptr )
            *buf = '\0';
        else
            *(++ptr) = '\0';

        ch->pnote->text = str_dup(buf);
        send_to_char("Ok.\r\n", ch);
        return;
    }


    if ( !str_prefix(arg, "subject") )
    {
        note_attach(ch);
        free_string(ch->pnote->subject);
        ch->pnote->subject = str_dup(argument);
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "to") )
    {
        note_attach(ch);
        free_string(ch->pnote->to_list);
        ch->pnote->to_list = str_dup(argument);
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "type") )
    {
        note_attach(ch);
        one_argument(argument, arg);

        if ( !*arg )
        {
            send_to_char("Types of notes: General, SOTD, QOTD.\r\n", ch);
            return;
        }
        else if ( !str_prefix(arg, "sotd") )
        {
            ch->pnote->flags &= ~PNOTE_QOTD;
            ch->pnote->flags |= PNOTE_SOTD;
        }
        else if ( !str_prefix(arg, "qotd") )
        {
            ch->pnote->flags &= ~PNOTE_SOTD;
            ch->pnote->flags |= PNOTE_QOTD;
        }
        else if ( !str_prefix(arg, "none") || !str_prefix(arg, "general") )
            ch->pnote->flags &= ~(PNOTE_SOTD | PNOTE_QOTD);
        else
        {
            send_to_char("What type of note is that?\r\n", ch);
            return;
        }

        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "clear") )
    {
        if ( ch->pnote != NULL )
        {
            free_string(ch->pnote->text);
            free_string(ch->pnote->subject);
            free_string(ch->pnote->to_list);
            free_string(ch->pnote->date);
            free_string(ch->pnote->sender);
            ch->pnote->flags = 0L;
            ch->pnote->next = note_free;
            note_free = ch->pnote;
            ch->pnote = NULL;
        }

        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "show") )
    {
        if ( ch->pnote == NULL )
        {
            send_to_char("You have no note in progress.\r\n", ch);
            return;
        }

        sprintf(buf, "%s: %s\r\nType: %s\r\nTo: %s\r\n",
                ch->pnote->sender, ch->pnote->subject,
                (IS_SET(ch->pnote->flags, PNOTE_SOTD) ? "SOTD" :
                 IS_SET(ch->pnote->flags, PNOTE_QOTD) ? "QOTD" :
                 "General"), ch->pnote->to_list);
        send_to_char(buf, ch);
        send_to_char(ch->pnote->text, ch);
        return;
    }

    if ( !str_prefix(arg, "post") || !str_prefix(arg, "send") )
    {
        FILE *fp;
        char *strtime;

        if ( ch->pnote == NULL )
        {
            send_to_char("You have no note in progress.\r\n", ch);
            return;
        }

        if ( !str_cmp(ch->pnote->to_list, "") )
        {
            send_to_char
                ("You need to provide a recipient (name, all, or immortal).\r\n",
                 ch);
            return;
        }

        if ( !str_cmp(ch->pnote->subject, "") )
        {
            send_to_char("You need to provide a subject.\r\n", ch);
            return;
        }

        for (vnum = 0, pnote = note_list; pnote;
             pnote = pnote->next, vnum++)
            ;

        if ( vnum > 200 )
        {
// TODO: seems arbitrary.
            send_to_char("There are too many notes right now.  Sorry.\r\n",
                         ch);
            return;
        }

        ch->pnote->next = NULL;
        strtime = ctime(&current_time);
        strtime[strlen(strtime) - 1] = '\0';
        ch->pnote->date = str_dup(strtime);
        ch->pnote->date_stamp = current_time;

        if ( note_list == NULL )
        {
            note_list = ch->pnote;
        }
        else
        {
            for (pnote = note_list; pnote->next != NULL;
                 pnote = pnote->next) ;
            pnote->next = ch->pnote;
        }
        pnote = ch->pnote;
        ch->pnote = NULL;

        if ( (fp = fopen(NOTE_FILE, "a")) == NULL )
        {
            logerr(NOTE_FILE);
        }
        else
        {
            fprintf(fp, "Sender  %s~\n", pnote->sender);
            fprintf(fp, "Date    %s~\n", pnote->date);
            fprintf(fp, "Stamp   %lu\n",
                    (unsigned long) pnote->date_stamp);
            fprintf(fp, "Flags   %lu\n", pnote->flags);
            fprintf(fp, "To      %s~\n", pnote->to_list);
            fprintf(fp, "Subject %s~\n", pnote->subject);
            fprintf(fp, "Text\n%s~\n", pnote->text);
            fclose(fp);
        }

// TODO: seems sane to only notify people of new notes when not in the
// play area, and notify them once they've spawned back in.  Or we could
// notify them on login.
        for ( victim = char_list; victim != NULL; victim = victim->next )
        {
            if ( IS_SET(victim->comm, COMM_QUIET ) ||
                !is_note_to(victim, pnote))
                continue;
            act_new("$n has added a new note.  Type &WNOTE&n to read it.",
                    ch, NULL, victim, TO_VICT, POS_DEAD);
        }
        send_to_char("Ok.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "remove") )
    {
        if ( !is_number(argument) )
        {
            send_to_char("Note remove which number?\r\n", ch);
            return;
        }

        anum = atoi(argument);
        vnum = 0;
        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            if ( is_note_to(ch, pnote) && vnum++ == anum )
            {
                note_remove(ch, pnote);
                send_to_char("Ok.\r\n", ch);
                return;
            }
        }

        send_to_char("No such note.\r\n", ch);
        return;
    }

    if ( !str_prefix(arg, "delete") && get_trust(ch) )
    {
        if ( !is_number(argument) )
        {
            send_to_char("Note delete which number?\r\n", ch);
            return;
        }

        anum = atoi(argument);
        vnum = 0;
        for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
        {
            if ( is_note_to(ch, pnote) && vnum++ == anum )
            {
                note_delete(pnote);
                send_to_char("Ok.\r\n", ch);
                return;
            }
        }

        send_to_char("No such note.\r\n", ch);
        return;
    }

    send_to_char("Huh?  Type 'help note' for usage.\r\n", ch);
    return;
}


/* RT code to delete yourself */

void
do_delet (CHAR_DATA * ch, char *argument)
{
    send_to_char("You must type the full command to delete yourself.\r\n",
                 ch);
}

// TODO: needs check so that players can be forced to delete.
void
do_delete (CHAR_DATA * ch, char *argument)
{
    char file[MAX_INPUT_LENGTH];
    char save[MAX_INPUT_LENGTH];

    if ( IS_NPC(ch) )
        return;

    if ( ch->pcdata->confirm_delete )
    {
        if ( argument[0] != '\0' )
        {
            send_to_char("Delete status removed.\r\n", ch);
            ch->pcdata->confirm_delete = FALSE;
            return;
        }
        else
        {
            send_to_char
                ("See ya here next time you get the urge to kill."
                 "\r\n\r\n", ch);
            send_to_char("&g                |\r\n", ch);
            send_to_char("&g               |.|\r\n", ch);
            send_to_char("&g               |.|\r\n", ch);
            send_to_char("&g              |\\./|\r\n", ch);
            send_to_char("&g              |\\./|\r\n", ch);
            send_to_char("&g .            |\\./|             .\r\n", ch);
            send_to_char("&g  \\^.\\        |\\./|         /.^/ \r\n", ch);
            send_to_char("&g   \\-.|\\      |\\./|       /|.-/ \r\n", ch);
            send_to_char("&g    \\--.|\\    |\\./|     /|.--/ \r\n", ch);
            send_to_char("&g     \\--.|\\   |\\./|    /|.--/ \r\n", ch);
            send_to_char("&g      \\--.|\\  |\\./|  /|.--/ \r\n", ch);
            send_to_char("&g         \\ .\\  |.|  /. / \r\n", ch);
            send_to_char("&g_ -_^_^_^_-  \\ \\ // /  -_^_^_^_- _\r\n", ch);
            send_to_char
                ("&g - -/_/_/- ^ ^  |  ^ ^ -\\_\\_\\- -  \r\n\r\n\r\n",
                 ch);

            if ( ch->desc )
                close_socket(ch->desc);

            sprintf(file, "%sPS%s", PLAYER_DIR, capitalize(ch->names));
            sprintf(save, "%sPS%s", BAK_PLAYER_DIR, capitalize(ch->names));
            extract_char(ch, TRUE);
            rename(file, save);

            return;
        }
    }

    if ( argument[0] != '\0' )
    {
        send_to_char("Just type delete. No argument.\r\n", ch);
        return;
    }

    send_to_char("Type delete again to confirm this command.\r\n", ch);
    send_to_char("WARNING: this command is irreversible.\r\n", ch);
    send_to_char
        ("Typing delete with an argument will undo delete status.\r\n",
         ch);
    ch->pcdata->confirm_delete = TRUE;
}

/* RT quiet blocks out all communication */
void
do_quiet (CHAR_DATA * ch, char *argument)
{
    if ( IS_SET(ch->comm, COMM_QUIET) )
    {
        send_to_char("Quiet mode removed.\r\n", ch);
        REMOVE_BIT(ch->comm, COMM_QUIET);
    }
    else
    {
        send_to_char
            ("From now on, you will only hear says and emotes.\r\n", ch);
        SET_BIT(ch->comm, COMM_QUIET);
    }
}

void
do_kills (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;

    if ( ch )
        if ( IS_SET(ch->comm, COMM_NOKILLS) )
        {
            send_to_char("You will now be notified of kills.\r\n", ch);
            REMOVE_BIT(ch->comm, COMM_NOKILLS);
        }
        else
        {
            send_to_char("You will no longer be notified of kills.\r\n",
                         ch);
            SET_BIT(ch->comm, COMM_NOKILLS);
        }
    else
    {
        for ( victim = char_list; victim != NULL; victim = victim->next )
        {
            if (!ground0_down &&
                (IS_SET(victim->comm, COMM_NOKILLS) ||
                 IS_SET(victim->comm, COMM_QUIET)))
                continue;
            send_to_char(argument, victim);
        }
    }
}

void
do_commbadge (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;

    *buf = '\0';

    if ( argument[0] == '\0' )
    {
        if ( IS_SET(ch->comm, COMM_NOBOUNTY) )
        {
            send_to_char("&rCommBadge&n is now ON.\r\n", ch);
            REMOVE_BIT(ch->comm, COMM_NOBOUNTY);
        }
        else
        {
            send_to_char("&rCommBadge&n is now OFF.\r\n", ch);
            SET_BIT(ch->comm, COMM_NOBOUNTY);
        }
        return;
    }

    if ( ch )
    {
        if ( IS_SET(ch->comm, COMM_QUIET) )
        {
            send_to_char("You must turn off quiet mode first.\r\n", ch);
            return;
        }
        else if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
        {
            send_to_char
                ("The gods have revoked your &rCommBadge&n priviliges."
                 "\r\n", ch);
            return;
        }

        REMOVE_BIT(ch->comm, COMM_NOBOUNTY);

        sprintf(buf, "&nYou &XCOMMBADGE&n: '&W%s&?&n'\r\n", argument);
        send_to_char(buf, ch);
    }

    for ( victim = char_list; victim != NULL; victim = victim->next )
    {
        if ( IS_SET(victim->comm, COMM_NOBOUNTY ) ||
            IS_SET(victim->comm, COMM_QUIET))
            continue;
        if ( ((argument[0] == '%') && ch && IS_IMMORTAL(ch)) || !ch )
            act_new("&XCOMMBADGE&n: '&W$t&?&n'", victim,
                    argument + 1, NULL, TO_CHAR, POS_DEAD);
        else
            act_new("$n &XCOMMBADGE&n: '&W$t&?&n'", ch,
                    argument, victim, TO_VICT, POS_DEAD);
    }
}


void
do_imptalk (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;

    if ( argument[0] == '\0' )
    {
        send_to_char("You cannot turn off IMP talk.\r\n", ch);
        return;
    }

    sprintf(buf, "$n: %s", argument);
    act_new("[IMP] $n: $t", ch, argument, NULL, TO_CHAR, POS_DEAD);
    for ( d = descriptor_list; d != NULL; d = d->next )
    {
        if ( d->connected == CON_PLAYING && IS_IMP(d->character) )
        {
            act_new("[IMP] $n: $t", ch, argument, d->character, TO_VICT,
                    POS_DEAD);
        }
    }

}

void
do_immtalk (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *listeners;

    if ( argument[0] == '\0' )
    {
        if ( IS_SET(ch->comm, COMM_NOWIZ) )
        {
            send_to_char("Immortal channel is now ON\r\n", ch);
            REMOVE_BIT(ch->comm, COMM_NOWIZ);
        }
        else
        {
            send_to_char("Immortal channel is now OFF\r\n", ch);
            SET_BIT(ch->comm, COMM_NOWIZ);
        }
        return;
    }

    if ( ch )
        REMOVE_BIT(ch->comm, COMM_NOWIZ);

    sprintf(buf, "$n: %s", argument);

    if ( ch )
        act_new("$n: $t", ch, argument, NULL, TO_CHAR, POS_DEAD);

    for ( listeners = char_list; listeners; listeners = listeners->next )
    {
        if ( IS_HERO(listeners) && !IS_SET(listeners->comm, COMM_NOWIZ) )
        {
            if ( ch )
                act_new("$n: $t", ch, argument, listeners, TO_VICT,
                        POS_DEAD);
            else
                act_new("&uAMR SELF-DESTRUCT&n: $t", listeners,
                        &(argument[1]), NULL, TO_CHAR, POS_DEAD);
        }
    }
}


void
do_say (CHAR_DATA * ch, char *argument)
{
    if ( argument[0] == '\0' )
    {
        send_to_char("Say what?\r\n", ch);
        return;
    }

    if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your communication priviliges.\r\n",
                     ch);
        return;
    }

    act("You say '&us$T&?&n'", ch, NULL, argument, TO_CHAR);
    act("$n says '&us$T&?&n'", ch, NULL, argument, TO_ROOM);
}


void
do_tell (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    if ( IS_SET(ch->comm, COMM_NOTELL) )
    {
        send_to_char("Your message didn't get through.\r\n", ch);
        return;
    }

    if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your communication priviliges.\r\n",
                     ch);
        return;
    }

    if ( IS_SET(ch->comm, COMM_QUIET) )
    {
        send_to_char("You must turn off quiet mode first.\r\n", ch);
        return;
    }
    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' || argument[0] == '\0' )
    {
        send_to_char("Tell whom what?\r\n", ch);
        return;
    }

    /*
     * Can tell to PC's anywhere, but NPC's only in same room.
     * -- Furey
     */
    if ( (victim = get_char_world(ch, arg) ) == NULL
        || (IS_NPC(victim) && victim->in_room != ch->in_room))
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim->desc == NULL && !IS_NPC(victim) )
    {
        act("$N seems to be linkdead.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if ( !IS_IMMORTAL(ch) && !IS_AWAKE(victim) )
    {
        act("$E can't hear you.", ch, 0, victim, TO_CHAR);
        return;
    }

    if ( IS_SET(victim->comm, COMM_QUIET) && !IS_IMMORTAL(ch) )
    {
        act("$E is not receiving tells.", ch, 0, victim, TO_CHAR);
        return;
    }

    if (!ch->trust && !victim->trust &&
        ch->team != victim->team &&
        !team_table[ch->team].independent &&
        !team_table[victim->team].independent &&
        team_table[victim->team].active)
    {
        act("$N is not on your team.", ch, 0, victim, TO_CHAR);
        return;
    }

    act("You tell $N '&ut$t&?&n'", ch, argument, victim, TO_CHAR);
    act_new("$n tells you '&ut$t&?&n'", ch, argument, victim, TO_VICT,
            POS_DEAD);
    victim->reply = ch;

    return;
}

void
do_reply (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *victim;

    if ( IS_SET(ch->comm, COMM_NOTELL) )
    {
        send_to_char("Your message didn't get through.\r\n", ch);
        return;
    }

    if ( (victim = ch->reply) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim->desc == NULL && !IS_NPC(victim) )
    {
        act("$N seems to have misplaced $S link...try again later.", ch,
            NULL, victim, TO_CHAR);
        return;
    }

    if ( !IS_IMMORTAL(ch) && !IS_AWAKE(victim) )
    {
        act("$E can't hear you.", ch, 0, victim, TO_CHAR);
        return;
    }

    if ( IS_SET(victim->comm, COMM_QUIET) && !IS_IMMORTAL(ch) )
    {
        act("$E is not receiving tells.", ch, 0, victim, TO_CHAR);
        return;
    }

    act("You reply to $N '&ut$t&?&n'", ch, argument, victim, TO_CHAR);
    act_new("$n replies '&ut$t&?&n'", ch, argument, victim, TO_VICT,
            POS_DEAD);
    victim->reply = ch;

    return;
}

void
do_emote (CHAR_DATA * ch, char *argument)
{
    if ( !IS_NPC(ch) && IS_SET(ch->comm, COMM_NOEMOTE | COMM_NOSOCIALS) )
    {
        send_to_char("You can't show your emotions.\r\n", ch);
        return;
    }

    if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your communication priviliges.\r\n", ch);
        return;
    }

    if ( argument[0] == '\0' )
    {
        send_to_char("Emote what?\r\n", ch);
        return;
    }

    act("$n $T&?&n", ch, NULL, argument, TO_ROOM | TO_NOT_NOSOCIALS);
    act("$n $T&?&n", ch, NULL, argument, TO_CHAR | TO_NOT_NOSOCIALS);
}

void
do_idea (CHAR_DATA * ch, char *argument)
{
    while ( isspace(*argument) )
        argument++;

    if ( *argument )
    {
        append_file(ch, IDEA_FILE, argument);
        send_to_char("Idea logged.\r\n", ch);
        return;
    }

    send_to_char("No bright ideas, huh?!\r\n", ch);
}


void
do_typo (CHAR_DATA * ch, char *argument)
{
    while ( isspace(*argument) )
        argument++;

    if ( *argument )
    {
        append_file(ch, TYPO_FILE, argument);
        send_to_char("Typo logged.\r\n", ch);
        return;
    }

    send_to_char("You just made one.\r\n", ch);
}


void
do_quit (CHAR_DATA * ch, char *argument)
{
    send_to_char("Goodbye!\r\n", ch);
    if ( IS_IMMORTAL(ch) )
    {
        save_char_obj(ch);

        if ( ch->desc )
            close_socket(ch->desc);

        extract_char(ch, TRUE);
        return;
    }

    if ( ch->desc )
        close_socket(ch->desc);
}

void
do_save (CHAR_DATA * ch, char *argument)
{
    if ( IS_NPC(ch) )
        return;

    save_char_obj(ch);
    send_to_char("You have been saved.\r\n", ch);
    return;
}

void
die_follower (CHAR_DATA * ch)
{
    CHAR_DATA *fch;

    if ( ch->leader )
        stop_follower(ch);

    for ( fch = char_list; fch != NULL; fch = fch->next )
        if ( fch->leader == ch )
            stop_follower(fch);
}

void
do_follow (CHAR_DATA * ch, char *argument)
{
/* RT changed to allow unlimited following and follow the NOFOLLOW rules */
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
        if ( ch->allows_followers )
        {
            send_to_char("You no longer allow any followers.\r\n", ch);
            ch->allows_followers = 0;
            die_follower(ch);
            return;
        }
        else
        {
            send_to_char("Members of your team can follow you now.\r\n",
                         ch);
            ch->allows_followers = 1;
            return;
        }
    }

    if ( (victim = get_char_room(ch, arg)) == NULL )
    {
        send_to_char("They aren't here.\r\n", ch);
        return;
    }

    if ( victim == ch )
    {
        if ( ch->leader == NULL )
        {
            send_to_char("You already follow yourself.\r\n", ch);
            return;
        }
        stop_follower(ch);
        return;
    }

    if ( ch->leader != NULL )
        stop_follower(ch);

    if ( victim->leader )
    {
        send_to_char("Follow someone who isn't already following someone "
                     "else.\r\n", ch);
        return;
    }

    if ( !victim->allows_followers )
    {
        act("$N doesn't look like $E wants you to follow $M.", ch, NULL,
            victim, TO_CHAR);
        act("$n tries to follow you, but you aren't allowing followers.",
            ch, NULL, victim, TO_VICT);
        return;
    }

    add_follower(ch, victim);
}


void
add_follower (CHAR_DATA * ch, CHAR_DATA * leader)
{
    if ( ch->leader != NULL )
    {
        logmesg("Error: add_follower: non-null leader.");
        return;
    }

    ch->leader = leader;

    if ( can_see(leader, ch) )
        act("$n now follows you.", ch, NULL, leader, TO_VICT);

    act("You now follow $N.", ch, NULL, leader, TO_CHAR);

    return;
}



void
stop_follower (CHAR_DATA * ch)
{
    if ( ch->leader == NULL )
    {
        logmesg("Error: stop_follower: null leader.");
        return;
    }

    if ( can_see(ch->leader, ch) && ch->in_room != NULL )
    {
        act("$n stops following you.", ch, NULL, ch->leader, TO_VICT);
        act("You stop following $N.", ch, NULL, ch->leader, TO_CHAR);
    }

    ch->leader = NULL;
    return;
}


void
do_characters (CHAR_DATA * ch, char *argument)
{
    int num;

    if ( IS_NPC(ch) )
        return;

    if ( !is_number(argument) )
    {
        send_to_char("Syntax: chars <number>\r\n", ch);
        return;
    }

    if ( ((num = atoi(argument)) < 500) || (num > 32000) )
    {
        send_to_char("The number must be in the range 500 - 32000.\r\n",
                     ch);
        return;
    }

    ch->desc->max_out = num;
}


void
gocial_act (CHAR_DATA * ch, int cmd, CHAR_DATA * vict, int toTp)
{
    char buf[MAX_STRING_LENGTH + 16];
    struct descriptor_data *d;
    struct char_data *to;
    char *ptr;

    char *act_parse(const char *, CHAR_DATA *, CHAR_DATA *,
                    const void *, const void *);

    if ( (ch == vict || !vict) && toTp == TO_VICT )
        return;
    if ( !ch )
        return;

    switch (toTp)
    {
        case TO_CHAR:
            ptr = (vict ? (vict == ch ? social_table[cmd].char_auto
                           : social_table[cmd].char_found)
                   : social_table[cmd].char_no_arg);
            break;
        case TO_VICT:
            ptr = social_table[cmd].vict_found;
            break;
        case TO_NOTVICT:
            ptr = (vict ? (vict == ch ? social_table[cmd].others_auto
                           : social_table[cmd].others_found)
                   : social_table[cmd].others_no_arg);
            break;
        default:
            logmesg
                ("Internal error -- gocial_act() called with toTp=%d",
                 toTp);
            return;
    }

    if ( !ptr || !*ptr )
        return;

    for ( d = descriptor_list; d; d = d->next )
    {
        if ( d->connected != CON_PLAYING || (to = d->character) == NULL )
            continue;
        if ( IS_SET(to->comm, COMM_QUIET) ||
             IS_SET(to->comm, COMM_NOSOCIALS) )
            continue;

        /*
         * We could more efficiently special-case these outside the loop, but it's
         * cleaner and more compact this way.
         */
        if ( toTp == TO_CHAR && to != ch )
            continue;
        if ( toTp == TO_VICT && to != vict )
            continue;
        if ( toTp == TO_NOTVICT && (to == ch || to == vict) )
            continue;

        strcpy(buf, "&BGocial:&n ");
        strcat(buf, act_parse(ptr, to, ch, NULL, vict));
        send_to_char(buf, to);
    }
}


void
do_gocial (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *vict = NULL;
    int cmd;

    if ( !IS_NPC(ch) && IS_SET(ch->comm, COMM_NOEMOTE | COMM_NOSOCIALS) )
    {
        send_to_char("You can't show your emotions.\r\n", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("What do you wish to gocial?\r\n", ch);
        return;
    }

    for ( cmd = 0; social_table[cmd].name; cmd++ )
        if ( !str_prefix(arg, social_table[cmd].name) )
            break;

    if ( !social_table[cmd].name )
    {
        send_to_char("What kind of social is that?!?!\r\n", ch);
        return;
    }
    else if ( !IS_NPC(ch) && IS_SET(ch->comm, COMM_QUIET) )
    {
        send_to_char("You must turn off quiet mode first.\r\n", ch);
        return;
    }
    else if ( !IS_NPC(ch) && IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your communication priviliges.\r\n",
                     ch);
        return;
    }

    one_argument(argument, arg);

    if ( *arg && !(vict = get_char_world(ch, arg)) )
    {
        send_to_char("Who's that?\r\n", ch);
        return;
    }

    gocial_act(ch, cmd, vict, TO_CHAR);
    gocial_act(ch, cmd, vict, TO_VICT);
    gocial_act(ch, cmd, vict, TO_NOTVICT);
}


void
do_teamtalk (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    if ( IS_SET(ch->comm, COMM_NOCHANNELS) )
    {
        send_to_char("The gods have revoked your communication priviliges.\r\n",
                     ch);
        return;
    }
    if ( argument[0] == '\0' || team_table[ch->team].independent )
        return;

    sprintf(buf, "%s %s&X: '&W%s&?&X'&n\r\n", ch->names,
            team_table[ch->team].who_name, argument);
    send_to_team(buf, ch->team);
}


void
do_clear (struct char_data *ch, char *argument)
{
    clear_screen(ch);
}


void
do_lines (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int lines = 0;

    if ( IS_NPC(ch) || !ch->pcdata )
        return;

    one_argument(argument, arg);

    if ( !*arg )
    {
        if ( ch->pcdata->lines == 0 )
        {
            send_to_char("You have pagination disabled.\r\n", ch);
            return;
        }
        else if ( ch->pcdata->lines == -1 )
        {
            send_to_char("You have page length autodetected.\r\n", ch);
            return;
        }

        printf_to_char(ch,
                       "Your terminal screen length is set to %d lines.",
                       ch->pcdata->lines);
        return;
    }
    else if ( !str_cmp(arg, "test") )
    {
        char buf[MAX_STRING_LENGTH];
        int bufp = 0;
        int x;

        for ( x = 100; x > 1; x-- )
            bufp += sprintf(buf + bufp, "%d\r\n", x);

        send_to_char(buf, ch);
        if ( ch->desc )
            ch->desc->fcommand = FALSE;
        return;
    }
    else if ( !str_cmp(arg, "off") || !str_cmp(arg, "0") )
    {
        send_to_char("Disabled pagination.\r\n", ch);
        ch->pcdata->lines = 0;
        return;
    }
    else if ( !str_cmp(arg, "auto") )
    {
        ch->pcdata->lines = -1;
        send_to_char("Enabled autodetection of pagination.\r\n", ch);
        if ( ch->desc )
            IAC_option(ch->desc, WILL, TELOPT_NAWS);
        return;
    }
    else if ( !isdigit(*arg) )
    {
        send_to_char("USAGE: lines [number]\r\n", ch);
        return;
    }
    else if ( (lines = atoi(arg)) < 6 || lines > 150 )
    {
        send_to_char("You can only have between 6 and 150 lines.\r\n", ch);
        return;
    }

    if ( ch->desc )
        ch->desc->lines = lines;

    ch->pcdata->lines = lines;
    printf_to_char(ch, "Set page size to %d lines.\r\n",
                   ch->pcdata->lines);
}


void
do_demosocial (struct char_data *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int cmd;

    one_argument(argument, arg);

    for ( cmd = 0; social_table[cmd].name; cmd++ )
        if (*arg == *social_table[cmd].name &&
            !str_prefix(arg, social_table[cmd].name))
            break;

    if ( !social_table[cmd].name )
    {
        send_to_char("What social is that?\r\n", ch);
        return;
    }

    send_to_char("&X[&WNo Argument&X]&n\r\n", ch);
    sprintf(buf, "&cOthers&X: &n%s", social_table[cmd].others_no_arg);
    act(buf, ch, NULL, NULL, TO_CHAR);
    sprintf(buf, "&cSelf  &X: &n%s", social_table[cmd].char_no_arg);
    act(buf, ch, NULL, NULL, TO_CHAR);

    if ( social_table[cmd].char_auto && *social_table[cmd].char_auto )
    {
        send_to_char("\r\n&X[&WOn Self&X]&n\r\n", ch);
        sprintf(buf, "&cOthers&X: &n%s", social_table[cmd].others_auto);
        act(buf, ch, NULL, ch, TO_CHAR);
        sprintf(buf, "&cSelf  &X: &n%s", social_table[cmd].char_auto);
        act(buf, ch, NULL, ch, TO_CHAR);
    }

    if ( social_table[cmd].char_found && *social_table[cmd].char_found )
    {
        struct room_data *oldroom;

        oldroom = enforcer->in_room;
        char_from_room(enforcer);
        char_to_room(enforcer, ch->in_room);

        send_to_char("\r\n&X[&WOn Victim&X]&n\r\n", ch);
        sprintf(buf, "&cOthers&X: &n%s", social_table[cmd].others_found);
        act(buf, ch, NULL, enforcer, TO_CHAR);
        sprintf(buf, "&cVictim&X: &n%s", social_table[cmd].vict_found);
        act(buf, ch, NULL, enforcer, TO_CHAR);
        sprintf(buf, "&cSelf  &X: &n%s", social_table[cmd].char_found);
        act(buf, ch, NULL, enforcer, TO_CHAR);

        char_from_room(enforcer);
        char_to_room(enforcer, oldroom);
    }
}


char *
no_newline (const char *str)
{
    static char buf[MAX_STRING_LENGTH];
    int len = strlen(str);

    if ( *(str + len - 1) == '\n' )
        len--;

    strncpy(buf, str, len);
    *(buf + len) = '\0';
    return (buf);
}


void
do_beeps (struct char_data *ch, char *argument)
{
    if ( IS_SET(ch->comm, COMM_NOBEEP) )
    {
        send_to_char("Beeps enabled.\r\n", ch);
        REMOVE_BIT(ch->comm, COMM_NOBEEP);
    }
    else
    {
        send_to_char("Beeps disabled.\r\n", ch);
        SET_BIT(ch->comm, COMM_NOBEEP);
    }
}


void
do_noskulls (struct char_data *ch, char *argument)
{
    if ( IS_SET(ch->comm, COMM_NOSKULLS) )
    {
        send_to_char("Death skull enabled.\r\n", ch);
        REMOVE_BIT(ch->comm, COMM_NOSKULLS);
    }
    else
    {
        send_to_char("Death skull disabled.\r\n", ch);
        SET_BIT(ch->comm, COMM_NOSKULLS);
    }
}


void
do_nosocials (struct char_data *ch, char *argument)
{
    if ( IS_SET(ch->comm, COMM_NOSOCIALS) )
    {
        send_to_char("You will now see socials.\r\n", ch);
        REMOVE_BIT(ch->comm, COMM_NOSOCIALS);
    }
    else
    {
        send_to_char("You will no longer see socials.\r\n", ch);
        SET_BIT(ch->comm, COMM_NOSOCIALS);
    }
}
