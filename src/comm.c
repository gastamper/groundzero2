
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
 *  Thanks to abaddon for proof-reading our comm.c and pointing out bugs.  *
 *  Any remaining bugs are, of course, our work, not his.  :)              *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/*
 * This file contains all of the OS-dependent stuff:
 *   startup, signals, BSD sockets for tcp/ip, i/o, timing.
 *
 * The data flow for input is:
 *    Game_loop ---> Read_from_descriptor ---> Read
 *    Game_loop ---> Read_from_buffer
 *
 * The data flow for output is:
 *    Game_loop ---> Process_Output ---> Write_to_descriptor -> Write
 *
 * The OS-dependent functions are Read_from_descriptor and Write_to_descriptor.
 * -- Furey  26 Jan 1993
 */

#include    "ground0.h"
#include    "mccp.h"
#include    "telnet.h"
#include    "ban.h"

#include    <sys/wait.h>
#include    <sys/stat.h>
#include    <arpa/inet.h>


extern char *color_table[];

/* command procedures needed */
DECLARE_DO_FUN (do_help);
DECLARE_DO_FUN (do_look);
DECLARE_DO_FUN (do_skills);
DECLARE_DO_FUN (do_outfit);
DECLARE_DO_FUN (do_save);
DECLARE_DO_FUN (do_immtalk);
DECLARE_DO_FUN (do_note);
DECLARE_DO_FUN (do_palette);
void system_info(struct descriptor_data *);

void check_multiplayer(const char *);
void recalc_num_on();

GOD_TYPE *get_god(char *a_name);
ushort port = 4000;
extern char *scenario_name;
extern int enforcer_depress;

unsigned long g_bytes_recv = 0;
unsigned long g_bytes_sent = 0;

/*
 * Malloc debugging stuff.
 */
#if defined(sun)
#undef MALLOC_DEBUG
#endif

#if defined(MALLOC_DEBUG)
#include <malloc.h>
extern int malloc_debug args((int));
extern int malloc_verify args((void));
#endif


/* dns.c */
int ResolvInitialize();
int ResolvQuery(struct descriptor_data *, struct in_addr);
int ResolvCancel(struct descriptor_data *);
void ResolvProcess();
void ResolvTerminate();



/*
 * Signal handling.
 * Apollo has a problem with __attribute(atomic) in signal.h,
 *   I dance around it.
 */
#if defined(apollo)
#define __attribute(x)
#endif

#include <signal.h>

#if defined(apollo)
#undef __attribute
#endif

/* Socket and TCP/IP stuff. */

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


/*
 * Global variables.
 */
DESCRIPTOR_DATA *descriptor_free;       /* Free list for descriptors      */
DESCRIPTOR_DATA *descriptor_list;       /* All open descriptors           */
DESCRIPTOR_DATA *d_next;        /* Next descriptor in loop        */
bool god;                       /* All new chars are gods!        */
bool wizlock;                   /* Game is wizlocked              */
bool newlock;                   /* Game is newlocked              */

bool g_coldboot = FALSE;        /* Never soft boot.               */
int g_control_socket = -1;      /* Messy global for soft reboot.  */
char **g_cmd_args = NULL;       /* ... another ...                */

time_t start_time;
char str_boot_time[MAX_INPUT_LENGTH];
time_t current_time;            /* time of this pulse */

ROOM_DATA *ammo_repop[3];
ROOM_DATA *deploy_area;
ROOM_DATA *safe_area;
ROOM_DATA *satori_area;
ROOM_DATA *god_general_area, *explosive_area;
ROOM_DATA *store_area;
ROOM_DATA *graveyard_area;
int iteration = 0;
int ground0_down;               /* Shutdown + return value      */
char scenario_dir[256];

/********************/
/*- Local variables */
/********************/

static bool g_never_warmboot = FALSE;
static char logdir[128];

extern const char go_ahead_str[];

void game_loop_unix(int control);
int init_socket(int port);
void new_descriptor(int control);
bool read_from_descriptor(DESCRIPTOR_DATA * d);
bool write_to_descriptor(struct descriptor_data *d, char *txt, int length,
                         bool proc);
bool check_parse_name(char *name);
bool check_reconnect(DESCRIPTOR_DATA * d, char *name, bool fConn);
bool check_playing(DESCRIPTOR_DATA * d, char *name);
void nanny(DESCRIPTOR_DATA * d, char *argument);
bool process_output(DESCRIPTOR_DATA * d, bool fPrompt);
void read_from_buffer(DESCRIPTOR_DATA * d);
void initialise_status(struct char_data *ch);
void deinitialise_status(struct char_data *ch);
int net_nonblock(int);

/* reboot.c */
int write_connections(void);
int read_connections(void);
int soft_reboot(void);
bool p_soft_reboot(void);
void handle_log_redir(const char *);
void cleanup_log_redir(void);


void
argument_error ()
{
    fprintf(stderr,
            "Syntax: ground0 <arguments>\n"
	    "-p <port>      Specify the port to listen on\n"
	    "-l <logdir>    Specify an alternate log directory.\n"
 	    "-s <scenario>  Select which scenario to load.\n"
	    "-c             Cold boot.\n"
	    "-C             Cold boot and never warm boot.\n");
    exit(STATUS_BOOT_ERROR);
}


void
parse_arguments (int argc, char **argv)
{
    int count;

    /* Store command line arguments for soft reboots. */
    g_cmd_args = calloc(argc + 1, sizeof(char *));

    for ( count = 0; count < argc; count++ )
        g_cmd_args[count] = argv[count];

    g_cmd_args[argc] = NULL;
    *scenario_dir = '\0';

    for ( count = 1; count < argc; count++ )
    {
        if ( argv[count][0] != '-' )
            argument_error();
        else
        {
            switch (argv[count][1])
            {
                case 'c':
                    g_coldboot = TRUE;
                    break;

                case 'C':
                    g_coldboot = TRUE;
                    g_never_warmboot = TRUE;
                    break;

                case 'l':
                    if ( ++count < argc )
                    {
                        strcpy(logdir, argv[count]);
                        break;
                    }
                    argument_error();
                    break;

                case 'p':
                    if ( ++count < argc && is_number(argv[count]) )
                    {
                        port = atoi(argv[count]);
                        break;
                    }
                    argument_error();
                    break;

                case 's':
                    if ( ++count < argc )
                    {
                        strcpy(scenario_dir, argv[count]);
                        break;
                    }
                    argument_error();
                    break;

                default:
                    argument_error();
                    break;
            }
        }
    }
}


int
main (int argc, char **argv)
{
    struct timeval now_time;
    pid_t pid;
    FILE *fp;
    int control;

    /*
     * Memory debugging if needed.
     */
#if defined(MALLOC_DEBUG)
    malloc_debug(2);
#endif

    /*
     * Init time.
     */
    gettimeofday(&now_time, NULL);
    current_time = (time_t) now_time.tv_sec;
    start_time = time(0);
    strcpy(str_boot_time, ctime(&current_time));

    umask(026);
    pid = getpid();
    fp = fopen("../tmp/ground0.pid", "w");

    if ( fp )
    {
        fprintf(fp, "%u\n", (unsigned int) pid);
        fclose(fp);
    }

    strcpy(logdir, "../log");

    umask(006);
    parse_arguments(argc, argv);
    handle_log_redir(logdir);
    boot_db();

    logmesg("Initializing resolver...");
    ResolvInitialize();

    if ( g_coldboot || !p_soft_reboot() || read_connections() )
    {
        g_control_socket = control = init_socket(port);
        logmesg("Socket initiated.");
        g_coldboot = FALSE;     /* Don't force next to be coldboot. */
    }
    else
    {
        control = g_control_socket;
        logmesg("Soft booted.");
    }

    logmesg("Ground ZERO is ready to rock on port %hu.", port);

    game_loop_unix(control);
    cleanup_log_redir();

    logmesg("Terminating resolver...");
    ResolvTerminate();

    if ( g_never_warmboot || g_coldboot )
    {
        close(control);
        logmesg("Normal termination of game.");
        sprintf(log_buf, "Exiting with value %d.", ground0_down);
        logmesg(log_buf);
        exit(ground0_down);
    }

    logmesg("Attempting soft boot...");
    soft_reboot();
    exit(EXIT_FAILURE);         /* We should never get here. */
}



int
init_socket (int port)
{
    static struct sockaddr_in sa_zero;
    struct sockaddr_in sa;
    int x = 1;
    int fd;

    if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        logerr("Init_socket: socket");
        exit(STATUS_ERROR);
    }

    if ( setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &x, sizeof(x) ) <
        0)
    {
        logerr("Init_socket: SO_REUSEADDR");
        close(fd);
        exit(STATUS_ERROR);
    }
#if 0
    {
        struct linger ld;

        ld.l_onoff = 1;
        ld.l_linger = 1000;

        if (setsockopt
            (fd, SOL_SOCKET, SO_DONTLINGER, (char *) &ld, sizeof(ld)) < 0)
        {
            logerr("Init_socket: SO_DONTLINGER");
            close(fd);
            exit(STATUS_ERROR);
        }
    }
#endif

    sa = sa_zero;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    if ( bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0 )
    {
        fprintf(stderr, "bind() failed.  Retrying...\n");
        while ( bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0 )
            sleep(1);
    }


    if ( listen(fd, 5) < 0 )
    {
        logerr("Init socket: listen");
        close(fd);
        exit(STATUS_ERROR);
    }

    return fd;
}


void
coredump ()
{
    char *b = NULL;

    logmesg("Initating core dump sequence...");
    *b = 'X';
}


void
deadlock_check (int sig)
{
    static int last_iteration = -1;

    if ( last_iteration == iteration )
    {
        logmesg
            ("DEADLOCK PROTECTION!  Mud apparently frozen on iteration %d.",
             iteration);
        coredump();
    }

    last_iteration = iteration;
    alarm(30);
}


void
game_loop_unix (int control)
{
    static struct timeval null_time;
    struct timeval last_time;

    signal(SIGALRM, deadlock_check);
    signal(SIGPIPE, SIG_IGN);
    gettimeofday(&last_time, NULL);
    current_time = (time_t) last_time.tv_sec;
    alarm(30);

    /* Main loop */
    while ( !ground0_down )
    {
        fd_set in_set;
        fd_set out_set;
        fd_set exc_set;
        DESCRIPTOR_DATA *d;
        int maxdesc;

        iteration++;

#if defined(MALLOC_DEBUG)
        if ( malloc_verify() != 1 )
            abort();
#endif

        ResolvProcess();

        /*
         * Poll all active descriptors.
         */
        FD_ZERO(&in_set);
        FD_ZERO(&out_set);
        FD_ZERO(&exc_set);
        FD_SET(control, &in_set);
        maxdesc = control;
        for ( d = descriptor_list; d; d = d->next )
        {
            maxdesc = UMAX(maxdesc, d->descriptor);
            FD_SET(d->descriptor, &in_set);
            FD_SET(d->descriptor, &out_set);
            FD_SET(d->descriptor, &exc_set);
        }

        while ( select(maxdesc + 1, &in_set, &out_set, &exc_set, &null_time )
               == -1)
        {
            if ( errno == EINTR )
                continue;
            logerr("Game_loop: select: poll");
            exit(STATUS_ERROR);
        }

        /* New connection? */
        if ( FD_ISSET(control, &in_set) )
            new_descriptor(control);

        /* Kick out the freaky folks. */
        for ( d = descriptor_list; d != NULL; d = d_next )
        {
            d_next = d->next;
            if ( FD_ISSET(d->descriptor, &exc_set) )
            {
                FD_CLR(d->descriptor, &in_set);
                FD_CLR(d->descriptor, &out_set);
                d->outtop = 0;
                close_socket(d);
            }
        }

        /* Process input. */
        for ( d = descriptor_list; d != NULL; d = d_next )
        {
            d_next = d->next;
            d->fcommand = FALSE;

            if ( FD_ISSET(d->descriptor, &in_set) )
            {
                if ( d->wait > 0 )
                {
                    --d->wait;
                    continue;
                }
                if ( d->character != NULL )
                    d->character->timer = 0;
                if ( !read_from_descriptor(d) )
                {
                    FD_CLR(d->descriptor, &out_set);
                    if ( d->character != NULL )
                        save_char_obj(d->character);
                    d->outtop = 0;
                    close_socket(d);
                    continue;
                }
            }

            if ( d->character != NULL && d->character->wait > 0 )
            {
                --d->character->wait;
                continue;
            }

            read_from_buffer(d);

            if ( d->incomm[0] != '\0' )
            {
                d->fcommand = TRUE;

                if ( d->showstr_point )
                    show_string(d, d->incomm);
                else if ( d->connected == CON_PLAYING )
                    interpret(d->character, d->incomm);
                else
                    nanny(d, d->incomm);

                d->incomm[0] = '\0';
            }
            else if ( d->connected == CON_RESOLVE_ADDRESS )
                nanny(d, d->incomm);
        }

        /* Autonomous game motion. */
        update_handler();

        /* Output. */
        for ( d = descriptor_list; d != NULL; d = d_next )
        {
            d_next = d->next;

            if ( d->wait > 0 )
            {
                --d->wait;
                continue;
            }

            if ( (d->fcommand || d->outtop > 0 || d->outcom_stream ) &&
                FD_ISSET(d->descriptor, &out_set))
            {
                bool ok = FALSE;

                if ( d->fcommand || d->outtop > 0 )
                    ok = process_output(d, d->fcommand);
                if ( d->outcom_stream )
                    ok = process_compressed_output(d);

                if ( !ok )
                {
                    if ( d->character != NULL )
                        save_char_obj(d->character);
                    d->outtop = 0;
                    close_socket(d);
                }
            }
        }

        /*
         * Synchronize to a clock.
         * Sleep( last_time + 1/PULSE_PER_SECOND - now ).
         * Careful here of signed versus unsigned arithmetic.
         */
        {
            struct timeval now_time;
            long secDelta;
            long usecDelta;

            gettimeofday(&now_time, NULL);
            usecDelta =
                ((int) last_time.tv_usec) - ((int) now_time.tv_usec) +
                1000000 / PULSE_PER_SECOND;
            secDelta = ((int) last_time.tv_sec) - ((int) now_time.tv_sec);
            while ( usecDelta < 0 )
            {
                usecDelta += 1000000;
                secDelta -= 1;
            }

            while ( usecDelta >= 1000000 )
            {
                usecDelta -= 1000000;
                secDelta += 1;
            }

            if ( secDelta > 0 || (secDelta == 0 && usecDelta > 0) )
            {
                struct timeval stall_time;

                stall_time.tv_usec = usecDelta;
                stall_time.tv_sec = secDelta;

                while ( select(0, NULL, NULL, NULL, &stall_time) == -1 )
                {
                    if ( errno == EINTR )
                        continue;
                    logerr("Game_loop: select: stall");
                    exit(STATUS_ERROR);
                }
            }
        }

        gettimeofday(&last_time, NULL);
        current_time = (time_t) last_time.tv_sec;
    }

    return;
}


struct descriptor_data *
alloc_descriptor (void)
{
    static struct descriptor_data d_zero;
    struct descriptor_data *dnew;
    int size;

    if ( descriptor_free == NULL )
        dnew = alloc_perm(sizeof(*dnew));
    else
    {
        dnew = descriptor_free;
        descriptor_free = descriptor_free->next;
    }

    *dnew = d_zero;
    dnew->descriptor = -1;
    dnew->connected = CON_BREAK_CONNECT;
    dnew->showstr_head = NULL;
    dnew->showstr_point = NULL;
    dnew->outsize = 2000;
    dnew->outbuf = alloc_mem(dnew->outsize);
    dnew->adm_data = NULL;
    dnew->wait = 0;
    dnew->character = 0;
    dnew->max_out = 5000;
    dnew->outcom = NULL;
    dnew->outcom_stream = NULL;

    dnew->botLagFlag = 0;

    dnew->telstate = TELST_NOMINAL;
    dnew->subneg_len = 0;
    memset(dnew->subneg_buf, 0, MAX_SB_LEN);
    memset(dnew->telopt, 0, sizeof(int) * NTELOPTS);
    memset(dnew->ttype_recvd, 0, MAX_SB_LEN);
    dnew->ttype = TTYPE_ANSI;
    dnew->lines = 24;
    dnew->columns = 80;

    dnew->termdata.fg = 0;
    dnew->termdata.bg = 0;
    dnew->termdata.isbold = 1;
    dnew->termdata.isflash = 0;

    dnew->hpoint = 0;

    for ( size = 0; size < MAX_HISTORY_SIZE; size++ )
        *dnew->inlast[size] = '\0';

    return (dnew);
}


void
new_descriptor (int control)
{
    DESCRIPTOR_DATA *dnew;
    struct sockaddr_in sock;
    int desc;
    socklen_t addr_size;

    addr_size = sizeof(sock);
    

    if ( (desc = accept(control, (struct sockaddr *) &sock, &addr_size)) < 0 )
    {
        logerr("New_descriptor: accept");
        return;
    }

    if ( net_nonblock(desc) == -1 )
    {
        logerr("nonblock()");
        close(desc);
        return;
    }

    dnew = alloc_descriptor();
    dnew->descriptor = desc;
    dnew->connected = CON_RESOLVE_ADDRESS;
    dnew->next = descriptor_list;
    dnew->wait = 1 RLSEC;
    descriptor_list = dnew;

    strcpy(dnew->host, inet_ntoa(sock.sin_addr));
    write_to_descriptor(dnew,
                        "Resolving hostname and negotiating connection options",
                        0, FALSE);
    ResolvQuery(dnew, sock.sin_addr);

    IAC_option(dnew, DO, TELOPT_LFLOW);
    IAC_option(dnew, DO, TELOPT_TTYPE);
    IAC_option(dnew, DO, TELOPT_NAWS);
    /* sixthgear removed MCCP protocol sent to allow Windows XP terminal to work */
    /* IAC_option(dnew, WILL, TELOPT_COMPRESS2); */
}



void
close_socket (DESCRIPTOR_DATA * dclose)
{
    extern DESCRIPTOR_DATA *pinger_list;
    CHAR_DATA *ch;

    logmesg("closing: connected = %d, host = %s", dclose->connected,
               dclose->host);

    if ( dclose->dns_query )
        ResolvCancel(dclose);

    if ( dclose->outtop > 0 )
        process_output(dclose, FALSE);

    if ( dclose->outcom_stream )
        end_compression(dclose);

    if ( dclose->snoop_by != NULL )
        write_to_buffer(dclose->snoop_by,
                        "Your victim has left the game.\n\r", 0);

    for (struct descriptor_data * d = descriptor_list; d != NULL;
         d = d->next)
        if ( d->snoop_by == dclose )
            d->snoop_by = NULL;

    if ( (ch = dclose->character) != NULL )
    {
        sprintf(log_buf, "Net death has claimed %s's soul.", ch->names);
        logmesg("%s", log_buf);
        wiznet(log_buf, NULL, NULL, WIZ_SITES, 0, get_trust(ch));

        if ( ch->log_fp != NULL )
        {
            fclose(ch->log_fp);
            ch->log_fp = NULL;
        }

        for (struct descriptor_data * d = descriptor_list; d != NULL;
             d = d->next)
            if ( d->character && d->character->dubbing == ch )
            {
                send_to_char("Your dubbing victim has left.\r\n",
                             d->character);
                d->character->dubbing = NULL;
            }

        if ( dclose->ping_pipe )
        {
            dclose->ping_wait = 0;
            pclose(dclose->ping_pipe);

            if ( dclose == pinger_list )
                pinger_list = pinger_list->ping_next;
            else
            {
                struct descriptor_data *pn = pinger_list;

                while ( pn && pn->ping_next != dclose )
                    pn = pn->ping_next;
                if ( pn )
                    pn->ping_next = dclose->ping_next;
            }

            dclose->ping_pipe = NULL;
            dclose->ping_next = NULL;
        }

        if ( dclose->connected == CON_PLAYING )
        {
            stop_manning(ch);
            REMOVE_BIT(ch->temp_flags, IN_VEHICLE);
            act("$n has lost $s link.", ch, NULL, NULL, TO_ROOM);
            save_char_obj(ch);
            ch->desc = NULL;

            if ( dclose->ttype == TTYPE_LINUX )
                write_to_buffer(dclose, "\x1B]R", 0);

            if ( !IS_IMMORTAL(ch) )
                ch->ld_timer = LD_TICKS;

            if ( (ch->in_room->level < 0) && !IS_IMMORTAL(ch ) &&
                !ch->in_room->interior_of)
            {
                logmesg("Transporting the character.");
                do_teleport(ch, "");
            }
        }
        else
        {
            free_char(dclose->character);
        }
    }

    if ( d_next == dclose )
        d_next = d_next->next;

    if ( dclose == descriptor_list )
    {
        descriptor_list = descriptor_list->next;
    }
    else
    {
        DESCRIPTOR_DATA *d;

        for ( d = descriptor_list; d && d->next != dclose; d = d->next ) ;
        if ( d != NULL )
            d->next = dclose->next;
        else
            logmesg("Close_socket: dclose not found.");
    }

    if ( dclose->ping_pipe )
    {
        if ( dclose == pinger_list )
        {
            pinger_list = pinger_list->next;
        }
        else
        {
            DESCRIPTOR_DATA *d = pinger_list;

            while ( d && d->next != dclose )
                d = d->next;
            if ( d )
                d->next = dclose->next;
        }
    }

    close(dclose->descriptor);
    free_mem(dclose->outbuf, dclose->outsize);
    dclose->next = descriptor_free;
    descriptor_free = dclose;
    return;
}



bool
read_from_descriptor (DESCRIPTOR_DATA * d)
{
    char buf[MAX_INPUT_LENGTH];
    register int idx = 0;

    if ( d->incomm[0] != '\0' )
        return TRUE;

    if ( strlen(d->inbuf) >= sizeof(d->inbuf) - 10 )
    {
        logmesg("%s input overflow!", d->host);
        write_to_descriptor(d, "\r\n*** PUT A LID ON IT!!! ***\r\n", 0,
                            FALSE);
        return (FALSE);
    }

    for ( ;; )
    {
        int nRead;

        nRead =
            read(d->descriptor, buf + idx, MAX_INPUT_LENGTH - 10 - idx);

        if ( nRead > 0 )
        {
            g_bytes_recv += nRead;
            idx += nRead;

            if ( buf[idx - 1] == '\n' || buf[idx - 1] == '\r' )
                break;
        }
        else if ( nRead == 0 )
        {
            logmesg("EOF encountered on read.");
            return (FALSE);
        }
        else if ( errno == EWOULDBLOCK || errno == EINTR )
            break;
        else
        {
            logerr("Read_from_descriptor");
            return (FALSE);
        }
    }

    process_telnet(d, (const unsigned char*) buf, idx);
    return (TRUE);
}


bool
shell_substitute (DESCRIPTOR_DATA * d)
{
    char buf[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char *ptr, *wptr;
    int i;

    if ( d->hpoint < 1 )
    {
        d->repeat = 0;
        return (TRUE);
    }

    if ( *d->incomm == '!' )
    {
        strcpy(buf, d->incomm + 1);
        ptr = buf;
        wptr = arg;

        while ( *ptr && !isspace(*ptr) )
            *(wptr++) = *(ptr++);
        *wptr = '\0';

        if ( !*arg || *arg == '!' )
            i = d->hpoint - 1;
        else if ( isdigit(*arg) )
            i = atoi(arg);
        else
            for ( i = d->hpoint - 1; i >= 0; i-- )
                if ( !str_prefix(arg, d->inlast[i]) )
                    break;

        if ( i <= -1 || i >= d->hpoint )
        {
            write_to_buffer(d, "Bad reference.\r\n", 0);
            d->fcommand = TRUE;
            return (FALSE);
        }

        strcpy(d->incomm, d->inlast[i]);
        strcat(d->incomm, ptr);

        if ( !str_cmp(d->incomm, d->inlast[d->hpoint - 1]) )
            d->repeat++;
        else
            d->repeat = 0;
    }

    if ( *d->incomm == '^' )
    {                           /* ^old^new */
        ptr = d->incomm + 1;
        wptr = arg;

        while ( *ptr && *ptr != '^' )
            *(wptr++) = *(ptr++);
        *wptr = '\0';

        if ( !*arg || *ptr != '^' )
        {
            write_to_buffer(d, "Bad substitution.\r\n", 0);
            d->fcommand = TRUE;
            return (FALSE);
        }

        strcpy(buf, d->inlast[d->hpoint - 1]);
        wptr = strstr(buf, arg);

        if ( !wptr || (wptr > buf && *(wptr - 1) == TCON_TOK ) ||
            *wptr == TCON_TOK)
        {
            write_to_buffer(d, "Bad substitution.\r\n", 0);
            d->fcommand = TRUE;
            return (FALSE);
        }

        *wptr = '\0';
        strcat(buf, ptr + 1);
        strcat(buf, d->inlast[d->hpoint - 1] + (wptr - buf) + strlen(arg));
        strcpy(d->incomm, buf);

        if ( !str_cmp(d->incomm, d->inlast[d->hpoint - 1]) )
            d->repeat++;
        else
            d->repeat = 0;
    }

    return (TRUE);
}


void
push_history (DESCRIPTOR_DATA * d)
{
    int i;

    if ( d->hpoint == MAX_HISTORY_SIZE )
        for ( i = 0; i < MAX_HISTORY_SIZE - 1; i++ )
            strcpy(d->inlast[i], d->inlast[i + 1]);
    else
        d->hpoint++;

    strcpy(d->inlast[d->hpoint - 1], d->incomm);
}


/*
 * Transfer one line from input buffer to input line.
 */
void
read_from_buffer (DESCRIPTOR_DATA * d)
{
    int i, j, k;
    int tconTokSeries = 0;

    /*
     * Hold horses if pending command already.
     */
    if ( d->incomm[0] != '\0' )
        return;

    /*
     * Look for at least one new line.
     */
    for ( i = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++ )
    {
        if ( d->inbuf[i] == '\0' )
            return;
    }

    /*
     * Canonical input processing.
     */
    for ( i = 0, k = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++ )
    {
        if ( k >= MAX_INPUT_LENGTH - 2 )
        {
            write_to_descriptor(d, "Line too long.\n\r", 0, FALSE);

            /* skip the rest of the line */
            for ( ; d->inbuf[i] != '\0'; i++ )
            {
                if ( d->inbuf[i] == '\n' || d->inbuf[i] == '\r' )
                    break;
            }
            d->inbuf[i] = '\n';
            d->inbuf[i + 1] = '\0';
            break;
        }

        if ( (d->inbuf[i] == '\x7F' || d->inbuf[i] == '\b') && k > 0 )
        {
            tconTokSeries = 0;
            --k;
        }
        else if ( d->inbuf[i] == '\x1B' )
        {
            tconTokSeries = 0;
            k = 0;
        }
        else if (d->inbuf[i] == TCON_TOK &&
                 (d->inbuf[i + 1] == '\r' || d->inbuf[i + 1] == '\n'))
        {
            if ( !(tconTokSeries % 2) )
            {
                tconTokSeries++;
                d->incomm[k++] = TCON_TOK;
            }
            d->incomm[k++] = TCON_TOK;
            tconTokSeries++;
        }
        else if ( d->inbuf[i] == TCON_TOK )
        {
            extern const int codeTrustLevel[];

            if (!d->character ||
                codeTrustLevel[((int) d->inbuf[i + 1])] >
                d->character->trust)
                if ( !(tconTokSeries % 2) )
                {
                    d->incomm[k++] = TCON_TOK;
                    tconTokSeries++;
                }
            d->incomm[k++] = TCON_TOK;
            tconTokSeries++;
        }
        else if ( isascii(d->inbuf[i]) && isprint(d->inbuf[i]) )
        {
            d->incomm[k++] = d->inbuf[i];
            tconTokSeries = 0;
        }
    }

    /*
     * Finish off the line.
     */
    if ( k == 0 )
        d->incomm[k++] = ' ';
    d->incomm[k] = '\0';

    /*
     * Do substitutions and count repeated commands (but if, and only if, the
     * player is actually in the game, so we don't, for instance, store their
     * passwords in the history buffer).
     */
    if ( d->connected == CON_PLAYING )
    {
        if ( *d->incomm == '^' || *d->incomm == '!' )
        {
            bool rv = shell_substitute(d);

            push_history(d);
            if ( !rv )
                *d->incomm = '\0';
        }
        else
        {
            if (d->hpoint > 0 &&
                !str_cmp(d->inlast[d->hpoint - 1], d->incomm))
                d->repeat++;
            else
                d->repeat = 0;

            push_history(d);
        }
    }

    /*
     * Shift the input buffer.
     */
    while ( d->inbuf[i] == '\n' || d->inbuf[i] == '\r' )
        i++;

    for ( j = 0; (d->inbuf[j] = d->inbuf[i + j]) != '\0'; j++ ) ;
}



/*
 * Low level output function.
 */
bool
process_output (DESCRIPTOR_DATA * d, bool fPrompt)
{

    /* we do the descriptor writes twice now to give ourselves an empty buffer
       to put a prompt in, in the case of truncated buffer writes */
    /*
     * Short-circuit if nothing to write.
     */
    if ( d->outtop != 0 )
    {
        /*
         * Snoop-o-rama.
         */
        if ( d->snoop_by != NULL )
        {
            if ( d->character != NULL )
                write_to_buffer(d->snoop_by, d->character->names, 0);
            write_to_buffer(d->snoop_by, "> ", 2);
            write_to_buffer(d->snoop_by, d->outbuf, d->outtop);
        }

        /*
         * OS-dependent output.
         */
        if ( !write_to_descriptor(d, d->outbuf, d->outtop, TRUE) )
        {
            d->outtop = 0;
            return FALSE;
        }
        else
            d->outtop = 0;
    }

    /*
     * Bust a prompt.
     */
    if ( !ground0_down && d->showstr_point )
    {
        if ( d->character )
            send_to_char("\r\n\n&{&X[&nHit &WRETURN&n to continue&X]&}",
                         d->character);
        else
            write_to_buffer(d, "\r\n\n[Hit ENTER to continue]", 0);
    }
    else if ( fPrompt && !ground0_down && d->connected == CON_PLAYING )
    {
        CHAR_DATA *ch = d->character;

        if ( !ch )
        {
            do_immtalk(NULL,
                       "%NULL character in descriptor detected. Closing link.");
            return FALSE;
        }

        ch = d->original ? d->original : d->character;

        if ( !IS_SET(ch->comm, COMM_COMPACT) && !IS_SET(ch->temp_flags, TEMP_BLOCK_ED) )
            write_to_buffer(d, "\r\n", 2);

        if ( IS_SET(ch->comm, COMM_PROMPT) )
        {
            char buf[MAX_INPUT_LENGTH];

            if ( IS_SET(ch->temp_flags, TEMP_BLOCK_ED) )
            {
                send_to_char("&X(&ced&X)&W> &n", d->character);
            }
            else
            {
                ch = (d->character->in_room->inside_mob ?
                      d->character->in_room->inside_mob : d->character);

                sprintf(buf, "&uH%d/%d&nhp", ch->hit, ch->max_hit);

                if ( d->character->invis_level )
                {
                    char buf2[10];

                    sprintf(buf2, " (&uw%d&n)", d->character->invis_level);
                    strcat(buf, buf2);
                }

                strcat(buf, "\r\n&W> &n");
                send_to_char(buf, d->character);
            }
        }

        if ( IS_SET(ch->comm, COMM_TELNET_GA) )
            write_to_buffer(d, go_ahead_str, 0);
    }

    /*
     * Short-circuit if nothing to write.
     */
    if ( d->outtop == 0 )
        return TRUE;

    /*
     * Snoop-o-rama.
     */
    if ( d->snoop_by != NULL )
    {
        if ( d->character != NULL )
            write_to_buffer(d->snoop_by, d->character->names, 0);
        write_to_buffer(d->snoop_by, "> ", 2);
        write_to_buffer(d->snoop_by, d->outbuf, d->outtop);
    }

    int tmp = d->outtop;

    d->outtop = 0;
    return write_to_descriptor(d, d->outbuf, tmp, TRUE);
}


void
send_to_char (const char *txt, CHAR_DATA * ch)
{
    if ( ch->desc == NULL || txt == NULL )
        return;

    write_to_buffer(ch->desc, txt, strlen(txt));
}


void
send_to_room (const char *txt, struct room_data *rm)
{
    struct char_data *ch = rm->people;

    while ( ch != NULL )
    {
        send_to_char(txt, ch);
        ch = ch->next_in_room;
    }
}


void
send_to_team (const char *txt, int team)
{
    CHAR_DATA *fch;

    for ( fch = char_list; fch; fch = fch->next )
        if ( fch->desc && !IS_NPC(fch) &&
             (IS_SET(fch->pcdata->listen_data, (1 << team)) ||
              fch->team == team) )
            send_to_char(txt, fch);
}


/*
 * Append onto an output buffer.
 */
void
write_to_buffer (DESCRIPTOR_DATA * d, const char *txt, int length)
{
    if ( length < 1 )
        length = strlen(txt);

    /*
     * Initial \r\n if needed.
     */
    if (d->outtop == 0 && !d->fcommand && d->connected != CON_GET_NAME &&
        (!d->character || !IS_SET(d->character->comm, COMM_COMPACT)))
    {
        d->outbuf[0] = '\r';
        d->outbuf[1] = '\n';
        d->outtop = 2;
    }

    if ( !ground0_down )
    {
        if ( d->outtop + 1 > d->max_out )
            return;
        if ( d->outtop + length > d->max_out )
            length = d->max_out - d->outtop - 1;
    }

    /*
     * Expand the buffer as needed.
     */
    while ( d->outtop + length >= d->outsize )
    {
        char *outbuf;

        if ( d->outsize >= 32000 )
        {
            if ( d->outtop + 1 == d->outsize )
                return;
            logmesg("Buffer overflow. Truncating.");
            length = d->outsize - d->outtop - 1;
            break;
        }
        outbuf = alloc_mem(2 * d->outsize);
        strncpy(outbuf, d->outbuf, d->outtop);
        free_mem(d->outbuf, d->outsize);
        d->outbuf = outbuf;
        d->outsize *= 2;
    }

    /*
     * Copy.
     */
    strncpy(d->outbuf + d->outtop, txt, length);
    d->outtop += length;
    if ( d->outtop > d->outsize )
        d->outbuf[d->outtop - 1] = '\0';
    else
        d->outbuf[d->outtop] = '\0';
}



/*
 * Lowest level output function.
 * Write a block of text to the file descriptor.
 * If this gives errors on very long blocks (like 'ofind all'),
 *   try lowering the max block size.
 */
bool
write_to_descriptor (struct descriptor_data *d, char *txt, int length,
                    bool proc)
{
    int iStart;
    int nWrite;
    int nBlock;

    if ( length <= 0 )
        length = strlen(txt);

    char buf[32769];

    if ( proc )
    {
        ProcessColors(d, !ttype_has_color(d->ttype), txt, length, buf,
                      32768);
        length = strlen(buf);
    }
    else
    {
        strncpy(buf, txt, length);
        buf[length] = '\0';
    }

    if ( d->outcom_stream )
        return write_compressed(d, buf, length);

    for ( iStart = 0; iStart < length; iStart += nWrite )
    {
        nBlock = UMIN(length - iStart, 4096);

        if ( (nWrite = write(d->descriptor, buf + iStart, nBlock)) < 0 )
        {
            if ( errno == EINTR )
                break;
            logerr("Write_to_descriptor");
            return FALSE;
        }

        g_bytes_sent += nWrite;
    }

    return TRUE;
}


GOD_TYPE *
get_god (char *a_name)
{
    sh_int count;
    int ikeys, gkeys;
    char *nameptr, namearr[MAX_STRING_LENGTH];

    ikeys = sizeof(GOD_TYPE)/sizeof(imp_table[0]);
    gkeys = sizeof(GOD_TYPE)/sizeof(god_table[0]);
    nameptr = capitalize(a_name);    

    // nameptr changes during iteration below, so make a copy
    for ( count = 0; nameptr[count]; count++ )
	namearr[count] = nameptr[count];
    // Add nul to the end
    namearr[count] = 0;

    for ( count = 0; count < ikeys; count++ )
        if ( !str_cmp(namearr, capitalize(imp_table[count].rl_name)) )
 	           return (GOD_TYPE *) (imp_table + count);
    for ( count = 0; count < gkeys; count++ )
        if ( !str_cmp(namearr, capitalize(god_table[count].rl_name)) )
            return (GOD_TYPE *) (god_table + count);

    return NULL;
}

/* This was "hard coded for security reasons" by previous maintainers at some point. */

/* Format is {"Login name", Trust Level, "Character name", "login name's password", ""}
 * For example: {"Greg", 10, "Snoopy", "password", ""} to login as Greg and play
 * the character named Snoopy.  Note that "password" applies to Greg and not Snoopy, which
 * must have its own separate password. Trust levels are shown in src/ground0.h */

const struct god_type imp_table[] = {
    {"Test", 10, "Test", "test", ""}
};


static void
finish_newbie (struct char_data *ch)
{
    struct descriptor_data *d = ch->desc;

    wizlog(0, 0, WIZ_SITES, 0, get_trust(ch), "%s@%s has connected.",
           ch->names, d->host);
    ch->pcdata->created_site = str_dup(d->host);
    set_title(ch, "is &YFRESH MEAT&n");
    ch->kills = 0;
    ch->team = -1;
    do_save(ch, "");

#if 0
    do_help(ch, "newbie");
    d->connected = CON_READ_NEWBIE;
#else
    do_help(ch, "motd");
    d->connected = CON_READ_MOTD;
#endif

    page_to_char("Press &WENTER&n to continue&X: &n", ch);
    show_string(d, "");
}


void
show_greetings (struct descriptor_data *d)
{
    extern char *help_greeting1;

    system_info(d);
    write_to_buffer(d, help_greeting1, 0);
    write_to_buffer(d,
                    "               Original DikuMUD by Hans Staerfeldt, Katja Nyboe\r\n"
                    "               Tom Madsen, Michael Seifert, and Sebastian Hammer\r\n"
                    "               Based on MERC 2.1 code by Hatchet, Furey, and Kahn\r\n"
                    "               ROM 2.3 code written by Russ Taylor, copyright 1993-1994\r\n"
                    "               Ground ZERO base by Cory Hilke.\r\n"
                    "\r\n", 0);
}


void
CompleteLogin (struct descriptor_data *d)
{
    const char *reason;

    write_to_buffer(d, "\r\n", 2);

    if ( (reason = GetFullBan(d->host)) != NULL )
    {
        write_to_buffer(d,
                        "Sorry, your site has been banned from this game for:&W\r\n",
                        0);
        write_to_buffer(d, reason, 0);
        write_to_buffer(d,
                        "\r\n&nSend complaints or questions to &cadmins@gz.malawar.net&n.\r\n",
                        0);

        wiznet(log_buf, NULL, NULL, WIZ_BANS, 0, 0);
        logmesg("%s", log_buf);

        close_socket(d);
        return;
    }

    show_greetings(d);

    write_to_buffer(d, "Welcome soldier.\r\n"
                    "Please state your name, clearly : ", 0);

    d->connected = CON_GET_NAME;
}


/*
 * Deal with sockets that haven't logged in yet.
 */
void
nanny (DESCRIPTOR_DATA * d, char *argument)
{
    DESCRIPTOR_DATA *d_old, *d_next;
    char buf[MAX_STRING_LENGTH];
    ROOM_DATA *toroom;
    NOTE_DATA *pnote;
    char *pwdnew, *p;
    CHAR_DATA *ch;
    int notes;
    bool fOld;

    bool traitor_autoexpire(CHAR_DATA *);

    while ( isspace(*argument) )
        argument++;

    ch = d->character;

    switch (d->connected)
    {

        default:
            logmesg("Nanny: bad d->connected %d.", d->connected);
            close_socket(d);
            return;

        case CON_RELOG:
            if ( *argument == 'n' || *argument == 'N' )
            {
                write_to_descriptor(d,
                                    "OK.  Come back when ya' grow a spine.\r\n",
                                    0, FALSE);
                close_socket(d);
                return;
            }

            ch->team = -1;

            if ( ch->trust )
            {
                do_help(ch, "imotd");
                page_to_char("Press &WENTER&n to continue&X: &n", ch);
                show_string(d, "");
                d->connected = CON_READ_IMOTD;
            }
            else if (TEAM_NONE != -1 &&
                     (RANK(ch) >= RANK_MERC || RANK(ch) == RANK_JOSH) &&
                     (!IS_SET(ch->act, PLR_TRAITOR) ||
                      traitor_autoexpire(ch)))
            {
                write_to_buffer(d, "Do you want to go solo? (y/n) ", 0);
                d->connected = CON_SOLO;
            }
            else
            {
                do_help(ch, "motd");
                page_to_char("Press &WENTER&n to continue&X: &n", ch);
                show_string(d, "");
                d->connected = CON_READ_MOTD;
            }

            break;

        case CON_RESOLVE_ADDRESS:
            write_to_descriptor(d, ".", 1, FALSE);
            break;

        case CON_GET_NAME:
            if ( argument[0] == '\0' )
            {
                close_socket(d);
                return;
            }

            argument[0] = UPPER(argument[0]);
            if ( !check_parse_name(argument) )
            {
                write_to_buffer(d, "Illegal name, try another.\n\rName: ",
                                0);
                return;
            }

            if ( !d->adm_data && (d->adm_data = get_god(argument)) )
            {
                ECHO_OFF(d);
                write_to_buffer(d, "&nPassword: ", 0);
                d->connected = CON_GET_ADMIN_PASS;
                return;
            }

            if ( (ch = load_char_obj(d, argument)) == NULL )
                fOld = FALSE;
            else
                fOld = TRUE;

            d->character->trust = 0;

            if ( d->adm_data )
            {
                if ( is_name(argument, d->adm_data->game_names) )
                {
                    logmesg("Trusting the admin.");
                    d->character->trust = d->adm_data->trust;
                }
                else
                {
                    write_to_buffer(d,
                                    "\a\aYou are not authorized to play that character.\r\n"
                                    "Enter your name: ", 0);
                    d->connected = CON_GET_NAME;
                    return;
                }
            }

            if ( ch && IS_SET(ch->act, PLR_DENY) )
            {
                sprintf(log_buf, "Denying Player: %s@%s.", argument,
                        d->host);
                logmesg(log_buf);
                wiznet(log_buf, ch, NULL, WIZ_BANS, 0, ch->trust);
                write_to_buffer(d, "You are denied access.\n\r", 0);
                close_socket(d);
                return;
            }

            if ( check_reconnect(d, argument, FALSE) )
            {
                fOld = TRUE;
            }
            else
            {
                if ( wizlock && !IS_HERO(ch) )
                {
                    write_to_buffer(d, "The game is wizlocked.\n\r", 0);
                    close_socket(d);
                    return;
                }
            }

            if ( fOld )
            {
                if (ch->pcdata && ch->pcdata->created_site &&
                    ch->kills < 10 && ch->deaths < 10 &&
                    str_cmp(ch->pcdata->created_site, "localhost") &&
                    str_cmp(ch->pcdata->created_site, d->host) &&
                    GetNewbieBan(d->host))
                {
                    write_to_buffer(d,
                                    "Sorry, your site is newbie banned.  Although the character\r\n"
                                    "you are attempting to login exists, it was created by\r\n"
                                    "another site and is still a newbie.  This is generally\r\n"
                                    "because you attempted to circumvent the newbie ban by\r\n"
                                    "having someone else create the character for you.  This\r\n"
                                    "is not allowed.\r\n\r\n"
                                    "You may e-mail admins@gz.malawar.net for clarification.\r\n",
                                    0);

                    sprintf(log_buf,
                            "NEWBIE CREATED BY %s LOGIN FROM NEWBIE BANNED SITE: %s",
                            ch->pcdata->created_site, d->host);
                    logmesg("%s", log_buf);
                    wiznet(log_buf, NULL, NULL, WIZ_BANS, 0, 0);

                    close_socket(d);
                    return;
                }

                /* Old player */
                ECHO_OFF(d);
                write_to_buffer(d, "Password: ", 0);
                d->connected = CON_GET_OLD_PASSWORD;
                return;
            }
            else
            {
                /* New player */
                if ( newlock )
                {
                    write_to_buffer(d, "The game is newlocked.\n\r", 0);
                    close_socket(d);
                    return;
                }

                for ( d_old = descriptor_list; d_old; d_old = d_old->next )
                {
                    if (d_old != d && d_old->connected != CON_PLAYING &&
                        d_old->character &&
                        !str_cmp(d_old->character->names, argument))
                    {
                        write_to_buffer(d,
                                        "There's someone already creating a character with that name.\r\n"
                                        "Illegal name, try another: ", 0);
                        return;
                    }
                }

                const char *reason = GetNewbieBan(d->host);

                if ( reason )
                {
                    write_to_buffer(d,
                                    "Your site has lost the privilege of creating new characters, here.\r\n"
                                    "The following reason was given by the administrator who performed\r\n"
                                    "the ban:\r\n\r\n", 0);
                    write_to_buffer(d, reason, 0);
                    write_to_buffer(d,
                                    "\r\nYou may mail admins@gz.malawar.net if you need clarification.\r\n",
                                    0);

                    sprintf(log_buf, "Newbie Ban of %s@%s.", argument,
                            d->host);
                    logmesg("%s", log_buf);
                    wiznet(log_buf, NULL, NULL, WIZ_BANS, 0, 0);

                    close_socket(d);
                    return;
                }

                sprintf(buf, "Did I get that right, %s (Y/N)? ", argument);
                write_to_buffer(d, buf, 0);
                d->connected = CON_CONFIRM_NEW_NAME;
                return;
            }
            break;

        case CON_GET_ADMIN_PASS:

            write_to_buffer(d, "\n\r", 2);
            if (strcmp
                (crypt(argument, d->adm_data->password),
                 d->adm_data->password))
            {
                write_to_buffer(d, "Wrong password.\n\r", 0);
                sprintf(log_buf, "Admin %s failed to enter a valid pass.",
                        d->adm_data->rl_name);
                logmesg(log_buf);
                close_socket(d);
                return;
            }

            ECHO_ON(d);

            sprintf(log_buf, "Admin %s has connected.",
                    d->adm_data->rl_name);
            logmesg(log_buf);
            wiznet(log_buf, NULL, NULL, WIZ_SITES, 0, 10);

            write_to_buffer(d, "What character will you be? ", 0);
            d->connected = CON_GET_NAME;
            break;

        case CON_GET_OLD_PASSWORD:

            write_to_buffer(d, "\n\r", 2);
            if (strcmp
                (crypt(argument, ch->pcdata->password),
                 ch->pcdata->password))
            {
                write_to_buffer(d, "Wrong password.\n\r", 0);
                close_socket(d);
                return;
            }

            if ( !ch->pcdata->password || !ch->pcdata->password[0] )
            {
                write_to_buffer(d, "Warning! Null password!\r\n", 0);
                write_to_buffer(d,
                                "Please report old password with bug.\r\n",
                                0);
                write_to_buffer(d,
                                "Type 'password null <new password>' to fix.\r\n",
                                0);
            }

            ECHO_ON(d);

            if ( check_reconnect(d, ch->names, TRUE) )
                return;

            if ( check_playing(d, ch->names) )
                return;

            sprintf(log_buf, "%s@%s has connected.", ch->names, d->host);
            logmesg(log_buf);
            wiznet(log_buf, NULL, NULL, WIZ_SITES, 0, get_trust(ch));
            ch->team = -1;

            if ( IS_HERO(ch) )
            {
                do_help(ch, "imotd");
                page_to_char("Press &WENTER&n to continue&X: &n", ch);
                show_string(d, "");

                d->connected = CON_READ_IMOTD;
            }
            else
            {
                if (TEAM_NONE != -1 &&
                    (RANK(ch) >= RANK_MERC || RANK(ch) == RANK_JOSH) &&
                    (!IS_SET(ch->act, PLR_TRAITOR) ||
                     traitor_autoexpire(ch)))
                {
                    write_to_buffer(d, "Do you want to go solo? (y/n) ",
                                    0);
                    d->connected = CON_SOLO;
                }
                else
                {
                    do_help(ch, "motd");
                    page_to_char("Press &WENTER&n to continue&X: &n", ch);
                    show_string(d, "");

                    d->connected = CON_READ_MOTD;
                }
            }
            break;


        case CON_BREAK_CONNECT:
            switch (*argument)
            {
                case 'y':
                case 'Y':
                    for (d_old = descriptor_list; d_old != NULL;
                         d_old = d_next)
                    {
                        d_next = d_old->next;
                        if ( d_old == d || d_old->character == NULL )
                            continue;

                        if ( str_cmp(ch->names, d_old->character->names) )
                            continue;

                        close_socket(d_old);
                    }
                    if ( check_reconnect(d, ch->names, TRUE) )
                        return;
                    write_to_buffer(d,
                                    "Reconnect attempt failed.\n\rName: ",
                                    0);
                    if ( d->character != NULL )
                    {
                        free_char(d->character);
                        d->character = NULL;
                    }
                    d->connected = CON_GET_NAME;
                    break;

                case 'n':
                case 'N':
                    write_to_buffer(d, "Name: ", 0);
                    if ( d->character != NULL )
                    {
                        free_char(d->character);
                        d->character = NULL;
                    }
                    d->connected = CON_GET_NAME;
                    break;

                default:
                    write_to_buffer(d, "Please type Y or N? ", 0);
                    break;
            }
            break;

        case CON_CONFIRM_NEW_NAME:
            switch (*argument)
            {
                case 'y':
                case 'Y':
                    ECHO_OFF(d);
                    sprintf(buf,
                            "New character.\n\rGive me a password for %s: ",
                            ch->names);
                    write_to_buffer(d, buf, 0);
                    d->connected = CON_GET_NEW_PASSWORD;
                    break;

                case 'n':
                case 'N':
                    write_to_buffer(d, "Ok, what IS it, then? ", 0);
                    free_char(d->character);
                    d->character = NULL;
                    d->connected = CON_GET_NAME;
                    break;

                default:
                    write_to_buffer(d, "Please type Yes or No? ", 0);
                    break;
            }
            break;

        case CON_GET_NEW_PASSWORD:
            write_to_buffer(d, "\n\r", 2);

            if ( strlen(argument) < 5 )
            {
                write_to_buffer(d,
                                "Password must be at least five characters long.\n\rPassword: ",
                                0);
                return;
            }

            pwdnew = crypt(argument, ch->names);
            for ( p = pwdnew; *p != '\0'; p++ )
            {
                if ( *p == '~' )
                {
                    write_to_buffer(d,
                                    "New password not acceptable, try again.\n\rPassword: ",
                                    0);
                    return;
                }
            }

            free_string(ch->pcdata->password);
            ch->pcdata->password = str_dup(pwdnew);
            write_to_buffer(d, "Please retype password: ", 0);
            d->connected = CON_CONFIRM_NEW_PASSWORD;
            break;

        case CON_CONFIRM_NEW_PASSWORD:
            write_to_buffer(d, "\n\r", 2);

            if (strcmp
                (crypt(argument, ch->pcdata->password),
                 ch->pcdata->password))
            {
                write_to_buffer(d,
                                "Passwords don't match.\n\rRetype password: ",
                                0);
                d->connected = CON_GET_NEW_PASSWORD;
                return;
            }

            ECHO_ON(d);
            write_to_buffer(d, "What is your sex (M/F)? ", 0);
            d->connected = CON_GET_NEW_SEX;
            break;


        case CON_GET_NEW_SEX:
            switch (argument[0])
            {
                case 'm':
                case 'M':
                    ch->sex = SEX_MALE;
                    ch->sex = SEX_MALE;
                    ch->sex = SEX_MALE;
                    break;
                case 'f':
                case 'F':
                    ch->sex = SEX_FEMALE;
                    ch->sex = SEX_FEMALE;
                    ch->sex = SEX_FEMALE;
                    break;
                default:
                    write_to_buffer(d,
                                    "That's not a sex.\n\rWhat IS your sex? ",
                                    0);
                    return;
            }

            if ( !HAS_TELOPT(d, TELOPT_TTYPE, RECV_WILL) )
            {
                write_to_buffer(d,
                                "Could not autodetect terminal type.\r\n"
                                "Would you like to use ISO6429 (\"ANSI\") colors? [Y/n/?] ",
                                0);
                d->connected = CON_Q_ISO6429;
                break;
            }

            finish_newbie(ch);
            break;

        case CON_Q_ISO6429:
            switch (LOWER(*argument))
            {
                case 'n':
                    write_to_buffer(d, "Using plain terminal type.\r\n",
                                    0);
                    d->ttype = TTYPE_PLAIN;
                    break;
                case '?':
                    write_to_buffer(d,
                                    "\x1B[1mIf this is bold/bright, you'll probably want to choose YES.\x1B[m\r\n"
                                    "Would you like to use ISO6429 (\"ANSI\") colors? [Y/n/?] ",
                                    0);
                    return;
                default:
                    write_to_buffer(d,
                                    "\x1B[1mISO6429 terminal type selected.\x1B[m\r\n",
                                    0);
                    d->ttype = TTYPE_ANSI;
                    break;
            }

            write_to_buffer(d,
                            "You can change your terminal type in the game with the TTYPE command.\r\n",
                            0);
            send_to_char("&n", ch);
            finish_newbie(ch);
            break;

        case CON_READ_NEWBIE:
            write_to_buffer(d, "\r\n", 2);
            do_help(ch, "motd");
            page_to_char("Press &WENTER&n to continue&X: &n", ch);
            show_string(d, "");

            d->connected = CON_READ_MOTD;
            break;

        case CON_READ_IMOTD:
            write_to_buffer(d, "\r\n", 2);
            do_help(ch, "motd");
            page_to_char("Press &WENTER&n to continue&X: &n", ch);
            show_string(d, "");

            d->connected = CON_READ_MOTD;
            break;

        case CON_SOLO:
            if ( *argument == 'y' || *argument == 'Y' )
            {
                logmesg("%s chose to start SOLO.", ch->names);
                write_to_buffer(d, "Going SOLO.\r\n", 0);
                ch->team = TEAM_NONE;
            }

            do_help(ch, "motd");
            page_to_char("Press &WENTER&n to continue&X: &n", ch);
            show_string(d, "");

            d->connected = CON_READ_MOTD;
            break;

        case CON_READ_MOTD:
            ch->next = char_list;
            char_list = ch;
            d->connected = CON_PLAYING;
            check_multiplayer(d->host);
            check_top_hours(d->character);

            ch->ld_timer = 0;

            send_to_char("\r\n", ch);

            REMOVE_BIT(ch->affected_by, AFF_BLIND);     /* in case they were still 
                                                           blind from previous game */

            if ( ch->trust == 0 )
            {
                if ( IS_SET(ch->act, PLR_TRAITOR ) &&
                    !traitor_autoexpire(ch))
                {
                    send_to_char
                        ("You are still an active traitor and thus placed on the &XTRAITOR&n team.\r\n",
                         ch);
                    ch->team = TEAM_TRAITOR;
                    ch->max_hit = ch->pcdata->solo_hit - 2000;
                    ch->hit = ch->max_hit;
                }
                else if ( ch->team == -1 )
                {
                    ch->team = lowestteam();
                    printf_to_char(ch,
                                   "You have joined the fight with team &W(%s&W)&n!\r\n",
                                   team_table[ch->team].who_name);
                    autoleader(ch->team, ch, NULL);
                }
            }
            else
            {
                ch->team = TEAM_ADMIN;
                autoleader(ch->team, ch, NULL);
            }

            if ( !IS_SET(ch->act, PLR_TRAITOR) )
            {
                team_table[ch->team].players++;
                ch->max_hit = ch->pcdata->solo_hit;
            }
            ch->hit = ch->max_hit;

            if (!d->adm_data ||
                !(toroom = find_location(ch, d->adm_data->room_name)))
            {
                /* Send people to the safe area if nowhere else. */
                toroom = safe_area;
            }

            char_to_room(ch, toroom);

            if ( ch->desc->ttype == TTYPE_LINUX )
                do_palette(ch, "use");

            act("$n has entered the game.", ch, NULL, NULL, TO_ROOM);
            do_look(ch, "auto");
            wiznet("$N has entered GroundZero.", ch, NULL, WIZ_LOGINS, 0,
                   get_trust(ch));
            /* check notes */
            notes = 0;

            for ( pnote = note_list; pnote != NULL; pnote = pnote->next )
                if ( is_note_to(ch, pnote ) &&
                    str_cmp(ch->names, pnote->sender) &&
                    pnote->date_stamp > ch->last_note)
                    notes++;

            if ( notes > 0 )
            {
                printf_to_char(ch,
                               "\r\nYou have %d new note%s waiting.\r\n",
                               notes, (notes == 1 ? "" : "s"));
                do_note(ch, "list -z -dun");
            }

            if ( GET_NO_PAGES(ch) > 0 )
                printf_to_char(ch,
                               "You have %d page%s waiting.  Type &WPAGE&n to see them.\r\n",
                               GET_NO_PAGES(ch),
                               (GET_NO_PAGES(ch) == 1 ? "" : "s"));
            do_poll(ch, "check");
            do_outfit(ch, "equip_on_login");
            recalc_num_on();
            break;
    }
}

/* this should be called whenever a character begins playing where they weren't
   before (ie connected becomes con_playing) */
void
recalc_num_on ()
{
    DESCRIPTOR_DATA *d;
    int i = 0;

    for ( d = descriptor_list; d != NULL; d = d->next )
        if ( d->connected == CON_PLAYING )
            if ( !IS_IMMORTAL(d->character) )
                i++;

    if ( (i % 20) == 0 && i > max_on )
    {
        CHAR_DATA *all_chars;
        char buf[MAX_STRING_LENGTH];

        sprintf(buf,
                "%d players on!!  Parachutes fill the air as new "
                "supplies are dropped from cargo planes above.\r\nScouts "
                "discover new parts of the city that have yet to be "
                "explored.\r\n", i);

        for ( all_chars = char_list; all_chars; all_chars = all_chars->next )
            send_to_char(buf, all_chars);

        expand_city();
        expansions++;
        scatter_objects();
    }

    max_on = UMAX(i, max_on);
}


void
check_multiplayer (const char *host)
{
    char names[MAX_STRING_LENGTH];
    struct descriptor_data *d;
    register int count = 0;
    register int k = 0;

    for ( d = descriptor_list; d; d = d->next )
        if ( !str_cmp(host, d->host) && d->connected == CON_PLAYING )
        {
            k += sprintf(names + k, " %s", d->character->names);
            count++;
        }

    if ( count > 1 )
        logmesg("MULTIPLAYING FROM SITE `%s'. %d CHARACTERS:%s.", host,
                   count, names);
}


bool
traitor_autoexpire (CHAR_DATA * ch)
{
    time_t ct = time(0);

    /* 7200 is 60 seconds per minute, 60 minutes per hour, 2 hours (60*60*2) */
    if ( !IS_SET(ch->act, PLR_TRAITOR ) ||
        (ct - ch->pcdata->time_traitored) < 7200)
        return (FALSE);
    REMOVE_BIT(ch->act, PLR_TRAITOR);
    return (TRUE);
}


/*
 * Parse a name for acceptability.
 */
bool
check_parse_name (char *name)
{
    char *pc;

    /* Reserved words. */
    if ( !str_prefix("self", name) )
        return FALSE;

    if (is_name(name, "red blue commbadge alpha bravo all auto immortal "
                "self someone enforcer something the you god safe "
                "list out map"))
        return FALSE;

    /* Length restrictions. */
    if ( strlen(name) < 2 )
        return FALSE;
    if ( strlen(name) > 11 )
        return FALSE;

    for ( pc = name; *pc != '\0'; pc++ )
        if ( !isalpha(*pc) )
            return FALSE;

    return TRUE;
}


/*
 * Look for link-dead player to reconnect.
 */
bool
check_reconnect (DESCRIPTOR_DATA * d, char *name, bool fConn)
{
    DESCRIPTOR_DATA *dsoft, *dsoft_n;
    CHAR_DATA *ch;

    /*
     * Look for soft reboot reconnects.  If one's there, kill it.
     */
    for ( dsoft = descriptor_list; dsoft; dsoft = dsoft_n )
    {
        dsoft_n = dsoft->next;

        if ( dsoft == d || dsoft->connected != CON_RELOG )
            continue;
        if ( !dsoft->character || !dsoft->character->names )
            continue;
        if ( str_cmp(dsoft->character->names, name) )
            continue;

        write_to_descriptor(dsoft,
                            "\r\nDetected another connection attempt.  Relog aborted.\r\n",
                            0, FALSE);
        close_socket(dsoft);
    }

    for ( ch = char_list; ch != NULL; ch = ch->next )
    {
        if ( !IS_NPC(ch )
            && (!fConn || ch->desc == NULL)
            && !str_cmp(d->character->names, ch->names))
        {
            if ( fConn == FALSE )
            {
                free_string(d->character->pcdata->password);
                d->character->pcdata->password =
                    str_dup(ch->pcdata->password);
            }
            else if ( ch->in_room != graveyard_area )
            {
                free_char(d->character);

                d->character = ch;

                if ( d->adm_data )
                {
                    d->character->trust = d->adm_data->trust;
                    d->character->max_hit = ch->pcdata->solo_hit;
                }
                else
                {
                    d->character->trust = 0;

                    if ( IS_SET(ch->act, PLR_TRAITOR) )
                        ch->max_hit = ch->pcdata->solo_hit - 2000;
                    else
                        ch->max_hit = ch->pcdata->solo_hit;
                }

                ch->desc = d;
                ch->timer = 0;
                send_to_char("Reconnecting.\n\r", ch);
                act("$n has reconnected.", ch, NULL, NULL, TO_ROOM);
                sprintf(log_buf, "%s@%s reconnected.", ch->names, d->host);
                logmesg("%s", log_buf);
                wiznet(log_buf, NULL, NULL, WIZ_SITES, 0, get_trust(ch));
                d->connected = CON_PLAYING;
                ch->ld_timer = 0;
                recalc_num_on();
            }
            else
            {                   /* The other character is in the graveyard. */
                OBJ_DATA *obj;

                /* Snag their owned items. */
                while ( (obj = ch->owned_objs) != NULL )
                {
                    disownObject(obj);
                    claimObject(d->character, obj);
                }

                sprintf(log_buf,
                        "Bubba, call the doctor, looks like %s is Lazarus!",
                        ch->names);
                logmesg("%s", log_buf);
                wiznet(log_buf, NULL, NULL, WIZ_SITES, 0, get_trust(ch));
                return (FALSE);
            }

            return TRUE;
        }
    }

    return FALSE;
}



/*
 * Check if already playing.
 */
bool
check_playing (DESCRIPTOR_DATA * d, char *name)
{
    DESCRIPTOR_DATA *dold;

    for ( dold = descriptor_list; dold; dold = dold->next )
    {
        if (dold != d
            && dold->character != NULL
            && dold->connected != CON_GET_NAME
            && dold->connected != CON_GET_OLD_PASSWORD
            && !str_cmp(name,
                        dold->original ? dold->original->names : dold->
                        character->names))
        {
            write_to_buffer(d, "That character is already playing.\n\r",
                            0);
            write_to_buffer(d, "Do you wish to connect anyway (Y/N)?", 0);
            d->connected = CON_BREAK_CONNECT;
            return TRUE;
        }
    }

    return FALSE;
}

/*
 * Write to one char   TJT.
 */
/*   void send_to_char( const char *txt, CHAR_DATA *ch )
   {
   if (  txt != NULL && ch->desc != NULL  )
   write_to_buffer( ch->desc, txt, strlen(txt) );
   return;
   }
 */
/*
 * Send a page to one char.
 */
void
page_to_char (const char *txt, CHAR_DATA * ch)
{
    char *tmp_head, *tmp_point;

    if ( txt == NULL || ch->desc == NULL )
        return;

    if ( !ch->desc || !ch->desc->lines )
    {
        send_to_char(txt, ch);
        return;
    }

    if ( ch->desc->showstr_head != NULL )
    {
        tmp_head = ch->desc->showstr_head;
        tmp_point = ch->desc->showstr_point;
        ch->desc->showstr_head =
            alloc_mem(strlen(txt) + strlen(ch->desc->showstr_point) + 1);
        strcpy(ch->desc->showstr_head, tmp_point);
        strcpy(ch->desc->showstr_head + strlen(tmp_point), txt);
        ch->desc->showstr_point = ch->desc->showstr_head;
        free_string(tmp_head);
    }
    else
    {
        ch->desc->showstr_head = alloc_mem(strlen(txt) + 1);
        strcpy(ch->desc->showstr_head, txt);
        ch->desc->showstr_point = ch->desc->showstr_head;
    }
}


/* string pager */
void
show_string (struct descriptor_data *d, char *input)
{
    char buffer[4 * MAX_STRING_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    register char *scan, *chk;
    int lines = 0, toggle = 1;
    int show_lines;

    if ( !d || !d->lines )
        return;

    one_argument(input, buf);

    if ( buf[0] != '\0' )
    {
        if ( d->showstr_head )
        {
            free_string(d->showstr_head);
            d->showstr_head = 0;
        }
        d->showstr_point = 0;
        return;
    }

    show_lines = d->lines - 2;

    for ( scan = buffer;; scan++, d->showstr_point++ )
    {
        if ( ((*scan = *d->showstr_point) == '\n' || *scan == '\r' ) &&
            (toggle = -toggle) < 0)
            lines++;
        else if ( !*scan || (show_lines > 0 && lines >= show_lines) )
        {
            *scan = '\0';
            write_to_buffer(d, buffer, 0);
            for ( chk = d->showstr_point; isspace(*chk); chk++ ) ;

            if ( !*chk )
            {
                if ( d->showstr_head )
                {
                    free_string(d->showstr_head);
                    d->showstr_head = 0;
                }
                d->showstr_point = 0;
            }

            return;
        }
    }
}


/* quick sex fixer */
void
fix_sex (CHAR_DATA * ch)
{
    if ( ch->sex < 0 || ch->sex > 2 )
        ch->sex = IS_NPC(ch) ? 0 : ch->sex;
}

void
act (const char *format, CHAR_DATA * ch, const void *arg1, const void *arg2,
    int type)
{
    /* to be compatible with older code */
    act_new(format, ch, arg1, arg2, type, POS_RESTING);
}


char *
act_parse (const char *fmt, CHAR_DATA * to,
          CHAR_DATA * ch, const void *arg1, const char *arg2)
{
    static char *const he_she[] = { "it", "he", "she" };
    static char *const him_her[] = { "it", "him", "her" };
    static char *const his_her[] = { "its", "his", "her" };

    int remainingSpace = MAX_STRING_LENGTH - 3;
    static char actbuf[MAX_STRING_LENGTH];
    CHAR_DATA *och = (CHAR_DATA *) arg2;
    OBJ_DATA *obj1 = (OBJ_DATA *) arg1;
    OBJ_DATA *obj2 = (OBJ_DATA *) arg2;
    char i[MAX_STRING_LENGTH];
    char *write_pt = actbuf;
    char *iter_i;

    if ( !to )
        return (NULL);

    if ( !fmt || !*fmt || !strchr(fmt, '$') )
    {
        strncpy(actbuf, fmt, remainingSpace);
        strcat(actbuf, "\r\n");
        return (actbuf);
    }
    else if ( ch && ch->valid != VALID_VALUE )
        return (NULL);

    for ( ; remainingSpace > 0; )
    {
        if ( *fmt == '$' )
        {
            fmt++;
            strcpy(i, " <@@@> ");

            switch (*fmt)
            {
                case 't':
                    strcpy(i, (char *) arg1);
                    break;
                case 'T':
                    strcpy(i, (char *) arg2);
                    break;
                case 'r':
                    sprintf(i, "%s%s&n", team_table[to->team].namecolor,
                            PERS(to, to));
                    break;
                case 'R':
                    sprintf(i, "%s&n", team_table[to->team].who_name);
                    break;
                case 'n':
                    if ( ch )
                        sprintf(i, "%s%s&n",
                                team_table[ch->team].namecolor, PERS(ch,
                                                                     to));
                    break;
                case 'N':
                    if ( och && och->valid == VALID_VALUE )
                        sprintf(i, "%s%s&n",
                                team_table[och->team].namecolor, PERS(och,
                                                                      to));
                    break;
                case 'e':
                    if ( ch )
                        strcpy(i, he_she[URANGE(0, ch->sex, 2)]);
                    break;
                case 'E':
                    if ( och && och->valid == VALID_VALUE )
                        strcpy(i, he_she[URANGE(0, och->sex, 2)]);
                    break;
                case 'm':
                    if ( ch )
                        strcpy(i, him_her[URANGE(0, ch->sex, 2)]);
                    break;
                case 'M':
                    if ( och && och->valid == VALID_VALUE )
                        strcpy(i, him_her[URANGE(0, och->sex, 2)]);
                    break;
                case 's':
                    if ( ch )
                        strcpy(i, his_her[URANGE(0, ch->sex, 2)]);
                    break;
                case 'S':
                    if ( och && och->valid == VALID_VALUE )
                        strcpy(i, his_her[URANGE(0, och->sex, 2)]);
                    break;
                case 'a':
                    if ( ch )
                    {
                        strcpy(i, team_table[ch->team].who_name);
                        strcat(i, "&n");
                    }
                    break;
                case 'A':
                    if ( och && och->valid == VALID_VALUE )
                    {
                        strcpy(i, team_table[och->team].who_name);
                        strcat(i, "&n");
                    }
                    break;
                case 'p':
                    if ( obj1 && obj1->valid == VALID_VALUE )
                        sprintf(i, "&uO%s&n", obj1->short_descr);
                    break;
                case 'P':
                    if ( obj2 && obj2->valid == VALID_VALUE )
                        sprintf(i, "&uO%s&n", obj2->short_descr);
                    break;
                case '$':
                    strcpy(i, "$$");
                    break;
                default:
                    logmesg("Invalid act code $%c.", *fmt);
                    break;
            }

            iter_i = i;

            while ( (*write_pt = *(iter_i++)) && remainingSpace > 0 )
            {
                write_pt++;
                remainingSpace--;
            }

            fmt++;
        }
        else if ( (*write_pt = *(fmt++)) == '\0' )
            break;
        else
        {
            remainingSpace--;
            write_pt++;
        }
    }

    *(write_pt++) = '\r';
    *(write_pt++) = '\n';
    *(write_pt++) = '\0';
    *actbuf = UPPER(*actbuf);
    return (actbuf);
}


void
act_new (const char *format, CHAR_DATA * ch, const void *arg1,
        const void *arg2, int type, int min_pos)
{
    bool isSocial = (type & TO_NOT_NOSOCIALS);
    CHAR_DATA *vch = (CHAR_DATA *) arg2;
    CHAR_DATA *to;

    if ( !format || !*format )
        return;

    type &= ~TO_NOT_NOSOCIALS;

    switch (type)
    {
        case TO_CHAR:
            to = ch;
            break;
        case TO_VICT:
            to = vch;
            break;
        case TO_ROOM:
            if ( !ch || ch->valid != VALID_VALUE || !ch->in_room )
            {
                logmesg("Act(TO_ROOM): NULL, invalid, or last ch.");
                return;
            }

            to = ch->in_room->people;
            break;
        case TO_NOTVICT:
            if ( !vch || vch->valid != VALID_VALUE || !vch->in_room )
            {
                logmesg("Act(TO_NOTVICT): Null, invalid, or lost vch.");
                return;
            }

            to = vch->in_room->people;
            break;
        default:
            logmesg("Act(%d): Unrecognized to type.", type);
            return;
    }

    for ( ; to; to = to->next_in_room )
    {
        if ( to->desc == NULL || to->position < min_pos )
            continue;

        if ( type == TO_CHAR && to != ch )
            continue;
        if ( type == TO_VICT && (to != vch || to == ch) )
            continue;
        if ( type == TO_ROOM && to == ch )
            continue;
        if ( type == TO_NOTVICT && (to == ch || to == vch) )
            continue;
        if ( isSocial && IS_SET(to->comm, COMM_NOSOCIALS) )
            continue;

        send_to_char(act_parse(format, to, ch, arg1, arg2), to);
    }
}


void
printf_to_char (CHAR_DATA * ch, char *fmt, ...)
{
    char buf[MAX_STRING_LENGTH];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    send_to_char(buf, ch);
}


void
clear_screen (struct char_data *ch)
{
    if ( IS_NPC(ch) || !ch->desc || !hascolor(ch) )
        return;

    write_to_descriptor(ch->desc, "\e[H\e[J", 0, FALSE);
    ch->desc->fcommand = TRUE;
}
