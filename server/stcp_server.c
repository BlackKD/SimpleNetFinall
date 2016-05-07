//�ļ���: server/stcp_server.c
//
//����: ����ļ�����STCP�������ӿ�ʵ��. 
//
//��������: 2015��

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

//����tcbtableΪȫ�ֱ���
server_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//������SIP���̵�����Ϊȫ�ֱ���
int sip_conn;

/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL. �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, 
// �ñ�����Ϊsip_sendseg��sip_recvseg���������. ���, �����������seghandler�߳�����������STCP��.
// ������ֻ��һ��seghandler.
void stcp_server_init(int conn) 
{
	    int i = 0;
    for(i = 0; i < 10 ; i++)
    {
        TCBtable[i]=NULL;
    }
    int connection = conn;//����ص�����TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���,
    pthread_t id;
    pthread_create(&id,NULL,&seghandler,(void *)&connection);
    printf("stcp_server_init finish!\n");
    return;
}

// ����������ҷ�����TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��, ����, TCB state������ΪCLOSED, �������˿ڱ�����Ϊ�������ò���server_port. 
// TCB������Ŀ������Ӧ��Ϊ�����������׽���ID�������������, �����ڱ�ʶ�������˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
int stcp_server_sock(unsigned int server_port) 
{
	return 0;
}

// �������ʹ��sockfd���TCBָ��, �������ӵ�stateת��ΪLISTENING. ��Ȼ��������ʱ������æ�ȴ�ֱ��TCB״̬ת��ΪCONNECTED 
// (���յ�SYNʱ, seghandler�����״̬��ת��). �ú�����һ������ѭ���еȴ�TCB��stateת��ΪCONNECTED,  
// ��������ת��ʱ, �ú�������1. �����ʹ�ò�ͬ�ķ�����ʵ�����������ȴ�.
int stcp_server_accept(int sockfd) 
{
	return 0;
}

// ��������STCP�ͻ��˵�����. �������ÿ��RECVBUF_POLLING_INTERVALʱ��
// �Ͳ�ѯ���ջ�����, ֱ���ȴ������ݵ���, ��Ȼ��洢���ݲ�����1. ����������ʧ��, �򷵻�-1.
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	return 0;
}

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
int stcp_server_close(int sockfd) 
{
	return 0;
}

// ������stcp_server_init()�������߳�. �������������Կͻ��˵Ľ�������. seghandler�����Ϊһ������sip_recvseg()������ѭ��, 
// ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�, �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���.
// ��鿴�����FSM���˽����ϸ��.
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

