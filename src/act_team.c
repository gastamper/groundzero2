
/*
 * act_team.c -- Team commands, etc., in one logical place.
 * Copyright 2000, GZ2 Coding Team
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include "ground0.h"


DECLARE_DO_FUN (do_gecho);
DECLARE_DO_FUN (do_goto);
bool g_safeVolMode = FALSE;

void induct(CHAR_DATA *, int);
int bestTeam(void);
int worstTeam(void);
int lowestteam(void);


char *
describe_room (struct room_data *r)
{
    static char buf[MAX_STRING_LENGTH];

    if ( r == safe_area )
        strcpy(buf, "&YSafe Room&n");
    else if ( r->level == -1 )
        sprintf(buf, "%s&n", r->name);
    else
        sprintf(buf, "(&uX%2d&n, &uX%2d&n, &uX%d&n)", r->x, r->y, r->level);

    return buf;
}


void
do_teamwho (struct char_data *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    int team = ch->team;
    int count = 0;
    char *p;
    int k;

    /* Let admins twho any non-independent team in the game. */
    if ( get_trust(ch) && *argument )
    {
        char arg[MAX_INPUT_LENGTH];
        one_argument(argument, arg);

        if ( *arg )
        {
            team = lookup_team(arg);

            if ( team == -1 || team_table[team].independent )
            {
                printf_to_char(ch, "%s is not a team.\r\n", arg);
                return;
            }
        }
    }

    /*
     * Since the argument check above guards against an admin twho'ing an
     * independent team, this check is only relevant when no argument has
     * been provided (i.e., we're doing twho on our own team).
     */
    if ( team_table[team].independent )
    {
        send_to_char("You're not on a real team.\r\n", ch);
        return;
    }

    k = sprintf(buf, "&mTEAM %s\r\n"
                     "&cLeader&X: %s%s  &cScore&X: &Y%d\r\n",
                without_colors(team_table[team].who_name),
                (team_table[team].teamleader ?
                 team_table[team].namecolor : ""),
                (team_table[team].teamleader ?
                 team_table[team].teamleader->names :
                 "(no-one)"),
                team_table[team].score);

    if ( team_table[team].hq )
    {
        struct room_data *rm = team_table[team].hq->in_room;
        struct char_data *guard = get_team_guard(team);

        if ( rm == NULL )
            rm = store_area;

        k += sprintf(buf + k, "&cHQ&X: &n%s  "
                              "&cGuard Status&X: &uc%s\r\n",
                     describe_room(rm),
                     (guard ? diagnose_simple(guard) : "dead"));
    }

    send_to_char(buf, ch);
    k = sprintf(buf, "\r\n"
                "  &WNAME            LOCATION                       KILLS "
                "BETRAYALS\r\n"
                "  &X=============== ============================== ===== "
                "=========\r\n");

    for ( struct char_data *memb = char_list; memb; memb = memb->next )
        if ( memb->desc && !IS_NPC(memb) && memb->team == team )
        {
            p = describe_room(memb->in_room);
            k += sprintf(buf + k,
                         "  %s%-15s &n%*s &Y%-5d &Y%d\r\n",
                         team_table[team].namecolor, memb->names,
                         -(30 + ((int)strlen(p) - str_len(p))), p,
                         memb->pcdata->gs_teamed_kills, memb->teamkill);

            count++;
        }

    sprintf(buf + k, "\r\n&cTeam Members&X: &n%d\r\n", count);
    send_to_char(buf, ch);
}


void
do_forgive (struct char_data *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    struct char_data *vict = ch->last_team_kill;

    if ( !get_trust(ch) &&
         (team_table[ch->team].independent || !team_table[ch->team].active) )
    {
        send_to_char("You're not even on a real team.\r\n", ch);
        return;
    }
    else if ( team_table[ch->team].teamleader == ch || get_trust(ch) )
    {
        char arg[MAX_INPUT_LENGTH];

        one_argument(argument, arg);

        if ( *arg )
        {
            if ( (vict = get_char_world(ch, arg)) == NULL )
            {
                send_to_char("Who's that?\r\n", ch);
                return;
            }
            else if ( vict == ch )
            {
                send_to_char("Yeah, right.\r\n", ch);
                return;
            }
            else if ( vict->teamkill < 1 )
            {
                send_to_char("They haven't killed any teammates.\r\n", ch);
                return;
            }

            if ( !ch->trust && vict->team != ch->team )
            {
                send_to_char("Ehm?  They're not on your team.\r\n", ch);
                return;
            }

            sprintf(buf,
                    "%s&W(%s&W)&n forgives %s&W(%s&W)&n for a teamkill!",
                    ch->names, team_table[ch->team].who_name, vict->names,
                    team_table[vict->team].who_name);
            do_gecho(NULL, buf);
            vict->teamkill--;

            return;
        }
    }

    if ( !vict )
    {
        send_to_char("No teammate has killed you, lately.\r\n", ch);
        return;
    }
    else if ( vict->teamkill < 1 )
    {
        act("$E doesn't have any team kills.", ch, NULL,
            ch->last_team_kill, TO_CHAR);
        return;
    }

    sprintf(buf, "%s&W(%s&W)&n forgives %s&W(%s&W)&n for a teamkill!",
            ch->names, team_table[ch->team].who_name,
            vict->names, team_table[vict->team].who_name);
    do_gecho(NULL, buf);

    vict->teamkill--;
}


void
do_resig (CHAR_DATA * ch, char *argument)
{
    send_to_char("Spell out RESIGN if you want to resign.\r\n", ch);
}


void
do_resign (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *targ;

    if ( team_table[ch->team].teamleader != ch )
    {
        send_to_char("Yeah, right pal.\r\n", ch);
        return;
    }

    one_argument(argument, arg);

    if ( !*arg )
    {
        sprintf(buf, "%s%s&n has resigned as the leader of %s&n",
                team_table[ch->team].namecolor, ch->names,
                team_table[ch->team].who_name);
        do_gecho(NULL, buf);
        team_table[ch->team].teamleader = NULL;
        autoleader(ch->team, NULL, ch);
        return;
    }
    else if ( (targ = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Who's that?\r\n", ch);
        return;
    }
    else if ( targ->team != ch->team )
    {
        send_to_char
            ("You can't give leadership to someone not on your team.\r\n",
             ch);
        return;
    }
    else if ( IS_NPC(targ) )
    {
        send_to_char("Uhm, not a chance.\r\n", ch);
        return;
    }
    else if ( RANK(targ) < RANK_MERC )
    {
        send_to_char("They don't have the experience to lead.\r\n", ch);
        return;
    }
    else if ( IS_SET(targ->act, PLR_NOLEADER) )
    {
        send_to_char
            ("Relinquish control to that asshole?  Think again!\r\n", ch);
        return;
    }

    sprintf(buf, "%s%s&n has relinquished control of %s&n to %s%s&n.",
            team_table[ch->team].namecolor, ch->names,
            team_table[ch->team].who_name,
            team_table[targ->team].namecolor, targ->names);
    do_gecho(NULL, buf);

    team_table[ch->team].teamleader = targ;

    logmesg("%s relinquished control of team %s to %s.", ch->names,
               team_table[ch->team].name, targ->names);
}


void
do_boo (CHAR_DATA * ch, char *argument)
{
    send_to_char("Spell out BOOT to kick someone out of your team.\r\n",
                 ch);
}


void
boot (CHAR_DATA * victim)
{
    char buf[MAX_INPUT_LENGTH];
    int old_team = victim->team;

    sprintf(buf, "%s%s&n has betrayed %s&n for the last time!",
            team_table[victim->team].namecolor, victim->names,
            team_table[victim->team].who_name);
    do_gecho(NULL, buf);
    logmesg("%s", buf);

    if ( team_table[victim->team].teamleader == victim )
    {
        team_table[victim->team].teamleader = NULL;
        autoleader(victim->team, NULL, victim);
    }

    SET_BIT(victim->act, PLR_TRAITOR);

    victim->max_hit -= 2000;
    victim->hit = UMIN(victim->hit, victim->max_hit);

    team_table[victim->team].players--;
    victim->team = TEAM_TRAITOR;
    team_table[victim->team].players++;

    if ( !IS_NPC(victim) )
        victim->pcdata->time_traitored = time(0);

    if ((team_table[old_team].hq &&
         victim->in_room->interior_of == team_table[old_team].hq) ||
        (victim->in_room->x ==
         (the_city->x_size - 1) * team_table[old_team].x_mult &&
         victim->in_room->y ==
         (the_city->y_size - 1) * team_table[old_team].y_mult))
    {
        act("A black helicopter swoops down and takes $n away.", victim,
            NULL, NULL, TO_ROOM);
        send_to_char
            ("A large soldier comes up and grabs you.  'Get out of here, traitor,\r\n"
             "before I decide to do something more drastic.'\r\n", victim);
        do_goto(victim, "random 0");
    }
}


void
do_boot (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);
    while ( isspace(*argument) )
        argument++;

    if ( !*arg || !*argument )
    {
        send_to_char("Boot who, for what reason?\r\n", ch);
        return;
    }
    else if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Boot who?\r\n", ch);
        return;
    }
    else if ( IS_NPC(victim) )
    {
        send_to_char("No.\r\n", ch);
        return;
    }
    else if ( ch->desc == NULL )
    {
        send_to_char("You can't boot linkdead players.\r\n", ch);
        return;
    }
    else if ( ch == victim )
    {
        send_to_char("Yourself?!\r\n", ch);
        return;
    }
    else if ( !get_trust(ch) )
    {
        if ( team_table[ch->team].teamleader != ch )
        {
            send_to_char
                ("You don't have the authority to boot that person.\r\n",
                 ch);
            return;
        }
        else if ( ch->team != victim->team )
        {
            send_to_char
                ("How do you propose to boot someone not on your team?!\r\n",
                 ch);
            return;
        }
        else if ( buttonpresser )
        {
            send_to_char("While the button's on?  Yeah, right.\r\n", ch);
            return;
        }
        else if ( victim->in_room == safe_area )
        {
            send_to_char
                ("You can't boot someone who's in the safe room.\r\n", ch);
            return;
        }
        else if ( g_safeVolMode && victim->pcdata->volunteering != -1 )
        {
            send_to_char("Volunteering is safe right now, so... no.\r\n",
                         ch);
            return;
        }
    }

    boot(victim);
    sprintf(buf, "%s booted %s for: %s", ch->names, victim->names,
            argument);
    logmesg("%s", buf);
    wiznet(buf, ch, NULL, WIZ_SECURE, 0, get_trust(ch));
}


void
do_untraitor (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);

    if ( (victim = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("Untraitor who?\r\n", ch);
        return;
    }
    sprintf(buf, "%s%s&n has been declassified as a traitor.\r\n",
            team_table[victim->team].namecolor, victim->names);
    do_gecho(NULL, buf);
    victim->teamkill = 0;
    REMOVE_BIT(victim->act, PLR_TRAITOR);
    if ( !IS_NPC(victim) )
        victim->max_hit = victim->pcdata->solo_hit;
}


void
do_showvol (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    struct descriptor_data *id;
    int team = ch->team;
    int i = 0;

    if ( get_trust(ch) )
    {
        char arg[MAX_INPUT_LENGTH];

        one_argument(argument, arg);

        if ( *arg && (team = lookup_team(arg)) == -1 )
        {
            send_to_char("What team is that?\r\n", ch);
            return;
        }
    }
    else if ( team_table[team].teamleader != ch )
    {
        send_to_char("You're not even leader.\r\n", ch);
        return;
    }

    i = sprintf(buf,
                "The following people are volunteering for team %s&n:\r\n",
                team_table[team].who_name);

    for ( id = descriptor_list; id; id = id->next )
        if (id->connected == CON_PLAYING && id->character &&
            !IS_NPC(id->character) &&
            id->character->pcdata->volunteering == team &&
            can_see(id->character, ch))
        {
            i += sprintf(buf + i, "%s&W(%s&W)&n\r\n",
                         id->character->names,
                         team_table[id->character->team].who_name);
        }

    send_to_char(buf, ch);
}


void
do_volunteer (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int i;

    if ( IS_SET(ch->act, PLR_TRAITOR) )
    {
        send_to_char("Not while you're a traitor, you don't.\r\n", ch);
        return;
    }
    else if ( ch->in_room == safe_area )
    {
        send_to_char("Not while you're hiding in safe...\r\n", ch);
        return;
    }
    else if ( IS_NPC(ch) )
    {
        send_to_char("Not as a mob you don't.\r\n", ch);
        return;
    }
    else if ( team_table[ch->team].teamleader == ch )
    {
        send_to_char
            ("You can't &Yvolunteer&n while you're the team leader.\r\n",
             ch);
        return;
    }
    else if ( buttonpresser && buttonpresser != enforcer )
    {
        send_to_char("Volunteering is off while the button is on.\r\n",
                     ch);
        return;
    }
    else if (!team_table[ch->team].independent &&
             team_table[ch->team].players < 2)
    {
        send_to_char("But you're the only person on your team!\r\n", ch);
        return;
    }

    one_argument(argument, arg);

    if ( !*arg )
    {
        if ( team_table[(int) ch->pcdata->volunteering].independent )
        {
            send_to_char
                ("You're not volunteering your services to any team.\r\n",
                 ch);
            return;
        }
        sprintf(arg,
                "You're volunteering your services to %s&n.\r\nType &Yvol off&n to cancel.\r\n",
                team_table[(int) ch->pcdata->volunteering].who_name);
        send_to_char(arg, ch);
        return;
    }
    else if ( !str_cmp(arg, "off") )
    {
        send_to_char("Volunteer status cancelled.\r\n", ch);
        ch->pcdata->volunteering = 0;
        return;
    }

    if ( (i = lookup_team(arg)) == -1 )
    {
        send_to_char("Now try volunteering for a team that exists!\r\n",
                     ch);
        return;
    }
    else if ( i == ch->team )
    {
        send_to_char("You're on that team, idiot.\r\n", ch);
        return;
    }

    /*
     * Independents can only volunteer to the lowestteam().  In other words, they
     * can't use SOLO to pick which team to play for.  It's no different than
     * just logging back in.
     *
     * People on a team can only volunteer to a team which has fewer players or,
     * if equal players a lower score.
     */
    if ( !get_trust(ch) )
    {
        if ( team_table[ch->team].independent )
        {
            if ( lowestteam() != i )
            {
                send_to_char
                    ("It doesn't appear they need your services.\r\n", ch);
                return;
            }
        }
        else if (team_table[ch->team].players < team_table[i].players ||
                 (team_table[ch->team].players == team_table[i].players &&
                  team_table[ch->team].score < team_table[i].score))
        {
            send_to_char("Your current team needs you more, ya' wuss.\r\n",
                         ch);
            return;
        }
    }

    if ( !team_table[i].teamleader )
    {
        if ( team_table[i].independent || !team_table[i].active )
        {
            send_to_char("That team's not active.\r\n", ch);
            return;
        }

        induct(ch, i);
        autoleader(i, ch, NULL);
        return;
    }

    ch->pcdata->volunteering = i;
    sprintf(arg, "%s%s&n volunteers his services to %s&n",
            team_table[ch->team].namecolor, ch->names,
            team_table[i].who_name);
    do_gecho(NULL, arg);
    act("Use '&cinduct $N&n' to induct $M into the team.",
        team_table[i].teamleader, NULL, ch, TO_CHAR);
}


void
induct (CHAR_DATA * targ, int team)
{
    char buf[MAX_STRING_LENGTH];
    int old_team = targ->team;

    targ->pcdata->gs_teamed_kills = 0;
    targ->pcdata->volunteering = -1;
    targ->team = team;
    team_table[targ->team].players++;
    team_table[old_team].players--;
    targ->teamkill = 0;

    sprintf(buf, "%s%s&n has switched to the %s&n team!",
            team_table[targ->team].namecolor, targ->names,
            team_table[targ->team].who_name);
    do_gecho(NULL, buf);

    if ((team_table[old_team].hq &&
         targ->in_room->interior_of == team_table[old_team].hq) ||
        (targ->in_room->x ==
         (the_city->x_size - 1) * team_table[old_team].x_mult &&
         targ->in_room->y ==
         (the_city->y_size - 1) * team_table[old_team].y_mult))
    {
        act("A black helicopter swoops down and takes $n away.", targ,
            NULL, NULL, TO_ROOM);
        send_to_char("You are auto-teleported.\r\n", targ);
        do_goto(targ, "random");
        complete_movement(targ, DIR_UP);
    }
}


void
do_induct (CHAR_DATA * ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *targ;

    if ( team_table[ch->team].teamleader != ch )
    {
        send_to_char("You ain't the leader, bub.\r\n", ch);
        return;
    }
    else if ( buttonpresser && buttonpresser != enforcer )
    {
        send_to_char("Not while the button is on.\r\n", ch);
        return;
    }

    one_argument(argument, arg);

    if ( !*arg )
    {
        send_to_char("Induct who?\r\n", ch);
        return;
    }
    else if ( (targ = get_char_world(ch, arg)) == NULL )
    {
        send_to_char("No such person.\r\n", ch);
        return;
    }
    else if ( IS_NPC(targ) )
    {
        send_to_char("Fuck off.\r\n", ch);
        sprintf(arg, "%s attempted to crash the mud by inducting mobile.",
                ch->names);
        logmesg(arg, ch->names);
        wiznet(arg, ch, NULL, WIZ_SECURE, 0, get_trust(ch));
        return;
    }
    else if ( targ->in_room == safe_area )
    {
        send_to_char("They can't be hiding in safe.\r\n", ch);
        return;
    }
    else if ( targ->pcdata->volunteering != ch->team )
    {
        send_to_char
            ("It'd help if they were volunteering for your team, first.\r\n",
             ch);
        return;
    }
    else if ( IS_SET(targ->act, PLR_TRAITOR) )
    {
        send_to_char
            ("You can't induct an active traitor (see &Yhelp traitor&n).\r\n",
             ch);
        return;
    }

    induct(targ, ch->team);
}


int
lookup_team (char *arg)
{
    int i;
    extern int g_numTeams;

    for ( i = 0; i < g_numTeams; i++ )
        if ( !str_prefix(arg, team_table[i].name) )
            break;

    if ( i == g_numTeams )
        return -1;
    return i;
}


static inline bool
fit_to_autolead (CHAR_DATA * ch, CHAR_DATA * best_leader)
{
    if ( RANK(ch) < RANK_MERC )
        return FALSE;
    if ( IS_SET(ch->act, PLR_NOLEADER) )
        return FALSE;

    /* Transition towards using REP for more things. */
    if ( !best_leader || REP(ch) > REP(best_leader) )
        return TRUE;

    return FALSE;
}


void
autoleader (int team, CHAR_DATA * just_check, CHAR_DATA * exclude)
{
    DESCRIPTOR_DATA *d;
    CHAR_DATA *best_leader = NULL;
    char buf[MAX_STRING_LENGTH];

    if ( team < 0 || team >= g_numTeams || team_table[team].independent )
        return;

    if ( !just_check )
    {
        for ( d = descriptor_list; d; d = d->next )
        {
            if ( d->connected != CON_PLAYING || !d->character )
                continue;
            if ( d->character == exclude || d->character->team != team )
                continue;
            if ( fit_to_autolead(d->character, best_leader) )
                best_leader = d->character;
        }
    }
    else if ( fit_to_autolead(just_check, team_table[team].teamleader) )
    {
        best_leader = just_check;
    }

    /* Bumblefuck. */
    if ( !best_leader || best_leader == team_table[team].teamleader )
        return;

    team_table[team].teamleader = best_leader;

    sprintf(buf, "The %s&n General has handpicked %s%s&n to lead&n",
            team_table[team].who_name, team_table[team].namecolor,
            best_leader->names);
    do_gecho(NULL, buf);
}


int
frobPlayerCount (int tm, int tmx)
{
    int frob;

    frob = (team_table[tm].score - team_table[tmx].score) / 10;
    frob += team_table[tm].players;
    return (frob);
}


int
lowestteam (void)
{
    int lowest = -1;
    int i;

    for ( i = 0; i < g_numTeams; i++ )
    {
        if ( !team_table[i].active )
            continue;
        else if ( lowest == -1 )
        {
            lowest = i;
            continue;
        }

        if ( team_table[lowest].players == team_table[i].players )
        {
            if ( team_table[i].score < team_table[lowest].score )
                lowest = i;
        }
        else if ( team_table[i].players < team_table[lowest].players )
            lowest = i;
    }

    return lowest;
}


int
worstTeam (void)
{
    int worst = -1;
    int i;

    for ( i = 0; i < g_numTeams; i++ )
    {
        if ( !team_table[i].active || team_table[i].independent )
            continue;
        else if ( worst == -1 )
        {
            worst = i;
            continue;
        }
        else if ( team_table[i].score > team_table[worst].score )
            continue;

        if ( frobPlayerCount(worst, i) >= team_table[i].players )
            worst = i;
    }

    return worst;
}


int
bestTeam (void)
{
    int best = -1;
    int i;

    for ( i = 0; i < g_numTeams; i++ )
    {
        if ( !team_table[i].active || team_table[i].independent )
            continue;
        else if ( best == -1 )
        {
            best = i;
            continue;
        }
        else if ( team_table[i].score < team_table[best].score )
            continue;

        if ( frobPlayerCount(i, best) >= team_table[best].players )
            best = i;
    }

    return best;
}


void
SetSquareFeature (int team, int x, int y, char feat)
{
    if ( !team_table[team].map_memory )
        return;
    int index = (y * the_city->x_size) + x;

    team_table[team].map_memory[index] |= feat;
}


void
UnsetSquareFeature (int team, int x, int y, char feat)
{
    if ( !team_table[team].map_memory )
        return;
    int index = (y * the_city->x_size) + x;

    team_table[team].map_memory[index] &= ~feat;
}


bool
HasSquareFeature (int team, int x, int y, char feat)
{
    if ( !team_table[team].map_memory )
        return FALSE;
    int index = (y * the_city->x_size) + x;

    return (team_table[team].map_memory[index] & feat);
}


struct char_data *
get_team_guard (int team)
{
    struct char_data *ch;
    char buf[MAX_STRING_LENGTH];

    struct room_data *rm = index_room(the_city->rooms_on_level,
                                      (the_city->x_size - 1) *
                                      team_table[team].x_mult,
                                      (the_city->y_size - 1) *
                                      team_table[team].y_mult);

    sprintf(buf, "%s %s", team_table[team].name, guardian->names);

    for ( ch = rm->people; ch; ch = ch->next_in_room )
         if ( ch->names && IS_NPC(ch) &&
              ch->ld_behavior == BEHAVIOR_GUARD &&
              !str_cmp(ch->names, buf) )
             break;

    return ch;
}
