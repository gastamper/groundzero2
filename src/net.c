/*
 * Module:      net.c
 * Owner:       Satori
 * Checked-in:  31 Jul, 2002
 * Version:     0.1.0
 *
 * Generic networking routines that can be used throughout the GZ code for
 * various different servers, etc.  The eventual goal is to have a fairly
 * abstract means by which we can easily create servers.
 */

#include    "ground0.h"
#include    "net.h"


int net_nonblock (int fd)
{
    int opt = fcntl(fd, F_GETFL);
    return fcntl(fd, F_SETFL, opt | O_NONBLOCK);
}


#if 0
int net_server (const char *bindto, uint16_t port)
{
    struct sockaddr_in host;
    int sockfd;

    if ( !bindto || !*bindto )
        host.sin_addr.s_addr = htonl(INADDR_ANY);
    else if ( inet_aton(bindto, &host.sin_addr) == 0 )
    {
        logmesg("%s: cannot find address to bind to", bindto);
        return -1;
    }

    host.sin_family = AF_INET;
    host.sin_port = htons(port);

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        logerr("net_server");
        return -1;
    }
    else if ( net_nonblock(sockfd) == -1 )
    {
        logerr("net_nonblock");
        close(sockfd);
        return -1;
    }
    else if ( SockOpt(sockfd, SO_REUSEADDR, 1) )
    {
        logerr("SO_REUSEADDR");
        close(sockfd);
        return -1;
    }
    else if ( SockOpt(sockfd, SO_LINGER, ((struct linger) {0,0})) == -1 )
    {
        logerr("SO_LINGER");
        close(sockfd);
        return -1;
    }
    else if ( bind(sockfd, (struct sockaddr *) &host, sizeof(host)) == -1 )
    {
        logerr("bind(%s:%d)", (bindto && *bindto ? bindto : "*"), port);
        close(sockfd);
        return -1;
    }

    listen(sockfd, 5);
    return sockfd;
}
#endif


int
net_bwrite (int fd, const char *str, int total)
{
    register char *ptr;
    int bytes;

    if ( total == 0 )
        total = strlen(str);

    ptr = (char *) str;

    while ( total > 0 )
    {
        bytes = write(fd, ptr, total);

        if ( bytes == -1 )
        {
            if ( errno == EINTR || errno == EAGAIN )
                break;
            logerr("write()");
            return -1;
        }
        else if ( bytes == 0 )
        {
            logmesg("wrote zero bytes -- this shouldn't happen");
            return -1;
        }

        total -= bytes;
        ptr += bytes;
    }

    return (strlen(str) - total);
}


int
net_read (int fd, char *buf, int n)
{
    register char *ptr = buf;
    int total = 0;
    int bytes;

    while ( total < n )
    {
        bytes = read(fd, ptr, n - total);

        if ( bytes == -1 )
        {
            if ( errno == EINTR || errno == EAGAIN )
                break;
            logerr("read()");
            return -1;
        }
        else if ( bytes == 0 )
        {
            logmesg("Connection closed by peer.  EOF on read.");
            return -1;
        }

        ptr += bytes;
        total += bytes;
    }

    return total;
}
