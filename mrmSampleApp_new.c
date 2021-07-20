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
//windows ϵͳͷ�ļ�
#include <signal.h> 
//ctrlc�źţ�linux

#include "mrmIf.h"
#include "mrm.h"

//С���ϵ�ͷ�ļ�
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





//static void my_handler(void) { // linuxϵͳ�³���ر�ʱִ�иú���
//	mrmSampleExit();
//}



//BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);

//BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
//{
//	// ����̨��Ҫ���ر�
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

#define DEFAULT_SCAN_COUNT			1000//�ĵ�����д�� 65535��һֱ����
#define DEFAULT_SCAN_INTERVAL		125000
#define DEFAULT_TX_GAIN				63

static int haveServiceIp, connected;
int sum = 0, ave = 0, count = 0, number = 0;
float distance = 0.0;
char recvbuf[N] = { 0 };        //����������յ�������
char InputString[N] = { 0 };    //����������յ�����Ч���ݰ�
int NewLineReceived = 0;      //ǰһ�����ݽ�����־
char ReturnTemp[N] = { 0 };     //�洢����ֵ
unsigned char g_frontservopos = 90;
int  userScanCount = 250, userScanInterval;
mrmInfo info;
int timeoutMs, recvflag=0 ,scanflag=0;
//�����־λ��flag=0ʱֹͣ��flag=1ʱ������flag=2ʱ����
FILE *fp = NULL;

// ��ʼTCPδ����
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
		memset(recvbuf, 0, sizeof(recvbuf));//�������
		n = recv(socket, recvbuf, sizeof(recvbuf), 0);//�������ݲ�����recvbuf�У����û�յ����ݾ��������⡣
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
		//���ݴ��������С����uwb�ò���
		/*Data_Pack();
		if (NewLineReceived == 1)
		{
			tcp_data_parse();
		}*/


		//0--��ͣ��1--��ʼ��2--������-1--�������
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
	pthread_exit(NULL);//�Ͽ����Ӿ��˳��߳�
}
/**
* Function       itoa
* @author        Danny
* @date          2017.08.16
* @brief         ��������ת��Ϊ�ַ�
* @param[in]     void
* @retval        void
* @par History   ��
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
* @brief         ���ɼ��Ĵ���������ͨ��tcp�������λ����ʾ
* @param[in]     void
* @retval        void
* @par History   ��
*/

//char *tcp_data_postback()
//{
//	//С���������������ɼ�����Ϣ������λ����ʾ
//	//�����ʽ��:
//	//    ������ ��ѹ  �Ҷ�  Ѳ��  ������� Ѱ��
//	//$4WD,CSB120,PV8.3,GS214,LF1011,HW11,GM11#
//	char *p = ReturnTemp;
//	char str[25];
//	float distance;
//	memset(ReturnTemp, 0, sizeof(ReturnTemp));
//	//������
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

			memset(str, 0, sizeof(str));//���str
			sprintf(str, "%a.bf", distance);//��float������ת����string��
			//strcpy(str, distance);//�ַ������ƣ���tcp_data_postback()�������Ƶ�str��ȥ��
			//puts(str);//����ַ��������С�
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
			sleep(0.5);//�������ܴ󣬲��ܵȴ�5��
		}

		if (recvflag == -1)//�������
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
	//���������׽���
	int socket_ = 0;
	//����ṹ��洢�������˵�ַ��Ϣ
	struct sockaddr_in server_addr;
	//ͨ��socket���������׽���
	socket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (socket < 0)
	{
		perror("Fail to socket");
		exit(EXIT_FAILURE);
	}
	//����������ip��ַ�Ͷ˿�
	memset(&server_addr, 0, sizeof(server_addr));//���
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8888);
	server_addr.sin_addr.s_addr = inet_addr("192.168.50.63");
	printf("Connecting...\r\n");
	while (!is_connected)
	{
		if (connect(socket_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1)
		{
			printf("connect failed:%d\r\n", errno);//ʧ��ʱ���Դ�ӡerrno
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
        case MRM_DETECTION_LIST_INFO://��Ҫ������������е�index
            // print number of detections and index and magnitude of 1st detection
			if (printInfo)
                printf("DETECTION_LIST_INFO: msgId %d, numDetections %d, 1st detection index: %d, 1st detection magnitude: %d\n",
                        info->msg.detectionList.msgId,
                        info->msg.detectionList.numDetections,
                        info->msg.detectionList.detections[0].index,
                        info->msg.detectionList.detections[0].magnitude);
			//if (count == 1)//����̫�����ڵ�һ�ε�����
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
			//distance = (ave * 64 - 10000) * 3 / 20000;//���շ��͸�������������
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

float distance_calculation1(mrmInfo *info ,FILE *fp)//����ǰ30���㣬�Լ�����ֵС��300�ĵ�
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

		distance = ((float)ave * 61 ) * 3 / 20000;//���շ��͸�������������
		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
		printf("sum=%d,ave=%d,distance= %f\n",sum,ave, distance);
	
		fprintf(fp, "1, %d, %d, %f\n", count,info->msg.detectionList.detections[0].index,distance);
		return distance;
	}
}

float distance_calculation2(mrmInfo *info, FILE *fp)   //����ǰ��ʮ�����Լ�����ֵС��300�ĵ㣬��ʮ����ʮ֮�䲻ɸѡֱ�������ȡƽ����Ϊ��ʼֵ����ʮ֮�������һ��ֵɸѡ�㲢���ϸ��¾�ֵ
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

			distance = ((float)ave * 61 ) * 3 / 20000;//���շ��͸�������������
		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
		printf("count=%d,ave=%d,distance= %f\n", count, ave, distance);
		fprintf(fp, "2, %d,%d,%d, %d, %f\n", count,sum,ave, info->msg.detectionList.detections[0].index, distance);
		return distance;
	}
}

//float distance_calculation3(mrmInfo *info, FILE *fp)//����ǰ��ʮ�����Լ�����ֵС��300�ĵ㣬����ʮһ����ֱ�������֮��ĵ�����һ����Ĳ�ֵ������200
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
//		//	distance = ((float)ave * 61 ) * 3 / 20000;//���շ��͸�������������
//		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//		printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//		fprintf(fp, "3, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//		return distance;
//	}
//}
//����uwb
//float distance_calculation4(mrmInfo *info, FILE *fp)//��ȥ����ֵС��300�ĵ㲢��¼����500�ĵ�ĸ�������ʮ��ֵ����6��ֵ������500������Ϊ��⿪ʼ�����Ը�ʮ����ľ�ֵ��ΪĿ��ĳ�ʼλ��
//{                                    //֮������ʱ��δ��⵽����300�ĵ㣬���»ص�δ����״̬
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
//		else   //�ѱ�����
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
//		//	distance = ((float)ave * 61 ) * 3 / 20000;//���շ��͸�������������
//		//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//		printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//		fprintf(fp, "4, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//		return distance;
//}

//float distance_calculation5(mrmInfo *info, FILE *fp)//��ȥ����ֵС��300�ĵ㲢��¼����500�ĵ�ĸ�������ʮ��ֵ����6��ֵ������500������Ϊ��⿪ʼ�����Ը�ʮ����ľ�ֵ��ΪĿ��ĳ�ʼλ��
//{                                    //֮������ʱ��δ��⵽����300�ĵ㣬���»ص�δ����״̬
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
//		else   //�ѱ�����
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
//	//	distance = ((float)ave * 61 ) * 3 / 20000;//���շ��͸�������������
//	//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//	printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//	fprintf(fp, "5, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//	return distance;
//}


//float distance_calculation6(mrmInfo *info, FILE *fp)//��ȥ����ֵС��300�ĵ㲢��¼����500�ĵ�ĸ�������ʮ��ֵ����6��ֵ������500������Ϊ��⿪ʼ�����Ը�ʮ����ľ�ֵ��ΪĿ��ĳ�ʼλ��
//{                                    //֮������ʱ��δ��⵽����300�ĵ㣬���»ص�δ����״̬
//	int flag = 0;  //�ж��Ƿ��ѵı�־λ
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
//		else   //�ѱ�����
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
//	//	distance = ((float)ave * 61 ) * 3 / 20000;//���շ��͸�������������
//	//distance = ((float)info->msg.detectionList.detections[0].index * 61) * 3 / 20000;
//	printf("sum=%d,ave=%d,distance= %f\n", sum, ave, distance);
//	fprintf(fp, "6, %d,%d, %d, %f\n", count, ave, info->msg.detectionList.detections[0].index, distance);
//	return distance;
//}

void send_data(float data)
{
	int n = 0;
	char str[1024] = { 0 };

	memset(str, 0, sizeof(str));//���str
	sprintf(str, "%a.bf", data);//��float������ת����string��
	//strcpy(str, distance);//�ַ������ƣ���tcp_data_postback()�������Ƶ�str��ȥ��
	//puts(str);//����ַ��������С�
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
	sleep(0.5);//�������ܴ󣬲��ܵȴ�5��
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
	//�����߳�id���ֱ����ڷ�����

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
	//   Need to add code to validate��֤ʵ�� command line arguments
	//

	// process command line arguments
	//if (argc < 3) {
	//	usage();
	//	exit(0);
	//}
	                                 //����ʱ���Ըı�Ĭ�ϲ���
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

	//			// Base Integration Index    //�����ۺ�ָ��
	//			case 'b':
	//				userBaseII = atoi(argv[i++]);
	//				break;

	//			// Scan Start                    //ɨ�跶Χ
	//			case 's':
	//				userScanStart = atoi(argv[i++]);
	//				break;
	//				
	//			// Scan Stop
	//			case 'p':
	//				userScanStop = atoi(argv[i++]);
	//				break;

	//			// Scan Count   //Ĭ����5
	//			case 'c':
	//				userScanCount = atoi(argv[i++]);
	//				break;

	//			// Scan Interval   //ɨ����
	//			case 'i':
	//				userScanInterval = atoi(argv[i++]);
	//				break;

	//			// TX Gain     //���棬���63
	//			case 'g':
	//				userTxGain = atoi(argv[i++]);
	//				break;

 //               // Service IP address
 //               case 'm':
 //                   haveServiceIp = 1;
 //                   serviceIp = argv[i++];
 //                   break;

	//			// Print Info Messages       //��ӡ��Ϣ�ı�־λ��Ĭ��Ϊ1
	//			case 'x':
	//				userPrintInfo = atoi(argv[i++]);
	//				break;
	//				
	//			// Log file                      //д���ļ��ı�־λ��Ĭ��Ϊ0
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
      // usage();                //��ӡ���б�

    // Now print out what we are doing
    printf("Radio address: %s (%s)\n", radioAddr, mrmIf == mrmIfUsb ? "USB" :    //��ӡ�Լ���ip��ַ�����ӷ�ʽ
            (mrmIf == mrmIfIp ? "Ethernet" : "Serial"));
    if (haveServiceIp)   //��̬���ͱ���                  �ж��Ƿ����ӷ�����              ��
        printf("Using MRM service at IP address: %s\n",serviceIp);                          // char ��*    ��   ������ֵ����
    else
        printf("Receiving scans directly from radio (no scan server).\n");

    // If using service, connect to it
    if (haveServiceIp)
    {
        mrm_uint32_t status;

        if (mrmIfInit(mrmIfIp, serviceIp) != OK)      //��ʼ��
        {
            printf("Failed to connect to service (bad IP address?).\n");
            exit(0);
        }
        // connect to radio
        if ((mrmConnect(mrmIf, radioAddr, &status) != OK) ||      //����      mrmif����������
                (status != MRM_SERVER_CONNECTIONSTATUSCODE_CONNECTED))
        {
            printf("Unable to connect to radio through service.\n");
            mrmSampleExit();//�˳�
        }
        connected = 1;   //���ӱ�־λ
    } else
    {
        // initialize the interface to the RCM
        if (mrmIfInit(mrmIf, radioAddr) != OK)   //�������ӷ�ʽ�Ĳ�ͬ�����г�ʼ��
        {
            printf("Initialization failed.\n");
            exit(0);
        }
    }

    // make sure radio is in active mode
    if (mrmSleepModeGet(&mode) != OK)              //��ȡ�״�Ĺ���ģʽ�����߻��ǻ���
    {
        printf("Time out waiting for sleep mode.\n");   //��ȡʧ��
        mrmSampleExit();
    }

	// print sleep mode
    printf("Radio sleep mode is %d.\n", mode);        
    if (mode != MRM_SLEEP_MODE_ACTIVE)
    {
        printf("Changing sleep mode to Active.\n");                 //���������ģʽ����
        mrmSleepModeSet(MRM_SLEEP_MODE_ACTIVE);
    }

    // make sure radio is in MRM mode
    if (mrmOpmodeGet(&mode) != OK)             //��ȡ����״̬
    {
        printf("Time out waiting for mode of operation.\n");
        mrmSampleExit();
    }

	// print radio opmode
    printf("Radio mode of operation is %d.\n", mode);
    if (mode != MRM_OPMODE_MRM)
    {
        printf("Changing radio mode to MRM.\n");//�������MRM����תΪMRM
        mrmOpmodeSet(MRM_OPMODE_MRM);
    }

    // retrieve config from MRM    ����MRM��������Ϣ
    if (mrmConfigGet(&config) != 0)
    {
        printf("Time out waiting for config confirm.\n");
        mrmSampleExit();
    }

	// modify config with user inputs     ʹ���û������޸�����
	config.baseIntegrationIndex = userBaseII;
	config.scanStartPs = userScanStart;
	config.scanEndPs = userScanStop;
	config.txGain = userTxGain;

	// write updated config to radio           //д���µ�����
	if (mrmConfigSet(&config) != 0)
	{
		printf("Time out waiting for set config confirm.\n");
		mrmSampleExit();
	}

    // print out configuration              ��ӡ�µ����
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
        // now get the filter config from MRM                     ��ȡMRM���˲�����Ϣ
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

    // retrieve status info from MRM                  ����״̬��Ϣ
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

	// start logging    д�����ļ���״̬�ͱ�ͷ
	if (userLogging)
	{
		sprintf(logFileName, "%sMRM_Scans.csv", userLogPrefix);  //д���ļ���
		//printf("\nLogging to file %s\n\n", logFileName);
		fp = fopen(logFileName, "w");     //���ļ�
		
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
          // if (haveServiceIp)    //������Ҫ�����������
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
    //if (mrmControl(userScanCount, userScanInterval) != 0)   //����MRMɨ�裬�����߳����棬���յ��źžͲ�ɨ��
    //{
    //    printf("Time out waiting for control confirm.\n");
    //    mrmSampleExit();
    //}
	//signal(SIGINT, my_handler);//linux����رպ�ִ��my_handler����
	//while (mrmInfoGet(timeoutMs, &info) == 0)    //��MRM�ռ�ɨ�����ռ���б�
	//{
	//	
	//	processInfo(&info, fp, userPrintInfo);
	//	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	//}//���ļ���д������     
	////�������޸ģ�ͨ��ip���ͳ�ȥ




	int socket_fd = 0;
	//if (!is_connected)
	//{
		socket_fd = connect_socket();
	//	// ��һ�����ӳɹ�
	//	printf("connect success\r\n");
	//	is_connected = true;
	//	// ����������Ϣ���߳�
	//	// socket_fd�Ǹ��̺߳���receive_message���ݵĲ���
	//	ret = pthread_create(&tid1, NULL, receive_message, socket_fd);
	//	if (ret != 0)
	//	{
	//		fprintf(stderr, "Fail to pthread_create : %s\n", strerror(errno));
	//		exit(EXIT_FAILURE);
	//	}

	//	// ����������Ϣ���߳�
	//	// socket_fd�Ǹ��̺߳���send_message���ݵĲ���
	//	ret = pthread_create(&tid2, NULL, send_message, socket_fd);
	//	if (ret != 0)
	//	{
	//		fprintf(stderr, "Fail to pthread_create : %s\n", strerror(errno));
	//		exit(EXIT_FAILURE);
	//	}
	//	// ����ʽ�̣߳��߳̽�����ʱ����ϵͳ�Զ����ͷ���Դ
	//	pthread_detach(tid1);
	//	pthread_detach(tid2);
	//}
	//else
	//{
	//	//����Ѿ����ӣ�����Ҫ���κδ���С���̶߳��Ѿ�����������
	//}


	while(mrmInfoGet(timeoutMs, &info)==0)
	{
		
		distance=distance_calculation1(&info,fp);
		

				mrmInfoGet(timeoutMs, &info) ;
				distance = distance_calculation1(&info, fp);
				int n = 0;
				char str[1024] = { 0 };

				memset(str, 0, sizeof(str));//���str
				sprintf(str, "%a.bf", distance);//��float������ת����string��
				//strcpy(str, distance);//�ַ������ƣ���tcp_data_postback()�������Ƶ�str��ȥ��
				//puts(str);//����ַ��������С�
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
				sleep(0.5);//�������ܴ󣬲��ܵȴ�5��

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

