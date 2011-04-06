/*
 *查询一个ip是一个RBL中的值
 * uint32_t ip, input: 字符串, 需要查询的ip,点分十进制形式的ip字符串,如"127.0.0.1",长度为INET_ADDRSTRLEN
 * const char *base, input: 字符串, rblbase的字符串指针,
 * int rblbase_len, input: 长度为RBLBASELEN 
 * uint32_t *rblret, output:
 * return: 成功返回0,其他为失败
 */
inline int get_rbl(uint32_t ip, const char *rblbase, int rblbase_len, uint32_t *rblret)
{
	uint32_t r_ip;
	char r_ipstr[INET_ADDRSTRLEN];
	struct addrinfo hints, *res = NULL;
	char check_name[INET_ADDRSTRLEN+rblbase_len+2]; 
	
	if (rblret == NULL)
		return 1;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET;
	
	//将ip反转
	r_ip = ntohl(ip);
	if(ip2str(r_ip, r_ipstr, sizeof(r_ipstr)))
	{ 
		return 1;
	}
	snprintf(check_name,INET_ADDRSTRLEN+rblbase_len+2,"%s.%s", r_ipstr, rblbase);
	if (getaddrinfo(check_name, "domain", &hints, &res))
	{
		*rblret = 0;
		if (res != NULL)
			freeaddrinfo(res);	
		return 0;
	}
	*rblret = (((struct sockaddr_in *)(res->ai_addr))->sin_addr).s_addr;
	freeaddrinfo(res);
	return 0;	
}


/*
 * 取src中的尖括号中的邮件地址，并替换原src
 */
int parse_from(char *src)
{
    char *pBeg = NULL;
    char *pEnd = NULL;
    char *p = NULL;
    int iLen = 0;
    int i = 0;
    char tmp[256] = {0};

    iLen = strlen(src);
    p = &src[iLen - 1];

    for (i=0; i<iLen; i++)
    {
        if ((*p == '<') && (pBeg==NULL))
            pBeg = p;
        else if ((*p == '>') && (pEnd==NULL))
            pEnd = p;
        p--;
    }

    if (pBeg && pEnd)
    {
        strncpy(tmp, pBeg+1, pEnd-pBeg-1);
        memset(src, 0x00, sizeof(src));
        strlcpy(src, tmp, iLen);
        return 0;
    }

    return -1;
}