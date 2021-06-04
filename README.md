# BUPT COMPUTER NETWORK DNS
北邮计网课设项目：DNS服务器  



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

## 执行
执行build/src/dns.exe即可

## feature
* 解析报文
* lru + hash cash
    * A,AAAA缓存(如果有cname，会直接把cname指向的ip存到A记录上)
    * 有一个缓存系统，可以自行添加更多解析报文
* 彩色等级log
* 报文转换表
* 读取本地A记录

目前多线程有死锁,在dev分支，有空改