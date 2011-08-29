#ifndef	_SMAP_H_
#define	_SMAP_H_
#include <stdint.h>


struct SMAP {
	unsigned int	seg_num;
	unsigned int	seg_shift;
	unsigned int	seg_mask;
	float	load_factor;
	unsigned int	mpool_enabled;	/* mempool switch */
	unsigned int	mt;			/* Multi-thread lock protected */
	unsigned int	conflict_type;
	struct SEGMENT *seg;	
};

struct PAIR {
	uint16_t type; /* number or string, if the type is int, more fast*/
	uint16_t key_len; /* strlen, number key_len == 0, if the type is int, more fast*/
	uint32_t data_len;
	union {
		uint64_t	ikey;
		char	*skey;
	};
	void *data;
};

typedef int (smap_callback)(struct SMAP *, struct PAIR *);

struct SMAP *smap_init(int, float, int, int, int);
int smap_deinit(struct SMAP *);
int smap_set(struct SMAP *, struct PAIR *, int, int);
int smap_put(struct SMAP *, struct PAIR *, int);
int smap_delete(struct SMAP *, struct PAIR *);
void smap_clear(struct SMAP *, int);
void *smap_get(struct SMAP *, struct PAIR *);
int smap_update(struct SMAP *, struct PAIR *);
int smap_traverse_unsafe
	(struct SMAP *, smap_callback *, unsigned long, unsigned long);
int smap_get_elm_num(struct SMAP *);
struct PAIR *smap_get_first
	(struct SMAP *, struct PAIR *, char *, int, unsigned long, unsigned long);
struct PAIR *smap_get_next
	(struct SMAP *, struct PAIR *, char *, int, unsigned long, unsigned long);

uint64_t smap_get_segment_counter(struct SMAP *);
uint64_t smap_get_bucket_counter(struct SMAP *);

#define KEYTYPE_NUM (0x1<<0)
#define KEYTYPE_STR (0x1<<1)
#define KEYTYPE_ALL (KEYTYPE_NUM|KEYTYPE_STR)

#define SMAP_RBTREE (0x1<<0)
#define SMAP_SLIST (0x1<<1)
#define DEFAULT_CONFLICT_TYPE SMAP_RBTREE

#define DEFAULT_ENTRY_POOL_SIZE -1
#define DEFAULT_INITIAL_CAPACITY 16
#define DEFAULT_LOAD_FACTOR 0.75f
#define DEFAULT_CONCURRENCY_LEVEL 16

#define SMAP_BREAK 1
#define SMAP_GENERAL_ERROR -1
#define SMAP_OOM -2
#define SMAP_OK 0
#define SMAP_DUPLICATE_KEY -3
#define SMAP_NONEXISTENT_KEY -4
#define SMAP_BUFFER_TOO_SHORT -5

#define SMAP_MAX_KEY_LEN 1023



#define SMAP_IS_NUM(pair) ((pair)->type == KEYTYPE_NUM)
#define SMAP_IS_STR(pair) ((pair)->type == KEYTYPE_STR)

#define SMAP_SET_NUM_PAIR(pair, key, value, value_len) do {	\
	(pair)->type = KEYTYPE_NUM;	\
	(pair)->ikey = key;	\
	(pair)->key_len = 0;	\
	(pair)->data_len = value_len;	\
	(pair)->data = value;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_SET_STR_PAIR(pair, key, klen, value, value_len) do {	\
	(pair)->type = KEYTYPE_STR;	\
	(pair)->skey = key;	\
	(pair)->key_len = klen;	\
	(pair)->data_len = value_len;	\
	(pair)->data = value;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_SET_NUM_KEY(pair, key) do {	\
	(pair)->type = KEYTYPE_NUM;	\
	(pair)->ikey = key;	\
	(pair)->key_len = 0;	\
} while (/*CONSTCOND*/ 0)


#define SMAP_SET_STR_KEY(pair, key, klen) do {	\
	(pair)->type = KEYTYPE_STR;	\
	(pair)->skey = key;	\
	(pair)->key_len = klen;	\
} while (/*CONSTCOND*/ 0)


#define SMAP_GET_NUM_KEY(pair) ((pair)->ikey)

#define SMAP_GET_KEY_LEN(pair) ((pair)->key_len)

#define SMAP_GET_STR_KEY(pair)	\
	(pair)->skey
//((pair)->key_len >= sizeof(char *) ? (pair)->skey: (char*)(&((pair)->skey)))

#define SMAP_GET_VALUE(pair) ((pair)->data)
#define SMAP_GET_VALUE_LEN(pair) ((pair)->data_len)
#define SMAP_SET_VALUE(pair, value, value_len)	\
{	\
	(pair)->data = value;	\
	(pair)->data_len = value_len;	\
} while (/*CONSTCOND*/ 0)

#endif
