#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "networks.h"


int g_addr_len = sizeof( struct sockaddr_in );


int ep_address_fromstring( char* p, int len, int defaultport, ep_address* epaddr )
{
    int i ;
    char* port_p;
    char domain[255];
    struct hostent *hp;
    
    for ( i = 0, port_p = p ; 
          *port_p != '\0' && ( len == 0 || i < len ) ; 
          i++, port_p++ )
    {
        if ( *port_p == ':' )
        {
            break;
        }
    }
    
    if ( i > 255 )
    {
        return -1;
    }
    
    if (  *port_p == ':' )
    {
        port_p ++;
        
        if ( *port_p == '\0' )
        {
            return -1;
        }
        
        defaultport = strtol( port_p, NULL, 10 );
        
    }
    else
    {
        if ( defaultport == 0 )
        {
            return -1;
        }
    }
    
    //printf( " i: %d", i );
    
    strncpy( domain, p, i );
    domain[i] = '\0';
    
    //puts(domain);
    
    hp = gethostbyname( domain );
    
    if ( hp == NULL || hp->h_length <= 0 )
    {
        return -1;
    }
    
    //strcpy( hp->h_name, epaddr->hostname );
    strcpy( epaddr->hostname, hp->h_name );
    
    
    epaddr->addr.sin_family = AF_INET;
    if ( defaultport == -1 )
    {
        memcpy( &(epaddr->addr.sin_port), p+len, 2 );
    }
    else
    {
        epaddr->addr.sin_port = htons( defaultport );
    }
    epaddr->addr.sin_addr.s_addr = *((unsigned long *)hp->h_addr_list[0]);
    
    //inet_pton( AF_INET, addr, &(p_remote_addr->sin_addr) );
    
    bzero( (void*)(&(epaddr->addr.sin_zero)), 8 );
    
    return 0;
}

int ep_address_inaddr_any( int port, ep_address* epaddr  )
{
    epaddr->hostname[0] = '\0';
    
    epaddr->addr.sin_family = AF_INET;
    epaddr->addr.sin_port = htons( port );
    epaddr->addr.sin_addr.s_addr = INADDR_ANY;
    
    bzero( (void*)(&(epaddr->addr.sin_zero)), 8 );
    
    return 0;
}

int ep_address_frombinary( char* h, char* p, ep_address* epaddr )
{
    epaddr->addr.sin_family = AF_INET;
    
    memcpy( &(epaddr->addr.sin_port), p, 2 );
    memcpy( &(epaddr->addr.sin_addr.s_addr), h, 4 );
    
    bzero( (void*)(&(epaddr->addr.sin_zero)), 8 );
    
    return 0;
}

int ep_address_fromfd( int fd, ep_address* epaddr  )
{
    
    int len = sizeof( struct sockaddr_in );
    
    getsockname( fd, ( struct sockaddr *) &(epaddr->addr), &len );
    
    return 0;
}

int ep_address_tobinary( ep_address* epaddr, char* p, char* h )
{
    //memcpy( p, &(epaddr->addr.sin_port), 6 );
    
    memcpy( p , &(epaddr->addr.sin_port), 2 );
    
    if ( h == NULL )
    {
        memcpy( p+2 , &(epaddr->addr.sin_addr.s_addr), 4 );
    }
    else
    {
        memcpy( h , &(epaddr->addr.sin_addr.s_addr), 4 );
    }
    
    return 0;
}

int ep_address_tostring_pretty( ep_address* epaddr, char* p, int size )
{
    int i = 0;
    
    if ( epaddr->hostname[0] != '\0' )
    {
        strncpy( p, epaddr->hostname, size );
        i = strlen( epaddr->hostname );
        p[i] = ' ';
        i ++ ;
        size = size - i - 1;
    }
    
    inet_ntop(AF_INET, &(epaddr->addr.sin_addr), p+i, size);
    
    i += strlen( p+i );
    
    sprintf( p+i, ":%d", ntohs(epaddr->addr.sin_port) );
    
    return 0;
}

int create_listen_socket( ep_address* epaddr )
{
    int listen_fd;
    int flag;
    
    if ( ( listen_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
    {
        perror("create socket error");
        exit(1);
    }
    
    if ( setsockopt( 
            listen_fd, SOL_SOCKET, SO_REUSEADDR, 
            (char *)&flag, sizeof(flag) ) == -1 )
    {
        perror("setsockopt error");
    }
    
    int flags = fcntl(listen_fd, F_GETFL, 0);
    
    fcntl(listen_fd, F_SETFL, flags|O_NONBLOCK);
    
    //bzero( (void*)(&(epaddr->addr.sin_zero)), 8 );
    
    if ( bind( listen_fd, (struct sockaddr *)&(epaddr->addr), g_addr_len ) == -1 )
    {
        perror("bind error");
        exit(1);
    }
    
    if ( listen( listen_fd, 1 ) == -1 )
    {
        perror("listen error");
        exit(1);
    }
    
    return listen_fd;
}


int accept_socket( int listen_fd, ep_address* epaddr )
{
    int accept_fd ;
    int flags ;
    
    //bzero( (void*)(&(epaddr->addr.sin_zero)), 8 );
    
    accept_fd = accept( listen_fd, (struct sockaddr *)&(epaddr->addr), 
                        &g_addr_len );
    
    flags = fcntl(accept_fd, F_GETFL, 0);
    
    fcntl( accept_fd, F_SETFL, flags|O_NONBLOCK );
    
    epaddr->hostname[0] = '\0';
    
    return accept_fd;
}


int connect_socket( ep_address* epaddr )
{
    int connect_fd;
    
    connect_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if ( connect( connect_fd, 
                  (struct sockaddr *)&(epaddr->addr), 
                  sizeof(struct sockaddr_in)
            ) == -1 )
    {
        printf( "connection error (%d)", errno );
        return -1;
    }
    
    return connect_fd;
}
