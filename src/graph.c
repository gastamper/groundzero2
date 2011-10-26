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
 * Module:      graph.c
 * Synopsis:    Graph-based algorithms for map.
 * Author:      Satori
 * Created:     1 May 2002
 *
 * NOTES:
 */

#include "ground0.h"

struct BFS_Queue
{
    struct room_data *room;
    int from_dir;
    struct BFS_Queue *next;
};


static struct BFS_Queue *bfs_head = NULL;
static struct BFS_Queue *bfs_tail = NULL;
static struct BFS_Queue *bfs_recycle = NULL;


static void
BFS_Enqueue ( struct room_data *room, int dir )
{
    struct BFS_Queue *bq;

    if ( bfs_recycle != NULL )
    {
        bq = bfs_recycle;
        bfs_recycle = bq->next;
    }
    else
    {
        // Allocate new one, permanently.
        bq = alloc_perm(sizeof(struct BFS_Queue));
    }

    bq->room = room;
    bq->from_dir = dir;
    bq->next = NULL;

    // Enqueue:
    if ( bfs_head == NULL )
    {
        // The first element is the head and the tail (obviously).
        bfs_head = bfs_tail = bq;
    }
    else
    {
        // Link old tail to bq, then make the new tail bq.
        bfs_tail->next = bq;
        bfs_tail = bq;
    }
}


static void
BFS_Dequeue ( void )
{
    struct BFS_Queue *bq = bfs_head;

    if ( bq == NULL )
        return;

    // Remove from old list, add to recycle list.
    bfs_head = bfs_head->next;
    bq->next = bfs_recycle;
    bfs_recycle = bq;
}


static inline struct room_data *
VALID_WALK ( struct room_data *rm, int dir )
{
    struct room_data *ret;

    if ( rm->exit[dir] )
        return NULL;
    if ( (ret = get_to_room(rm, dir)) == NULL )
        return NULL;
    if ( ret->visited )
        return NULL;

    return ret;
}


#define BFS_NO_MOVE     -1
#define BFS_NO_PATH     -2


int
CorrectStep ( struct room_data *from, struct room_data *to )
{
    struct level_data *lvl = from->this_level;
    struct room_data *base = lvl->rooms_on_level;
    int dir;

    if ( from == to )
        return BFS_NO_MOVE;

    for ( int x = 0; x < lvl->x_size; x++ )
        for ( int y = 0; y < lvl->y_size; y++ )
            index_room(base, x, y)->visited = false;

    from->visited = true;

    for ( dir = 0; dir < DIR_UP; dir++ )
        if ( (base = VALID_WALK(from, dir)) )
        {
            base->visited = true;
            BFS_Enqueue(base, dir);
        }

    while ( bfs_head )
    {
        if ( bfs_head->room == to )
        {
            dir = bfs_head->from_dir;
            while ( bfs_head ) BFS_Dequeue();
            return dir;
        }

        for ( dir = 0; dir < DIR_UP; dir++ )
            if ( (base = VALID_WALK(bfs_head->room, dir)) )
            {
                base->visited = true;
                BFS_Enqueue(base, bfs_head->from_dir);
            }

        BFS_Dequeue();
    }

    return BFS_NO_PATH;
}


int
chase ( struct char_data *ch, struct char_data *vict )
{
    int dir;

    if ( ch->in_room->this_level != vict->in_room->this_level )
        return -1;

    dir = CorrectStep(ch->in_room, vict->in_room);

    if ( dir == BFS_NO_MOVE )
        return 1;
    else if ( dir == BFS_NO_PATH )
        return -1;

    if ( ch->in_room->exit[dir] )
    {
        // Fall back to next best behavior:
        int adir[2];

        adir[0] = (ch->in_room->x > vict->in_room->x ? DIR_WEST : DIR_EAST);
        adir[1] = (ch->in_room->y > vict->in_room->y ? DIR_SOUTH : DIR_NORTH);

        int x_delta = ABS(ch->in_room->x - vict->in_room->x);
        int y_delta = ABS(ch->in_room->y - vict->in_room->y);
        int head = (y_delta > x_delta);

        if ( !number_range(0, 3) || ch->in_room->exit[adir[head]] )
            head = !head;

        dir = adir[(head ? 1 : 0)];
    }

    move_char(ch, dir, FALSE, MWALL_NOMINAL);
    return 0;
}
