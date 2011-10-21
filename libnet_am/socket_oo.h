
#include <netdb.h>
#include <sys/socket.h>

struct sockaddr_oo
{
    struct sockaddr_in addr;
    char hostname[255];
};

typedef struct sockaddr_oo SockAddrObj;


//SocketObj* SocketObj_new( char* address, int port );
SockAddrObj* SockAddrObj_fromfd( SockAddrObj* self, int fd );
SockAddrObj* SockAddrObj_fromstring( SockAddrObj* self, char* address, int timeout );
SockAddrObj* SockAddrObj_frombinary( SockAddrObj* self, char* h, char* p );
SockAddrObj* SockAddrObj_accept( SockAddrObj* self );

int SockAddrObj_bind_and_listen( SockAddrObj* self );
int SockAddrObj_connect( SockAddrObj* self );

void SocketAddrObj_delete( SockAddrObj* self );
