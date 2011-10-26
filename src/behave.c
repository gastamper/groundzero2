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
 * Module:      behave.c
 * Synopsis:    Mobile behavior stuff.
 * Author:      Satori
 * Created:     21 February 2002
 *
 * NOTES:
 */

#include "ground0.h"

int chase(struct char_data *, struct char_data *);
void move_random(struct char_data *);
int use_kit_person(struct char_data *, struct obj_data *,
                   struct char_data *);
int is_aggro(struct char_data *, struct char_data *);
bool medic_should_heal(struct char_data *, struct char_data *);

DECLARE_DO_FUN (do_use);

char *npc_parse(const char *, struct char_data *, struct char_data *,
                struct obj_data *, const char *);


void
npc_act (struct char_data *ch)
{
    struct char_data *enemies;
    char *hold;
    int i;

    if ( ch->move_delay && (iteration % (ch->move_delay + 1)) )
        return;

  /**** Process their heartbeat events. ****/
    struct cmd_node *node = ch->npc ? ch->npc->heartbeat.head : NULL;

    while ( node != NULL )
    {
        hold = npc_parse(node->cmd, ch, NULL, NULL, NULL);
        if ( !hold )
            continue;
        interpret(ch, hold);
        node = node->next;
    }

    switch (ch->ld_behavior)
    {

        case BEHAVIOR_LD:
        case BEHAVIOR_PLAYER_CONTROL:
            return;

        case BEHAVIOR_PILLBOX:
        case BEHAVIOR_GUARD:
            if (!ch->fighting ||
                !can_see_linear_room(ch, ch->fighting->in_room))
                for (enemies = ch->in_room->people; enemies;
                     enemies = enemies->next_in_room)
                    if ( (enemies != ch) && is_aggro(ch, enemies) )
                    {
                        ch->fighting = enemies;
                        break;
                    }
            return;

        case BEHAVIOR_SEEKING_PILLBOX:
            if ( !ch->chasing )
                return;
            chase(ch, ch->chasing);
            return;

        case BEHAVIOR_MEDIC:
            if ( ch->hit <= ch->max_hit - 2000 )
                do_use(ch, "kit");

            if ( ch->chasing &&
                 ch->in_room->level == ch->chasing->in_room->level &&
                 ch->chasing->hit < ch->chasing->max_hit &&
                 (ABS(ch->in_room->x - ch->chasing->in_room->x) <= 8) &&
                 (ABS(ch->in_room->y - ch->chasing->in_room->y) <= 8) &&
                 medic_should_heal(ch, ch->chasing) &&
                 (i = chase(ch, ch->chasing)) != -1 )
            {
                /* The chaser or chasee can die from a mine in chase(). */
                if ( !ch->chasing )
                    return;

                if ( i == 1 ) /* chase() returned BFS_NO_MOVE. */
                {
                    OBJ_DATA *obj = get_obj_carry(ch, "kit");

                    if ( !obj )
                    {
                        logmesg("Medic without a kit");
                        return;
                    }

                    use_kit_person(ch, obj, ch->chasing);
                }

                return;
            }

            ch->chasing = NULL;

            for ( enemies = char_list; enemies; enemies = enemies->next )
            {
                if ( ch == enemies )
                    continue;

                if ( (ch->in_room->level == enemies->in_room->level) &&
                     (ABS(ch->in_room->x - enemies->in_room->x) <= 8) &&
                     (ABS(ch->in_room->y - enemies->in_room->y) <= 8) &&
                     medic_should_heal(ch, enemies) )
                {
                    ch->chasing = enemies;
                    return;
                }
            }

            /* Might as well. */
            move_random(ch);
            break;

        case BEHAVIOR_SEEK:
            if ( !ch->fighting || chase(ch, ch->fighting) == -1 )
            {
                for ( enemies = char_list; enemies; enemies = enemies->next )
                    if ( (ch->in_room->level == enemies->in_room->level) &&
                         (ABS(ch->in_room->x - enemies->in_room->x) <= 8) &&
                         (ABS(ch->in_room->y - enemies->in_room->y) <= 8) &&
                         !IS_NPC(enemies) && is_aggro(ch, enemies) )
                        ch->fighting = enemies;

                if ( !ch->fighting )
                    move_random(ch);

                return;
            }
            break;

        case BEHAVIOR_WANDER:
        case BEHAVIOR_BOOMER:   /* Boomers wander. */
            if ( !ch->fighting || chase(ch, ch->fighting) == -1 )
            {
                move_random(ch);
                return;
            }
            break;

        case BEHAVIOR_TOSSBACK:
            move_random(ch);
            break;

        case BEHAVIOR_SEEKING_BOOMER:  /*taken from above code  -Tireg*/
            if ( ch->chasing &&
                 ch->in_room->level == ch->chasing->in_room->level &&
                 ch->chasing->hit < ch->chasing->max_hit &&
                 (ABS(ch->in_room->x - ch->chasing->in_room->x) <= 8) &&
                 (ABS(ch->in_room->y - ch->chasing->in_room->y) <= 8) &&
                 (i = chase(ch, ch->chasing)) != -1 )
            {
                if ( !ch->chasing )
                    return;
            
                if ( i == 1 ) /* chase() returned BFS_NO_MOVE. */
                {
                    do_pull(ch, "all");
                }
            return;
            }
            break;
    }
}


#if 0
void
chase (struct char_data *ch, struct char_data *vict)
{
    int dir[2];

    dir[0] = (ch->in_room->x > vict->in_room->x ? DIR_WEST : DIR_EAST);
    dir[1] = (ch->in_room->y > vict->in_room->y ? DIR_SOUTH : DIR_NORTH);

    int x_delta = ABS(ch->in_room->x - vict->in_room->x);
    int y_delta = ABS(ch->in_room->y - vict->in_room->y);
    int head = (y_delta > x_delta);

    if ( !number_range(0, 3) || ch->in_room->exit[dir[head]] )
        head = !head;

    move_char(ch, dir[(head ? 1 : 0)], FALSE, MWALL_NOMINAL);
}
#endif


void
move_random (struct char_data *ch)
{
    if ( !number_range(0, 19) )
        return;

    int tries = 0, dir;

    do
        dir = number_range(DIR_NORTH, DIR_WEST);
    while ( ch->in_room->exit[dir] && tries++ < 50 );
    move_char(ch, dir, FALSE, MWALL_NOMINAL);
}


char *
npc_parse (const char *fmt, struct char_data *me, struct char_data *him,
          struct obj_data *obj, const char *text)
{
    static char npcbuf[MAX_STRING_LENGTH];
    char i[MAX_STRING_LENGTH];
    char *write_pt = npcbuf;
    char *iter_i;

    for ( ; *fmt; fmt++ )
    {
        if ( *fmt == '@' )
        {
            switch (*(++fmt))
            {
                case 'n':
                    if ( !me || !me->names )
                        return NULL;
                    one_argument(me->names, i);
                    break;
                case 'N':
                    if ( !him || !him->names )
                        return NULL;
                    one_argument(him->names, i);
                    break;
                case 't':
                    if ( !text )
                        return NULL;
                    strcpy(i, text);
                    break;
                case 'o':
                    if ( !obj || !obj->name )
                        return NULL;
                    one_argument(obj->name, i);
                    break;
                case '@':
                    i[0] = '@';
                    i[1] = '\0';
                    break;
                default:
                    i[0] = '@';
                    i[1] = *fmt;
                    i[2] = '\0';
                    break;
            }

            for ( iter_i = i; *iter_i; iter_i++ )
                *(write_pt++) = *iter_i;
        }
        else
            *(write_pt++) = *fmt;
    }

    *write_pt = '\0';
    return npcbuf;
}


bool
medic_should_heal (struct char_data * ch, struct char_data * targ)
{
    if ( !IS_NPC(ch) || ch->ld_behavior != BEHAVIOR_MEDIC )
        return (0);
    if ( IS_NPC(targ) && targ->ld_behavior == BEHAVIOR_MEDIC )
        return (0);
    if ( ch->team != targ->team )
        return (0);
    if ( targ->hit >= targ->max_hit - 1000 )
        return (0);

    return (1);
}
