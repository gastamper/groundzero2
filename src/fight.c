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

void explode_obj(OBJ_DATA * obj);
void flash_obj(OBJ_DATA * obj);
void sound_obj(OBJ_DATA * obj);
void dark_obj(OBJ_DATA * obj, ROOM_DATA * room, int recurs_depth);
void teleport_obj(OBJ_DATA * obj);
void telelocator_obj(OBJ_DATA * obj);
void extinguish_obj(OBJ_DATA * obj, ROOM_DATA * room, int recurs_depth);
void report_condition(CHAR_DATA * ch, CHAR_DATA * victim);
void npc_act(struct char_data *);
OBJ_DATA *find_worn_eq_char(CHAR_DATA *, int, unsigned int);
int is_aggro(CHAR_DATA *, CHAR_DATA *);
OBJ_DATA *find_eq_char(CHAR_DATA *, int, unsigned int);
OBJ_DATA *revert_vehicle(CHAR_DATA *);
CHAR_DATA *find_manning(ROOM_DATA *, int);
bool man_heading(struct char_data *, int, int);
void pull_obj(OBJ_DATA *, CHAR_DATA *, int, char *);

char *npc_parse(const char *, struct char_data *, struct char_data *,
                struct obj_data *, const char *);


DECLARE_DO_FUN (do_use);
DECLARE_DO_FUN (do_pull);
DECLARE_DO_FUN (do_look);

int buttontimer = -1;
CHAR_DATA *buttonpresser = NULL;
CHAR_DATA *extract_list = NULL;
CHAR_DATA *mech_mob = NULL;


#define NUM_KILL_MESSAGES   41

const char *kill_messages[] = {
    "just &rwasted",
    "just &cfucked &Yup",
    "just &Bdecimated",
    "just &Xbutchered",
    "just &Yexecuted",
    "RAPED",
    "just &Xwasted&n ammo on",
    "fragged",
    "took out",
    "just &BF&X-&BU&X-&BB&X-&BA&X-&BR&Xed",     /* 10 */
    "just &Bwaxed",
    "beat-up",
    "yells &rFINISH HIM&n at",
    "schooled",
    "beat down",
    "knocked up",
    "ate",
    "planted",
    "buried",
    "off'd",                    /* 20 */
    "finished off",
    "annihilated",
    "decapitated",
    "ripped a new one for",
    "kill-stole",
    "&Ymolested",
    "&rb00m'd",
    "8=====>&W~~~",
    "just beat the living shit outta",
    "&Bcrushed",                /* 30 */
    "&ybashed",
    "&ctossed",
    "&RK&X.&RO&X'ed",
    "tore up",
    "busted up",
    "&Rwracked",
    "turned out",
    "knocked the fuck outta",
    "ripped apart",
    "&Yshredded",               /* 40 */
    "&Wowned&n"
};


bool
unarmed (struct char_data *ch)
{
    struct obj_data *wpn;
    struct obj_data *ammo;

    if ( (wpn = get_eq_char(ch, WEAR_WIELD)) )
        if ( IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED )
            || ((ammo = find_ammo(wpn)) && ammo->ammo > 0))
            return FALSE;

    if ( (wpn = get_eq_char(ch, WEAR_SEC)) )
        if ( IS_SET(wpn->general_flags, GEN_NO_AMMO_NEEDED )
            || ((ammo = find_ammo(wpn)) && ammo->ammo > 0))
            return FALSE;

    return TRUE;
}


bool
unarmed_followers (struct char_data * ch)
{
    struct char_data *fch;

    for ( fch = ch->in_room->people; fch; fch = fch->next_in_room )
        if ( fch->leader == ch && !unarmed(fch) )
            return FALSE;

    return TRUE;
}


void
add_deaths (CHAR_DATA * killer, CHAR_DATA * victim)
{
    int prev_rank = RANK(victim);

    if ( IS_NPC(victim) )
        return;

    if ( !IS_SET(victim->act, PLR_FREEZE_STATS) )
        victim->deaths++;
    victim->pcdata->gs_deaths++;

    if ( prev_rank != RANK(victim) )
        report_new_rank(victim, prev_rank);
    if ( victim->team == TEAM_NONE &&
         !IS_SET(victim->act, PLR_FREEZE_STATS) &&
         (victim->desc || victim->idle < 112) )
        victim->max_hit = --victim->pcdata->solo_hit;
}

void
add_kills (CHAR_DATA * killer, CHAR_DATA * victim)
{
    int prev_rank = RANK(killer);

    if ( IS_NPC(killer) )
        return;

    if ( !team_table[killer->team].independent &&
         killer->team == victim->team )
    {
        if ( victim->owner == killer || !IS_NPC(victim) )
        {
            killer->teamkill++;
            killer->pcdata->gs_hellpts += 2;
            victim->last_team_kill = killer;

            if ( killer->teamkill > 5 )
                boot(killer);
        }

        return;
    }

    if ( !IS_NPC(victim) )
    {
        killer->pcdata->gs_kills++;

        if ( unarmed(victim) && unarmed_followers(victim) )
            killer->pcdata->gs_hellpts++;
        if ( victim->armor < 5 )
            killer->pcdata->gs_hellpts++;
    }

    if ( victim->owner != killer && !IS_SET(killer->act, PLR_FREEZE_STATS) )
        killer->kills += (IS_NPC(victim) ? victim->worth : 1);

    if ( prev_rank != RANK(killer) )
        report_new_rank(killer, prev_rank);
    if ( killer->team == TEAM_NONE && !IS_NPC(victim) &&
         RANK(victim) >= RANK_MERC &&
         !IS_SET(killer->act, PLR_FREEZE_STATS) )
        killer->max_hit = ++killer->pcdata->solo_hit;
}


void
adjust_kp (CHAR_DATA * killer, CHAR_DATA * victim)
{
    CHAR_DATA *tanker;

    if ( killer == victim )
    {
        if ( !IS_NPC(victim) )
            victim->pcdata->gs_lemmings++;
        killer = NULL;
    }

    if ( victim->interior )
    {
        for (tanker = victim->interior->people; tanker;
             tanker = tanker->next_in_room)
        {
            if ( !IS_NPC(tanker) )
                add_deaths(killer, tanker);
        }
    }
    else if ( !IS_NPC(victim) )
        add_deaths(killer, victim);

    if ( !killer )
        return;

    if ( killer->interior )
    {
        for ( tanker = killer->interior->people; tanker;
              tanker = tanker->next_in_room )
        {
            if ( !IS_NPC(tanker) )
                add_kills(tanker, victim);
        }
    }
    else if ( !IS_NPC(killer) )
        add_kills(killer, victim);
}


void
kill_message (struct char_data *killer, struct char_data *vict,
             const char *with)
{
    char buf[MAX_STRING_LENGTH];
    struct descriptor_data *d;

    if ( vict != killer && killer )
    {
        sprintf(buf,
                "&n%s&W(%s&W)&n %s&n %s&W(%s&W)&n. %s&n\r\n",
                capitalize((IS_NPC(killer) ? killer->short_descript : killer->names)),
                team_table[killer->team].who_name,
                kill_messages[number_range(0, NUM_KILL_MESSAGES - 1)],
                (IS_NPC(vict) ? vict->short_descript : vict->names),
                team_table[vict->team].who_name,
                killer->kill_msg ? killer->kill_msg : "&gCARNAGE!");

        logmesg("%s(%s) killed %s(%s) with: %s.",
                   capitalize(killer->names), team_table[killer->team].name,
                   vict->names, team_table[vict->team].name,
                   without_colors(with));
    }
    else
    {
        sprintf(buf,
                "&n%s&W(%s&W) &Rdied &Wof natural causes.&n  %s&n\n\r",
                capitalize(IS_NPC(vict) ? vict->short_descript : vict->names),
                team_table[vict->team].who_name,
                vict->suicide_msg ? vict->
                suicide_msg : "&RWhat a dumbass!\r\n");

        logmesg("%s(%s) committed suicide.", capitalize(vict->names),
                   team_table[vict->team].name);
    }

    for ( d = descriptor_list; d; d = d->next )
        if ( d->connected == CON_PLAYING && d->character )
            if ( ground0_down ||
                 !IS_SET(d->character->comm, COMM_NOKILLS | COMM_QUIET) )
                send_to_char(buf, d->character);
}


int
door_dir (CHAR_DATA * ch, CHAR_DATA * victim)
{
    int diff_x = ch->in_room->x - victim->in_room->x;
    int diff_y = ch->in_room->y - victim->in_room->y;

    if ( diff_x )
        if ( diff_x > 0 )
            return DIR_WEST;
        else
            return DIR_EAST;
    else if ( diff_y )
        if ( diff_y > 0 )
            return DIR_SOUTH;
        else
            return DIR_NORTH;
    else
        return DIR_DOWN;
}


/* out of necessity, some of this code is duplicated in shoot_damage */
int
damage_char (CHAR_DATA * ch, CHAR_DATA * attacker, sh_int damage,
            bool noarmor, const char *with)
{
    int mod = (noarmor ? 0 : ch->armor);

    if ( damage >= 0 )
    {
        /* let's find out who is REALLY responsible for this =) */
        if ( attacker )
            while ( attacker->owner )
                attacker = attacker->owner;

        if ( attacker && (attacker != ch) )
            ch->last_hit_by = attacker;
    }

    if ( damage >= 0 )
    {
        ch->hit -= UMAX(1, damage - mod - ch->abs);

        if ( IS_SET(ch->act, ACT_DAMAGEABLE_ABS) && IS_NPC(ch) )
            ch->abs -= UMAX(1, number_range(damage / 4, damage));
    }
    else
        ch->hit -= damage;

    if ( ch->desc )
        ch->desc->fcommand = TRUE;

    return check_char_alive(ch, with);
}


int
shoot_damage (CHAR_DATA * ch, CHAR_DATA * attacker, sh_int damage,
             bool noarmor)
{
    int retval;
    int mod = (noarmor ? 0 : ch->armor);

    if ( damage >= 0 )
    {
        /* let's find out who is REALLY responsible for this =) */
        if ( attacker )
            while ( attacker->owner )
                attacker = attacker->owner;
        if ( attacker && (attacker != ch) )
            ch->last_hit_by = attacker;
    }

    if ( ch->interior && find_manning(ch->interior, MAN_SHIELD) &&
         (ch->shield_dir == door_dir(ch, attacker)) )
    {
        ch->hit -= UMAX(1, damage - mod - ch->abs);

        if ( IS_SET(ch->act, ACT_DAMAGEABLE_ABS) && IS_NPC(ch) )
            ch->abs -= UMAX(1, number_range(damage / 4, damage));
    }
    else if ( damage >= 0 )
        ch->hit -= UMAX(1, damage - mod);
    else
        ch->hit -= damage;

    if ( ch->desc )
        ch->desc->fcommand = TRUE;

    /* Medics heal themselves. */
    if ( IS_NPC(ch) && ch->ld_behavior == BEHAVIOR_MEDIC &&
         (ch->hit <= ch->max_hit / 2) )
    {
        do_use(ch, "kit");
    }

    retval = test_char_alive(ch);

    /* Boomers pull on death. */
    if ( retval && IS_NPC(ch) && (ch->ld_behavior == BEHAVIOR_BOOMER || ch->ld_behavior == BEHAVIOR_SEEKING_BOOMER))
    {
        do_pull(ch, "all");
    }

    return retval;
}

void
heal_char (CHAR_DATA * ch, sh_int bonus)
{
    ch->hit = UMIN(ch->max_hit, ch->hit + bonus);
}


/* Returns TRUE if object is killed. */
int
mod_hp_obj (OBJ_DATA * obj, sh_int damage_ch, sh_int damage_struct,
            CHAR_DATA * damage_from)
{
    if ( (obj->hp_char == 30000) || (obj->hp_struct == 30000) )
        return (FALSE);

    if ( obj->interior && obj->interior->people )
    {
        act("You feel an &Rimpact&n against $p.", obj->interior->people,
            obj, NULL, TO_CHAR);
        act("You feel an &Rimpact&n against $p.", obj->interior->people,
            obj, NULL, TO_ROOM);
    }

    obj->last_hit_by = damage_from;
    obj->hp_char -= UMAX(1, damage_ch);
    obj->hp_struct -= UMAX(1, damage_struct);
    return (!check_obj_alive(obj));
}


int
check_obj_alive (OBJ_DATA * obj)
{
    return ((obj->hp_char > 0) || (obj->hp_struct > 0));
}

int
damage_obj (OBJ_DATA * obj, sh_int damage_ch, sh_int damage_struct,
           CHAR_DATA * damage_from)
{
    int rv;

    mod_hp_obj(obj, damage_ch, damage_struct, damage_from);

    if ( (rv = !check_obj_alive(obj)) )
    {
        if ( !IS_SET(obj->extract_flags, EXTRACT_TELELOC_ON_EXTRACT) )
            claimObject(damage_from, obj);
        else
            obj->owner = NULL;

        bang_obj(obj, obj->extract_me);
    }

    return (rv);
}

int
damage_room (ROOM_DATA * a_room, sh_int damage_struct)
{
    int wall_shock_constant = 60000;
    int fakewall_shock_constant = 6000;
    sh_int check_wall;
    ROOM_DATA *temp_room;
    int roll;

    for ( check_wall = 0; check_wall < 6; check_wall++ )
    {
        if ( !IS_SET(a_room->exit[check_wall], EX_ISWALL) )
            continue;
        if ( IS_SET(a_room->exit[check_wall], EX_ISNOBREAKWALL) )
            continue;

        if ( IS_SET(a_room->exit[check_wall], EX_FAKEWALL) )
            roll = number_range(0, fakewall_shock_constant);
        else
            roll = number_range(0, wall_shock_constant);

        if ( roll < damage_struct )
        {
            temp_room = get_to_room(a_room, check_wall);
            falling_wall(a_room, check_wall);
            falling_wall(temp_room, rev_dir[check_wall]);
            return 1;
        }
    }
    return 0;
}


void
falling_wall (ROOM_DATA * a_room, sh_int dir)
{
    CHAR_DATA *those_in_room, *next_char;

    if ( (those_in_room = a_room->people) != NULL )
    {
        act("The wall to the $t &uaexplodes&n in a shower of rubble and debris.", a_room->people, dir_name[dir], NULL, TO_CHAR);
        act("The wall to the $t &uaexplodes&n in a shower of rubble and debris.", a_room->people, dir_name[dir], NULL, TO_ROOM);
        for (those_in_room = a_room->people; those_in_room;
             those_in_room = next_char)
        {
            next_char = those_in_room->next_in_room;
            send_to_char("You are &Rhit&n by falling rock!\n\r",
                         those_in_room);
            act("$n is &Rhit&n by falling rock!\n\r", those_in_room, NULL,
                NULL, TO_ROOM);
            damage_char(those_in_room, NULL,
                        (IS_SET(a_room->exit[dir], EX_FAKEWALL) ? 2000 :
                         4000), FALSE, "a falling wall");
        }
    }

    a_room->exit[dir] = 0;
}


void
shoot_messages (int shots, CHAR_DATA * ch, CHAR_DATA * victim,
               OBJ_DATA * weapon)
{
    char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];

    if ( shots == 0 )
        return;
    if ( IS_SET(weapon->general_flags, GEN_NO_AMMO_NEEDED) )
    {
        sprintf(buf2, "%d round%s", shots, (shots == 1) ? "" : "s");
        sprintf(buf, "$n %ss&n $N with $p.", weapon->attack_message);
        act(buf, ch, weapon, victim, TO_ROOM);
        sprintf(buf, "You %s&n $N with $p.", weapon->attack_message);
        act(buf, ch, weapon, victim, TO_CHAR);
        sprintf(buf, "&YOUCH!&n $N %ss&n you with $p!",
                weapon->attack_message);
        act(buf, victim, weapon, ch, TO_CHAR);
    }
    else
    {
        sprintf(buf2, "%d round%s", shots, (shots == 1) ? "" : "s");
        sprintf(buf, "$n &Rshoots&n %s from $p at $N.", buf2);
        act(buf, ch, weapon, victim, TO_ROOM);
        sprintf(buf, "You &Rshoot&n %s from $p at $N.", buf2);
        act(buf, ch, weapon, victim, TO_CHAR);
        sprintf(buf, "&YOUCH!&n $N &Rshot&n you with %s%s$p!",
                (shots != 1) ? buf2 : "", (shots != 1) ? " from " : "");
        act(buf, victim, weapon, ch, TO_CHAR);
    }

    if ( !test_char_alive(victim) )
    {
        if ( shots == 1 )
            sprintf(buf, "$n &uastaggers&n as $e is &Rhit!&n");
        else
            sprintf(buf, "$n &uastaggers&n as $e is &Rhit&n %d times!", shots);
        act(buf, victim, NULL, ch, TO_ROOM);
    }

    if ( victim->interior )
    {
        int diff_x = ch->in_room->x - victim->in_room->x;
        int diff_y = ch->in_room->y - victim->in_room->y;

        if ( diff_x )
            sprintf(buf, "You feel an &Rimpact&n from the %s.",
                    (diff_x > 0) ? "east" : "west");
        else if ( diff_y )
            sprintf(buf, "You feel an &Rimpact&n from the %s.",
                    (diff_y > 0) ? "north" : "south");
        else
            sprintf(buf, "You feel an &Rimpact&n from below.");

        act(buf, victim->interior->people, victim, NULL, TO_ROOM);
        act(buf, victim->interior->people, victim, NULL, TO_CHAR);
    }
}


int
autothrow_exp (CHAR_DATA * ch, CHAR_DATA * victim, OBJ_DATA * weapon,
              int verify)
{
    int dx;
    int dy;

    if ( !ch->fighting )
        return 0;

    dx = ABS(victim->in_room->x - ch->in_room->x);
    dy = ABS(victim->in_room->y - ch->in_room->y);

    if ( dx && dy )
        return 0;

    if ( dx > 2 || dy > 2 )
        return 0;

    if ( verify )
        return 1;

    act("You slip the pin from $p and cock your arm back.", ch, weapon,
        victim, TO_CHAR);
    act("$n slips the pin from $p and cocks $s arm back.", ch, weapon,
        victim, TO_ROOM);
    weapon->timer = weapon->cooked_timer;

    unequip_char(ch, weapon);

    if ( !dx && !dy )
    {
        act("You drop $p at $N's feet.  Better run!", ch, weapon, victim,
            TO_CHAR);
        act("$n drops $p at your feet.  Uh oh.", ch, weapon, victim,
            TO_VICT);
        act("$n drops $p at $N's feet.", ch, weapon, victim, TO_NOTVICT);
        drop_obj(ch, weapon);
    }
    else
    {
        act("You &uathrow&n $p at $N.", ch, weapon, victim, TO_CHAR);
        act("$n &uathrows&n $p at you.", ch, weapon, victim, TO_VICT);
        act("$n &uathrows&n $p at $N.", ch, weapon, victim, TO_NOTVICT);
        send_object(weapon, victim->in_room, (dx ? dx : dy));
    }

    claimObject(ch, weapon);
    return 1;
}


int
shoot_char (CHAR_DATA * ch, CHAR_DATA * victim, OBJ_DATA * weapon,
           int verify)
{
    sh_int dx, dy, start_x, start_y, pos_neg, count, shots;
    ROOM_DATA *current_room;
    OBJ_DATA *clone;
    CHAR_DATA *real_victim;
    struct obj_data *ammo;
    int real_shots = 0;
    int dam;

    if ( !verify && (dam = check_special_attack(ch, weapon)) )
        return (dam == -1 ? 0 : 1);

    if ( !weapon )
        return 0;

    if ( weapon->item_type == ITEM_EXPLOSIVE )
        return autothrow_exp(ch, victim, weapon, verify);

    if ( !(ammo = find_ammo(weapon) ) &&
        !IS_SET(weapon->general_flags, GEN_NO_AMMO_NEEDED))
        return 0;

    if ( !ch->fighting )
        return 0;

    if ( verify )
        return 1;

    if ( weapon->wait_time-- > 0 )
        return 1;

    if ( weapon->rounds_delay > 0 )
        weapon->wait_time = weapon->rounds_delay;

    if ( ammo )
        dam = ammo->damage_char[0];
    else
        dam = weapon->damage_char[0];

    for ( shots = 0; shots < weapon->rounds_per_second; shots++ )
    {
        if ( !IS_SET(weapon->general_flags, GEN_NO_AMMO_NEEDED ) &&
            ammo->ammo <= 0)
        {
            send_to_char("&uaCLICK!&n\n\r", ch);
            act("&uaCLICK!&n  What a pity, $n seems to have run out of ammo.", ch, NULL, NULL, TO_ROOM);
            obj_from_obj(ammo);
            extract_obj(ammo, ammo->extract_me);
            shoot_messages(real_shots, ch, victim, weapon);
            return 0;
        }

        if ( !IS_NPC(ch ) &&
            !IS_SET(weapon->general_flags, GEN_NO_AMMO_NEEDED))
            ammo->ammo--;

        start_x = ch->in_room->x;
        start_y = ch->in_room->y;
        dx = start_x - victim->in_room->x;
        dy = start_y - victim->in_room->y;

        if ( (dx > 0) || (dy > 0) )
            pos_neg = -1;
        else
            pos_neg = 1;

        real_victim = victim;

        if ( dx || dy )
        {
            struct obj_data *trailer = NULL;

            if ( IS_SET(weapon->general_flags, GEN_LEAVE_TRAIL) )
                trailer = weapon;
            else if ( ammo && IS_SET(ammo->general_flags, GEN_LEAVE_TRAIL) )
                trailer = ammo;

            if ( trailer && trailer->spawnTrail )
            {
                struct obj_data *spawn;
                int max;
                int i;

                current_room = ch->in_room;
                max = (dx ? ABS(dx) : ABS(dy));

                for ( i = 1; i <= max; i++ )
                {
                    current_room = index_room(current_room,
                                              (dx ? pos_neg : 0),
                                              (dy ? pos_neg : 0));

                    if ( trailer->spawnMax != -1 )
                    {
                        count = 0;

                        for (spawn = current_room->contents;
                             count < trailer->spawnMax && spawn;
                             spawn = spawn->next_content)
                        {
                            if ( spawn->pIndexData == trailer->spawnTrail )
                                count++;
                        }

                        if ( count >= trailer->spawnMax )
                            continue;
                    }

                    spawn = create_object(trailer->spawnTrail, 0);
                    spawn->timer = trailer->spawnTimer;
                    claimObject(ch, spawn);
                    obj_to_room(spawn, current_room);
                }
            }
        }

        if ( dx || dy )
            for ( dx ? (count = start_x + pos_neg )
                 : (count = start_y + pos_neg);
                 dx ? (count != victim->in_room->x) : (count !=
                                                       victim->in_room->y);
                 count = count + pos_neg)
            {
                current_room =
                    index_room(ch->in_room->this_level->rooms_on_level,
                               dx ? count : start_x, dy ? count : start_y);

                if ( current_room->people )
                {
                    if ( number_range(0, 10) + weapon->range - 4 > 0 )
                    {
                        act("A bullet whizzes overhead.",
                            current_room->people, NULL, NULL, TO_ROOM);
                        act("A bullet whizzes overhead.",
                            current_room->people, NULL, NULL, TO_CHAR);
                    }
                    else
                    {
                        act("&YOops.&n  You &Rhit&n $N by accident.  &YDoh!&n", ch, NULL, current_room->people, TO_CHAR);
                        real_victim = current_room->people;
                        break;
                    }
                }
            }

        if ( real_victim != victim )
        {
            act("&YOUCH!&n  $N &Rnailed&n you with $p!", real_victim,
                weapon, ch, TO_CHAR);
            act("$n &uastaggers&n as $e is &Rhit!&n", real_victim, NULL,
                ch, TO_ROOM);
        }
        else
            real_shots++;

        if ( dam >= 0 )
            real_victim->fighting = ch;

        if ( IS_SET(weapon->general_flags, GEN_NO_AMMO_NEEDED) )
        {
            if ( weapon->extract_flags )
            {
                clone = create_object(weapon->pIndexData, 0);
                clone_object(weapon, clone);
                obj_to_char(clone, real_victim);
                claimObject(ch, clone);
                bang_obj(clone, 1);
            }
            else
            {
                dam = (int) ((double) weapon->damage_char[0] *
                                      weapon->dam_mult);

                if ( shoot_damage(real_victim, ch, dam,
                                  IS_SET(weapon->general_flags,
                                         GEN_IGNORE_ARMOR)) )
                {
                    shoot_messages(real_shots, ch, victim, weapon);
                    check_char_alive(real_victim, weapon->short_descr);
                    return 1;   /* ie was shot to death */
                }
            }
        }
        else
        {
            if ( ammo->extract_flags )
            {
                clone = create_object(ammo->pIndexData, 0);
                clone_object(ammo, clone);
                obj_to_char(clone, real_victim);
                claimObject(ch, clone);
                bang_obj(clone, 1);
            }
            else
            {
                dam = (int) ((double) ammo->damage_char[0] *
                                      weapon->dam_mult);

                if (shoot_damage(real_victim, ch, dam,
                                 IS_SET(ammo->general_flags,
                                        GEN_IGNORE_ARMOR)))
                {
                    shoot_messages(real_shots, ch, victim, weapon);
                    check_char_alive(real_victim, weapon->short_descr);
                    return 1;   /* ie was shot to death */
                }
            }
        }                       /* end of if ammoless or not */

    }

    shoot_messages(real_shots, ch, victim, weapon);
    return 1;
}


/* Routes to correct _obj function */
void
bang_obj (OBJ_DATA * obj, int perm_extract)
{
    if ( !obj )
    {
        logmesg("REALBUG: Attempt to bang a NULL object.");
        return;
    }
    else if ( !obj->carried_by && !obj->in_room )
    {
        logmesg
            ("REALBUG: Attempt to bang object not in room or on player (no longer extracting).");
        return;
    }

    if ((obj->in_room == safe_area ||
         (obj->carried_by && obj->carried_by->in_room == safe_area)) &&
        safe_area->people)
    {
        CHAR_DATA *chars_hit;

        chars_hit = safe_area->people;
        act("Aren't you glad you're in the safe room?", chars_hit, obj,
            NULL, TO_CHAR);
        act("Aren't you glad you're in the safe room?", chars_hit, obj,
            NULL, TO_ROOM);
        extract_obj(obj, perm_extract || obj->extract_me);
        return;
    }
    else if (obj->in_room == graveyard_area
             || (obj->carried_by &&
                 obj->carried_by->in_room == graveyard_area))
    {
        extract_obj(obj, perm_extract || obj->extract_me);
        return;
    }

    if ( IS_SET(obj->extract_flags, EXTRACT_TELELOC_ON_EXTRACT) )
        telelocator_obj(obj);

    if (!obj->owner && obj->last_hit_by &&
        obj->last_hit_by->valid == VALID_VALUE)
        claimObject(obj->last_hit_by, obj);

    if ( !obj->extract_flags )
    {
        if ( obj->in_room )
        {
            if ( obj->in_room->people )
            {
                act("$p falls apart.", obj->in_room->people, obj, NULL,
                    TO_CHAR);
                act("$p falls apart.", obj->in_room->people, obj, NULL,
                    TO_ROOM);
            }
        }
        else
        {
            act("$p falls apart.", obj->carried_by->in_room->people, obj,
                NULL, TO_CHAR);
            act("$p falls apart.", obj->carried_by->in_room->people, obj,
                NULL, TO_ROOM);
        }
    }
    else
    {
        if ( IS_SET(obj->extract_flags, EXTRACT_EXPLODE_ON_EXTRACT) )
            explode_obj(obj);
        if ( IS_SET(obj->extract_flags, EXTRACT_BLIND_ON_EXTRACT) )
            flash_obj(obj);
        if ( IS_SET(obj->extract_flags, EXTRACT_BURN_ON_EXTRACT) )
            burn_obj(obj);
        if ( IS_SET(obj->extract_flags, EXTRACT_CHOKE_ON_EXTRACT) )
            choke_obj(obj,
                      obj->in_room ? obj->in_room : obj->carried_by->
                      in_room, 2);
        if ( IS_SET(obj->extract_flags, EXTRACT_STUN_ON_EXTRACT) )
            sound_obj(obj);
        if ( IS_SET(obj->extract_flags, EXTRACT_DARK_ON_EXTRACT) )
            dark_obj(obj,
                     obj->in_room ? obj->in_room : obj->carried_by->
                     in_room, 2);
        if ( IS_SET(obj->extract_flags, EXTRACT_EXTINGUISH_ON_EXTRACT) )
            extinguish_obj(obj,
                           (obj->in_room ? obj->in_room : obj->carried_by->
                            in_room), 2);
        if ( IS_SET(obj->extract_flags, EXTRACT_TELERAN_ON_EXTRACT) )
            teleport_obj(obj);
    }

    extract_obj(obj, perm_extract || obj->extract_me);
}


void
teleport_obj (OBJ_DATA * obj)
{
    CHAR_DATA *next_vict;
    CHAR_DATA *vict;
    ROOM_DATA *room =
        (obj->in_room ? obj->in_room : obj->carried_by->in_room);

    for ( vict = room->people; vict; vict = next_vict )
    {
        next_vict = vict->next_in_room;

        if ( IS_NPC(vict) )
            continue;

        act("$n disappears in a &Wflash!", vict, NULL, NULL, TO_ROOM);
        act("A bright flash erupts.  You feel yourself transported.", vict,
            NULL, NULL, TO_VICT);

        do_goto(vict, "random 0");
        complete_movement(vict, DIR_UP);
        WAIT_STATE(vict, 2);
    }
}


void
telelocator_obj (OBJ_DATA * obj)
{
    CHAR_DATA *ch;

    if ( !(ch = obj->owner) )
        return;
    if ( !obj->in_room )
        return;
    if ( obj->in_room == ch->in_room )
        return;
    if ( obj->in_room->level )
        return;
    if ( !ch->desc )
        return;

    act("$n disappears in a bright &Wflash!", ch, NULL, NULL, TO_ROOM);
    act("You are teleported by $p&n.", ch, obj, NULL, TO_CHAR);

    char_from_room(ch);
    char_to_room(ch, obj->in_room);
    do_look(ch, "auto");
    complete_movement(ch, DIR_UP);
    WAIT_STATE(ch, 4);

    act("$n appears in a bright &Wflash!", ch, NULL, NULL, TO_ROOM);
}


void
burn_obj (OBJ_DATA * obj)
{
    CHAR_DATA *next_vict;
    OBJ_DATA *the_fire;
    ROOM_DATA *toroom;
    CHAR_DATA *vict;
    int i;

    if ( !obj->in_room || VNUM_FIRE == -1 )
        return;

    if ( IS_SET(obj->general_flags, GEN_BURNS_ROOM) )
    {
        OBJ_DATA *v_obj;
        OBJ_DATA *next_v_obj;

        for ( vict = obj->in_room->people; vict; vict = next_vict )
        {
            next_vict = vict->next_in_room;

            if ( IS_NPC(vict) && IS_SET(vict->act, ACT_NO_BURN) )
                continue;

            act("$n is &Rburned!", vict, NULL, NULL, TO_ROOM);
            act("Blisters erupt on your skin as $p burns you!", vict, obj,
                NULL, TO_CHAR);

            damage_char(vict, obj->owner, obj->damage_char[1], FALSE,
                        "a fire");
        }

        toroom = obj->in_room;

        for ( v_obj = toroom->contents; v_obj; v_obj = next_v_obj )
        {
            next_v_obj = v_obj->next_content;

            if (v_obj == obj ||
                !IS_SET(v_obj->general_flags, GEN_COMBUSTABLE))
                continue;

            if ( toroom->people )
            {
                act("$p is &Rburned!", toroom->people, v_obj, NULL,
                    TO_CHAR);
                act("$p is &Rburned!", toroom->people, v_obj, NULL,
                    TO_ROOM);
            }

            damage_obj(v_obj, obj->damage_char[1],
                       obj->damage_structural[1], obj->owner);
        }

        return;
    }

    toroom = (obj->carried_by ? obj->carried_by->in_room : obj->in_room);

    if ( toroom->people != NULL )
    {
        act("$p &Ybursts&n into &Rflame&n setting the room on fire!",
            toroom->people, obj, NULL, TO_ROOM);
        act("$p &Ybursts&n into &Rflame&n setting the room on fire!",
            toroom->people, obj, NULL, TO_CHAR);

        for ( vict = toroom->people; vict; vict = next_vict )
        {
            next_vict = vict->next_in_room;

            if ( IS_NPC(vict) && IS_SET(vict->act, ACT_NO_BURN) )
                continue;

            act("$n is &Rburned!", vict, NULL, NULL, TO_ROOM);
            act("You are &Rburned!", vict, obj, NULL, TO_CHAR);
            damage_char(vict, obj->owner, obj->damage_char[1], FALSE,
                        "a fire");
        }
    }

    for ( i = 0; i < obj->num_extract_burn; i++ )
    {
        the_fire = create_object(get_obj_index(VNUM_FIRE), 0);
        the_fire->timer = obj->burn_time;
        claimObject(obj->owner, the_fire);
        obj_to_room(the_fire, toroom);
    }
}


void
choke_obj (OBJ_DATA * obj, ROOM_DATA * room, int recurs_depth)
{
    int dir;
    CHAR_DATA *chars_hit, *next_char;
    OBJ_DATA *the_gas;

    if ( VNUM_GAS == -1 )
        return;

    if ( IS_SET(obj->general_flags, GEN_CHOKES_ROOM) )
    {
        /* guaranteed to be observers from call */
        for ( chars_hit = room->people; chars_hit; chars_hit = next_char )
        {
            next_char = chars_hit->next_in_room;

            if ( IS_NPC(chars_hit) && IS_SET(chars_hit->act, ACT_NO_GAS) )
                continue;
            if (find_worn_eq_char
                (chars_hit, SEARCH_GEN_FLAG, GEN_ANTI_POISON))
                continue;

            act("$n coughs and chokes on the poison gas!", chars_hit, NULL,
                NULL, TO_ROOM);
            act("$p fills your lungs and you choke!", chars_hit, obj, NULL,
                TO_CHAR);

            /* make it ignore armor */
            damage_char(chars_hit, obj->owner, obj->damage_char[1], TRUE,
                        "poison");
        }
        return;
    }

    if ( IS_SET(room->room_flags, ROOM_GAS) )
        return;

    the_gas = create_object(get_obj_index(VNUM_GAS), 0);
    the_gas->timer = obj->choke_time;
    claimObject(obj->owner, the_gas);
    obj_to_room(the_gas, room);

    chars_hit = room->people;

    if ( chars_hit )
    {
        act("$p &Ybursts,&n filling the room with poison gas!", chars_hit,
            obj, NULL, TO_ROOM);
        act("$p &Ybursts,&n filling the room with poison gas!", chars_hit,
            obj, NULL, TO_CHAR);
        for ( ; chars_hit; chars_hit = next_char )
        {
            next_char = chars_hit->next_in_room;

            if ( IS_NPC(chars_hit) && IS_SET(chars_hit->act, ACT_NO_GAS) )
                continue;
            if ( find_eq_char(chars_hit, SEARCH_GEN_FLAG, GEN_ANTI_POISON) )
                continue;

            act("$n coughs and chokes!", chars_hit, NULL, NULL, TO_ROOM);
            act("You cough and choke as poison gas fills your lungs!",
                chars_hit, obj, NULL, TO_CHAR);
            damage_char(chars_hit, obj->owner, obj->damage_char[1], TRUE,
                        "poison");
        }
    }

    if ( !recurs_depth )
        return;

    for ( dir = 0; dir < 4; dir++ )
        if ( !room->exit[dir] )
            choke_obj(obj, get_to_room(room, dir), recurs_depth - 1);
}


void
flash_obj (OBJ_DATA * obj)
{
    CHAR_DATA *chars_hit;

    if ( obj->in_room )
        chars_hit = obj->in_room->people;
    else
        chars_hit = obj->carried_by->in_room->people;

    if ( chars_hit )
    {
        act("$p &Yflares&n brightly!", chars_hit, obj, NULL, TO_ROOM);
        act("$p &Yflares&n brightly!", chars_hit, obj, NULL, TO_CHAR);
    }
    else
        return;

    for ( ; chars_hit; chars_hit = chars_hit->next_in_room )
    {
        OBJ_DATA *prot_obj;

        if ( IS_NPC(chars_hit) && IS_SET(chars_hit->act, ACT_NO_BLIND) )
            continue;
	
	// Don't blind administrators if holylight is set
	if ( IS_SET(chars_hit->act, PLR_HOLYLIGHT) )
	    continue;

        if ( ((prot_obj = get_eq_char(chars_hit, WEAR_HEAD)) == NULL ) ||
            !IS_SET(prot_obj->general_flags, GEN_ANTI_BLIND))
        {
            act("$n is &Yblinded!", chars_hit, NULL, NULL, TO_ROOM);
            act("You are &Yblinded!", chars_hit, NULL, NULL, TO_CHAR);
            SET_BIT(chars_hit->affected_by, AFF_BLIND);
        }
        else
            act("Aren't you glad you are wearing $p?", chars_hit, prot_obj,
                NULL, TO_CHAR);
    }
}


void
sound_obj (OBJ_DATA * obj)
{
    CHAR_DATA *chars_hit;

    if ( obj->in_room )
        chars_hit = obj->in_room->people;
    else
        chars_hit = obj->carried_by->in_room->people;

    for ( ; chars_hit; chars_hit = chars_hit->next_in_room )
    {
        OBJ_DATA *prot_obj;

        if ( IS_NPC(chars_hit) && IS_SET(chars_hit->act, ACT_NO_STUN) )
            continue;

        if ( ((prot_obj = get_eq_char(chars_hit, WEAR_HEAD)) == NULL ) ||
            !IS_SET(prot_obj->general_flags, GEN_ANTI_DAZE))
        {
            act("$n is &Ydazed!", chars_hit, NULL, NULL, TO_ROOM);
            act("You are &Ydazed!", chars_hit, NULL, NULL, TO_CHAR);
            SET_BIT(chars_hit->affected_by, AFF_DAZE);
        }
        else
            act("Aren't you glad you are wearing $p?", chars_hit, prot_obj,
                NULL, TO_CHAR);
    }
}


void
dark_obj (OBJ_DATA * obj, ROOM_DATA * room, int recurs_depth)
{
    int dir;
    OBJ_DATA *the_darkness;

    if ( VNUM_DARK == -1 )
        return;

    if ( IS_SET(room->room_flags, ROOM_DARK) )
        return;

    obj_to_room(the_darkness =
                create_object(get_obj_index(VNUM_DARK), 0), room);
    claimObject(obj->owner, the_darkness);
    the_darkness->timer = LIFETIME_SMOKE;

    if ( room->people )
    {
        act("The room fills with smoke making it impossible to see.",
            room->people, NULL, NULL, TO_ROOM);
        act("The room fills with smoke making it impossible to see.",
            room->people, NULL, NULL, TO_CHAR);
    }

    if ( !recurs_depth )
        return;

    for ( dir = 0; dir < 4; dir++ )
        if ( !room->exit[dir] )
            dark_obj(obj, get_to_room(room, dir), recurs_depth - 1);
}

void
extinguish_obj (OBJ_DATA * obj, ROOM_DATA * room, int recurs_depth)
{
    int dir;
    void fire_extinguisher(ROOM_DATA *, int);

    fire_extinguisher(room, 0);

    if ( !recurs_depth )
        return;

    for ( dir = 0; dir < 4; dir++ )
        if ( !room->exit[dir] )
            extinguish_obj(obj, get_to_room(room, dir), recurs_depth - 1);
}


#define DAMCH(o,x)      ((o)->damage_char[(x)])
#define DAMST(o,x)      ((o)->damage_structural[(x)])
#define F_OR_0(i,j)     ((i) ? ((int)((float)i/(float) (i-j)-.5)+1) : 0)

void
explode_obj (OBJ_DATA * obj)
{
    char tochar_buf[MAX_STRING_LENGTH];
    char toroom_buf[MAX_STRING_LENGTH];
    CHAR_DATA *chars_hit, *ch_next, *a_moron = NULL, *listenners;
    CHAR_DATA *the_owner = NULL;
    sh_int max_dist, dist_x, dist_y, count, dir, x_mod, y_mod;
    ROOM_DATA *start_room, *temp_room;
    OBJ_DATA *objs_hit, *obj_next, *first_obj;
    int dam_ch[3], dam_st[3];
    int i = 0;

    if ( obj->carried_by )
    {
        a_moron = obj->carried_by;

        if ( !IS_SET(obj->general_flags, GEN_CAN_BURY) )
        {
            if ( !*obj->explode_desc )
            {
                strcpy(tochar_buf, "$p &uaexplodes&n in your hand!");
                strcpy(toroom_buf, "$p &uaexplodes&n in $n's hand!");
            }
            else
            {
                sprintf(tochar_buf, "%s &uaexplodes&n in your hand!",
                        obj->explode_desc);
                sprintf(toroom_buf, "%s &uaexplodes&n in $n's hand!",
                        obj->explode_desc);
            }

            for ( i = 0; i < obj->num_extract_explode; i++ )
            {
                act(tochar_buf, obj->carried_by, obj, NULL, TO_CHAR);
                act(toroom_buf, obj->carried_by, obj, NULL, TO_ROOM);
            }
        }
        else
        {
            act("Oops!  $n &uastepped&n on $p!", obj->carried_by, obj,
                NULL, TO_ROOM);
            act("Oops!  You &uastepped&n on $p!", obj->carried_by, obj,
                NULL, TO_CHAR);
        }

        start_room = obj->carried_by->in_room;
        obj_from_char(obj);
        chars_hit = start_room->people;
    }
    else
    {
        start_room = obj->in_room;
        obj_from_room(obj);
        chars_hit = start_room->people;

        if ( chars_hit )
        {
            if ( !obj->explode_desc || !*obj->explode_desc )
            {
                strcpy(tochar_buf,
                       "$p &uaexplodes&n, sending shrapnel flying!");
            }
            else
            {
                sprintf(tochar_buf,
                        "%s &uaexplodes&n, sending shrapnel flying!",
                        obj->explode_desc);
            }

            for ( i = 0; i < obj->num_extract_explode; i++ )
            {
                act(tochar_buf, chars_hit, obj, NULL, TO_CHAR);
                act(tochar_buf, chars_hit, obj, NULL, TO_ROOM);
            }
        }
    }

    for ( i = 0; i < 3; i++ )
    {
        dam_ch[i] = obj->damage_char[i] * obj->num_extract_explode;
        dam_st[i] = obj->damage_structural[i] * obj->num_extract_explode;
    }

    /* find damage extent */
    max_dist =
        UMAX(F_OR_0(dam_ch[1], dam_ch[2]), F_OR_0(dam_st[1], dam_st[2]));

    /* damage chars in room where damage initiated */
    for ( ; chars_hit; chars_hit = ch_next )
    {
        ch_next = chars_hit->next_in_room;

        act("You are &Rhit&n by shrapnel!\n\r", chars_hit, NULL, NULL,
            TO_CHAR);
        act("$n is &Rhit&n by shrapnel!\n\r", chars_hit, NULL, NULL,
            TO_ROOM);

        if ( chars_hit->interior && chars_hit->interior->people )
        {
            act("You hear shrapnel ricochet off your vehicle's exterior!",
                chars_hit->interior->people, NULL, chars_hit, TO_CHAR);
            act("You hear shrapnel ricochet off your vehicle's exterior!",
                chars_hit->interior->people, NULL, chars_hit, TO_ROOM);
        }

        chars_hit->last_hit_by = obj->owner;
        damage_char(chars_hit, obj->owner, dam_ch[(chars_hit != a_moron)],
                    FALSE, obj->short_descr);
    }

    /* damage objects in room where damage initiated */
    objs_hit = start_room->contents;
    first_obj = objs_hit;
    the_owner = NULL;

    for ( ; objs_hit; objs_hit = objs_hit->next_content )
    {
        if ( !objs_hit->in_room )
        {
            sprintf(log_buf,
                    "%s could not be damaged correctly because it "
                    "was both in a room and not (was extracted). (local)",
                    objs_hit->name);
            logmesg(log_buf);
            extract_obj(objs_hit, objs_hit->extract_me);
        }

        strcpy(tochar_buf, "$p is &Rhit&n by shrapnel!");

        if ( objs_hit->in_room->people )
        {
            act(tochar_buf, objs_hit->in_room->people, objs_hit, NULL,
                TO_CHAR);
            act(tochar_buf, objs_hit->in_room->people, objs_hit, NULL,
                TO_ROOM);
        }

        mod_hp_obj(objs_hit, dam_ch[1], dam_st[1], obj->owner);
    }

    /*
     * All this convoluted stuff is done so that it doesn't happen that an
     * object damages another object which explodes destroying another, then
     * the first tries to damage the one already destroyed by what it causes
     * to explode.  ex.
     *   -> = causes to explode/be extracted
     *   AP mine -> clip
     *   AP mine -> grenade -> C4
     *   AP mine -> grenade -> pants
     *   AP mine -> C4 *** crash *** because was already destroyed by grenade
     */

    for ( objs_hit = first_obj; objs_hit; objs_hit = obj_next )
    {
        obj_next = objs_hit->next_content;

        if ( !check_obj_alive(objs_hit) )
        {
            objs_hit->destination = objs_hit->in_room;
            if ( !objs_hit->in_room )
                logmesg("explode_obj1: obj not in room");
            obj_from_room(objs_hit);
            obj_to_room(objs_hit, explosive_area);
        }
    }

    /* damage walls in room where damage initiated */
    damage_room(start_room, dam_st[1]);

    /* figure out who all hears it */
    for ( listenners = char_list; listenners; listenners = listenners->next )
    {
        dist_x = listenners->in_room->x - start_room->x;
        dist_y = listenners->in_room->y - start_room->y;

        if ( (listenners->in_room->x >= 0) && (listenners->in_room->y >= 0 )
            && (dist_x < 5) && (dist_x > -5) && (dist_y < 5) &&
            (dist_y > -5) &&
            (listenners->in_room->level == start_room->level) &&
            (listenners->in_room != start_room))
        {
            if ( obj->num_extract_explode < 10 )
                act("You hear $p &uaexplode&n nearby.", listenners, obj,
                    NULL, TO_CHAR);
            else if ( obj->num_extract_explode < 20 )
                act("You hear a massive boom as $p &uaexplodes&n nearby.",
                    listenners, obj, NULL, TO_CHAR);
            else
                act("You go temporarily deaf from the thunderous boom of $p &uaexploding&n nearby.", listenners, obj, NULL, TO_CHAR);
        }
    }

    /* go in each direction to do damage in other rooms */
    /* in retrospect, some recursion could have been done here to avoid copying
       alot of this code.  Wrote this in 95 though and changing this could
       affect stability.  11/8/98 */
    for ( dir = 0; dir < 4; dir++ )
    {
        temp_room = start_room;
        for ( count = 0; !temp_room->exit[dir] && count < max_dist; count++ )
        {
            x_mod = y_mod = 0;
            if ( dir )
                if ( dir > 1 )
                    if ( dir > 2 )
                        x_mod = -1;
                    else
                        y_mod = -1;
                else
                    x_mod = 1;
            else
                y_mod = 1;
            temp_room = index_room(temp_room, x_mod, y_mod);
            /* damage the characters */

            if ( temp_room->people )
            {
                for (chars_hit = temp_room->people; chars_hit;
                     chars_hit = ch_next)
                {
                    ch_next = chars_hit->next_in_room;
                    act("You are &Rhit&n by shrapnel!\n\r", chars_hit,
                        NULL, NULL, TO_CHAR);
                    act("$n is &Rhit&n by shrapnel!\n\r", chars_hit, NULL,
                        NULL, TO_ROOM);
                    chars_hit->last_hit_by = obj->owner;
                    if ( chars_hit != obj->owner )
                    {
                        int pdam;

                        dist_x = chars_hit->in_room->x - start_room->x;
                        dist_y = chars_hit->in_room->y - start_room->y;

                        pdam =
                            dam_ch[1] - (dam_ch[1] -
                                         dam_ch[2]) *
                            (dist_x ? abs(dist_x) : abs(dist_y));

                        if ( obj->owner && !IS_NPC(obj->owner) && pdam > 0 )
                            obj->owner->pcdata->gs_booms++;


                        damage_char(chars_hit, obj->owner, pdam, FALSE,
                                    obj->short_descr);
                    }
                    else
                        the_owner = obj->owner;
                }
            }
            /* damage the objects */
            if ( temp_room->contents )
            {
                first_obj = objs_hit = temp_room->contents;
                for ( ; objs_hit; objs_hit = obj_next )
                {
                    obj_next = objs_hit->next_content;

                    if ( !objs_hit->in_room )
                    {
                        sprintf(log_buf,
                                "%s could not be damaged correctly "
                                "because it was both in a room and not (was "
                                "extracted) (distant).", objs_hit->name);
                        logmesg(log_buf);
                        extract_obj(objs_hit, objs_hit->extract_me);
                    }

                    if ( objs_hit->in_room->people )
                    {
                        act("$p is &Rhit&n by shrapnel!",
                            objs_hit->in_room->people, objs_hit, NULL,
                            TO_CHAR);
                        act("$p is &Rhit&n by shrapnel!",
                            objs_hit->in_room->people, objs_hit, NULL,
                            TO_ROOM);
                    }

                    mod_hp_obj(objs_hit,
                               (dam_ch[1] -
                                count * (dam_ch[1] - dam_ch[2])),
                               (dam_st[1] -
                                count * (dam_st[1] - dam_st[2])),
                               obj->owner);
                }

                /* ship out the dead stuff */
                for ( objs_hit = first_obj; objs_hit; objs_hit = obj_next )
                {
                    obj_next = objs_hit->next_content;
                    if ( !check_obj_alive(objs_hit) )
                    {
                        objs_hit->destination = objs_hit->in_room;
                        if ( !objs_hit->in_room )
                            logmesg("explode_obj2: obj not in room");
                        obj_from_room(objs_hit);
                        obj_to_room(objs_hit, explosive_area);
                    }
                }
            }
            /* damage the walls */
            damage_room(temp_room, dam_st[1]);
        }
    }

    /* ensure that the owner does get his kills if he killed others and 
       himself ie the owner must be damaged last. */
    if ( the_owner )
    {
        dist_x = the_owner->in_room->x - start_room->x;
        dist_y = the_owner->in_room->y - start_room->y;
        damage_char(the_owner, the_owner, dam_ch[1] -
                    (dam_ch[1] -
                     dam_ch[2]) * (dist_x ? abs(dist_x) : abs(dist_y)),
                    FALSE, obj->short_descr);
    }

    /* all the itsy bitsy pieces from destroyed stuff goes the way of the 
       dodo now. */
    while ( (objs_hit = explosive_area->contents) != NULL )
    {
        if ( objs_hit->destination )
        {
            ROOM_DATA *temp_room;

            if ( !objs_hit->in_room )
                logmesg("explode_obj3: obj not in room");
            obj_from_room(objs_hit);
            temp_room = objs_hit->destination;
            objs_hit->destination = NULL;
            obj_to_room(objs_hit, temp_room);
        }
        bang_obj(objs_hit, objs_hit->extract_me);
    }
    obj_to_room(obj, start_room);
}


void
update_pos (CHAR_DATA * ch)
{
    return;
}


void
report_condition (CHAR_DATA * ch, CHAR_DATA * victim)
{
    if ( ch->interior && IS_SET(ch->temp_flags, IS_VEHICLE) )
    {
        struct char_data *ich;

        for ( ich = ch->interior->people; ich; ich = ich->next_in_room )
            report_condition(ich, victim);

        return;
    }

    if ( victim->hit > 0 )
        act("$N&uc$t&n", ch, diagnose(victim), victim, TO_CHAR);
}



CHAR_DATA *next_violence;

/* reason for this being global is for concern about people offing those
   in front of them in the character list (see extract_char) in handler.c */

#define VIOLENCE_OKAY(ch, targ, x) \
  ((ch)->in_room && (targ) && (ch)->in_room != safe_area && \
   (ch)->in_room != graveyard_area && (ch)->in_room != store_area && \
   (targ)->in_room && (targ)->in_room != safe_area && \
   (targ)->in_room != graveyard_area && (targ)->in_room != store_area && \
   can_see_linear_char((ch), (targ), 1, (x)))

void
violence_update ()
{
    CHAR_DATA *ch;

    for ( ch = char_list; ch; ch = next_violence )
    {
        next_violence = ch->next;

        if ( (ch->in_room->level < 0) && !ch->desc && !IS_NPC(ch ) &&
            !IS_IMMORTAL(ch) && ch->in_room != graveyard_area)
            do_teleport(ch, "");
        if ( ch->valid != VALID_VALUE )
        {
            sprintf(log_buf, "%s's character is corrupted", ch->names);
            logmesg(log_buf);
        }

        /* Medics never chase to kill, only to heal. */
        if ( !IS_NPC(ch) || ch->ld_behavior != BEHAVIOR_MEDIC )
            if ( ch->in_room && ch->chasing &&
                 can_see_linear_char(ch, ch->chasing, 0, 0) )
                ch->fighting = ch->chasing;

        if ( VIOLENCE_OKAY(ch, ch->fighting, 0) )
        {
            if ( !ch->interior || !IS_SET(ch->temp_flags, IS_VEHICLE) )
                if ( shoot_char(ch, ch->fighting, get_eq_char(ch, WEAR_WIELD), 0) )
                    if ( ch->fighting )
                        SET_BIT(ch->temp_flags, REPORT);
        }

        if ( VIOLENCE_OKAY(ch, ch->fighting, 1) )
        {
            if ( !ch->interior || !IS_SET(ch->temp_flags, IS_VEHICLE ) ||
                 man_heading(ch, door_dir(ch, ch->fighting), MAN_GUN) )
                if ( shoot_char(ch, ch->fighting, get_eq_char(ch, WEAR_SEC), 0) )
                    if ( ch->fighting )
                        SET_BIT(ch->temp_flags, REPORT);
        }

        if ( !ch->desc && ch->in_room )
            npc_act(ch);
    }

    for ( ch = char_list; ch; ch = ch->next )
        if ( IS_SET(ch->temp_flags, REPORT) )
        {
            REMOVE_BIT(ch->temp_flags, REPORT);

            if ( ch->fighting )
                report_condition(ch, ch->fighting);
        }
}


int
test_char_alive (CHAR_DATA * ch)
{
    return (ch->hit <= 0);
}


int
check_char_alive (CHAR_DATA * ch, const char *with)
{
    if ( test_char_alive(ch) )
    {
        char_death(ch, with);
        return 1;
    }
    else
        return 0;
}


void
char_death (struct char_data *ch, const char *with)
{
    OBJ_DATA *obj, *obj_next;
    CHAR_DATA *killer;
    char *hold;

    DECLARE_DO_FUN(do_gocial);

    if ( !ch || ch->in_extract )
        return;
    else if ( ch == enforcer )
    {
        do_gocial(ch, "power");
        return;
    }


    if ( ch->fighting && ch->fighting->fighting == ch )
        REMOVE_BIT(ch->fighting->temp_flags, REPORT);

    stop_manning(ch);

    if ( !ground0_down && ch->desc )
    {
        if ((obj =
             find_worn_eq_char(ch, SEARCH_GEN_FLAG,
                               GEN_CONDITION_MONITOR)) != NULL)
        {
            act("A helicopter rushes in and transports $n to safety.", ch,
                NULL, NULL, TO_ROOM);
            act("Right before you die, your condition monitor beeps loudly.  A helicopter rushes\n" "in and transports you to safety.", ch, NULL, NULL, TO_CHAR);

            char_from_room(ch);
            char_to_room(ch, safe_area);

            ch->hit = 1;
            extract_obj(obj, obj->extract_me);

            return;
        }
    }

    /*
     * If this is a vehicle and it wasn't hit by anyone, but someone died, we
     * blame the driver of the vehicle (probably because they just drove
     * their tank through a wall a bunch of times).
     */
    if (ch->interior && !ch->last_hit_by &&
        IS_SET(ch->temp_flags, IS_VEHICLE))
        ch->last_hit_by = find_manning(ch->interior, MAN_DRIVE | MAN_HUD);

    killer = (ch->last_hit_by ? ch->last_hit_by : ch);

  /**** Process death event queue for mobiles. ****/
    struct cmd_node *node = ch->npc ? ch->npc->death.head : NULL;

    while ( node != NULL )
    {
        hold = npc_parse(node->cmd, ch, killer, NULL, with);
        if ( !hold )
            continue;
        interpret(ch, hold);
        node = node->next;
    }

  /**** Process kill event queue for mobiles. ****/
    if ( killer != ch && killer->npc )
    {
        struct cmd_node *node = killer->npc->kill.head;

        while ( node != NULL )
        {
            hold = npc_parse(node->cmd, killer, ch, NULL, with);
            logmesg("%s", hold);
            if ( !hold )
                continue;
            interpret(killer, hold);
            node = node->next;
        }
    }

    kill_message(killer, ch, with);
    adjust_kp(killer, ch);

    if (!team_table[killer->team].independent &&
        team_table[killer->team].active && !IS_NPC(ch) &&
        (!IS_NPC(killer) || killer->interior) &&
        killer->team != ch->team && ch->owner != killer)
    {
        team_table[killer->team].score++;

        if ( !IS_NPC(killer) && killer->pcdata )
            killer->pcdata->gs_teamed_kills++;
    }

    check_top_kills(killer);
    check_top_deaths(ch);

    if ( !IS_NPC(killer) && !ground0_down )
        save_char_obj(killer);

    if ( !IS_SET(ch->comm, COMM_NOSKULLS) )
        send_to_char("&YYOU ARE DEAD!&n\n\r"
                     "&X     _.--\"\"\"\"\"\"--._\n\r"
                     "   .'              '.\n\r"
                     "  /                  \\\n\r"
                     " ;                    ;\n\r"
                     " |                    |\n\r"
                     " |                    |\n\r"
                     " ;                    ;\n\r"
                     "  \\ (''--,    ,--'') /\n\r"
                     "   \\ \\  _ )  ( _  / /\n\r"
                     "    ) )(&r'&X)/  \\(&r'&X)( (\n\r"
                     "   (_ '\"\"' /\\ '\"\"' _)\n\r"
                     "    \\'\"-, /  \\ ,-\"'/\n\r"
                     "     '\\ / '\"\"' \\ /'\n\r"
                     "      |/\\/\\/\\/\\/\\|\n\r"
                     "      |\\        /|\n\r"
                     "      ; |/\\/\\/\\| ;\n\r"
                     "       \\'-'--'-'/\n\r"
                     "        \\      /\n\r" "         '.__.'\n\r" "&n\r\n",
                     ch);
    else if ( !number_range(0, 5) )
        send_to_char("You're &Ydead&n, Jim!\r\n", ch);
    else
        send_to_char("You are &YDEAD&n!\r\n", ch);

    act("$n is &YDEAD&n!", ch, NULL, NULL, TO_ROOM);
    REMOVE_BIT(ch->affected_by, AFF_BLIND | AFF_DAZE);

    if ( ch->interior )
    {
        if ( IS_SET(ch->temp_flags, IS_VEHICLE) )
        {
            struct obj_data *veh = revert_vehicle(ch);

            killer = ch->last_hit_by ? ch->last_hit_by
                : find_manning(veh->interior, MAN_DRIVE | MAN_HUD);
            if ( !killer )
                killer = ch;
            claimObject(killer, veh);
            bang_obj(veh, veh->extract_me);
            return;
        }
        else
        {
            struct char_data *ich;

            for ( ich = ch->interior->people; ich; ich = ich->next_in_room )
            {
                ich->last_hit_by = ch->last_hit_by;
                char_death(ich, "tank death");
            }
        }
    }

    for ( obj = ch->carrying; obj != NULL; obj = obj_next )
    {
        obj_next = obj->next_content;

        if ( obj->carried_by != ch )
            logmesg("char_death: carried_by not ch");

        if (!obj->extract_me && obj->wear_loc != WEAR_NONE &&
            (obj->item_type == ITEM_EXPLOSIVE ||
             IS_SET(obj->general_flags, GEN_TIMER_ITEM)))
            pull_obj(obj, ch, 1, "");

        if ( obj->extract_me )
        {
            act("$p drops to the ground and falls apart, useless.", ch,
                obj, NULL, TO_ROOM);
            obj_from_char(obj);
            extract_obj(obj, obj->extract_me);
        }
        else
        {
            act("$p drops to the ground.", ch, obj, NULL, TO_ROOM);
            obj_from_char(obj);
            obj_to_room(obj, ch->in_room);
        }
    }

    if ( !IS_NPC(ch) )
        save_char_obj(ch);

    if ( !IS_NPC(ch) && ch->desc )
    {
        switch (RANK(ch))
        {
            case RANK_UNRANKED:
                printf_to_char(ch, "&WNEWBIE PERK&X: &nFully healed.\r\n");
                ch->hit = ch->max_hit;
                break;
            case RANK_NOVICE:
                printf_to_char(ch, "&WNOVICE PERK&X: &nHalf-healed.\r\n");
                ch->hit = (ch->max_hit / 2);
                break;
            default:
                ch->hit = 1500;
                break;
        }
    }
    ch->armor = 0;

    if ( ch->fighting && ch->fighting->fighting == ch )
        ch->fighting->fighting = NULL;

    if ( !ch->desc || ground0_down )
    {
        ch->in_extract = 1;
        ch->next_extract = extract_list;
        extract_list = ch;

        char_from_room(ch);
        char_to_room(ch, safe_area);
    }
    else
    {
        extract_char(ch, FALSE);
        ch->last_hit_by = NULL;
    }
}
