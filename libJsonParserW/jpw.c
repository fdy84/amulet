/*
 * JSON PASER Wrapped. Version 0.1
 * JSON PASER Wrapped is based on official json parser.
 *    It supports object operation, auto memory management.
 *    You can just only CREATE object and need not to free them, JPW will do it for you.
 * 
 * Author:
 *     <zhangshuo> zhangshuo@staff.sina.com.cn
 * 2009.7.21
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "jpw.h"


/* type strings */
/*
static const char *type_name[] = {
    "Null",
    "Bool",
    "Float",
    "Int",
    "Object",
    "Array",
    "String",
    "Key-Value Pair",
    "Index-Value Pair",
    "Root"
};
*/

struct jpw_gen_t *json_saver(struct jpw_root *, enum jpw_event, struct jpw_value *);
static int json_event_translater(void *, int , const JSON_value *);
static struct jpw_gen_t *jpw_root_new(struct jpw_root *);
static void jpw_root_del(struct jpw_root *, struct jpw_gen_t *);

/* external interface */

struct jpw_root *
jpw_new()
{
    struct jpw_root *jpwp;
    struct jpw_gen_t *root_node;
    
    jpwp = (struct jpw_root *)calloc(1, sizeof(struct jpw_root));
    if (jpwp == NULL) {
        PRINT("Not enough memory.");
        return NULL;
    }

    if ((root_node = jpw_root_new(jpwp)) == NULL)
        return NULL;

    SLIST_INIT(&(jpwp->mlist_head));
    
    /* set current node */
    jpwp->current_node = root_node;

    return jpwp;
}

struct jpw_gen_t *
jpw_parse(struct jpw_root *jpwp, const char *json_str)
{
    int i;
    JSON_config config;  
    struct JSON_parser_struct *jc;

    if (jpwp == NULL || json_str == NULL)
        return NULL;

    init_JSON_config(&config);
    
    config.depth = 50;
    config.callback = &json_event_translater;
    config.callback_ctx = jpwp;
    config.allow_comments = 1;
    config.handle_floats_manually = 0;

    jc = new_JSON_parser(&config);
    if (jc == NULL) {
        PRINT("Not enough memory.");
        return NULL;
    }

    for (i=0; json_str[i] != '\0'; i++) {
        if (!JSON_parser_char(jc, (unsigned char)json_str[i])) {
            PRINT("JSON_parser_char: syntax error, byte %d", i);
            delete_JSON_parser(jc);
            return NULL;
        }
    }

    if (!JSON_parser_done(jc)) {
        PRINT("JSON_parser_end: syntax error");
        delete_JSON_parser(jc);
        return NULL;
    }

    delete_JSON_parser(jc);

    return (struct jpw_gen_t *)jpwp->root->data;
}

int 
jpw_delete(struct jpw_root *jpwp)
{
    if (jpwp == NULL)
        return -1;
    
    jpw_root_del(jpwp, jpwp->root);
    _jpw_delete_all(jpwp);
    free(jpwp);
    
    return 0;
}


static struct jpw_gen_t *
jpw_root_new(struct jpw_root *jpwp)
{
	struct jpw_gen_t *root_node;
	
	if (jpwp == NULL)
		return NULL;
	
	root_node = (struct jpw_gen_t *)malloc(sizeof(struct jpw_gen_t));
	if (root_node == NULL)
		return NULL;
	
    root_node->data_type = JPW_TYPE_ROOT;
    root_node->prev = NULL;
    jpwp->root = root_node;

	return root_node;
}


static void 
jpw_root_del(struct jpw_root *jpwp, struct jpw_gen_t *node)
{
	UNUSED_ARG(jpwp);
    free(node);
}

 
/* 
 * translate JSON_PARSER events to JPW events
 */
static int 
json_event_translater(void *ctx, int type, const JSON_value *value)
{
    struct jpw_ivpair *ivp;
    struct jpw_kvpair *kvp;
    struct jpw_root *jpwp;
    struct jpw_value val;

    jpwp = (struct jpw_root *)ctx;
    
    if (jpwp->current_node == NULL) {
        return -1;
    }
    
    /* if the type is IVPAIR or KVPAIR and their size more than 1 */
    if (jpwp->current_node->data_type == JPW_TYPE_IVPAIR) {
        ivp = (struct jpw_ivpair *)(jpwp->current_node->data);
        if (ivp->size >= 1) {
            json_saver(jpwp, JPW_EV_IVPAIR_END, &val);
            jpwp->current_node = jpwp->current_node->prev;  
        }
    } else if (jpwp->current_node->data_type == JPW_TYPE_KVPAIR) {
        kvp = (struct jpw_kvpair *)(jpwp->current_node->data);
        if (kvp->size >= 1) {
            json_saver(jpwp, JPW_EV_KVPAIR_END, &val);
            jpwp->current_node = jpwp->current_node->prev;   
        }
    }

    /* if the parent is ARRAY, we should set IVPAIR begin */
    if (jpwp->current_node->data_type == JPW_TYPE_ARRAY && type != JSON_T_ARRAY_END) {
        jpwp->current_node = json_saver(jpwp, JPW_EV_IVPAIR_BEGIN, &val);
    }
        
    switch(type) {
        /* containers */
        case JSON_T_ARRAY_BEGIN :
            jpwp->current_node = json_saver(jpwp, JPW_EV_ARRAY_BEGIN, &val);        
            break;

        case JSON_T_ARRAY_END :
            if (jpwp->current_node->data_type == JPW_TYPE_IVPAIR) {
                jpwp->current_node = jpwp->current_node->prev;
                PRINT("Empty ivpair. array closed. remove last ivpair.");
            }
            json_saver(jpwp, JPW_EV_ARRAY_END, &val);
            jpwp->current_node = jpwp->current_node->prev;   
            break;

        case JSON_T_OBJECT_BEGIN :
            jpwp->current_node = json_saver(jpwp, JPW_EV_OBJECT_BEGIN, &val);
            break;

        case JSON_T_OBJECT_END :
            if (jpwp->current_node->data_type == JPW_TYPE_KVPAIR) {
                jpwp->current_node = jpwp->current_node->prev;
                PRINT("Empty kvpair. object closed.");
            }
            json_saver(jpwp, JPW_EV_OBJECT_END, &val);
            jpwp->current_node = jpwp->current_node->prev;
            break;

        case JSON_T_KEY :
            val.v_string = value->vu.str.value;
            jpwp->current_node = json_saver(jpwp, JPW_EV_KVPAIR_BEGIN, &val);
            break;
        
        /* values */
        case JSON_T_INTEGER :
            val.v_int = (intmax_t)(value->vu.integer_value);
            json_saver(jpwp, JPW_EV_INT, &val);
            break;
        case JSON_T_FLOAT :
            val.v_float = value->vu.float_value;
            json_saver(jpwp, JPW_EV_FLOAT, &val);
            break;
        case JSON_T_NULL :
            json_saver(jpwp, JPW_EV_NULL, &val);
            break;
        case JSON_T_TRUE : 
            val.v_int = 1;
            json_saver(jpwp, JPW_EV_BOOL, &val);
            break;
        case JSON_T_FALSE :
            val.v_int = 0;
            json_saver(jpwp, JPW_EV_BOOL, &val);
            break;
        case JSON_T_STRING :
            val.v_string = value->vu.str.value;
            val.v_str_len = value->vu.str.length;
            json_saver(jpwp, JPW_EV_STRING, &val);
            break;
        default :
            PRINT("FATAL ERROR.");
    }
    return 1;
}


/*
 * create new node, and append it to 'current_node'
 * return the new node.
 */
struct jpw_gen_t * 
json_saver(struct jpw_root *jpwp, enum jpw_event type, struct jpw_value *value)
{
    struct jpw_gen_t *gen_node;

    /*
    PRINT("===> parent type: %s[%d], ptr: %p", 
            type_name[(int)(jpwp->current_node->data_type)],
            (int)(jpwp->current_node->data_type),
            jpwp->current_node);
    */

    /* start the test */
    switch(type) {
        case JPW_EV_ARRAY_BEGIN :
            //PRINT("array begin.");      
            /* create a new array and init it */
            if ((gen_node = _jpw_array_new(jpwp)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return gen_node;

        case JPW_EV_ARRAY_END : 
            //PRINT("array end.");   
            /* exit current node, go to parent node */
            if (jpwp->current_node->data_type != JPW_TYPE_ARRAY) {
                PRINT("Json syntax error.");
                return NULL;
            }
            return NULL;

        case JPW_EV_OBJECT_BEGIN :
            //PRINT("object begin."); 
            /* create a new object and init it */
            if ((gen_node = _jpw_object_new(jpwp)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return gen_node;

        case JPW_EV_OBJECT_END :
            //PRINT("object end."); 
            /* exit current node, go to parent node */
            if (jpwp->current_node->data_type != JPW_TYPE_OBJECT) {
                PRINT("Json syntax error.");
                return NULL;
            }
            return NULL;

        case JPW_EV_KVPAIR_BEGIN :
            //PRINT("kvp begin. key:%s", value->v_string); 
            /* create a new kvpair and init it */
            if ((gen_node = _jpw_kvpair_new(jpwp, value->v_string)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }

            if (jpwp->current_node->data_type != JPW_TYPE_OBJECT) {
                PRINT("Json syntax error.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return gen_node; 

        case JPW_EV_KVPAIR_END :
            //PRINT("kvp end."); 
            return NULL;

        case JPW_EV_IVPAIR_BEGIN :
            //PRINT("ivp begin."); 
            /* create a new kvpair and init it */
            if ((gen_node = _jpw_ivpair_new(jpwp)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }

            if (jpwp->current_node->data_type != JPW_TYPE_ARRAY) {
                PRINT("Json syntax error.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return gen_node;

        case JPW_EV_IVPAIR_END :
            //PRINT("ivp end.");
            return NULL;

        case JPW_EV_INT:
            //PRINT("int: %jd", value->v_int);
            /* create a new int and init it */
            if ((gen_node = _jpw_int_new(jpwp, value->v_int)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return NULL;

        case JPW_EV_FLOAT:
            //PRINT("float: %Lf", value->v_float);
            /* create a new float and init it */
            if ((gen_node = _jpw_float_new(jpwp, value->v_float)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return NULL;

        case JPW_EV_NULL :
            //PRINT("null: %s", NULL);
            /* create a new NULL and init it */
            if ((gen_node = _jpw_null_new(jpwp)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return NULL;

        case JPW_EV_BOOL :
            //PRINT("bool: %d", (int)(value->v_int));
            /* create a new bool and init it */
            if ((gen_node = _jpw_bool_new(jpwp, (int)(value->v_int))) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return NULL;

        case JPW_EV_STRING:
            //PRINT("string: %s", value->v_string);
            /* create a new string and init it */
            if ((gen_node = _jpw_string_new(jpwp, value->v_string, value->v_str_len)) == NULL) {
                PRINT("Error. No memory.");
                return NULL;
            }
            _jpw_append(jpwp->current_node, gen_node);
            return NULL;
    
        default:
            PRINT("FATAL ERROR.");
            break;
    }

    return NULL;
}


/**************************************************************
 * JPW API functions
 **************************************************************/

int 
jpw_typeof(struct jpw_root *root, struct jpw_gen_t *obj)
{
    if (root == NULL || obj == NULL)
        return -1;

    return obj->data_type;
}


char *
jpw_tostring(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _data_to_string(root, obj);
}


struct jpw_gen_t *
jpw_object_new(struct jpw_root *root)
{
	return _jpw_object_new(root);
}

size_t 
jpw_object_get_len(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_object_get_size(root, obj);
}

int 
jpw_object_add(struct jpw_root *root, struct jpw_gen_t *obj, const char *key,
				   struct jpw_gen_t *val)
{
	struct jpw_gen_t *kvp;
    int ret;
    
    kvp = _jpw_kvpair_new(root, key);
    if ((ret = _jpw_append(kvp, val)) < 0) {
        PRINT("append error.");
        return ret;
    }

    if ((ret = _jpw_append(obj, kvp)) < 0) {
        PRINT("append error.");
        return ret;
    }

    return ret;
}


struct jpw_gen_t *
jpw_object_get_item(struct jpw_root *root, struct jpw_gen_t *obj, const char *key)
{
    struct jpw_gen_t *kvp;
    kvp = _jpw_object_get_item(root, obj, key); 
    return _jpw_kvpair_get_item(root, kvp);
}


struct jpw_gen_t *
jpw_array_new(struct jpw_root *root)
{
	return _jpw_array_new(root);
}

size_t 
jpw_array_get_len(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_array_get_size(root, obj);
}


int 
jpw_array_add(struct jpw_root *root, struct jpw_gen_t *obj,
				  struct jpw_gen_t *val)
{
    struct jpw_gen_t *ivp;
    int ret;
    
    ivp = _jpw_ivpair_new(root);

    if ((ret = _jpw_append(ivp, val)) < 0) {
        PRINT("append error.");
        return ret;
    }

    if ((ret = _jpw_append(obj, ivp)) < 0) {
        PRINT("append error.");
        return ret;
    }

    return ret;
}

struct jpw_gen_t *
jpw_array_get_item(struct jpw_root *root, struct jpw_gen_t *obj, int idx)
{
	struct jpw_gen_t *ivp;
    ivp = _jpw_array_get_item(root, obj, idx); 
    return _jpw_ivpair_get_item(root, ivp);
}

struct jpw_gen_t *
jpw_boolean_new(struct jpw_root *root, boolean b)
{
    return _jpw_bool_new(root, b);
}

boolean 
jpw_boolean_get_val(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_bool_get_value(root, obj);
}

struct jpw_gen_t *
jpw_int_new(struct jpw_root *root, intmax_t value)
{
    return _jpw_int_new(root, value);
}

intmax_t 
jpw_int_get_val(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_int_get_value(root, obj);	
}

struct jpw_gen_t *
jpw_float_new(struct jpw_root *root, long double value)
{
    return _jpw_float_new(root, value);
}

long double 
jpw_float_get_val(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_float_get_value(root, obj);
}

struct jpw_gen_t *
jpw_string_new(struct jpw_root *root, const char *s)
{
    return _jpw_string_new(root, s, 0);
}

struct jpw_gen_t *
jpw_string_len_new(struct jpw_root *root, char *s, size_t len)
{
    return _jpw_string_new(root, s, len);
}

char *
jpw_string_get_str(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_string_get_string(root, obj);
}

size_t 
jpw_string_get_len(struct jpw_root *root, struct jpw_gen_t *obj)
{
    return _jpw_string_get_length(root, obj);
}





