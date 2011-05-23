#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "smap2.h"
#include "tree.h"

struct SMAP_ENT{
        RB_ENTRY(SMAP_ENT)    val_ent;
        uint64_t		key;
        void *data;
};

static int hash(int h) 
{
        // Spread bits to regularize both segment and index locations,
        // using variant of single-word Wang/Jenkins hash.
        h += (h <<  15) ^ 0xffffcd7d;
        h ^= (((unsigned int)h) >> 10);
        h += (h <<   3);
        h ^= (((unsigned int)h) >>  6);
        h += (h <<   2) + (h << 14);
        return h ^ (((unsigned int)h) >> 16);
}

static int
number_cmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	int64_t diff = a->key - b->key;

	if (diff == 0)
		return (0);
	else if (diff < 0)
		return (-1);
	else
		return (1);
}

typedef int CMP(struct SMAP_ENT *, struct SMAP_ENT *) ;
CMP *cmp;

RB_HEAD(SMAP_TREE, SMAP_ENT);
RB_PROTOTYPE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, cmp);
RB_GENERATE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, cmp);

struct slot {
        uint64_t counter;
        struct SMAP_TREE root;
};


struct segment {
		uint64_t	counter;
		int	slot_num;
		struct slot *sp;
        pthread_rwlock_t seg_lock;
};


#define MAXIMUM_CAPACITY (1 << 30)
#define MAX_SEGMENTS (1 << 16) 

/*
 * hash_type: number or string
 * level: The concurrency level
 */
struct smap *
smap_init(int capacity , int level, int hash_type)
{
	struct slot *sp;
	struct smap *mp;
	int i;
	int rc;
	int sshift = 0;
	int ssize = 1;
	int cap, c;

	mp = malloc(sizeof(struct smap));

	while ( ssize < level ) {
		++sshift;
		ssize <<= 1;
	}

	mp->segment_shift = 32 - sshift;
	mp->segment_mask = ssize - 1;

	if (capacity > MAXIMUM_CAPACITY)
		capacity = MAXIMUM_CAPACITY;
	c = capacity / ssize;
	if (c * ssize < capacity)
		++c;
	while (cap < c)
		cap <<= 1;

	mp->seg = malloc(sizeof(struct segment)*ssize);
	
	mp->seg_num = ssize;

	for (i = 0; i < ssize; i++) {
		rc = pthread_rwlock_init(&(mp->seg[i].seg_lock), NULL);
		if (rc != 0)
			return (NULL);
		sp = malloc(sizeof(struct slot) * cap);
		if (sp == NULL)
		        return (NULL);
		
		for (i = 0; i < cap; i++) {
			sp[i].counter = 0;
			RB_INIT(&(sp[i].root));
		}
		mp->seg[i].slot_num = cap;		
	}


	
	return mp;
}

static struct segment *
get_segment(struct smap *mp, int hash)
{
	int seg_hash;
	seg_hash = (((unsigned int)hash) >> mp->segment_shift) & mp->segment_shift;
	return mp->seg + seg_hash;
}

static struct slot *
get_slot(struct segment *seg, int hash)
{
	return &(seg->sp[hash & (seg->slot_num - 1)]);
}


int
smap_insert(struct smap *mp, uint64_t key, void *value)
{
	struct slot *sp;
	struct SMAP_ENT *entry;
	struct SMAP_ENT *rc;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);

	h = hash((int)key);
	seg = get_segment(mp, h);

	sp = get_slot(seg, h);

	entry = malloc(sizeof(struct SMAP_ENT));
	if (entry == NULL)
		return (SMAP_OOM);
	
	entry->key = key;
	entry->data = value;
	
	pthread_rwlock_wrlock(&(seg->seg_lock));
	
	seg->counter++;
	rc = RB_INSERT(SMAP_TREE, &(sp->root), entry);

	pthread_rwlock_unlock(&(seg->seg_lock));


	if (rc == NULL)
		return (SMAP_OK);
	else
		return (SMAP_DUPLICATE_KEY);
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_delete(struct smap *mp, uint64_t key)
{
	struct slot *sp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);

	h = hash((int)key);
	seg = get_segment(mp, h);

	sp = get_slot(seg, h);
	
	entry.key = key;
	
	pthread_rwlock_wrlock(&(seg->seg_lock));
	
	rc = RB_FIND(SMAP_TREE, &(sp->root), &entry);
	if (rc == NULL) {
		pthread_rwlock_unlock(&(seg->seg_lock));
		return (SMAP_NONEXISTENT_KEY);
	}
	RB_REMOVE(SMAP_TREE, &(sp->root), rc);

	sp->counter--;
	pthread_rwlock_unlock(&(seg->seg_lock));
	
	free(rc);

	return (SMAP_OK);
}

uint64_t
smap_get_elm_num(struct smap *mp)
{
	uint64_t c = 0;
	int i;
		
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	
	for (i = 0; i < mp->seg_num; i++) {
		c += mp->seg[i].counter;
	}
	return c;
}

/*
 * it won't free the value, do it yourself.
 */

void *
smap_get(struct smap *mp, uint64_t key)
{
	struct slot *sp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	struct SMAP_ENT *np;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (NULL);

	h = hash((int)key);
	
	seg = get_segment(mp, h);
	sp = get_slot(seg, h);
	
	entry.key = key;
	
	pthread_rwlock_rdlock(&(seg->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(sp->root), &entry);
	pthread_rwlock_unlock(&(seg->seg_lock));
	if (rc == NULL) {
		return (NULL);
	} else {
		return (rc->data);
	}
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_update(struct smap *mp, uint64_t key, void *data)
{
	struct slot *sp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);

	h = hash((int)key) % (mp->slot_num);
	seg = get_segment(mp, h);
	sp = get_slot(seg, h);
	
	entry.key = key;
	
	pthread_rwlock_rdlock(&(seg->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(sp->root), &entry);
	
	/* if no entry */
	if (rc == NULL) {
		pthread_rwlock_unlock(&(seg->seg_lock));
		return (SMAP_NONEXISTENT_KEY);
	} else {
		rc->data = data;
		pthread_rwlock_unlock(&(seg->seg_lock));
		return (SMAP_OK);
	}
	
}

/*
 * it won't free the value, do it yourself.
 */
/*
int
smap_traverse(struct smap *mp, void (*routine)(uint64_t, void *),uint32_t start)
{
	struct slot *sp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	int i;
	struct segment *seg;
	
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->slot_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->slot_num; i++) {
		sp = &(mp->sp[(i + start) % (mp->slot_num)]);
		
			pthread_rwlock_wrlock(&(sp->slot_lock));
		while (!RB_EMPTY(&(sp->root))) {
			for (np = RB_MIN(SMAP_TREE, &(sp->root));
				np && ((tnp) = RB_NEXT(SMAP_TREE, &(sp->root), np), 1); np = tnp) {
				routine(np->key, np->data);
			}
		}
		
			pthread_rwlock_unlock(&(sp->slot_lock));
	}
}
*/

int
main(void)
{
	struct smap* map;
	int i;
	int rc;

	map = smap_init(1024, 1, 0);

	if (map == NULL)
		printf("error map NULL \n");

	for (i = 0; i < 10240; i++) {
		rc = smap_insert(map, i, NULL);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
	return (0);
}
