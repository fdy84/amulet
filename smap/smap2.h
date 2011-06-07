#ifndef	_SMAP_H_
#define	_SMAP_H_
#include <stdint.h>


struct smap {
	int	bucket_num;
	int	seg_num;
	int seg_shift;
	int seg_mask;
	int cap;
	int entry_pool;
	struct SEGMENT *seg;	
};

struct PAIR {
	int type; /* number or string */
	int key_len;
	union {
		uint64_t	ikey;
		char	*skey;
	};
	void *data;
};

typedef int (smap_callback)(struct smap *, struct PAIR *);

struct smap *smap_init(int, int, int);
int smap_insert(struct smap *, struct PAIR *, int);
int smap_delete(struct smap *, struct PAIR *, int);
void *smap_get(struct smap *, struct PAIR *);
void *smap_update(struct smap *, struct PAIR *);
int smap_traverse(struct smap *, smap_callback *, uint32_t);
uint64_t smap_get_elm_num(struct smap *);

#define HASHTYPE_NUM 0
#define HASHTYPE_STR 1

#define DEFAULT_ENTRY_POOL_SIZE -1

#define SMAP_BREAK 1
#define SMAP_GENERAL_ERROR -1
#define SMAP_OOM -2
#define SMAP_OK 0
#define SMAP_DUPLICATE_KEY -3
#define SMAP_NONEXISTENT_KEY -4


#define SMAP_IS_NUM (pair) (pair->type == HASHTYPE_NUM)
#define SMAP_IS_STR (pair) (pair->type == HASHTYPE_STR)

#define SMAP_SET_NUM_PAIR(pair, key, value) do {	\
	pair->type = HASHTYPE_NUM;	\
	pair->nkey = key;	\
	pair->data = value;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_SET_STR_PAIR(pair, key, key_len, value) do {	\
	pair->type = HASHTYPE_STR;	\
	pair->skey = key;	\
	pair->key_len = key_len;	\
	pair->data = value;	\
} while (/*CONSTCOND*/ 0)

#define SMAP_GET_NUM_KEY(pair) (pair->nkey)

#define SMAP_GET_KEY_LEN(pair) (pair->key_len)

#define SMAP_GET_STR_KEY(pair)	\
(pair->key_len >= sizeof(char *) ? pair->skey: (char*)(&pair->skey))

#endif
