# **rspool2** 使用手册 #

## 索引 ##

  * **rspool2** 使用手册
    * **rspool2** 的使用
      * **rspool2** 数据类型
      * **rspool2** 函数


> refer to readme.txt please. only chinese edition is available.

## **rspool2** 的使用 ##

### **rspool2** 数据类型 ###

RSPOOL{{{
typedef struct resource\_pool
{
> RSPOOL\_GROUP **grp\_array[MAX\_GROUPS](MAX_GROUPS.md);
> pthread\_mutex\_t g\_arr\_lock;
> int group\_count;
> int max\_gid;**

> /**for all foreach**/
> int cur\_gid;

} RSPOOL;}}}

testdoc

=== *rspool2* 函数 ===

int rspool_put[#RSPOOL RSPOOL]* *rsp*int* gid*void* *handle*rsp 句柄 gid 组号 handle 资源句柄

```