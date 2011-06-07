#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "smap2.h"
#include "tree.h"
#include "queue.h"

struct SMAP_ENT {
	union {
		SLIST_ENTRY(SMAP_ENT) mem_ent;
		RB_ENTRY(SMAP_ENT)    val_ent;
	};
	struct PAIR pair;
};

RB_HEAD(SMAP_TREE, SMAP_ENT);

struct BUCKET {
        uint64_t counter;
        struct SMAP_TREE root;
};
SLIST_HEAD(ENTRY_POOL_HEAD, SMAP_ENT);

struct SEGMENT {
		struct ENTRY_POOL_HEAD	*entry_pool;
		uint64_t	counter;
		int	bucket_num;
		struct BUCKET *bp;
		int entry_pool_size;
        pthread_rwlock_t seg_lock;
};

#define MAX_CAPACITY (1 << 30)
#define MAX_SEGMENTS (1 << 16) 

static inline int
nhash(int h) 
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

static inline int
shash(char *str, int len)
{
	unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
	unsigned int hash = 0;
	int i = 0;
 
	while (*str)
	{
		hash = hash * seed + (*str++);
		i++;
		if (i = len)
			break;
	}
 
	return (hash & 0x7FFFFFFF);
}

static inline int
ncmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	int64_t diff = a->pair.ikey - b->pair.ikey;

	if (diff == 0)
		return (0);
	else if (diff < 0)
		return (-1);
	else
		return (1);
}

static inline int
scmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	int c = a->pair.key_len - b->pair.key_len;
	if (c == 0) {
		if (a->pair.key_len >= sizeof(char *))
			c = strncmp(a->pair.skey, b->pair.skey, a->pair.key_len);
		else
			c = strncmp((char*)(&a->pair.skey), (char*)(&b->pair.skey), a->pair.key_len);
		return (c ? (c < 0 ? -1 : 1) : 0);
	} else 
		return (c < 0 ? -1 : 1);
}

/* It support both number and string */
static inline int
smap_cmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	if (a->pair.type == b->pair.type) {
		if (a->pair.type == HASHTYPE_NUM) {
			return ncmp(a, b);
		} else {
			return scmp(a, b);
		}
	} else {
		return (a->pair.type - b->pair.type);
	}
}

RB_PROTOTYPE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, smap_cmp);
RB_GENERATE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, smap_cmp);

static void
rdlock(pthread_rwlock_t *lock)
{
	int rc;
	rc = pthread_rwlock_rdlock(lock);
	if (rc != 0) {
		printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
		exit(1);
	}
}

static void
wrlock(pthread_rwlock_t *lock)
{
	int rc;
//	printf("lock:%p\n",lock);
	rc = pthread_rwlock_wrlock(lock);
	if (rc != 0) {
		printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
		exit(1);
	}
}

static void
unlock(pthread_rwlock_t *lock)
{
	int rc;
//	printf("unlock:%p\n",lock);
	rc = pthread_rwlock_unlock(lock);
	if (rc != 0) {
		printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
		exit(1);
	}
}


/*
 * This is a simple memory pool, itself is not thread safed,
 * but each memory pool all with "segment" one-to-one, 
 * lock segment will lock the thread pool, thus can be safe too.
 * Memory pool is a simple list structure, 
 * through the entry of fixed length to achieve rapid memory allocation.
 */
static struct SMAP_ENT *
entry_alloc(struct smap *mp, struct SEGMENT *sp)
{
	struct SMAP_ENT *entry;
	if (!SLIST_EMPTY(sp->entry_pool) && mp->entry_pool) {
		entry = SLIST_FIRST(sp->entry_pool);
		SLIST_REMOVE_HEAD(sp->entry_pool, mem_ent);
		sp->entry_pool_size--;
	} else {
		entry = (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
	}
	return entry;
}

static void
entry_free(struct smap *mp, struct SEGMENT *sp, struct SMAP_ENT *ptr)
{
	if (sp->entry_pool_size < (sp->bucket_num*2) && mp->entry_pool) {
		SLIST_INSERT_HEAD(sp->entry_pool, ptr, mem_ent);
		sp->entry_pool_size++;
	} else {
		free(ptr);
	}
}

/*
 *
 * level: The concurrency level
 */
struct smap *
smap_init(int capacity , int level, int entry_pool_size)
{
	struct BUCKET *bp;
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

	if (capacity > MAX_CAPACITY)
		capacity = MAX_CAPACITY;
	c = capacity / ssize;
	if (c * ssize < capacity)
		++c;
	cap = 1;
	while (cap < c)
		cap <<= 1;

	mp->seg = (struct SEGMENT *)malloc(sizeof(struct SEGMENT)*ssize);
	mp->seg_num = ssize;

	if (entry_pool_size > 0) {
		mp->entry_pool = 1;
	} else if (entry_pool_size == 0) {
		mp->entry_pool = 0;
	} else if (entry_pool_size != DEFAULT_ENTRY_POOL_SIZE) {
		return (NULL);
	}

	for (i = 0; i < ssize; i++) {
		rc = pthread_rwlockattr_init(&attr);
		rc = pthread_rwlock_init(&(mp->seg[i].seg_lock), &attr);
		if (rc != 0)
			return (NULL);
		bp = (struct BUCKET *)malloc(sizeof(struct BUCKET) * cap);
		if (bp == NULL)
		        return (NULL);

		for (j = 0; j < cap; j++) {
			bp[j].counter = 0;
			RB_INIT(&(bp[j].root));
		}
		mp->seg[i].bucket_num = cap;	
		mp->seg[i].bp = bp;

		/* initialize the memory pool */
		if (entry_pool_size != 0)
		{
			mp->seg[i].entry_pool = (struct ENTRY_POOL_HEAD *)malloc(sizeof(struct ENTRY_POOL_HEAD));
			SLIST_INIT(mp->seg[i].entry_pool);
			if (entry_pool_size == DEFAULT_ENTRY_POOL_SIZE)
				entry_pool_size = cap/2;
			for (j = 0; j < entry_pool_size; j++) {
				struct SMAP_ENT *new_mem_ent = (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
				SLIST_INSERT_HEAD(mp->seg[i].entry_pool, new_mem_ent, mem_ent);
			}
			mp->seg[i].entry_pool_size = entry_pool_size;
		}
	}
	return (mp);
}

/*
 * clear a map, it would not always lock, just realloc the bucket and mpool.
 */
void
smap_clear(struct smap *mp, int start)
{
	int i, j;
	struct SEGMENT *sp;
	struct BUCKET *old_bp;
	struct BUCKET *new_bp;
	struct BUCKET *bp;
	struct SMAP_ENT *np, *tnp;
	int entry_pool_size = 0;
	struct ENTRY_POOL_HEAD	*entry_pool, *tentry_pool;
	
	entry_pool = (struct ENTRY_POOL_HEAD *)malloc(sizeof(struct ENTRY_POOL_HEAD));

	for (i = 0; i < mp->seg_num; i++) {
		SLIST_INIT(entry_pool);
		sp = &(mp->seg[(i + start) % (mp->seg_num)]);

		new_bp = (struct BUCKET *)malloc(sp->bucket_num * sizeof(struct BUCKET));
		for (j = 0; j < sp->bucket_num; j++) {
			new_bp[j].counter = 0;
			RB_INIT(&(new_bp[j].root));
		}

		/* alloc a new bucket array and delete the old one. */
		wrlock(&(sp->seg_lock));
		old_bp = sp->bp;
		sp->bp = new_bp;
		unlock(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(old_bp[j]);
			for (np = RB_MIN(SMAP_TREE, &(bp->root));
				np && ((tnp) = RB_NEXT(SMAP_TREE, &(bp->root), np), 1);
				np = tnp) {
				RB_REMOVE(SMAP_TREE, &(bp->root), np);
				if(np->pair.type == HASHTYPE_STR && 
					np->pair.key_len >= sizeof(char *))
					free(np->pair.skey);
				SLIST_INSERT_HEAD(entry_pool, np, mem_ent);
				
				if (entry_pool_size < sp->bucket_num)
					entry_pool_size++;
				else
					free(np);
			}
		}
		wrlock(&(sp->seg_lock));
		free(old_bp);
		
		/* Process the entry_pool. */
		if (sp->entry_pool_size >= entry_pool_size) {
			unlock(&(sp->seg_lock));
			SLIST_FOREACH_SAFE(np, entry_pool, mem_ent, tnp) {
				free(np);
			}
		} else {
			tentry_pool = sp->entry_pool;
			sp->entry_pool = entry_pool;
			sp->entry_pool_size = entry_pool_size;
			unlock(&(sp->seg_lock));
			SLIST_FOREACH_SAFE(np, tentry_pool, mem_ent, tnp) {
				free(np);
			}
			free(tentry_pool);
			entry_pool = tentry_pool;
		}
		entry_pool_size = 0;
	}
}


static inline struct SEGMENT *
get_segment(struct smap *mp, int hash)
{
	int seg_hash;
	
	/* right shift 32bit problem */
	uint64_t shit = hash & 0x7FFFFFFF;

	seg_hash = (shit >> mp->seg_shift) & (mp->seg_shift);
	return (mp->seg + seg_hash);
}

static inline struct BUCKET *
get_bucket(struct SEGMENT *sp, int hash)
{
	int index = hash & (sp->bucket_num - 1);
	return &(sp->bp[index]);
}


int
smap_insert(struct smap *mp, struct PAIR *pair, int lock)
{
	struct BUCKET *bp;
	struct SMAP_ENT *entry;
	struct SMAP_ENT *rc;
	int h;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (SMAP_GENERAL_ERROR);
	
	if (pair->type == HASHTYPE_NUM) {
		h = nhash((int)pair->ikey);
	} else if (pair->type == HASHTYPE_STR) {
		if (pair->key_len == 0 || pair->skey == NULL)
			return (SMAP_GENERAL_ERROR);
		h = shash((char *)pair->skey, pair->key_len);
	} else {
		return (SMAP_GENERAL_ERROR);
	}

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	if (lock)
		wrlock(&(sp->seg_lock));
	/*
	 * Because we have locked the segment,
	 * so although mpool itself is not thread safe,
	 * its in the segment, also can assure safety.
	 */
	entry = entry_alloc(mp, sp);
	if (entry == NULL) {
		if (lock)
			unlock(&(sp->seg_lock));
		return (SMAP_OOM);
	}

	if (pair->type == HASHTYPE_NUM) {
		entry->pair.ikey = pair->ikey;
		entry->pair.data = pair->data;
	} else if (pair->type == HASHTYPE_STR) {
		if (pair->key_len >= sizeof(char *)) {
			entry->pair.skey = (char *)malloc(pair->key_len + 1);
			memcpy(entry->pair.skey, pair->skey, pair->key_len);
		} else {
			memcpy((char *)(&(entry->pair.skey)), pair->skey, pair->key_len);
		}
		entry->pair.key_len = pair->key_len;
	}

	entry->pair.type = pair->type;
	entry->pair.data = pair->data;

	sp->counter++;
	rc = RB_INSERT(SMAP_TREE, &(bp->root), entry);
	if (lock)
		unlock(&(sp->seg_lock));

	if (rc == NULL)
		return (SMAP_OK);
	else
		return (SMAP_DUPLICATE_KEY);
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_delete(struct smap *mp, struct PAIR *pair, int lock)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *res;
	int h;
	struct SEGMENT *sp;

	if (pair->type == HASHTYPE_NUM) {
		h = nhash((int)pair->ikey);
		entry.pair.ikey = pair->ikey;
	} else if (pair->type == HASHTYPE_STR) {
		h = shash((char *)pair->skey, pair->key_len);
		if (pair->key_len >= sizeof(char *))
			entry.pair.skey = pair->skey;
		else
			memcpy((char *)(&(entry.pair.skey)), pair->skey, pair->key_len);

		entry.pair.key_len = pair->key_len;
	} else {
		return (SMAP_GENERAL_ERROR);
	}
	entry.pair.type = pair->type;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	if (lock)
		wrlock(&(sp->seg_lock));
	
	res = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	if (res == NULL) {
		if (lock)
			unlock(&(sp->seg_lock));
		return (SMAP_NONEXISTENT_KEY);
	}
	RB_REMOVE(SMAP_TREE, &(bp->root), res);

	bp->counter--;
	
	if (res->pair.key_len >= sizeof(char *))
		free(res->pair.skey);

	entry_free(mp, sp, res);
	if (lock)
		unlock(&(sp->seg_lock));
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
smap_get(struct smap *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	struct SMAP_ENT *np;
	int h;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);

	if (pair->type == HASHTYPE_NUM) {
		h = nhash((int)pair->ikey);
		entry.pair.ikey = pair->ikey;
	} else if (pair->type == HASHTYPE_STR) {
		h = shash((char *)pair->skey, pair->key_len);
		if (pair->key_len >= sizeof(char*))
			entry.pair.skey = pair->skey;
		else
			memcpy((char *)(&(entry.pair.skey)), pair->skey, pair->key_len);
		entry.pair.key_len = pair->key_len;
	} else {
		return (NULL);
	}
	entry.pair.type = pair->type;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	
	rdlock(&(sp->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	unlock(&(sp->seg_lock));
	if (rc == NULL) {
		return (NULL);
	} else {
		pair->data = rc->pair.data;
		return (rc->pair.data);
	}
}

/*
 * it won't free the value, do it yourself.
 */

void *
smap_update(struct smap *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	int h;
	void *old_data;
	struct SEGMENT *sp;

	if (mp == NULL)
		return (NULL);

	if (pair->type == HASHTYPE_NUM) {
		h = nhash((int)pair->ikey);
		entry.pair.ikey = pair->ikey;
	} else if (pair->type == HASHTYPE_STR) {
		h = shash((char *)pair->skey, pair->key_len);
		if (pair->key_len >= sizeof(char*))
			entry.pair.skey = pair->skey;
		else
			memcpy((char *)(&(entry.pair.skey)), pair->skey, pair->key_len);

		entry.pair.key_len = pair->key_len;
	} else {
		return (NULL);
	}
	entry.pair.type = pair->type;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	rdlock(&(sp->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	
	/* if no entry */
	if (rc == NULL) {
		unlock(&(sp->seg_lock));
		return (NULL);
	} else {
		old_data = rc->pair.data;
		rc->pair.data = pair->data;
		unlock(&(sp->seg_lock));
		return (old_data);
	}
}

/*
 * it won't free the value, do it yourself.
 */
int
smap_traverse(struct smap *mp, smap_callback *routine, uint32_t start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	int i, j;
	int rc;
	struct SEGMENT *sp;
	
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % (mp->seg_num)]);
		bp = sp->bp;
		wrlock(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);

			for (np = RB_MIN(SMAP_TREE, &(bp->root));
				np && ((tnp) = RB_NEXT(SMAP_TREE, &(bp->root), np), 1);
				np = tnp) {
				rc = routine(mp, (struct PAIR *)(&(np->pair)));
				if (rc == SMAP_BREAK) {
					unlock(&(sp->seg_lock));
					goto out;
				}
			}
		}
		unlock(&(sp->seg_lock));
	}
	out:
	return 0;
}

/*
int
main(void)
{
	struct smap* map;
	int i;
	int rc;

	map = smap_init(1024, 10, 0, 0);

	if (map == NULL)
		printf("error map NULL \n");

	for (i = 0; i < 10240; i++) {
		rc = smap_insert(map, i, "haha", 1);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
	
	smap_clear(map, 0);
	
	for (i = 0; i < 10240; i++) {
		rc = smap_insert(map, i, "hoho", 1);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
	smap_traverse(map, test1, 0);
	smap_traverse(map, test, 0);
	return (0);
}
*/

int
main(void)
{
	printf("%lu, %lu\n", sizeof(struct PAIR), sizeof(struct SMAP_ENT));
	return 0;
}