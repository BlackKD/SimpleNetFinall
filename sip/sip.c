//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2015年

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON() {
    struct sockaddr_in servaddr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET for ipv4; SOCK_STREAM for byte stream
    if(sockfd < 0) {
        printf("Socket error!\n");
        return 0;
    }
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = (unsigned long)(((unsigned long)topology_getMyNodeID())<<24)+ init_ip;
    servaddr.sin_port = htons(SON_PORT);
    //connect to the server
    if(connect(sockfd, (struct sockaddr* )&servaddr, sizeof(servaddr)) < 0) {//创建套接字连接服务器
        printf("IP %d Link Wrong!\n",topology_getMyNodeID());
        //exit(1);
        return -1;
    }
    else
    {
        printf("IP %d Link Success!\n",topology_getMyNodeID());
        return sockfd;
        
    }
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	//你需要编写这里的代码.
	while(1)
    {  
	pthread_mutex_lock(dv_mutex);
	sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
    memset(pkt,0,sizeof(sip_pkt_t));
    pkt->header.src_nodeID = topology_getMyNodeID();
    //printf("send neb my iD : %d\n",pkt->header.src_nodeID);
    pkt->header.dest_nodeID = BROADCAST_NODEID;
    pkt->header.length = sizeof(pkt_routeupdate_t);
    pkt->header.type = ROUTE_UPDATE;
	//dv_entry_t * senddv = dv[0]->dvEntry;
    pkt_routeupdate_t* pkuppt = (pkt_routeupdate_t*)malloc(sizeof(pkt_routeupdate_t));
    memset(pkuppt,0,sizeof(pkt_routeupdate_t));
	pkuppt->entryNum = topology_getNodeNum();
	int i = 0;
	for(i = 0 ; i < topology_getNodeNum(); i++)
	{
		pkuppt->entry[i].nodeID = dv[0].dvEntry[i].nodeID;
		pkuppt->entry[i].cost = dv[0].dvEntry[i].cost;
		//memcpy(pkuppt->entry[i],dv[0].dvEntry[i],sizeof(dv_entry_t));
	}
		memcpy(pkt->data,pkuppt,sizeof(pkt_routeupdate_t));
        
        if(son_sendpkt(BROADCAST_NODEID, pkt, son_conn)==-1)
        {
            printf("SON SLEEP!!!!!!!!!!\n");
            break;
        }
        pthread_mutex_unlock(dv_mutex);
        sleep(ROUTEUPDATE_INTERVAL);


    }
  return 0;

}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.

void updatatable(sip_pkt_t *pkt)
{
	pthread_mutex_lock(dv_mutex);
	int i= 0;
	for(i = 0 ; i < topology_getNbrNum()+1;i++)
	{
		if(dv[i].nodeID == pkt->header.src_nodeID)
		{
			int j = 0;
			for(j = 0 ; j < topology_getNodeNum(); j++)
			{
				dv[i].dvEntry[j].cost = ((pkt_routeupdate_t *)(pkt->data))->entry[j].cost;
			}
		}
	}
 
    //int  i = 0;
    for(i = 0 ; i < topology_getNodeNum();i++)
    {
        if(dv[0].dvEntry[i].nodeID == dv[0].nodeID) continue;
        int destnode = dv[0].dvEntry[i].nodeID;
        int j = 0;
        for(j = 0;j <topology_getNbrNum()+1;j++)
        {
            if(dv[j].nodeID!=dv[0].nodeID&&dv[j].nodeID!=destnode)
            {
                if(dvtable_getcost(dv,dv[0].nodeID,dv[j].nodeID)+dvtable_getcost(dv,dv[j].nodeID,destnode) < dv[0].dvEntry[i].cost)
                {
                    //dv[0].dvEntry[i].cost = topology_getCost(dv[0].nodeID,dv[j].nodeID)+dv[j].dvEntry[i].cost;
                    dvtable_setcost(dv,dv[0].nodeID,destnode,dvtable_getcost(dv,dv[0].nodeID,dv[j].nodeID)+dvtable_getcost(dv,dv[j].nodeID,destnode));
                    pthread_mutex_lock(routingtable_mutex);
                    routingtable_setnextnode(routingtable,destnode,dv[j].nodeID);
                    pthread_mutex_unlock(routingtable_mutex);
                }
            }
        }
    }
	//需要在此处刷新向量和路由
	pthread_mutex_unlock(dv_mutex);
    //nbrcosttable_print(nct);
    dvtable_print(dv);
    routingtable_print(routingtable);
    //nbrcosttable_print(nbr_cost_entry_t* nct)
}

void sleeptable(int ID)
{
    pthread_mutex_lock(dv_mutex);
    int i = 0;
    for(i = 0 ; i < topology_getNbrNum()+1;i++)
    {
        int j = 0;
        for(j = 0 ; j < topology_getNodeNum();j++ )
        {
            if(i==0)
            {
                if(dv[i].dvEntry[j].nodeID == ID)
                //printf("!!!!!!!!!!!!%d!!!!!!!!!\n",dv[i].dvEntry[j].nodeID);
                {
                    dv[i].dvEntry[j].cost = INFINITE_COST;
                }
                else
                {
                    dv[i].dvEntry[j].cost = topology_getCost(dv[i].nodeID,dv[i].dvEntry[j].nodeID);
                }
            }
            else
            {
                dv[i].dvEntry[j].cost = INFINITE_COST;
            }
        }
    }
    pthread_mutex_lock(routingtable_mutex);
    free(routingtable);
    routingtable = routingtable_create();
    pthread_mutex_unlock(routingtable_mutex);
   // dvtable_print(dv);
    for(i = 0 ; i < topology_getNodeNum();i++)
    {
        if(dv[0].dvEntry[i].nodeID == dv[0].nodeID) continue;
        int destnode = dv[0].dvEntry[i].nodeID;
        int j = 0;
        for(j = 0;j <topology_getNbrNum()+1;j++)
        {
            if(dv[j].nodeID!=dv[0].nodeID&&dv[j].nodeID!=destnode)
            {
                if(dvtable_getcost(dv,dv[0].nodeID,dv[j].nodeID)+dvtable_getcost(dv,dv[j].nodeID,destnode) < dv[0].dvEntry[i].cost)
                {
                    //dv[0].dvEntry[i].cost = topology_getCost(dv[0].nodeID,dv[j].nodeID)+dv[j].dvEntry[i].cost;
                    dvtable_setcost(dv,dv[0].nodeID,destnode,dvtable_getcost(dv,dv[0].nodeID,dv[j].nodeID)+dvtable_getcost(dv,dv[j].nodeID,destnode));
                    pthread_mutex_lock(routingtable_mutex);
                    routingtable_setnextnode(routingtable,destnode,dv[j].nodeID);
                    pthread_mutex_unlock(routingtable_mutex);
                }
            }
        }
    }

    
    sip_pkt_t* pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
    memset(pkt,0,sizeof(sip_pkt_t));
    pkt->header.src_nodeID = topology_getMyNodeID();
    //printf("send neb my iD : %d\n",pkt->header.src_nodeID);
    pkt->header.dest_nodeID = BROADCAST_NODEID;
    pkt->header.length = sizeof(pkt_routeupdate_t);
    pkt->header.type = ROUTE_UPDATE;
    //dv_entry_t * senddv = dv[0]->dvEntry;
    pkt_routeupdate_t* pkuppt = (pkt_routeupdate_t*)malloc(sizeof(pkt_routeupdate_t));
    memset(pkuppt,0,sizeof(pkt_routeupdate_t));
    pkuppt->entryNum = topology_getNodeNum();
    //int i = 0;
    for(i = 0 ; i < topology_getNodeNum(); i++)
    {
        pkuppt->entry[i].nodeID = dv[0].dvEntry[i].nodeID;
        pkuppt->entry[i].cost = dv[0].dvEntry[i].cost;
        //memcpy(pkuppt->entry[i],dv[0].dvEntry[i],sizeof(dv_entry_t));
    }
    memcpy(pkt->data,pkuppt,sizeof(pkt_routeupdate_t));
    if(son_sendpkt(BROADCAST_NODEID, pkt, son_conn)==-1)
    {
        printf("SON SLEEP!!!!!!!!!!\n");
    }
    //sip_pkt_t *pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));
    //for(k = 0 ; k < 10;k++)
     //   son_recvpkt(pkt,son_conn);
    
    
    pthread_mutex_unlock(dv_mutex);
    dvtable_print(dv);
    routingtable_print(routingtable);

}

// receive the packets from SON
void* pkthandler(void* arg) {
	//你需要编写这里的代码.
	sip_pkt_t *pkt = (sip_pkt_t*)malloc(sizeof(sip_pkt_t));

	while(son_recvpkt(pkt,son_conn)>0) {
		//printf("Routing: received a packet from neighbor %d\n",pkt->header.src_nodeID);
		switch(pkt->header.type)
		{
			case ROUTE_UPDATE:updatatable(pkt); break;
            case SLEEP:sleeptable(pkt->header.dest_nodeID);break;
			case SIP: {
				// destination is itself and then send it to STCP
                printf("Routing: received a SIP packet from neighbor %d\n",pkt->header.src_nodeID);
				if (pkt->header.dest_nodeID == topology_getMyNodeID()) {
					sendseg_arg_t *sseg = (sendseg_arg_t*)(pkt->data);
					if (forwardsegToSTCP(stcp_conn, pkt->header.src_nodeID,  &(sseg->seg)) < 0) {
						printf("pkhandler: forwardsegTpSTCP error!\n");
					}
				}
				else { // send to next node
					int nextNodeID = routingtable_getnextnode(routingtable, pkt->header.dest_nodeID);
					if (son_sendpkt(nextNodeID, pkt, son_conn) < 0) {
						printf("pkhandler: son_dendpkt error!\n");
					}
				}
				break;//heiheihei 注意对SIP报转发时要从路由表中查下一跳IP，并且对获得目的地不是自己的包要进行转发（同上）
			}
		}
	}
    printf("SON SLEEP!!!!!!!!!!\n");
  return 0;
}

/*
 * receive the segments from STCP with getSegToSend function,
 * and send it with son_sendpkt function
 */
void* seghandler(void *arg) {
	sendpkt_arg_t spkt;
	sip_pkt_t *pktPtr      = &(spkt.pkt);

	// the received content will be put into the data field of spkt.pkt 
	sendseg_arg_t *ssegPtr = (sendseg_arg_t*)(pktPtr->data);
	int *destNodeIdPtr     = &(ssegPtr->nodeID);
	seg_t *segPtr          = &(ssegPtr->seg);

	while (1) {
		// receive segs from local STCP
		if (getsegToSend(stcp_conn, destNodeIdPtr, segPtr) > 0) {
			// sip nextNodeID
			int nextNodeID  = routingtable_getnextnode(routingtable, *destNodeIdPtr);
			spkt.nextNodeID = nextNodeID;
			// fill the sip header
			sip_hdr_t *siphdr   = &(pktPtr->header);
			siphdr->src_nodeID  = topology_getMyNodeID();
			siphdr->dest_nodeID = *destNodeIdPtr;
			siphdr->length      = sizeof(sendseg_arg_t);
			siphdr->type        = SIP;
			// send it
            printf("segPtr-port = %d,pktPtr-port %d\n",segPtr->header.src_port,((sendseg_arg_t *)(pktPtr->data))->seg.header.src_port);
            printf("get seg from stp begin to send %d\n",nextNodeID);
			if (son_sendpkt(nextNodeID, pktPtr, son_conn) < 0)
				return NULL;
		}
		else 
			return NULL;
	}
}

// 等待来自本地STCP的连接
// 当本地STCP断开后，等待下一次的STCP连接
void waitSTCP() {
	socklen_t clilen;
    int listenfd;
    struct sockaddr_in cliaddr,servaddr;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SIP_PORT);
    bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    listen(listenfd, MAX_NODE_NUM);//开始监听
    clilen = sizeof(cliaddr);
    
    pthread_t tid;
    while(1) {
    	stcp_conn = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
    	printf("Linked to STCP\n");
		pthread_create(&tid, NULL, seghandler, NULL);
		// wait for seghandler's return and then wait for next connection
		pthread_join(tid, NULL);	
    }
    
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
    close(son_conn);
    close(stcp_conn);
	//你需要编写这里的代码.
  return;
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}

	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


