# 投屏器demo使用方法
### 该应用作为RV1126B投屏器发送端使用，使用命令参考如下：

```
执行命令 projector_demo  -d /dev/video1 -w 3840 -h 2160 -p NV12  --stream-mmap=3 --enable-encode=1  --dst-ip=192.168.1.112  --enSlice=0 --bitrate=10 &


```

其中

-d : 指定取流节点，通过v4l2工具确定

-w、-h：宽高

-p：指定VI出图格式，参考main_tx.cpp代码查看支持的参数

-dst-ip：指定接收端的id

-enSlice：指定是否开启slice编码，接收端也要同步设置，目前设置的是4slice编码

-bitrate：码率，单位为mbps

其余进阶参数可以参阅main_tx.cpp



### 接收端环境准备

目前支持3588接收端演示，固件可以使用rxfirmware-RK3588，需要基于RK3588-EVB1使用

```
执行命令 /usr/bin/testHdmiRxDec -w 3840 -h 2160 -p NV12 --stream-mmap=3 --display-type=0 --display-w=3840 --display-h=2160  --enable-adjust-clk=0  --dst-ip=192.168.1.111 --enSlice=0


```

其中：enSlice参数和发送端要一一对应，dst参数要指定发送端设备的ip

通过RK3588的hdmi接口连接显示器，即可获得投屏后的画面。



### TRACE功能

应用默认开启了trace功能，如果不需要trace功能可以通过如下补丁关闭：

```
diff --git a/low_delay_net_display/tools/rt_trace.cpp b/low_delay_net_display/tools/rt_trace.cpp
index b580ea8..1e0957f 100644
--- a/low_delay_net_display/tools/rt_trace.cpp
+++ b/low_delay_net_display/tools/rt_trace.cpp
@@ -27,9 +27,9 @@
 #include "rt_trace.h"
 #include "rt_common.h"
 
-#define HAVE_TRACE
+#define WRITE_MSG(format_begin, format_end, name, value) {;}
 
-#ifdef HAVE_TRACE
+#if 0
 
 #ifdef __cplusplus
 #define CC_LIKELY(exp)      (__builtin_expect(!!(exp), true))
diff --git a/low_delay_net_display/tools/trace.cpp b/low_delay_net_display/tools/trace.cpp
index 5c39901..e62dcab 100644
--- a/low_delay_net_display/tools/trace.cpp
+++ b/low_delay_net_display/tools/trace.cpp
@@ -9,7 +9,7 @@
 #include <inttypes.h> 
 #include <string.h>
 
-#ifdef TRACE_ON
+#if 0


```



