#ifndef	_SMAP_H_
#define	_SMAP_H_
#include <stdint.h>


struct smap {
	int hash_type;
	uint32_t	bucket_num;
	uint32_t	seg_num;
	int seg_shift;
	int seg_mask;
	int cap;
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

struct smap *smap_init(int, int, int, int);
int smap_insert(struct smap *, struct PAIR *, int);
int smap_delete(struct smap *, uint64_t, int);
void *smap_get(struct smap *, uint64_t);
int smap_update(struct smap *, uint64_t, void *);
int smap_traverse(struct smap *, int (*routine)(struct smap *, struct PAIR *),uint32_t);
void smap_key_lock(struct smap *, uint64_t);
int smap_key_unlock(struct smap *, uint64_t);
uint64_t smap_get_elm_num(struct smap *);

#define HASHTYPE_NUM 0
#define HASHTYPE_STR 1

#define SMAP_BREAK 1
#define SMAP_GENERAL_ERROR -1
#define SMAP_OOM -2
#define SMAP_OK 0
#define SMAP_DUPLICATE_KEY -3
#define SMAP_NONEXISTENT_KEY -4
#endif
