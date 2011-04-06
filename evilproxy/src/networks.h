
#include <netdb.h>
#include <sys/socket.h>

struct _stt_ep_address
{
    struct sockaddr_in addr;
    char hostname[255];
};

typedef struct _stt_ep_address ep_address;

int ep_address_fromstring( char *p, int len, int defaultport, ep_address* epaddr );
int ep_address_inaddr_any( int port, ep_address* epaddr  );
int ep_address_frombinary( char* h, char* p, ep_address* epaddr );
int ep_address_fromfd( int fd, ep_address* epaddr  );
int ep_address_tobinary( ep_address* epaddr, char* p, char* h );
int ep_address_tostring_pretty( ep_address* epaddr, char* p, int size );


int create_listen_socket( ep_address* epaddr );
int accept_socket( int listen_fd, ep_address* epaddr );
int connect_socket( ep_address* epaddr );

