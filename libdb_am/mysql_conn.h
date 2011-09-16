/*-
 * Copyright (c) 2007-2009 SINA Corporation, All Rights Reserved.
 * Author: Zhang shuo <zhangshuo@staff.sina.com.cn>
 * Date: Feb 24th, 2009
 *
 * This is a MYSQL connection handle implementation.
 * You can create, destroy and re-connect an invalid MYSQL connection
 */

#ifndef _MYSQL_CONN_H
#define _MYSQL_CONN_H

#include <mysql/mysql.h>



#include <stdint.h>
#include <stdlib.h>


/* debug */
#define MYSQL_DEBUG


#define MYSQL_CONN_NAME_LEN		128
#define MYSQL_CONN_TIMEOUT		3
#define MYSQL_CHARSET_NAME		"utf8"	


#define MS_CONN_RETRY			0x1



extern size_t escape_quotes_for_mysql(struct charset_info_st *charset_info,
                                      char *to, size_t to_length,
                                      const char *from, size_t length);

extern size_t escape_string_for_mysql(struct charset_info_st *charset_info,
                                      char *to, size_t to_length,
                                      const char *from, size_t length);




typedef struct _mysql_handle
{
    MYSQL *msp;
    char db_host[MYSQL_CONN_NAME_LEN + 1];
    char db_user[MYSQL_CONN_NAME_LEN + 1];
    char db_pass[MYSQL_CONN_NAME_LEN + 1];
    char db_name[MYSQL_CONN_NAME_LEN + 1];
    unsigned int db_port;
} MYSQL_CONN;
    
  
MYSQL_CONN *
__mysql_init(const char *db_host, const char *db_user, const char *db_pass, 
		   const char *db_name, unsigned int db_port);  

unsigned long 
__mysql_real_escape_string(MYSQL_CONN *handle, char *to, const char *from, 
						   unsigned long to_len, unsigned long from_len);

int
__mysql_query(MYSQL_CONN *handle, const char *sql_str, int sql_len, int options);

MYSQL_RES *__mysql_store_result(MYSQL_CONN *handle, int options);
int __mysql_options(MYSQL_CONN *handle, int option, const void *arg);
int __mysql_set_server_option(MYSQL_CONN *handle, enum enum_mysql_set_option option);
int __mysql_conn(MYSQL_CONN *handle);
int __mysql_destroy(MYSQL_CONN *handle);


#define mysql_get_handle(handle)		(handle->msp)
#define mysql_get_host(handle)			(handle->db_host)
#define mysql_get_user(handle)			(handle->db_user)
#define mysql_get_pass(handle)			(handle->db_pass)
#define mysql_get_name(handle)			(handle->db_name)
#define mysql_get_port(handle)			(handle->db_port)



#endif


