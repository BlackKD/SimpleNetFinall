// �ļ��� pkt.c
// ��������: 2015��

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
 #define PKTSTART1 0	// PKTSTART1 -- ��� 
 #define PKTSTART2 1    // PKTSTART2 -- ���յ�'!', �ڴ�'&' 
 #define PKTRECV   2    // PKTRECV -- ���յ�'&', ��ʼ��������
 #define PKTSTOP1  3    // PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
static inline int recv2buf(char *buffer, int buf_len, int conn) { 
	int state = PKTSTART1;
	int bytes_in_buf = 0; // how many bytes are there in the buffer
	char c;

	// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
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
// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
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

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	printf("In son_recvpkt ");
	
	char *buf = (char *)pkt;
	if (recv2buf(buf, sizeof(sip_pkt_t), son_conn) > 0)
		return 1;
	else 
		return -1;
}

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.
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

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
    char *buffer = (char *)malloc(1504);
    memset(buffer,0,1504);
	//char buffer[1504];
	build_sipSendBuf(buffer, pkt);

	// send it
	return Send(sip_conn, buffer, 1504);
}

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	printf("In recvpkt ");
	
	char *buf = (char *)pkt;
	if (recv2buf(buf, sizeof(sip_pkt_t), conn) > 0)
		return 1;
	else 
		return -1;
}
