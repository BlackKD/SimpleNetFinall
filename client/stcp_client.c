//文件名: client/stcp_client.c
//
//描述: 这个文件包含STCP客户端接口实现 
//
//创建日期: 2015年

#include <sys/types.h>	/* basic system data types */
#include <sys/socket.h>	/* basic socket definitions */
#include <netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>	/* inet(3) functions */
#include <errno.h>
#include <fcntl.h>		/* for nonblocking */
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>	/* for S_xxx file mode constants */
#include <sys/uio.h>		/* for iovec{} and readv/writev */
#include <unistd.h>
#include <sys/wait.h>
#include <sys/un.h>	
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "../topology/topology.h"
#include "stcp_client.h"

#define TABLE_LEN MAX_TRANSPORT_CONNECTIONS
static client_tcb_t *ctcb_table[TABLE_LEN];
static pthread_mutex_t tcb_mutex[TABLE_LEN];
int connfd;

client_tcb_t *create_ctcb(unsigned client_port) {
	client_tcb_t *p = (client_tcb_t *)malloc(sizeof(client_tcb_t));
	if( p != NULL ) {
		p->client_portNum = client_port;
		p->state = CLOSED;
		p->next_seqNum = 0;
		p->unAck_segNum = 0;

		//p->client_nodeID = topology_getMyNodeID();

		p->sendBufunAckHead  = NULL;
		p->sendBufunAckTail  = NULL;
		p->sendBufunSentHead = NULL;
		p->sendBufunSentTail = NULL;
	}
	else 
		printf("create_ctcb error! malloc error.\n");

	return p;
}


segBuf_t *create_sbuf() {
	segBuf_t *p = (segBuf_t *)malloc(sizeof(segBuf_t));
	if(p != NULL) {
		p->sentTime = 0;
		p->next = NULL;
		memset(&(p->seg), 0, sizeof(seg_t));
	} 
	else 
		printf("create_sbuf error.\n");

	return p;
}
 
 /*
  * Find thd tcb whose client_portNum is des_port;
  * On success, the tcb's index is returned;
  * On failure, -1 is returned.
  */
int search_ctcb(unsigned dest_port) {
	int i;
	for(i = 0; i < TABLE_LEN; i ++) {
		if(ctcb_table[i] != NULL && ctcb_table[i]->client_portNum == dest_port)
			return i;
	} 
	return -1;
}

/*
 * Send segs, and change state
 * On error, 0 is returned
 * On success, 1 is returned
 */
int sendseg(int conn, client_tcb_t *p, seg_t *segPtr) {
	// check whether the connection is okay
	struct sockaddr_in sa;
	int len = sizeof(sa);
	if((getpeername(conn, (struct sockaddr *)&sa, &len) < 0) && errno == ENOTCONN) {
		printf("The other side is closed.\n");
		return 0;
	}

	// send the segment
	if( !sip_sendseg(conn, p->server_nodeID, segPtr) )
		return 0;
	
	int seg_type = segPtr->header.type;
	
	// change state
	switch(p->state) {
	 	case CLOSED: {
			if(seg_type == SYN)
				p->state = SYNSENT;
			else 
				printf("Trying to send not-a-SYN seg during CLOSED.\n");
			break;
		}	
	 	case CONNECTED: {
			if(seg_type == FIN)
				p->state = FINWAIT;
			else if(seg_type != DATA)
				printf("Trying to send a seg besides FIN or DATA.\n");
			break;
		}
		case SYNSENT: {
			if(seg_type != SYN)
				printf("Trying to send a Not-SYN while in SYNSENT.\n");
			break;
		}
		case FINWAIT: {
			if(seg_type != FIN)
				printf("Trying to send a Not-FIN while in FINWAIT.\n");
			break;
		}
		default: 
			printf("Trying to send a seg while in Unknown state.\n");
	} 

	return 1;
}

/*
 * If the ack is not received in  time_out*max_times microseconds, then return 1
 * else return 0
 */
int wait_syn_ack(int conn, int sock, seg_t *segPtr, int time_out, int max_times) { 
	printf("wait_syn_ack\n");

	int seconds = 0;
	int times = 0;
	
	client_tcb_t *tcb_p = ctcb_table[sock];
	
	do {
		pthread_mutex_lock(&(tcb_mutex[sock]));
		if (tcb_p->state == CONNECTED) { 
			pthread_mutex_unlock(&(tcb_mutex[sock]));
			break;
		}
		pthread_mutex_unlock(&(tcb_mutex[sock])); 

		// check time
		if(seconds > time_out) { 
			// fail
			if(times >= max_times ) {
				printf("give up transmiting SYN\n");
				return 0;
			}
			// retransmit 
			printf("retransmit SYN times: %d, dest_port: %d\n", times, segPtr->header.dest_port);
			if( !sendseg(conn, tcb_p, segPtr) ) return 0;
			seconds = 0;
			times ++;
		}
		// make this thread sleep 1 microsecond
		seconds ++;
		usleep(1);
	} while(1);

	return 1;
}

/*
 * If the ack is received in time_out microseconds*max_times, then return 1
 * else return -1
 */
int wait_fin_ack(int conn, int sock, seg_t *segPtr, int time_out, int max_times) {	
	printf("wait_fin_ack\n");

	int seconds = 0;
	int times = 0;
	client_tcb_t *tcb_p = ctcb_table[sock];
	
	do {
		pthread_mutex_lock(&(tcb_mutex[sock]));
		if (tcb_p->state == CLOSED) { 
			pthread_mutex_unlock(&(tcb_mutex[sock]));
			break;
		}
		pthread_mutex_unlock(&(tcb_mutex[sock]));

		// check time
		if(seconds > time_out) { 
			// fail
			if(times >= max_times ) {
				printf("give up transmiting FIN\n");
				return 0;
			}
			// retransmit 
			printf("retransmit FIN times: %d, dest_port: %d\n", times, segPtr->header.dest_port);
			if( !sendseg(conn, tcb_p, segPtr) ) return 0;
			seconds = 0;
			times ++;
		}
		// make this thread sleep 1 microsecond
		seconds ++;
		usleep(1);
	} while(1); 

	return 1;
}

void handle_synack(seg_t *p) {
	printf("handle_synack\n");

	stcp_hdr_t *h = &(p->header);
	int sock = search_ctcb(h->dest_port);
	if(sock < 0) return;

	client_tcb_t *tcb = ctcb_table[sock];
	
	pthread_mutex_lock(&(tcb_mutex[sock]));
	if(tcb->state == SYNSENT && h->ack_num == 1) {
		printf("port %d state to CONNECTED\n", h->dest_port);
		tcb->state = CONNECTED;
	}
	pthread_mutex_unlock(&(tcb_mutex[sock]));
}

void handle_finack(seg_t *p) {
	printf("handle_finack\n");

	stcp_hdr_t *h = &(p->header);
	int sock = search_ctcb(h->dest_port);
	if(sock < 0) return;

	client_tcb_t *tcb = ctcb_table[sock];
	
	pthread_mutex_lock(&(tcb_mutex[sock]));
	if(tcb->state == FINWAIT && h->ack_num == tcb->next_seqNum) {
		printf("port %d state to CLOSED\n", h->dest_port);
		tcb->state = CLOSED;
	}
	pthread_mutex_unlock(&(tcb_mutex[sock]));
}

int sendData(client_tcb_t *tcb, segBuf_t *sb);

int sendUnsent(client_tcb_t *tcb) {
	printf("send the unsent segs, if any\n");
	if(tcb == NULL) return 0;

	while(tcb->sendBufunSentHead != NULL && tcb->unAck_segNum < GBN_WINDOW) {
		segBuf_t *unsent = tcb->sendBufunSentHead;
		tcb->sendBufunSentHead = unsent->next;
		printf("sending an unsent seg, seq_num: %d\n", unsent->seg.header.seq_num);
		if( !sendData(tcb, unsent) ) 
			return 0;
	}
	return 0;
}

void handle_dataack(seg_t *p) {
	printf("handle_dataack, the acked_num is: %d\n", p->header.ack_num);

	stcp_hdr_t *h = &(p->header);
	int sock = search_ctcb(h->dest_port);
	if(sock < 0) return;

	client_tcb_t *tcb = ctcb_table[sock];

	printf("handle_dataack before lock; ");
	pthread_mutex_lock(tcb->bufMutex);
	printf("handle_dataack locked\n");

	// ack the unacked
	segBuf_t *cur = tcb->sendBufunAckHead;
	while(cur != NULL) {
		// check whether the current node should be removed or not
		printf("ack unack, cur seq_num: %d, the received ack_num: %d\n", (cur->seg).header.seq_num, p->header.ack_num);
		if ((cur->seg).header.seq_num < p->header.ack_num) {
			printf("sockfd:%d, Remove the unacked seq_num: %d, the received acked_num: %d ", 
					sock, (cur->seg).header.seq_num, p->header.ack_num);
			segBuf_t *cur_temp = cur;
			free(cur_temp);
			tcb->unAck_segNum --;
		}
		else break;

		// check next
		cur = cur->next;
	}
	tcb->sendBufunAckHead = cur;
	if (cur == NULL)
		tcb->sendBufunAckTail = NULL; // empty

	// send the unsent
	sendUnsent(tcb);

	pthread_mutex_unlock(tcb->bufMutex);
}

/*
 * Add a segBuf to "sent but not acked" buffer tail;
 * The case that unAck_segNum >= GBN_WINDOW should be handled before
 * On success, 1 will be returned.
 * On error, 0 will be returned.
 */
int add2notackedBuf(client_tcb_t *tcb, segBuf_t *sb) {
	printf("add2notackedBuf\n");
	if(tcb == NULL || sb == NULL) return 0;

	// link to the tail
	if(tcb->sendBufunAckHead == NULL) { 
		// the buffer is NULL
		tcb->sendBufunAckHead = sb;
	}
	else { 
		tcb->sendBufunAckTail->next = sb;
	}
	tcb->sendBufunAckTail = sb;
	sb->next = NULL;

	tcb->unAck_segNum ++;

	return 1;
}

/* 
 * If the length of "sent but not acked" buffer is not less than GBN_WINDOW, then
 * add the seg to the "unsent" buffer tail;
 * else, send it
 * On success, 1 will be returned
 * On error, 0 will be returned
 */
int sendData(client_tcb_t *tcb, segBuf_t *sb) {
	printf("sendData ");
	if(tcb == NULL || sb == NULL) return 0;

	if(tcb->unAck_segNum < GBN_WINDOW) { // send it
		printf("send the DATA directly the seq_num: %d: and ", sb->seg.header.seq_num);
		if( !sendseg(connfd, tcb, &(sb->seg)) ) {
			printf("Send a DATA failed.\n");
			return 0;
		}
		sb->sentTime = 1;
		return add2notackedBuf(tcb, sb);
	}
	else { // add it to the "unsent" buffer
		printf("notAcked buffer is full and add the segBuf to unsent buffer\n");

		if(tcb->sendBufunSentHead == NULL) // empty
			tcb->sendBufunSentHead = sb;
		else {
			tcb->sendBufunSentTail->next = sb;
		}
		tcb->sendBufunSentTail = sb;
		sb->next = NULL;
		sb->sentTime = 0;
	}
	return 1;
}

/*
 * retransmit all the unAcked from head
 */
void retransmitData(client_tcb_t *tcb) {
	// excepction
	if(tcb == NULL) return;

	segBuf_t *remove_head = tcb->sendBufunAckHead;
	// all the segs are going to be removed from this buffer
	tcb->sendBufunAckHead = NULL; 
	tcb->sendBufunAckTail = NULL;
	tcb->unAck_segNum     = 0;

	while(remove_head != NULL) { 
		printf("retransmiting seq_num: %d\n", remove_head->seg.header.seq_num);
		segBuf_t *temp = remove_head;
		remove_head = remove_head->next;
		sendData(tcb, temp);
	}
}

void *checkDataTimeout(void *arg) {
	int i;
	while(1) {
		for(i = 0; i < TABLE_LEN; i ++) {
			client_tcb_t *tcb = ctcb_table[i];
			if(tcb != NULL  && tcb->bufMutex != NULL) {
				pthread_mutex_lock(tcb->bufMutex);
				//printf("checkDataTimeout locked\n");
			
				segBuf_t *sb = tcb->sendBufunAckTail;
				if(sb == NULL) {
					pthread_mutex_unlock(tcb->bufMutex);
				//	printf("checkDataTimeout unlock\n");
					continue;
				}

				sb->sentTime += SENDBUF_POLLING_INTERVAL;
				// if newest unAcked seg timeout, retransmit all the unacked
				// else add all the unacked segs' sentTime
				if(sb->sentTime >= DATA_TIMEOUT) { // 100000
					printf("\n\n\n retransmitting  \n\n\n");
					retransmitData(tcb);
				}
				else {
					while( (sb = sb->next) != NULL ) {
						sb->sentTime += SENDBUF_POLLING_INTERVAL; // 100
						i ++;
					}
				}

				pthread_mutex_unlock(tcb->bufMutex);
				//printf("checkDataTimeout unlock\n");
			}
		}
		usleep(SENDBUF_POLLING_INTERVAL);
	}
	return NULL;

}

void free_sendBuf(client_tcb_t *tcb) {
	segBuf_t *p;
	
	pthread_mutex_lock(tcb->bufMutex);

	// sent but not acked
	p = tcb->sendBufunAckHead;
	while(p != NULL) {
		p = p->next;
		free(p);
	}
	tcb->sendBufunAckHead = NULL;

	// unsent
	p = tcb->sendBufunSentHead;
	while(p != NULL) {
		p = p->next;
		free(p);
	}
	tcb->sendBufunSentHead = NULL;
	tcb->sendBufunSentTail = NULL;

	pthread_mutex_unlock(tcb->bufMutex);

	tcb->bufMutex = NULL;
}
/*面向应用层的接口*/

// stcp客户端初始化
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn) {
	pthread_t tid;
	pthread_t dto;
	int i;
	for(i = 0; i < TABLE_LEN; i ++) {
		ctcb_table[i] = NULL;
		pthread_mutex_init(&(tcb_mutex[i]), NULL);
	}

	connfd = conn;

    pthread_create(&tid, NULL, &seghandler, NULL);
	pthread_create(&dto, NULL, &checkDataTimeout, NULL);

    return;
}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
	printf("stcp_client_sock.\n");

	int i;
	for(i = 0; i < TABLE_LEN; i ++) {
		if( ctcb_table[i] == NULL ) {
			client_tcb_t *p;
			if( (p = create_ctcb(client_port)) != NULL ) {
				ctcb_table[i] = p;
				p->bufMutex = &(tcb_mutex[i]);
				break;
			}
			else {
				i = -1;
				break;
			}
		}
	}

    return i;
}

// 这个函数用于连接服务器. 它以套接字ID, 服务器节点ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器节点ID和服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) {
	printf("stcp_client_connect\n");

	client_tcb_t *p = ctcb_table[sockfd];
	seg_t seg;
	int seq_num;
	if( p == NULL ) {
		printf("connect error! ctcb_table[%d] is NULL.\n", sockfd);
		return -1;
	}
	
	p->server_portNum = server_port;
	p->server_nodeID  = nodeID; 
	seq_num = p->next_seqNum;

	memset(&seg, 0, sizeof(seg_t));
	// set header
	stcp_hdr_t *h = &(seg.header);
	h->src_port  = p->client_portNum;
	h->dest_port = server_port;
	h->seq_num   = seq_num;
	h->ack_num   = 0;
	h->length 	 = 0;
	h->type      = SYN;
	
	// send SYN 
	if( !sendseg(connfd, p, &seg) )
		return -1;

	// wait SYNACK
	if( wait_syn_ack(connfd, sockfd, &seg, SYN_TIMEOUT, SYN_MAX_RETRY) ) {
		p->state = CONNECTED;
		p->next_seqNum = 0;   //DATA's first seq_num is 0
		return 1;
	}
	else {
		p->state = CLOSED;
		return -1;
	}
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目.
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生. 
// 这个函数在成功时返回1，否则返回-1. 
// stcp_client_send是一个非阻塞函数调用.
// 因为用户数据被分片为固定大小的STCP段, 所以一次stcp_client_send调用可能会产生多个segBuf
// 被添加到发送缓冲区链表中. 如果调用成功, 数据就被放入TCB发送缓冲区链表中, 根据滑动窗口的情况,
// 数据可能被传输到网络中, 或在队列中等待传输.
int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
	printf("stcp_client_send\n");

	client_tcb_t *p = ctcb_table[sockfd];
	if(p == NULL) return -1;

	int rest_len = length;
	char *data_begin = (char *)data;
	while( rest_len > 0 ) {
		segBuf_t *sb = create_sbuf();
		if(sb == NULL) 	return -1;

		seg_t *seg = &(sb->seg);
		int cur_send_len = (rest_len >= MAX_SEG_LEN - 1) ? MAX_SEG_LEN - 1 : rest_len;
		printf("rest length: %d\n", rest_len);
		unsigned seq_num = p->next_seqNum;
		p->next_seqNum  += cur_send_len;

		printf("Send Data seq_num: %d\n", seq_num);
		// copy data
		int i;
		for(i = 0; i < cur_send_len; i ++) {
			(seg->data)[i] = *(data_begin ++);
		}
		rest_len -= cur_send_len;
		// set header
		stcp_hdr_t *h = &(seg->header);
		h->src_port  = p->client_portNum;
		h->dest_port = p->server_portNum;
		h->seq_num   = seq_num;
		h->ack_num   = 0;
		h->length 	 = cur_send_len;
		h->type      = DATA;

		// unsentBuffer will handle it
		pthread_mutex_lock(p->bufMutex);
		int ret = sendData(p, sb);	
		pthread_mutex_unlock(p->bufMutex);


		if(!ret) return -1;
	}

	return -1;
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
	printf("stcp_client_disconnect\n");

	client_tcb_t *p = ctcb_table[sockfd];
	seg_t seg;
	int seq_num;
	if(p == NULL) {
		printf("In stcp_client_disconnect: ctcb_table[%d] is NULL.\n", sockfd);
		return -1;
	}

	seq_num = p->next_seqNum ++;
	memset(&seg, 0, sizeof(seg_t));
	// set header
	stcp_hdr_t *h = &(seg.header);
	h->src_port  = p->client_portNum;
	h->dest_port = p->server_portNum;
	h->seq_num   = seq_num;
	h->ack_num   = 0;
	h->length 	 = 0;
	h->type      = FIN;

	// send FIN
	if( !sendseg(connfd, p, &seg) )
		return -1;
	
	// creat a timer and wait FIN ack
	if( wait_fin_ack(connfd, sockfd, &seg, FIN_TIMEOUT, FIN_MAX_RETRY) ) {
		p->state = CLOSED;
		return 1;
	}
  
  	return -1;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
	printf("stcp_client_close\n");
	client_tcb_t *p = ctcb_table[sockfd];
	if(p == NULL) {
		printf("In stcp_client_close: ctcb_table[%d] is NULL.\n", sockfd);
		return -1;
	}
	else {
		free_sendBuf(p);
		free(p);
		ctcb_table[sockfd] = NULL;
		return 1;
	}
}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
	int lost;
	seg_t *seg_p = (seg_t *)malloc(sizeof(seg_t));
	while(1) {
		memset(seg_p, 0, sizeof(seg_t));
		int src_nodeID = 0;
		lost = sip_recvseg(connfd, &src_nodeID, seg_p);

		if( !lost ){
			switch(seg_p->header.type) {
				case SYNACK:  handle_synack(seg_p); break;
				case FINACK:  handle_finack(seg_p); break;
				case DATAACK: handle_dataack(seg_p); break;
				case DATA: 
				case SYN:
				case FIN:
				default:      printf("Un-handled segment type.\n");
			}
		}
		else { // lost

		}
	}

	free(seg_p);
    return 0;
}
