#ifndef __JPW_H_
#define __JPW_H_

#include "json_parser.h"
#include "jpw_objects.h"
#include "tree.h"
#include "queue.h"


#ifndef UNUSED_ARG
#define UNUSED_ARG(a) (void)(a)
#endif

typedef int     boolean;


#ifndef PRINT
#define PRINT(fmt, arg...) \
		printf("%s[%d]: "#fmt"\n", __func__, __LINE__, ##arg)
#endif


enum jpw_event {
    /* container, edge driven event */
    JPW_EV_ARRAY_BEGIN,
    JPW_EV_ARRAY_END,
    JPW_EV_OBJECT_BEGIN,
    JPW_EV_OBJECT_END,
    JPW_EV_KVPAIR_BEGIN,
    JPW_EV_KVPAIR_END,
    JPW_EV_IVPAIR_BEGIN,
    JPW_EV_IVPAIR_END,
    /* value, state driven event */
    JPW_EV_STRING,
    JPW_EV_BOOL,
    JPW_EV_FLOAT,
    JPW_EV_INT,
    JPW_EV_NULL
};

struct jpw_value {
    intmax_t v_int;
    long double v_float;
    const char *v_string;
    size_t v_str_len;
};

/* APIs */

/* create, delete handle. parsing strings */
struct jpw_root *jpw_new(void);
struct jpw_gen_t *jpw_parse(struct jpw_root *, const char *);
int jpw_delete(struct jpw_root *);

/* get the type of any kind of json objects */
int jpw_typeof(struct jpw_root *, struct jpw_gen_t *);
/* translate any json objects to string */
char *jpw_tostring(struct jpw_root *, struct jpw_gen_t *);

/* json OBJECT fucntions */
struct jpw_gen_t *jpw_object_new(struct jpw_root *);
size_t jpw_object_get_len(struct jpw_root *, struct jpw_gen_t *);
int jpw_object_add(struct jpw_root *, struct jpw_gen_t *, const char *, struct jpw_gen_t *);
struct jpw_gen_t *jpw_object_get_item(struct jpw_root *, struct jpw_gen_t *, const char *);

/* json ARRAY fucntions */
struct jpw_gen_t *jpw_array_new(struct jpw_root *);
size_t jpw_array_get_len(struct jpw_root *, struct jpw_gen_t *);
int jpw_array_add(struct jpw_root *, struct jpw_gen_t *, struct jpw_gen_t *);
struct jpw_gen_t *jpw_array_get_item(struct jpw_root *, struct jpw_gen_t *, int);

/* json BOOLEAN fucntions */
struct jpw_gen_t *jpw_boolean_new(struct jpw_root *, boolean);
boolean jpw_boolean_get_val(struct jpw_root *, struct jpw_gen_t *);

/* json INTEGER fucntions */
struct jpw_gen_t *jpw_int_new(struct jpw_root *, intmax_t );
intmax_t jpw_int_get_val(struct jpw_root *, struct jpw_gen_t *);

/* json FLOAT fucntions */
struct jpw_gen_t *jpw_float_new(struct jpw_root *, long double );
long double jpw_float_get_val(struct jpw_root *, struct jpw_gen_t *);

/* json STRING fucntions */
struct jpw_gen_t *jpw_string_new(struct jpw_root *, const char *);
struct jpw_gen_t *jpw_string_len_new(struct jpw_root *, char *, size_t );
char *jpw_string_get_str(struct jpw_root *, struct jpw_gen_t *);
size_t jpw_string_get_len(struct jpw_root *, struct jpw_gen_t *);

/*******************************************/





#endif

