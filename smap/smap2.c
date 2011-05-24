#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "smap2.h"
#include "tree.h"
#include "queue.h"

struct SMAP_ENT{
	SLIST_ENTRY(SMAP_ENT) mem_ent;
	RB_ENTRY(SMAP_ENT)    val_ent;
	uint64_t		key;
	void *data;
};

RB_HEAD(SMAP_TREE, SMAP_ENT);
struct bucket {
        uint64_t counter;
        struct SMAP_TREE root;
};

struct segment {
		uint64_t	counter;
		int	bucket_num;
		struct bucket *bp;
		SLIST_HEAD(, SMAP_ENT)	mpool;
		int pool_size;
        pthread_rwlock_t seg_lock;
};

#define MAXIMUM_CAPACITY (1 << 30)
#define MAX_SEGMENTS (1 << 16) 

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
CMP *cmp = number_cmp;

RB_PROTOTYPE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, cmp);
RB_GENERATE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, cmp);


void
rdlock(pthread_rwlock_t *lock)
{
	int rc;
	rc = pthread_rwlock_rdlock(lock);
	if (rc != 0) {
		printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
		exit(1);
	}
}

void
wrlock(pthread_rwlock_t *lock)
{
	int rc;
	printf("lock:%p\n",lock);
	rc = pthread_rwlock_wrlock(lock);
	if (rc != 0) {
		printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
		exit(1);
	}
}

void
unlock(pthread_rwlock_t *lock)
{
	int rc;
	printf("unlock:%p\n",lock);
	rc = pthread_rwlock_unlock(lock);
	if (rc != 0) {
		printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
		exit(1);
	}
}

/*
 * hash_type: number or string
 * level: The concurrency level
 */
struct smap *
smap_init(int capacity , int level, int pool_size, int hash_type)
{
	struct bucket *bp;
	struct smap *mp;
	int i, j;
	int rc;
	int sshift = 0;
	int ssize = 1;
	int cap, c;
	pthread_rwlockattr_t attr;

	mp = (struct smap *)malloc(sizeof(struct smap));

	while ( ssize < level ) {
		++sshift;
		ssize <<= 1;
	}

	mp->seg_shift = 32 - sshift;
	mp->seg_mask = ssize - 1;

	if (capacity > MAXIMUM_CAPACITY)
		capacity = MAXIMUM_CAPACITY;
	c = capacity / ssize;
	if (c * ssize < capacity)
		++c;
	cap = 1;
	while (cap < c)
		cap <<= 1;

	mp->seg = (struct segment *)malloc(sizeof(struct segment)*ssize);
	
	mp->seg_num = ssize;

	for (i = 0; i < ssize; i++) {
		rc = pthread_rwlockattr_init(&attr);
		rc = pthread_rwlock_init(&(mp->seg[i].seg_lock), &attr);
		if (rc != 0)
			return (NULL);
		bp = (struct bucket *)malloc(sizeof(struct bucket) * cap);
		if (bp == NULL)
		        return (NULL);
		
		for (j = 0; j < cap; j++) {
			bp[j].counter = 0;
			RB_INIT(&(bp[j].root));
		}
		mp->seg[i].bucket_num = cap;	
		mp->seg[i].bp = bp;
		SLIST_INIT(&(mp->seg[i].mpool));
		if (pool_size == 0)
			pool_size = cap/2;
		for (j = 0; j < pool_size; j++) {
			struct SMAP_ENT *new_mem_ent = (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
			SLIST_INSERT_HEAD(&(mp->seg[i].mpool), new_mem_ent, mem_ent);
		}
		mp->seg[i].pool_size = pool_size;
	}


	
	return mp;
}

static struct segment *
get_segment(struct smap *mp, int hash)
{
	int seg_hash;
	
	/* right shift 32bit problem */
	uint64_t shit = hash & 0x7FFFFFFF;

	seg_hash = (shit >> mp->seg_shift) & (mp->seg_shift);
	return mp->seg + seg_hash;
}

static struct bucket *
get_bucket(struct segment *seg, int hash)
{
	int index = hash & (seg->bucket_num - 1);
	return &(seg->bp[index]);
}


int
smap_insert(struct smap *mp, uint64_t key, void *value, int lock)
{
	struct bucket *bp;
	struct SMAP_ENT *entry;
	struct SMAP_ENT *rc;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);

	h = hash((int)key);
	seg = get_segment(mp, h);
	bp = get_bucket(seg, h);
	
	if (lock)
		wrlock(&(seg->seg_lock));
	
	if (!SLIST_EMPTY(&(seg->mpool))) {
		entry = SLIST_FIRST(&(seg->mpool));
		SLIST_REMOVE_HEAD(&(seg->mpool), mem_ent);
		seg->pool_size--;
	} else {
		entry = (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
	}
	if (entry == NULL) {
		if (lock)
			unlock(&(seg->seg_lock));
		return (SMAP_OOM);
	}
	
	entry->key = key;
	entry->data = value;
	
	seg->counter++;
	rc = RB_INSERT(SMAP_TREE, &(bp->root), entry);
	if (lock)
		unlock(&(seg->seg_lock));

	if (rc == NULL)
		return (SMAP_OK);
	else
		return (SMAP_DUPLICATE_KEY);
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_delete(struct smap *mp, uint64_t key, int lock)
{
	struct bucket *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *res;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);

	h = hash((int)key);
	seg = get_segment(mp, h);

	bp = get_bucket(seg, h);
	
	entry.key = key;
	
	if (lock)
		wrlock(&(seg->seg_lock));
	
	res = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	if (res == NULL) {
		if (lock)
			unlock(&(seg->seg_lock));
		return (SMAP_NONEXISTENT_KEY);
	}
	RB_REMOVE(SMAP_TREE, &(bp->root), res);

	bp->counter--;

	if (seg->pool_size < seg->bucket_num ) {
		SLIST_INSERT_HEAD(&(seg->mpool),
			res, mem_ent);
		seg->pool_size++;
	} else {
		free(res);
	}
	if (lock)
		unlock(&(seg->seg_lock));
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
	struct bucket *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	struct SMAP_ENT *np;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (NULL);

	h = hash((int)key);
	
	seg = get_segment(mp, h);
	bp = get_bucket(seg, h);
	
	entry.key = key;
	
	rdlock(&(seg->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	unlock(&(seg->seg_lock));
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
	struct bucket *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	int h;
	struct segment *seg;

	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);

	h = hash((int)key) % (mp->bucket_num);
	seg = get_segment(mp, h);
	bp = get_bucket(seg, h);
	
	entry.key = key;
	
	rdlock(&(seg->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	
	/* if no entry */
	if (rc == NULL) {
		unlock(&(seg->seg_lock));
		return (SMAP_NONEXISTENT_KEY);
	} else {
		rc->data = data;
		unlock(&(seg->seg_lock));
		return (SMAP_OK);
	}
	
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_traverse(struct smap *mp, void (*routine)(struct smap *, uint64_t, void *),uint32_t start)
{
	struct bucket *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	int i, j;
	struct segment *seg;
	
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		seg = &(mp->seg[(i + start) % (mp->seg_num)]);
		bp = seg->bp;
		wrlock(&(seg->seg_lock));
		for (j = 0; j < seg->bucket_num; j++) {
			while (!RB_EMPTY(&(bp[j].root))) {
				for (np = RB_MIN(SMAP_TREE, &(bp[j].root));
					np && ((tnp) = RB_NEXT(SMAP_TREE, &(bp[j].root), np), 1);
					np = tnp) {
					routine(mp, np->key, np->data);
				}
			}
		}
		unlock(&(seg->seg_lock));
	}
}

void
test(struct smap *mp, uint64_t key, void *value)
{
	if (SMAP_OK == smap_delete(mp, key, 0))
		printf("key = %ld, value = %s\n", key, (char *)value);
	else
		printf("no key!\n");
}

int
main(void)
{
	struct smap* map;
	int i;
	int rc;

	map = smap_init(1024, 10, 0, 0);

	if (map == NULL)
		printf("error map NULL \n");

	for (i = 0; i < 10260; i++) {
		rc = smap_insert(map, i, "haha", 1);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
	for (i = 0; i < 10; i++) {
		rc = smap_insert(map, i, NULL, 1);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
	smap_traverse(map, test, 0);
//	smap_traverse(map, test, 0);
	return (0);
}
