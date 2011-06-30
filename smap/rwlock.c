/* just for x64 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "atomic.h"
#include "rwlock.h"


static inline void _pause( int32_t delay ) {
	int32_t i;
    for (i = 0; i < delay; i++) {
       __asm__ __volatile__("pause;");
    }
    return;
}

static inline void _or( volatile void *ptr, uint64_t addend ) {
    __asm__ __volatile__("lock\norq %1,%0" : "=m"(*(volatile uint64_t*)ptr) : "r"(addend), "m"(*(volatile uint64_t*)ptr) : "memory");
}
static inline void _and( volatile void *ptr, uint64_t addend ) {
    __asm__ __volatile__("lock\nandq %1,%0" : "=m"(*(volatile uint64_t*)ptr) : "r"(addend), "m"(*(volatile uint64_t*)ptr) : "memory");
}

static inline int64_t _fetchadd8(volatile void *ptr, int64_t addend){
    int64_t result;                                                                        \
    __asm__ __volatile__("lock\nxadd" "q" " %0,%1"                                     \
                          : "=r"(result),"=m"(*(volatile int64_t*)ptr)                     \
                          : "0"(addend), "m"(*(volatile int64_t*)ptr)                      \
                          : "memory");                                               \
    return result;
}

void
pause_times(int32_t *count) 
{
        if( *count<=16 ) {
            _pause(*count);
            // Pause twice as long the next time.
            *count*=2;
        } else {
            // Pause is so long that we might as well yield CPU to scheduler.
            sched_yield();
        }
}

static inline int64_t
_cmpswp8 (volatile void *ptr, int64_t value, int64_t comparand )
{
    int64_t result;
    __asm__ __volatile__("lock\ncmpxchg" "q" " %2,%1"
                          : "=a"(result), "=m"(*(volatile int64_t*)ptr)
                          : "q"(value), "0"(comparand), "m"(*(volatile int64_t*)ptr)
                          : "memory");
    return result;
}

static inline uint64_t
CAS(volatile uint64_t *addr, uint64_t newv, uint64_t oldv) {
 return (uint64_t)_cmpswp8((volatile void *)addr, (intptr_t)newv, (intptr_t)oldv);
}



//! Acquire write lock on the given mutex.
int
internal_acquire_writer(struct spin_rw_mutex *m)
{

    int32_t backoff = 1;
    for(;;) {
        volatile state_t s = (volatile state_t )(m->state); // ensure reloading
        if( !(s & BUSY) ) { // no readers, no writers
            if(CAS(&(m->state), WRITER, s) == s)
                break; // successfully stored writer flag
            backoff = 1; // we could be very close to complete op.
        } else if( !(s & WRITER_PENDING) ) { // no pending writers
            _or(&(m->state), WRITER_PENDING);
        }
        pause_times(&backoff);
    }

    return 0;
}

//! Release writer lock on the given mutex
void
internal_release_writer(struct spin_rw_mutex *m)
{
    _and( &m->state, READERS );
}

//! Acquire read lock on given mutex.
void
internal_acquire_reader(struct spin_rw_mutex *m) 
{
    int32_t backoff = 1;
    for(;;) {
        volatile state_t s = (volatile state_t )(m->state);// ensure reloading
        if( !(s & (WRITER|WRITER_PENDING)) ) { // no writer or write requests
            state_t t = (state_t)_fetchadd8( &(m->state), (intptr_t) ONE_READER );
            if(!( t&WRITER )) 
                break; // successfully stored increased number of readers
            // writer got there first, undo the increment
            _fetchadd8( &(m->state), -(intptr_t)ONE_READER );
        }
        pause_times(&backoff);
    }
}

void
internal_release_reader(struct spin_rw_mutex *m)
{
    _fetchadd8( &(m->state),-(intptr_t)ONE_READER);
}

//! Acquire lock on given mutex.
void
acquire(struct spin_rw_mutex *m, int write ) {
    if (write) internal_acquire_writer(m);
    else        internal_acquire_reader(m);
}
//! Release lock.
void
release(struct spin_rw_mutex *m, int write) {
    if (write) internal_release_writer(m);
    else            internal_release_reader(m);
}

/*
struct spin_rw_mutex m;
void *
test1111111111(void *argv)
{

	int i, j, k;
	for (i = 0; i < 1; i++) {

		acquire(&m, 0);
		for (k = 0; k < 1000; k++) {
			for (j = 0; j < 1000000; j++);
			printf("\t%s%s%s\n", __func__, __func__, __func__);
		}
		release(&m, 0);
	}
	
	return;
}
void *
test2222222222(void *argv)
{

	int i, j, k;
	for (i = 0; i < 1; i++) {
		
		acquire(&m, 0);
		for (k = 0; k < 1000; k++){
			printf("\t%s%s%s\n", __func__, __func__, __func__);
			for (j = 0; j < 1000000; j++);
		}
		release(&m, 0);
	}
	
	
	
	return;
}

void *
testw(void *argv)
{

	int i, j, k;
	for (i = 0; i < 100; i++) {
		
		acquire(&m, 1);
		for (k = 0; k < 100; k++){
			for (j = 0; j < 1000000; j++);
			printf("\t\t\t\t\t\t\t\t%s%s%s\n", __func__, __func__, __func__);
		}
		release(&m, 1);
	}
	
	
	return;
}

int
main()
{
	struct smap* map;
	int i, rc, ret;
	pthread_t ntid;
	struct thread_args *thr_arg;
	
	m.state = 0;

   ret = pthread_create(&ntid, NULL, testw, (void *)thr_arg);
	ret = pthread_create(&ntid, NULL, test1111111111, (void *)thr_arg);
	ret = pthread_create(&ntid, NULL, test2222222222, (void *)thr_arg);


	acquire(&m, 0);
	for (i = 0; i < 1100; i++)
		printf("\t%s%s%s\n", __func__, __func__, __func__);
	release(&m, 0);

	acquire(&m, 1);
	acquire(&m, 0);
	for (i = 0; i < 1100; i++)
		printf("\t%s%s%s\n", __func__, __func__, __func__);
	release(&m, 0);
	release(&m, 1);
	getch();
	return 0;
}
*/
