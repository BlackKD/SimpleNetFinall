//文件名: server/stcp_server.c
//
//描述: 这个文件包含STCP服务器接口实现. 
//
//创建日期: 2015年

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"

//声明tcbtable为全局变量
server_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//声明到SIP进程的连接为全局变量
int sip_conn;

/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
void stcp_server_init(int conn) 
{
	    int i = 0;
    for(i = 0; i < 10 ; i++)
    {
        TCBtable[i]=NULL;
    }
    int connection = conn;//针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
    pthread_t id;
    pthread_create(&id,NULL,&seghandler,(void *)&connection);
    printf("stcp_server_init finish!\n");
    return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_server_sock(unsigned int server_port) 
{
	return 0;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
int stcp_server_accept(int sockfd) 
{
	return 0;
}

// 接收来自STCP客户端的数据. 这个函数每隔RECVBUF_POLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	return 0;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_server_close(int sockfd) 
{
	return 0;
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
void* seghandler(void* arg) 
{
    while(1)
    {
        seg_t * mytcpMessage = (seg_t*)malloc(sizeof(seg_t));
        memset(mytcpMessage,0,sizeof(seg_t));
        int j = sip_recvseg(connfd, mytcpMessage);

        if(j == 0)
        {
            switch (mytcpMessage->header.type) {
                case SYN:
                {
                    printf("receive SYN \n");
                    int dest_port = mytcpMessage->header.dest_port;
                    printf("dest port %d\n",dest_port);
                    int i = 0;
                   /* for(i = 0; i < 10 ;i++)
                    {
                        if(TCBtable[i]!= NULL)
                        printf("%d\n",TCBtable[i]->server_portNum);
                    }
                   */
                    for(i = 0 ; i < MAX_TRANSPORT_CONNECTIONS ; i++)
                    {
                        if(TCBtable[i]!=NULL)
                        {
                            if(TCBtable[i]->server_portNum == dest_port)
                            {
                                pthread_mutex_lock(TCBtable[i]->bufMutex);
				printf("received a SYN package\n");
                                
                                TCBtable[i]->client_portNum = mytcpMessage->header.src_port;
                                
                                TCBtable[i]->state = CONNECTED;
                                //TCBtable[i]->expect_seqNum = mytcpMessage->header.seq_num + 1;
                                TCBtable[i]->recvBuf = mytcpMessage -> data;
                                TCBtable[i]->usedBufLen = TCBtable[i]->usedBufLen +mytcpMessage ->header.length;
                                
                                
                                seg_t * retcpMessage = (seg_t*)malloc(sizeof(seg_t));
                                memset(retcpMessage,0,sizeof(seg_t));
                                retcpMessage->header.src_port = TCBtable[i]->server_portNum;
                                retcpMessage->header.dest_port = TCBtable[i]->client_portNum;
                                retcpMessage->header.ack_num = mytcpMessage->header.seq_num+1;
                                retcpMessage->header.type = SYNACK;
                                sip_sendseg(connfd, retcpMessage);
                       //         while(1)
                         //       {
                           //     sip_sendseg(*(int *)arg, retcpMessage);
                             //   }
                                pthread_mutex_unlock(TCBtable[i]->bufMutex);
				break;
                            }
                        }
                    }
                    if(i == 10)
                    {
                        printf("SYN error!\n");
                    }
                }
                    break;
                case FIN:
                {
                    printf("receive FIN \n");
                    int dest_port = mytcpMessage->header.dest_port;
                    int i = 0;

                    for(i = 0 ; i < MAX_TRANSPORT_CONNECTIONS ; i++)
                    {
                        if(TCBtable[i]!=NULL)
                        {
                            if(TCBtable[i]->server_portNum == dest_port)
                            {
                                printf("i: %d\n",i);
                                pthread_mutex_lock(TCBtable[i]->bufMutex);
                                seg_t * retcpMessage = (seg_t*)malloc(sizeof(seg_t));
                                memset(retcpMessage,0,sizeof(seg_t));
                                retcpMessage->header.src_port = TCBtable[i]->server_portNum;
                                retcpMessage->header.dest_port = TCBtable[i]->client_portNum;
                                retcpMessage->header.ack_num = mytcpMessage->header.seq_num+1;
                                retcpMessage->header.type = FINACK;
                                sip_sendseg(connfd, retcpMessage);
                               // sip_sendseg(*(int *)arg, retcpMessage);
                                printf("send ackfin %d",i);
                             //   while(1)
                               // {
                                //    sip_sendseg(*(int *)arg, retcpMessage);
                               // }
                                TCBtable[i]->state = CLOSEWAIT;
								//TCBtable[i]->usedBufLen = 0;
                                pthread_mutex_unlock(TCBtable[i]->bufMutex);
								break;
                            }
                        }
                    }
                    if(i == 10)
                    {
                        printf("FIN error or Link has been closed!\n");
                    }
                }break;
                case DATA:
                {
					  printf("receive DATA \n");
                    int dest_port = mytcpMessage->header.dest_port;
                    printf("dest port %d\n",dest_port);
                    int i = 0;
					for(i = 0;i < MAX_TRANSPORT_CONNECTIONS;i ++)
					{
						if(TCBtable[i]!=NULL)
						{
							if(TCBtable[i]->server_portNum == dest_port)
							{
								printf("i: %d\n",i);
								if(mytcpMessage->header.seq_num == TCBtable[i]->expect_seqNum)
								{
									pthread_mutex_lock(TCBtable[i]->bufMutex);
									if(TCBtable[i]->usedBufLen + mytcpMessage->header.length >= RECEIVE_BUF_SIZE)
									{
										printf("TCB buffer overflow! throw away! %d\n",TCBtable[i]->usedBufLen);
										//TCBtable[i]->expect_seqNum = mytcpMessage->header.seq_num + mytcpMessage->header.length;
										seg_t * retcpMessage = (seg_t*)malloc(sizeof(seg_t));
										memset(retcpMessage,0,sizeof(seg_t));
										retcpMessage->header.src_port = TCBtable[i]->server_portNum;
										retcpMessage->header.dest_port = TCBtable[i]->client_portNum;
										retcpMessage->header.ack_num = TCBtable[i]->expect_seqNum;
										retcpMessage->header.type = DATAACK;
										sip_sendseg(connfd, retcpMessage);
										printf("send next Dataack %d %d %d",i,TCBtable[i]->expect_seqNum,TCBtable[i]->usedBufLen);
									}
									else
									{
										printf("TCB buffer OK! add more! %d\n",TCBtable[i]->usedBufLen);
										//TCBtable[i]->expect_seqNum =  mytcpMessage->header.seq_num +1;
										TCBtable[i]->expect_seqNum = mytcpMessage->header.seq_num + mytcpMessage->header.length;
										memcpy(TCBtable[i]->recvBuf + TCBtable[i]->usedBufLen , mytcpMessage->data , mytcpMessage->header.length);
										TCBtable[i]->usedBufLen = TCBtable[i]->usedBufLen + mytcpMessage->header.length;
										seg_t * retcpMessage = (seg_t*)malloc(sizeof(seg_t));
										memset(retcpMessage,0,sizeof(seg_t));
										retcpMessage->header.src_port = TCBtable[i]->server_portNum;
										retcpMessage->header.dest_port = TCBtable[i]->client_portNum;
										retcpMessage->header.ack_num = TCBtable[i]->expect_seqNum;
										retcpMessage->header.type = DATAACK;
										sip_sendseg(connfd, retcpMessage);
										printf("send next Dataack %d %d %d",i,TCBtable[i]->expect_seqNum,TCBtable[i]->usedBufLen);
									}
									pthread_mutex_unlock(TCBtable[i]->bufMutex);
								}
								else
								{
								printf("Server need old packet!n\n");
								seg_t * retcpMessage = (seg_t*)malloc(sizeof(seg_t));
                                memset(retcpMessage,0,sizeof(seg_t));
                                retcpMessage->header.src_port = TCBtable[i]->server_portNum;
                                retcpMessage->header.dest_port = TCBtable[i]->client_portNum;
                                retcpMessage->header.ack_num = TCBtable[i]->expect_seqNum;
                                retcpMessage->header.type = DATAACK;
                                sip_sendseg(connfd, retcpMessage);
                               // sip_sendseg(*(int *)arg, retcpMessage);
                                printf("send old Dataack %d need old packet%d\n",i,TCBtable[i]->expect_seqNum);
								}
								break;
							}
						}
					}
					if(i == 10)
					{
						 printf("DATA error !\n");
					}
                    
                }break;
                default:
                    break;
            }
        }
        else
        {
          //  if(j == 1)
          //  {
          //      seg_t * retcpMessage = (seg_t*)malloc(sizeof(seg_t));
          //      memset(retcpMessage,0,sizeof(seg_t));
          //      retcpMessage->header.src_port = TCBtable[i]->server_portNum;
          //      retcpMessage->header.dest_port = TCBtable[i]->client_portNum;
          //      retcpMessage->header.seq_num = TCBtable[i]->expect_seqNum;
          //      retcpMessage->header.type = DATAACK;
          //      sip_sendseg(*(int *)arg, retcpMessage);
          //  }
        }
        
    }
  return 0;
}

