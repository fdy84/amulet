#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "socket_oo.h"


SockAddrObj* SockAddrObj_fromfd( SockAddrObj* self, int fd )
{
    int sockaddrlen = sizeof(struct sockaddr_in);
    
    // create SockAddrObj if self is NULL
    if ( self == NULL )
    {
        self = (SockAddrObj*)malloc(sizeof(SockAddrObj));
        if ( self == NULL )
            return NULL;
    }
    
    // fill SockAddrObj Info
    if (getsockname( fd, (struct sockaddr *)&(self->addr), &sockaddrlen ) == 0)
        return self;
    
    return NULL;
}


SockAddrObj* SockAddrObj_fromstring( SockAddrObj* self, char* address, int len, long defaultport )
{
    int sockaddrlen = sizeof(struct sockaddr_in);
    
    int i;
    char* c; 
    char* endaddr;
    char domain[256];
    struct hostent *hp;
    long port;

    
    // parse the address
    if ( len >= 256 )
        return NULL;
    
    endaddr = address + ( ( len < 0 ) ? 256 : (len+1) );
    
    for ( c = address; *c != '\0' && *c != ':' && c < endaddr ; c++ );
    
    if ( *c == ':' )
    {
        if ( c[1] == '\0' ) return NULL;
        
        port = strtol( c+1, NULL, 10 );
        
        if ( port == LONG_MIN || port == LONG_MAX ) return NULL;
    }
    else
    {
        port = defaultport;
        
        if ( port < 0 || port >= 65536 ) return NULL;
    }
    
    i = c-address-1;
    strncpy( domain, address, i );
    domain[i] = '\0';

    hp = gethostbyname( domain );
    if ( hp == NULL || hp->h_length <= 0 ) return NULL;


    // create SockAddrObj if self is NULL
    if ( self == NULL )
    {
        self = (SockAddrObj*)malloc(sizeof(SockAddrObj));
        if ( self == NULL )
            return NULL;
    }

    
    // fill SockAddrObj Info
    strcpy( self->hostname, hp->h_name );
    
    self->addr.sin_family = AF_INET;
    self->addr.sin_port = htons( port );
    self->addr.sin_addr.s_addr = *((unsigned long *)hp->h_addr_list[0]);
    bzero( (void*)(&(self->addr.sin_zero)), 8 );
    
    return self;
}


SockAddrObj* SockAddrObj_frombinary( SockAddrObj* self, char* p, char* h )
{
    
    if ( h == NULL ) h = p + 2;
    
    // create SockAddrObj if self is NULL
    if ( self == NULL )
    {
        self = (SockAddrObj*)malloc(sizeof(SockAddrObj));
        if ( self == NULL )
            return NULL;
    }
    
    // fill SockAddrObj Info
    self->addr.sin_family = AF_INET;
    
    memcpy( &(self->addr.sin_port), p, 2 );
    memcpy( &(self->addr.sin_addr.s_addr), h, 4 );
    
    bzero( (void*)(&(self->addr.sin_zero)), 8 );
    
    return self;
}




char* SockAddrObj_tostring( SockAddrObj* self, char* address, int maxlength )
{
    int i = 0;
    
    if ( self->hostname[0] != '\0' )
        i = strlen( strncpy( address, self->hostname, maxlength ) );
    else
        i = strlen( inet_ntop(AF_INET, &(self->addr.sin_addr), address, maxlength) );
    
    if ( i == maxlength ) return address;
    
    snprintf( address+i, maxlength-i, ":%d", ntohs(self->addr.sin_port) );
    
    return address;
}


char* SockAddrObj_tobinary( SockAddrObj* self, char* p, char* h )
{
    if ( h == NULL ) h = p + 2;
    
    memcpy( p , &(self->addr.sin_port), 2 );
    memcpy( h , &(self->addr.sin_addr.s_addr), 4 );
    
    return p;
}


char g_SockAddrObj_errorbox[][50] = {
    "Not Defined Error: ",
    "Create Socket Error (socket): ",
    "Manipulating Socket Options Error (setsockopt): ",
    "Bind Socket Error (bind): ",
    "Listen Socket Error (listen): ",
};

char* SockAddrObj_printerror( SockAddrObj* self, int errno, char* msgbuf, int maxlength )
{
    
    int i = 0;
    char* tp = msgbuf;
    
    if ( errno > 0 || errno <= -5 ) errno = 0;
    errno = -errno;
    
    i = strlen( strncpy( tp, g_SockAddrObj_errorbox[errno], maxlength ) );
    
    if ( i == maxlength )
        return msgbuf;
    else
        tp = tp + i;
        maxlength = maxlength-i;

    if ( self->hostname[0] != '\0' )
        i = strlen( strncpy( tp, self->hostname, maxlength ) );

    if ( i == maxlength )
        return msgbuf;
    else
        tp = tp + i;
        maxlength = maxlength-i;

    i = strlen( inet_ntop(AF_INET, &(self->addr.sin_addr), tp, maxlength) );
    
    if ( i == maxlength )
        return msgbuf;
    else
        tp = tp + i;
        maxlength = maxlength-i;
    
    snprintf( tp, maxlength, ":%d", ntohs(self->addr.sin_port) );
    
    return msgbuf;
}


int SockAddrObj_bind_and_listen( SockAddrObj* self )
{
    int sockaddrlen = sizeof(struct sockaddr_in);
    
    int listen_fd;
    int flag, flags;
    
    if ( ( listen_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
        return -1;
    
    if ( setsockopt( 
            listen_fd, SOL_SOCKET, SO_REUSEADDR, 
            (char *)&flag, sizeof(flag) ) == -1 )
        return -2;
    
    flags = fcntl(listen_fd, F_GETFL, 0);
    
    fcntl(listen_fd, F_SETFL, flags|O_NONBLOCK);
    
    if ( bind( listen_fd, (struct sockaddr *)&(self->addr), sockaddrlen ) == -1 )
        return -3;
    
    if ( listen( listen_fd, 1 ) == -1 )
        return -4;
    
    return listen_fd;
}


SockAddrObj* SockAddrObj_accept( SockAddrObj* self, int listen_fd, int* p_accept_fd )
{
    int sockaddrlen = sizeof(struct sockaddr_in);
    
    int accept_fd;
    int flags;
    
    // create SockAddrObj if self is NULL
    if ( self == NULL )
    {
        self = (SockAddrObj*)malloc(sizeof(SockAddrObj));
        if ( self == NULL )
            return NULL;
    }
    
    accept_fd = accept( listen_fd, (struct sockaddr *)&(self->addr), &sockaddrlen );
    flags = fcntl(accept_fd, F_GETFL, 0);
    fcntl( accept_fd, F_SETFL, flags|O_NONBLOCK );
    
    self->hostname[0] = '\0';
    
    *p_accept_fd = accept_fd;
    
    return self;
}

int SockAddrObj_connect( SockAddrObj* self, int timeout )
{
    int sockaddrlen = sizeof(struct sockaddr_in);
    
    int connect_fd;
    
    connect_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if ( connect( connect_fd, (struct sockaddr *)&(self->addr), sockaddrlen ) == -1 )
        return -5;
    
    return connect_fd;
}
