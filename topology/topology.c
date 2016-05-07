//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2015年
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "topology.h"
#include "../common/constants.h"

#define NODE_NAME_LEN 20

extern unsigned long get_local_ip();

int myNodeId = 0;

FILE *open_dat() {
	FILE *fp=fopen("../topology/topology.dat","r");
    if(fp==NULL)//如果失败了
    {
        perror("../topology/topology.dat");
        //printf("错误！");
        exit(1);//中止程序
    }
    return fp;
}

/*
 * Place the number of neighbours in the area pointered by nbrNum
 * Return an malloced array of neighbors' nodeID
 * On error, return NULL
 */
int *read_Nbr(int *num) {
	//printf("In read_Nbr\n");

	int myId = topology_getMyNodeID();

	FILE *f = open_dat();

	int nbrNum = 0;
	int nbrArr[MAX_NODE_NUM];
	
	char left_node[NODE_NAME_LEN], right_node[NODE_NAME_LEN];
	int cost = 0;
	fscanf(f, "%s%s%d", left_node, right_node, &cost); // first entry
	while (!feof(f)) {
		int left_nodeId = topology_getNodeIDfromname(left_node);
		int right_nodeId = topology_getNodeIDfromname(right_node);

		int nbrId = -1;
		// check whether there's my neighbor
		if (left_nodeId == myId) 
			nbrId = right_nodeId;
		else if (right_nodeId == myId)
			nbrId = left_nodeId;

		// check whether the neighbor appeared
		int i = 0;
		for(i = 0; i < nbrNum; i ++) {
			if (nbrArr[i] == nbrId)
				break;
		}
		if(i >= nbrNum && nbrId != -1) { // a new neighbor
			nbrArr[nbrNum ++] = nbrId;
		}

		fscanf(f, "%s%s%d", left_node, right_node, &cost); // next entry or EOF
	}

	*num = nbrNum;

	// malloc an array to places the neiboId
	int *arr = (int *)malloc(sizeof(int) * nbrNum);
	if (arr == NULL) {
		printf("malloc error!\n");
		return NULL;
	}
	// move the neiboIds
	int i = 0;
	for(i = 0; i < nbrNum; i ++) {
		arr[i] = nbrArr[i];
	}

	fclose(f);

  	return arr;
}

/*
 * Place the number of all nodes in the area pointered by nbrNum
 * Return an malloced array of all nodes' nodeID
 * On error, return NULL
 */
int *read_Nodes(int *num) {
	FILE *f = open_dat();

	int nodesNum = 0;
	int nodeArr[MAX_NODE_NUM];
	
	char left_node[NODE_NAME_LEN], right_node[NODE_NAME_LEN];
	int cost = 0;
	fscanf(f, "%s%s%d", left_node, right_node, &cost); // first entry
	while (!feof(f)) {
		
		int left_nodeId = topology_getNodeIDfromname(left_node);
		int right_nodeId = topology_getNodeIDfromname(right_node);

		// check whether the left node appeared
		int left_appeared  = 0; // false
		int right_appeared = 0; // false
		int i = 0;
		for(i = 0; i < nodesNum; i ++) {
			if (nodeArr[i] == left_nodeId)
				left_appeared = 1; // true
			if (nodeArr[i] == right_nodeId)
				right_appeared = 1; // true
		}
		if (left_appeared == 0) { // a new node
			nodeArr[nodesNum ++] = left_nodeId;
		}
		if (right_appeared == 0) { // a new node
			nodeArr[nodesNum ++] = right_nodeId;
		}

		fscanf(f, "%s%s%d", left_node, right_node, &cost); // next entry
	}

	*num = nodesNum;

	// malloc an array to places the nodeId
	int *arr = (int *)malloc(sizeof(int) * nodesNum);
	if (arr == NULL) {
		printf("malloc error!\n");
		return NULL;
	}
	// move the nodeIds
	int i = 0;
	for(i = 0; i < nodesNum; i ++) {
		arr[i] = nodeArr[i];
	}

	fclose(f);

  	return arr;
}
//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	//printf("In topology_getNodeIDfromname ");
	if (hostname == NULL) {
		printf("hostname is NULL!\n");
		return -1;
	}

	//printf("hostname: %s ", hostname);
	if (strncmp(hostname, "csnetlab_1", 11) == 0) {
		//printf("NodeID is 185\n");
		return 185;
	}
	else if (strncmp(hostname, "csnetlab_2", 11) == 0) {
		//printf("NodeID is 186\n");
		return 186;
	}
	else if (strncmp(hostname, "csnetlab_3", 11) == 0) {
		//printf("NodeID is 187\n");
		return 187;
	}
	else if (strncmp(hostname, "csnetlab_4", 11) == 0) {
		//printf("NodeID is 188\n");
		return 188;
	}
	else { 
		//printf("Unknown hostname, -1 is returned\n");
  		return -1;
  	}
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	//printf("in topology_getNodeIDfromip ");
	if (addr == NULL) {
		printf("addr is NULL\n");
		return -1;
	}

	unsigned char *ip = (unsigned char *)(&(addr->s_addr));
	//printf("addr: %u.%u.%u.%u ", (unsigned)(ip[3]), (unsigned)(ip[2]), (unsigned)(ip[1]), (unsigned)(ip[0]));
	return (int)(ip[3]);
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	//printf("In topology_getMyNodeID ");

	if (myNodeId == 0) { // hasn't know myNodeId
		struct in_addr ip;
		ip.s_addr = get_local_ip();

		if( ip.s_addr != 0) {
			myNodeId = topology_getNodeIDfromip(&ip);
			return myNodeId;
		}
		else 
  			return -1;
  	}
  	
  	//printf("return myNodeId: %d\n", myNodeId);
	return myNodeId;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	int nbrNum = 0;
	int *p = read_Nbr(&nbrNum);
	if (p != NULL) {
		free(p);
		return nbrNum;
	}
	else
  		return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	int nbrNum = 0;
	int *p = read_Nbr(&nbrNum);
	if (p != NULL) {
		return p;
	}
	else
  		return NULL;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
	int nodesNum = 0;
	int *p = read_Nodes(&nodesNum);
	if (p != NULL) {
		free(p);
		return nodesNum;
	}
	else
  		return 0;	
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	int nodesNum = 0;
	int *p = read_Nbr(&nodesNum);
	if (p != NULL) {
		return p;
	}
	else
  		return NULL;

}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	int ret_cost = INFINITE_COST;
	FILE *f = open_dat();

	char left_node[NODE_NAME_LEN], right_node[NODE_NAME_LEN];
	int cost = 0;
	fscanf(f, "%s%s%d", left_node, right_node, &cost); // first entry
	while (!feof(f)) {
		int left_nodeId = topology_getNodeIDfromname(left_node);
		int right_nodeId = topology_getNodeIDfromname(right_node);

		// check 
		if (left_nodeId == fromNodeID && right_nodeId == toNodeID) {
			ret_cost = cost; 
			break;
		}
		fscanf(f, "%s%s%d", left_node, right_node, &cost); // next entry or EOF
	}
	
	fclose(f);
  	return ret_cost;
}
