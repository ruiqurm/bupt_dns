# BUPT COMPUTER NETWORK DNS
北邮计网课设项目：DNS服务器  


现在的预计工作：
* select
* 返回错误报文
* 解析authority
* 命令行
* 异常处理
* cache cname
## 目录结构
```
|  .gitignore
│  CMakeLists.txt
│  README.md
├─include
│      cache.h
│      common.h
│      log.h
│      relay.h
│
├─src
│      cache.c
│      CMakeLists.txt
│      common.c
│      log.c
│      main.c
│      relay.c
│
└─test
        CMakeLists.txt
        test.c
```
## 如何部署
### 生成makefile
```cmd
mkdir build
cd build
cmake .. -G "Unix Makefiles"
//或者用cmake-gui ..操作
make
```
### 切换为clang编译器
windows下要切换clang可以用`cmake-gui ..`。configure编译器路径，然后generate.

### 生成vs solution


```cmd
mkdir build
cd build
cmake ..
```
生成的解决方案在`/build/`路径下
## 执行
执行build/src/dns.exe即可


## 命令行解析
1. 加载 dnsrelay.txt
2. 设置 cache 大小
3. help
4. debug
5. 设置服务器