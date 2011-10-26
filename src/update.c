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
DECLARE_DO_FUN (do_quit);

/*
 * Local functions.
 */
int hit_gain(CHAR_DATA * ch);
int mana_gain(CHAR_DATA * ch);
int move_gain(CHAR_DATA * ch);
void mobile_update(void);
void weather_update(void);
void char_update(void);
void obj_update(void);
void aggr_update(void);
void base_purge(void);
void core_meltdown(int);

DECLARE_DO_FUN (do_gamestats);

void teamed_mob(CHAR_DATA *, int, char *, int, int);
char *npc_parse(const char *, struct char_data *,
                struct char_data *, struct obj_data *, const char *);

/* used for saving */

int save_number = 0;
int tick_counter = 0;
extern DESCRIPTOR_DATA *pinger_list;

extern int enforcer_depress;


const char *mech_charge_messages[MAX_CHARGE_MECH + 3][2] = {
    {"You hear the mech's power generators fire up and begin working.",
     "The computer announces, \"Charging weapons systems.\""},
    {"The large blaster on the mech begins to crackle with energy...",
     "The computer announces, \"Ichi...\""},
    {"Lightning arcs across the massive blaster arm of the mech.",
     "Your seat begins to vibrate.  The computer announces, \"Ni...\""},
    {"The air in front of the mech's blaster congeals into a ball of plasma.",
     "You can just barely make out the computer announcing, \"San,\" over the roar of the engines."},
    {"The air dances with wisps of electricity as the mech's blaster arm begins to glow a bright red.",
     "A light on the console goes green."},
    {"A deafening grating sound emerges from the mech's engines.",
     "A warning light begins to blink like mad on the console."},
    {"The mech begins to shake uncontrollably.  Something's not right.",
     "You become aware of the computer repeating, \"Warning, engine overload imminent. . .\""},
    {"It's suddenly very quiet.  Deathly quiet...",
     "Ooops, looks like you've overloaded your mech."}
};


/* Regeneration stuff. */
int
hit_gain (CHAR_DATA * ch)
{
    int gain;

    gain = number_range(125, 325);
    if ( ch->in_room == safe_area )
        gain += 200;

    switch (ch->position)
    {
        case POS_RESTING:
            gain *= 1.5;
            break;
        case POS_SLEEPING:
            gain *= 2;
            break;
    }

    return UMIN(gain, ch->max_hit - ch->hit);
}


/* Calculate how much the button is worth. */
int
calculate_worth (void)
{
    int scores = 0;
    int count = 0;
    int t;

    for ( t = 0; t < g_numTeams; t++ )
        if ( team_table[t].active && !team_table[t].independent )
        {
            scores += team_table[t].score;
            count++;
        }

    if ( count == 0 )
        return 0;

    return (int) (ceil(scores / (count * 10.)) * 10);
}


void
char_update (void)
{
    CHAR_DATA *ch;
    CHAR_DATA *ch_next;
    char buf[MAX_INPUT_LENGTH];

    /* update save counter */
    save_number++;

    if ( save_number > 29 )
        save_number = 0;

    for ( ch = char_list; ch != NULL; ch = ch_next )
    {
        ch_next = ch->next;
        ch->idle++;

        if ( !IS_NPC(ch) && ch->desc && ch->in_room == graveyard_area )
            do_teleport(ch, "");

        if ( !IS_NPC(ch) && !(ch->idle % 5) )
            ch->pcdata->gs_idle++;

        if ( ch->ld_timer == 1 && ch->in_room != graveyard_area )
        {
            ch->last_hit_by = NULL;
            char_death(ch, "idle death");
        }

        if ( ch->ld_timer )
            ch->ld_timer--;

        if ( IS_SET(ch->affected_by, AFF_BLIND) )
        {
            int n;

            if (team_table[ch->team].hq &&
                ch->in_room == team_table[ch->team].hq->interior)
                n = number_range(0, 2);
            else
                n = number_range(0, 5);

            if ( !n )
            {
                REMOVE_BIT(ch->affected_by, AFF_BLIND);
                send_to_char("You can see again!\r\n", ch);
            }
        }

        if ( IS_SET(ch->affected_by, AFF_DAZE) && !number_range(0, 2) )
        {
            REMOVE_BIT(ch->affected_by, AFF_DAZE);
            send_to_char("You are no longer dazed!\n\r", ch);
        }

        if ( ch->position >= POS_STUNNED )
        {
            if ( ch->hit < ch->max_hit )
                ch->hit += hit_gain(ch);
            else
            {
                ch->hit = ch->max_hit;

                if ( ch->idle >= 112 && ch->desc && !ch->trust )
                    close_socket(ch->desc);

                if ( (ch->idle % 36) == 0 )
                {
                    if ( team_table[ch->team].teamleader == ch )
                        autoleader(ch->team, NULL, ch);

                    if (ch->in_room == safe_area && !ch->trust &&
                        !team_table[ch->team].independent)
                    {
                        send_to_char
                            ("A C-47 transport plane lands nearby and a soldier"
                             "runs up to you screaming\r\n"
                             "'What the hell are you still doing here?!?  You"
                             "are hustled aboard the plane\r\n"
                             "and the cargo doors close behind you...\r\n",
                             ch);

                        sprintf(buf, "IDLE: %s was kicked out of safe.",
                                ch->names);
                        logmesg(buf);
                        wiznet(buf, NULL, NULL, WIZ_SECURE, 0,
                               get_trust(ch));
                        do_teleport(ch, "");
                    }
                }
            }
        }

        if ( ch->pcdata && ch->pcdata->pageDelay )
            ch->pcdata->pageDelay--;

        if ( ch->position == POS_STUNNED )
            update_pos(ch);

        if ( !IS_NPC(ch) && !ch->trust )
            if ( IS_IMMORTAL(ch) )
                ch->timer = 0;

        ch->donate_num = UMAX(0, ch->donate_num - 2);
    }

    for ( ch = char_list; ch != NULL; ch = ch_next )
    {
        ch_next = ch->next;
        if ( ch->desc != NULL && ch->desc->descriptor % 30 == save_number )
            save_char_obj(ch);
    }
}

/*
 * Update all objs.
 * This function is performance sensitive.
 */
void
obj_update (void)
{
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    CHAR_DATA *all_chars;
    bool supplyTicked = FALSE;

    for ( obj = object_list; obj != NULL; obj = obj_next )
    {
        obj_next = obj->next;

        if ( obj->arrival_time >= 0 && --obj->arrival_time == -1 )
        {
            ROOM_DATA *temp_room;
            ROOM_DATA *was_in;

            if ( !obj->in_room )
                logmesg("obj_update: object not in room");

            was_in = obj->in_room;
            temp_room = obj->destination;

            obj_from_room(obj);
            obj->destination = NULL;
            obj_to_room(obj, temp_room);

            if (was_in == deploy_area &&
                IS_SET(obj->general_flags, GEN_DEPLOYMENT))
            {
                const char *supply_m, *land_m;

                switch (obj->in_room->level)
                {
                    case 0:
                        supply_m =
                            "A supply plane flies high overhead, deploying cargo.\r\n";
                        land_m =
                            "$p floats down on a parachute and lands, here.";
                        break;
                    default:
                        /* Lame messages if an item is deployed somewhere on L1/L2/etc. */
                        supply_m =
                            "You see a black cat run across a doorway in the same direction, twice.  \"Whoa, deja vu.\"\r\n";
                        land_m =
                            "$p appears from nowhere.  Someone's altered the matrix.";
                        break;
                }

                if ( !supplyTicked )
                {
                    for (all_chars = char_list; all_chars;
                         all_chars = all_chars->next)
                        if ( !IS_NPC(all_chars ) &&
                            all_chars->in_room->level ==
                            obj->in_room->level)
                            send_to_char(supply_m, all_chars);

                    supplyTicked = TRUE;
                }

                if ( obj->in_room->people )
                {
                    act(land_m, obj->in_room->people, obj, NULL, TO_ROOM);
                    act(land_m, obj->in_room->people, obj, NULL, TO_CHAR);

                    for (all_chars = obj->in_room->people; all_chars;
                         all_chars = all_chars->next_in_room)
                        if ( all_chars->pcdata )
                        {
                            all_chars->pcdata->it_is_chardata = 0;
                            all_chars->pcdata->it.od = obj;
                        }
                }

                continue;
            }

            if ( obj->in_room->people )
            {
                if ( !IS_SET(obj->wear_flags, ITEM_TAKE ) &&
                    IS_SET(obj->general_flags, GEN_CAN_PUSH))
                {
                    if ( !number_range(0, 9) )
                    {
                        act("$p slams into the wall hard and explodes!",
                            obj->in_room->people, obj, NULL, TO_ROOM);
                        act("$p slams into the wall hard and explodes!",
                            obj->in_room->people, obj, NULL, TO_CHAR);
                        bang_obj(obj, obj->extract_me);
                        continue;
                    }
                    else
                    {
                        act("$p rolls to a stop.", obj->in_room->people,
                            obj, NULL, TO_ROOM);
                        act("$p rolls to a stop.", obj->in_room->people,
                            obj, NULL, TO_CHAR);
                    }
                }
                else
                {
                    act("$p lands at your feet.", obj->in_room->people,
                        obj, NULL, TO_ROOM);
                    act("$p lands at your feet.", obj->in_room->people,
                        obj, NULL, TO_CHAR);
                }
            }

            if ( IS_SET(obj->general_flags, GEN_EXTRACT_ON_IMPACT) )
            {
                bang_obj(obj, obj->extract_me);
                continue;
            }
            else if ( (obj->timer <= 0 || obj->timer > 2 ) &&
                     obj->item_type == ITEM_EXPLOSIVE &&
                     IS_SET(obj->wear_flags, ITEM_TAKE))
            {
                for (all_chars = obj->in_room->people; all_chars;
                     all_chars = all_chars->next_in_room)
                {
                    if ( IS_NPC(all_chars ) &&
                        all_chars->ld_behavior == BEHAVIOR_TOSSBACK)
                    {
                        get_obj(all_chars, obj, NULL);
                        send_object(obj, obj->startpoint, 1);
                        act("You &uathrow&n $p.", all_chars, obj, NULL,
                            TO_CHAR);
                        act("$n &uathrows&n $p.", all_chars, obj, NULL,
                            TO_ROOM);
                        break;
                    }
                    else if ( all_chars->pcdata )
                    {
                        all_chars->pcdata->it_is_chardata = 0;
                        all_chars->pcdata->it.od = obj;

                        if ( obj->owner && obj->owner != all_chars )
                        {
                            switch (obj->owner->sex)
                            {
                                case SEX_MALE:
                                    all_chars->pcdata->him = obj->owner;
                                    break;
                                case SEX_FEMALE:
                                    all_chars->pcdata->her = obj->owner;
                                    break;
                            }
                        }
                    }
                }
            }
        }

        /* for clarity, this means that we either have a timer of -1 or 0
           (a stopped timer) from the first half of the clause, or we have a
           timer that is positive int after subtracting 1 from it, the else
           clause means the timer must have just expired */
        if ( obj->timer <= 0 || --obj->timer > 0 )
        {
            if ( obj->carried_by && obj->timer > 0 )
            {
                if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) )
                    act("$p burns quietly in your hand.", obj->carried_by,
                        obj, NULL, TO_CHAR);
                else
                    act("$p ticks very quietly.", obj->carried_by, obj,
                        NULL, TO_CHAR);
            }

            if ( IS_SET(obj->general_flags, GEN_BURNS_ROOM) )
                if (obj->in_room &&
                    (obj->in_room->people || obj->in_room->contents))
                    burn_obj(obj);

            if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) )
                if ( obj->in_room && obj->in_room->people )
                    choke_obj(obj, obj->in_room, 0);

            continue;
        }
        else
        {
            if ( obj->arrival_time >= 0 )
            {
                extract_obj(obj, 0);
            }
            else if ( obj->extract_flags )
            {
                bang_obj(obj, 0);
            }
            else if ( IS_SET(obj->general_flags, GEN_BURNS_ROOM) )
            {
                if ( obj->in_room->people )
                {
                    act("$p burns down and dies leaving nothing but "
                        "ashes.", obj->in_room->people, obj, NULL,
                        TO_CHAR);
                    act("$p burns down and dies leaving nothing but "
                        "ashes.", obj->in_room->people, obj, NULL,
                        TO_ROOM);
                }

                extract_obj(obj, 1);
            }
            else if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) )
            {
                if ( obj->in_room->people )
                {
                    act("$p dissipates into the air.",
                        obj->in_room->people, obj, NULL, TO_CHAR);
                    act("$p dissipates into the air.",
                        obj->in_room->people, obj, NULL, TO_ROOM);
                }

                extract_obj(obj, 1);
            }
            else if ( IS_SET(obj->general_flags, GEN_DARKS_ROOM) )
            {
                if ( obj->in_room->people )
                {
                    act("This room is no longer dark.",
                        obj->in_room->people, obj, NULL, TO_CHAR);
                    act("This room is no longer dark.",
                        obj->in_room->people, obj, NULL, TO_ROOM);
                }
                extract_obj(obj, 1);
            }
        }
    }
}


void
tick_stuff (int imm_generated)
{
    static int pulse_point = PULSE_TICK;
    CHAR_DATA *all_chars;

    if ( (--pulse_point > 0) && !imm_generated )
        return;

    tick_counter++;

    if ( !teleport_time )
    {
        if ( !number_range(0, 99) )
        {
            for (all_chars = char_list; all_chars;
                 all_chars = all_chars->next)
            {
                send_to_char("A scratchy voice comes in over your comm "
                             "badge '&utRedeployment systems are\n\r"
                             "operational for the moment, but may go "
                             "offline at any second.  Should you\n\rneed "
                             "redeployment in the near future, please do "
                             "so now.&n'\n\r", all_chars);
            }

            teleport_time = 1;
        }
    }
    else
    {
        if ( !number_range(0, 11) )
        {
            for (all_chars = char_list; all_chars;
                 all_chars = all_chars->next)
                send_to_char("A scratchy voice comes in over your comm "
                             "badge '&utTransportation systems have\n\rjust "
                             "gone off line.  Redeployment services are "
                             "temporarily unavailable.&n'\n\r", all_chars);

            teleport_time = 0;
        }
    }
    pulse_point = PULSE_TICK;
    char_update();
}


void
ping_update (void)
{
    DESCRIPTOR_DATA *d, *d_next, *d_prev;
    char buf[MAX_STRING_LENGTH];

    for ( d_prev = NULL, d = pinger_list; d; d_prev = d, d = d_next )
    {
        d_next = d->ping_next;

        if ( --d->ping_wait > 0 )
            continue;

        while ( !feof(d->ping_pipe) )
        {
            if ( fgets(buf, 128, d->ping_pipe) != NULL )
                write_to_buffer(d, buf, strlen(buf));
        }

        if ( d == pinger_list )
            pinger_list = d->ping_next;
        else
            d_prev->ping_next = d->ping_next;

        pclose(d->ping_pipe);
        d->ping_pipe = NULL;
        d->ping_next = NULL;
    }
}


/*
 * Handle all kinds of updates.
 * Called once per pulse from game loop.
 * Random times to defeat tick-timing clients and players.
 */
void
update_handler ()
{
    static int pulse_violence = PULSE_VIOLENCE;
    static int pulse_objects = PULSE_OBJECTS;
    static int pulse_purge = PULSE_PURGE;
    static int heartbeat = 0;

    CHAR_DATA *true_extract = NULL;
    CHAR_DATA *ch;
    CHAR_DATA *next_ch;
    CHAR_DATA *prev_ch;
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;
    int count, winscore;

    extern bool time_protection;
    extern int round_time_left;
    extern unsigned long g_bytes_recv;
    extern unsigned long g_bytes_sent;
    extern char *help_gameover;
    void clear_screen(struct char_data *);

    heartbeat++;
    tick_stuff(0);

    if ( !(heartbeat % (5 RLMIN)) )
    {
        for ( count = 0, d = descriptor_list; d; d = d->next, count++ )
            ;

        if ( g_bytes_sent || g_bytes_recv )
        {
            logmesg("RUSAGE: %d sent, %d received, %d connections",
                       g_bytes_sent, g_bytes_recv, count);
            g_bytes_sent = 0;
            g_bytes_recv = 0;
        }
    }

    if ( buttontimer == -1 && !g_safeVolMode && !(heartbeat % (10 RLMIN)) )
    {
        int best;
        int worst;

        best = bestTeam();
        worst = worstTeam();

        if ( best != g_numTeams && worst != g_numTeams )
        {
            int dscore = team_table[best].score - team_table[worst].score;
            int dplayers =
                team_table[best].players - team_table[worst].players;

            if ( dscore > 20 && dplayers > 0 )
            {
                g_safeVolMode = TRUE;

                sprintf(buf, "Safe volunteering mode enabled!  "
                        "Type \"&cVOLUNTEER %s&W\" to volunteer.",
                        team_table[worst].name);
                do_commbadge(enforcer, buf);
            }
        }
    }
    else if ( g_safeVolMode && !(heartbeat % (5 RLMIN)) )
    {
        int best;
        int worst;

        best = bestTeam();
        worst = worstTeam();

        if (best == g_numTeams || worst == g_numTeams ||
            (team_table[best].score - team_table[worst].score) < 20 ||
            (team_table[best].players - team_table[worst].players) < 1)
        {
            do_commbadge(enforcer, "Safe volunteering mode disabled!");
            g_safeVolMode = FALSE;
        }
    }

    count = 0;
    winscore = 0;

    if ( round_time_left == -1 )
        round_time_left = enforcer_depress;

    if ( --pulse_violence <= 0 )
    {
        pulse_violence = PULSE_VIOLENCE;
        violence_update();
        ping_update();
    }

    if ( --pulse_objects <= 0 )
    {
        pulse_objects = PULSE_OBJECTS;
        obj_update();
    }

    if ( --pulse_purge <= 0 )
    {
        pulse_purge = PULSE_PURGE;
        base_purge();
    }

    if ( buttontimer == -1 && time_protection )
        if ( !(heartbeat % PULSE_ENFORCER) )
            if ( !--round_time_left )
            {
                do_depress(enforcer, "");

                for (struct char_data * ech = char_list; ech;
                     ech = ech->next)
                {
                    if ( ech->npc && ech->npc->enforcer.head )
                    {
                        struct cmd_node *node = ech->npc->enforcer.head;
                        char *hold;

                        while ( node != NULL )
                        {
                            hold = npc_parse(node->cmd, ech, NULL, NULL, NULL);
                            if ( !hold )
                                continue;
                            interpret(ech, hold);
                            node = node->next;
                        }
                    }
                }
            }

    if ( !(heartbeat % (25 RLMIN)) )
        save_polls();

    if ( !(heartbeat % PULSE_BUTTON) && buttontimer != -1 )
    {
        if ((buttontimer <= 300 && buttontimer >= 180 &&
             !(buttontimer % 60)) || (buttontimer < 180 && buttontimer > 10
                                      && !(buttontimer % 30)) ||
            buttontimer == 10 || buttontimer <= 5)
        {
            sprintf(buf, "%d second%s until core meltdown!\r\n",
                    buttontimer, (buttontimer == 1 ? "" : "s"));
            do_gecho(NULL, buf);
        }

        if ( !--buttontimer )
        {
            if ( !buttonpresser )
            {
                buttontimer = -1;
                return;
            }

            if ( team_table[buttonpresser->team].independent )
            {
                for ( d = descriptor_list; d; d = d->next )
                {
                    if ( d->connected == CON_PLAYING && d->character )
                    {
                        d->character->last_hit_by = buttonpresser;
                        kill_message(buttonpresser, d->character,
                                     "the core meltdown");
                        adjust_kp(buttonpresser, d->character);
                        check_top_deaths(d->character);
                        save_char_obj(d->character);
                        winscore++;

                        /* Give them HP for non-mercs. */
                        if ( !IS_NPC(buttonpresser ) &&
                            buttonpresser->team == TEAM_NONE &&
                            RANK(d->character) < RANK_MERC &&
                            !IS_SET(buttonpresser->act, PLR_FREEZE_STATS))
                            buttonpresser->pcdata->solo_hit++;

                        clear_screen(d->character);
                    }
                }

                sprintf(buf,
                        "%s got %d kills for pressing the button!\r\n",
                        buttonpresser->names, winscore);
                do_gecho(NULL, buf);
            }
            else
                core_meltdown(buttonpresser->team);

            do_gecho(NULL, help_gameover);
            do_gamestats(NULL, "");
            do_reboot(buttonpresser, "-q");
        }
    }

    /* get rid of dead folk */
    for ( prev_ch = NULL, ch = extract_list; ch; ch = next_ch )
    {
        next_ch = ch->next_extract;

        if ( !IS_NPC(ch ) && !team_table[ch->team].independent &&
            ch->in_room != graveyard_area)
        {
            team_table[ch->team].players--;

            if ( team_table[ch->team].teamleader == ch )
            {
                team_table[ch->team].teamleader = NULL;
                autoleader(ch->team, NULL, ch);
            }
        }

        if ( ch->owned_objs )
        {
            if ( ch->in_room != graveyard_area )
            {
                char_from_room(ch);
                char_to_room(ch, graveyard_area);

                logmesg("Sending %s to the graveyard.", ch->names);
            }

            prev_ch = ch;
            continue;
        }

        ch->next_extract = true_extract;
        true_extract = ch;

        /* Remove from extract_list */
        if ( prev_ch )
            prev_ch->next_extract = next_ch;
        else
            extract_list = next_ch;
    }

    while ( (ch = true_extract) != NULL )
    {
        true_extract = ch->next_extract;
        extract_char(ch, TRUE);
    }

    /* Just to wrap it around every once in a while. */
    if ( heartbeat == 1000000000 )
        heartbeat = 0;

    tail_chain();
}


void
base_purge ()
{
    int i;
    CHAR_DATA *rch;
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    bool found = FALSE;
    char buf[MAX_STRING_LENGTH];

    for ( i = 0; i < g_numTeams; found = FALSE, i++ )
    {
        if ( !team_table[i].active )
            continue;

        if ( !team_table[i].hq )
            continue;

        for (obj = team_table[i].hq->interior->contents; obj != NULL;
             obj = obj_next)
        {
            obj_next = obj->next_content;
            if ( obj->item_type == ITEM_TEAM_ENTRANCE )
                continue;
            found = TRUE;
            extract_obj(obj, obj->extract_me);
        }

        for (rch = team_table[i].hq->interior->people; rch;
             rch = rch->next_in_room)
        {
            if ( (rch->idle % 18) != 0 )
                continue;

            send_to_char
                ("A large soldier comes up and grabs you.  'What do you think you're\r\n"
                 "doing just sitting around?  We need you out there, man!'  With that\r\n"
                 "you're sent out into service...\r\n", rch);

            do_goto(rch, "random 0");
            found = TRUE;
        }

        if ( found )
        {
            sprintf(buf,
                    "Base Computer %s&X: '&WThe automated base purge system has been triggered...&X'&n\r\n",
                    team_table[i].who_name);
            send_to_team(buf, i);
        }
    }
}


static inline void
gs_line (CHAR_DATA * winner, const char *award, CHAR_DATA * toch)
{
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "&G| &n%-30s&X: %s%-11s &G|", award,
            (winner ? team_table[winner->team].namecolor : "&Y"),
            (winner ? winner->names : "None"));

    if ( toch )
    {
        send_to_char(buf, toch);
        send_to_char("\r\n", toch);
    }
    else
        do_gecho(NULL, buf);
}


#define HARMLESS(ch)  ((ch)->pcdata->gs_deaths - (ch)->pcdata->gs_kills)


#define GSHDR \
  "&G--------&WG&G-&WA&G-&WM&G-&WE&G-----&WS&G-&WT&G-&WA&G-&WT&G-&WI&G-&WS&G-&WT&G-&WI&G-&WC&G-&WS&G--------"
#define GSFTR \
  "&G-----------------------------------------------&n"

void
do_gamestats (CHAR_DATA * ch, char *argument)
{
    CHAR_DATA *kills = NULL;
    CHAR_DATA *booms = NULL;
    CHAR_DATA *deaths = NULL;
    CHAR_DATA *lemmings = NULL;
    CHAR_DATA *harmless = NULL;
    CHAR_DATA *dishonor = NULL;
    CHAR_DATA *idler = NULL;
    DESCRIPTOR_DATA *d;
    CHAR_DATA *gs;


    for ( d = descriptor_list; d; d = d->next )
    {
        if ( d->connected != CON_PLAYING || !(gs = d->character) )
            continue;
        if ( IS_IMMORTAL(gs ) || gs->pcdata->gs_kills <= 4 ||
            gs->pcdata->gs_hellpts <= 1)
            continue;

        if (!dishonor ||
            gs->pcdata->gs_hellpts > dishonor->pcdata->gs_hellpts)
            dishonor = gs;
    }

    for ( d = descriptor_list; d; d = d->next )
    {
        if ( d->connected != CON_PLAYING || !(gs = d->character) )
            continue;
        if ( IS_IMMORTAL(gs) )
            continue;

        if ( dishonor != gs )
        {
            if ( !kills )
            {
                if ( gs->pcdata->gs_kills )
                    kills = gs;
            }
            else if ( gs->pcdata->gs_kills > kills->pcdata->gs_kills )
                kills = gs;
        }

        if ( !deaths )
        {
            if ( gs->pcdata->gs_deaths )
                deaths = gs;
        }
        else if ( gs->pcdata->gs_deaths > deaths->pcdata->gs_deaths )
            deaths = gs;

        if ( !booms )
        {
            if ( gs->pcdata->gs_booms )
                booms = gs;
        }
        else if ( gs->pcdata->gs_booms > booms->pcdata->gs_booms )
            booms = gs;

        if ( !lemmings )
        {
            if ( gs->pcdata->gs_lemmings )
                lemmings = gs;
        }
        else if ( gs->pcdata->gs_lemmings > lemmings->pcdata->gs_lemmings )
            lemmings = gs;

        if ( !harmless )
        {
            if ( HARMLESS(gs) > 0 )
                harmless = gs;
        }
        else if ( HARMLESS(gs) > HARMLESS(harmless) )
            harmless = gs;

        if ( !idler )
        {
            if ( gs->pcdata->gs_idle )
                idler = gs;
        }
        else if ( gs->pcdata->gs_idle > idler->pcdata->gs_idle )
            idler = gs;
    }

    if ( ch )
        send_to_char(GSHDR "\r\n", ch);
    else
        do_gecho(NULL, GSHDR);

    gs_line(kills, "Most Deadly", ch);
    gs_line(deaths, "Most Dead", ch);
    gs_line(booms, "Demolition Man", ch);
    gs_line(lemmings, "The Prestigious Lemming Award", ch);
    gs_line(dishonor, "Most Dishonorable", ch);

    if ( harmless != idler )
    {
        gs_line(harmless, "Most Harmless", ch);
        gs_line(idler, "Sitting Duck Award", ch);
    }
    else
        gs_line(harmless, "Most Useless", ch);

    if ( ch )
        send_to_char(GSFTR "\r\n", ch);
    else
        do_gecho(NULL, GSFTR);
}


void
core_meltdown (int winteam)
{
    char buf[MAX_STRING_LENGTH];
    struct descriptor_data *d;
    int winscore;
    int count;

    winscore = calculate_worth();

    if ( worstTeam() == winteam )
    {
        sprintf(buf,
                "Team &W(%s&W)&n gets %d &Y+10&n added to their kills.\r\n",
                team_table[winteam].who_name, winscore);
        winscore += 10;
    }
    else
    {
        sprintf(buf, "Team &W(%s&W)&n gets %d added to their kills.\r\n",
                team_table[winteam].who_name, winscore);
    }

    do_gecho(NULL, buf);

    for ( d = descriptor_list; d; d = d->next )
    {
        if ( d->connected != CON_PLAYING || !d->character )
            continue;

        if ( d->character->team == winteam )
        {
            int prev_rank = RANK(d->character);

            count = d->character->kills;

            if ( !IS_SET(d->character->act, PLR_FREEZE_STATS) )
                d->character->kills += winscore;

            printf_to_char(d->character,
                           "You went from %d kills to %d kills.\r\n",
                           count, d->character->kills);

            if ( prev_rank != RANK(d->character) )
                report_new_rank(d->character, prev_rank);

            check_top_kills(d->character);
        }

        save_char_obj(d->character);
    }
}
