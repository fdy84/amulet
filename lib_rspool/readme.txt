rspool2

rspool2有如下特点：
1、相比以前版本，结构更加合理，程序阅读方便
2、以前版本的rspool需要预先制定池大小，目前版本不需要
3、使用组的概念代替grid概念，相同类型资源放在一组
4、提供了group_foreach和all_foreach。能按组循环或者全部循环。

使用示例：


RSPOOL *rsp;

rsp = rspool_new();
/*         句柄 组号 资源句柄 */
rspool_put(rsp, 0, myhande); 
new_handle = rspool_get(rsp, 0);

while ((new_handle = rspool_group_foreach(rsp, 3)) != NULL) {
    printf("===>foreach: %s\n", new_handle);
}

rspool_del(rsp);

具体请参考 test_main.c
