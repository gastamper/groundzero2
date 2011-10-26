/* files used in db.c */

/* vals from db.c */
extern bool fBootDb;
extern int newmobs;
extern int newobjs;
extern OBJ_INDEX_DATA *obj_index_hash[MAX_KEY_HASH];
extern int top_mob_index;
extern int top_obj_index;
extern int top_affect;
extern int top_ed;


/* from db2.c */
/*extern int    social_count; */
int maxSocial;


struct cmd_synopses_data
{
    char *name;
    char *description;
};
