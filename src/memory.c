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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "ground0.h"
#include "db.h"
#include "memory.h"

/* globals */
struct allocations_type *perm_alloc_list = NULL, *temp_alloc_list = NULL;
CHAR_DATA *char_free;
NOTE_DATA *note_free;
OBJ_DATA *obj_free;
PC_DATA *pcdata_free;

OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
ROOM_DATA *room_index_hash[MAX_KEY_HASH];
char *string_hash[MAX_KEY_HASH];

char string_space[MAX_STRING];
char *top_string;
char str_empty[1];

int top_area;
int top_help;
int top_mob_index;
int top_obj_index;
int top_reset;
int top_room;
int top_shop;

void *rgFreeList[MAX_MEM_LIST];
const int rgSizeList[MAX_MEM_LIST] = {
    16, 32, 64, 128, 256, 1024, 2048, 4096, 8192, 16384, 32768, 65536,
    131072, 262144, 524288, 1048576
};

int nAllocString;
int sAllocString;
int nAllocPerm;
int sAllocPerm;

/* prototypes */
void get_string_space();

void
get_string_space ()
{
    top_string = string_space;
}


void
FlushNpcEvent (struct cmd_queue *q)
{
    struct cmd_node *tmp;

    while ( (tmp = q->head) != NULL )
    {
        q->head = tmp->next;
        free_string(((char *) tmp->cmd));
        free(tmp);
    }

    q->head = q->tail = NULL;
}


/*
 * Free a character.
 */
void
free_char (CHAR_DATA * ch)
{
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    for ( obj = ch->carrying; obj != NULL; obj = obj_next )
    {
        obj_next = obj->next_content;
        extract_obj(obj, obj->extract_me);
    }

    free_string(ch->names);
    free_string(ch->where_start);
    free_string(ch->short_descript);
    ch->names = ch->where_start = ch->short_descript = NULL;

    if ( ch->pcdata != NULL )
    {
        PC_DATA *p = ch->pcdata;

        free_string(p->password);
        free_string(p->poofin_msg);
        free_string(p->poofout_msg);
        free_string(p->title_line);
        p->password = p->poofin_msg = p->poofout_msg = p->title_line;
        p->next = pcdata_free;
        pcdata_free = p;
    }

    if ( ch->npc )
    {
        FlushNpcEvent(&ch->npc->heartbeat);
        FlushNpcEvent(&ch->npc->depress);
        FlushNpcEvent(&ch->npc->enforcer);
        FlushNpcEvent(&ch->npc->death);
        FlushNpcEvent(&ch->npc->kill);
        free(ch->npc);
    }

    ch->next = char_free;
    char_free = ch;
    return;
}

void
mark_alloc (void *ptr, int size, struct allocations_type **first_alloc,
           char *filename, int line_num)
{
    struct allocations_type *creator;

    creator = malloc(sizeof(struct allocations_type));

    creator->next = NULL;
    creator->ptr = ptr;
    creator->size = size;
    creator->filename = filename;
    creator->line_num = line_num;
    if ( !*first_alloc )
        *first_alloc = creator;
    else
    {
        creator->next = *first_alloc;
        *first_alloc = creator;
    }
}

void
mark_free (void *ptr, struct allocations_type **first_alloc, char *filename,
          int line_num)
{
    struct allocations_type *tracker, *previous;

    previous = *first_alloc;
    for ( tracker = *first_alloc; tracker; tracker = tracker->next )
    {
        if ( tracker->ptr == ptr )
            break;
        previous = tracker;
    }
    if ( !tracker )
    {
        sprintf(log_buf,
                "BUG: attempt to free memory that is already free in "
                "%s line %d.", filename, line_num);
        logmesg(log_buf);
        exit(STATUS_ERROR);
    }
    else
    {
        if ( tracker == *first_alloc )
            *first_alloc = (*first_alloc)->next;
        else
            previous->next = tracker->next;
        free(tracker);
    }
}

struct allocations_type *
get_mem_reference (void *ptr, struct allocations_type **first_alloc)
{
    struct allocations_type *tracker;

    for ( tracker = *first_alloc; tracker; tracker = tracker->next )
        if ( tracker->ptr == ptr )
            return tracker;
    return NULL;
}

int
size_correct (void *ptr, int size, struct allocations_type **first_alloc)
{
    struct allocations_type *tracker;

    if ( (tracker = get_mem_reference(ptr, first_alloc)) != NULL )
        if ( tracker->size == size )
            return 1;
    return 0;
}

/*
 * Allocate some ordinary memory,
 *   with the expectation of freeing it someday.
 */
void *
f_alloc_mem (int sMem, char *filename, int line_num)
{
    void *pMem;
    int iList;

    for ( iList = 0; iList < MAX_MEM_LIST; iList++ )
    {
        if ( sMem <= rgSizeList[iList] )
            break;
    }

    if ( iList == MAX_MEM_LIST )
    {
        sprintf(log_buf, "Alloc_mem: size %d too large in %s %d.", sMem,
                filename, line_num);
        logmesg(log_buf);
        exit(STATUS_ERROR);
    }

    if ( rgFreeList[iList] == NULL )
    {
        pMem = alloc_perm(rgSizeList[iList]);
    }
    else
    {
        pMem = rgFreeList[iList];
        rgFreeList[iList] = *((void **) rgFreeList[iList]);
    }

#if 0
    {
        char *count;

        for ( count = pMem; count < pMem + sMem; count++ )
            *count = 'Z';
    }
#endif
    mark_alloc(pMem, rgSizeList[iList], &temp_alloc_list, filename,
               line_num);
    return pMem;
}



/*
 * Free some memory.
 * Recycle it back onto the free list for blocks of that size.
 */
void
f_free_mem (void *pMem, int sMem, char *filename, int line_num)
{
    int iList;

    for ( iList = 0; iList < MAX_MEM_LIST; iList++ )
    {
        if ( sMem <= rgSizeList[iList] )
            break;
    }

    if ( iList == MAX_MEM_LIST )
    {
        logmesg("Free_mem: size %d too large.", sMem);
        exit(STATUS_ERROR);
    }

    {
        char *count;
        char *endp = (char *) pMem + sMem;

        for ( count = (char *) pMem; count < endp; count++ )
            *count = 'X';
    }

    if ( size_correct(pMem, rgSizeList[iList], &temp_alloc_list) )
    {
        *((void **) pMem) = rgFreeList[iList];
        rgFreeList[iList] = pMem;
        mark_free(pMem, &temp_alloc_list, filename, line_num);
    }
    else
    {
        sprintf(log_buf, "Attempt to free memory of incorrect size.\n"
                "Allocation occurred in %s line %d.\nAttempted freeing in "
                "%s line %d.",
                (get_mem_reference(pMem, &temp_alloc_list))->filename,
                (get_mem_reference(pMem, &temp_alloc_list))->line_num,
                filename, line_num);
        logmesg(log_buf);
        *((char *) NULL) = 5;
        exit(STATUS_ERROR);
    }

    return;
}



/*
 * Allocate some permanent memory.
 * Permanent memory is never freed,
 *   pointers into it may be copied safely.
 */
void *
f_alloc_perm (int sMem, char *filename, int line_num)
{
    static char *pMemPerm;
    static int iMemPerm;
    void *pMem;

    while ( sMem % sizeof(long) != 0 )
        sMem++;

    if ( sMem > MAX_PERM_BLOCK )
    {
        logmesg("Alloc_perm: %d too large.", sMem);
        exit(STATUS_ERROR);
    }

    if ( pMemPerm == NULL || iMemPerm + sMem > MAX_PERM_BLOCK )
    {
        logmesg("allocating new block");
        iMemPerm = 0;
        if ( (pMemPerm = calloc(1, MAX_PERM_BLOCK)) == NULL )
        {
            logerr("Alloc_perm");
            exit(STATUS_ERROR);
        }
    }

    pMem = pMemPerm + iMemPerm;
    iMemPerm += sMem;
    nAllocPerm += 1;
    sAllocPerm += sMem;
    mark_alloc(pMem, sMem, &perm_alloc_list, filename, line_num);
    return pMem;
}



/*
 * Duplicate a string into dynamic memory.
 * Fread_strings are read-only and shared.
 */
char *
f_str_dup (const char *str, char *filename, int line_num)
{
    char *str_new;

    if ( str[0] == '\0' )
        return &str_empty[0];

    if ( str >= string_space && str < top_string )
        return (char *) str;

    str_new = alloc_mem(strlen(str) + 1);
    strcpy(str_new, str);
    return str_new;
}


/*
 * Free a string.
 * Null is legal here to simplify callers.
 * Read-only shared strings are not touched.
 */
void
f_free_string (char *pstr, char *filename, int line_num)
{
    if (pstr == NULL || pstr == &str_empty[0]
        || (pstr >= string_space && pstr < top_string))
        return;

    free_mem(pstr, strlen(pstr) + 1);
    return;
}


void
do_memory (CHAR_DATA * ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "Helps   %5d\n\r", top_help);
    send_to_char(buf, ch);
    sprintf(buf, "Socials %5d\n\r", maxSocial);
    send_to_char(buf, ch);
    sprintf(buf, "Objs    %5d\n\r", top_obj_index);
    send_to_char(buf, ch);
    sprintf(buf, "String space is %f%% filled.\n\r",
            (((float) (top_string - string_space)) / (float) MAX_STRING) *
            100);
    send_to_char(buf, ch);

    return;
}

void
do_dump (CHAR_DATA * ch, char *argument)
{
    int count, count2, num_pcs, aff_count;
    PC_DATA *pc;
    OBJ_DATA *obj;
    OBJ_INDEX_DATA *pObjIndex;
    DESCRIPTOR_DATA *d;
    FILE *fp;
    int vnum, nMatch = 0;
    char *printer;

    /* open file */
    fp = fopen("mem.dmp", "w");

    /* report use of data structures */

    num_pcs = 0;
    aff_count = 0;

    /* pcdata */
    count = 0;
    for ( pc = pcdata_free; pc != NULL; pc = pc->next )
        count++;

    fprintf(fp, "Pcdata  %4d (%8d bytes), %2d free (%d bytes)\n",
            num_pcs, num_pcs * ((int)sizeof(*pc)), count,
            count * ((int)sizeof(*pc)));

    /* descriptors */
    count = 0;
    count2 = 0;
    for ( d = descriptor_list; d != NULL; d = d->next )
        count++;
    for ( d = descriptor_free; d != NULL; d = d->next )
        count2++;

    fprintf(fp, "Descs  %4d (%8d bytes), %2d free (%d bytes)\n",
            count, count * ((int)sizeof(*d)), count2, count2 * ((int)sizeof(*d)));

    /* object prototypes */
    for ( vnum = 0; vnum < top_obj_index; vnum++ )
        if ( (pObjIndex = get_obj_index(vnum)) != NULL )
        {
            nMatch++;
        }

    fprintf(fp, "ObjProt %4d (%8d bytes)\n", top_obj_index,
            top_obj_index * ((int)sizeof(*pObjIndex)));


    /* objects */
    count = 0;
    count2 = 0;
    for ( obj = object_list; obj != NULL; obj = obj->next )
    {
        count++;
    }
    for ( obj = obj_free; obj != NULL; obj = obj->next )
        count2++;

    fprintf(fp, "Objs    %4d (%8d bytes), %2d free (%d bytes)\n",
            count, count * ((int)sizeof(*obj)), count2,
            count2 * ((int)sizeof(*obj)));
    fprintf(fp, "\n\n\nAll strings:\n\n");
    for ( printer = string_space; printer < top_string; printer++ )
        fprintf(fp, "%c", printer[0]);

    fclose(fp);


    /* start printing out object data */
    fp = fopen("obj.dmp", "w");

    fprintf(fp, "\nObject Analysis\n");
    fprintf(fp, "---------------\n");
    nMatch = 0;
    for ( vnum = 0; vnum < top_obj_index; vnum++ )
        if ( (pObjIndex = get_obj_index(vnum)) != NULL )
        {
            nMatch++;
            fprintf(fp, "#%-4d %3d active      %s\n",
                    pObjIndex->vnum, pObjIndex->count,
                    pObjIndex->short_descr);
        }

    /* close file */
    fclose(fp);
}
