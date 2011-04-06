inline int ip2str(const uint32_t ip, char *ipstr, socklen_t len)
{
	struct in_addr in;
	in.s_addr = ip;
	if (inet_ntop(AF_INET, &in, ipstr, len) == NULL)
	{
		return 1;
	}
	return 0;
}


inline int str2ip(const char *ipstr, uint32_t *ip)
{
	struct in_addr in;
	if (ip == NULL)
		return 1;
	if (inet_pton(AF_INET, ipstr, &in) != 1)
	{
		return 1;
	}
	*ip = in.s_addr;
	return 0;
}

/*
 *获得一个IP的Ptr记录
 * const char *ipstr: input, 点分十进制形式的ip字符串,如"127.0.0.1"
 * char *ptr, output: 字符串, ptr记录的字符串指针,长度为PTRLEN
 * return: 成功返回0,其他为失败
 */
inline int get_ptr(const uint32_t ip, char *ptr, int len)
{                                                                                                     
	struct sockaddr_in sa;
	
	sa.sin_addr.s_addr = ip;
	sa.sin_family = AF_INET;
	sa.sin_len = sizeof(sa); 
	
	if (getnameinfo((struct sockaddr *)&sa, sa.sin_len, ptr, len, NULL, 0,NI_NAMEREQD))
	{
		snprintf(ptr, len, "UNKNOWN");
		return 0;
	}
	return 0;
}