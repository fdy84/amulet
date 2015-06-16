# **evilproxy** 使用手册 #

## 索引 ##

  * **evilproxy** 使用手册
    * evilproxy project
      * 0. History and credits
      * 1. Download & installation
      * 2. Documentation
      * 3. Help and bug reports
    * **evilproxy** 的使用
      * **evilproxy** 数据类型
      * **evilproxy** 函数


## evilproxy project ##

A Part of amulet project. exporting from dnprj project.

Website: http://code.google.com/p/amulet/ Main author: Dai,Nan <[dainan13@gmail.com](mailto:dainan13@gmail.com)>

evilproxy project is free software released under the New BSD License (see the LICENSE file for details)

### 0. History and credits ###

evilproxy project is an alpha version in current.

### 1. Download & installation ###

The latest development code is available from https://amulet.googlecode.com/svn/trunk/evilproxy/

evilproxy is a MITMA ( man in the middle attack ) proxy server. you can use it to watch the protocol of softwares though the proxy.

To install, unpack the evilproxy archive, move to the src dir and run

```
scons
```

And then you can run

```
./evilproxy [-D [[IP:]PORT]] [-L [[IP:]PORT]]
```

to set it up.

It Dependences with :

  * libev   http://libev.schmorp.de/


And to compile and install it, you must have :

  * python  http://www.python.org
  * scons   http://www.scons.org


### 2. Documentation ###

Documentation in reStructuredText format is available in the doc directory included with the source package.

### 3. Help and bug reports ###

You can report bugs and send patches to the amulet issue tracker, http://code.google.com/p/amulet/issues/list

## **evilproxy** 的使用 ##

### **evilproxy** 数据类型 ###

### **evilproxy** 函数 ###
