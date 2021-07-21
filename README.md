# uwb_controller
[TOC]

## 1.硬件

<img src="C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210627150109790.png" alt="image-20210627150109790" style="zoom:50%;" />

* 上面两个是天线接口，一个收，一个发。

* 下面有两个micro-USB接口，左边的负责供电（可以用移动电源供电，注意不要把移动电源接到数据端口），右边的负责传输数据。
* 接上电源之后，蓝灯会亮，然后连接上数据端口，绿灯会亮。等绿灯亮了之后才能进行控制

## 2.天线

![image-20210702135712934](C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702135712934.png)

### 30°天线

<img src="C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702135731467.png" alt="image-20210702135731467" style="zoom: 25%;" />

### 90°天线

<img src="C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702135723176.png" alt="image-20210702135723176" style="zoom: 15%;" />



## 3.连接

* 使用USB连接到电脑，电脑上需要安装官方提供的软件（即使是通过程序控制UWB，也需要在电脑上提前安装，因为Service模块是随着软件一同安装到电脑上的）


## 4.设置

* `Antenna Mode`必须设置成一收一发，哪个收哪个发无所谓
* `Scan start`和`Scan stop`非常重要，这两个参数决定了测距的范围
* ![image-20210627152008995](C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210627152008995.png)
* `Scan start`决定下界，`Scan stop`决定上界

## 5.Service

* 其实就是过滤器，可以从检测到的信号中过滤出运动信号
* 随着官方提供的软件自动安装到电脑上，UWB传来的信号会先经过Service，经过Service过滤之后再传给软件
* 通过IP连接localhost127.0.0.1：21210
* 代码中通过`haveServiceIp`来控制是否连接Service
* 可以设置阈值来影响检测效果（值越大，检测到的数据越少）
  * <img src="C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702134349185.png" alt="image-20210702134349185" style="zoom: 67%;" />

## 6.控制

我们对UWB的控制都是通过发送指令来完成的，具体的工作交给UWB自己处理，我们只需要根据API发送正确的指令即可

```c
通过mrmControl(userScanCount, userScanInterval)来控制UWB开始扫描
```

​	第一个参数是扫描点数，点数到达该值后会自动停止扫描。若把改参数设置为**65535**，则将永久扫描，不会自动停止

​	第二个参数是扫描间隔，使用默认即可

```c
通过mrmInfoGet(timeoutMs, &info)来获取检测到的信息
```

​	第一个参数是超时时间，使用默认即可

​	第二个参数用于读入数据，获取到的数据就存放在它之中

​	需要注意的是，一旦开始扫描，就**不能随意地停止程序**，停止程序会造成UWB持续地扫描，从而无法再次建立连接，只能通过重新插	拔解决。实际测试的时候可以将`userScanCount`设置为一个较小的值，等扫描自动停止后再开启下一轮扫描。

​	如果以后需要改动程序，一定要用`mrmSampleExit();`来结束程序。

## 7.数据格式

* 通过`mrmInfoGet(timeoutMs, &info)`读取到的数据有两种数据类型，分别是`MRM_FULL_SCAN_INFO`原始数据和`MRM_DETECTION_LIST_INFO`动检测数据

* 我们一般只看动检测数据就足够，`bin=info->msg.detectionList.detections[0].index`就是我们用来计算距离的数据
* 计算公式就是`distance = ((float)bin * 61) * 3 / 20000;`

* 因为UWB的穿透性很强，它能穿透物体检测到物体的内部和后面，所以我们每次只取检测的第一个点`detections[0]`，代表第一个检测到的物体到UWB的距离

## 8.配置文件

* 将测试中经常需要更改的参数放在配置文件中，这样每次更改完参数之后不需要重新编译
* 使用时将配置文件和exe文件放在同一目录下即可，不需要安装IDE
* `userScanCount`：用来控制扫描的点数，设为65535时一直扫描
* COM端口：用来设置UWB连接到电脑的哪个端口，每台电脑都不相同，可以用官方软件查看连接到的COM口
* `Scan stop`：决定最大的测距范围
* `detectionListThresholdMult`：设置Service的动检测阈值
* `distance_calculation_mode`：设置距离计算的模式。
  * 1：动检测数据
  * 2：原始数据
  * 3：滤除部分干扰点的动检测数据
* 中心节点的IP地址（默认端口是8888，不用变）

## 9.记录

![image-20210702132535221](C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702132535221.png)

* 原始记录文件非常复杂，有大量我们不需要的内容。所以我把记录功能给删去了，由中心节点来进行记录。
* 如果测试的时候想保存数据，可以用输出重定向。也可以在一台机器上同时运行中心节点和UWB控制程序，让中心节点来负责记录。

## 10.程序结构

![image-20210702132907311](C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702132907311.png)

* `mrmSampleApp.c`:程序的入口，所有控制代码都在该文件中
* `mrmIf.c`:一些和连接相关的函数
* `mrm.c`:一些控制UWB的函数，结构完全相同，只是封装的指令不同
* `hostInterfaceMRM.h`:一些与UWB通信的结构体，以及定义消息类型。如果有额外需要，但是里面没有，参照api文档添加即可。
  * <img src="C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702134820515.png" alt="image-20210702134820515" style="zoom:50%;" />
* `hostInterfaceCommon.h`:定义消息的参数。如果没有，参照文档添加即可
  * <img src="C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210702134704498.png" alt="image-20210702134704498" style="zoom:50%;" />

## 11.连接不上的解决办法

* 连接不上一般有两种可能

  ![image-20210721111059765](C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210721111059765.png)

  * 程序没有运行就关闭，这时uwb会一直进行扫描，无法再次建立连接。重新插拔即可解决（尽量不要在程序运行中关闭程序）
  * 如果重新插拔之后还是连接不上，尝试使用官方软件进行连接。如果官方软件出现error或者timeout，重新启动电脑可以解决。

## 12.测试步骤

* 场地布置（uwb更容易检测到纵向移动的目标）![image-20210721111710468](C:\Users\10733\AppData\Roaming\Typora\typora-user-images\image-20210721111710468.png)
* 网络布置：所有设备都需要连接到同一个网络，中心节点需要录入每个uwb和小车的ip；uwb需要录入中心节点的ip
* GPS值录入：用小车上的惯导测量三个uwb节点的GPS，录入中心节点的代码中
* 开始测试
