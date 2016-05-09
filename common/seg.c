
#include "seg.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#define BEGIN_CHAR0 '!'
#define BEGIN_CHAR1 '&'
#define END_CHAR0   '!'
#define END_CHAR1   '#'

/*
 * Wrapped send
 * On error,  -1 is returned
 * On success, 1 is returned
 */
static inline int Send(int conn, char *buffer, size_t size) {
	// calling send
	if (send(conn, buffer, size, 0) <= 0)
    {
        printf("send error! errno: %d\n", errno);
        return -1;
    }
    else
    {
        return 1;
    }
}

/*
 * Wrapped recv
 * On error,  -1 is returned
 * On success, 1 is returned
 */
static inline int Recv(int conn, char *buf, size_t size) {
	// calling send
	if (recv(conn, buf, size, 0) <= 0)
    {
        printf("recv error! errno: %d\n", errno);
        exit(0);
        return -1;
    }
    else
    {
        return 1;
    }
}

/*
 * Places the bytes between BEGIN_CHAR01 and END_CHAR01 in the buffer
 * On error,  -1 is returned.
 * On success, 1 is returned.
 */
 #define PKTSTART1 0	// PKTSTART1 -- 起点 
 #define PKTSTART2 1    // PKTSTART2 -- 接收到'!', 期待'&' 
 #define PKTRECV   2    // PKTRECV -- 接收到'&', 开始接收数据
 #define PKTSTOP1  3    // PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
static inline int recv2buf(char *buffer, int buf_len, int conn) { 
	int state = PKTSTART1;
	int bytes_in_buf = 0; // how many bytes are there in the buffer
	char c;

	// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
	while (Recv(conn, &c, sizeof(char)) > 0) {
		switch(state) {
			case PKTSTART1: if (c == BEGIN_CHAR0) state = PKTSTART2; break;

			case PKTSTART2: if (c == BEGIN_CHAR1) state = PKTRECV;
                            else {
								printf("No '%c' following '%c' truning to PKTSTART1\n", BEGIN_CHAR1, BEGIN_CHAR0);
								state = PKTSTART1;
							}  
							break;

			case PKTRECV:   if (c == END_CHAR0) 
								state = PKTSTOP1;
							else if (bytes_in_buf < buf_len) 
								buffer[bytes_in_buf++] = c;
							else {
								printf("Too many bytes to recv, now bytes_in_buf is: %d\n", bytes_in_buf);
								return -1;
							}
							break;

			case PKTSTOP1:	if (c == END_CHAR1) {
								printf("Received a packet successfully. \n");
								return 1;
							}
							else if(bytes_in_buf < buf_len - 1) { // END_CHAR0 appeared in the data
								buffer[bytes_in_buf++] = END_CHAR0;
								buffer[bytes_in_buf++] = c;
								state = PKTRECV;
							}
							else {
								printf("No '%c' following '%c' ", END_CHAR1, END_CHAR0);
								return -1;
							}
							break;

			default: printf("recv2buf, Unknown state\n");
					 return -1;
		}
	}

	return -1; // recv error
}

/*
 * Fill the sending buffer
 */
static inline void  nodeSegToBuf(char *buffer, int nodeID, seg_t* segPtr) {
	buffer[0] = BEGIN_CHAR0;
	buffer[1] = BEGIN_CHAR1;

	sendseg_arg_t *p = (sendseg_arg_t *)(&(buffer[2]));
	p->nodeID = nodeID;
	memcpy(&(p->seg), segPtr, sizeof(seg_t));

	buffer[2 + sizeof(sendseg_arg_t)] = END_CHAR0;
	buffer[3 + sizeof(sendseg_arg_t)] = END_CHAR1;
}

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
	segPtr->header.checksum = 0;
	segPtr->header.checksum = checksum(segPtr);

	char *buffer = (char *)malloc(sizeof(sendseg_arg_t) + 4);
	nodeSegToBuf(buffer, dest_nodeID, segPtr);
	
	// send it
	int ret = Send(sip_conn, buffer, sizeof(buffer));
	
	free(buffer);
	return ret;	
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回0, 丢包则返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
	char *buffer = (char *)malloc(sizeof(sendseg_arg_t));
	int ret = -1;
	
	if (recv2buf(buffer, sizeof(sendseg_arg_t), sip_conn) > 0) {
		sendseg_arg_t *p = (sendseg_arg_t *)buffer;
		*src_nodeID = p->nodeID;
		memcpy(segPtr, &(p->seg), sizeof(seg_t));
        
		ret = 0;
	} 

	// simulating the seg-lost situation
	if (seglost(segPtr) <= 0)
		ret = 1;
	if (checkchecksum(segPtr) <= 0)
		ret = 1;

	free(buffer);
  	return ret;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
	char *buffer = (char *)malloc(sizeof(sendseg_arg_t));
	int ret = -1;
	
	if (recv2buf(buffer, sizeof(sendseg_arg_t), stcp_conn) > 0) {
		sendseg_arg_t *p = (sendseg_arg_t *)buffer;
		*dest_nodeID = p->nodeID;
		memcpy(segPtr, &(p->seg), sizeof(seg_t));
        
		ret = 1;
	} 
	
	free(buffer);
  	return ret;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
	char *buffer = (char *)malloc(sizeof(sendseg_arg_t) + 4);
	nodeSegToBuf(buffer, src_nodeID, segPtr);
	
	// send it
	int ret = Send(stcp_conn, buffer, sizeof(buffer));
	
	free(buffer);
	return ret;	
}

int seglost(seg_t* segPtr) {
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
      return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
		//	//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
	segment->header.checksum = 0;
	int mychecksum = sizeof(stcp_hdr_t) + strlen(segment->data);
	unsigned char *temp  = (unsigned char*)malloc(mychecksum);
	memcpy(temp,segment,mychecksum);

	unsigned long cksum=0;
	while(mychecksum>1)
	{
	cksum+=*(unsigned short int *)temp;
	temp +=2;
	
	mychecksum-=sizeof(unsigned short int);
	}
	if(mychecksum)
	{
	cksum+=*(unsigned char *)temp;
	}
	while (cksum>>16)
		cksum=(cksum>>16)+(cksum & 0xffff);
	printf("cksum %d\n",(unsigned short int)(~cksum));
	return (unsigned short int)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{       
	int checksum = sizeof(stcp_hdr_t) + strlen(segment->data);
	unsigned char *temp = (unsigned char*)malloc(checksum);
	memcpy(temp,segment,checksum);

	unsigned long cksum=0;
	while(checksum>1)
	{
	//printf("!%d\n",*(unsigned short int*)temp);
	cksum+=*(unsigned short int *)temp;
	temp += 2;
	//printf("%d\n",cksum);
	checksum-=sizeof(unsigned short int);
	}
	if(checksum)
	{
	cksum+=*(unsigned char *)temp;
	}
	
	while (cksum>>16)
		cksum=(cksum>>16)+(cksum & 0xffff);
	printf("checkchecksum %u\n",~cksum);
  	if( (unsigned short)(~cksum) != 0 )
	{
		// TODO:
		printf("check sum error!\n");

		return -1;
		//return 0;
	}
	else
	{
		return 0;
	}
}
