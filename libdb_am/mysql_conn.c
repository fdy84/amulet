/*-
 * Copyright (c) 2007-2009 SINA Corporation, All Rights Reserved.
 * Author: Zhang shuo <zhangshuo@staff.sina.com.cn>
 * Date: Feb 24th, 2009
 *
 * This is a MYSQL connection handle implementation.
 * You can create, destroy and re-connect an invalid MYSQL connection
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>

#include "mysql_conn.h"
#include "log.h"

typedef void CSF_WRITE_LOG(int, const char *, ...);
extern CSF_WRITE_LOG *write_log;


int
__mysql_conn(MYSQL_CONN *handle)
{
	MYSQL *retmysql;
	MYSQL *msp;
	int retval;
	uint32_t timeout = MYSQL_CONN_TIMEOUT;
	
	if (handle == NULL)
		return -1;
		
	/* init mysql lib */
	msp = mysql_init(NULL);
	
	if (msp == NULL)
		return -1;
		
	retval = mysql_options(msp, MYSQL_OPT_CONNECT_TIMEOUT, (const char *)(&timeout));

	if (retval != 0) {
		#ifdef MYSQL_DEBUG
		WLOG_ERR("Failed to set option: %s", mysql_error(msp));
		#endif
		
		mysql_close(msp);
		return -1;
	}
	
	retval = mysql_options(msp, MYSQL_SET_CHARSET_NAME, MYSQL_CHARSET_NAME); 
	
	if (retval != 0) {
		#ifdef MYSQL_DEBUG
		WLOG_ERR("Failed to set option: %s", mysql_error(msp));
		#endif
		
		mysql_close(msp);
		return -1;
	}
	
	handle->msp = msp;
	
	retmysql = mysql_real_connect(
		handle->msp, 
		handle->db_host, 
		handle->db_user,      
		handle->db_pass, 
		handle->db_name, 
		handle->db_port,
		NULL, CLIENT_INTERACTIVE | CLIENT_MULTI_STATEMENTS);
		
	if (retmysql == NULL) {
		#ifdef MYSQL_DEBUG
		WLOG_ERR("Failed to connect: %s\n", mysql_error(handle->msp));
		#endif 
		
		mysql_close(handle->msp);
		handle->msp = NULL;
		return -2;
	}
	
	if (mysql_set_character_set(msp, "utf8") == 0) {
		WLOG_DEBUG("New client character set: %s\n", mysql_character_set_name(msp));
	}
	else {
		WLOG_ERR("Character set: %s failed.\n", mysql_character_set_name(msp));
	}
	
	mysql_autocommit(handle->msp, 1);

	return 0;
}


MYSQL_CONN *
__mysql_init(const char *db_host, const char *db_user, const char *db_pass, 
		   const char *db_name, unsigned int db_port)
{
	MYSQL_CONN *handle;
	
	if (db_host == NULL || db_user == NULL || db_pass == NULL || 
	    db_name == NULL) 
	{
		return NULL;
	}
	
	/* create a mysql conn handle */
	handle = (MYSQL_CONN *)calloc(1, sizeof(MYSQL_CONN));
	
	/* save the mysql information */
	strncpy(handle->db_host, db_host, MYSQL_CONN_NAME_LEN);
	strncpy(handle->db_user, db_user, MYSQL_CONN_NAME_LEN);
	strncpy(handle->db_pass, db_pass, MYSQL_CONN_NAME_LEN);
	strncpy(handle->db_name, db_name, MYSQL_CONN_NAME_LEN);
	handle->db_port = db_port;
	
	
	/* real connect to mysql server */
	if (__mysql_conn(handle) != 0) {
		return NULL;
	}
	
	return handle;
}


int
__mysql_query(MYSQL_CONN *handle, const char *sql_str, int sql_len, int options)
{
	int retval = 0;
	int	err;
	int	retry_flag = 0;
	
	if (handle == NULL || sql_str == NULL)
		return -1;
		
	if (handle->msp == NULL) {
		if (__mysql_conn(handle) != 0) {
			#ifdef MYSQL_DEBUG
			WLOG_WARNING("__mysql_store_result: MySQL can not be reconnected!.\n");
			#endif
			return -1;
		}
	}

		
	for( ; ; ) {
		retval = mysql_real_query(mysql_get_handle(handle), sql_str, sql_len);

		if (retval != 0) {
			WLOG_ERR("mysql query error:[%d]:[%s]\n", retval, mysql_error(mysql_get_handle(handle)));
			err = mysql_errno(mysql_get_handle(handle));
			
            /* syntax error */
            if (err >= ER_ERROR_FIRST && err <= ER_ERROR_LAST)
			{
				if (err == ER_SYNTAX_ERROR)
				{
					WLOG_ERR("SQL syntax error.\n");
					
					do{}while(0);
				}
				return err;	
			}
			
            /* connection error */
            else if (err == CR_SERVER_GONE_ERROR || err == CR_SERVER_LOST) 
            {		
				if ((options & MS_CONN_RETRY) != MS_CONN_RETRY)
					retry_flag = 1;
				
				/* Disconnected again, never retry */
				if (retry_flag) break;

                retry_flag = 1;
				WLOG_INFO("Try to reconnect to mysql ...\n");
				
				/* close for a new connection */
				mysql_close(handle->msp);

                if (__mysql_conn(handle) != 0) {
					WLOG_EMERG("Fatal error: MySQL can not be reconnected!.\n");
					handle->msp = NULL;
					
					return -1;
                }
                else {
					continue;
                }

			} 
            
            /* other fatal error */
            else {
				WLOG_EMERG("Mysql Unknow error. mysql failed.\n");
				return -1;
			}

		} 
        else {
			break;
		}
	}

	return 0;
}


MYSQL_RES *
__mysql_store_result(MYSQL_CONN *handle, int options)
{
	MYSQL_RES *result = NULL;
	int	err;
	int	retry_flag = 0;
	
	if (handle == NULL)
		return NULL;
		
	if (handle->msp == NULL) {
		if (__mysql_conn(handle) != 0) {
			#ifdef MYSQL_DEBUG
			WLOG_EMERG("__mysql_store_result: MySQL can not be reconnected!.\n");
			#endif
			return NULL;
		}
	}
	
	
	for( ;handle->msp != NULL ;) {
		result = mysql_store_result(mysql_get_handle(handle));

		if (result == NULL) {
			err = mysql_errno(mysql_get_handle(handle));
			
            /* syntax error */
            if (err >= ER_ERROR_FIRST && err <= ER_ERROR_LAST) {
				break;	
			} 
            
            /* connection error */
            else if (err == CR_SERVER_GONE_ERROR || err == CR_SERVER_LOST)
            {
				if ((options & MS_CONN_RETRY) != MS_CONN_RETRY)
					retry_flag = 1;

				/* Disconnected again, never retry */
				if (retry_flag)
					break;

				/* Reconnect it again */
                retry_flag = 1;
                mysql_close(handle->msp);

                if (__mysql_conn(handle) != 0) {
					#ifdef MYSQL_DEBUG
					WLOG_EMERG("Fatal error: MySQL can not be reconnected!.\n");
					#endif
					handle->msp = NULL;
					return NULL;
                }
                else {
					continue;
                }

			} 
            
            /* other fatal error */
            else {
				#ifdef MYSQL_DEBUG
				if (err)
					WLOG_EMERG("Mysql Unknow error. mysql failed.\n");
				#endif
				
				return NULL;
			}
		} 
        else {
			break;
		}
	}
	
    return result;
}


int
__mysql_options(MYSQL_CONN *handle, int option, const void *arg)
{
	if (handle == NULL || arg == NULL)
		return -1;
		
	if (handle->msp == NULL) {
		/* reconnect mysql server */
		if (__mysql_conn(handle) != 0) {
			#ifdef MYSQL_DEBUG
			WLOG_EMERG("__mysql_options: MySQL can not be reconnected!.\n");
			#endif
			return -2;
		}
	}
	
	if (handle->msp != NULL) {
		mysql_options(handle->msp, option, arg);
		return 0;
	}

	return -2;
}


int
__mysql_set_server_option(MYSQL_CONN *handle, enum enum_mysql_set_option option)
{
	if (handle == NULL)
		return -1;
		
	if (handle->msp == NULL) {
		/* reconnect mysql server */
		if (__mysql_conn(handle) != 0) {
			#ifdef MYSQL_DEBUG
			WLOG_EMERG("__mysql_options: MySQL can not be reconnected!.\n");
			#endif
			return -2;
		}
	}
	
	if (handle->msp != NULL) {
		mysql_set_server_option(handle->msp, option);
		return 0;
	}

	return -2;
}




unsigned long 
__mysql_real_escape_string(MYSQL_CONN *handle, char *to, const char *from, 
						   unsigned long to_len, unsigned long from_len)
{
	if (handle->msp->server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES)
		return escape_quotes_for_mysql(handle->msp->charset, to, to_len, from, from_len);
		
	return escape_string_for_mysql(handle->msp->charset, to, to_len, from, from_len);
}



int
__mysql_destroy(MYSQL_CONN *handle)
{
	if (handle == NULL)
		return -1;
		
	mysql_close(handle->msp);
	free(handle);
	
	return 0;
}







