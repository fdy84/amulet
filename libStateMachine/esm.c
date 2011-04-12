/*-
 * Copyright (c) 2009 Zhangshuo, All Rights Reserved.
 *
 * Easy State Machine(ESM).
 *
 * This is a lib used to create your own state machine.
 * Remove the main function when using. There is a sample in the main.
 * Author: Zhang shuo <nattyzs@163.com>
 * Create date: 2009-9-16
 * Last update: 2010-1-27
 */
 
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <limits.h>

#include "esm.h"
#include "utils.h"

#ifdef DEBUG
#define PRINT(fmt, arg...) \
		printf("%s[%d]: "#fmt"\n", __func__, __LINE__, ##arg)
#else
#define PRINT(fmt, arg...) do{}while(0)
#endif

ESM * 
esm_new()
{
	ESM *handle;
	
	handle = (ESM *)malloc(sizeof(ESM));
	if (handle == NULL) {
		PRINT("not enough memory.\n");
		return NULL;
	}
	
	return handle;
}

void 
esm_init(ESM *handle, ESM_STATE_CHANGE *call_back)
{
	int i;
	
	for (i = 0; i < ESM_MAX_CHARSET; i++) {
		(handle->char_set)[i].set_type = EMPTY;
	}

	handle->call_back = call_back;
}

void
esm_reset(ESM *handle)
{
	esm_init(handle, handle->call_back);
}

int 
esm_del(ESM *handle)
{
	if (handle != NULL) {
		free(handle);
		return 0;
	} else {
		return -1;
	}
}


static inline int 
esm_state_append(ESM_PFM_T *handle, int state)
{
	if (handle->nstate < ESM_STATE_HISTORY_LEN) {
		handle->state_history[handle->nstate] = state;
		handle->nstate++;
		return handle->nstate;
	} else {
		return ESM_STATE_HISTORY_LEN;
	}
}

void 
esm_debug_list_history_state(ESM_PFM_T *handle)
{
	int i;
	printf("ESM debug: There are [%d] state in history.\n", handle->nstate);
	printf("ESM debug:");
	
	for (i = 0; i < handle->nstate; i++) {
		printf("<%d> = %d | ", i, handle->state_history[i]);
	}
	printf("<EOF>\n");
	printf("ESM debug end.");
}

// 如果不去掉do{}while(0)，此处的continue将无法发挥作用
#define ESM_STATE_JUDGE(handle)	\
	if ((handle)->temp_state == _W) {	\
	esm_state_append((handle), -1);	\
		return -1;		\
	} else if ((handle)->temp_state == _I) {	\
		continue;	\
	} else {	\
		(handle)->current_state = (handle)->temp_state;		\
		(handle)->is_matched = 1;	\
	}

int  
esm_perform(ESM *handle, const unsigned char *ch, const void *udata, ESM_PFM_T *esi_temp_data)
{	
	int i;
	unsigned char *s;
	
	esi_temp_data->is_matched = 0;
	esi_temp_data->temp_state = -1;
	esi_temp_data->is_state_changed = 0;

#if defined(DEBUG)
	printf("ESM_PFM_T value(begin)(data: %d):\n", *ch);
	printf("\t.current_state = %d\n"
		   "\t.last_state = %d\n"
		   "\t.is_state_changed = %d\n"
		   "\t.is_matched = %d\n"
		   "\t.temp_state = %d\n"
		   "\t.nstate = %d\n--END--\n",
		   esi_temp_data->current_state, esi_temp_data->last_state, esi_temp_data->is_state_changed, 
		   esi_temp_data->is_matched, esi_temp_data->temp_state, esi_temp_data->nstate
		  );
#endif
	
	/* check from the first state */
	for (i = 0; i < ESM_MAX_CHARSET; i++) {
		if ((handle->char_set)[i].set_type == EMPTY) {
			break;
		} else if ((handle->char_set)[i].set_type == RANGE) {
			//printf("range!!! data: %d, range_b: %d, range_e: %d\n", *ch, (handle->char_set)[i].range_begin, (handle->char_set)[i].range_end);
			if (*ch >= (handle->char_set)[i].range_begin 
			    && *ch <= (handle->char_set)[i].range_end) {
			    esi_temp_data->temp_state = 
					(handle->state_transition_table)[esi_temp_data->current_state][i];
					
				//printf("set hit!!! s: %d, temp: %d, i: %d\n", esi_temp_data->current_state, esi_temp_data->temp_state, i);
				ESM_STATE_JUDGE(esi_temp_data)
			}
		} else if ((handle->char_set)[i].set_type == EXRANGE) {
			if (*ch < (handle->char_set)[i].range_begin 
			    || *ch > (handle->char_set)[i].range_end) {
			    
			    esi_temp_data->temp_state = 
					(handle->state_transition_table)[esi_temp_data->current_state][i];
				ESM_STATE_JUDGE(esi_temp_data)
			}
		} else if ((handle->char_set)[i].set_type == ENUM) {
			//printf("i: %d, input: %c, enum: %s\n", i, *ch, ((handle)->char_set)[i].char_enum);
		
			for (s = ((handle)->char_set)[i].char_enum; *s != '\0';	 s++) {							
				if (*s == *ch) {
					(esi_temp_data)->temp_state = 									
						((handle)->state_transition_table)[(esi_temp_data)->current_state][i];
						
					//printf("set hit!!! l: %d, temp: %d, i: %d\n", handle->current_state, handle->temp_state, i);	
					
					ESM_STATE_JUDGE(esi_temp_data)
					break;
				}	
			}
			//printf("!!!!! no hit.\n");	
		} else if ((handle->char_set)[i].set_type == EXENUM) {
			for (s = ((handle)->char_set)[i].char_enum; *s != *ch; s++) {								
				if (!(*s)) {	
					(esi_temp_data)->temp_state = 						
							((handle)->state_transition_table)[(esi_temp_data)->current_state][i];
					ESM_STATE_JUDGE(esi_temp_data)
					break;				
				}					
			}
		}
		
		if (esi_temp_data->current_state != esi_temp_data->last_state) {
			if (handle->call_back != NULL) {
				(handle->call_back)(ch, esi_temp_data->current_state, esi_temp_data->last_state, udata);
			}
			esi_temp_data->is_state_changed = 1;
			esi_temp_data->last_state = esi_temp_data->current_state;
			break;
		}

		if (esi_temp_data->is_matched)
			break;
	}
		
#if defined(STATE_HISTORY)
	esm_state_append(esi_temp_data, esi_temp_data->current_state);
#endif

#if defined(DEBUG)
	printf("ESM_PFM_T value(end):\n");
	printf("\t.current_state = %d\n"
		   "\t.last_state = %d\n"
		   "\t.is_state_changed = %d\n"
		   "\t.is_matched = %d\n"
		   "\t.temp_state = %d\n"
		   "\t.nstate = %d\n--END--\n",
		   esi_temp_data->current_state, esi_temp_data->last_state, esi_temp_data->is_state_changed, 
		   esi_temp_data->is_matched, esi_temp_data->temp_state, esi_temp_data->nstate
		  );
#endif

	return esi_temp_data->current_state;
}


int  
esm_set_range_set(ESM *handle, int set_index, unsigned char begin, unsigned char end)
{
	if (set_index < 0 && set_index >= ESM_MAX_CHARSET) {
		PRINT("state over limit.\n");
		return -1;
	}
	(handle->char_set)[set_index].set_type = RANGE;
	(handle->char_set)[set_index].range_begin = begin;
	(handle->char_set)[set_index].range_end = end;
	return 0;
}

int  
esm_set_exclude_range_set(ESM *handle, int set_index, unsigned char begin, unsigned char end)
{
	if (set_index < 0 && set_index >= ESM_MAX_CHARSET) {
		PRINT("state over limit.\n");
		return -1;
	}
	(handle->char_set)[set_index].set_type = EXRANGE;
	(handle->char_set)[set_index].range_begin = begin;
	(handle->char_set)[set_index].range_end = end;
	return 0;
}

int 
esm_set_enum_set(ESM *handle, int set_index, const unsigned char *char_enum)
{
	if (set_index < 0 && set_index >= ESM_MAX_CHARSET) {
		PRINT("state over limit.\n");
		return -1;
	}
	(handle->char_set)[set_index].set_type = ENUM;
	strlcpy((handle->char_set)[set_index].char_enum, char_enum, ESM_MAX_SET_LEN);
	return 0;
}

int 
esm_set_exclude_enum_set(ESM *handle, int set_index, const unsigned char *char_enum)
{
	if (set_index < 0 && set_index >= ESM_MAX_CHARSET) {
		PRINT("state over limit.\n");
		return -1;
	}
	(handle->char_set)[set_index].set_type = EXENUM;
	strlcpy((handle->char_set)[set_index].char_enum, char_enum, ESM_MAX_SET_LEN);
	return 0;
}

int 
esm_set_state(ESM *handle, int state_index, const int *states, int states_count)
{
	int i;
	
	if (state_index < 0 || state_index >= ESM_MAX_STATES) {
		PRINT("state over limit.\n");
		return -1;
	}
	
	for (i = 0; i < ESM_MAX_CHARSET && i < states_count; i++) {
		handle->state_transition_table[state_index][i] = states[i];
	}
	
	if (i < ESM_MAX_CHARSET) {
		handle->state_transition_table[state_index][i] = _W;
	}
	
	return 0;
}



#ifdef no_use

/* ==================================================================
 * Main function starts. You should remove this when using as a lib.
 * ==================================================================*/

#define ____	-1

#define CMDL	0
#define TEXT	1
#define CR_1	2
#define CR_2	3
#define CR_3	4
#define CR_4	5
#define DATA	6
#define SP_1	7
#define SP_2	8
#define RURL	9

#define SET_CHAR_TXT	0
#define SET_CHAR_CR		1
#define SET_CHAR_LF		2
#define SET_CHAR_SP		3

#define NSET	4


#define NOUSE(arg)	((void)arg)

#include <time.h>

int 
main(int argc, char **argv)
{
	ESM esm;
	unsigned int i;
	
	NOUSE(argc);
	NOUSE(argv);
	
	esm_init(&esm, NULL);
	
	esm_set_exclude_enum_set(&esm, SET_CHAR_TXT, "\r\n ");
	esm_set_enum_set(&esm, SET_CHAR_CR, "\r");
	esm_set_enum_set(&esm, SET_CHAR_LF, "\n");
	esm_set_enum_set(&esm, SET_CHAR_SP, " ");

	/* create your states 
                                      TXT    CR    LF    SP   */
/* cmd line */	int S_CMDL[NSET] = { CMDL, CR_1, ____, SP_1 };
/* text */		int S_TEXT[NSET] = { TEXT, CR_1, TEXT, TEXT };
/* \r */		int S_CR_1[NSET] = { TEXT, TEXT, CR_2, TEXT };
/* \r\n */		int S_CR_2[NSET] = { TEXT, CR_3, TEXT, TEXT };
/* \r\n\r */	int S_CR_3[NSET] = { TEXT, TEXT, CR_4, TEXT };
/* \r\n\r\n */	int S_CR_4[NSET] = { DATA, DATA, DATA, DATA };
/* data */		int S_DATA[NSET] = { DATA, DATA, DATA, DATA };
/* SP1 */		int S_SP_1[NSET] = { RURL, ____, ____, RURL };
/* URL */		int S_RURL[NSET] = { RURL, ____, ____, SP_2 };
/* SP2 */		int S_SP_2[NSET] = { CMDL, ____, ____, CMDL };

	
	/* insert the states into ESM in order */
	esm_set_state(&esm, CMDL, S_CMDL, NSET);
	esm_set_state(&esm, TEXT, S_TEXT, NSET);
	esm_set_state(&esm, CR_1, S_CR_1, NSET);
	esm_set_state(&esm, CR_2, S_CR_2, NSET);
	esm_set_state(&esm, CR_3, S_CR_3, NSET);
	esm_set_state(&esm, CR_4, S_CR_4, NSET);
	esm_set_state(&esm, DATA, S_DATA, NSET);
	esm_set_state(&esm, SP_1, S_SP_1, NSET);
	esm_set_state(&esm, SP_2, S_SP_2, NSET);
	esm_set_state(&esm, RURL, S_RURL, NSET);
	
	unsigned char test[] = "GET /index.html HTTP/1.1\r\n"
							 "Connection: Keep-Alive\r\n"
							 "HOST: www.163.com\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "Content-Length: 14\r\n"
							 "\r\n"
							 "DATA...DATA...";
	
	printf("%s\n\n",  test);
	
	int state;
	unsigned int len = strlen(test);
	printf("data len: %u\n", len);
	

	time_t start_t = time(NULL);
	
	int j;
	for (j=0; j<500000;j++) {
	
	for (i = 0; i < len; i++) {
		esm_macro_perform(&esm, &(test[i]), NULL);
		state = esm_get_state(&esm);
		//state = esm_perform(&esm, &(test[i]), NULL);
		
		//PRINT("cur char: %c, state: %d\n", test[i], state);
		
		switch (state) {
			case RURL :
				//if (esm_is_state_changed(&esm))
				//	printf("\n-------REQ URL-------\n");
				//printf("%c", test[i]);
				break;
			case SP_1 :
				//PRINT("url is coming.");
				break;
			case SP_2 :
				//PRINT("url ends.");
				break;
			case TEXT :
				//if (esm_is_state_changed(&esm))
				//	printf("\n-------HTTP FIELD-------\n");
				//printf("%c", test[i]);
				break;
			case DATA :
				//if (esm_is_state_changed(&esm))
				//	printf("\n-------DATA start-------\n");
				//printf("%c", test[i]);
				break;
			case CR_4 :
				//if (esm_is_state_changed(&esm))
				//	printf("\n------HTTP header end-------\n");
				esm_state_reset(&esm);
				break;
		}
	}

	}

	time_t end_t = time(NULL);
	printf("time elapse: %ju\n", (uintmax_t)(end_t-start_t));
	
	//esm_del(esm);

	return 0;
}

#endif
