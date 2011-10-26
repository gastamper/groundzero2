/* GroundZero2 is based upon code from GroundZero by Corey Hilke, et al.
 * GroundZero was based upon code from ROM by Russ Taylor, et al.
 * ROM was based upon code from Merc by Hatchett, Furey, and Kahn.
 * Merc was based upon code from DIKUMUD.
 *
 * DikuMUD was written by Katja Nyboe, Tom Madsen, Hans Henrik Staerfeldt,
 * Michael Seifert, and Sebastian Hammer.
 */

/**
 * NAME:        reboot.c
 * SYNOPSIS:    Natural progression into the next round.
 * AUTHOR:      satori
 * CREATED:     04 Sep 2001
 * MODIFIED:    04 Sep 2001
 * 
 * This is totally temporary hot-copy bullshit. The proper way to handle
 * this is to not reboot at all -- just reset the world.
 */

#include "ground0.h"

#define RECONNECT_FILE    "../tmp/reboot.dat"
DECLARE_DO_FUN (do_help);

struct reboot_header_data
{
    int control;
};

struct reconnect_data
{
    int fd;
    char name[64];
    char godname[64];
    char host[80];
    int ttype;
    int lines;
    int columns;
    int mccp;
};


bool write_to_descriptor(struct descriptor_data *, char *, int, bool);
DESCRIPTOR_DATA *alloc_descriptor(void);
GOD_TYPE *get_god(const char *);
void show_greetings(struct descriptor_data *);


extern int g_control_socket;


int
write_connections (void)
{
    struct descriptor_data *d_next;
    struct reboot_header_data rhd;
    struct descriptor_data *d;
    struct reconnect_data rd;
    struct char_data *ch;
    int count = 0;
    FILE *fp;

    bool end_compression(struct descriptor_data *);

    if ( !(fp = fopen(RECONNECT_FILE, "wb")) )
    {
        logerr(RECONNECT_FILE);
        return (-1);
    }

    for ( d = descriptor_list; d; d = d->next )
        if ( d->connected == CON_PLAYING && (ch = d->character) &&
             ch->desc && ch->names && strlen(ch->names) < 64 &&
             *ch->names )
            count++;

    if ( !count )
        return (0);

    /* Write header. */
    rhd.control = g_control_socket;
    fwrite((const char *) &rhd, sizeof(rhd), 1, fp);

    for ( d = descriptor_list; d; d = d_next )
    {
        d_next = d->next;

        if ( d->connected != CON_PLAYING || !(ch = d->character ) ||
            !ch->desc)
        {
            write_to_descriptor(d,
                                "We're rebooting.  Sorry, come back later.\r\n",
                                0, FALSE);
            close_socket(d);
            continue;
        }
        else if ( !ch->names || strlen(ch->names) > 63 || !*ch->names )
        {
            logmesg("Skipped reboot record for %s: bad name.",
                       ch->names);
            continue;
        }

        rd.fd = d->descriptor;
        strcpy(rd.name, ch->names);

        if ( d->adm_data && *d->adm_data->rl_name )
            strcpy(rd.godname, d->adm_data->rl_name);
        else
            *rd.godname = '\0';

        strcpy(rd.host, d->host);
        rd.ttype = d->ttype;
        rd.lines = d->lines;
        rd.columns = d->columns;
        rd.mccp = (d->outcom_stream != NULL);
        fwrite((const char *) &rd, sizeof(rd), 1, fp);

        if ( rd.mccp )
            end_compression(d);
    }

    fclose(fp);
    return (1);
}


int
read_connections (void)
{
    struct reboot_header_data rhd;
    struct descriptor_data *dnew;
    struct reconnect_data *rds;
    long fbytes = 0;
    int reconnects;
    ssize_t bytes;
    FILE *fp;

    bool begin_compression(struct descriptor_data *);

    if ( !(fp = fopen(RECONNECT_FILE, "rb")) )
    {
        logerr(RECONNECT_FILE);
        return (-1);
    }

    fseek(fp, 0L, SEEK_END);
    fbytes = ftell(fp) - sizeof(rhd);
    rewind(fp);

    if ( (fbytes % sizeof(struct reconnect_data)) != 0 )
    {
        logmesg
            ("Corrupt reboot file: %d bytes is not a multiple of record size.",
             fbytes);
        fclose(fp);
        return (-1);
    }

    if (!fread((char *) &rhd, sizeof(rhd), 1, fp)) {
        logmesg ("Error: Could not load header data of reboot file!");
        fclose(fp);
        return (-1);
    };
    
    reconnects = (fbytes / sizeof(struct reconnect_data));
    rds = calloc(reconnects, sizeof(struct reconnect_data));    
    
    if (!fread((char *) rds, sizeof(struct reconnect_data), reconnects, fp)) {
        logmesg ("Error: Could not load reconnect data from reboot file!");
        fclose(fp);
        return (-1);
    };
    
    fclose(fp);

    unlink(RECONNECT_FILE);
    g_control_socket = rhd.control;

    while ( reconnects-- )
    {
        while ( (bytes = write(rds[reconnects].fd, "\r\n", 2)) == -1 )
        {
            if ( errno == EINTR )
                continue;
            logmesg("Lost connection to %s while rebooting.",
                       rds[reconnects].host);
            break;
        }

        if ( bytes == -1 )
        {
            close(rds[reconnects].fd);
            continue;
        }

        dnew = alloc_descriptor();

        dnew->ttype = rds[reconnects].ttype;
        dnew->lines = rds[reconnects].lines;
        dnew->columns = rds[reconnects].columns;
        dnew->descriptor = rds[reconnects].fd;
        strcpy(dnew->host, rds[reconnects].host);

        if ( rds[reconnects].mccp )
            begin_compression(dnew);

        dnew->next = descriptor_list;
        descriptor_list = dnew;

        if ( *rds[reconnects].godname )
            dnew->adm_data = get_god(rds[reconnects].godname);

        logmesg("Reloading character %s...", rds[reconnects].name);
        dnew->character = load_char_obj(dnew, rds[reconnects].name);

        if ( !dnew->character )
        {
            logmesg("Could not reload character '%s' for relogin.",
                       rds[reconnects].name);
            write_to_descriptor(dnew,
                                "Could not automate relogin process.  Try it manually, if you want.\r\n",
                                0, FALSE);
            close_socket(dnew);
            continue;
        }

        dnew->character->trust = 0;

        if ( dnew->adm_data )
        {
            logmesg("Trusting the admin.");
            dnew->character->trust = dnew->adm_data->trust;
            do_help(dnew->character, "imotd");
            page_to_char("Press &WENTER&n to continue&X: &n",
                         dnew->character);
            show_string(dnew, "");
            dnew->connected = CON_READ_IMOTD;
        }
        else
        {
            send_to_char("&nWelcome back, soldier.\r\n"
                         "&WPlay again&X? [&RY&X/&rn&X]&n ",
                         dnew->character);
            dnew->connected = CON_RELOG;
        }
    }

    return (0);
}


int
soft_reboot (void)
{
    extern char **g_cmd_args;

    if ( write_connections() < 1 )
        return (-1);

    execv(g_cmd_args[0], g_cmd_args);
    logmesg("Uhhmmmm...");
    return (-1);                /* Failure if we reach here. */
}


/* Soft reboot predicate. */
bool
p_soft_reboot (void)
{
    return (access(RECONNECT_FILE, F_OK) == 0);
}


/* Oh, totally crappy */
void
handle_log_redir (const char *dir)
{
    char fname[256];
    int num = 0;

    while ( num <= 9999 )
    {
        sprintf(fname, "%s/%04d.log", dir, num++);
        if ( access(fname, F_OK) )
            break;
    }

    if ( num > 9999 )
    {
        if (system("../automaint -l") == -1) {
            logmesg("handle_log_redir: Failed to run automaint script!");
        }
        sprintf(fname, "%s/%04d.log", dir, (num = 0));
    }

    if ( (stderr = freopen(fname, "w", stderr)) != NULL )
    {
        logmesg("STDERR: Opened '%s' for logging.", fname);
        return;
    }

    logerr(fname);
    logmesg("Could not open logging file; redirecting to stderr.");

    if ( (stderr = fdopen(STDERR_FILENO, "w")) != NULL )
    {
        logmesg("STDERR: Opened default stderr fd for logging.");
        return;
    }

    logerr("stderr");
    logmesg
        ("Could not redirect logging to stderr; redirecting to /dev/null.");

    if ( (stderr = fopen("/dev/null", "w")) != NULL )
    {
        logmesg("STDERR: Opened /dev/null for logging.");
        return;
    }

    logerr("/dev/null");
    logmesg
        ("Could not redirect logging to /dev/null; redirecting to stdout.");
    stderr = stdout;
    logmesg("STDERR: Opened stdout for logging.");
}


void
cleanup_log_redir (void)
{
    fflush(stderr);
    fclose(stderr);
}
