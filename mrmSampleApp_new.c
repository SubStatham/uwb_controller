//_____________________________________________________________________________
//
// Copyright 2014 Time Domain Corporation
//
//
// mrmSampleApp.c
//
//   Sample code showing how to interface to P400 MRM module.
//
//   This code uses the functions in mrm.c to:
//      - connect to the radio via ethernet, usb, or serial
//      - get the MRM configuration from the radio and print it
//      - get the status/info from the radio and print it
//      - start MRM and receive scans from it according to user parameters
//
//_____________________________________________________________________________


//_____________________________________________________________________________
//
// #includes 
//_____________________________________________________________________________
#define N 1024
#define _CRT_SECURE_NO_WARNINGS
//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <WinSock2.h> 
//windows 系统头文件
#include <signal.h> 
//ctrlc信号，linux

#include "mrmIf.h"
#include "mrm.h"

//小车上的头文件
//#include <wiringPi.h>
//#include <softPwm.h>
//#include <sys/time.h>

#include <stdio.h>
#include <string.h>
//#include <wiringSerial.h>
#include <errno.h>

//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#include <sys/types.h>	       
#include <stdlib.h>
//#include <pthread.h>
#include <stdbool.h>





//static void my_handler(void) { // linux系统下程序关闭时执行该函数
//	mrmSampleExit();
//}



//BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);

//BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
//{
//	// 控制台将要被关闭
//	if (CTRL_CLOSE_EVENT == dwCtrlType)
//	{
//		 mrmSampleExit();
//	}
//	return TRUE;
//}
//_____________________________________________________________________________
//
// #defines 
//_____________________________________________________________________________

#define DEFAULT_RADIO_IP			"192.168.1.100"
#define DEFAULT_BASEII				12
#define DEFAULT_SCAN_START			10000
#define DEFAULT_SCAN_STOP			80000

#define DEFAULT_SCAN_COUNT			1000//文档里面写着 65535是一直运行
#define DEFAULT_SCAN_INTERVAL		125000
#define DEFAULT_TX_GAIN				63

static int haveServiceIp, connected;
int sum = 0, ave = 0, count = 0, number = 0;
float distance = 0.0;
char recvbuf[N] = { 0 };        //用来储存接收到的内容
char InputString[N] = { 0 };    //用来储存接收到的有效数据包
int NewLineReceived = 0;      //前一次数据结束标志
char ReturnTemp[N] = { 0 };     //存储返回值
unsigned char g_frontservopos = 90;
int  userScanCount = 250, userScanInterval;
mrmInfo info;
int timeoutMs, recvflag=0 ,scanflag=0;
//定义标志位，flag=0时停止，flag=1时工作，flag=2时重启
FILE *fp = NULL;

// 初始TCP未连接
bool is_connected = false;

static void mrmSampleExit(void)
{
	if (connected)
		mrmDisconnect();
	mrmIfClose();
	exit(0);
}

//_____________________________________________________________________________
//
// typedefs
//_____________________________________________________________________________


//_____________________________________________________________________________
//
// static data
//_____________________________________________________________________________



//_____________________________________________________________________________
//
// local functions
//_____________________________________________________________________________

//_____________________________________________________________________________

 //processInfo - process info messages from MRM
//_____________________________________________________________________________
//static void processInfo(mrmInfo *info, int printInfo)   //printInfo  = 1
//{
//	WSADATA wsadata;
//	WSAStartup(MAKEWORD(2, 2), &wsadata);
//
//	SOCKET sock = socket(PF_INET,SOCK_STREAM,0);
//
//	//SOCKADDR sockAddr;
//
//	SOCKADDR_IN  sockAddr;
//	memset(&sockAddr, 0, sizeof(sockAddr));
//	sockAddr.sin_family = PF_INET;
//	sockAddr.sin_port = htons(8888);
//	//sockAddr.sin_addr = inet_addr(RASPI_IP);
//
//
//	connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
//	if (printInfo)
//		send(sock, info->msg.scanInfo.msgId, sizeof(info->msg.scanInfo.msgId), 0);
//
//
//
//}
//




void *receive_message(int socket)
{
	int n = 0,m=0;
	// while(1)
	while (is_connected)
	{
		memset(recvbuf, 0, sizeof(recvbuf));//清空数组
		n = recv(socket, recvbuf, sizeof(recvbuf), 0);//接收数据并存入recvbuf中，如果没收到数据就阻塞在这。
		if (n < 0)
		{
			perror("Fail to recv!\n");
			break;
			//	exit(EXIT_FAILURE);
		}
		else if (n == 0) {
			printf("clinet_recv is not connect\n");
			break;
			//	exit(EXIT_FAILURE);
		}
		printf("Recv %d bytes : %s\n", n, recvbuf);
		recvflag = atoi(recvbuf);
		//数据打包，控制小车，uwb用不到
		/*Data_Pack();
		if (NewLineReceived == 1)
		{
			tcp_data_parse();
		}*/


		//0--暂停，1--开始，2--重启，-1--程序结束
		if (recvflag == 0)
		{
			userScanCount = 0;
			mrmControl(userScanCount, userScanInterval) != 0;
			scanflag = 0;
		}
		else if (recvflag == 1)
		{
			userScanCount = 65535;
			mrmControl(userScanCount, userScanInterval) != 0;
			scanflag = 1;
		}
		else if (recvflag == 2)
		{
			reboot();
			scanflag = 0;
		}


	}
	shutdown(socket, 2);
	printf("receive thread closed\r\n");
	is_connected = false;
	pthread_exit(NULL);//断开连接就退出线程
}
/**
* Function       itoa
* @author        Danny
* @date          2017.08.16
* @brief         将整型数转换为字符
* @param[in]     void
* @retval        void
* @par History   无
*/
//void itoa(int i, char *string)
//{
//	sprintf(string, "%d", i);
//	return;
//}

/**
* Function       tcp_data_postback
* @author        Danny
* @date          2017.08.16
* @brief         将采集的传感器数据通过tcp传输给上位机显示
* @param[in]     void
* @retval        void
* @par History   无
*/

//char *tcp_data_postback()
//{
//	//小车超声波传感器采集的信息发给上位机显示
//	//打包格式如:
//	//    超声波 电压  灰度  巡线  红外避障 寻光
//	//$4WD,CSB120,PV8.3,GS214,LF1011,HW11,GM11#
//	char *p = ReturnTemp;
//	char str[25];
//	float distance;
//	memset(ReturnTemp, 0, sizeof(ReturnTemp));
//	//超声波
//	distance = Distance();
//	if ((int)distance == -1)
//	{
//		distance = 0;
//	}
//	strcat(p, "$4WD,CSB");
//	itoa((int)distance, str);
//	strcat(p, str);
//	
//	//printf("ReturnTemp_first:%s\n",p);
//	return ReturnTemp;
//}
void *send_message(int socket)
{
	int n = 0;
	char str[1024] = { 0 };
	// while(1)
	while (is_connected)
	{
		while (scanflag == 1)
		{

			mrmInfoGet(timeoutMs, &info) == 0;
			distance = distance_calculation1(&info, fp);

			//SetConsoleCtrlHandler(HandlerRoutine, TRUE);
			//processInfo(&info, fp, userPrintInfo);
			//send_data(distance);

			memset(str, 0, sizeof(str));//清空str
			sprintf(str, "%a.bf", distance);//把float型数据转换成string型
			//strcpy(str, distance);//字符串复制：把tcp_data_postback()函数复制到str中去。
			//puts(str);//输出字符串并换行。
			n = send(socket, str, strlen(str), 0);
			if (n < 0)
			{
				perror("Fail to send!\n");
				break;
				//	exit(EXIT_FAILURE);
			}
			else if (n == 0) {
				printf("clinet_postback is not connect\n");
				break;
				//	exit(EXIT_FAILURE);
			}
			printf("send %d bytes : %s\n", n, str);
			sleep(0.5);//数据量很大，不能等待5秒
		}

		if (recvflag == -1)//程序结束
		{
			shutdown(socket, 2);
			printf("send thread closed\r\n");
			is_connected = false;
			pthread_exit(NULL);
		}
	}
	/*shutdown(socket, 2);
	printf("send thread closed\r\n");
	is_connected = false;
	pthread_exit(NULL);*/
}

int connect_socket()
{
	//定义连接套接字
	int socket_ = 0;
	//定义结构体存储服务器端地址信息
	struct sockaddr_in server_addr;
	//通过socket创建监听套接字
	socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (socket < 0)
	{
		perror("Fail to socket");
		exit(EXIT_FAILURE);
	}
	//填充服务器的ip地址和端口
	memset(&server_addr, 0, sizeof(server_addr));//清空
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8888);
	server_addr.sin_addr.s_addr = inet_addr("192.168.50.63");
	printf("Connecting...\r\n");
	while (!is_connected)
	{
		if (connect(socket_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1)
		{
			printf("connect failed:%d\r\n", errno);//失败时可以打印errno
			sleep(5);
		}
		else
		{
			is_connected = true;
			break;
		}
	}
	return socket_;
}

static void processInfo(mrmInfo *info, FILE *fp, int printInfo)
{
	unsigned int i;

	
    switch (info->msg.scanInfo.msgType)
    {
        case MRM_DETECTION_LIST_INFO://需要的是这个数据中的index
            // print number of detections and index and magnitude of 1st detection
			if (printInfo)
                printf("DETECTION_LIST_INFO: msgId %d, numDetections %d, 1st detection index: %d, 1st detection magnitude: %d\n",
                        info->msg.detectionList.msgId,
                        info->msg.detectionList.numDetections,
                        info->msg.detectionList.detections[0].index,
                        info->msg.detectionList.detections[0].magnitude);
			//if (count == 1)//这样太依赖于第一次的数据
			//{
			//	sum = sum + info->msg.detectionList.detections[0].index;
			//	ave = sum / count;
			//	count = count + 1;
			//}
			//else
			//{
			//	if ((ave - info->msg.detectionList.detections[0].index < 300) && (info->msg.detectionList.detections[0].index - ave < 300))
			//	{
			//		count = count + 1;
			//		sum = sum + info->msg.detectionList.detections[0].index;
			//		ave = sum / count;
			//	}
			//}
			//distance = (ave * 64 - 10000) * 3 / 20000;//最终发送给服务器的数据
			// log detection list message
            //if (haveServiceIp && fp)
			if(fp)
            {
    			fprintf(fp, "%ld, MrmDetectionListInfo, %d, %d", (long)time(NULL), info->msg.detectionList.msgId, info->msg.detectionList.numDetections);
				for (i = 0; i < info->msg.detectionList.numDetections; i++)
					fprintf(fp, ", %d, %d", info->msg.detectionList.detections[i].index, info->msg.detectionList.detections[i].magnitude);
				fprintf(fp, "\n");
            }
            break;

		case MRM_FULL_SCAN_INFO:
			if (printInfo)
				printf("FULL_SCAN_INFO: msgId %d, sourceId %d, timestamp %d, "
						"scanStartPs %d, scanStopPs %d, "
						"scanStepBins %d, scanFiltering %d, antennaId %d, "
						"operationMode %d, numSamplesTotal %d, numMessagesTotal %d\n",
						info->msg.scanInfo.msgId,
						info->msg.scanInfo.sourceId,
						info->msg.scanInfo.timestamp,
						info->msg.scanInfo.scanStartPs,
						info->msg.scanInfo.scanStopPs,
						info->msg.scanInfo.scanStepBins,
						info->msg.scanInfo.scanFiltering,
						info->msg.scanInfo.antennaId,
						info->msg.scanInfo.operationMode,
						info->msg.scanInfo.numSamplesTotal,
						info->msg.scanInfo.numMessagesTotal);

			// log scan message
			if (fp) {
				fprintf(fp, "%ld, MrmFullScanInfo, %d, %d, %d, %d, %d, %d, %d, %d, %u", (long)time(NULL), info->msg.scanInfo.msgId, info->msg.scanInfo.sourceId,
					info->msg.scanInfo.timestamp, info->msg.scanInfo.scanStartPs, info->msg.scanInfo.scanStopPs, info->msg.scanInfo.scanStepBins, 
						info->msg.scanInfo.scanFiltering, info->msg.scanInfo.antennaId, info->msg.scanInfo.numSamplesTotal);
				
				for (i = 0; i < info->msg.scanInfo.numSamplesTotal; i++)
					fprintf(fp, ", %d", info->scan[i]);
				fprintf(fp, "\n");
			}
            free(info->scan);
            break;

        default:
            break;
    }
}

float distance_calculation1(mrmInfo *info ,FILE *fp)//抛弃前30个点，以及索引值小于300的点
{
	if (info->msg.scanInfo.msgType== MRM_DETECTION_LIST_INFO)
	{       
		/*number = number + 1;*/
		count = count + 1;
		if (count>30)
		{
			if ((info->msg.detectionList.detections[0].index>200))
				//if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
				{
					//sum = sum + info->msg.detectionList.detections[0].index;
					//ave = sum / count;
				ave = info->msg.detectionList.detections[0].index;
					//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
				}
		}
		//else
		//{
		//	if (info->msg.detectionList.detections[0].index > 300)
		//	{
		//		sum = sum + info->msg.detectionList.detections[0].index;
		//		//number = number + 1;
		//		//count = count + 1;
		//	}
		//}	

		distance = ((float)ave * 61 ) * 3 / 20000;//最终发送给服务器的数据
		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
		printf("sum=%d,ave=%d,distance= %f\n",sum,ave, distance);
	
		fprintf(fp, "1, %d, %d, %f\n", count,info->msg.detectionList.detections[0].index,distance);
		return distance;
	}
}

float distance_calculation2(mrmInfo *info, FILE *fp)   //舍弃前三十个点以及索引值小于300的点，三十到四十之间不筛选直接输出并取平均作为初始值，四十之后根据上一个值筛选点并不断更新均值
{
	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
	{
		/*number = number + 1;*/
		count = count + 1;
		if (30<count<40)
		{
			number = number + 1;
			if ((info->msg.detectionList.detections[0].index > 300))
				//if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
			{
				sum = sum + info->msg.detectionList.detections[0].index;
				ave = sum / number;
			//	distance = ((float)ave * 61) * 3 / 20000;

			}
		}
		else if(count>=40)
		{
			if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
			{
				//sum = sum + info->msg.detectionList.detections[0].index;
				ave = info->msg.detectionList.detections[0].index;
			//	distance = ((float)ave * 61) * 3 / 20000;
				//number = number + 1;
				//count = count + 1;
			}
		}

			distance = ((float)ave * 61 ) * 3 / 20000;//最终发送给服务器的数据
		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
		printf("count=%d,ave=%d,distance= %f\n", count, ave, distance);
		fprintf(fp, "2, %d,%d,%d, %d, %f\n", count,sum,ave, info->msg.detectionList.detections[0].index, distance);
		return distance;
	}
}

//float distance_calculation3(mrmInfo *info, FILE *fp)//舍弃前三十个点以及索引值小于300的点，第三十一个点直接输出，之后的点与上一个点的差值不超过200
//{
//	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
//	{
//		/*number = number + 1;*/
//		count = count + 1;
//		if (count = 30)
//		{
//			if ((info->msg.detectionList.detections[0].index > 300))
//				//if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
//			{
//				/*sum = sum + info->msg.detectionList.detections[0].index;
//				ave = sum / count;*/
//				ave= info->msg.detectionList.detections[0].index;
//				distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//
//			}
//		}
//		else if(count>30)
//		{
//			if ((info->msg.detectionList.detections[0].index > 300)&& (abs(info->msg.detectionList.detections[0].index-ave)<200))
//			{
//				//sum = sum + info->msg.detectionList.detections[0].index;
//				//number = number + 1;
//				//count = count + 1;
//				distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//			}
//		}
//
//		//	distance = ((float)ave * 61 ) * 3 / 20000;//最终发送给服务器的数据
//		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//		printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//		fprintf(fp, "3, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//		return distance;
//	}
//}
//唤醒uwb
//float distance_calculation4(mrmInfo *info, FILE *fp)//舍去索引值小于300的点并记录大于500的点的个数，当十个值中有6个值都大于500，则认为检测开始，并以该十个点的均值作为目标的初始位置
//{                                    //之后若长时间未检测到大于300的点，重新回到未唤醒状态
//	int flag = 0;
//	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
//	{
//		/*number = number + 1;*/
//		if (flag = 0)
//		{
//			if ((info->msg.detectionList.detections[0].index > 300))
//				//if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
//			{
//				/*sum = sum + info->msg.detectionList.detections[0].index;
//				ave = sum / count;*/
//				number = number + 1;
//				if ((info->msg.detectionList.detections[0].index > 500))
//
//				{
//					count = count + 1;
//					sum = sum + info->msg.detectionList.detections[0].index;
//					ave = info->msg.detectionList.detections[0].index;
//					/*distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;*/
//				}
//				if (number == 10)
//				{
//					if (count >= 6)
//					{
//						flag = 1;
//						printf("start detection\n");
//					}
//					else
//					{
//						number = 0;
//						count = 0;
//						sum = 0;
//						ave = 0;
//					}
//				}
//
//			}
//		}
//		else   //已被唤醒
//		{
//			if ((info->msg.detectionList.detections[0].index > 300))
//			{
//				distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//			}
//		}
//	}
//	
//		
//			
//		
//
//		//	distance = ((float)ave * 61 ) * 3 / 20000;//最终发送给服务器的数据
//		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//		printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//		fprintf(fp, "4, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//		return distance;
//}

//float distance_calculation5(mrmInfo *info, FILE *fp)//舍去索引值小于300的点并记录大于500的点的个数，当十个值中有6个值都大于500，则认为检测开始，并以该十个点的均值作为目标的初始位置
//{                                    //之后若长时间未检测到大于300的点，重新回到未唤醒状态
//	int flag = 0;
//	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
//	{
//		/*number = number + 1;*/
//		if (flag = 0)
//		{
//			if ((info->msg.detectionList.detections[0].index > 300))
//				//if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
//			{
//				/*sum = sum + info->msg.detectionList.detections[0].index;
//				ave = sum / count;*/
//				number = number + 1;
//				if ((info->msg.detectionList.detections[0].index > 500))
//
//				{
//					count = count + 1;
//					sum = sum + info->msg.detectionList.detections[0].index;
//					ave = sum / count;
//					/*distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;*/
//				}
//				if (number == 10)
//				{
//					if (count >= 6)
//					{
//						flag = 1;
//						printf("start detection\n");
//					}
//					else
//					{
//						number = 0;
//						count = 0;
//						sum = 0;
//						ave = 0;
//					}
//				}
//
//			}
//		}
//		else   //已被唤醒
//		{
//			if ((info->msg.detectionList.detections[0].index > 300)&& (abs(ave - info->msg.detectionList.detections[0].index) < 300))
//			{
//				distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//			}
//		}
//	}
//
//
//
//
//
//	//	distance = ((float)ave * 61 ) * 3 / 20000;//最终发送给服务器的数据
//	//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//	printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//	fprintf(fp, "5, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//	return distance;
//}


//float distance_calculation6(mrmInfo *info, FILE *fp)//舍去索引值小于300的点并记录大于500的点的个数，当十个值中有6个值都大于500，则认为检测开始，并以该十个点的均值作为目标的初始位置
//{                                    //之后若长时间未检测到大于300的点，重新回到未唤醒状态
//	int flag = 0;  //判断是否唤醒的标志位
//	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
//	{
//		/*number = number + 1;*/
//		if (flag = 0)
//		{
//			if ((info->msg.detectionList.detections[0].index > 300))
//				//if ((abs(ave - info->msg.detectionList.detections[0].index) < 300))
//			{
//				/*sum = sum + info->msg.detectionList.detections[0].index;
//				ave = sum / count;*/
//				number = number + 1;
//				if ((info->msg.detectionList.detections[0].index > 500))
//
//				{
//					count = count + 1;
//					//sum = sum + info->msg.detectionList.detections[0].index;
//					ave = info->msg.detectionList.detections[0].index;
//					/*distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;*/
//				}
//				if (number == 10)
//				{
//					if (count >= 6)
//					{
//						flag = 1;
//						printf("start detection\n");
//					}
//					else
//					{
//						number = 0;
//						count = 0;
//					//	sum = 0;
//						ave = 0;
//					}
//				}
//
//			}
//		}
//		else   //已被唤醒
//		{
//			if ((info->msg.detectionList.detections[0].index > 300) && (abs(ave - info->msg.detectionList.detections[0].index) < 300))
//			{
//				distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//			}
//		}
//	}
//
//
//
//
//
//	//	distance = ((float)ave * 61 ) * 3 / 20000;//最终发送给服务器的数据
//	//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//	printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//	fprintf(fp, "6, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//	return distance;
//}

void send_data(float data)
{
	int n = 0;
	char str[1024] = { 0 };

	memset(str, 0, sizeof(str));//清空str
	sprintf(str, "%a.bf", data);//把float型数据转换成string型
	//strcpy(str, distance);//字符串复制：把tcp_data_postback()函数复制到str中去。
	//puts(str);//输出字符串并换行。
	n = send(socket, str, strlen(str), 0);
	if (n < 0)
	{
		perror("Fail to send!\n");
		//return 0;
		//	exit(EXIT_FAILURE);
	}
	else if (n == 0) {
		printf("clinet_postback is not connect\n");
		//return 0;
		//	exit(EXIT_FAILURE);
	}
	printf("send %d bytes : %s\n", n, str);
	sleep(0.5);//数据量很大，不能等待5秒
}
//_____________________________________________________________________________
//
// usage - print command line options
//_____________________________________________________________________________

static void usage(void)
{
	printf("  USAGE: mrmsampleApp [OPTIONS] -u <USB Com Port> | -e <Ethernet IP Address> | -o <Serial COM Port>\n");
	printf("OPTIONS:    -m <MRM Service IP address> (Default = No Server, connect directly to radio)\n");
	printf("            -b <Base Integration Index> (Default = %d)\n", DEFAULT_BASEII);
	printf("            -s <Scan Start (picoseconds)> (Default = %d)\n", DEFAULT_SCAN_START);
	printf("            -p <Scan Stop (picoseconds)> (Default = %d)\n", DEFAULT_SCAN_STOP);
	printf("            -c <Scan Counts> (Default = %d)\n", DEFAULT_SCAN_COUNT);
	printf("            -i <Scan Interval (microseconds)> (Default = %d)\n", DEFAULT_SCAN_INTERVAL);
	printf("            -g <TX Gain> (Default = %d)\n", DEFAULT_TX_GAIN);
	printf("            -x Print Info Messages <1 = On/0 = Off> (Default = On(1))\n");
	printf("            -l <Log File Prefix> (Default = No Log File)\n");
   // exit(0);
}

//_____________________________________________________________________________
//
// mrmSampleExit - close interfaces and exit sample app
//_____________________________________________________________________________



//_____________________________________________________________________________
//
// main - sample app entry point
//_____________________________________________________________________________

int main(int argc, const char *argv[])
{
	//FILE *fp = NULL;
    char *radioAddr = DEFAULT_RADIO_IP, *serviceIp;
    int haveRadioAddr = 0;
    int i,mode;
	int userBaseII, userScanStart, userScanStop, userScanInterval, userTxGain, userPrintInfo, userLogging;
	char userLogPrefix[256]="test";
	char logFileName[256];
	char buf[1024] = { 0 };
	//定义线程id，分别用于发和收

	/*pthread_t tid1;
	pthread_t tid2;
	pthread_t tid3;*/
	int ret = 0;

	/*int sum=0, ave=0,count=1;*/
	mrmIfType mrmIf;

    // config and status strucs
    mrmConfiguration config;
    mrmMsg_GetStatusInfoConfirm statusInfo;

    // filter settings
    mrmFilterConfig filterConfig;

    // raw and filtered scans and detection lists are sent in this struct
    mrmInfo info;

	// announce to the world
	printf("MRM Sample App\n\n");
	//SetConsoleCtrlHandler(HandlerRoutine, TRUE);

	// Set Defaults

//#define DEFAULT_RADIO_IP			"192.168.1.100"
//#define DEFAULT_BASEII				12
//#define DEFAULT_SCAN_START			10000
//#define DEFAULT_SCAN_STOP			39297
//#define DEFAULT_SCAN_COUNT			5
//#define DEFAULT_SCAN_INTERVAL		125000
//#define DEFAULT_TX_GAIN				63
	userBaseII = DEFAULT_BASEII;
	userScanStart = DEFAULT_SCAN_START;
	userScanStop = DEFAULT_SCAN_STOP;
	//userScanCount = DEFAULT_SCAN_COUNT;
	userScanInterval = DEFAULT_SCAN_INTERVAL;
	userTxGain = DEFAULT_TX_GAIN;
	userPrintInfo = 1;
	userLogging = 1;

	//
	// NOTE
	//   Need to add code to validate（证实） command line arguments
	//

	// process command line arguments
	//if (argc < 3) {
	//	usage();
	//	exit(0);
	//}
	                                 //启动时可以改变默认参数
 //   for (i = 1; i < argc; )
 //   {
 //       // see if arg starts with a hyphen (meaning it's an option)
 //       if (*argv[i] == '-')
 //       {
 //           switch (argv[i++][1])
 //           {
	//			// USB
	//			case 'u':
			        mrmIf = mrmIfUsb;
					haveRadioAddr = 1;
					haveServiceIp = 1;
					serviceIp = "127.0.0.1";
					radioAddr = "COM3";
	//				break;

	//			// Ethernet
	//			case 'e':
	//		        mrmIf = mrmIfIp;
	//				haveRadioAddr = 1;
	//				radioAddr = argv[i++];
	//				break;

	//			// Serial COM
	//			case 'o':
	//		        mrmIf = mrmIfSerial;
	//				haveRadioAddr = 1;
	//				radioAddr = argv[i++];
	//				break;

	//			// Base Integration Index    //基础综合指数
	//			case 'b':
	//				userBaseII = atoi(argv[i++]);
	//				break;

	//			// Scan Start                    //扫描范围
	//			case 's':
	//				userScanStart = atoi(argv[i++]);
	//				break;
	//				
	//			// Scan Stop
	//			case 'p':
	//				userScanStop = atoi(argv[i++]);
	//				break;

	//			// Scan Count   //默认是5
	//			case 'c':
	//				userScanCount = atoi(argv[i++]);
	//				break;

	//			// Scan Interval   //扫描间隔
	//			case 'i':
	//				userScanInterval = atoi(argv[i++]);
	//				break;

	//			// TX Gain     //增益，最大63
	//			case 'g':
	//				userTxGain = atoi(argv[i++]);
	//				break;

 //               // Service IP address
 //               case 'm':
 //                   haveServiceIp = 1;
 //                   serviceIp = argv[i++];
 //                   break;

	//			// Print Info Messages       //打印信息的标志位，默认为1
	//			case 'x':
	//				userPrintInfo = atoi(argv[i++]);
	//				break;
	//				
	//			// Log file                      //写入文件的标志位，默认为0
	//			case 'l':
	//				strcpy(userLogPrefix, argv[i++]);
	//				userLogging = 1;
	//				break;

 //               default:
 //                   printf("Unknown option letter.\n\n");
 //                   usage();
 //                   break;
 //           }
 //       }
 //   }

 //   if (!haveRadioAddr)
      // usage();                //打印向导列表

    // Now print out what we are doing
    printf("Radio address: %s (%s)\n", radioAddr, mrmIf == mrmIfUsb ? "USB" :    //打印自己的ip地址和连接方式
            (mrmIf == mrmIfIp ? "Ethernet" : "Serial"));
    if (haveServiceIp)   //静态整型变量                  判断是否连接服务器              ？
        printf("Using MRM service at IP address: %s\n",serviceIp);                          // char 型*    ？   变量的值何来
    else
        printf("Receiving scans directly from radio (no scan server).\n");

    // If using service, connect to it
    if (haveServiceIp)
    {
        mrm_uint32_t status;

        if (mrmIfInit(mrmIfIp, serviceIp) != OK)      //初始化
        {
            printf("Failed to connect to service (bad IP address?).\n");
            exit(0);
        }
        // connect to radio
        if ((mrmConnect(mrmIf, radioAddr, &status) != OK) ||      //连接      mrmif是连接类型
                (status != MRM_SERVER_CONNECTIONSTATUSCODE_CONNECTED))
        {
            printf("Unable to connect to radio through service.\n");
            mrmSampleExit();//退出
        }
        connected = 1;   //连接标志位
    } else
    {
        // initialize the interface to the RCM
        if (mrmIfInit(mrmIf, radioAddr) != OK)   //根据连接方式的不同，进行初始化
        {
            printf("Initialization failed.\n");
            exit(0);
        }
    }

    // make sure radio is in active mode
    if (mrmSleepModeGet(&mode) != OK)              //获取雷达的工作模式：休眠还是唤醒
    {
        printf("Time out waiting for sleep mode.\n");   //获取失败
        mrmSampleExit();
    }

	// print sleep mode
    printf("Radio sleep mode is %d.\n", mode);        
    if (mode != MRM_SLEEP_MODE_ACTIVE)
    {
        printf("Changing sleep mode to Active.\n");                 //如果是休眠模式则唤醒
        mrmSleepModeSet(MRM_SLEEP_MODE_ACTIVE);
    }

    // make sure radio is in MRM mode
    if (mrmOpmodeGet(&mode) != OK)             //获取工作状态
    {
        printf("Time out waiting for mode of operation.\n");
        mrmSampleExit();
    }

	// print radio opmode
    printf("Radio mode of operation is %d.\n", mode);
    if (mode != MRM_OPMODE_MRM)
    {
        printf("Changing radio mode to MRM.\n");//如果不是MRM，则转为MRM
        mrmOpmodeSet(MRM_OPMODE_MRM);
    }

    // retrieve config from MRM    检索MRM的配置信息
    if (mrmConfigGet(&config) != 0)
    {
        printf("Time out waiting for config confirm.\n");
        mrmSampleExit();
    }

	// modify config with user inputs     使用用户输入修改配置
	config.baseIntegrationIndex = userBaseII;
	config.scanStartPs = userScanStart;
	config.scanEndPs = userScanStop;
	config.txGain = userTxGain;

	// write updated config to radio           //写入新的配置
	if (mrmConfigSet(&config) != 0)
	{
		printf("Time out waiting for set config confirm.\n");
		mrmSampleExit();
	}

    // print out configuration              打印新的输出
    printf("\nConfiguration:\n");
    printf("\tnodeId: %d\n", config.nodeId);
    printf("\tscanStartPs: %d\n", config.scanStartPs);
    printf("\tscanEndPs: %d\n", config.scanEndPs);
    printf("\tscanResolutionBins: %d\n", config.scanResolutionBins);
    printf("\tbaseIntegrationIndex: %d\n", config.baseIntegrationIndex);
    for (i = 0 ; i < 4; i++)
    {
        printf("\tsegment %d segmentNumSamples: %d\n", i, config.segmentNumSamples[i]);
        printf("\tsegment %d segmentIntMult: %d\n", i, config.segmentIntMult[i]);
    }
    printf("\tantennaMode: %d\n", config.antennaMode);
    printf("\ttxGain: %d\n", config.txGain);
    printf("\tcodeChannel: %d\n", config.codeChannel);

    if (haveServiceIp)
    {
        // now get the filter config from MRM                     获取MRM的滤波器信息
        if (mrmFilterConfigGet(&filterConfig) != 0)
        {
            printf("Time out waiting for filter config confirm.\n");
            mrmSampleExit();
        }

        // print out filter configuration
        printf("\nFilter Configuration:\n");
        printf("\tfilterFlags: %d\n", filterConfig.filterFlags);
        printf("\tmotionFilterIndex: %d\n", filterConfig.motionFilterIndex);
        printf("\tdetectionListThresholdMult: %d\n", filterConfig.detectionListThresholdMult);
    }

    // retrieve status info from MRM                  检索状态信息
    if (mrmStatusInfoGet(&statusInfo) != 0)
    {
        printf("Time out waiting for status info confirm.\n");
        mrmSampleExit();
    }

    // print out status info
    printf("\nStatus Info:\n");
    printf("\tPackage version: %s\n", statusInfo.packageVersionStr);
    printf("\tApp version: %d.%d build %d\n", statusInfo.appVersionMajor,
            statusInfo.appVersionMinor, statusInfo.appVersionBuild);
    printf("\tUWB Kernel version: %d.%d build %d\n", statusInfo.uwbKernelVersionMajor,
            statusInfo.uwbKernelVersionMinor, statusInfo.uwbKernelVersionBuild);
    printf("\tFirmware version: %x/%x/%x ver %X\n", statusInfo.firmwareMonth,
            statusInfo.firmwareDay, statusInfo.firmwareYear,
            statusInfo.firmwareVersion);
    printf("\tSerial number: %08X\n", statusInfo.serialNum);
    printf("\tBoard revision: %c\n", statusInfo.boardRev);
    printf("\tTemperature: %.2f degC\n\n", statusInfo.temperature/4.0);

	// start logging    写配置文件的状态和表头
	if (userLogging)
	{
		sprintf(logFileName, "%sMRM_Scans.csv", userLogPrefix);  //写入文件名
		//printf("\nLogging to file %s\n\n", logFileName);
		fp = fopen(logFileName, "w");     //打开文件
		
		if (fp)  
		{
			// Log Configuration
			fprintf(fp, "Timestamp, Config, NodeId, ScanStartPs, ScanStopPs, ScanResolutionBins, BaseIntegrationIndex, Segment1NumSamples, Segment2NumSamples, Segment3NumSamples, Segment4NumSamples, Segment1AdditionalIntegration, Segment2AdditionalIntegration, Segment3AdditionalIntegration, Segment4AdditionalIntegration, AntennaMode, TransmitGain, CodeChannel\n");
			fprintf(fp, "%ld, Config, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
					(long)time(NULL), config.nodeId, config.scanStartPs, config.scanEndPs, config.scanResolutionBins, config.baseIntegrationIndex,
					config.segmentNumSamples[0], config.segmentNumSamples[1], config.segmentNumSamples[2], config.segmentNumSamples[3],
					config.segmentIntMult[0], config.segmentIntMult[1], config.segmentIntMult[2], config.segmentIntMult[3], 
					config.antennaMode, config.txGain, config.codeChannel);

			// Log Control
			fprintf(fp, "Timestamp, Control, ScanCount, IntervalTimeMicroseconds\n");
			fprintf(fp, "%ld, Control, %d, %d\n", (long)time(NULL), userScanCount, userScanInterval);

			// Write Full Scan Header     
			fprintf(fp, "Timestamp, MrmFullScanInfo, MessageId, SourceId, Timestamp, ScanStartPs, ScanStopPs, ScanStepBins, Filtering, AntennaId, NumSamplesTotal, ScanData\n");

            // If using service, print Detection List header   
          // if (haveServiceIp)    //真正需要的是这块数据
    			fprintf(fp, "Timestamp, MrmDetectionListInfo, MessageId, NumDetections, DetectionData\n");
		}
		else
		{
			printf("Error opening %s\n", logFileName);
			mrmSampleExit();
		}
	}

    //
    // Ready to start collecting scans...
	//

    // Start scanning on the MRM
    printf("\nScanning with scan count of %d and interval of %d (microseconds)\n", userScanCount, userScanInterval);
    timeoutMs = 200;
    //if (mrmControl(userScanCount, userScanInterval) != 0)   //控制MRM扫描，放在线程里面，不收到信号就不扫描
    //{
    //    printf("Time out waiting for control confirm.\n");
    //    mrmSampleExit();
    //}
	//signal(SIGINT, my_handler);//linux程序关闭后执行my_handler函数
	//while (mrmInfoGet(timeoutMs, &info) == 0)    //从MRM收集扫描或接收检测列表
	//{
	//	
	//	processInfo(&info, fp, userPrintInfo);
	//	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	//}//往文件里写入数据     
	////在这里修改，通过ip发送出去




	int socket_fd = 0;
	//if (!is_connected)
	//{
		socket_fd = connect_socket();
	//	// 第一次连接成功
	//	printf("connect success\r\n");
	//	is_connected = true;
	//	// 创建接收消息的线程
	//	// socket_fd是给线程函数receive_message传递的参数
	//	ret = pthread_create(&tid1, NULL, receive_message, socket_fd);
	//	if (ret != 0)
	//	{
	//		fprintf(stderr, "Fail to pthread_create : %s\n", strerror(errno));
	//		exit(EXIT_FAILURE);
	//	}

	//	// 创建发送消息的线程
	//	// socket_fd是给线程函数send_message传递的参数
	//	ret = pthread_create(&tid2, NULL, send_message, socket_fd);
	//	if (ret != 0)
	//	{
	//		fprintf(stderr, "Fail to pthread_create : %s\n", strerror(errno));
	//		exit(EXIT_FAILURE);
	//	}
	//	// 分离式线程：线程结束的时候由系统自动来释放资源
	//	pthread_detach(tid1);
	//	pthread_detach(tid2);
	//}
	//else
	//{
	//	//如果已经连接，不需要做任何处理，小车线程都已经正常启动。
	//}


	while(mrmInfoGet(timeoutMs, &info)==0)
	{
		
		distance=distance_calculation1(&info,fp);
		

				mrmInfoGet(timeoutMs, &info) ;
				distance = distance_calculation1(&info, fp);
				int n = 0;
				char str[1024] = { 0 };

				memset(str, 0, sizeof(str));//清空str
				sprintf(str, "%a.bf", distance);//把float型数据转换成string型
				//strcpy(str, distance);//字符串复制：把tcp_data_postback()函数复制到str中去。
				//puts(str);//输出字符串并换行。
				n = send(socket, str, strlen(str), 0);
				if (n < 0)
				{
					perror("Fail to send!\n");
					//return 0;
					//	exit(EXIT_FAILURE);
				}
				else if (n == 0) {
					printf("clinet_postback is not connect\n");
					//return 0;
					//	exit(EXIT_FAILURE);
				}
				printf("send %d bytes : %s\n", n, str);
				sleep(0.5);//数据量很大，不能等待5秒

				//SetConsoleCtrlHandler(HandlerRoutine, TRUE);
				//processInfo(&info, fp, userPrintInfo);
				//send_data(distance);
		//SetConsoleCtrlHandler(HandlerRoutine, TRUE);
		//processInfo(&info, fp, userPrintInfo);
		//send_data(distance);
	}
   
	
	// stop radio
    if (mrmControl(0, 0) != 0)
    {
        printf("Time out waiting for control confirm.\n");
        mrmSampleExit();
    }

    // perform cleanup
	printf("\n\nAll Done!\n");
	if (fp)
		fclose(fp);
    mrmSampleExit();
    return 0;
}

