#ifndef _ESM_H_
#define _ESM_H_

#include <stdint.h>

/* compile configure */
#define STATE_HISTORY

#ifdef DEBUG
#undef DEBUG
#endif
/* end compile configure */

#define ESM_MAX_CHARSET			64
#define ESM_MAX_STATES			64
#define ESM_MAX_SET_LEN			256
#define ESM_STATE_HISTORY_LEN	1000

#define _W					-1	/* wrong state, statemachine shall stop */
#define _I					-2	/* if state is _I, ignore and check next set */

/* Data structure */
enum esm_set_type {
	EMPTY,
	RANGE,
	EXRANGE,
	ENUM,
	EXENUM,
};

typedef struct esm_char_set {
	enum esm_set_type set_type;
	
	/* range */
	unsigned char range_begin;
	unsigned char range_end;
	
	/* set enumerate */
	unsigned char char_enum[ESM_MAX_SET_LEN];
} ESM_SET;

typedef void ESM_STATE_CHANGE(const unsigned char *, int, int, const void *);

typedef struct esm_perform_temp {
	int current_state;
	int last_state;
	int is_state_changed;
	int is_matched;
	int temp_state;
	int state_history[ESM_STATE_HISTORY_LEN];
	int nstate;
		
	/* loop variable used for esm_perform macro */
	int i;
	unsigned char *s;
	
} ESM_PFM_T;


typedef struct esm_handle {
	ESM_SET char_set[ESM_MAX_CHARSET];
	int state_transition_table[ESM_MAX_STATES][ESM_MAX_CHARSET];
	ESM_STATE_CHANGE *call_back;
	//const void *arg;
} ESM;
	

/* Macros */
// usage: ESM_PFM_T temp = esm_new_temp_data();
#define esm_new_temp_data(noarg)	{	\
		.current_state = 0,	\
		.last_state = 0,	\
		.is_state_changed = 0,	\
		.is_matched = 0,	\
		.temp_state = 0,	\
		.nstate = 0,	\
	}

#define esm_state_reset(handle)		\
	do{								\
		(handle)->current_state = 0;	\
		(handle)->last_state = 0;	\
		(handle)->is_state_changed = 0;	\
		(handle)->is_matched = 0;	\
		(handle)->temp_state = 0;	\
		(handle)->nstate = 0;	\
	}while(0)
	
#define esm_state_goto(handle, state)	\
	do{								\
		(handle)->last_state = (handle)->current_state;	\
		(handle)->current_state = state;	\
	}while(0)

#define esm_is_state_changed(handle)	(handle)->is_state_changed
#define esm_get_state(handle)			((handle)->current_state)


// DO NOT USE THIS MACRO(bugs in it). USE FUNCTION PLEASE.
#define esm_macro_perform(handle, ch, udata)	\
	do{									\
		(handle)->is_state_changed = 0;		\
		(handle)->is_matched = 0;				\
											\
		for ((handle)->i = 0; (handle)->i < ESM_MAX_CHARSET; (handle)->i++) {		\
			if (((handle)->char_set)[(handle)->i].set_type == EMPTY) {		\
				break;														\
			} else if (((handle)->char_set)[(handle)->i].set_type == RANGE) {		\
				if (*ch >= ((handle)->char_set)[(handle)->i].range_begin 			\
					&& *ch <= ((handle)->char_set)[(handle)->i].range_end) {	\
					(handle)->current_state = 									\
						((handle)->state_transition_table)[(handle)->current_state][(handle)->i];		\
					(handle)->is_matched = 1;	\
				}								\
			} else if (((handle)->char_set)[(handle)->i].set_type == EXRANGE) {		\
				if (*ch < ((handle)->char_set)[(handle)->i].range_begin 				\
					|| *ch > ((handle)->char_set)[(handle)->i].range_end) {				\
					(handle)->current_state = 										\
						((handle)->state_transition_table)[(handle)->current_state][(handle)->i];		\
					(handle)->is_matched = 1;	\
				}								\
			} else if (((handle)->char_set)[(handle)->i].set_type == ENUM) {		\
				for ((handle)->s = ((handle)->char_set)[(handle)->i].char_enum; 	\
					 *((handle)->s) != '\0';	\
					((handle)->s)++) 			\
				{								\
					if (*((handle)->s) == *ch) {	\
						(handle)->current_state = 									\
							((handle)->state_transition_table)[(handle)->current_state][(handle)->i];	\
						(handle)->is_matched = 1;	\
						break;	\
					}	\
				}		\
						\
			} else if (((handle)->char_set)[(handle)->i].set_type == EXENUM) {		\
				for ((handle)->s = ((handle)->char_set)[(handle)->i].char_enum; 	\
					*((handle)->s) != *ch;	\
					((handle)->s)++) 			\
				{								\
					if (!(*((handle)->s))) {	\
						(handle)->current_state = 						\
								((handle)->state_transition_table)[(handle)->current_state][(handle)->i];		\
						(handle)->is_matched = 1;	\
						break;				\
					}					\
				}		\
			}						\
									\
			if ((handle)->current_state != (handle)->last_state) {		\
				if ((handle)->call_back != NULL) {					\
					((handle)->call_back)(ch, (handle)->current_state, (handle)->last_state, udata);	\
				}									\
				(handle)->is_state_changed = 1;		\
				(handle)->last_state = (handle)->current_state;		\
				break;		\
			}		\
			if ((handle)->is_matched)break;	\
		}		\
	}while(0);


/* declaration */
ESM *esm_new(void);
void esm_init(ESM *, ESM_STATE_CHANGE *);
int esm_del(ESM *);
void esm_reset(ESM *);
int esm_perform(ESM *, const unsigned char *, const void *, ESM_PFM_T *);
int esm_set_range_set(ESM *, int, unsigned char, unsigned char);
int esm_set_exclude_range_set(ESM *, int, unsigned char, unsigned char);
int esm_set_enum_set(ESM *, int, const unsigned char *);
int esm_set_exclude_enum_set(ESM *, int, const unsigned char *);
int esm_set_state(ESM *, int, const int *, int);

/* debug functions */
void esm_debug_list_history_state(ESM_PFM_T *handle);

#endif

