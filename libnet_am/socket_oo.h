#include <netdb.h>
#include <sys/socket.h>

struct sockaddr_oo
{
    struct sockaddr_in addr;
    char hostname[256];
};

typedef struct sockaddr_oo SockAddrObj;


SockAddrObj* SockAddrObj_fromfd( SockAddrObj* self, int fd );
SockAddrObj* SockAddrObj_fromstring( SockAddrObj* self, char* address, int len, long defaultport );
SockAddrObj* SockAddrObj_frombinary( SockAddrObj* self, char* h, char* p );

char* SockAddrObj_tostring( SockAddrObj* self, char* address, int maxlength );
char* SockAddrObj_tobinary( SockAddrObj* self, char* h, char* p );

int SockAddrObj_bind_and_listen( SockAddrObj* self );
SockAddrObj* SockAddrObj_accept( SockAddrObj* self, int listen_fd, int* p_accept_fd );
int SockAddrObj_connect( SockAddrObj* self, int timeout );

char* SockAddrObj_errormsg( SockAddrObj* self, int errno, char* msgbuf, int maxlength );
