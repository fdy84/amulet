/* 
 * Copyright 2009 SINA corp.
 * This is a resource pool implementation.
 * The pool can hold handles of any type of resources.
 * Author:
 *      zhangshuo(Avin) <zhangshuo@staff.sina.com.cn>
 * Last update: 2009.7.24
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "rspool.h"


RSPOOL * 
rspool_new()
{
    RSPOOL *rsp;
 
    rsp = (RSPOOL *)calloc(1, sizeof(RSPOOL));
    if (rsp == NULL) 
        return NULL;

    pthread_mutex_init(&(rsp->g_arr_lock), NULL);    
    return rsp;
}


int 
rspool_put(RSPOOL *rsp, int gid, void *handle)
{
    RSPOOL_GROUP *rspgp;
    struct rspool_node *rspnp;
    
    if (rsp == NULL || handle == NULL) 
        return RSPOOL_ERR_PARAINVALID;

    if (gid < 0 || gid > MAX_GROUPS - 1) 
        return RSPOOL_ERR_GIDOVERLIMIT;
    
    if (rsp->grp_array[gid] == NULL) {
        /* lock */
        pthread_mutex_lock(&(rsp->g_arr_lock)); 
        if (rsp->grp_array[gid] == NULL) {
            rsp->grp_array[gid] = (RSPOOL_GROUP *)calloc(1, sizeof(RSPOOL_GROUP));
            if (rsp->grp_array[gid] == NULL) {
                /* unlock 1 */
                pthread_mutex_unlock(&(rsp->g_arr_lock));
                return RSPOOL_ERR_NOMEM;
            }
            
            rspgp = rsp->grp_array[gid];
            
            /* init the group lock */
            pthread_mutex_init(&(rspgp->g_lock), NULL); 
            STAILQ_INIT(&(rspgp->rgh));
            rsp->group_count++;  
        }
        /* unlock 2 */
        pthread_mutex_unlock(&(rsp->g_arr_lock));
    } 

	rspgp = rsp->grp_array[gid];
    rsp->max_gid = gid > rsp->max_gid ? gid : rsp->max_gid;
    
    rspnp = (struct rspool_node *)malloc(sizeof(struct rspool_node));
    if (rspnp == NULL)
        return RSPOOL_ERR_NOMEM;

    rspnp->handle = handle;
    
    pthread_mutex_lock(&(rspgp->g_lock)); 
    STAILQ_INSERT_TAIL(&(rspgp->rgh), rspnp, entries);
    rspgp->group_size++;
    pthread_mutex_unlock(&(rspgp->g_lock));

    return RSPOOL_OK;
}


void * 
rspool_get(RSPOOL *rsp, int gid)
{
    struct rspool_node *rspnp;
    RSPOOL_GROUP *rspgp;
    void *handle;
    
    if (rsp == NULL || gid < 0 || gid > MAX_GROUPS - 1) 
        return NULL;
    
    if (rsp->grp_array[gid] == NULL)
        return NULL;

    rspgp = rsp->grp_array[gid];

    /* lock */
    pthread_mutex_lock(&(rspgp->g_lock)); 
    rspnp = STAILQ_FIRST(&(rspgp->rgh));
    if (rspnp == NULL) {
        /* unlock 1 */
        pthread_mutex_unlock(&(rspgp->g_lock));
        return NULL;
    }

    handle = rspnp->handle;
    STAILQ_REMOVE(&(rspgp->rgh), rspnp, rspool_node, entries);
    rspgp->group_size--;
    /* unlock 2 */
    pthread_mutex_unlock(&(rspgp->g_lock));

    free(rspnp);
    return handle;
}


void * 
rspool_group_foreach(RSPOOL *rsp, int gid)
{
    struct rspool_node *rspnp;
    RSPOOL_GROUP *rspgp;
    void *handle;
    
    if (rsp == NULL || gid < 0 || gid > MAX_GROUPS - 1) 
        return NULL;
    
    if (rsp->grp_array[gid] == NULL)
        return NULL;

    rspgp = rsp->grp_array[gid];

    /* lock */
    pthread_mutex_lock(&(rspgp->g_lock)); 
    if (rspgp->cur_node == NULL) 
        rspnp = STAILQ_FIRST(&(rspgp->rgh));
    else
        rspnp = STAILQ_NEXT(rspgp->cur_node, entries);

    if (rspnp == NULL) {
        rspgp->cur_node = NULL;
        /* unlock 1 */
        pthread_mutex_unlock(&(rspgp->g_lock));
        return NULL;
    }

    /* unlock 2 */
    pthread_mutex_unlock(&(rspgp->g_lock));

    handle = rspnp->handle;
    rspgp->cur_node = rspnp;

    return handle;
}


void * 
rspool_all_foreach(RSPOOL *rsp)
{
    struct rspool_node *rspnp;
    RSPOOL_GROUP *rspgp;
    void *handle;
    int gid;
    
    if (rsp == NULL) 
        return NULL;

    for (gid = rsp->cur_gid; gid < rsp->max_gid + 1; ) {
        if (rsp->grp_array[gid] == NULL) {
            gid++;
            continue;
        }

        rspgp = rsp->grp_array[gid];
        
        /* lock */
        pthread_mutex_lock(&(rspgp->g_lock)); 
        if (rspgp->cur_node == NULL) 
            rspnp = STAILQ_FIRST(&(rspgp->rgh));
        else
            rspnp = STAILQ_NEXT(rspgp->cur_node, entries);

        if (rspnp == NULL) {
            rspgp->cur_node = NULL;
            gid++;
            /* unlock 1 */
            pthread_mutex_unlock(&(rspgp->g_lock));
            continue;
        }

        /* unlock 2 */
        pthread_mutex_unlock(&(rspgp->g_lock));

        handle = rspnp->handle;
        rspgp->cur_node = rspnp;
        rsp->cur_gid = gid;

        return handle;
    }

    rspgp->cur_node = NULL;
    rsp->cur_gid = 0;

    return NULL;
}


int 
rspool_del(RSPOOL *rsp)
{
    int gid;
    RSPOOL_GROUP *rspgp;
    
    if (rsp == NULL) 
        return RSPOOL_ERR_PARAINVALID;

    for (gid = 0; gid < rsp->max_gid + 1; gid++) {
        if (rsp->grp_array[gid] == NULL)
            continue;

        rspgp = rsp->grp_array[gid];
          
        for ( ; ; ) {
            if (rspool_get(rsp, gid) == NULL)
                break;
        }
        
        pthread_mutex_destroy(&(rspgp->g_lock));
        free(rspgp);
    }

    pthread_mutex_destroy(&(rsp->g_arr_lock));
    free(rsp);
    return RSPOOL_OK;
}





/* 
 * del item. 
 * method 1 :

    for (j = 0; j < rspgp->group_size; j++){
        rspnp = STAILQ_FIRST(&rspgp->rgh);
        STAILQ_REMOVE_HEAD(&rspgp->rgh, entries);
        free(rspnp);
        free_c++;
    }

    method 2 :

    for ( ; ; ) {
        if (rspool_get(rsp, gid) == NULL)
            break;
    }

 */


