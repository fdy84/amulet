#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "smap.h"

struct timeval tvafter,tvpre;
struct timezone tz;
#define LOOP_TIMES 1000000

char buf[LOOP_TIMES][64];

int sh = 0;
int shit(struct SMAP *mp, struct PAIR *p)
{
	sh++;
/*	
	if (SMAP_IS_NUM(p))
		printf("no: %08d, key: %014d, value: %s\n", sh, p->ikey, p->data);
	else
		printf("no: %08d, key: \"%s\", value: %s\n", sh, p->skey, p->data);
*/
	return 0;
}

int
main(void)
{
	struct SMAP* map;
	struct PAIR pair;
	struct PAIR *p;
	char keybuf[SMAP_MAX_KEY_LEN + 1];
	long i;
	int rc;

//	printf("sizeof(smap):\t%d \nsizeof(pair):\t%d \nsizeof(ent):\t%d \nsizeof(seg):\t%d \nsizeof(bucket):\t%d\n",
//	 sizeof(struct SMAP), sizeof(struct PAIR), sizeof(struct SMAP_ENT), sizeof(struct SEGMENT), sizeof(struct BUCKET));

	map = smap_init(LOOP_TIMES*2,
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
			SMAP_SET_NUM_PAIR(&pair, i, buf[i], 8);
		else
			SMAP_SET_STR_PAIR(&pair, buf[i], 7, buf[i], 8);
		rc = smap_put(map, &pair, 1);
		if (rc < 0){
			printf("i: %d, error: %d\n", i, rc);
		}
	}
	gettimeofday (&tvafter , &tz);

	int ins_int_str = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;



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
	
	int get = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;
	



	gettimeofday (&tvpre , &tz);

	for (i = 0; i < LOOP_TIMES; i++) {
		SMAP_SET_NUM_KEY(&pair, 1);
		val = smap_get(map, &pair);
		if (val == NULL){
			printf("i: %d, get error\n", i);
		} else {
//			printf("%s\n", (char *)val);
		}
	}
	gettimeofday (&tvafter , &tz);
	int get_one = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;



	i = 0;
	memset(&pair, 0, sizeof(pair));
	gettimeofday (&tvpre , &tz);
	for (p = smap_get_first(map, &pair, keybuf, KEYTYPE_ALL, 0);
		p != NULL; p = smap_get_next(map, p, keybuf, KEYTYPE_ALL, 0)) {
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
	int traverse1 = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;
	

	i = 0;
	memset(&pair, 0, sizeof(pair));
	gettimeofday (&tvpre , &tz);
	for (p = smap_get_first(map, &pair, keybuf, KEYTYPE_NUM, 0);
		p != NULL; p = smap_get_next(map, p, keybuf, KEYTYPE_NUM, 0)) {
		i++;
	}
	gettimeofday (&tvafter , &tz);
	int traverse1_num = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;
	
	
	i = 0;
	memset(&pair, 0, sizeof(pair));
	gettimeofday (&tvpre , &tz);
	for (p = smap_get_first(map, &pair, keybuf, KEYTYPE_STR, 0);
		p != NULL; p = smap_get_next(map, p, keybuf, KEYTYPE_STR, 0)) {
		i++;
	}
	gettimeofday (&tvafter , &tz);
	int traverse1_str = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;
	

	gettimeofday (&tvpre , &tz);
	smap_traverse_unsafe(map, shit, KEYTYPE_ALL, 0);
	gettimeofday (&tvafter , &tz);
	int traverse2 = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;


	gettimeofday (&tvpre , &tz);
	smap_traverse_unsafe(map, shit, KEYTYPE_NUM, 0);
	gettimeofday (&tvafter , &tz);
	int traverse2_num = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;


	gettimeofday (&tvpre , &tz);
	smap_traverse_unsafe(map, shit, KEYTYPE_STR, 0);
	gettimeofday (&tvafter , &tz);
	int traverse2_str = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;


	gettimeofday (&tvpre , &tz);

	for (i = 0; i < LOOP_TIMES; i++) {
		if (i%2)
			SMAP_SET_NUM_KEY(&pair, i);
		else
			SMAP_SET_STR_KEY(&pair, buf[i], 7);
		rc = smap_delete(map, &pair);
		if (rc != SMAP_OK){
			printf("i: %d, delete error\n", i);
		} else {
//			printf("%s\n", (char *)val);
		}
	}
	gettimeofday (&tvafter , &tz);
	int delete1 = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;






//#endif
//smap_get_bucket_counter(map);
//smap_get_segment_counter(map);
	gettimeofday (&tvpre , &tz);
	smap_deinit(map);
	gettimeofday (&tvafter , &tz);
	int deinit = (tvafter.tv_sec-tvpre.tv_sec)*1000+(tvafter.tv_usec-tvpre.tv_usec)/1000;

	printf("insert int&str: %d times: %dms\n", LOOP_TIMES, ins_int_str);
	printf("get: %d times: %dms\n", LOOP_TIMES, get);
	printf("get one: %d times: %dms\n", LOOP_TIMES, get_one);
	printf("traverse 1 time: %dms %d\n", traverse1, i);
printf("traverse 1 num time: %dms %d\n", traverse1_num, i);	
printf("traverse 1 str time: %dms %d\n", traverse1_str, i);	
printf("traverse 2 time: %dms %d\n", traverse2, sh);
printf("traverse 2 num time: %dms %d\n", traverse2_num, sh);
printf("traverse 2 str time: %dms %d\n", traverse2_str, sh);
printf("delete: %d times: %dms\n", LOOP_TIMES, delete1);
	printf("deinit: %d times: %dms\n", LOOP_TIMES, deinit);



	return (0);
}

