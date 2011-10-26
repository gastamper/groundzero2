/*
 * Module:      net.h
 * Owner:       Satori
 * Checked-in:  07 Oct, 2000
 * Version:     0.0.0a
 *
 * General header file for inclusion of the generic networking stuff as well
 * as including system header files for networking.
 */

#ifndef GZ2_NET_H_
#define GZ2_NET_H_

#include    <arpa/inet.h>
#include    <arpa/telnet.h>
#include    <fcntl.h>
#include    <netinet/in.h>
#include    <sys/socket.h>

#define     net_write(_1, _2)         net_bwrite((_1), (_2), strlen((_2)))

#define     SockOpt(sock_, opt_, val_)                              \
    ({                                                              \
        setsockopt((sock_), SOL_SOCKET, (opt_),                     \
                   (char *) &(val_), sizeof(val_));                 \
    })

int net_nonblock(int);
int net_bwrite(int, const char *, int);
int net_read(int, char *, int);

#endif                          /* GZ2_NET_H_ */
