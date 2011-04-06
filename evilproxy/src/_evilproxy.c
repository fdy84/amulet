// a single header file is required

#include <stdio.h> // for puts
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>

#include <ev.h>

#include "networks.h"
#include "base.h"

#include "evilproxy.h"


#define EPT_NULL     -1
#define EPT_LOCAL     0
#define EPT_DYNAMIC   1

#define EPP_UserNameLen   512
#define EPP_BufferLen   10240

#define EPP_NULL     -5
#define EPP_SOCK5    -4
#define EPP_READY     0 // Not Ready Stat must be less than 0
#define EPP_LEFT      1
#define EPP_RIGHT     2


#define SOCKS_P_ERROR    -1
#define SOCKS_P_OK        0 // Error must be less than 0
#define SOCKS_P_CONTINUE  1




typedef int (*EPAPI_ON_PROXY)( ep_address*, void** );
typedef int (*EPAPI_ON_TRANS)( void* , char* );



struct _stt_ep_listen;

struct _stt_ep
{
    
    int proxytype;
    ep_address localaddr;
    ep_address proxyaddr;
    
    int listen_fd;
    struct ev_loop *loop;
    
    ep_listen* listen_watcher;
    
    void* data;
    
    EPAPI_ON_PROXY usr_proxy;
    EPAPI_ON_TRANS usr_trans;
    
};

typedef struct _stt_ep EVILPROXY;



struct _stt_ep_listen
{
    ev_io listen;
    
    EVILPROXY* ep;
    
};

typedef struct _stt_ep_listen ep_listen;



struct _stt_ep_proxy
{
    int stat;
    char user[EPP_UserNameLen];
    char buf[EPP_BufferLen];
    int len;
    
    int left_fd;
    int right_fd;
    
    EVILPROXY* ep;
    
    ep_address left_local_addr;
    ep_address left_remote_addr;
    ep_address right_local_addr;
    ep_address right_remote_addr;
    
    ev_io left;
    ev_io right;
    
};


typedef struct _stt_ep_proxy ep_proxy;


int socks_v5_pre( ep_proxy* ep )
{
    int i;
    int fd;
    
    if ( ep->len < 2 || ep->len < ( ep->buf[1] +2 ) )
    {
        return SOCKS_P_CONTINUE;
    }
    
    if ( ep->buf[0] != 5 || ep->buf[1] == 0 || ep->len > ( ep->buf[1] +2 ) )
    {
        //puts("v5 ERR not v5 or not for connect or more bytes recved.");
        goto PROTOCOL_METHOD_ERROR;
    }
    
    for ( i = 2; i < ep->len; i++ )
    {
        if ( ep->buf[i] == 0 )
        {
            break ;
        }
    }
    
    if ( i == ep->len )
    {
        //puts("v5 ERR no method 0.");
        goto PROTOCOL_METHOD_ERROR;
    }
    
    write( ep->left.fd, "\x05\x00", 2 );
    
    ep->stat = EPP_SOCK5;
    ep->len = 0;
    
    return SOCKS_P_CONTINUE;

PROTOCOL_METHOD_ERROR :
    
    write( ep->left.fd, "\x05\xFF", 2 );
    ep->len = 0;
    return SOCKS_P_ERROR;
    
}

int socks_v5( ep_proxy* ep )
{
    
    int i;
    int fd;
    
    char pbuf[100];
    
    if ( ep->len < 7 )
    {
        return SOCKS_P_CONTINUE;
    }
    
    switch ( ep->buf[3] )
    {
        case 1: // IPv4
            if ( ep->len > 10 )
            {
                //puts("v5 ERR length error for ipv4.");
                goto PROTOCOL_ERROR;
            }
            else if ( ep->len < 10 )
            {
                return SOCKS_P_CONTINUE;
            }
            else
            {
                ep_address_frombinary( 
                    ep->buf+4, ep->buf+6, 
                    &( ep->right_remote_addr )  
                );
            }
            break;
            
        case 3: // domain
            if ( ep->len > (  7 + ep->buf[4] ) )
            {
                //puts("v5 ERR length error for domain.");
                goto PROTOCOL_ERROR;
            }
            else if ( ep->len < ( 7 + ep->buf[4] ) )
            {
                return SOCKS_P_CONTINUE;
            }
            else
            {
                //puts("===");
                if ( ep_address_fromstring( 
                            ep->buf+5, 
                            ep->buf[4], 
                            -1, 
                            &( ep->right_remote_addr )
                        ) == -1 )
                {
                    ep->buf[1] = '\x04';
                    write( ep->left.fd, ep->buf , ep->len );
                    //puts("v5 ERR can not connect.");
                    return SOCKS_P_ERROR;
                }
            }
            break;
            
        case 4: // IPv6
            if ( ep->len > 22 )
            {
                //puts("v5 ERR for ipv6.");
                goto PROTOCOL_ERROR;
            }
            else if ( ep->len < 22 )
            {
                return SOCKS_P_CONTINUE;
            }
            
            //puts("v5 ERR for ipv6.");

            ep->buf[1] = '\x08';
            write( ep->left.fd, ep->buf , ep->len );
            
            return SOCKS_P_ERROR;
            
        default :
            //puts("v5 ERR unkown");
            return SOCKS_P_ERROR;
    }
    
    //ep_address_tostring_pretty( &( ep->right_remote_addr ), pbuf, 100 );
    //puts(pbuf);
    
    fd = connect_socket( &( ep->right_remote_addr ) );
    
    if ( fd == -1 )
    {
        ep->buf[1] = '\x05';
        write( ep->left.fd, ep->buf , ep->len );
        return SOCKS_P_ERROR;
    }
    
    ep->buf[1] = '\x00'; //REP
    ep->buf[3] = '\x01'; //ATYP return ipv4
    
    ep_address_fromfd( fd, &(ep->right_local_addr));
    ep_address_tobinary( &( ep->right_local_addr ), ep->buf+8, ep->buf+4 );
    
    write( ep->left.fd, ep->buf, 10 );
    
    ep->len = 0;
    
    //ev_io_init (&(p_ep_proxy->right), right_socket_cb, fd, EV_READ);
    //ev_io_start (EV_A_ &(p_ep_proxy->right));
    
    ep->protocol = 5;

    return fd;
    
PROTOCOL_ERROR :
    
    write( ep->left.fd, "\x05\xFF", 2 );
    ep->len = 0;
    return SOCKS_P_ERROR;   
}


int 
evilproxy_dynamic_proxy( ep_proxy* p_epp )
{
    int r;
    
    if ( p_epp->len < 1 )
    {
        return SOCKS_P_CONTINUE;
    }
    
    switch( p_epp->buf[0] )
    {
        case 4 :
            r = socks_v4(p_epp);
            break;
        
        case 5 :
            if ( p_epp->stat == EPP_SOCK5 )
            {
                r = socks_v5(ep);
            }
            else
            {
                r = socks_v5_pre(ep);
            }
            break;
        
        default :
            r = SOCKS_P_ERROR;
    }
    
    return r;
}



int 
evilproxy_local_proxy( ep_proxy* p_epp )
{
    
    int fd;
    
    memcpy( (void *) ( &(p_epp->right_remote_addr) ),
            (void *) ( &(p_epp->ep->proxyaddr) ),
            sizeof(ep_address) ) ;
    
    p_epp->right_fd = connect_socket( &( ep->right_remote_addr ) );
    
    if ( fd == -1 )
    {
        return SOCKS_P_ERROR;
    }
    
    ep_address_fromfd( fd, &(ep->right_local_addr) );
    
    ev_io_init (&(p_ep_proxy->right), right_socket_cb, rp, EV_READ);
    ev_io_start (EV_A_ &(p_ep_proxy->right));
    
    return SOCKS_P_OK;
    
}


static void
evilproxy_clean_in_left( ep_proxy* p_epp )
{
    ev_io_stop( EV_A_ &(p_epp->left) );
    ev_io_stop( EV_A_ &(p_epp->right) );
    
    return
}


static void
evilproxy_left_socket_cb( EV_P_ ev_io *w, int revents )
{
    
    int r ;
    
    EVILPROXY *ep;
    ep_proxy *p_epp;
    
    p_epp = parent(ep_proxy, left, w);
    ep = p_epp->ep;
    
    if ( p_epp->stat < EPP_READY )
                      // EPP_NULL and EPP_SOCK5PRE and so on...
    {
        switch(ep->proxytype)
        {
            case EPT_LOCAL :
                r = evilproxy_local_proxy( p_epp );
                break;

            case EPT_DYNAMIC :
                r = evilproxy_dynamic_proxy( p_epp );
                break;
            default :
                evilproxy_clean_in_left( p_epp );
                return 
        }
        
        switch(r)
        {
            case SOCKS_P_OK :
                p_epp->stat = EPP_READY ;
                break;
            case SOCKS_P_CONTINUE :
                break;
            case SOCKS_P_ERROR :
            default:
                evilproxy_clean_in_left( p_epp );
                break;
                
        }
        
        return 
    }
    
}


static void
evilproxy_listen_cb( EV_P_ ev_io *w, int revents )
{
    
    EVILPROXY *ep;
    ep_proxy *p_epp;
    ep_address tempaddr;
    int fd;
    
    ep = w->ep;
        
    p_epp = (ep_proxy *)malloc( sizeof(ep_proxy) );
    
    if( p_ep_proxy == NULL )
    {
        fd = accept_socket( ep->listen_fd, &(tempaddr) );
        close(fd);
        return;
    }
    
    p_epp->ep = ep;
    
    p_epp->left_fd = accept_socket( ep->listen_fd, &(p_epp->left_remote_addr) );
    p_epp->stat = 0;
    p_epp->len = 0 ;
    
    memset( (void *)(p_epp->buf), 0, EPP_BufferLen );
    
    ev_io_init (&(p_ep_proxy->left), left_socket_cb, fd, EV_READ);
    ev_io_start (EV_A_ &(p_ep_proxy->left));
    
    
    if ( ep->proxytype == EPT_LOCAL )
    {
        rp = local_proxy( p_ep_proxy );
        
        if ( rp == SOCKS_P_ERROR )
        {
            printf( COLOR( "     ", COLOR_ON_RED ) 
                "Connection Closed. LOCAL ERROR \r\n" );
            
            close( fd );
            ev_io_stop (EV_A_ &(p_ep_proxy->left));
            free(p_ep_proxy);
            return;
        }
        
        printf( COLOR( "     ", COLOR_ON_RED ) 
                "Accept LOCAL %d %d\r\n", fd, rp);
        
        ev_io_init (&(p_ep_proxy->right), right_socket_cb, rp, EV_READ);
        ev_io_start (EV_A_ &(p_ep_proxy->right));
        
        p_ep_proxy->stat = 1;
    }
}






EVILPROXY* 
evilproxy_init( ep_address* serv_addr, ep_address* proxy_addr, void* data )
{
    
    EVILPROXY* ep ;
    
    if ( serv_addr == NULL )
    {
        return NULL;
    }
    
    ep = (EVILPROXY*)malloc( sizeof(EVILPROXY) );
    
    if ( ep == NULL )
    {
        return NULL;
    }
    
    memcpy( &(ep->localaddr), serv_addr, sizeof(ep_address) );
    ep->proxytype = ( proxyaddr == NULL ) ? EPT_DYNAMIC : EPT_LOCAL ;
    
    if ( proxy_addr != NULL )
    {
        memcpy( &(ep->proxyaddr), proxy_addr, sizeof(ep_address) );
    }
    
    ep->loop = ev_default_loop(0);
    ep->listen_fd = create_listen_socket( serv_addr );
    ep->data = data
    
    ep->listen_watcher = (ep_listen*)malloc( sizeof(ep_listen) );
    
    ev_io_init( (ev_io *)(&ep->listen_watcher), 
                evilproxy_listen_cb, ep->listen_fd, EV_READ );
    ev_io_start( ep->loop, (ev_io *)(&ep->listen_watcher) );
    
    return ep;
    
}

void 
evilproxy_loop( EVILPROXY* ep )
{
    
    ev_loop( ep->loop, 0 );
    
}
















