#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "smap.h"

#ifdef SMAP_RWLOCK
#include "rwlock.h"
#else
#include <pthread.h>
typedef pthread_rwlock_t rwlock_t;
#endif

#include "tree.h"
#include "queue.h"

/*
 * need: resize, mempool, ref_count?, STORE_VALUE/STORE_POINTER, lua
 */

struct SMAP_ENT {
	union {
		SLIST_ENTRY(SMAP_ENT) mem_ent;
		RB_ENTRY(SMAP_ENT)    val_ent;
	};
	int ref_count;	/* reserved */
	int hash;
	struct PAIR pair;
};

RB_HEAD(SMAP_TREE, SMAP_ENT);

struct BUCKET {
        long counter;
        struct SMAP_TREE root;
};
SLIST_HEAD(ENTRY_POOL_HEAD, SMAP_ENT);

struct SEGMENT {
		struct ENTRY_POOL_HEAD	entry_pool;
		long	counter;
		unsigned int		entry_pool_size;
		unsigned int		bucket_num;
		struct BUCKET *bp;
        rwlock_t seg_lock;
};

#define MAX_CAPACITY (1 << 30)
#define MAX_SEGMENTS (1 << 16) 

#define SMAP_LOCK_INIT(lock) lock_init(lock, mp->mt)
#define SMAP_LOCK_DESTROY(lock) lock_destroy(lock, mp->mt)
#define SMAP_WRLOCK(lock) wrlock(lock, mp->mt)
#define SMAP_RDLOCK(lock) rdlock(lock, mp->mt)
#define SMAP_UNLOCK(lock, write) unlock(lock, write, mp->mt)

#define IS_BIG_KEY(pair)  ((pair)->key_len >= sizeof(char *))

static inline int
memrcmp(const void *v1, const void *v2, size_t n)
{
	unsigned char *c1 = (unsigned char *)((unsigned char *)v1 + n),
	 *c2 = (unsigned char *)v2 + n;
	int ret = 0;

	while(n && (ret=*c1-*c2) == 0) n--,c1--,c2--;

	return ret;
}

static inline int
nhash(int h) 
{
	/*
	 * Spread bits to regularize both segment and index locations,
	 * using variant of single-word Wang/Jenkins hash.
	 */
	h += (h << 15) ^ 0xffffcd7d;
	h ^= (((unsigned int)h) >> 10);
	h += (h << 3);
	h ^= (((unsigned int)h) >>  6);
	h += (h << 2) + (h << 14);
	return h ^ (((unsigned int)h) >> 16);
}

static inline int
shash(char *str, int len)
{
	unsigned int seed = 1313131313; // 31 131 1313 13131 131313 etc..
	unsigned int hash = 0;
	int i = len;
 
	while (*str && i--) {
		hash = hash * seed + (*str++);
	}
	return (hash);
}

static inline int
ncmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	int64_t diff = a->pair.ikey - b->pair.ikey;
	
	return (diff ? (diff < 0 ? -1 : 1) : 0);

}

static inline int
scmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	long c = a->pair.key_len - b->pair.key_len;

	if (c == 0) {
		if (IS_BIG_KEY(&(a->pair))) {
			c = memrcmp(a->pair.skey, b->pair.skey, a->pair.key_len);
		} else {
			c = (uint64_t)(a->pair.skey) - (uint64_t)(b->pair.skey);
		}
		return (c ? (c < 0 ? -1 : 1) : 0);
	} else {
		return (c < 0 ? -1 : 1);
	}

}

/* 
 * It support both number and string.
 * We compare the types of keywords first, 
 * so that we can separate the numbers and strings in the tree, 
 * so we can iterate numbers and strings respectively, more faster.
 */
static inline int
smap_cmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	if (a->pair.type == b->pair.type) {
		if (a->hash == b->hash) {
			if (a->pair.type == KEYTYPE_NUM) {
				return (ncmp(a, b));
			} else {
				return (scmp(a, b));
			}
		} else {
			return (a->hash > b->hash ? 1 : -1);
		}
	} else {
		return (a->pair.type - b->pair.type);
	}
}


static inline int
check_str_pair(struct PAIR *pair)
{
	if (pair->key_len == 0 ||
		pair->key_len > SMAP_MAX_KEY_LEN ||
		pair->skey == NULL)
		return (SMAP_GENERAL_ERROR);
	else
		return (SMAP_OK);
}

static inline int
smap_pair_copyin(struct PAIR *dst, struct PAIR *src)
{
	if (src->type == KEYTYPE_NUM) {
		SMAP_SET_NUM_PAIR(dst, src->ikey, src->data);
	} else if (src->type == KEYTYPE_STR) {
		if (IS_BIG_KEY(src)) {
			dst->skey = (char *)malloc(src->key_len + 1);
			if (dst->skey == NULL)
				return (SMAP_OOM);
			memcpy(dst->skey, src->skey, src->key_len);
			dst->skey[src->key_len] = '\0';
		} else {
			memcpy((char *)(&(dst->skey)), src->skey, src->key_len);
			((char *)(&(dst->skey)))[src->key_len] = '\0';
		}
		dst->key_len = src->key_len;
		dst->data = src->data;
	} else {
		return (SMAP_GENERAL_ERROR);
	}
	dst->type = src->type;
	
	return (SMAP_OK);
}


static inline int
smap_pair_set_str_key(struct PAIR *dst, struct PAIR *src)
{
	if (IS_BIG_KEY(src)) {
		dst->skey = src->skey;
	} else {
		memcpy((char *)(&(dst->skey)), src->skey, src->key_len);
	}
	dst->key_len = src->key_len;
	dst->type = KEYTYPE_STR;
	return (SMAP_OK);
}

static inline int
smap_set_key(struct PAIR *dst, struct PAIR *src, int *hash)
{
	if (src->type == KEYTYPE_NUM) {
		SMAP_SET_NUM_KEY(dst, src->ikey);
		*hash = nhash((int)src->ikey);
	} else if (src->type == KEYTYPE_STR) {
		smap_pair_set_str_key(dst, src);
		*hash = shash((char *)src->skey, src->key_len);
	} else {
		return (SMAP_GENERAL_ERROR); 
	}
	return (SMAP_OK);
}

static int
smap_pair_copyout(char *dstkeybuf, struct PAIR *dst, struct PAIR *src)
{
	if (src->type == KEYTYPE_NUM) {
		dst->ikey = src->ikey;
		dst->data = src->data;
	} else if (src->type == KEYTYPE_STR) {
		dst->skey = dstkeybuf;
		if (src->key_len >= sizeof(char*)) {
			memcpy(dst->skey, src->skey, src->key_len);
		} else {
			memcpy(dst->skey, (char *)(&(src->skey)), src->key_len);
		}
		dst->skey[src->key_len] = '\0';

		dst->key_len = src->key_len;
		dst->data = src->data;
	} else {
		return (SMAP_GENERAL_ERROR);
	}
	dst->type = src->type;
	return (SMAP_OK);
}


static inline struct SEGMENT *
get_segment(struct SMAP *mp, int hash)
{
	int seg_hash;
	
	if (mp->seg_shift == 32) {
		return (mp->seg);
	} else {
		seg_hash = ((unsigned int)hash >> mp->seg_shift) & (mp->seg_mask);
		return (mp->seg + seg_hash);
	}
}

static inline struct BUCKET *
get_bucket(struct SEGMENT *sp, int hash)
{
	int idx = hash & (sp->bucket_num - 1);
	return &(sp->bp[idx]);
}

RB_PROTOTYPE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, smap_cmp);
RB_GENERATE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, smap_cmp);

static inline int
lock_init(rwlock_t *lock, int mt)
{
	if (mt) {
#ifdef SMAP_RWLOCK
		lock->state = 0;
#else
		return pthread_rwlock_init(lock, NULL);
#endif
	} else {
		return (0);
	}
	return (0);
}

static inline int
lock_destroy(rwlock_t *lock, int mt)
{
	if (mt) {
#ifdef SMAP_RWLOCK
		lock->state = 0;
#else
		return pthread_rwlock_destroy(lock);
#endif
	} else {
		return (0);
	}
	return (0);
}

static inline void
rdlock(rwlock_t *lock, int mt)
{
	if (mt) {
#ifdef SMAP_RWLOCK
		acquire(lock, 0);
#else
		int rc;
		rc = pthread_rwlock_rdlock(lock);
		if (rc != 0) {
			printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
			exit(1);
		}
#endif
	}
}

static inline void
wrlock(rwlock_t *lock, int mt)
{
	if (mt) {
#ifdef SMAP_RWLOCK
		acquire(lock, 1);
#else
		int rc;
		rc = pthread_rwlock_wrlock(lock);
		if (rc != 0) {
			printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
			exit(1);
		}
#endif
	}
}

static inline void
unlock(rwlock_t *lock, int write, int mt)
{
	if (mt) {
#ifdef SMAP_RWLOCK
		release(lock, write);
#else
		int rc;
		rc = pthread_rwlock_unlock(lock);
		if (rc != 0) {
			printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
			exit(1);
		}
#endif
	}
}


/*
 * This is a simple memory pool, itself is not thread safed,
 * but each memory pool all with "segment" one-to-one, 
 * lock segment will lock the memory pool, thus can be safe too.
 * Memory pool is a simple list structure, 
 * through the entry of fixed length to achieve rapid memory allocation.
 */
static inline struct SMAP_ENT *
entry_alloc(struct SMAP *mp, struct SEGMENT *sp)
{
	struct SMAP_ENT *ep;
	if (mp->mpool_enabled && !SLIST_EMPTY(&(sp->entry_pool))) {
		ep = SLIST_FIRST(&(sp->entry_pool));
		SLIST_REMOVE_HEAD(&(sp->entry_pool), mem_ent);
		sp->entry_pool_size--;
	} else {
		ep = (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
	}
	return ep;
}

static inline void
entry_free(struct SMAP *mp, struct SEGMENT *sp, struct SMAP_ENT *ptr)
{
	if (mp->mpool_enabled && sp->entry_pool_size < (sp->bucket_num*2)) {
		SLIST_INSERT_HEAD(&(sp->entry_pool), ptr, mem_ent);
		sp->entry_pool_size++;
	} else {
		free(ptr);
	}
}

static inline void
entry_deinit(struct SEGMENT *seg)
{
	struct SMAP_ENT *np, *tnp;

	SLIST_FOREACH_SAFE(np, &(seg->entry_pool), mem_ent, tnp) {
		SLIST_REMOVE(&(seg->entry_pool), np, SMAP_ENT, mem_ent) ;
		free(np);
	}
	seg->entry_pool_size = 0;
}

static inline int
entry_init(struct SEGMENT *seg, int entry_pool_size)
{
	int j;

	SLIST_INIT(&(seg->entry_pool));

	for (j = 0; j < entry_pool_size; j++) {
		struct SMAP_ENT *new_mem_ent =
		 (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
		if (new_mem_ent == NULL) {
			entry_deinit(seg);
			return (-1);
		}
		SLIST_INSERT_HEAD(&(seg->entry_pool), new_mem_ent, mem_ent);
	}
	seg->entry_pool_size = entry_pool_size;
	return (0);
}

static inline void
free_res(struct SMAP *mp, int i)
{
	while (i--) {
		entry_deinit(&(mp->seg[i]));
		free(mp->seg[i].bp);
		SMAP_LOCK_DESTROY(&(mp->seg[i].seg_lock));
	}
	free(mp->seg);
	free(mp);
}

/*
 *
 * level: The concurrency level
 */
struct SMAP *
smap_init(int capacity , float load_factor, int level, int entry_pool_size, int mt)
{
	struct BUCKET *bp;
	struct SMAP *mp;
	int i, j;
	int rc;
	int sshift = 0;
	int ssize = 1;
	int cap, c;

	printf("sizeof(smap):\t%d \nsizeof(pair):\t%d \nsizeof(ent):\t%d \nsizeof(seg):\t%d \nsizeof(bucket):\t%d\n",
	 sizeof(struct SMAP), sizeof(struct PAIR), sizeof(struct SMAP_ENT), sizeof(struct SEGMENT), sizeof(struct BUCKET));

	if (!(load_factor > 0) || capacity < 0 || level <= 0)
		return (NULL);

	mp = (struct SMAP *)malloc(sizeof(struct SMAP));
	if (mp == NULL)
		return (NULL);

	if (level > MAX_SEGMENTS)
		level = MAX_SEGMENTS;

	while ( ssize < level ) {
		++sshift;
		ssize <<= 1;
	}
	mp->mt = mt;
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
	if (mp->seg == NULL)
		return (NULL);

	mp->seg_num = ssize;

	if (entry_pool_size > 0 || entry_pool_size == DEFAULT_ENTRY_POOL_SIZE) {
		mp->mpool_enabled = 1;
	} else if (entry_pool_size == 0) {
		mp->mpool_enabled = 0;
	} else {
		return (NULL);
	}

	if (entry_pool_size == DEFAULT_ENTRY_POOL_SIZE)
		entry_pool_size = cap/2;

	for (i = 0; i < ssize; i++) {
		rc = SMAP_LOCK_INIT(&(mp->seg[i].seg_lock));
		if (rc != 0) {
			free_res(mp, i);
			return (NULL);
		}

		bp = (struct BUCKET *)malloc(sizeof(struct BUCKET) * cap);
		if (bp == NULL) {
			SMAP_LOCK_DESTROY(&(mp->seg[i].seg_lock));
			free_res(mp, i);
			return (NULL);
		}

		for (j = 0; j < cap; j++) {
			bp[j].counter = 0;
			RB_INIT(&(bp[j].root));
		}
		mp->seg[i].bucket_num = cap;	
		mp->seg[i].bp = bp;
		mp->seg[i].counter = 0;

		/* initialize the memory pool */
		if (entry_pool_size != 0) {
			rc = entry_init(&(mp->seg[i]), entry_pool_size);
			if (rc < 0) {
				SMAP_LOCK_DESTROY(&(mp->seg[i].seg_lock));
				free_res(mp, i);
				return (NULL);
			}
		
		} else {
			SLIST_INIT(&(mp->seg[i].entry_pool));
		}
	}
	return (mp);
}


/* XXX NOT Thread-safe! be sure there is not any other operation */
int
smap_deinit(struct SMAP *mp)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	unsigned int i, j;
	struct SEGMENT *sp;
	
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[i]);
		SMAP_WRLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);

			RB_FOREACH_SAFE(np, SMAP_TREE, &(bp->root), tnp) {
				RB_REMOVE(SMAP_TREE, &(bp->root), np);
				if (IS_BIG_KEY(&(np->pair)))
					free(np->pair.skey);
				
				/* XXX np is alloc from entry_alloc, free to new mempool? */
				free(np);
			}
		}
		free(sp->bp);
		entry_deinit(sp);
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		SMAP_LOCK_DESTROY(&(sp->seg_lock));
	}
	free(mp->seg);
	free(mp);
	return (0);
}


/*
 * clear a map, it would not always locked, just new the bucket and mpool.
 */
void
smap_clear(struct SMAP *mp, int start)
{
	unsigned int i, j;
	struct SEGMENT *sp;
	struct BUCKET *old_bp;
	struct BUCKET *new_bp;
	struct BUCKET *bp;
	struct SMAP_ENT *np, *tnp;
	unsigned int new_pool_size = 0;
	struct ENTRY_POOL_HEAD	new_pool, old_pool;
	

	for (i = 0; i < mp->seg_num; i++) {
		SLIST_INIT(&new_pool);
		new_pool_size = 0;
		sp = &(mp->seg[(i + start) % (mp->seg_num)]);

		new_bp =
			(struct BUCKET *)malloc(sp->bucket_num * sizeof(struct BUCKET));
		for (j = 0; j < sp->bucket_num; j++) {
			new_bp[j].counter = 0;
			RB_INIT(&(new_bp[j].root));
		}

		/* alloc a new bucket array and free old one. */
		SMAP_WRLOCK(&(sp->seg_lock));
		old_bp = sp->bp;
		sp->bp = new_bp;
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(old_bp[j]);
			RB_FOREACH_SAFE(np, SMAP_TREE, &(bp->root), tnp) {
				RB_REMOVE(SMAP_TREE, &(bp->root), np);
				if(np->pair.type == KEYTYPE_STR && 
					IS_BIG_KEY(&(np->pair)))
					free(np->pair.skey);
				
				/* XXX new_pool_size < bucket_num ? or a varible? */
				if (new_pool_size < sp->bucket_num) {
					SLIST_INSERT_HEAD(&new_pool, np, mem_ent);
					new_pool_size++;
				} else {
					free(np);
				}
			}
		}
		free(old_bp);
		
		/* 
		 * Process the new_pool. 
		 * We exchange the new_pool and orignal pool,
		 * If new_pool have more entry, use it,
		 * else use the orignal pool.
		 * and, free another one.
		 */
		SMAP_WRLOCK(&(sp->seg_lock));
		if (sp->entry_pool_size >= new_pool_size) {
			SMAP_UNLOCK(&(sp->seg_lock), 1);

			SLIST_FOREACH_SAFE(np, &new_pool, mem_ent, tnp) {
				SLIST_REMOVE(&new_pool, np, SMAP_ENT, mem_ent);
				free(np);
			}
		} else {
			old_pool = sp->entry_pool;
			sp->entry_pool = new_pool;
			sp->entry_pool_size = new_pool_size;
			SMAP_UNLOCK(&(sp->seg_lock), 1);
	
			SLIST_FOREACH_SAFE(np, &old_pool, mem_ent, tnp) {
				SLIST_REMOVE(&new_pool, np, SMAP_ENT, mem_ent);
				free(np);
			}
		}
	}
}


int
smap_put(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT *entry;
	struct SMAP_ENT *rc;
	struct SEGMENT *sp;
	int h;
	int r;

	if (mp == NULL || pair == NULL)
		return (SMAP_GENERAL_ERROR);
	
	if (pair->type == KEYTYPE_NUM) {
		h = nhash((int)pair->ikey);
	} else if (pair->type == KEYTYPE_STR) {
		r = check_str_pair(pair);
		if (r != SMAP_OK)
			return (r);
		h = shash((char *)pair->skey, pair->key_len);
	} else {
		return (SMAP_GENERAL_ERROR); 
	}

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_WRLOCK(&(sp->seg_lock));
		
	/*
	 * Because we have locked the segment,
	 * so although mpool itself is not thread safe,
	 * its in the segment, also can assure safety.
	 */
	entry = entry_alloc(mp, sp);
	if (entry == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_OOM);
	}
	
	/* Copy the key and value */
	r = smap_pair_copyin(&(entry->pair), pair);
	if (r != SMAP_OK)
		return (r);

	entry->hash = h;

	rc = RB_INSERT(SMAP_TREE, &(bp->root), entry);

	if (rc == NULL) {
		sp->counter++;
		bp->counter++;
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_OK);
	} else {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_DUPLICATE_KEY);
	}
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_delete(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *res;
	struct SEGMENT *sp;
	int h;
	int r;

	if (mp == NULL || pair == NULL)
		return (SMAP_GENERAL_ERROR);

	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (SMAP_GENERAL_ERROR); 

	entry.hash = h;
	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_WRLOCK(&(sp->seg_lock));
	
	res = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	if (res == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_NONEXISTENT_KEY);
	}
	RB_REMOVE(SMAP_TREE, &(bp->root), res);
	
	sp->counter--;
	bp->counter--;
	
	if (IS_BIG_KEY(&(res->pair)))
		free(res->pair.skey);

	entry_free(mp, sp, res);
	SMAP_UNLOCK(&(sp->seg_lock), 1);
	return (SMAP_OK);
}

int
smap_get_elm_num(struct SMAP *mp)
{
	int c = 0;
	unsigned int i;
		
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	
	for (i = 0; i < mp->seg_num; i++) {
		c += mp->seg[i].counter;
	}
	return c;
}

uint64_t
smap_get_segment_counter(struct SMAP *mp)
{
	uint64_t c = 0;
	unsigned int i;
		
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	
	for (i = 0; i < mp->seg_num; i++) {
		printf("segment:%d, num: %d\n", i, mp->seg[i].counter);
	}
	return c;
}

uint64_t
smap_get_bucket_counter(struct SMAP *mp)
{
	uint64_t c = 0;
	unsigned int i,j;
		
	if (mp == NULL)
		return (SMAP_GENERAL_ERROR);
	
	for (i = 0; i < mp->seg_num; i++) {
		for (j = 0; j < mp->seg[i].bucket_num; j++) {
			printf("segment: %d, bucket: %d, num: %d\n", i, j, mp->seg[i].bp[j].counter);
		}
	}
	return c;
}

/*
 */

void *
smap_get(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT *rc;
	struct SEGMENT *sp;
	struct SMAP_ENT entry;
	int h;
	int r;

	if (mp == NULL || pair == NULL)
		return (NULL);
	
	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (NULL);

	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_RDLOCK(&(sp->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	if (rc == NULL) {
		return (NULL);
	} else {
		pair->data = rc->pair.data;
		return (rc->pair.data);
	}
}

/*
 * it won't free the value, do it yourself.
 * XXX Maybe we need to upgrade the rdlock to wrlock
 */

void *
smap_update(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	int h, r;
	void *old_data;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);

	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (NULL);

	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_RDLOCK(&(sp->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	
	/* if no entry */
	if (rc == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (NULL);
	} else {
		old_data = rc->pair.data;
		rc->pair.data = pair->data;
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (old_data);
	}
}

/*
 * it won't free the value, do it yourself.
 */
int
smap_traverse_unsafe(
	struct SMAP *mp,
	smap_callback *routine,
	unsigned int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	unsigned int i, j;
	int rc;
	struct SEGMENT *sp;
	struct PAIR pair;
	char keybuf[SMAP_MAX_KEY_LEN+1];
	
	if (mp == NULL || routine == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);

			RB_FOREACH_SAFE(np, SMAP_TREE, &(bp->root), tnp) {
				smap_pair_copyout(keybuf, &pair, &(np->pair));
				rc = routine(mp, &pair);
				if (rc == SMAP_BREAK) {
					goto out;
				}
			}
		}
	}
	out:
	return 0;
}

int
smap_traverse_num_unsafe(
	struct SMAP *mp,
	smap_callback *routine,
	unsigned int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	unsigned int i, j;
	int rc;
	struct SEGMENT *sp;
	struct PAIR pair;
	char keybuf[SMAP_MAX_KEY_LEN+1];
	
	if (mp == NULL || routine == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);

			RB_FOREACH_SAFE(np, SMAP_TREE, &(bp->root), tnp) {
				if (np->pair.type == KEYTYPE_NUM) {
					smap_pair_copyout(keybuf, &pair, &(np->pair));
					rc = routine(mp, &pair);
					if (rc == SMAP_BREAK) {
						goto out;
					}
				} else {
					break;
				}
			}
		}
	}
	out:
	return 0;
}

/*
 * it won't free the value, do it yourself.
 */
int
smap_traverse_str_unsafe(
	struct SMAP *mp,
	smap_callback *routine,
	unsigned int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	unsigned int i, j;
	int rc;
	struct SEGMENT *sp;
	struct PAIR pair;
	char keybuf[SMAP_MAX_KEY_LEN+1];
	
	if (mp == NULL || routine == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);

			RB_FOREACH_REVERSE_SAFE(np, SMAP_TREE, &(bp->root), tnp) {
				if (np->pair.type == KEYTYPE_STR) {
					smap_pair_copyout(keybuf, &pair, &(np->pair));
					rc = routine(mp, &pair);
					if (rc == SMAP_BREAK) {
						goto out;
					}
				} else {
					break;
				}
			}
		}
	}
	out:
	return 0;
}

struct PAIR *
smap_get_first(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	unsigned int i, j;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL || keybuf == NULL)
		return (NULL);

	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);

		SMAP_RDLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);
			
			np = RB_MIN(SMAP_TREE, &(bp->root));
			
			if (np) {
				smap_pair_copyout(keybuf, pair, &(np->pair));
				SMAP_UNLOCK(&(sp->seg_lock), 0);
				return (pair);
			} else {
				continue;
			}
		}
		SMAP_UNLOCK(&(sp->seg_lock), 0);
	}
	return (NULL);
}

struct PAIR *
smap_get_first_num(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	unsigned int i, j;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL || keybuf == NULL)
		return (NULL);

	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);

		SMAP_RDLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);
			
			np = RB_MIN(SMAP_TREE, &(bp->root));
			
			if (np) {
				if (np->pair.type == KEYTYPE_NUM) {
					smap_pair_copyout(keybuf, pair, &(np->pair));
					SMAP_UNLOCK(&(sp->seg_lock), 0);
					return (pair);
				} else {
					continue;
				}
			}
		}
		SMAP_UNLOCK(&(sp->seg_lock), 0);
	}
	return (NULL);
}

struct PAIR *
smap_get_first_str(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	unsigned int i, j;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL || keybuf == NULL)
		return (NULL);

	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);

		SMAP_RDLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);
			
			np = RB_MAX(SMAP_TREE, &(bp->root));
			
			if (np) {
				if ( np->pair.type == KEYTYPE_STR) {
					smap_pair_copyout(keybuf, pair, &(np->pair));
					SMAP_UNLOCK(&(sp->seg_lock), 0);
					return (pair);
				} else {
					continue;
				}
			}
		}
		SMAP_UNLOCK(&(sp->seg_lock), 0);
	}
	return (NULL);
}
/*
 *  Because the pair may be deleted, 
 *  so the next pair can not be got by "pair 's next",
 *  only by the value of the current pair to find the next pair.
 */
struct PAIR *
smap_get_next(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *rc;
	struct SMAP_ENT entry;
	int h, r;
	unsigned int i;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);

	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (NULL);

	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_RDLOCK(&(sp->seg_lock));

	/* Get the next pair by the value of current pair */
	rc = RB_NFIND(SMAP_TREE, &(bp->root), &entry);
	if (rc && smap_cmp(rc, &entry) == 0) {
		/* We got the itself, get the next one */
		rc = RB_NEXT(SMAP_TREE, &(bp->root), rc);
	}
	
	if (rc) 
		goto got_pair;

	/* 
	 * If we did not get pair, look in the segment for each bucket,
	 * until you get the next pair in a tree.
	 */
	for (bp++; (bp - sp->bp) < sp->bucket_num; bp++) {
		rc = RB_MIN(SMAP_TREE, &(bp->root));
		
		if (rc) {
			goto got_pair;
		} else {
			continue;
		}
	}
	/*
	 * Find all the bucket from the previous segment?
	 * OK, it can only traverse the rest of the segment,
	 * to find the pair in the other segment.
	 * And we also need to unlock the segment above.	 	 
	 */	
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	while(1) {
		sp++;
		if ((sp - mp->seg) == mp->seg_num)
			sp = mp->seg;
		if (sp == (mp->seg + (start & mp->seg_shift))) {
			/* LOOP Back ! return NULL */
			return (NULL);
		} else {
			SMAP_RDLOCK(&(sp->seg_lock));
			for (i = 0; i < sp->bucket_num; i++) {
				bp = &(sp->bp[i]);
				
				rc = RB_MIN(SMAP_TREE, &(bp->root));
				
				if(rc) {
					goto got_pair;
				} else {
					continue;
				}
			}
			SMAP_UNLOCK(&(sp->seg_lock), 0);
		}
	}
	
	/* UNREACHED! */
	return (NULL);
	
got_pair:
	smap_pair_copyout(keybuf, pair, &(rc->pair));
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	return (pair);
}

/*
 *  Because the pair may be deleted, 
 *  so the next pair can not be got by "pair 's next",
 *  only by the value of the current pair to find the next pair.
 */
struct PAIR *
smap_get_next_num(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *rc;
	struct SMAP_ENT entry;
	int h;
	unsigned int i;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);

	if (pair->type == KEYTYPE_NUM) {
		h = nhash((int)pair->ikey);
		SMAP_SET_NUM_KEY(&(entry.pair), pair->ikey);
	} else {
		return (NULL);
	}
	
	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_RDLOCK(&(sp->seg_lock));

	/* Get the next pair by the value of current pair */
	rc = RB_NFIND(SMAP_TREE, &(bp->root), &entry);
	if (rc && smap_cmp(rc, &entry) == 0) {
		/* We got the itself, get the next one */
		rc = RB_NEXT(SMAP_TREE, &(bp->root), rc);
	}
	
	if (rc && rc->pair.type == KEYTYPE_NUM) 
		goto got_pair;

	/* 
	 * If we did not get pair, look in the segment for each bucket,
	 * until you get the next pair in a tree.
	 */
	for (bp++; (bp - sp->bp) < sp->bucket_num; bp++) {
		rc = RB_MIN(SMAP_TREE, &(bp->root));
		
		if (rc && rc->pair.type == KEYTYPE_NUM) {
			goto got_pair;
		} else {
			continue;
		}
	}
	/*
	 * Find all the bucket from the previous segment?
	 * OK, it can only traverse the rest of the segment,
	 * to find the pair in the other segment.
	 * And we also need to unlock the segment above.	 	 
	 */	
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	while(1) {
		sp++;
		if ((sp - mp->seg) == mp->seg_num)
			sp = mp->seg;
		if (sp == (mp->seg + (start & mp->seg_shift))) {
			/* LOOP Back ! return NULL */
			return (NULL);
		} else {
			SMAP_RDLOCK(&(sp->seg_lock));
			for (i = 0; i < sp->bucket_num; i++) {
				bp = &(sp->bp[i]);
				
				rc = RB_MIN(SMAP_TREE, &(bp->root));
				
				if(rc && rc->pair.type == KEYTYPE_NUM) {
					goto got_pair;
				} else {
					continue;
				}
			}
			SMAP_UNLOCK(&(sp->seg_lock), 0);
		}
	}
	
	/* UNREACHED! */
	return (NULL);
	
got_pair:
	smap_pair_copyout(keybuf, pair, &(rc->pair));
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	return (pair);
}

/*
 *  Because the pair may be deleted, 
 *  so the next pair can not be got by "pair 's next",
 *  only by the value of the current pair to find the next pair.
 */
struct PAIR *
smap_get_next_str(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *rc;
	struct SMAP_ENT entry;
	int h;
	unsigned int i;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);
	
	if (pair->type == KEYTYPE_STR) {
		h = shash((char *)pair->skey, pair->key_len);
		smap_pair_set_str_key(&(entry.pair), pair);
	} else {
		return (NULL);
	}
	

	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_RDLOCK(&(sp->seg_lock));

	/* Get the next pair by the value of current pair */
	rc = RB_PFIND(SMAP_TREE, &(bp->root), &entry);
	if (rc && smap_cmp(rc, &entry) == 0) {
		/* We got the itself, get the prev one */
		rc = RB_PREV(SMAP_TREE, &(bp->root), rc);
	}
	
	if (rc && rc->pair.type == KEYTYPE_STR) 
		goto got_pair;

	/* 
	 * If we did not get pair, look in the segment for each bucket,
	 * until you get the next pair in a tree.
	 */
	for (bp++; (bp - sp->bp) < sp->bucket_num; bp++) {
		rc = RB_MAX(SMAP_TREE, &(bp->root));
		
		if (rc && rc->pair.type == KEYTYPE_STR) {
			goto got_pair;
		} else {
			continue;
		}
	}
	/*
	 * Find all the bucket from the previous segment?
	 * OK, it can only traverse the rest of the segment,
	 * to find the pair in the other segment.
	 * And we also need to unlock the segment above.	 	 
	 */	
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	while(1) {
		sp++;
		if ((sp - mp->seg) == mp->seg_num)
			sp = mp->seg;
		if (sp == (mp->seg + (start & mp->seg_shift))) {
			/* LOOP Back ! return NULL */
			return (NULL);
		} else {
			SMAP_RDLOCK(&(sp->seg_lock));
			for (i = 0; i < sp->bucket_num; i++) {
				bp = &(sp->bp[i]);
				
				rc = RB_MAX(SMAP_TREE, &(bp->root));
				
				if(rc && rc->pair.type == KEYTYPE_STR) {
					goto got_pair;
				} else {
					continue;
				}
			}
			SMAP_UNLOCK(&(sp->seg_lock), 0);
		}
	}
	
	/* UNREACHED! */
	return (NULL);
	
got_pair:
	smap_pair_copyout(keybuf, pair, &(rc->pair));
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	return (pair);
}


