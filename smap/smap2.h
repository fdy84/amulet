#ifndef	_SMAP_H_
#define	_SMAP_H_
#include <stdint.h>


struct SMAP {
	int	seg_num;
	int seg_shift;
	int seg_mask;
	int load_factor;
	int mpool_enabled;	/* mempool switch */
	int mt;			/* Multi-thread lock protected */
	struct SEGMENT *seg;	
};

struct PAIR {
	int type; /* number or string */
	int key_len;
//	int data_len;
	union {
		uint64_t	ikey;
		char	*skey;
	};
	void *data;
};

typedef int (smap_callback)(struct SMAP *, struct PAIR *);

struct SMAP *smap_init(int, float, int, int, int);
int smap_insert(struct SMAP *, struct PAIR *, int);
int smap_delete(struct SMAP *, struct PAIR *, int);
void *smap_get(struct SMAP *, struct PAIR *);
void *smap_update(struct SMAP *, struct PAIR *);
int smap_traverse(struct SMAP *, smap_callback *, uint32_t);
uint64_t smap_get_elm_num(struct SMAP *);
struct PAIR *smap_get_first(struct SMAP *, struct PAIR *, char *, int);
struct PAIR *smap_get_next(struct SMAP *, struct PAIR *, char *, int);

#define HASHTYPE_NUM 0
#define HASHTYPE_STR 1

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

#define SMAP_MAX_KEY_LEN 1023



#define SMAP_IS_NUM(pair) ((pair)->type == HASHTYPE_NUM)
#define SMAP_IS_STR(pair) ((pair)->type == HASHTYPE_STR)

#define SMAP_SET_NUM_PAIR(pair, key, value) do {	\
	(pair)->type = HASHTYPE_NUM;	\
	(pair)->ikey = key;	\
	(pair)->data = value;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_SET_STR_PAIR(pair, key, klen, value) do {	\
	(pair)->type = HASHTYPE_STR;	\
	(pair)->skey = key;	\
	(pair)->key_len = klen;	\
	(pair)->data = value;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_SET_STR_KEY(pair, key, klen) do {	\
	(pair)->type = HASHTYPE_STR;	\
	(pair)->skey = key;	\
	(pair)->key_len = klen;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_SET_NUM_KEY(pair, key) do {	\
	(pair)->type = HASHTYPE_NUM;	\
	(pair)->ikey = key;	\
} while (/*CONSTCOND*/ 0)
#define SMAP_GET_NUM_KEY(pair) ((pair)->ikey)

#define SMAP_GET_KEY_LEN(pair) ((pair)->key_len)

#define SMAP_GET_STR_KEY(pair)	\
	(pair)->skey
//((pair)->key_len >= sizeof(char *) ? (pair)->skey: (char*)(&((pair)->skey)))

#define SMAP_GET_VALUE(pair) ((pair)->data)


#endif
