/*-
 * Copyright (c) 2010-2011 zhuyan <nayuhz@gmail.com>
 * SMAP: A simple implementation of concurrent hashmap.
 * Support for integer and string as key (can be mixed).
 * This is a simple concurrent hashmap implementation that uses the classic
 * segment-bucket method to complete the concurrent control.
 * The data structures using a hash table + red-black tree, 
 * do not use the classic hash table + single linked list. 
 * The following reasons:
 * 1. In the conflict resolved, the performance of a single linked list is
 * worse than the red-black tree.
 * 2. red-black tree is ordered, which makes it possible to sort keys.
 * But the red-black tree is relatively space-consuming.
 *
 *
                +------+          
                | map_t|          
                +------+          
+-------segments-+                  
|         +-seg+ | +----+     +----+
|         |lock| | |lock| ... |lock|
|         +----+ | +----+     +----+
|buckets         |               
|+-+-+-+ ... +-+ | +-+...     +-+...                 
|+-+-+-+     +-+ | +-+        +-+         
|    rb-tree /\  |
|     entry #  # |
+----------------+
 */
 

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

/* Default use rbtree */
#ifndef	SMAP_USE_SLIST
#define SMAP_USE_RBTREE
#define SMAP_CONFLICT_TYPE	SMAP_RBTREE
#else
#define SMAP_CONFLICT_TYPE	SMAP_SLIST
#endif

/*
 * need: resize, mempool, ref_count?, STORE_VALUE/STORE_POINTER, lua
 */

struct SMAP_ENT {
	union {
		SLIST_ENTRY(SMAP_ENT) mem_ent;	/* memory pool entry */
#ifdef	SMAP_USE_RBTREE
		RB_ENTRY(SMAP_ENT)    node;	/* the bucket red-black tree entry */
#else
		SLIST_ENTRY(SMAP_ENT) node;	/* memory pool entry */
#endif
	};
	uint32_t copied_data;	/* reserved */
	int hash;	/* the hash code of key, both string or number */
	struct PAIR pair;	/* the k-v pair */
};
#ifdef	SMAP_USE_RBTREE
RB_HEAD(SMAP_TREE, SMAP_ENT);
#else
SLIST_HEAD(SMAP_LIST, SMAP_ENT);
#endif

struct BUCKET {
        intptr_t counter;	/* the number of smap_ent under the bucket */
        union {
#ifdef	SMAP_USE_RBTREE
			struct SMAP_TREE root_head;	/* the root of rbtree */
#else
			struct SMAP_LIST root_head;
#endif
    	};
};
SLIST_HEAD(ENTRY_POOL_HEAD, SMAP_ENT);

struct SEGMENT {
		struct ENTRY_POOL_HEAD	entry_pool;	/* for allocation of smap_ent */
		intptr_t	counter;	/* the number of smap_ent under the bucket */
		uint32_t		entry_pool_size;
		/* the number of buckets under the segment*/
		uint32_t		bucket_num;
		struct BUCKET *bp;	/* the bucket array, the true hash table */
        rwlock_t seg_lock;	/* lock the sgemtn in rwlock */
};

#define MAX_CAPACITY (1 << 30)
#define MAX_SEGMENTS (1 << 16) 

#define SMAP_LOCK_INIT(lock) lock_init(lock, mp->mt)
#define SMAP_LOCK_DESTROY(lock) lock_destroy(lock, mp->mt)
#define SMAP_WRLOCK(lock) wrlock(lock, mp->mt)
#define SMAP_RDLOCK(lock) rdlock(lock, mp->mt)
#define SMAP_UNLOCK(lock, write) unlock(lock, write, mp->mt)

#define IS_BIG_KEY(pair)  ((pair)->key_len >= sizeof(char *))
#define IS_BIG_VALUE(pair)  ((pair)->data_len > sizeof(void *))

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
			c = memcmp(a->pair.skey, b->pair.skey, a->pair.key_len);
		} else {
		//	c = (uint64_t)(a->pair.skey) - (uint64_t)(b->pair.skey);
			c = memcmp((char *)(&(a->pair.skey)), (char *)(&(b->pair.skey)), a->pair.key_len);
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

#ifdef SMAP_USE_RBTREE
RB_PROTOTYPE_STATIC(SMAP_TREE, SMAP_ENT, node, smap_cmp);
RB_GENERATE_STATIC(SMAP_TREE, SMAP_ENT, node, smap_cmp);
#endif

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
smap_pair_key_copyin(struct PAIR *dst, struct PAIR *src)
{
	if (src->type == KEYTYPE_NUM) {
		dst->ikey = src->ikey;
		dst->key_len = 0;
	} else if (src->type == KEYTYPE_STR) {
		if (IS_BIG_KEY(src)) {
			dst->skey = (char *)malloc(src->key_len + 1);
			if (dst->skey == NULL)
				return (SMAP_OOM);
			memcpy(dst->skey, src->skey, src->key_len);
			dst->skey[src->key_len] = '\0';
		} else {
			/* Clean the skey data to 0*/
			dst->ikey = 0;

			memcpy((char *)(&(dst->skey)), src->skey, src->key_len);

			((char *)(&(dst->skey)))[src->key_len] = '\0';
		}
		dst->key_len = src->key_len;
	} else {
		return (SMAP_GENERAL_ERROR);
	}
	dst->type = src->type;

	return (SMAP_OK);
}

static inline int
smap_pair_val_copyin(struct PAIR *dst, struct PAIR *src, int copy_data)
{
	if (copy_data) {
		if (IS_BIG_VALUE(src)) {
			dst->data = malloc(src->data_len);
			if (dst->data == NULL)
				return (SMAP_OOM);
			memcpy(dst->data, src->data, src->data_len);
		} else {
			memcpy(&(dst->data), src->data, src->data_len);
		}
	} else {
		dst->data = src->data;
	}
	dst->data_len = src->data_len;
	
	return (SMAP_OK);
}

static inline int
smap_pair_copyin(struct PAIR *dst, struct PAIR *src, int copy_data)
{
	int rc;

	/* copy the key */
	rc = smap_pair_key_copyin(dst, src);
	if (rc != SMAP_OK)
		return (rc);
	
	/* copy the value */
	rc = smap_pair_val_copyin(dst, src, copy_data);
	if (rc != SMAP_OK)
		return (rc);

	return (SMAP_OK);
}


static inline int
smap_pair_set_str_key(struct PAIR *dst, struct PAIR *src)
{
	if (IS_BIG_KEY(src)) {
		dst->skey = src->skey;
	} else {
		memcpy((char *)(&(dst->skey)), src->skey, src->key_len);
		((char *)(&(dst->skey)))[src->key_len] = '\0';
//dst->skey = (intptr_t)(*src->skey);
	}
	dst->key_len = src->key_len;
	dst->type = KEYTYPE_STR;
	return (SMAP_OK);
}

static inline int
smap_set_key(struct PAIR *dst, struct PAIR *src, int *hash)
{
	int h;

	if (src->type == KEYTYPE_NUM) {
		SMAP_SET_NUM_KEY(dst, src->ikey);
		h = nhash((int)src->ikey);
	} else if (src->type == KEYTYPE_STR) {
		smap_pair_set_str_key(dst, src);
		h = shash((char *)src->skey, src->key_len);
	} else {
		return (SMAP_GENERAL_ERROR); 
	}
	*hash = h;
	return (SMAP_OK);
}

inline static int
smap_pair_key_copyout(
	char *dstkeybuf,
	int buf_len,
	struct PAIR *dst,
	struct PAIR *src)
{
	if (src->type == KEYTYPE_NUM) {
		dst->ikey = src->ikey;
	} else if (src->type == KEYTYPE_STR) {
		dst->skey = dstkeybuf;
		if (IS_BIG_KEY(src)) {
			if (src->key_len > buf_len)
				return (SMAP_BUFFER_TOO_SHORT);
			memcpy(dst->skey, src->skey, src->key_len);
		} else {
			memcpy(dst->skey, (char *)(&(src->skey)), src->key_len);
		}
		dst->skey[src->key_len] = '\0';
		dst->key_len = src->key_len;
	} else {
		return (SMAP_GENERAL_ERROR);
	}
	return (SMAP_OK);
}

inline static int
smap_pair_copyout(
	char *dstkeybuf,
	int buf_len,
	struct PAIR *dst,
	struct PAIR *src)
{
	int rc;
	rc = smap_pair_key_copyout(dstkeybuf, buf_len, dst, src);
	if (rc != SMAP_OK)
		return (rc);
	
	/* don't copy value, just use pointer */
	if (IS_BIG_VALUE(src))
		dst->data = src->data;
	else
		dst->data = &(src->data);
	dst->data_len = src->data_len;
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
smap_init(
	int capacity,
	float load_factor,
	int level,
	int entry_pool_size,
	int mt)
{
	struct BUCKET *bp;
	struct SMAP *mp;
	int i, j;
	int rc;
	int sshift = 0;
	int ssize = 1;
	int cap, c;

	printf("conflict_type: %d\n", SMAP_CONFLICT_TYPE);
	printf("sizeof(smap):\t%lu \nsizeof(pair):\t%lu \nsizeof(ent):\t%lu \nsizeof(seg):\t%lu \nsizeof(bucket):\t%lu\n",
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
#ifdef	SMAP_USE_RBTREE
			RB_INIT(&(bp[j].root_head));
#else
			SLIST_INIT(&(bp[j].root_head));
#endif
		}
		mp->conflict_type = SMAP_CONFLICT_TYPE;
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

#ifdef	SMAP_USE_RBTREE
static inline void
_deinit(struct SMAP_TREE *root)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;

	RB_FOREACH_SAFE(np, SMAP_TREE, root, tnp) {
		RB_REMOVE(SMAP_TREE, root, np);
		if (IS_BIG_KEY(&(np->pair)))
			free(np->pair.skey);
		if (np->copied_data && IS_BIG_VALUE(&(np->pair)))
			free(np->pair.data);
		
		/* XXX np is alloc from entry_alloc, free to new mempool? */
		free(np);
	}
}

#else
static inline void
_deinit(struct SMAP_LIST *head)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	
	while (!SLIST_EMPTY(head)) {           /* List Deletion. */
		np = SLIST_FIRST(head);
		SLIST_REMOVE_HEAD(head, node);
		if (IS_BIG_KEY(&(np->pair)))
			free(np->pair.skey);
		if (np->copied_data && IS_BIG_VALUE(&(np->pair)))
			free(np->pair.data);
		
		/* XXX np is alloc from entry_alloc, free to new mempool? */
		free(np);
	}
}
#endif

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
			_deinit(&(bp->root_head));
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

#ifdef	SMAP_USE_RBTREE
static inline void
_clear(
	struct SMAP_TREE *root,
	struct ENTRY_POOL_HEAD *new_pool,
	int *pool_size,
	int bucket_num)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;

	RB_FOREACH_SAFE(np, SMAP_TREE, root, tnp) {
		RB_REMOVE(SMAP_TREE, root, np);
		if (IS_BIG_KEY(&(np->pair)))
			free(np->pair.skey);
		if (np->copied_data && IS_BIG_VALUE(&(np->pair)))
			free(np->pair.data);
		
		/* XXX np is alloc from entry_alloc, free to new mempool? */
		free(np);
		
		/* XXX new_pool_size < bucket_num ? or a varible? */
		if ((*pool_size) < bucket_num) {
			SLIST_INSERT_HEAD(new_pool, np, mem_ent);
			(*pool_size)++;
		} else {
			free(np);
		}
	}
}
#else
static inline void
_clear(
	struct SMAP_LIST *head,
	struct ENTRY_POOL_HEAD *new_pool,
	int *pool_size,
	int bucket_num)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	
	while (!SLIST_EMPTY(head)) {           /* List Deletion. */
		np = SLIST_FIRST(head);
		SLIST_REMOVE_HEAD(head, node);
		if (IS_BIG_KEY(&(np->pair)))
			free(np->pair.skey);
		if (np->copied_data && IS_BIG_VALUE(&(np->pair)))
			free(np->pair.data);
		
		/* XXX np is alloc from entry_alloc, free to new mempool? */
		free(np);

		/* XXX new_pool_size < bucket_num ? or a varible? */
		if ((*pool_size) < bucket_num) {
			SLIST_INSERT_HEAD(new_pool, np, mem_ent);
			(*pool_size)++;
		} else {
			free(np);
		}
	}
}
#endif

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
#ifdef	SMAP_USE_RBTREE
			RB_INIT(&(new_bp[j].root_head));
#else
			SLIST_INIT(&(new_bp[j].root_head));
#endif
		}

		/* alloc a new bucket array and free old one. */
		SMAP_WRLOCK(&(sp->seg_lock));
		old_bp = sp->bp;
		sp->bp = new_bp;
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(old_bp[j]);
			_clear(&(bp->root_head), &new_pool, &new_pool_size, sp->bucket_num);
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
#ifdef	SMAP_USE_SLIST
static inline void *
LIST_INSERT(struct SMAP_LIST *head, struct SMAP_ENT *entry)
{
	struct SMAP_ENT *np;

	SLIST_FOREACH(np, head, node) {
		if (smap_cmp(np, entry) == 0) {
			return (entry);
		}
	}
	SLIST_INSERT_HEAD(head, entry, node);
	return (NULL);
}
#endif

static int
_replace(struct SMAP_ENT *np, struct PAIR *pair)
{
	/* if we alloc memory for value, we copy it */
	if (!np->copied_data) {
		np->pair.data = pair->data;
	} else if (np->copied_data) {
		/* We must consider all sorts of situations */
		if (IS_BIG_VALUE(&(np->pair))) {
			if (IS_BIG_VALUE(pair)) {
				if (np->pair.data_len >= pair->data_len) {
					memcpy(np->pair.data, pair->data, pair->data_len);
				} else {
					void *p;
					//int nlen = pair->data_len - np->pair.data_len;
					p = realloc(np->pair.data, pair->data_len);
					if (p == NULL)
						return (SMAP_OOM);
					memcpy(((char *)p), pair->data, pair->data_len);
					np->pair.data = p;
				}
			} else {
				free(np->pair.data);
				memcpy(&(np->pair.data), pair->data, pair->data_len);
			}
		} else {
			if (IS_BIG_VALUE(pair)) {
				void *p;
				p = malloc(pair->data_len);
				if (p == NULL)
					return (SMAP_OOM);
				memcpy(p, pair->data, pair->data_len);
				np->pair.data = p;
			} else {
				memcpy(&(np->pair.data), pair->data, pair->data_len);
			}
		}
	}
	np->pair.data_len = pair->data_len;
	return (SMAP_OK);
}

int
smap_set(struct SMAP *mp, struct PAIR *pair, int replace, int copy_data)
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
	r = smap_pair_copyin(&(entry->pair), pair, copy_data);
	entry->copied_data = copy_data;
	if (r != SMAP_OK)
		return (r);

	entry->hash = h;
#ifdef	SMAP_USE_RBTREE
	rc = RB_INSERT(SMAP_TREE, &(bp->root_head), entry);
#else
	rc = LIST_INSERT(&(bp->root_head), entry);
#endif
	if (rc == NULL) {
		sp->counter++;
		bp->counter++;
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_OK);
	} else {
		if (replace)
			r = _replace(rc, pair);
		else
			r = SMAP_DUPLICATE_KEY;
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (r);
	}
}

int
smap_put(struct SMAP *mp, struct PAIR *pair, int copy_data)
{
	return (smap_set(mp, pair, 0, copy_data));
}


/*
 * it won't free the value, do it yourself.
 */

int
smap_delete(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *np, *tnp;
	struct SEGMENT *sp;
	int h;
	int r;

	if (mp == NULL || pair == NULL)
		return (SMAP_GENERAL_ERROR);

	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (SMAP_GENERAL_ERROR); 

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	entry.hash = h;
	
	SMAP_WRLOCK(&(sp->seg_lock));
	
#ifdef	SMAP_USE_RBTREE
	np = RB_FIND(SMAP_TREE, &(bp->root_head), &entry);
	if (np == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_NONEXISTENT_KEY);
	}
	RB_REMOVE(SMAP_TREE, &(bp->root_head), np);
#else
	if (!SLIST_EMPTY(&(bp->root_head))) {
		np = SLIST_FIRST(&(bp->root_head));
		if (smap_cmp(np, &entry) == 0) {
			SLIST_REMOVE_HEAD(&(bp->root_head), node);
		} else {
			SLIST_FOREACH_SAFE(np, &(bp->root_head), node, tnp) {
				if (tnp && smap_cmp(tnp, &entry) == 0) {
					SLIST_NEXT(np, node) = SLIST_NEXT(tnp, node);
					np = tnp;
					break;
				}
			}
		}
	} else {
		np = NULL;
	}
	if (np == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_NONEXISTENT_KEY);
	}

#endif
		
	sp->counter--;
	bp->counter--;
	
	if (IS_BIG_KEY(&(np->pair)))
		free(np->pair.skey);
	if (np->copied_data && IS_BIG_VALUE(&(np->pair)))
		free(np->pair.data);

	entry_free(mp, sp, np);
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
		printf("segment: %d, num: %ld\n", i, mp->seg[i].counter);
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
			printf("segment: %d, bucket: %d, num: %ld\n", i, j, mp->seg[i].bp[j].counter);
		}
	}
	return c;
}

/*
 * If copied the data, remember to free the memory.
 */

void *
smap_get(struct SMAP *mp, struct PAIR *pair, int copy_value)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SEGMENT *sp;
	struct SMAP_ENT entry;
	int h;
	int r;

	if (mp == NULL || pair == NULL)
		return (NULL);
	
	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (NULL);

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	entry.hash = h;
	
	SMAP_RDLOCK(&(sp->seg_lock));

#ifdef	SMAP_USE_RBTREE
	np = RB_FIND(SMAP_TREE, &(bp->root_head), &entry);
#else
	SLIST_FOREACH(np, &(bp->root_head), node) {
		if (smap_cmp(np, &entry) == 0) {
			break;
		}
	}
#endif
	
	if (np == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 0);
		return (NULL);
	}
	
	if (copy_value) {
		pair->data = malloc(np->pair.data_len);
		if (pair->data == NULL)
			return (NULL);
		if (IS_BIG_VALUE(&(np->pair))) {
	        memcpy(pair->data, np->pair.data, np->pair.data_len);
		} else {
			memcpy(pair->data, &(np->pair.data), np->pair.data_len);
		}
	} else {
		if (IS_BIG_VALUE(&(np->pair))) {
	        pair->data = np->pair.data;
		} else {
			pair->data = &(np->pair.data);

		}
	}
	pair->data_len = np->pair.data_len;
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	return (pair->data);

}

/*
 * it won't free the value, do it yourself.
 * XXX Maybe we need to upgrade the rdlock to wrlock
 */

int
smap_update(struct SMAP *mp, struct PAIR *pair)
{
	struct BUCKET *bp;
	struct SMAP_ENT entry;
	struct SMAP_ENT *np, *tnp;
	int h, r;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (SMAP_GENERAL_ERROR);

	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (SMAP_GENERAL_ERROR);

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	entry.hash = h;
	
	SMAP_RDLOCK(&(sp->seg_lock));

#ifdef	SMAP_USE_RBTREE
	np = RB_FIND(SMAP_TREE, &(bp->root_head), &entry);
#else
	SLIST_FOREACH_SAFE(np, &(bp->root_head), node, tnp) {
		if (smap_cmp(np, &entry) == 0) {
			break;
		}
	}
#endif
	/* if no entry */
	if (np == NULL) {
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (SMAP_NONEXISTENT_KEY);
	} else {
		r = _replace(np, pair);
		
		SMAP_UNLOCK(&(sp->seg_lock), 1);
		return (r);
	}
}

inline static int
traverse_all(struct SMAP *mp, void *rh, smap_callback *routine)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	struct PAIR pair;
	int rc;
	
#ifdef	SMAP_USE_RBTREE
	struct SMAP_TREE *root = (struct SMAP_TREE *)rh;
	RB_FOREACH_SAFE(np, SMAP_TREE, root, tnp) {
//		smap_pair_copyout(keybuf, &pair, &(np->pair));
		rc = routine(mp, &(np->pair));
		if (rc == SMAP_BREAK)
			return (SMAP_BREAK);
	}
#else
	struct SMAP_LIST *head = (struct SMAP_LIST *)rh;
	SLIST_FOREACH_SAFE(np, head, node, tnp) {
//		smap_pair_copyout(keybuf, &pair, &(np->pair));
		rc = routine(mp, &(np->pair));
		if (rc == SMAP_BREAK)
			return (SMAP_BREAK);
	}
#endif
	return (SMAP_OK);
}


inline static int
traverse_num(struct SMAP *mp, void *rh, smap_callback *routine)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	struct PAIR pair;
	int rc;
	
#ifdef	SMAP_USE_RBTREE
	struct SMAP_TREE *root = (struct SMAP_TREE *)rh;
	RB_FOREACH_SAFE(np, SMAP_TREE, root, tnp) {
		if (np->pair.type & KEYTYPE_NUM) {
//			smap_pair_copyout(keybuf, &pair, &(np->pair));
			rc = routine(mp, &(np->pair));
			if (rc == SMAP_BREAK)
				return (SMAP_BREAK);
		} else {
			return (SMAP_OK);
		}
	}
#else
	struct SMAP_LIST *head = (struct SMAP_LIST *)rh;
	SLIST_FOREACH_SAFE(np, head, node, tnp) {
		if (np->pair.type & KEYTYPE_NUM) {
//			smap_pair_copyout(keybuf, &pair, &(np->pair));
			rc = routine(mp, &(np->pair));
			if (rc == SMAP_BREAK)
				return (SMAP_BREAK);
		}
	}
#endif
	return (SMAP_OK);
}

inline static int
traverse_str(struct SMAP *mp, void *rh, smap_callback *routine)
{
	struct SMAP_ENT *np;
	struct SMAP_ENT *tnp;
	struct PAIR pair;
	int rc;
	char keybuf[SMAP_MAX_KEY_LEN+1];

#ifdef	SMAP_USE_RBTREE
	struct SMAP_TREE *root = (struct SMAP_TREE *)rh;
	RB_FOREACH_REVERSE_SAFE(np, SMAP_TREE, root, tnp) {
		if (np->pair.type == KEYTYPE_STR) {
//			smap_pair_copyout(keybuf, &pair, &(np->pair));
			rc = routine(mp, &(np->pair));
			if (rc == SMAP_BREAK)
				return (SMAP_BREAK);
		} else {
			return (SMAP_OK);
		}
	}
#else
	struct SMAP_LIST *head = (struct SMAP_LIST *)rh;
	SLIST_FOREACH_SAFE(np, head, node, tnp) {
		if (np->pair.type == KEYTYPE_STR) {
//			smap_pair_copyout(keybuf, &pair, &(np->pair));
			rc = routine(mp, &(np->pair));
			if (rc == SMAP_BREAK)
				return (SMAP_BREAK);
		}
	}
#endif
	return (SMAP_OK);
}

/*
 * it won't free the value, do it yourself.
 */
int
smap_traverse_unsafe(
	struct SMAP *mp,
	smap_callback *routine,
	unsigned long key_type,
	unsigned long start)
{
	struct BUCKET *bp;
	unsigned int i, j;
	int rc;
	struct SEGMENT *sp;
	
	if (mp == NULL || routine == NULL)
		return (SMAP_GENERAL_ERROR);
	if (start >= mp->seg_num)
		return (SMAP_GENERAL_ERROR);
		
	for (i = 0; i < mp->seg_num; i++) {
		sp = &(mp->seg[(i + start) % mp->seg_num]);
		for (j = 0; j < sp->bucket_num; j++) {
			bp = &(sp->bp[j]);
			switch (key_type) {
				case KEYTYPE_ALL:
					rc = traverse_all(mp, &(bp->root_head), routine);
					break;
				case KEYTYPE_NUM:
					rc = traverse_num(mp, &(bp->root_head), routine);
					break;
				case KEYTYPE_STR:
					rc = traverse_str(mp, &(bp->root_head), routine);
					break;
				default:
					return (SMAP_GENERAL_ERROR);
			}
			if (rc == SMAP_BREAK)
				return (SMAP_OK);
		}
	}
	return (SMAP_OK);
}

#ifdef	SMAP_USE_RBTREE

#define GET_TYPED_PAIR(key_type, rh) \
	switch (key_type) { \
		case KEYTYPE_ALL: \
		case KEYTYPE_NUM: \
			np = RB_MIN(SMAP_TREE, (struct SMAP_TREE *)(rh)); \
			break; \
		case KEYTYPE_STR: \
			np = RB_MAX(SMAP_TREE, (struct SMAP_TREE *)(rh)); \
			break; \
		default: \
			return (NULL); \
	} \
	if (np && (np->pair.type & key_type)) { \
		goto got_pair; \
	} else { \
		continue; \
	}

#else
		
#define GET_TYPED_PAIR(key_type, rh) \
	SLIST_FOREACH(np, (struct SMAP_LIST *)(rh), node) {	\
		if (np && (np->pair.type & key_type)) { \
			goto got_pair; \
		}	\
	}	\
	continue;

#endif

struct PAIR *
smap_get_first(
	struct SMAP *mp,
	struct PAIR *pair,
	char *keybuf,
	int	buf_len,
	unsigned long key_type,
	unsigned long start)
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
			GET_TYPED_PAIR(key_type, &(bp->root_head));
		}
		SMAP_UNLOCK(&(sp->seg_lock), 0);
	}
	return (NULL);
got_pair:
	smap_pair_copyout(keybuf, buf_len, pair, &(np->pair));
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	return (pair);
}

/*
 * Due to the pair may be deleted, 
 * so "the next pair" might not be "the pair's next", 
 * So we can only according to the current value of the pair
 * to find near a pair.
 */
struct PAIR *
smap_get_next(
	struct SMAP *mp,
	struct PAIR *pair,
	char *keybuf,
	int	buf_len,
	unsigned long key_type,
	unsigned long start)
{
	struct BUCKET *bp;
	struct SMAP_ENT *np;
	struct SMAP_ENT entry;
	int h, r;
	unsigned int i;
	struct SEGMENT *sp;

	if (mp == NULL || pair == NULL)
		return (NULL);

	r = smap_set_key(&(entry.pair), pair, &h);
	if (r != SMAP_OK)
		return (NULL);

	sp = get_segment(mp, h);
	bp = get_bucket(sp, h);
	entry.hash = h;
	
	SMAP_RDLOCK(&(sp->seg_lock));

	/* Get the next pair by the value of current pair */
#ifdef	SMAP_USE_RBTREE
	switch (key_type) {
		case KEYTYPE_ALL:
		case KEYTYPE_NUM:
			np = RB_NFIND(SMAP_TREE, &(bp->root_head), &entry);
			if (np && smap_cmp(np, &entry) == 0) {
				/* We got the itself, get the next one */
				np = RB_NEXT(SMAP_TREE, &(bp->root_head), np);
			}
			break;
		case KEYTYPE_STR:
			np = RB_PFIND(SMAP_TREE, &(bp->root_head), &entry);
			if (np && smap_cmp(np, &entry) == 0) {
				/* We got the itself, get the prev one */
				np = RB_PREV(SMAP_TREE, &(bp->root_head), np);
			}
			break;
		default:
			return (NULL);
	}
#else
	SLIST_FOREACH(np, &(bp->root_head), node) {
		if (smap_cmp(np, &entry) == 0) {
			while (np = SLIST_NEXT(np, node))
				if (np->pair.type & key_type)
					break;
			break;
		}
	}
#endif

	if (np && (np->pair.type & key_type)) 
		goto got_pair;
	/* 
	 * If we did not get pair, look in the segment for each bucket,
	 * until you get the next pair in a tree.
	 */
	for (bp++; (bp - sp->bp) < sp->bucket_num; bp++) {
		GET_TYPED_PAIR(key_type, &(bp->root_head));
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
				GET_TYPED_PAIR(key_type, &(bp->root_head));
			}
			SMAP_UNLOCK(&(sp->seg_lock), 0);
		}
	}
	/* UNREACHED! */
	return (NULL);
	
got_pair:
	smap_pair_copyout(keybuf, buf_len, pair, &(np->pair));
	SMAP_UNLOCK(&(sp->seg_lock), 0);
	return (pair);
}

