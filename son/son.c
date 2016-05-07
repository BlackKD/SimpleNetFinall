//文件名: son/son.c
//
//描述: 这个文件实现SON进程
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中.  
//
//创建日期: 2015年

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止.
void* waitNbrs(void* arg) {
	//你需要编写这里的代码.
    int myid = topology_getMyNodeID();
    pid_t pid;
    socklen_t clilen;
    int listenfd;
    //char buf[MAXLINE];
		int j = 0;
		int full = 0;
		for(j = 0;j<topology_getNbrNum();j++)
        {
            if(nt[j].nodeID > topology_getMyNodeID())
			{
                full ++;
				//printf("nt num :%d   %d\n",j,topology_getMyNodeID());
			}
        }   
    struct sockaddr_in cliaddr,servaddr;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(CONNECTION_PORT);
    bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
    listen(listenfd,MAX_NODE_NUM);//开始监听


	int temp = 0;
	printf("full:%d",full);
    for(temp = 0;temp < full;temp++)
    {
	
    clilen = sizeof(cliaddr);
    int connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);
	//close(listenfd);
   // if((pid = fork()) == 0)
    //{
     //   close(listenfd);
        int neberid = topology_getNodeIDfromip((struct in_addr*)&(cliaddr.sin_addr));
        printf("neberid : %d\n",neberid);
        /*int i = 0;
        for(i = 0; i < topology_getNbrNum();i++)
        {
            if(nt[i].nodeID == neberid)
            {
                nt[i].conn = connfd;
                break;
            }
        }
        if(i == topology_getNbrNum())
        {
            printf("error node !\n");
            return 0;
        }*/
        int t = nt_addconn(nt, neberid, connfd);
        if(t!=1&&t!=0)
        {
            printf("error neberid!\n");
        }
        else if(t == 0)
		{
			temp--;
		}else
        {
            //return 1;
			printf("neberid : %d link sucess\n",neberid);
        }
		
		    int j= 0;
        //int full = 1;
         
    }

  return 0;
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
    int i = 0;
    for(i = 0;i < topology_getNbrNum();i++)
    {
        if(nt[i].nodeID < topology_getMyNodeID())
        {
            struct sockaddr_in servaddr;
            int sockfd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET for ipv4; SOCK_STREAM for byte stream
            if(sockfd < 0) {
                printf("Socket error!\n");
                return 0;
            }
            memset(&servaddr, 0, sizeof(struct sockaddr_in));
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = nt[i].nodeIP;
			printf("IP :%d\n",servaddr.sin_addr.s_addr);
            servaddr.sin_port = htons(CONNECTION_PORT);
            //connect to the server
            if(connect(sockfd, (struct sockaddr* )&servaddr, sizeof(servaddr)) < 0) {//创建套接字连接服务器
                printf("Link Wrong!\n");
                //exit(1);
                return -1;
            }
            else
            {
                printf("%d Link Success!\n",nt[i].nodeID);
                nt[i].conn = sockfd;
                
            }
        }
    }
	//你需要编写这里的代码.
  return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
void* listen_to_neighbor(void* arg) {
	//你需要编写这里的代码.
    //int sockfd = 0;
    //struct fd_set set;
    while(1)
    {
  /*      FD_ZERO(&set);//将你的套节字集合清空
        int i =0;
        for(i = 0 ; i <topology_getNbrNum();i++)
        {
            if(nt[i].conn!= -1)
                  FD_SET(nt[i].conn, &set);//加入你感兴趣的套节字到集合,这里是一个读数据的套节字s
        }
        select(0,&set,NULL,NULL,NULL);//检查套节字是否可读,
        //很多情况下就是是否有数据(注意,只是说很多情况)
        //这里select是否出错没有写
        for(i = 0;i<topology_getNbrNum();i++)
        {
        if(FD_ISSET(nt[i].conn, &set) //检查s是否在这个集合里面,
        { //select将更新这个集合,把其中不可读的套节字去掉
            //只保留符合条件的套节字在这个集合里面
            //char *buffer = (char *)malloc(2000);
            //memset(buffer,0,2000);
            sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
            memset(pkt,0,sizeof(sip_pkt_t));
            recv(nt[i].conn, pkt, sizeof(sip_pkt_t), 0);
            forwardpktToSIP(pkt,sip_conn);
            printf("send packet to IP\n");
        }
        }
   */
        sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
        memset(pkt,0,sizeof(sip_pkt_t));
        recvpkt(pkt, nt[*((int *)arg)].conn);
        forwardpktToSIP(pkt,sip_conn);
        printf("send packet to IP\n");

    }
  return 0;
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
    socklen_t clilen;
    int listenfd;
    //char buf[MAXLINE];
    struct sockaddr_in cliaddr,servaddr;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family=AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(SON_PORT);
    bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
    listen(listenfd,MAX_NODE_NUM);//开始监听
    clilen = sizeof(cliaddr);
    int connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);
    sip_conn = connfd;
    printf("IP linked\n");
    while(1)
    {
        sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
        memset(pkt,0,sizeof(sip_pkt_t));
        int nextNode = 0;
    // 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
        if(getpktToSend(pkt,&nextNode,sip_conn)==1)
        {
            printf("in son ready to send:%d\n",pkt->header.src_nodeID);
            if(nextNode == BROADCAST_NODEID)
            {
                int i = 0;
                for(i = 0 ;i <topology_getNbrNum();i++)
                {
                    printf("send to everyone!\n");
                    if(sendpkt(pkt, nt[i].conn) == 1)
                    {
                        printf("send to nextnode sucess!\n");
                    }
                    else
                    {
                        printf("send fail %d",i);
                    }
                }
            
            }
            else
            {
                int i = 0;
            for(i = 0 ;i <topology_getNbrNum();i++)
            {
                if(nextNode == nt[i].nodeID)
                {
                    break;
                }
            }
            if(i == topology_getNbrNum())
            {
                printf("cant find next node!\n");
                exit(0);
                
            }
            // 如果报文发送成功, 返回1, 否则返回-1.
            if(sendpkt(pkt, nt[i].conn) == 1)
            {
                printf("send to nextnode sucess!\n");
            }
            else
            {
                printf("send to nextnode fail\n");
            }
            }
        }
   
        
    }
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
    nt_destroy(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接.
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	printf("finish\n");
	//while(1);
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
