#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "smap2.h"

#define F_CONNECTING       1      /* connect() in progress */
#define F_READING          2      /* connect() complete; now reading */
#define F_DONE             4      /* all done */
#define F_JOINED 8 /*main has pthread_join'ed*/

#define MAXTHEADS 1



struct file
{
	int     f_fd;                 /* descriptor */
	int     f_flags;              /* F_xxx below */
	pthread_t f_tid;              /* thread ID */
}file[MAXTHEADS+2];


struct thread_args
{
	struct file *fptr;
	struct SMAP* map;
};

void stop_thr(struct file *fptr);
void *getmap(void *argv);
void *insert_map(void *argv);
void *del_map(void *argv);
int ndone; /*number of terminated threads*/
pthread_mutex_t ndone_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ndone_cond = PTHREAD_COND_INITIALIZER;
int nconn;
pthread_mutex_t wdb_mutex = PTHREAD_MUTEX_INITIALIZER;



#define LOOP_TIMES 3000000


char buf[LOOP_TIMES*2][64];

int sh = 0;

int main()
{
	struct SMAP* map;
	int i, rc, ret;
	pthread_t ntid;
	struct thread_args *thr_arg;
	struct PAIR pair;
	
	
	for (i = 0; i < LOOP_TIMES *2; i++) {
		sprintf(buf[i], "%07d", i);
	}
	map = smap_init(LOOP_TIMES*2,
		DEFAULT_LOAD_FACTOR, 128, LOOP_TIMES/100, 1);
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
	
	nconn = 0;	
	for (i = 0; i < MAXTHEADS; i++)
	{
		file[i].f_flags = 0;	
	}
		
	for (i = 0; i < MAXTHEADS; i++) {
		if ((thr_arg = (struct thread_args *)malloc(sizeof(struct thread_args))) == NULL)
		{
			perror("malloc");
			exit(1);	
		}
		thr_arg->fptr = &file[i];
		thr_arg->map = map;
		file[i].f_flags = F_CONNECTING;
		ret = pthread_create(&ntid, NULL, getmap, (void *)thr_arg);
		if (ret != 0)
		{
				perror("pthread_create");
				exit(1);
		}
		nconn++;
		file[i].f_tid = ntid;
	}
			if ((thr_arg = (struct thread_args *)malloc(sizeof(struct thread_args))) == NULL)
		{
			perror("malloc");
			exit(1);	
		}
		thr_arg->fptr = &file[i];
		thr_arg->map = map;
		file[i].f_flags = F_CONNECTING;
		ret = pthread_create(&ntid, NULL, insert_map, (void *)thr_arg);
		nconn++;




i++;

			if ((thr_arg = (struct thread_args *)malloc(sizeof(struct thread_args))) == NULL)
		{
			perror("malloc");
			exit(1);	
		}
		thr_arg->fptr = &file[i];
		thr_arg->map = map;
		file[i].f_flags = F_CONNECTING;
		ret = pthread_create(&ntid, NULL, del_map, (void *)thr_arg);
		nconn++;




		
	while (nconn != 0)
	{
		pthread_mutex_lock(&ndone_mutex);
		while(ndone == 0)
			pthread_cond_wait(&ndone_cond, &ndone_mutex);
		
		for (i = 0; i < MAXTHEADS+2; i++)
		{
			if (file[i].f_flags & F_DONE)
			{
				pthread_join(file[i].f_tid, NULL);

				//file[i].f_tid = 0;
				file[i].f_flags = 0;  /* clears F_DONE */
				ndone--;
				nconn--;
			}
		}
		pthread_mutex_unlock(&ndone_mutex);
	}
	return 0;

}


void stop_thr(struct file *fptr)
{
	pthread_mutex_lock(&ndone_mutex);
	fptr->f_flags = F_DONE;     /* clears F_READING */
	ndone++;
	pthread_cond_signal(&ndone_cond);
	pthread_mutex_unlock(&ndone_mutex);
	return;
}

void *insert_map(void *argv)
{
	struct timeval tvafter,tvpre;
	struct timezone tz;
	struct SMAP* map = ((struct thread_args *)argv)->map;
	struct file *fptr = ((struct thread_args *)argv)->fptr;
	int i, j, s,z;
	void *ret;
	int rc;
	struct PAIR pair;

	gettimeofday (&tvpre , &tz);

	for (i = LOOP_TIMES; i <  LOOP_TIMES*2; i++) {
		if (i%2)
			SMAP_SET_NUM_PAIR(&pair, i, buf[i]);
		else
			SMAP_SET_STR_PAIR(&pair, buf[i], 7, buf[i]);
		rc = smap_insert(map, &pair, 1);

	}
	gettimeofday (&tvafter , &tz);
	printf("insert: %dms\n", (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);
	stop_thr(fptr);
}

void *del_map(void *argv)
{
	struct timeval tvafter,tvpre;
	struct timezone tz;
	struct SMAP* map = ((struct thread_args *)argv)->map;
	struct file *fptr = ((struct thread_args *)argv)->fptr;
	int i, j, s,z;
	void *ret;
	int rc;
	struct PAIR pair;

	gettimeofday (&tvpre , &tz);

	for (i = 0; i < LOOP_TIMES; i++) {
		if (i%2)
			SMAP_SET_NUM_KEY(&pair, i);
		else
			SMAP_SET_STR_KEY(&pair, buf[i], 7);
		rc = smap_delete(map, &pair, 1);
		if (rc != SMAP_OK){
			printf("i: %d, delete error\n", i);
		} else {
//			printf("%s\n", (char *)val);
		}
	}
	gettimeofday (&tvafter , &tz);
	printf("del: %dms\n", (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);
	stop_thr(fptr);
}

int c = 0;
void *getmap(void *argv)
{
	struct timeval tvafter,tvpre;
	struct timezone tz;
	struct SMAP* map = ((struct thread_args *)argv)->map;
	struct file *fptr = ((struct thread_args *)argv)->fptr;
	int i, j, s,z;
	void *ret;
	struct PAIR pair;
	z = c++;
	s = (LOOP_TIMES/MAXTHEADS) * z;
	free(argv);
	
	
	
	gettimeofday (&tvpre , &tz);
	for (j = 0 ; j < 1; j++) {
	for (i = s; i < s + LOOP_TIMES/MAXTHEADS; i++) {
		if (i%2)
			SMAP_SET_NUM_KEY(&pair, i);
		else
			SMAP_SET_STR_KEY(&pair, buf[i], 7);
		ret = smap_get(map, &pair);

	}	
	}
	gettimeofday (&tvafter , &tz);
	printf("%d: %dms\n", z, (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000);
	stop_thr(fptr);
	return;
}

