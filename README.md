## fpgaol_pi_ng

使用qt的fpgaol设备端程序

qt没有http服务器实现，使用QtWebApp处理http

使用qtwebsocket处理ws

可能http和ws需要不同端口

### 依赖

* qt

* qtwebsockets

* qtserial

#### ubuntu

apt-get install qt5-default libqt5websocket libqt5serialport

### 编译

build.sh

build_win32.bat

### 运行

现在没有etc配置文件，在仓库目录使用build.sh默认编译后可执行文件在build\release\fpgaol_pi_ng

（os x需要修改相对路径）

保持字节流文件在../../*.bit，

静态文件使用../../docroot

现在供调试用的页面比较旧，将来可能会弄子仓专门管理room页面前端程序

本地测试打开浏览器127.0.0.1:8080

现在http没有设置允许跨域

http默认在8080

ws在8090