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
 * Module:      dns.c
 * Synopsis:    Interface to libares.
 * Author:      Satori
 * Created:     09 Dec 2001
 *
 * NOTES:
 *
 * Add lib/ares/libares.a to the link pass in Makefile.
 */

#include "ground0.h"

#include <ares.h>
#include <netdb.h>


struct dns_query_data
{
    unsigned int reuse:1;
    struct descriptor_data *d;
    struct dns_query_data *next;
};


static ares_channel x_aresChannel;
static struct dns_query_data *x_recycleQuery = NULL;
static struct dns_query_data *x_headQuery = NULL;

static void x_DnsCallback(void *, int status, int timeouts, struct hostent *);
static struct dns_query_data *x_MakeQueryData(struct descriptor_data *);
static void x_DestroyQueryData(struct dns_query_data *);


void CompleteLogin(struct descriptor_data *);


int
ResolvInitialize (void)
{
    char *errstr;
    int status;

    if ( (status = ares_init(&x_aresChannel)) != ARES_SUCCESS )
    {
		errstr = (char *) ares_strerror(status);
        logmesg("ares_init: %s\n", errstr);
        ares_free_string((void *) errstr);
        return -1;
    }

    return 0;
}


int
ResolvQuery (struct descriptor_data *d, struct in_addr ia4)
{
    if ( d->dns_query )
    {
        logmesg("Trying two separate queries on same descriptor.");
        return -1;
    }

    d->dns_query = x_MakeQueryData(d);
    ares_gethostbyaddr(x_aresChannel, &ia4, sizeof(ia4), AF_INET,
                       x_DnsCallback, d->dns_query);
    return 0;
}


int
ResolvCancel (struct descriptor_data *d)
{
    if ( !d->dns_query )
        return -1;
    x_DestroyQueryData(d->dns_query);
    d->dns_query = NULL;
    return 0;
}


void
ResolvProcess (void)
{
    fd_set ifs;
    fd_set ofs;

    FD_ZERO(&ifs);
    FD_ZERO(&ofs);

    int nfds = ares_fds(x_aresChannel, &ifs, &ofs);

    if ( nfds == 0 )
        return;

    struct timeval tv;

    tv.tv_sec = tv.tv_usec = 0;
    struct timeval *tvp = ares_timeout(x_aresChannel, NULL, &tv);

    tv.tv_sec = tv.tv_usec = 0;

    if ( tvp == NULL )
        return;

    select(nfds, &ifs, NULL, &ofs, tvp);
    ares_process(x_aresChannel, &ifs, &ofs);
}


void
ResolvTerminate (void)
{
    ares_destroy(x_aresChannel);
}


/** HELPER FUNCTIONS AND CALLBACKS *******************************************/

static void
x_DnsCallback (void *arg, int status, int timeouts, struct hostent *he)
{
    struct dns_query_data *dqd = arg;

    /* Throw-away result, mark dqd as reuseable. */
    if ( dqd->d == NULL )
    {
        dqd->reuse = 1;
        return;
    }

    if ( status != ARES_SUCCESS )
    {
        // Log spam:
        //logmesg("Couldn't lookup IP '%s': %s", dqd->d->host,
        //           ares_strerror(status, &errstr));
        // ares_free_errmem(errstr);
    }
    else
    {
        // Log spam:
        // logmesg("Resolv: %s -> %s", dqd->d->host, he->h_name);
        strncpy(dqd->d->host, he->h_name, 79);
        dqd->d->host[79] = '\0';
    }

    struct descriptor_data *d = dqd->d;

    x_DestroyQueryData(dqd);
    CompleteLogin(d);
}


static struct dns_query_data *
x_MakeQueryData (struct descriptor_data *d)
{
    struct dns_query_data *dqd = x_recycleQuery;
    struct dns_query_data *prev = NULL;

    while ( dqd && !dqd->reuse )
    {
        prev = dqd;
        dqd = dqd->next;
    }

    if ( dqd )
    {                           /* Recycle one. */
        if ( prev == NULL )
            x_recycleQuery = dqd->next;
        else
            prev->next = dqd->next;
    }
    else                        /* ...Or create one. */
        dqd = alloc_perm(sizeof(struct dns_query_data));

    dqd->reuse = 0;
    dqd->d = d;
    dqd->next = x_headQuery;
    x_headQuery = dqd;
    return dqd;
}


static void
x_DestroyQueryData (struct dns_query_data *dqd)
{
    if ( dqd == x_headQuery )
        x_headQuery = dqd->next;
    else
    {
        struct dns_query_data *tmp = x_headQuery;

        while ( tmp && tmp->next != dqd )
            tmp = tmp->next;
        if ( !tmp )
            return;
        tmp->next = dqd->next;
    }

    dqd->d = NULL;
    dqd->next = x_recycleQuery;
    x_recycleQuery = dqd;
}
