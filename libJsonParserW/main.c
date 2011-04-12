/* 
 * test file for JPW
 *
 * Avin <zhangshuo@staff.sina.com.cn>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jpw.h"

#ifndef UNUSED_ARG
#define UNUSED_ARG(a) (void)(a)
#endif

int 
main(int argc, char **argv)
{
    UNUSED_ARG(argv);
    UNUSED_ARG(argc);
    
    struct jpw_root *json_handle;
    struct jpw_gen_t *root;
    char *buf;
    
    unsigned char json_str[] = ""
"{\"command\":\"get_groups\", \"groups\":[\"main\", {\"haha\":990}, \"grp2\", 123.34, 456343459, [1,2,3]]}";
	json_str[13] = 129;


	printf("Source Json String: \n%s\n\n", json_str);
    
//int i=0;
//while (i++ < 10000000) {
    
    json_handle = jpw_new();
    if (json_handle == NULL) {
        printf("no memory.");
        return 0;
    }
    
    root = jpw_parse(json_handle, json_str);
    buf = jpw_tostring(json_handle, root);
    printf("Root To String: \n%s\n\n", buf);
    
    /* get values */
    struct jpw_gen_t *_str;
    _str = jpw_object_get_item(json_handle, root, "command");
    buf = jpw_tostring(json_handle, _str);
    printf("Get key('command'): %s\n\n", buf);
    
    struct jpw_gen_t *key_groups_array;
    key_groups_array = jpw_object_get_item(json_handle, root, "groups");
    
    struct jpw_gen_t *key_groups_array_arg1;
    key_groups_array_arg1 = jpw_array_get_item(json_handle, key_groups_array, 0);
    buf = jpw_tostring(json_handle, key_groups_array_arg1);
    printf("Get key('groups')->array[1]: %s\n\n", buf);
    
    struct jpw_gen_t *key_groups_array_arg2;
    key_groups_array_arg2 = jpw_array_get_item(json_handle, key_groups_array, 1);
    buf = jpw_tostring(json_handle, key_groups_array_arg2);
    printf("Get key('groups')->array[2]: %s\n\n", buf);
    
    struct jpw_gen_t *key_groups_array_arg2_key_haha;
    key_groups_array_arg2_key_haha = jpw_object_get_item(json_handle, key_groups_array_arg2, "haha");
    buf = jpw_tostring(json_handle, key_groups_array_arg2_key_haha);
    printf("Get key('groups')->array[2]->key('haha'): %s\n\n", buf);
    
    intmax_t num_int;
    num_int = jpw_int_get_val(json_handle, key_groups_array_arg2_key_haha);
    printf("Get Int: %jd\n\n", num_int);

	/* objects */
    struct jpw_gen_t *arr;
    struct jpw_gen_t *obj;
    struct jpw_gen_t *v_int;
    struct jpw_gen_t *v_float;
    struct jpw_gen_t *v_str;
    struct jpw_gen_t *v_bool;
    struct jpw_gen_t *arr2;

    char haha[] = "my\" j\\son \t en/code string ppp";
    haha[strlen(haha) - 1] = 7;		/* special charactor encoding test */
    haha[strlen(haha) - 5] = 180;		/* special charactor encoding test */

    arr2 = jpw_array_new(json_handle);
    v_int = jpw_int_new(json_handle, 12345678910);
    v_float = jpw_float_new(json_handle, 1234567890.123456);
    v_str = jpw_string_new(json_handle, haha);
    v_bool = jpw_boolean_new(json_handle, 0);

    if (jpw_array_add(json_handle, arr2, v_int) < 0) 
        printf("jpw_obj_array_add error.\n");
    if (jpw_array_add(json_handle, arr2, v_float))
        printf("jpw_obj_array_add error.\n");
    if (jpw_array_add(json_handle, arr2, v_str))
        printf("jpw_obj_array_add error.\n");
    if (jpw_array_add(json_handle, arr2, v_bool))
        printf("jpw_obj_array_add error.\n");

    obj = jpw_object_new(json_handle);
    if (jpw_object_add(json_handle, obj, "key_\tof_array", arr2) < 0)
        printf("jpw_obj_object_add error.\n");

    arr = jpw_array_new(json_handle);
    if (jpw_array_add(json_handle, arr, obj) < 0) 
        printf("jpw_obj_array_add error.\n");

    buf = jpw_tostring(json_handle, arr);
    printf("My Json String: \n%s\n\n", buf); 
    

    jpw_delete(json_handle);

//}

    printf("#### end ####\n");
    return 0;
    
}


