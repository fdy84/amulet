# **libnet** 使用手册 #

## 索引 ##

  * **libnet** 使用手册
    * **libnet** 的使用
      * **libnet** 数据类型
      * **libnet** 函数


## **libnet** 的使用 ##

### **libnet** 数据类型 ###

### **libnet** 函数 ###

inline int get\_rbluint32\_t**ip\*const char** **rblbase\*int** rblbase\_len\*uint32\_trblret\*查询一个ip是一个RBL中的值 uint32\_t ip, input: 字符串, 需要查询的ip,点分十进制形式的ip字符串,如"127.0.0.1",长度为INET\_ADDRSTRLEN const char **base, input: 字符串, rblbase的字符串指针, int rblbase\_len, input: 长度为RBLBASELEN uint32\_t**rblret, output: return: 成功返回0,其他为失败

int parse\_fromcharsrc\*取src中的尖括号中的邮件地址，并替换原src
