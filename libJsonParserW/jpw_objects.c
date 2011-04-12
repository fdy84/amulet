/* 
 * this is objects implementation for JPW
 *
 * Avin <zhangshuo@staff.sina.com.cn>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "json_parser.h"
#include "jpw_objects.h"

#define JPW_VALUE_BUF_SIZE		102400
#define JPW_CONTAINER_BUF_SIZE	102400

const char *json_hex_chars = "0123456789abcdef";

static int jpw_escape_str(char *, char *);
static char *_data_to_string_internal(struct jpw_root *, struct jpw_gen_t *);

RB_PROTOTYPE_STATIC(rbt, jpw_gen_t, rbt_entries, jpw_cmp);
RB_PROTOTYPE_STATIC(rbt2, jpw_gen_t, rbt_entries, jpw_cmp);

/* comparasion function for red-black tree */
static int
jpw_cmp(struct jpw_gen_t *a, struct jpw_gen_t *b)
{
	int diff;
    struct jpw_string *str_a;
    struct jpw_string *str_b;
    
    if (a->data_type == JPW_TYPE_KVPAIR) {
        str_a = (struct jpw_string *)(((struct jpw_kvpair *)(a->data))->v_key->data);
        str_b = (struct jpw_string *)(((struct jpw_kvpair *)(b->data))->v_key->data);
        diff = strncmp(str_a->str, str_b->str, str_a->len);     
    } else if (a->data_type == JPW_TYPE_IVPAIR) {
        diff = ((struct jpw_ivpair *)(a->data))->v_index - \
               ((struct jpw_ivpair *)(b->data))->v_index;
    } else {
        PRINT("type error.");
    }

	if (diff == 0)
		return 0;
	else if (diff < 0)
		return -1;
	else
		return 1;
}

RB_GENERATE_STATIC(rbt, jpw_gen_t, rbt_entries, jpw_cmp);
RB_GENERATE_STATIC(rbt2, jpw_gen_t, rbt_entries, jpw_cmp);

int 
_jpw_append(struct jpw_gen_t *obj, struct jpw_gen_t *append)
{
	if (obj == NULL || append == NULL) {
		PRINT("parameter invalid.");
		return JPW_PARA_INVALID;
	}
	
	switch (obj->data_type) {
		case JPW_TYPE_ROOT :
			obj->data = append;
			append->prev = obj;
			break;
			
		case JPW_TYPE_OBJECT :
			/* only KVPAIR should be appended into OBJECT, check */
			if (append->data_type != JPW_TYPE_KVPAIR) {
				PRINT("Type error. OBJECT should append KVPAIR.");
				return JPW_TYPE_ERROR;
			}
			RB_INSERT(rbt, &(((struct jpw_object *)(obj->data))->obj_head), append);
            (((struct jpw_object *)(obj->data))->size)++;
            append->prev = obj;
            break;
            
		case JPW_TYPE_ARRAY :
			/* only IVPAIR should be appended into ARRAY */
			if (append->data_type != JPW_TYPE_IVPAIR) {
				PRINT("Type error. ARRAY should append IVPAIR.");
				return JPW_TYPE_ERROR;
			}
			/* set the IVPAIR index via array size. auto incease */
			((struct jpw_ivpair *)(append->data))->v_index = ((struct jpw_array *)(obj->data))->size;
			/* append data into array */
			RB_INSERT(rbt2, &(((struct jpw_array *)(obj->data))->arr_head), append);
            (((struct jpw_array *)(obj->data))->size)++;
            append->prev = obj;
            break;
            
		case JPW_TYPE_KVPAIR :
			if (((struct jpw_kvpair *)(obj->data))->size >= 1) {
				PRINT("KVPAIR can only be appended 1 item.");
				return JPW_OVER_LIMIT;
			}
			((struct jpw_kvpair *)(obj->data))->v_data = append;
			append->prev = obj;
			((struct jpw_kvpair *)(obj->data))->size++;
			break;
			
		case JPW_TYPE_IVPAIR :
			if (((struct jpw_ivpair *)(obj->data))->size >= 1) {
				PRINT("IVPAIR can only be appended 1 item.");
				return JPW_OVER_LIMIT;
			}
			((struct jpw_ivpair *)(obj->data))->v_data = append;
			append->prev = obj;
			((struct jpw_ivpair *)(obj->data))->size++;
			break;
			
		default :
			PRINT("Type error. This type(%d) has NO append function.", obj->data_type);
			return JPW_TYPE_ERROR;
	}
	
	/* increase the referece count */
	append->ref_count++;
	//PRINT("new RC: %d", append->ref_count);
	
	return JPW_OK;	
}


struct jpw_gen_t *
_jpw_array_new(struct jpw_root *jpwp)
{
	struct jpw_gen_t *gen_node;
	struct jpw_array *array_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	array_node = (struct jpw_array *)malloc(sizeof(struct jpw_array));
	if (gen_node == NULL || array_node == NULL)
		return NULL;
		
	/* init gen_node */
	gen_node->data_type = JPW_TYPE_ARRAY;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init array node */
	array_node->size = 0;
	RB_INIT(&(array_node->arr_head));
	/* attach real data */
	gen_node->data = array_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_array_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_ARRAY) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_array *)(node->data));
	free(node);
}


struct jpw_gen_t *
_jpw_array_get_item(struct jpw_root *jpwp, struct jpw_gen_t *obj, int idx)
{
	UNUSED_ARG(jpwp);
	
	struct jpw_gen_t *r_kvp;
	
	struct jpw_gen_t gen_iv_find;
	struct jpw_ivpair iv_find;
	
	if (jpwp == NULL || obj == NULL)
        return NULL;
    
    if (obj->data_type != JPW_TYPE_ARRAY) {
		PRINT("type error.");
		return NULL;
	}
	
	iv_find.v_index = idx;
	gen_iv_find.data = &(iv_find);
	gen_iv_find.data_type = JPW_TYPE_IVPAIR;
	
	r_kvp = RB_FIND(rbt2, &(((struct jpw_array *)(obj->data))->arr_head), &gen_iv_find);
	return r_kvp;
}


size_t 
_jpw_array_get_size(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_ARRAY) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_array *)(obj->data))->size;
}


struct jpw_gen_t *
_jpw_object_new(struct jpw_root *jpwp)
{
	struct jpw_gen_t *gen_node;
	struct jpw_object *object_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	object_node = (struct jpw_object *)malloc(sizeof(struct jpw_object));
	if (gen_node == NULL || object_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_OBJECT;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init object node */
	object_node->size = 0;
	RB_INIT(&(object_node->obj_head));
	/* attach real data */
	gen_node->data = object_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_object_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_OBJECT) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_object *)(node->data));
	free(node);
}


struct jpw_gen_t *
_jpw_object_get_item(struct jpw_root *jpwp, struct jpw_gen_t *obj, const char *key)
{
	UNUSED_ARG(jpwp);
    struct jpw_gen_t *ret;
	
	struct jpw_gen_t gen_kv_find;
	struct jpw_kvpair kv_find;
	
	struct jpw_gen_t gen_str_find;
	struct jpw_string str_find; 
    char *key_buf;
    size_t key_len;

	if (jpwp == NULL || obj == NULL || key == NULL)
        return NULL;
    
    if (obj->data_type != JPW_TYPE_OBJECT) {
		PRINT("type error.");
		return NULL;
	}

    key_len = strlen(key);
    key_buf = (char *)malloc(key_len + 1);
    strncpy(key_buf, key, key_len);
	
	str_find.str = key_buf;
	str_find.len = key_len;
	gen_str_find.data = &str_find;
	gen_str_find.data_type = JPW_TYPE_STRING;
	
	kv_find.v_key = &gen_str_find;
	gen_kv_find.data = &kv_find;
	gen_kv_find.data_type = JPW_TYPE_KVPAIR;

    ret = RB_FIND(rbt, &(((struct jpw_object *)(obj->data))->obj_head), &gen_kv_find);
    free(key_buf);
	
	return ret;
}


size_t 
_jpw_object_get_size(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_OBJECT) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_object *)(obj->data))->size;
}


struct jpw_gen_t *
_jpw_kvpair_new(struct jpw_root *jpwp, const char *key)
{
	struct jpw_gen_t *gen_node;
	struct jpw_gen_t *key_node;
	struct jpw_kvpair *kvpair_node;
	
	if (jpwp == NULL || key == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	kvpair_node = (struct jpw_kvpair *)malloc(sizeof(struct jpw_kvpair));
	key_node = _jpw_string_new(jpwp, key, strlen(key));
	if (gen_node == NULL || kvpair_node == NULL || key_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_KVPAIR;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init kvpair node */
	kvpair_node->v_key = key_node;
	kvpair_node->size = 0;
	/* attach real data */
	gen_node->data = kvpair_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_kvpair_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_KVPAIR) {
		PRINT("type error.");
		return;
	}

	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_kvpair *)(node->data));
	free(node);
}

struct jpw_gen_t *
_jpw_kvpair_get_item(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return NULL;
    
    if (obj->data_type != JPW_TYPE_KVPAIR) {
		PRINT("type error.");
		return NULL;
	}
	
	return ((struct jpw_kvpair *)(obj->data))->v_data;
}

size_t 
_jpw_kvpair_get_size(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_KVPAIR) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_kvpair *)(obj->data))->size;
}


struct jpw_gen_t *
_jpw_ivpair_new(struct jpw_root *jpwp)
{
	struct jpw_gen_t *gen_node;
	struct jpw_ivpair *ivpair_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	ivpair_node = (struct jpw_ivpair *)malloc(sizeof(struct jpw_ivpair));
	if (gen_node == NULL || ivpair_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_IVPAIR;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init ivpair node */
	ivpair_node->v_index = -1;
	ivpair_node->size = 0;
	/* attach real data */
	gen_node->data = ivpair_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_ivpair_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_IVPAIR) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_ivpair *)(node->data));
	free(node);
}

struct jpw_gen_t *
_jpw_ivpair_get_item(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);

    if (jpwp == NULL || obj == NULL)
        return NULL;
	
	if (obj->data_type != JPW_TYPE_IVPAIR) {
		PRINT("type error.");
		return NULL;
	}
	
	return ((struct jpw_ivpair *)(obj->data))->v_data;
}

size_t 
_jpw_ivpair_get_size(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_IVPAIR) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_ivpair *)(obj->data))->size;
}


struct jpw_gen_t *
_jpw_string_new(struct jpw_root *jpwp, const char *str, size_t len)
{
	struct jpw_gen_t *gen_node;
	struct jpw_string *string_node;
	char *d_str;
    size_t str_len;
	
	if (jpwp == NULL || str == NULL)
		return NULL;
	
    str_len = (len == 0 ? strlen(str) : len);
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	string_node = (struct jpw_string *)malloc(sizeof(struct jpw_string));
	d_str = (char *)malloc(str_len + 1);
	if (gen_node == NULL || string_node == NULL || d_str == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_STRING;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init string node */
    strncpy(d_str, str, str_len);
    d_str[str_len] = '\0';
	string_node->str = d_str;
	string_node->len = str_len; 
	/* attach real data */
	gen_node->data = string_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
    
    return gen_node;
}

void 
_jpw_string_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_STRING) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free(((struct jpw_string *)(node->data))->str);
	free((struct jpw_string *)(node->data));
	free(node);
}

char *
_jpw_string_get_string(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return NULL;
    
    if (obj->data_type != JPW_TYPE_STRING) {
		PRINT("type error.");
		return NULL;
	}
	
	return ((struct jpw_string *)(obj->data))->str;
}

size_t 
_jpw_string_get_length(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_STRING) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_string *)(obj->data))->len;
}


struct jpw_gen_t *
_jpw_bool_new(struct jpw_root *jpwp, int v_bool)
{
	struct jpw_gen_t *gen_node;
	struct jpw_bool *bool_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	bool_node = (struct jpw_bool *)malloc(sizeof(struct jpw_bool));
	if (gen_node == NULL || bool_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_BOOLEAN;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init string node */
	bool_node->v_bool = (v_bool >= 1 ? 1 : 0);
	/* attach real data */
	gen_node->data = bool_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_bool_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_BOOLEAN) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_bool *)(node->data));
	free(node);
}

int
_jpw_bool_get_value(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return -1;
    
    if (obj->data_type != JPW_TYPE_BOOLEAN) {
		PRINT("type error.");
		return -1;
	}
	
	return ((struct jpw_bool *)(obj->data))->v_bool;
}


struct jpw_gen_t *
_jpw_float_new(struct jpw_root *jpwp, long double v_float)
{
	struct jpw_gen_t *gen_node;
	struct jpw_float *float_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	float_node = (struct jpw_float *)malloc(sizeof(struct jpw_float));
	if (gen_node == NULL || float_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_FLOAT;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init string node */
	float_node->v_float = v_float;
	/* attach real data */
	gen_node->data = float_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_float_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_FLOAT) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_float *)(node->data));
	free(node);
}

long double
_jpw_float_get_value(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_FLOAT) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_float *)(obj->data))->v_float;
}


struct jpw_gen_t *
_jpw_int_new(struct jpw_root *jpwp, intmax_t v_int)
{
	struct jpw_gen_t *gen_node;
	struct jpw_int *int_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	int_node = (struct jpw_int *)malloc(sizeof(struct jpw_int));
	if (gen_node == NULL || int_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_INT;
	gen_node->prev = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* init string node */
	int_node->v_int = v_int;
	/* attach real data */
	gen_node->data = int_node;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_int_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_INT) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free((struct jpw_int *)(node->data));
	free(node);
}

intmax_t
_jpw_int_get_value(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	
	if (jpwp == NULL || obj == NULL)
        return 0;
    
    if (obj->data_type != JPW_TYPE_INT) {
		PRINT("type error.");
		return 0;
	}
	
	return ((struct jpw_int *)(obj->data))->v_int;
}


struct jpw_gen_t *
_jpw_null_new(struct jpw_root *jpwp)
{
	struct jpw_gen_t *gen_node;
	
	if (jpwp == NULL)
		return NULL;
	
	gen_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	if (gen_node == NULL)
		return NULL;

	/* init gen_node */
	gen_node->data_type = JPW_TYPE_NULL;
	gen_node->prev = NULL;
	gen_node->data = NULL;
	gen_node->ref_count = 1;
	gen_node->s_buf = NULL;
	/* insert this node into memory list */
	SLIST_INSERT_HEAD(&(jpwp->mlist_head), gen_node, sl_entries);
	
	return gen_node;
}

void 
_jpw_null_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    if (jpwp == NULL || node == NULL)
        return;
	
	if (node->data_type != JPW_TYPE_NULL) {
		PRINT("type error.");
		return;
	}
	
	if (node->s_buf != NULL)
		free(node->s_buf);
	free(node);
}

void * 
_jpw_null_get_value(struct jpw_root *jpwp, struct jpw_gen_t *obj)
{
	UNUSED_ARG(jpwp);
	UNUSED_ARG(obj);
	
	return NULL;
}


void 
_jpw_delete_all(struct jpw_root *jpwp)
{
	struct jpw_gen_t *node;
	struct jpw_gen_t *temp_node;

    if (jpwp == NULL)
        return;
	
	SLIST_FOREACH_SAFE(node, &(jpwp->mlist_head), sl_entries, temp_node) {
		SLIST_REMOVE(&(jpwp->mlist_head), node, jpw_gen_t, sl_entries);
		_jpw_obj_delete(jpwp, node);
	}
}


void 
_jpw_obj_delete(struct jpw_root *root, struct jpw_gen_t *obj)
{
	if (root == NULL || obj == NULL) 
		return;
	
	switch ((int)(obj->data_type)) {
        case JPW_TYPE_NULL :
            _jpw_null_del(root, obj);
            break;
        case JPW_TYPE_BOOLEAN :
            _jpw_bool_del(root, obj);
            break;
        case JPW_TYPE_FLOAT :
            _jpw_float_del(root, obj);
            break;
        case JPW_TYPE_INT :
            _jpw_int_del(root,obj);
            break;
        case JPW_TYPE_OBJECT :
            _jpw_object_del(root, obj);
            break;
        case JPW_TYPE_ARRAY :
            _jpw_array_del(root, obj);
            break;
        case JPW_TYPE_STRING :
            _jpw_string_del(root, obj);
            break;
        case JPW_TYPE_KVPAIR :
            _jpw_kvpair_del(root, obj);
            break;
        case JPW_TYPE_IVPAIR :
            _jpw_ivpair_del(root, obj);
            break;
    }
}


char *
_data_to_string(struct jpw_root *jpwp, struct jpw_gen_t *node)
{ 
    if (node == NULL || jpwp == NULL)
        return NULL;

    if (node->s_buf != NULL) {
        free(node->s_buf);
    }
	node->s_buf = _data_to_string_internal(jpwp, node);

	return node->s_buf;
}


static char *
_data_to_string_internal(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
    int v_bool;
    intmax_t v_int;
    char *v_str;
    long double v_float;
    char *buf;
    char *temp_str;
    struct jpw_gen_t *t_node;

    if (jpwp == NULL || node == NULL) {
		return NULL;
	}
    
    switch(node->data_type) {
        case JPW_TYPE_NULL :
            buf = (char *)malloc(JPW_VALUE_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            strncpy(buf, "NULL", JPW_VALUE_BUF_SIZE);
            return buf;

        case JPW_TYPE_BOOLEAN :
            buf = (char *)malloc(JPW_VALUE_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            v_bool = ((struct jpw_bool *)(node->data))->v_bool;
            strncpy(buf, (v_bool==1 ? "True" : "False"), JPW_VALUE_BUF_SIZE);
            return buf;

        case JPW_TYPE_FLOAT :
            buf = (char *)malloc(JPW_VALUE_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            v_float = ((struct jpw_float *)(node->data))->v_float;
            snprintf(buf, JPW_VALUE_BUF_SIZE, "%Lf", v_float);
            return buf;

        case JPW_TYPE_INT :
            buf = (char *)malloc(JPW_VALUE_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            v_int = ((struct jpw_int *)(node->data))->v_int;
            snprintf(buf, JPW_VALUE_BUF_SIZE, "%jd", v_int);
            return buf;
            
		case JPW_TYPE_STRING :
            buf = (char *)malloc(JPW_VALUE_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            v_str = ((struct jpw_string *)(node->data))->str;
            strncpy(buf, "\"", JPW_VALUE_BUF_SIZE);
            jpw_escape_str(buf, v_str);
            //strncat(buf, v_str, JPW_VALUE_BUF_SIZE);
            strncat(buf, "\"", JPW_VALUE_BUF_SIZE);
            return buf;

        case JPW_TYPE_OBJECT :
            buf = (char *)malloc(JPW_CONTAINER_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            strncpy(buf, "{", JPW_CONTAINER_BUF_SIZE);
            RB_FOREACH(t_node, rbt, &(((struct jpw_object *)(node->data))->obj_head)) {
                if ((temp_str = _data_to_string_internal(jpwp, t_node)) == NULL)
					return NULL;
                strncat(buf, temp_str, JPW_CONTAINER_BUF_SIZE);
                strncat(buf, ", ", JPW_CONTAINER_BUF_SIZE);
                free(temp_str);
            }
            buf[strlen(buf) - 2] = '\0';    // remove last comma ',' 
            strncat(buf, "}", JPW_CONTAINER_BUF_SIZE);
            return buf;

        case JPW_TYPE_ARRAY :
            buf = (char *)malloc(JPW_CONTAINER_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            strncpy(buf, "[", JPW_CONTAINER_BUF_SIZE);
            RB_FOREACH(t_node, rbt2, &(((struct jpw_array *)(node->data))->arr_head)) {
                if ((temp_str = _data_to_string_internal(jpwp, t_node)) == NULL)
					return NULL;
                strncat(buf, temp_str, JPW_CONTAINER_BUF_SIZE);
                strncat(buf, ", ", JPW_CONTAINER_BUF_SIZE);
                free(temp_str);
            }
            buf[strlen(buf) - 2] = '\0';    // remove last comma ',' 
            strncat(buf, "]", JPW_CONTAINER_BUF_SIZE);
            return buf;

        case JPW_TYPE_KVPAIR :
            buf = (char *)malloc(JPW_CONTAINER_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            temp_str = _data_to_string_internal(jpwp, ((struct jpw_kvpair *)(node->data))->v_key);
            if (temp_str == NULL) 
				return NULL;
            snprintf(buf, JPW_CONTAINER_BUF_SIZE, "%s: ", temp_str);
            free(temp_str);
            t_node = ((struct jpw_kvpair *)(node->data))->v_data;
            if ((temp_str = _data_to_string_internal(jpwp, t_node)) == NULL) 
				return NULL;
            strncat(buf, temp_str, JPW_CONTAINER_BUF_SIZE);
            free(temp_str);
            return buf;

        case JPW_TYPE_IVPAIR :
            buf = (char *)malloc(JPW_CONTAINER_BUF_SIZE + 1);
            if (buf == NULL)
				return NULL;
            t_node = ((struct jpw_ivpair *)(node->data))->v_data;
            if ((temp_str = _data_to_string_internal(jpwp, t_node)) == NULL)
				return NULL;
            strncpy(buf, temp_str, JPW_CONTAINER_BUF_SIZE);
            free(temp_str);
            return buf;

        default :
            PRINT("some error occoured. node type is not recognized.");
            return NULL;
    }
    
    return NULL;
}


/* 
 * this function comes from JSON-C 0.8
 * escape json special charactors
 */
#define ENCODE_BUF_LEN		16
static int 
jpw_escape_str(char *buf, char *str)
{
	int pos = 0;
	int start_offset = 0;
	unsigned char c;
	char encode[ENCODE_BUF_LEN];
  
	do {
		c = str[pos];
		switch(c) {
			case '\0':
				break;
			case '\b':
			case '\n':
			case '\r':
			case '\t':
			case '"':
			case '\\':
			case '/':
				if(pos - start_offset > 0)
					strncat(buf, str + start_offset, pos - start_offset);
				if(c == '\b') strncat(buf, "\\b", JPW_VALUE_BUF_SIZE);
				else if(c == '\n') strlcat(buf, "\\n", JPW_VALUE_BUF_SIZE);
				else if(c == '\r') strlcat(buf, "\\r", JPW_VALUE_BUF_SIZE);
				else if(c == '\t') strlcat(buf, "\\t", JPW_VALUE_BUF_SIZE);
				else if(c == '"') strlcat(buf, "\\\"", JPW_VALUE_BUF_SIZE);
				else if(c == '\\') strlcat(buf, "\\\\", JPW_VALUE_BUF_SIZE);
				else if(c == '/') strlcat(buf, "\\/", JPW_VALUE_BUF_SIZE);
				start_offset = ++pos;
				break;
			default:
				if(c < ' ') {
					if(pos - start_offset > 0)
						strncat(buf, str + start_offset, pos - start_offset);
						snprintf(encode, ENCODE_BUF_LEN, "\\u00%c%c", json_hex_chars[c >> 4], json_hex_chars[c & 0xf]);
						strncat(buf, encode, ENCODE_BUF_LEN);
						start_offset = ++pos;
					} else pos++;
				}
	} while(c);
  
	if(pos - start_offset > 0)
		strncat(buf, str + start_offset, pos - start_offset);

	return 0;
}


void 
_jpw_cleanup(struct jpw_root *jpwp, struct jpw_gen_t *node)
{ 
    struct jpw_gen_t *t_node;

    if (jpwp == NULL || node == NULL) {
		return;
	}
    
    switch(node->data_type) {
        case JPW_TYPE_NULL :
            _jpw_null_del(jpwp, node);
            return;

        case JPW_TYPE_BOOLEAN :
            _jpw_bool_del(jpwp, node);
            return;

        case JPW_TYPE_FLOAT :
            _jpw_float_del(jpwp, node);
            return;

        case JPW_TYPE_INT :
            _jpw_int_del(jpwp, node);
            return;
            
		case JPW_TYPE_STRING :
            _jpw_string_del(jpwp, node);
            return;

        case JPW_TYPE_OBJECT :
            RB_FOREACH(t_node, rbt, &(((struct jpw_object *)(node->data))->obj_head)) {
                _jpw_cleanup(jpwp, t_node);
            }
			_jpw_object_del(jpwp, node);
            return;

        case JPW_TYPE_ARRAY :
            RB_FOREACH(t_node, rbt2, &(((struct jpw_array *)(node->data))->arr_head)) {
                _jpw_cleanup(jpwp, t_node);
            }
            _jpw_array_del(jpwp, node);
            return;

        case JPW_TYPE_KVPAIR : 
            t_node = ((struct jpw_kvpair *)(node->data))->v_data;
            _jpw_cleanup(jpwp, t_node);
            _jpw_kvpair_del(jpwp, node);
            return;

        case JPW_TYPE_IVPAIR :
            t_node = ((struct jpw_ivpair *)(node->data))->v_data;
            _jpw_cleanup(jpwp, t_node);
            _jpw_ivpair_del(jpwp, node);
            return;

        default :
            PRINT("some error occoured. node type is not recognized.");
            return;
    }
    
    return;
}

