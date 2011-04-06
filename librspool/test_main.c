/* 
 * Copyright 2009 SINA corp.
 * This is a test case of resource pool implementation.
 * Author:
 *      zhangshuo(Avin) <zhangshuo@staff.sina.com.cn>
 * Last update: 2009.7.9
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "rspool.h"


int 
main(int argc, char **argv)
{
    int ret;
    char *new_handle;
    RSPOOL *rsp;
    
    (void)argc;
    (void)argv;

    char myhande[] = "this my text handle";
    char myhande1[] = "this my text handle1";
    char myhande2[] = "this my text handle2";
    char myhande3[] = "this my text handle3";
    char myhande4[] = "this my text handle4";

    char my1hande[] = "1this my text handle";
    char my1hande1[] = "1this my text handle1";
    char my1hande2[] = "1this my text handle2";
    char my1hande3[] = "1this my text handle3";
    char my1hande4[] = "1this my text handle4";


    for ( ; ; ) {


        /* first we create a new respool */
        rsp = rspool_new();
        if (rsp == NULL) {
            printf("rspool new error.\n");
            return 0;
        }

        /* we put an pointer into group 0 */
        
        ret = rspool_put(rsp, 3, myhande); 
        ret = rspool_put(rsp, 3, myhande1);
        ret = rspool_put(rsp, 3, myhande2);
        ret = rspool_put(rsp, 3, myhande3);
        ret = rspool_put(rsp, 3, myhande4);
    
        ret = rspool_put(rsp, 1, my1hande);
        ret = rspool_put(rsp, 1, my1hande1);
        ret = rspool_put(rsp, 1, my1hande2);
        ret = rspool_put(rsp, 1, my1hande3);
        ret = rspool_put(rsp, 1, my1hande4);
      

        /* get the handle from the pool */
        
        while ((new_handle = rspool_group_foreach(rsp, 1)) != NULL) {
            printf("===>foreach: %s\n", new_handle);
        }

        while ((new_handle = rspool_group_foreach(rsp, 3)) != NULL) {
            printf("===>foreach: %s\n", new_handle);
        }

        while ((new_handle = rspool_all_foreach(rsp)) != NULL) {
            printf("===>all_foreach: %s\n", new_handle);
        }

        while ((new_handle = rspool_all_foreach(rsp)) != NULL) {
            printf("===>all_foreach: %s\n", new_handle);
        }

        printf("------------------------\n");
    
        for ( ; ;) {
            new_handle = rspool_get(rsp, 3);
            if (new_handle == NULL) {
                printf("rspool_get error.\n");
                break;
            }
            //rspool_put(rsp, 0, new_handle);
            printf("===>get: %s\n", new_handle);
        }
    
        for ( ; ;) {
            new_handle = rspool_get(rsp, 1);
            if (new_handle == NULL) {
                printf("rspool_get error.\n");
                break;
            }
            //rspool_put(rsp, 0, new_handle);
            printf("===>get: %s\n", new_handle);
        }

        /* group 3 is empty. we test whether the behavior is safe */
        while ((new_handle = rspool_group_foreach(rsp, 3)) != NULL) {
            printf("===>foreach: %s\n", new_handle);
        }


        rspool_del(rsp);
        break;
    }

    return 0;

}

