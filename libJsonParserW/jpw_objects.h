#ifndef __JPW_OBJ_H_
#define __JPW_OBJ_H_

#include <stdint.h>

#include "tree.h"
#include "queue.h"


#ifndef UNUSED_ARG
#define UNUSED_ARG(a) (void)(a)
#endif

#ifndef PRINT
#define PRINT(fmt, arg...) \
		printf("%s[%d]: "#fmt"\n", __func__, __LINE__, ##arg)
#endif


enum jpw_type {
    JPW_TYPE_NULL,
    JPW_TYPE_BOOLEAN,
    JPW_TYPE_FLOAT,
    JPW_TYPE_INT,
    JPW_TYPE_OBJECT,
    JPW_TYPE_ARRAY,
    JPW_TYPE_STRING,
    JPW_TYPE_KVPAIR,
    JPW_TYPE_IVPAIR,
    JPW_TYPE_ROOT
};


/********************************************
 * Tree and Queue Nodes
 ********************************************/

/* data node for RB TREE */

/*
struct jpw_node {
	enum jpw_type v_type;
    void *v_data;
    RB_ENTRY(jpw_node) rbt_entries; 
};
*/

/********************************************
 * json basic types
 ********************************************/

/* JSON type string */
struct jpw_string {
    char *str;
    size_t len;
}; 

/* JSON type bool */
struct jpw_bool {
    int v_bool;
};

/* JSON type float */
struct jpw_float {
    long double v_float;
};

/* JSON type int */
struct jpw_int {
    intmax_t v_int;
};


/********************************************
 * json general type
 ********************************************/

/* general type */
struct jpw_gen_t {
    enum jpw_type data_type;
    void *data;
    struct jpw_gen_t *prev;
    int ref_count;
    char *s_buf;
    
    /* only used for type: array, object */
    RB_ENTRY(jpw_gen_t) rbt_entries; 
    
    /* memory management node */
    SLIST_ENTRY(jpw_gen_t) sl_entries;
};

/********************************************
 * json container types
 ********************************************/

/* JSON type key-value for object */
struct jpw_kvpair {
    struct jpw_gen_t *v_key;
    struct jpw_gen_t *v_data;
    size_t size;
};

/* JSON type index-value for array */
struct jpw_ivpair {
    int v_index;
    struct jpw_gen_t *v_data;
    size_t size;
};


/* JSON type object */
struct jpw_object {
    RB_HEAD(rbt, jpw_gen_t) obj_head;
    size_t size;
};


/* JSON type array */
struct jpw_array {
    RB_HEAD(rbt2, jpw_gen_t) arr_head;
    size_t size;
};


/********************************************
 * JPW handle type
 ********************************************/

struct jpw_root {
    
    enum jpw_type v_type;
    void *v_data;
    
    struct jpw_gen_t *root;
    struct jpw_gen_t *current_node;

    //const char *error_msg;
    SLIST_HEAD(mlist, jpw_gen_t) mlist_head;
};


/********************************************
 * funcitons
 ********************************************/
 
#define JPW_OK				0
#define JPW_PARA_INVALID	-1
#define JPW_TYPE_ERROR		-2 
#define JPW_OVER_LIMIT		-3

/* appending */
int _jpw_append(struct jpw_gen_t *, struct jpw_gen_t *);

/* create */
struct jpw_gen_t *_jpw_array_new(struct jpw_root *);
struct jpw_gen_t *_jpw_object_new(struct jpw_root *);
struct jpw_gen_t *_jpw_kvpair_new(struct jpw_root *, const char *);
struct jpw_gen_t *_jpw_ivpair_new(struct jpw_root *);
struct jpw_gen_t *_jpw_string_new(struct jpw_root *, const char *, size_t);
struct jpw_gen_t *_jpw_bool_new(struct jpw_root *, int);
struct jpw_gen_t *_jpw_float_new(struct jpw_root *, long double);
struct jpw_gen_t *_jpw_int_new(struct jpw_root *, intmax_t);
struct jpw_gen_t *_jpw_null_new(struct jpw_root *);

/* destory */
void _jpw_array_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_object_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_kvpair_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_ivpair_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_string_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_bool_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_float_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_int_del(struct jpw_root *, struct jpw_gen_t *);
void _jpw_null_del(struct jpw_root *, struct jpw_gen_t *);

/* manage */
struct jpw_gen_t *_jpw_array_get_item(struct jpw_root *, struct jpw_gen_t *, int);
size_t _jpw_array_get_size(struct jpw_root *, struct jpw_gen_t *);

struct jpw_gen_t *_jpw_object_get_item(struct jpw_root *, struct jpw_gen_t *, const char *);
size_t _jpw_object_get_size(struct jpw_root *, struct jpw_gen_t *);

struct jpw_gen_t *_jpw_kvpair_get_item(struct jpw_root *, struct jpw_gen_t *);
size_t _jpw_kvpair_get_size(struct jpw_root *, struct jpw_gen_t *);

struct jpw_gen_t *_jpw_ivpair_get_item(struct jpw_root *, struct jpw_gen_t *);
size_t _jpw_ivpair_get_size(struct jpw_root *, struct jpw_gen_t *);

char *_jpw_string_get_string(struct jpw_root *, struct jpw_gen_t *);
size_t _jpw_string_get_length(struct jpw_root *, struct jpw_gen_t *);

int _jpw_bool_get_value(struct jpw_root *, struct jpw_gen_t *);
long double _jpw_float_get_value(struct jpw_root *, struct jpw_gen_t *);
intmax_t _jpw_int_get_value(struct jpw_root *, struct jpw_gen_t *);
void *_jpw_null_get_value(struct jpw_root *, struct jpw_gen_t *);

/* utils */
void _jpw_delete_all(struct jpw_root *);
void _jpw_obj_delete(struct jpw_root *, struct jpw_gen_t *);
char *_data_to_string(struct jpw_root *, struct jpw_gen_t *);
void _jpw_cleanup(struct jpw_root *, struct jpw_gen_t *);




#endif

