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
 * Module:      ban.c
 * Synopsis:    Administrativia: Handle disallowed sites/users.
 * Author:      Satori
 * Created:     28 Mar 2002
 *
 * NOTES:
 */

#include "ground0.h"


struct ban_data
{
    uint match_:1;              /* For use by DisplayBans().            */
    uint newbie:1;              /* Don't allow normal creation.         */

    char *immortal;             /* Who created the ban?                 */
    char *site;                 /* What site is banned?                 */
    char *reason;               /* For what reason?                     */
    time_t date;                /* When did he do it?                   */
    time_t expirationDate;      /* ...and when does it expire? (if ever) */

    struct ban_data *next;      /* Next in the list.                    */
};


static struct ban_data *bans = NULL;
static struct ban_data *bans_free = NULL;


void
ListBans (struct char_data *ch, char *arguments)
{
    bool newbieOnly = FALSE;
    bool fullOnly = FALSE;
    bool processFirst = FALSE;  /* Use first arg as search pattern? */
    int total;                  /* Total number of bans.            */

    struct ban_data *ban;

    char arg[MAX_INPUT_LENGTH];

    arguments = one_argument(arguments, arg);

    /* The first argument selects the type of bans to be displayed. */
    if ( !str_cmp(arg, "-newbie") )
        newbieOnly = TRUE;
    else if ( !str_cmp(arg, "-full") )
        fullOnly = TRUE;
    else if ( !str_cmp(arg, "-all") )
        newbieOnly = fullOnly = FALSE;
    else
        processFirst = TRUE;

    /* This loop also counts the total number of bans, now. */
    /* Disqualify based upon first argument; qualify the rest. */
    for ( total = 0, ban = bans; ban; total++, ban = ban->next )
    {
        if ( newbieOnly && !ban->newbie )
            ban->match_ = 0;
        else if ( fullOnly && ban->newbie )
            ban->match_ = 0;
        else
            ban->match_ = 1;
    }

    if ( !processFirst )
        arguments = one_argument(arguments, arg);

    /* Run through all remaining arguments and disqualify on text matches. */
    while ( *arg )
    {
        for ( ban = bans; ban; ban = ban->next )
        {
            if ( str_prefix(arg, ban->immortal ) &&
                str_prefix(arg, ban->site) && !strstr(arg, ban->reason))
            {
                /*
                 * Not part of the banned site, the banning imm's name, or the
                 * reason for banning, so disqualify it from display.
                 */
                ban->match_ = 0;
            }
        }

        arguments = one_argument(arguments, arg);
    }

    /* Display the header: */
    send_to_char("&mSITE            BANNED DATE / BY          REASON\r\n",
                 ch);

    char buf[MAX_STRING_LENGTH];
    char timestr[12];
    int count = 0;
    int k = 0;

    /* Now go through and buffer up all of the qualified ban records: */
    for ( ban = bans; ban; ban = ban->next )
    {
        if ( ban->match_ == 0 )
            continue;

        count++;
        strftime(timestr, 12, "%d %b %Y", localtime(&ban->date));

        k += sprintf(buf + k,
                     "&%c%-15.15s &r%-11.11s &X/ &W%-11.11s &c%.35s\r\n",
                     (ban->newbie ? 'C' : 'B'),
                     ban->site, timestr, ban->immortal, ban->reason);

        if ( k >= MAX_STRING_LENGTH - 128 )
        {
            /* Flush buf when it starts to get full. */
            send_to_char(buf, ch);
            k = 0;
        }
    }

    /* Reset color and flush the remainder of the buffer. */
    sprintf(buf + k, "&n\r\n&B%d&X/&c%d &wbans shown.\r\n", count, total);
    send_to_char(buf, ch);
}


const char *
GetNewbieBan (const char *site)
{
    struct ban_data *ban;

    for ( ban = bans; ban; ban = ban->next )
    {
        if ( ban->newbie == 0 )
            continue;
        if ( !str_suffix(ban->site, site) || !str_prefix(ban->site, site) )
            return ban->reason;
    }

    return NULL;
}


const char *
GetFullBan (const char *site)
{
    struct ban_data *ban;

    for ( ban = bans; ban; ban = ban->next )
    {
        if ( ban->newbie )
            continue;
        if ( str_suffix(ban->site, site) && str_prefix(ban->site, site) )
            continue;

        return ban->reason;
    }

    return NULL;
}


struct ban_data *
MakeBan (struct char_data *ch, time_t date)
{
    struct ban_data *bd;

    if ( bans_free )
    {
        bd = bans_free;
        bans_free = bans_free->next;
    }
    else
    {
        bd = alloc_perm(sizeof(struct ban_data));
    }

    bd->date = date;
    bd->immortal = (ch ? strdup(ch->names) : NULL);
    bd->site = NULL;
    bd->reason = NULL;
    bd->expirationDate = 0;
    bd->newbie = 0;

    /* Sorted insertion */
    if ( bans == NULL || date <= bans->date )
    {
        bd->next = bans;
        bans = bd;
    }
    else
    {
        struct ban_data *point = bans;

        while ( point->next && point->next->date <= date )
            point = point->next;

        bd->next = point->next;
        point->next = bd;
    }

    return bd;
}


void
DestroyBan (struct ban_data *bd, bool remove)
{
    if ( bd->immortal )
        free(bd->immortal);
    if ( bd->site )
        free(bd->site);
    if ( bd->reason )
        free(bd->reason);

    if ( remove )
    {
        if ( bans == bd )
        {
            bans = bans->next;
        }
        else
        {
            struct ban_data *temp = bans;

            while ( temp && temp->next != bd )
                temp = temp->next;
            if ( temp )
                temp->next = bd->next;
        }
    }

    bd->next = bans_free;
    bans_free = bd;
}


void
LoadBans (FILE * fp)
{
    char name[32], site[32], temp[16], reason[256];
    struct ban_data *bd;
    char line[256];
    int lineNo = 0;
    int count = 0;
    long date;
    int i;

    logmesg("Banning troublesome sites...");

    while ( (i = get_line(fp, line, 256)) != -1 && *line != '$' )
    {
        lineNo += i;

        if (sscanf(line, "%ld %32[^:]: ban %32s %16s %256[^\n]",
                   &date, name, site, temp, reason) != 5)
        {
            logmesg("%s:%d:Invalid ban record ``%s''\n",
                       BAN_LIST, lineNo, line);
            return;
        }

        bd = MakeBan(NULL, date);

        bd->immortal = strdup(name);
        bd->site = strdup(site);
        bd->reason = strdup(reason);
        bd->newbie = !str_prefix(temp, "newbie");

        count++;
    }

    logmesg("Banned %d total sites.", count);
}


/*** COMMAND INTERFACE *******************************************************/

void
do_ban (struct char_data *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    struct ban_data *bd;

    argument = one_argument(argument, arg);

    if ( !str_cmp(arg, "list") )
    {
        ListBans(ch, argument);
        return;
    }
    else if ( !str_cmp(arg, "save") )
    {
        FILE *fp = fopen(BAN_LIST, "w");

        if ( fp == NULL )
        {
            printf_to_char(ch, BAN_LIST ": %s\r\n", strerror(errno));
            return;
        }

        fprintf(fp, "#BANS\n");
        int count = 0;

        for ( bd = bans; bd; bd = bd->next )
        {
            count++;

            fprintf(fp, "%ld %s: ban %s %s %s\n",
                    (long) bd->date, bd->immortal,
                    bd->site, (bd->newbie ? "new" : "full"), bd->reason);
        }

        fprintf(fp, "$~\n\n#$\n");
        fclose(fp);

        printf_to_char(ch, "Saved %d ban records.\r\n", count);
        return;
    }
    else if ( !*arg )
    {
        goto usage;
    }

    struct ban_data *prev;
    char arg2[MAX_INPUT_LENGTH];
    bool newbie = FALSE;
    bool removing = FALSE;

    argument = one_argument(argument, arg2);

    while ( isspace(*argument) )
        argument++;

    if ( !str_prefix(arg2, "newbie") )
        newbie = TRUE;
    else if ( !str_prefix(arg2, "full") )
        newbie = FALSE;
    else if ( !str_prefix(arg2, "remove") )
        removing = TRUE;
    else
        goto usage;

    for ( prev = NULL, bd = bans; bd; prev = bd, bd = bd->next )
        if ( !str_cmp(bd->site, arg) && (removing || bd->newbie == newbie) )
            break;

    if ( removing )
    {
        if ( bd == NULL )
        {
            send_to_char("No such ban to remove.\r\n", ch);
            return;
        }
        else if ( prev == NULL )
        {
            /* First ban in the list. */
            bans = bans->next;
        }
        else
        {
            /* In body of the list. */
            prev->next = bd->next;
        }

        DestroyBan(bd, FALSE);
        send_to_char("Removed.\r\n", ch);
        return;
    }
    else if ( !*argument )
    {
        send_to_char("You must provide a reason for the ban.\r\n", ch);
        return;
    }

    if ( bd )
    {
        /* Updating an existing ban. */
        if ( bd->immortal && str_cmp(bd->immortal, ch->names) )
        {
            free(bd->immortal);
            bd->immortal = strdup(ch->names);
        }

        bd->date = time(0);

        if ( bd->reason && str_cmp(bd->reason, argument) )
        {
            free(bd->reason);
            bd->reason = strdup(argument);
        }

        send_to_char("Updated ban record.\r\n", ch);
        return;
    }

    /* Add a new ban. */
    bd = MakeBan(ch, time(0));
    bd->site = strdup(arg);
    bd->reason = strdup(argument);
    bd->newbie = newbie;

    printf_to_char(ch, "Added %s ban on site %s.\r\n",
                   (newbie ? "newbie" : "full"), bd->site);
    return;

  usage:
    send_to_char("&BUSAGE&X: &Wban save&X - &csave bans\r\n"
                 "     &X| &Wban list &X[&W-newbie&X|&W-full&X]"
                 " [&wpattern...&X] - &c list bans\r\n"
                 "     &X| &Wban &X<&wsite&X> <&Wnew&X|&Wfull&X>"
                 " <&wreason&X> - &cban a site\r\n"
                 "     &X| &Wban &X<&wsite&X> &Wremove&X"
                 " - &cunban a site\r\n", ch);
}


void
do_mortal_ban (struct char_data *ch, char *argument)
{
    /* Map a mortal's 'ban' command to 'ban list'. */
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "list %s", argument);
    do_ban(ch, buf);
}


void
do_unban (struct char_data *ch, char *argument)
{
    /* Map 'unban foo' to 'ban foo remove'. */
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);
    sprintf(buf, "%s remove", arg);
    do_ban(ch, buf);
}
