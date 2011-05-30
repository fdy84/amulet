#include <stdio.h>
#include <pthread.h>
#include "smap2.h"

int main()
{
	struct smap* map;
	int i, rc;

	map = smap_init(102400, 10, 4, 0);
	printf("start\n");
	for (i = 0; i < 1024000; i++) {
		rc = smap_insert(map, i, "haha", 1);
		if (rc < 0) {
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
		time_t t1, t2;
	time(&t1);
 	for (i = 0; i < 100; i++) {
		getmap(map);
	}
		time(&t2);
	
	printf("%d\n", (int)(t2-t1));
	return 0;

}

#include <time.h>


int getmap(struct smap* map)
{
	void *ret;
	int i;


	for (i = 0; i < 1024000; i++) {
		ret = smap_get(map, i);
		if (ret == NULL) {
			printf("i: %d, error!\n", i);
			break;
		}
	}

	return 0;
}
