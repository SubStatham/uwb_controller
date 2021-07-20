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

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <WinSock2.h> 
#include <sys/types.h>
#include<stdbool.h>
#include<windows.h>
#include<fcntl.h>
#include<assert.h>
#include<errno.h>



#include "mrmIf.h"
#include "mrm.h"



//_____________________________________________________________________________
//
// #defines 
//_____________________________________________________________________________

#define DEFAULT_RADIO_IP			"192.168.1.100"
#define DEFAULT_BASEII				12
#define DEFAULT_SCAN_START			10000
#define DEFAULT_SCAN_STOP			80000
#define DEFAULT_SCAN_COUNT			200
#define DEFAULT_SCAN_INTERVAL		125000
#define DEFAULT_TX_GAIN				63
#define N 1024





//_____________________________________________________________________________
//
// static data
//_____________________________________________________________________________

static int haveServiceIp, connected;
float distance = 0.0;
int   userScanInterval;
mrmInfo info;
bool is_connected = false;
int timeoutMs = 500, recvflag = 0, scanflag = 0, sendflag = 0, index = 0;
char recvbuf[N] = { 0 };
int socket_fd = 0;
int n = 0;
char str[N] = { 0 };
int sum = 0, count = 0, number = 0;





int connect_socket(char* buffer7)
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
	server_addr.sin_addr.s_addr = inet_addr(buffer7);
	printf("Connecting...\r\n");
	while (!is_connected)
	{
		if (connect(socket_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1)
		{
			printf("connect failed:%d\r\n", errno);//失败时可以打印errno
			Sleep(2);
			//	n = send(socket_fd, str, strlen(str), 0);

		}
		else
		{
			is_connected = true;
			n = send(socket_fd, str, strlen(str), 0);
			break;
		}
	}
	return socket_;
}

//输出运动信号
float distance_calculation1(mrmInfo *info)//抛弃索引值小于300的点
{


	//筛选数据类型
	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
	{


		if ((info->msg.detectionList.detections[0].index > 200))
		{
			index = info->msg.detectionList.detections[0].index;
			sendflag = 1;
		}


		if (sendflag)
		{
			//距离值
			distance = ((float)index * 61) * 3 / 20000;
			printf("%f\n", distance);

		}
		return distance;
	}


}

//原始数据
float distance_calculation2(mrmInfo *info)//抛弃索引值小于300的点
{
	int i;


	if (info->msg.scanInfo.msgType == MRM_FULL_SCAN_INFO)
	{
		for (i = 0; i < info->msg.scanInfo.numSamplesTotal; i++)
		{
			printf(" %d/n", info->scan[i]);
		}

	}
	return  info->scan[i];

}

//过滤之后的运动信号
float distance_calculation3(mrmInfo *info) {


	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
	{
		if (count == 2) {//连续过滤掉2个点的时候考虑参考值选择错误，重新选取参考值
			count = 0;
			number = 0;//number为0代表准备接受数据作为参考值

		}

		if (number == 0) {
			if ((info->msg.detectionList.detections[0].index > 200))
			{
				index = info->msg.detectionList.detections[0].index;
				printf("index = %f\n", ((float)index * 61) * 3 / 20000);
				sendflag = 1;
				number = 1;
			}
			else {
				count++;
			}
		}
		else {
			//number为1时根据参考值过滤点
			if ((info->msg.detectionList.detections[0].index > 200) && (abs(info->msg.detectionList.detections[0].index - index) < 70 * (count + 1)))
			{
				index = info->msg.detectionList.detections[0].index;
				sendflag = 1;
				count = 0;//一旦检测到合适的点就将count清0
			}
			else {
				count++;
			}
		}

		if (sendflag)
		{
			distance = ((float)index * 61) * 3 / 20000;//最终发送给服务器的数据
			printf("%f\n", distance);

		}
		return distance;
	}
}


//过滤后的运动信号，并添加预测点
float distance_calculation4(mrmInfo *info) {
	int flag = 0;//用来表示物体时靠近还是远离，以此来预测下一个点
	//1--远离，-1--靠近

	if (info->msg.scanInfo.msgType == MRM_DETECTION_LIST_INFO)
	{
		if (count == 5) {//连续预测掉5个点的时候考虑参考值选择错误，重新选取参考值
			count = 0;
			number = 0;//number为0代表准备接受数据作为参考值

		}

		if (number == 0) {
			if ((info->msg.detectionList.detections[0].index > 200))
			{
				index = info->msg.detectionList.detections[0].index;
				printf("index = %f\n", ((float)index * 61) * 3 / 20000);
				sendflag = 1;
				number = 1;
			}
			else {
				count++;
			}
		}
		else {
			//number为1时根据参考值过滤点

			if (info->msg.detectionList.detections[0].index > 200 && info->msg.detectionList.detections[0].index - index > 0 && info->msg.detectionList.detections[0].index - index < 70) {
				flag = 1;
				index = info->msg.detectionList.detections[0].index;
				count = 0;//一旦检测到合适的点就将count清0
			}
			else if (info->msg.detectionList.detections[0].index > 200 && index - info->msg.detectionList.detections[0].index > 0 && index - info->msg.detectionList.detections[0].index < 70) {
				flag = -1;
				index = info->msg.detectionList.detections[0].index;
				count = 0;//一旦检测到合适的点就将count清0
			}
			else {
				index = 50 * flag + index;
				count++;
			}
			sendflag = 1;



		}

		if (sendflag)
		{
			distance = ((float)index * 61) * 3 / 20000;//最终发送给服务器的数据
			printf("%f\n", distance);

		}
		return distance;
	}
}

//_____________________________________________________________________________
//
// mrmSampleExit - close interfaces and exit sample app
//_____________________________________________________________________________

static void mrmSampleExit(void)
{
	if (connected)
		mrmDisconnect();
	mrmIfClose();
	printf("finish");
	getchar();
	exit(0);
}


//_____________________________________________________________________________
//
// main - sample app entry point
//_____________________________________________________________________________

int main(int argc, char *argv[])
{

	char *radioAddr = DEFAULT_RADIO_IP, *serviceIp;
	int haveRadioAddr = 0;
	int i, mode;
	int userBaseII, userScanStart, userScanStop, userScanCount, userScanInterval, userTxGain, userPrintInfo, userLogging;
	char buf[1024] = { 0 };
	char buffer1[50], buffer2[50], buffer3[50], buffer4[50], buffer5[50], buffer6[50];
	int distance_calculation_mode;
	FILE* r = fopen("配置文件.txt", "r");
	if (r == NULL) {
		printf("文件为空");
		return;
	}

	fgets(buffer1, 50, r);//扫描点数
	fgets(buffer2, 50, r);//端口
	fgets(buffer3, 50, r);//scan stop  影响最大检测距离
	fgets(buffer4, 50, r);//动检测的阈值
	fgets(buffer5, 50, r);//1--运动数据   2--原始数据    3--过滤后的运动数据   
	fgets(buffer6, 50, r);//中心节点的IP地址，端口号默认8888

	distance_calculation_mode = atoi(buffer5);
	int ret = 0;


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

	// Set Defaults
	userBaseII = DEFAULT_BASEII;
	userScanStart = DEFAULT_SCAN_START;
	userScanStop = atoi(buffer3);
	userScanCount = atoi(buffer1);
	userScanInterval = DEFAULT_SCAN_INTERVAL;
	userTxGain = DEFAULT_TX_GAIN;
	userPrintInfo = 0;
	userLogging = 0;


	mrmIf = mrmIfUsb;
	haveRadioAddr = 1;
	haveServiceIp = 1;
	serviceIp = "127.0.0.1";//Service地址，localhost
	radioAddr = buffer2;




	// Now print out what we are doing
	printf("Radio address: %s (%s)\n", radioAddr, mrmIf == mrmIfUsb ? "USB" :
		(mrmIf == mrmIfIp ? "Ethernet" : "Serial"));
	if (haveServiceIp)
		printf("Using MRM service at IP address: %s\n", serviceIp);
	else
		printf("Receiving scans directly from radio (no scan server).\n");

	// If using service, connect to it
	if (haveServiceIp)
	{
		mrm_uint32_t status;

		if (mrmIfInit(mrmIfIp, serviceIp) != OK)
		{
			printf("Failed to connect to service (bad IP address?).\n");
			getchar();
			exit(0);
		}
		// connect to radio
		if ((mrmConnect(mrmIf, radioAddr, &status) != OK) ||
			(status != MRM_SERVER_CONNECTIONSTATUSCODE_CONNECTED))
		{
			printf("Unable to connect to radio through service.\n");
			mrmSampleExit();
		}
		connected = 1;
	}
	else
	{
		// initialize the interface to the RCM
		if (mrmIfInit(mrmIf, radioAddr) != OK)
		{
			printf("Initialization failed.\n");
			getchar();
			exit(0);
		}
	}

	// make sure radio is in active mode
	if (mrmSleepModeGet(&mode) != OK)
	{
		printf("Time out waiting for sleep mode.\n");
		mrmSampleExit();
	}

	// print sleep mode
	printf("Radio sleep mode is %d.\n", mode);
	if (mode != MRM_SLEEP_MODE_ACTIVE)
	{
		printf("Changing sleep mode to Active.\n");
		mrmSleepModeSet(MRM_SLEEP_MODE_ACTIVE);
	}

	// make sure radio is in MRM mode
	if (mrmOpmodeGet(&mode) != OK)
	{
		printf("Time out waiting for mode of operation.\n");
		mrmSampleExit();
	}

	// print radio opmode
	printf("Radio mode of operation is %d.\n", mode);
	if (mode != MRM_OPMODE_MRM)
	{
		printf("Changing radio mode to MRM.\n");
		mrmOpmodeSet(MRM_OPMODE_MRM);
	}

	// retrieve config from MRM
	if (mrmConfigGet(&config) != 0)
	{
		printf("Time out waiting for config confirm.\n");
		mrmSampleExit();
	}

	// modify config with user inputs
	config.baseIntegrationIndex = userBaseII;
	config.scanStartPs = userScanStart;
	config.scanEndPs = userScanStop;
	config.txGain = userTxGain;

	// write updated config to radio
	if (mrmConfigSet(&config) != 0)
	{
		printf("Time out waiting for set config confirm.\n");
		mrmSampleExit();
	}

	// print out configuration
	printf("\nConfiguration:\n");
	printf("\tnodeId: %d\n", config.nodeId);
	printf("\tscanStartPs: %d\n", config.scanStartPs);
	printf("\tscanEndPs: %d\n", config.scanEndPs);
	printf("\tscanResolutionBins: %d\n", config.scanResolutionBins);
	printf("\tbaseIntegrationIndex: %d\n", config.baseIntegrationIndex);
	for (i = 0; i < 4; i++)
	{
		printf("\tsegment %d segmentNumSamples: %d\n", i, config.segmentNumSamples[i]);
		printf("\tsegment %d segmentIntMult: %d\n", i, config.segmentIntMult[i]);
	}
	printf("\tantennaMode: %d\n", config.antennaMode);
	printf("\ttxGain: %d\n", config.txGain);
	printf("\tcodeChannel: %d\n", config.codeChannel);



	//mrmFilterConfigSet(mrmFilterConfig *config)

	if (haveServiceIp)
	{
		// now get the filter config from MRM
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

		filterConfig.detectionListThresholdMult = atoi(buffer4);
		if (mrmFilterConfigSet(&filterConfig) != 0)
		{
			printf("Time out waiting for filter config confirm.\n");
			mrmSampleExit();
		}
		// print out filter configuration
		printf("\nNew Filter Configuration:\n");
		printf("\tfilterFlags: %d\n", filterConfig.filterFlags);
		printf("\tmotionFilterIndex: %d\n", filterConfig.motionFilterIndex);
		printf("\tdetectionListThresholdMult: %d\n", filterConfig.detectionListThresholdMult);
	}

	// retrieve status info from MRM
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
	printf("\tTemperature: %.2f degC\n\n", statusInfo.temperature / 4.0);



	//
	// Ready to start collecting scans...
	//

	// Start scanning on the MRM
	printf("\nScanning with scan count of %d and interval of %d (microseconds)\n", userScanCount, userScanInterval);


	//向UWB发送控制信息，开始扫描
	mrmControl(userScanCount, userScanInterval);



	while (1)
	{
		//连接断开后要自动进行重连
		if (!is_connected)
		{
			socket_fd = connect_socket(buffer6);
			printf("connect success\r\n");
		}

		if (mrmInfoGet(timeoutMs, &info) == -1)
		{
			printf("finish!!!");
			break;
		}


		switch (distance_calculation_mode) {
		case 1:
			distance = distance_calculation1(&info);
			break;
		case 2:
			distance = distance_calculation2(&info);
			break;
		case 3:
			distance = distance_calculation3(&info);
			break;
		case 4:
			distance = distance_calculation4(&info);
			break;
		}

		memset(str, 0, sizeof(str));//清空str
		sprintf(str, "%f", distance);//把float型数据转换成string型
		//用#将数据之间隔开，因为发送速度太快，中心节点有可能会接收到两组连续的数据
		strcat(str, "#");

		if (sendflag == 1)
		{

			n = send(socket_fd, str, strlen(str), 0);
			if (n < 0)
			{
				perror("Fail to send!\n");
				printf("Fail to send!\n");
			}
			else if (n == 0) {
				printf("clinet_postback is not connect\n");
				//return 0;
					//exit(EXIT_FAILURE);
			}
			//printf("send %d bytes : %s\n", n, str);
			sendflag = 0;
		}

	}




	// stop radio
	if (mrmControl(0, 0) != 0)
	{
		printf("Time out waiting for control confirm.\n");
		mrmSampleExit();
	}

	// perform cleanup
	printf("\n\nAll Done!\n");

	mrmSampleExit();
	return 0;
}










