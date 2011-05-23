#ifndef	_SMAP_H_
#define	_SMAP_H_

struct smap {
	uint32_t	slot_num;
	uint32_t	seg_num;
	int segment_shift;
	int segment_mask;
	int cap;
	struct segment *seg;	
};

struct smap *smap_init(int, int, int);
int smap_insert(struct smap *, uint64_t, void *);
int smap_delete(struct smap *, uint64_t);
void *smap_get(struct smap *, uint64_t);
int smap_update(struct smap *, uint64_t, void *);
int smap_traverse(struct smap *, void (*routine)(uint64_t, void *),uint32_t);
void smap_key_lock(struct smap *, uint64_t);
int smap_key_unlock(struct smap *, uint64_t);
uint64_t smap_get_elm_num(struct smap *);

#define SMAP_GENERAL_ERROR -1
#define SMAP_OOM -2
#define SMAP_OK 0
#define SMAP_DUPLICATE_KEY -3
#define SMAP_NONEXISTENT_KEY -4
#endif
