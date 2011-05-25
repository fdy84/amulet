#include <stdio.h>
#include <pthread.h>
#include "smap2.c"


#define F_CONNECTING       1      /* connect() in progress */
#define F_READING          2      /* connect() complete; now reading */
#define F_DONE             4      /* all done */
#define F_JOINED 8 /*main has pthread_join'ed*/

#define MAXTHEADS 4



struct file
{
	int     f_fd;                 /* descriptor */
	int     f_flags;              /* F_xxx below */
	pthread_t f_tid;              /* thread ID */
}file[MAXTHEADS];


struct thread_args
{
	struct file *fptr;
	struct smap* map;
};

void stop_thr(struct file *fptr);
void *getmap(void *argv);
int ndone; /*number of terminated threads*/
pthread_mutex_t ndone_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ndone_cond = PTHREAD_COND_INITIALIZER;
int nconn;
pthread_mutex_t wdb_mutex = PTHREAD_MUTEX_INITIALIZER;


int main()
{
	struct smap* map;
	int i, rc, ret;
	pthread_t ntid;
	struct thread_args *thr_arg;
	map = smap_init(1024, 10, 0, 0);
	for (i = 0; i < 10240000; i++) {
		rc = smap_insert(map, i, "haha", 1);
		if (rc < 0) {
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
	
	nconn = 0;	
	for (i = 0; i < MAXTHEADS; i++)
	{
		file[i].f_flags = 0;	
	}
		
	for (i = 0; i < 4; i++) {
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
	while (nconn != 0)
	{
		pthread_mutex_lock(&ndone_mutex);
		while(ndone == 0)
			pthread_cond_wait(&ndone_cond, &ndone_mutex);
		
		for (i = 0; i < MAXTHEADS; i++)
		{
			if (file[i].f_flags & F_DONE)
			{
				pthread_join(file[i].f_tid, NULL);

				file[i].f_tid = 0;
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


void *getmap(void *argv)
{
	struct smap* map = ((struct thread_args *)argv)->map;
	struct file *fptr = ((struct thread_args *)argv)->fptr;
	int i, j;
	void *ret;
	free(argv);
	for (j = 0 ; j < 10; j++) {
	for (i = 0; i < 10240000; i++) {
		ret = smap_get(map, i);
		if (ret == NULL) {
			printf("i: %d, error!\n", i);
			break;
		}
	}	
	}
	stop_thr(fptr);
	return;
}

