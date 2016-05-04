
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
	int myid = topology_getMyNodeID();
   /* FILE *fp=fopen("../topology/topology.dat","r");
    if(fp==NULL)//如果失败了
    {
        perror("../topology/topology.dat");
        printf("错误！");
        exit(1);//中止程序
    }*/
    /*nbr_entry_t* myentry = (nbr_entry_t*)malloc(sizeof(nbr_entry_t)*topology_getNbrNum());
    memset((char *)myentry,0,sizeof(nbr_entry_t)*topology_getNbrNum());
    int * nodelist = topology_getNbrArray();
    int i = 0;
    for(i = 0 ; i < topology_getNbrNum();i++)
    {
        myentry[i].nodeID = nodelist[i];
        myentry[i].nodeIP = (unsigned long)(((unsigned long)nodelist[i])<<24)+ init_ip;
		printf("node ip = %d\n",myentry[i].nodeIP);
        myentry[i].conn = -1;
    }*/
	nbr_cost_entry_t* nbr_cost_table = (nbr_cost_entry_t*)malloc(sizeof(nbr_cost_entry_t*)*topology_getNbrNum());
	memset(nbr_cost_table,0,sizeof(nbr_cost_entry_t*)*topology_getNbrNum());
	int * nodelist = topology_getNbrArray();
	int i = 0;
	for(i = 0 ; i <topology_getNbrNum();i++ )
	{
		nbr_cost_table[i].nodeID = nodelist[i];
		nbr_cost_table[i].cost = topology_getCost(myid,nodelist[i]);
	}
	/*int i = 0;
    do
    {
        char temp[20];
        memset(temp,0,20);
        int temp1 = 0;
        int temp2 = 0;
        int temp3 = 0;
        fscanf(fp,"%s",&temp);
        temp1 = topology_getNodeIDfromname(temp);
        memset(temp,0,20);
        fscanf(fp,"%s",&temp);
        temp2 = topology_getNodeIDfromname(temp);
        fscanf(fp,"%d",&temp3);
        
        if(temp1==myid&&temp2!=myid)
        {
            nbr_cost_table[i]->nodeID = temp2;
			nbr_cost_table[i]->cost = temp3;
            i++;
        }
        else if(temp1!=myid&&temp2==myid)
        {
            nbr_cost_table[i]->nodeID = temp1;
			nbr_cost_table[i]->cost = temp3;
            i++;
        }
    }while (!feof(fp));*/
	
	
	
	
	
  return nbr_cost_table;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	free(nct);
  return;
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	int i  = 0;
	for(i = 0 ; i < topology_getNbrNum();i++ )
	{
		if(nct[i].nodeID == nodeID)
		{
			return nct[i].cost;
		}
	}
  return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	int j = 0;
	printf("nbr_cost_table:\n");
	for(j = 0; j < topology_getNbrNum();j++)
	{
		printf("%d    %d\n",nct[j].nodeID,nct[j].cost);
	}
	//sleep(5);
  return;
}
