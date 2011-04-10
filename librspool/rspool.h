#ifndef _RSPOOL_H_
#define _RSPOOL_H_

#include <pthread.h>
#include "queue.h"

#define RSPOOL_ERR_NOMEM            -1
#define RSPOOL_ERR_PARAINVALID      -2
#define RSPOOL_ERR_GIDOVERLIMIT     -3
#define RSPOOL_ERR_GIDALREADYINUSE  -4
#define RSPOOL_OK                   0


#define MAX_GROUPS      10240

struct rspool_node
{
    void *handle;
    STAILQ_ENTRY(rspool_node) entries;
};

typedef struct rspool_group
{
    int group_size;
    pthread_mutex_t g_lock;
    STAILQ_HEAD(rspool_head, rspool_node) rgh;

    /* for group foreach */
    struct rspool_node *cur_node;
} RSPOOL_GROUP;

/* autodoc
    testdoc
*/
typedef struct resource_pool
{
    RSPOOL_GROUP *grp_array[MAX_GROUPS];
    pthread_mutex_t g_arr_lock;
    int group_count;
    int max_gid;

    /* for all foreach */
    int cur_gid;

} RSPOOL;


RSPOOL *rspool_new(void);
int rspool_put(RSPOOL *, int, void *);
void *rspool_get(RSPOOL *, int);
void *rspool_group_foreach(RSPOOL *, int);
void *rspool_all_foreach(RSPOOL *);
int rspool_del(RSPOOL *);


#endif

