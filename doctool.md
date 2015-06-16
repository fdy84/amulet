# doctool工具的使用 #

doctool工具提供了一套自动化文档生成工具。


## 准备工作 ##

需要Python2.7以上版本的支持。并在此之上安装Sphinx文档模块工具。

Sphinx文档模块工具的安装可以使用`easy_install -U Sphinx`命令进行安装，windows用户的easy\_install.exe命令被安装在Python安装目录的Scripts目录下面，如果你的Python安装目录没有Scripts目录则需要先安装setuptools模块工具，setuptools模块工具的下载页面在：

http://pypi.python.org/pypi/setuptools/0.6c11

请按照说明步骤进行安装。

在Python与Sphinx文档模块工具安装后，windows用户请确认检查`.py`文件是否已与Python程序进行了文件关联，如果没有关联请手动进行关联或对系统%PATH%环境变量是否含括python主程序的目录，以上两个条件请至少满足其一。

## 文档生成 ##

进入\_doctools/script目录，执行python compile.py，windows用户可以双击compile.bat文件进行文档生成。

生成的文件默认被放在\_doctools/build目录下，编译文档请参考sphinx操作或make.bat/Makefile中的说明。

## 管理与配置 ##

需要进行文档自动生成的项目需要在\_doctools/build/config.conf文件中加入项目的条目，conf文件中的section名叫作为工程名，当前支持src参数来指定目录名，auth参数指定作者信息

## 文档自动生成的注释约定 ##

当前支持在.c文件与.h文件中通过注释的方式来指定生成文档。

注释必须以`/*`作为一行的起头，后面跟autodoc标识。
```
/* autodoc
   这里的内容将被作为rst文档的一部分，所以务必请这个的注释内容符合rst语法
*/
int foo(int a, int b)
{
   ...
}



/*
最后生成的rst文档片段如下：

.. c:function:: int foo(int a, int b)

    这里的内容将被作为rst文档的一部分，所以务必请这个的注释内容符合rst语法

*/
```