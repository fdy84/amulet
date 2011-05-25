#include <stdio.h>
#include <pthread.h>
#include "smap2.c"

int main()
{
	struct smap* map;
	int i, rc;
	printf("start\n");
	map = smap_init(1024, 10, 0, 0);
	for (i = 0; i < 10240000; i++) {
		rc = smap_insert(map, i, "haha", 1);
		if (rc < 0) {
			printf("i: %d, error: %d\n", i, rc);
			break;
		}
	}
 	for (i = 0; i < 10; i++) {
		getmap(map);
	}
	return 0;

}

int getmap(struct smap* map)
{
	void *ret;
	int i;
	for (i = 0; i < 10240000; i++) {
		ret = smap_get(map, i);
		if (ret == NULL) {
			printf("i: %d, error!\n", i);
			break;
		}
	}	
	return 0;
}
