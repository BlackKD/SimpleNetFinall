// 文件名 pkt.c
// 创建日期: 2015年

#include "pkt.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>

#define BEGIN_CHAR0 '!'
#define BEGIN_CHAR1 '&'
#define END_CHAR0   '!'
#define END_CHAR1   '#'

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
 * Fill the sending buffer for son
 */
static inline void build_sonSendBuf(char *buffer, int nextNodeID, sip_pkt_t* pkt) {
	buffer[0] = BEGIN_CHAR0;
	buffer[1] = BEGIN_CHAR1;

	sendpkt_arg_t *p = (sendpkt_arg_t *)(&(buffer[2]));
	p->nextNodeID = nextNodeID;
	memcpy(&(p->pkt), pkt, sizeof(sip_pkt_t));

	buffer[2 + sizeof(sendpkt_arg_t)] = END_CHAR0;
	buffer[3 + sizeof(sendpkt_arg_t)] = END_CHAR1;
}

/*
 * Fill the sending buffer for sip
 */
static inline void build_sipSendBuf(char *buffer, sip_pkt_t* pkt) {
 	buffer[0] = BEGIN_CHAR0;
	buffer[1] = BEGIN_CHAR1;

	sip_pkt_t *p = (sip_pkt_t *)(&(buffer[2]));
	memcpy(p, pkt, sizeof(sip_pkt_t));

	buffer[2 + sizeof(sip_pkt_t)] = END_CHAR0;
	buffer[3 + sizeof(sip_pkt_t)] = END_CHAR1;
 }
// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	char buffer[1508];
	build_sonSendBuf(buffer, nextNodeID, pkt);
    sendpkt_arg_t *p= (sendpkt_arg_t *)(buffer + 2);
    printf("nextNodeID: %d", p->nextNodeID);
    printf("myId: %d, nextId: %d\n", p->pkt.header.src_nodeID, p->pkt.header.dest_nodeID);
	// send it
	return Send(son_conn, buffer, sizeof(buffer));
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	printf("In son_recvpkt ");
	
	char *buf = (char *)pkt;
	if (recv2buf(buf, sizeof(sip_pkt_t), son_conn) > 0)
		return 1;
	else 
		return -1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode, int sip_conn)
{
	printf("In getpktToSend\n");
	char buffer[1508];

	if (recv2buf(buffer, sizeof(sip_pkt_t) + sizeof(int), sip_conn) > 0) {
		sendpkt_arg_t *p = (sendpkt_arg_t *)buffer;
		*nextNode = p->nextNodeID;
		memcpy(pkt, &(p->pkt), sizeof(sip_pkt_t));
        printf("ready myid %d  %d in buffer, next :%d\n",p->pkt.header.src_nodeID, pkt->header.src_nodeID,*nextNode);
		return 1;
	}
	else 
		return 0;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
    char *buffer = (char *)malloc(1504);
    memset(buffer,0,1504);
	//char buffer[1504];
	build_sipSendBuf(buffer, pkt);

	// send it
	return Send(sip_conn, buffer, 1504);
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
    char *buffer = (char *)malloc(1504);
    memset(buffer,0,1504);

	//char buffer[1504];
	build_sipSendBuf(buffer, pkt);
    sip_pkt_t * p = (sip_pkt_t *)(buffer+2);
    printf("rec myid %d  in buffer\n",p->header.src_nodeID);

	// send it
	return Send(conn, buffer, 1504);
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	printf("In recvpkt ");
	
	char *buf = (char *)pkt;
	if (recv2buf(buf, sizeof(sip_pkt_t), conn) > 0)
		return 1;
	else 
		return -1;
}
