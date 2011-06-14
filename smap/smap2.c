#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "smap2.h"
#include "tree.h"
#include "queue.h"

/*
 * need: deinit, resize, mempool, ref_count?, lua
 */

struct SMAP_ENT {
	union {
		SLIST_ENTRY(SMAP_ENT) mem_ent;
		RB_ENTRY(SMAP_ENT)    val_ent;
	};
	int hash;
	int ref_count;	/* reserved */
	struct PAIR pair;
};

RB_HEAD(SMAP_TREE, SMAP_ENT);

struct BUCKET {
        long counter;
        struct SMAP_TREE root;
};
SLIST_HEAD(ENTRY_POOL_HEAD, SMAP_ENT);

struct SEGMENT {
		struct ENTRY_POOL_HEAD	*entry_pool;
		long	counter;
		int		entry_pool_size;
		int		bucket_num;
		struct BUCKET *bp;
        pthread_rwlock_t seg_lock;
};

#define MAX_CAPACITY (1 << 30)
#define MAX_SEGMENTS (1 << 16) 

#define SMAP_WRLOCK(lock) wrlock(lock, mp->mt)
#define SMAP_RDLOCK(lock) rdlock(lock, mp->mt)
#define SMAP_UNLOCK(lock) unlock(lock, mp->mt)

#define IS_BIG_KEY(pair)  ((pair)->key_len >= sizeof(char *))




int
memrcmp(const void *v1,const void *v2,size_t n)
{
	const unsigned char *c1=v1 + n,*c2=v2 + n;
	int ret=0;

	while(n && (ret=*c1-*c2)==0) n--,c1--,c2--;

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
	} else 
		return (c < 0 ? -1 : 1);

}

/* It support both number and string */
static inline int
smap_cmp(struct SMAP_ENT *a, struct SMAP_ENT *b)
{
	if (a->hash == b->hash) {
		if (a->pair.type == b->pair.type) {
			if (a->pair.type == HASHTYPE_NUM) {
				return ncmp(a, b);
			} else {
				return scmp(a, b);
			}
		} else {
			return (a->pair.type - b->pair.type);
		}
	} else {
		return (a->hash > b->hash ? 1 : -1);
	}
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
	int index = hash & (sp->bucket_num - 1);
	return &(sp->bp[index]);
}

RB_PROTOTYPE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, smap_cmp);
RB_GENERATE_STATIC(SMAP_TREE, SMAP_ENT, val_ent, smap_cmp);

static void
rdlock(pthread_rwlock_t *lock, int mt)
{
	if (mt) {
		int rc;
		rc = pthread_rwlock_rdlock(lock);
		if (rc != 0) {
			printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
			exit(1);
		}
	}
}

static void
wrlock(pthread_rwlock_t *lock, int mt)
{
	if (mt) {
		int rc;
		rc = pthread_rwlock_wrlock(lock);
		if (rc != 0) {
			printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
			exit(1);
		}
	}
}

static void
unlock(pthread_rwlock_t *lock, int mt)
{
	if (mt) {
		int rc;
	
		rc = pthread_rwlock_unlock(lock);
		if (rc != 0) {
			printf("lock! error: %d: %s \n", __LINE__, strerror(rc));
			exit(1);
		}
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
entry_alloc(struct SMAP *mp, struct SEGMENT *sp)
{
	struct SMAP_ENT *ep;
	if (!SLIST_EMPTY(sp->entry_pool) && mp->mpool_enabled) {
		ep = SLIST_FIRST(sp->entry_pool);
		SLIST_REMOVE_HEAD(sp->entry_pool, mem_ent);
		sp->entry_pool_size--;
	} else {
		ep = (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
	}
	return ep;
}

static void
entry_free(struct SMAP *mp, struct SEGMENT *sp, struct SMAP_ENT *ptr)
{
	if (sp->entry_pool_size < (sp->bucket_num*2) && mp->mpool_enabled) {
		SLIST_INSERT_HEAD(sp->entry_pool, ptr, mem_ent);
		sp->entry_pool_size++;
	} else {
		free(ptr);
	}
}

static void
entry_deinit(struct SEGMENT *seg)
{
	struct SMAP_ENT *np, *tnp;

	SLIST_FOREACH_SAFE(np, seg->entry_pool, mem_ent, tnp) {
		SLIST_REMOVE(seg->entry_pool, np, SMAP_ENT, mem_ent) ;
		free(np);
	}
	seg->entry_pool_size = 0;
}

static int
entry_init(struct SEGMENT *seg, int entry_pool_size)
{
	int j;
	seg->entry_pool =
	(struct ENTRY_POOL_HEAD *)malloc(sizeof(struct ENTRY_POOL_HEAD));
	if (seg->entry_pool == NULL) {
		return (-1);
	}

	SLIST_INIT(seg->entry_pool);

	for (j = 0; j < entry_pool_size; j++) {
		struct SMAP_ENT *new_mem_ent =
		 (struct SMAP_ENT *)malloc(sizeof(struct SMAP_ENT));
		if (new_mem_ent == NULL) {
			entry_deinit(seg);
			return (-1);
		}
		SLIST_INSERT_HEAD(seg->entry_pool, new_mem_ent, mem_ent);
	}
	seg->entry_pool_size = entry_pool_size;
	return (0);
}




static void
free_res(struct SMAP *mp, int i)
{
	struct SMAP_ENT *np, *tnp;

	while (i--) {
		entry_deinit(&(mp->seg[i]));
		free(mp->seg[i].entry_pool);
		free(mp->seg[i].bp);
		pthread_rwlock_destroy(&(mp->seg[i].seg_lock));
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
		rc = pthread_rwlock_init(&(mp->seg[i].seg_lock), NULL);
		if (rc != 0) {
			free_res(mp, i);
			return (NULL);
		}

		bp = (struct BUCKET *)malloc(sizeof(struct BUCKET) * cap);
		if (bp == NULL) {
			pthread_rwlock_destroy(&(mp->seg[i].seg_lock));
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
				pthread_rwlock_destroy(&(mp->seg[i].seg_lock));
				free_res(mp, i);
				return (NULL);
			}
		
		}
	}
	return (mp);
}

/*
 * clear a map, it would not always lock, just realloc the bucket and mpool.
 */
void
smap_clear(struct SMAP *mp, int start)
{
	int i, j;
	struct SEGMENT *sp;
	struct BUCKET *old_bp;
	struct BUCKET *new_bp;
	struct BUCKET *bp;
	struct SMAP_ENT *np, *tnp;
	int entry_pool_size = 0;
	struct ENTRY_POOL_HEAD	*entry_pool, *tentry_pool;
	
	entry_pool = 
		(struct ENTRY_POOL_HEAD *)malloc(sizeof(struct ENTRY_POOL_HEAD));

	for (i = 0; i < mp->seg_num; i++) {
		SLIST_INIT(entry_pool);
		sp = &(mp->seg[(i + start) % (mp->seg_num)]);

		new_bp =
			(struct BUCKET *)malloc(sp->bucket_num * sizeof(struct BUCKET));
		for (j = 0; j < sp->bucket_num; j++) {
			new_bp[j].counter = 0;
			RB_INIT(&(new_bp[j].root));
		}

		/* alloc a new bucket array and delete the old one. */
		SMAP_WRLOCK(&(sp->seg_lock));
		old_bp = sp->bp;
		sp->bp = new_bp;
		SMAP_UNLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(old_bp[j]);
			for (np = RB_MIN(SMAP_TREE, &(bp->root));
				np && ((tnp) = RB_NEXT(SMAP_TREE, &(bp->root), np), 1);
				np = tnp) {
				RB_REMOVE(SMAP_TREE, &(bp->root), np);
				if(np->pair.type == HASHTYPE_STR && 
					IS_BIG_KEY(&(np->pair)))
					free(np->pair.skey);
				SLIST_INSERT_HEAD(entry_pool, np, mem_ent);
				
				if (entry_pool_size < sp->bucket_num)
					entry_pool_size++;
				else
					free(np);
			}
		}
		SMAP_WRLOCK(&(sp->seg_lock));
		free(old_bp);
		
		/* Process the entry_pool. */
		if (sp->entry_pool_size >= entry_pool_size) {
			SMAP_UNLOCK(&(sp->seg_lock));
			SLIST_FOREACH_SAFE(np, entry_pool, mem_ent, tnp) {
				SLIST_REMOVE(entry_pool, np, SMAP_ENT, mem_ent);
				free(np);
			}
		} else {
			tentry_pool = sp->entry_pool;
			sp->entry_pool = entry_pool;
			sp->entry_pool_size = entry_pool_size;
			SMAP_UNLOCK(&(sp->seg_lock));
			SLIST_FOREACH_SAFE(np, tentry_pool, mem_ent, tnp) {
				SLIST_REMOVE(entry_pool, np, SMAP_ENT, mem_ent);
				free(np);
			}
			free(tentry_pool);
			entry_pool = tentry_pool;
		}
		entry_pool_size = 0;
	}
}

int
smap_insert(struct SMAP *mp, struct PAIR *pair, int lock)
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
		if (pair->key_len == 0 ||
			pair->key_len > SMAP_MAX_KEY_LEN ||
			pair->skey == NULL)
			return (SMAP_GENERAL_ERROR);

		h = shash((char *)pair->skey, pair->key_len);
	} else {
		return (SMAP_GENERAL_ERROR);
	}

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	if (lock)
		SMAP_WRLOCK(&(sp->seg_lock));
		
	/*
	 * Because we have locked the segment,
	 * so although mpool itself is not thread safe,
	 * its in the segment, also can assure safety.
	 */
	entry = entry_alloc(mp, sp);
	if (entry == NULL) {
		if (lock)
			SMAP_UNLOCK(&(sp->seg_lock));
		return (SMAP_OOM);
	}
	
	/* Copy the key and value */
	if (pair->type == HASHTYPE_NUM) {
		entry->pair.ikey = pair->ikey;
		entry->pair.data = pair->data;
	} else if (pair->type == HASHTYPE_STR) {
		if (IS_BIG_KEY(pair)) {
			entry->pair.skey = (char *)malloc(pair->key_len + 1);
			memcpy(entry->pair.skey, pair->skey, pair->key_len);
			entry->pair.skey[pair->key_len] = '\0';
		} else {
			memcpy((char *)(&(entry->pair.skey)), pair->skey, pair->key_len);
			((char *)(&entry->pair.skey))[pair->key_len] = '\0';
		}
		entry->pair.key_len = pair->key_len;
	} else {
		return (SMAP_GENERAL_ERROR);
	}

	entry->pair.type = pair->type;
	entry->pair.data = pair->data;
	entry->hash = h;

	rc = RB_INSERT(SMAP_TREE, &(bp->root), entry);

	if (rc == NULL) {
		sp->counter++;
		bp->counter++;
		if (lock)
			SMAP_UNLOCK(&(sp->seg_lock));
		return (SMAP_OK);
	} else {
		if (lock)
			SMAP_UNLOCK(&(sp->seg_lock));
		return (SMAP_DUPLICATE_KEY);
	}
}

/*
 * it won't free the value, do it yourself.
 */

int
smap_delete(struct SMAP *mp, struct PAIR *pair, int lock)
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
		if (IS_BIG_KEY(pair))
			entry.pair.skey = pair->skey;
		else
			memcpy((char *)(&(entry.pair.skey)), pair->skey, pair->key_len);

		entry.pair.key_len = pair->key_len;
	} else {
		return (SMAP_GENERAL_ERROR);
	}
	entry.pair.type = pair->type;
	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	if (lock)
		SMAP_WRLOCK(&(sp->seg_lock));
	
	res = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	if (res == NULL) {
		if (lock)
			SMAP_UNLOCK(&(sp->seg_lock));
		return (SMAP_NONEXISTENT_KEY);
	}
	RB_REMOVE(SMAP_TREE, &(bp->root), res);
	
	sp->counter--;
	bp->counter--;
	
	if (IS_BIG_KEY(&(res->pair)))
		free(res->pair.skey);

	entry_free(mp, sp, res);
	if (lock)
		SMAP_UNLOCK(&(sp->seg_lock));
	return (SMAP_OK);
}

uint64_t
smap_get_elm_num(struct SMAP *mp)
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

uint64_t
smap_get_segment_counter(struct SMAP *mp)
{
	uint64_t c = 0;
	int i;
		
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
	int i,j;
		
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
 * it won't free the value, do it yourself.
 */

void *
smap_get(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *rc;
	int h;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);

	if (pair->type == HASHTYPE_NUM) {
		h = nhash((int)pair->ikey);
		entry.pair.ikey = pair->ikey;
	} else if (pair->type == HASHTYPE_STR) {
		if (pair->key_len == 0 ||
			pair->key_len > SMAP_MAX_KEY_LEN ||
			pair->skey == NULL)
			return (NULL);

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
	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	
	SMAP_RDLOCK(&(sp->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	SMAP_UNLOCK(&(sp->seg_lock));
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
smap_update(struct SMAP *mp, struct PAIR *pair)
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
	entry.hash = h;

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	
	SMAP_RDLOCK(&(sp->seg_lock));
	rc = RB_FIND(SMAP_TREE, &(bp->root), &entry);
	
	/* if no entry */
	if (rc == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock));
		return (NULL);
	} else {
		old_data = rc->pair.data;
		rc->pair.data = pair->data;
		SMAP_UNLOCK(&(sp->seg_lock));
		return (old_data);
	}
}

/*
 * it won't free the value, do it yourself.
 */
int
smap_traverse(struct SMAP *mp, smap_callback *routine, uint32_t start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	int i, j;
	int rc;
	struct SEGMENT *sp;
	
	if (mp == NULL || routine == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) & mp->seg_shift]);
		SMAP_WRLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);

			for (np = RB_MIN(SMAP_TREE, &(bp->root));
				np && ((tnp) = RB_NEXT(SMAP_TREE, &(bp->root), np), 1);
				np = tnp) {
				rc = routine(mp, (struct PAIR *)(&(np->pair)));
				if (rc == SMAP_BREAK) {
					SMAP_UNLOCK(&(sp->seg_lock));
					goto out;
				}
			}
		}
		SMAP_UNLOCK(&(sp->seg_lock));
	}
	out:
	return 0;
}

static void
smap_pair_copyout(char *dstkeybuf, struct PAIR *dst, struct PAIR *src)
{
	dst->type = src->type;
	if (src->type == HASHTYPE_NUM) {
		dst->ikey = src->ikey;
		dst->data = src->data;
	} else if (src->type == HASHTYPE_STR) {
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
		return;
	}
}

struct PAIR *
smap_get_first(struct SMAP *mp, struct PAIR *pair, char *keybuf, int start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	int i, j;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL || keybuf == NULL)
		return (NULL);

	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) & mp->seg_shift]);

		SMAP_RDLOCK(&(sp->seg_lock));
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);
			
			np = RB_MIN(SMAP_TREE, &(bp->root));
			
			if (np) {
				smap_pair_copyout(keybuf, pair, &(np->pair));
				SMAP_UNLOCK(&(sp->seg_lock));
				return (pair);
			} else {
				continue;
			}
		}
		SMAP_UNLOCK(&(sp->seg_lock));
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
	int h, i;
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
	SMAP_UNLOCK(&(sp->seg_lock));
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
			SMAP_UNLOCK(&(sp->seg_lock));
		}
	}
	
	/* UNREACHED! */
	return (NULL);
	
got_pair:
	smap_pair_copyout(keybuf, pair, &(rc->pair));
	SMAP_UNLOCK(&(sp->seg_lock));
	return (pair);
}

int shit(struct SMAP * mp, struct PAIR *p)
{
	return 0;
}
#ifdef __SMAP_MAIN__

#include <sys/time.h>
struct timeval tvafter,tvpre;
struct timezone tz;
#define LOOP_TIMES 1000000

char buf[LOOP_TIMES][64];


int
main(void)
{
	struct SMAP* map;
	struct PAIR pair;
	struct PAIR *p;
	char keybuf[SMAP_MAX_KEY_LEN + 1];
	long i;
	int rc;

	printf("sizeof(smap):\t%d \nsizeof(pair):\t%d \nsizeof(ent):\t%d \nsizeof(seg):\t%d \nsizeof(bucket):\t%d\n",
	 sizeof(struct SMAP), sizeof(struct PAIR), sizeof(struct SMAP_ENT), sizeof(struct SEGMENT), sizeof(struct BUCKET));

	map = smap_init(LOOP_TIMES,
		DEFAULT_LOAD_FACTOR, 128, LOOP_TIMES/100, 0);

	if (map == NULL)
		printf("error map NULL \n");



	int len;

	for (i = 0; i < LOOP_TIMES; i++) {
		sprintf(buf[i], "%07d", i);
	}


	gettimeofday (&tvpre , &tz);

	for (i = 0; i < LOOP_TIMES; i++) {
		if (i%2)
			SMAP_SET_NUM_PAIR(&pair, i, buf[i]);
		else
			SMAP_SET_STR_PAIR(&pair, buf[i], 7, buf[i]);
		rc = smap_insert(map, &pair, 1);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
		}
	}
	gettimeofday (&tvafter , &tz);


printf("insert int&str: %d times: %dms\n", LOOP_TIMES, (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);



	void *val;
	gettimeofday (&tvpre , &tz);

	for (i = 0; i < LOOP_TIMES; i++) {
		if (i%2)
			SMAP_SET_NUM_KEY(&pair, i);
		else
			SMAP_SET_STR_KEY(&pair, buf[i], 7);
		val = smap_get(map, &pair);
		if (val == NULL){
			printf("i: %d, get error\n", i);
		} else {
//			printf("%s\n", (char *)val);
		}
	}
	gettimeofday (&tvafter , &tz);

printf("get: %d times: %dms\n", LOOP_TIMES, (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);


	i = 0;
	memset(&pair, 0, sizeof(pair));
	gettimeofday (&tvpre , &tz);
	for (p = smap_get_first(map, &pair, keybuf, -1);
		p != NULL; p = smap_get_next(map, p, keybuf, -1)) {
		i++;
		/*
		int c = SMAP_IS_NUM(&pair);
		if (c)
			printf("no: %08d, key: %014d, value: %s\n", i, p->ikey, p->data);
		else
			printf("no: %08d, key: \"%x12s\", value: %s\n", i, p->skey, p->data);
		*/
	}
	gettimeofday (&tvafter , &tz);
printf("traverse 1 time: %dms\n", (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);
	
	i = 0;
	memset(&pair, 0, sizeof(pair));
	gettimeofday (&tvpre , &tz);
smap_traverse(map, shit, 0);
	gettimeofday (&tvafter , &tz);
printf("traverse 2 time: %dms\n", (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);
//#endif
//smap_get_bucket_counter(map);
//smap_get_segment_counter(map);

	return (0);
}
#endif
