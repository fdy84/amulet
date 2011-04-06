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

// only for gcc
#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
#define parent(TYPE, MEMBER, p) ((TYPE *)( (void*)p - offsetof(TYPE, MEMBER) ))




struct _stt_ep_listen
{
    ev_io listen;
    
    int proxytype;
    
    ep_address proxyaddr;
};

#define EPT_NULL     -1
#define EPT_LOCAL     0
#define EPT_DYNAMIC   1

typedef struct _stt_ep_listen ep_listen;

ep_listen g_listen_watcher;


struct _stt_ep_proxy
{
    int stat;
    int protocol;
    char user[512];
    char buf[10240];
    int len;
    
    ep_address left_local_addr;
    ep_address left_remote_addr;
    ep_address right_local_addr;
    ep_address right_remote_addr;
    
    ev_io left;
    ev_io right;
};


typedef struct _stt_ep_proxy ep_proxy;


#define EP_PRINT_SEND 1
#define EP_PRINT_RECV -1

int prettyprint( int f, ep_proxy* ep )
{
    char buf[1024*6+1];
    int i;
    
    char* n;
    char* p;
    
    int c;
    
    int z;
    
    
    bzero( (void*)(buf), 1024*6+1 );
    
    printf(( f == EP_PRINT_SEND )?"\033[44m\033[37m":"\033[42m\033[37m");
    ep_address_tostring_pretty( &( ep->left_remote_addr ), buf, 1024*6 );
    printf(buf);
    printf(" (%d)", ep->left.fd );
    printf(( f == EP_PRINT_SEND )?" -> ":" <- ");
    ep_address_tostring_pretty( &( ep->right_remote_addr ), buf, 1024*6 );
    printf(buf);
    printf(" (%d)", ep->right.fd );
    printf("\033[0m\r\n");
    
    c = 0;
    
    printf( "<LENGTH:%d>\r\n", ep->len );
    
    if ( ep->len == 0 )
    {
        puts("");
        return;
    }
    
    bzero( (void*)(buf), 1024*6+1 );
    
    for ( i = 0, n = buf, p = ep->buf ;
          i < ep->len && ( n - buf ) < 1024*6-10;
          i++, p++ )
    {
        if ( *p >= ' ' && *p <= '~' )
        {
            if ( c != 0 )
            {
                n += sprintf( n, "\033[0m" );
                c = 0;
            }
            *n = *p;
            n++;
            continue;
        }
        
        if ( c != 1 )
        {
            n += sprintf( n, "\033[33m" );
            c = 1;
        }
        
        if ( *p == '\x0D' && *(p+1) == '\x0A' )
        {
            n += sprintf( n, "(0D)(0A)\r\n" );
            p++;
            i++;
            continue;
        }
        
        n += sprintf( n, "(%02X)", (unsigned char)(*p) );
        //n += 4;
    }
    
    if ( c != 0 )
    {
        n += sprintf( n, "\033[0m" );
        c = 0;
    }
    
    //*n = '\0';
    
    puts(buf);
}


/*

SOCKS v4


>>>>>
+----+----+----+----+----+----+----+----+----+----+....+----+
| VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
+----+----+----+----+----+----+----+----+----+----+....+----+
   1    1      2              4           variable       1

VN : 4 
CD : 1 = Connect , 2 = BIND
USERID : len 512 // RFC 1413

<<<<<
+----+----+----+----+----+----+----+----+
| VN | CD | DSTPORT |      DSTIP        |
+----+----+----+----+----+----+----+----+
   1    1      2              4


VN : 0
CD : 90: request granted
     91: request rejected or failed
     92: request rejected becasue SOCKS server cannot connect to
	     identd on the client
     93: request rejected because the client program and identd
	     report different user-ids




*/

//-------------------------------------------------------------------------

/*
SOCKS v5

>>>>>
+----+----------+----------+
|VER | NMETHODS | METHODS  |
+----+----------+----------+
|  1 |    1     | 1 to 255 | 
+----+----------+----------+

VER : 5

<<<<<
+----+--------+
|VER | METHOD |
+----+--------+
|  1 |    1   | 
+----+--------+

METHOD :    X'00' NO AUTHENTICATION REQUIRED
            X'01' GSSAPI
            X'02' USERNAME/PASSWORD
            X'03' to X'7F' IANA ASSIGNED
            X'80' to X'FE' RESERVED FOR PRIVATE METHODS
            X'FF' NO ACCEPTABLE METHODS


>>>>>
+----+-----+-------+-------+-------------------+----------+
|VER | CMD | RSV   | ATYP  | DST.ADDR          | DST.PORT |
+----+-----+-------+-------+-------------------+----------+
| 1  | 1   | X'00' |  1    | Variable          |    2     |
+----+-----+-------+-------+-------------------+----------+
                   | =0x01 |       IPv4        |
                   +-------+-------------------+
                           |         4         |
                   +-------+-------------------+
                   | =0x03 | LENGTH | DOMAIN   |
                   +-------+--------+----------+
                           |   1    | Variable |
                   +-------+--------+----------+
                   | =0x04 |       IPv6        |
                   +-------+-------------------+
                           |        16         |
                           +-------------------+

VER : 5
CMD : 1 CONNECT, 2 BIND, 3 UDP
RSV : 0
ATYP : 1 ipv4, 3 domain, 4 ipv6



<<<<<
          +----+--------+-------+-------+----------+----------+
          |VER | REP    |  RSV  | ATYP  | BND.ADDR | BND.PORT |
          +----+--------+-------+-------+----------+----------+
          | 1  |  1     | X'00' | 1     | Variable | 2        |
+---------+----+--------+-------+-------+----------+----------+
| CONNECT | -  | =0x00  | --    |       |  IPv4    |
+---------+----+--------+-------+-------+----------+
                                | =0x01 |   4      |
+---------+----+--------+-------+-------+----------+
| CONNECT | -  | !=0x00 | --    |       |  IPv4    |
+---------+----+--------+-------+-------+----------+----------+---------+
                                | =0x01 |   4      | 2        | close() |
                                +-------+----------+----------+---------+

REP :   X'00' succeeded
        X'01' general SOCKS server failure
        X'02' connection not allowed by ruleset
        X'03' Network unreachable
        X'04' Host unreachable
        X'05' Connection refused
        X'06' TTL expired
        X'07' Command not supported
        X'08' Address type not supported
        X'09' to X'FF' unassigned

*/

//#define SOCKS_P_OK        0
#define SOCKS_P_ERROR    -1
#define SOCKS_P_CONTINUE -2

int socks_v4( ep_proxy* ep )
{
    int i;
    int fd;
    
    char pbuf[100];
    
    if ( ep->len < 9 )
    {
        return SOCKS_P_CONTINUE;
    }
    
    if ( ep->buf[0] != 4 && ep->buf[1] != 1 )
    {
        puts("v4 ERR not v4 or not for connect.");
        goto PROTOCOL_ERROR;
    }
    
    for ( i=8 ; i < (ep->len-1) ; i++ )
    {
        if ( ep->buf[i] == '\0' ) // END NULL
        {
            puts("v4 ERR NULL");
            goto PROTOCOL_ERROR;
        }
    }
    
    if ( ep->buf[ep->len-1] != '\0' ) // END NULL
    {
        puts("v4 ERR not NULL");
        return SOCKS_P_CONTINUE;
    }
    
    ep_address_frombinary( ep->buf+4, ep->buf+2, &( ep->right_remote_addr )  );
    
    fd = connect_socket( &( ep->right_remote_addr ) );
    
    if ( fd == -1 )
    {
        ep_address_tostring_pretty( &( ep->right_remote_addr ), pbuf, 100 );
        puts(pbuf);
        puts("v4 ERR link error");
        write( ep->left.fd, "\x00\x92\x00\x00\x00\x00\x00\x00", 8 );
        return SOCKS_P_ERROR;
    }
    
    ep->buf[0] = 0;
    ep->buf[1] = 90;
    
    ep_address_fromfd( fd, &(ep->right_local_addr));
    ep_address_tobinary( &( ep->right_local_addr ), ep->buf+2, NULL  );
    
    write( ep->left.fd, ep->buf, 8 );
    
    ep->len = 0;
    
    //ev_io_init (&(ep->right), right_socket_cb, fd, EV_READ);
    //ev_io_start (EV_A_ &(ep->right));
    
    ep->protocol = 4;
    
    return fd;

PROTOCOL_ERROR :
    
    write( ep->left.fd, "\x00\x93\x00\x00\x00\x00\x00\x00", 8 );
    ep->len = 0;
    return SOCKS_P_ERROR;
}


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
        if ( ep->buf[1] == 0 )
        {
            puts("v5 NO Methods.");
        }
        if ( ep->len > ( ep->buf[1] +2 ) )
        {
            puts("v5 Length Error.");
        }
        puts("v5 ERR not v5 or not for connect or more bytes recved.");
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
        puts("v5 ERR no method 0.");
        goto PROTOCOL_METHOD_ERROR;
    }
    
    write( ep->left.fd, "\x05\x00", 2 );
    
    ep->protocol = 501;
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
                puts("v5 ERR length error for ipv4.");
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
                puts("v5 ERR length error for domain.");
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
                    puts("v5 ERR can not connect.");
                    return SOCKS_P_ERROR;
                }
            }
            break;
            
        case 4: // IPv6
            if ( ep->len > 22 )
            {
                puts("v5 ERR for ipv6.");
                goto PROTOCOL_ERROR;
            }
            else if ( ep->len < 22 )
            {
                return SOCKS_P_CONTINUE;
            }
            
            puts("v5 ERR for ipv6.");

            ep->buf[1] = '\x08';
            write( ep->left.fd, ep->buf , ep->len );
            
            return SOCKS_P_ERROR;
            
        default :
            puts("v5 ERR unkown");
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


int dynamic_proxy( ep_proxy* ep )
{
    int r;
    
    if ( ep->len < 1 )
    {
        return SOCKS_P_CONTINUE;
    }
    
    switch( ep->buf[0] )
    {
        case 4 :
            r = socks_v4(ep);
            break;
        
        case 5 :
            if ( ep->protocol == 501 )
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

int local_proxy( ep_proxy* ep )
{
    int fd;
    
    memcpy( (void *) (&ep->right_remote_addr),
            (void *) (&g_listen_watcher.proxyaddr),
            sizeof(ep_address) ) ;
    
    
    fd = connect_socket( &( ep->right_remote_addr ) );
    
    if ( fd == -1 )
    {
        return SOCKS_P_ERROR;
    }
    
    ep_address_fromfd( fd, &(ep->right_local_addr));
    
    return fd;
}

#define ep_w (parent(ep_proxy, left, w))

static void
left_socket_cb (EV_P_ ev_io *w, int revents)
{
    
    int rl = 0;
    int olen = 0;
    
    int rp = 0;
    int e;
    
    if ( ep_w->stat < 0 )
    {

        prettyprint( EP_PRINT_RECV, ep_w );
        
        memset( (void *)(ep_w->buf), 0, 10240 );
        ep_w->len = 0;
        
        ep_w->stat = 1;
    }
    
    olen = ep_w->len;
    
    do
    {
        ep_w->len += rl;
        rl = recv( w->fd, (ep_w->buf)+(ep_w->len),
                   10240-ep_w->len, MSG_DONTWAIT );
    } while( rl > 0 );
    
    e = errno;
    
    printf( COLOR( "     ", COLOR_ON_CYAN ) 
            "LEFT fd:%d len:%d e:%d rl:%d\r\n", w->fd, ep_w->len, e, rl);
    
    if ( ep_w->stat == 0 )
    {
        if ( g_listen_watcher.proxytype == EPT_DYNAMIC )
        {
            rp = dynamic_proxy(ep_w);
        }
        //else
        //{
        //    rp = local_proxy(ep_w);
        //}
        
        if ( rp == SOCKS_P_ERROR || e != EAGAIN )
        {
            printf( COLOR( "     ", COLOR_ON_RED ) 
                "Connection Closed. DYN ERROR fd : %d rp:%d e:%d rl:%d\r\n", 
                w->fd, rp, e, rl);
            
            close( w->fd );
            ev_io_stop (EV_A_ w);
            free(ep_w);
            return;
        }
        
        if ( rp == SOCKS_P_CONTINUE )
        {
            return;
        }
        
        printf( COLOR( "     ", COLOR_ON_RED ) 
                "Accept DYN %d %d\r\n", w->fd, rp);
        
        ev_io_init (&(ep_w->right), right_socket_cb, rp, EV_READ);
        ev_io_start (EV_A_ &(ep_w->right));
        
        ep_w->stat = 1;
    }
    
    if ( ep_w->len-olen > 0 )
    {
        write( ep_w->right.fd, (ep_w->buf)+olen, ep_w->len-olen );
        printf( COLOR( "     ", COLOR_ON_CYAN ) 
                "L->R sends %d \r\n", ep_w->len-olen );
    }
    
    if ( rl == 0 || ( rl <= 0 && e != EAGAIN ) )
    {
        printf( COLOR( "     ", COLOR_ON_RED ) 
                "connection Closed. L fd: L %d R %d\r\n", 
                w->fd, ep_w->right.fd ) ;
        
        ev_io_stop (EV_A_ w);
        ev_io_stop (EV_A_ &(ep_w->right));
        
        close( w->fd );
        close( ep_w->right.fd );
        
        if ( ep_w->len != 0 )
        {
            prettyprint( EP_PRINT_SEND, ep_w );
        }
        
        free(ep_w);
        
        return ;
    }
    
    return ;
}

#undef ep_w
#define ep_w (parent(ep_proxy, right, w))

static void
right_socket_cb (EV_P_ ev_io *w, int revents)
{
    
    int rl = 0;
    int olen = 0;
    
    int e;
    
    if ( ep_w->stat > 0 )
    {
        prettyprint( EP_PRINT_SEND, ep_w );
        
        memset( (void *)ep_w->buf, 0, 10240 );
        ep_w->len = 0;
        
        ep_w->stat = -1;
    }
    
    olen = ep_w->len;
    
    do
    {
        ep_w->len += rl;
        rl = recv( w->fd, (ep_w->buf)+(ep_w->len), 
                   10240-ep_w->len, MSG_DONTWAIT );
    } while( rl > 0 );
    
    e = errno;
    
    printf( COLOR( "     ", COLOR_ON_CYAN ) 
            "RIGHT fd:%d len:%d e:%d rl:%d\r\n", w->fd, ep_w->len, e, rl);
    
    write( ep_w->left.fd, (ep_w->buf)+olen, ep_w->len-olen );
    
    printf( COLOR( "     ", COLOR_ON_CYAN ) 
            "R->L sends %d \r\n", ep_w->len-olen );
    
    if ( rl == 0 || ( rl <= 0 && e != EAGAIN ) )
    {
        printf( COLOR( "     ", COLOR_ON_RED ) 
                "Connection Closed. R fd: L %d, R %d\r\n", 
                ep_w->left.fd, w->fd ) ;
        
        ev_io_stop (EV_A_ w);
        ev_io_stop (EV_A_ &(ep_w->left));
        
        close( w->fd );
        close( ep_w->left.fd );
        
        if ( ep_w->len != 0 )
        {
            prettyprint( EP_PRINT_RECV, ep_w );
        }
        
        free(ep_w);
        
        return ;
    }
    
    return ;
}

#undef ep_w


static void
listen_cb (EV_P_ ev_io *w, int revents)
{
    int fd, rp;
    ep_proxy *p_ep_proxy;
    
    p_ep_proxy = (ep_proxy *)malloc( sizeof(ep_proxy) );
    
    p_ep_proxy->stat = 0;
    p_ep_proxy->protocol = 0;
    p_ep_proxy->len = 0 ;
    
    memset( (void *)(p_ep_proxy->buf), 0, 10240 );
    
    fd = accept_socket( w->fd, &(p_ep_proxy->left_remote_addr) );
    printf ( COLOR( "     ", COLOR_ON_RED )  "Accepted fd:%d\r\n", fd);
    
    //rfd = connect_socket( "192.168.1.1", 23, &(p_ep_proxy->remote_addr) );
    
    ev_io_init (&(p_ep_proxy->left), left_socket_cb, fd, EV_READ);
    ev_io_start (EV_A_ &(p_ep_proxy->left));
    
    //ev_io_init (&(p_ep_proxy->right), right_socket_cb, rfd, EV_READ);
    //ev_io_start (EV_A_ &(p_ep_proxy->right));
    
    if ( g_listen_watcher.proxytype == EPT_LOCAL )
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



static const char *optString = "vVD:L:";
static const char *versioninfo = "evil proxy (0.10a)";
static const char *tips = "\
 -V print version information and exit\r\n\
 -v show verbose message and exit\r\n\
 -D [listen-IP:]listen-port\r\n\
       Dynamic SOCKS-based port forwarding\r\n\
 -L [listen-IP:]listen-port:host:port\r\n\
       Forward local port to remote address\r\n\
";


int chrcnt( char* p, char c )
{
    int i = 0;
    for ( ; *p != '\0' ; p++ )
    {
        if ( *p == c )
        {
            i++;
        }
    }
    return i;
}


int
main ( int argc, char *argv[] )
{
    
    int listen_fd;
    struct ev_loop *loop;
    
    ep_address serv_addr;
    
    char* strsplit;
    
    int opt;
    
    char prtbuf[40];
    
    g_listen_watcher.proxytype = EPT_NULL;
    
    opt = getopt( argc, argv, optString );
    
    while( opt != -1 )
    {
        switch( opt )
        {
            
            case 'v':
                puts(versioninfo);
                puts(tips);
                break;
            
            case 'V':
                puts(versioninfo);
                break;
            
            case 'D':
                // optarg is global var . defined in unistd.h
                switch ( chrcnt( optarg, ':' )  )
                {
                    case 0 :
                        if ( ep_address_inaddr_any( 
                                    strtol( optarg, NULL, 10 ), 
                                    &serv_addr 
                                ) == -1 )
                        {
                            exit(1);
                        }
                        break;
                    case 1 :
                        if ( ep_address_fromstring( 
                                    optarg, 0, 0, &serv_addr 
                                ) == -1 )
                        {
                            exit(1);
                        }
                        break;
                    default :
                        puts(versioninfo);
                        puts(tips);
                        exit(-1);
                }
                
                g_listen_watcher.proxytype = EPT_DYNAMIC;
                
                break;
                
            case 'L':
                switch( chrcnt( optarg, ':' )  )
                {
                    case 2:
                        strsplit = strchr( optarg, ':' );
                        if ( ep_address_inaddr_any( 
                                    strtol( optarg, &strsplit, 10 ), 
                                    &serv_addr 
                                ) == -1 )
                        {
                            exit(1);
                        }
                        if ( ep_address_fromstring( 
                                    strsplit+1, 0, 0, 
                                    &g_listen_watcher.proxyaddr 
                                ) == -1 )
                        {
                            exit(1);
                        }
                        break;
                        
                    case 3:
                        if ( ep_address_fromstring( 
                                    strsplit+1, 0, 0, &serv_addr 
                                ) == -1 )
                        {
                            exit(1);
                        }
                        if ( ep_address_fromstring( 
                                    strsplit+1, 0, 0, 
                                    &g_listen_watcher.proxyaddr 
                                ) == -1 )
                        {
                            exit(1);
                        }
                        break;
                        
                    default :
                        puts(versioninfo);
                        puts(tips);
                        exit(1);
                }
                
                g_listen_watcher.proxytype = EPT_LOCAL;
                
                break;
                
            default :
                break;
            
        }
        
        opt = getopt( argc, argv, optString );
    }
    
    if ( g_listen_watcher.proxytype == EPT_NULL )
    {
        return 1;
    }
    
    // use the default event loop unless you have special needs
    //struct ev_loop *loop = ev_default_loop (0);
    loop = ev_default_loop (0);
    
    listen_fd = create_listen_socket( &serv_addr );
    
    ep_address_tostring_pretty( &serv_addr, prtbuf, 40 );
    puts(prtbuf);
    
    if ( g_listen_watcher.proxytype == EPT_LOCAL )
    {
        ep_address_tostring_pretty( &(g_listen_watcher.proxyaddr), prtbuf, 40 );
        puts(prtbuf);
    }
    
    
    ev_io_init ((ev_io *)(&g_listen_watcher), listen_cb, listen_fd, EV_READ);
    ev_io_start (loop, (ev_io *)(&g_listen_watcher));

    // initialise an io watcher, then start it
    // this one will watch for stdin to become readable
    // ev_io_init (&stdin_watcher, stdin_cb, /*STDIN_FILENO*/ 0, EV_READ);
    // ev_io_start (loop, &stdin_watcher);

    // initialise a timer watcher, then start it
    // simple non-repeating 5.5 second timeout
    // ev_timer_init (&timeout_watcher, timeout_cb, 5.5, 0.);
    // ev_timer_start (loop, &timeout_watcher);

    // now wait for events to arrive
    ev_loop (loop, 0);

    // unloop was called, so exit
    return 0;
}
