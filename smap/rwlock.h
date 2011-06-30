#ifndef	_SMAP_RWLOCK_H_
#define	_SMAP_RWLOCK_H_
#include <inttypes.h>

typedef intptr_t state_t;
static const state_t WRITER = 1;
static const state_t WRITER_PENDING = 2;
static const state_t READERS = ~(1 | 2);
static const state_t ONE_READER = 4;
static const state_t BUSY = 1 | ~(1 | 2);

typedef struct spin_rw_mutex {
    //! State of lock
    /** Bit 0 = writer is holding lock
        Bit 1 = request by a writer to acquire lock (hint to readers to wait)
        Bit 2..N = number of readers holding lock */
    state_t state;
} rwlock_t;

void acquire(struct spin_rw_mutex *, int);
void release(struct spin_rw_mutex *, int);

#endif