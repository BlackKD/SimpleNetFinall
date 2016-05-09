//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年

#include "neighbortable.h"
#include "../topology/topology.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
   /* int myid = topology_getMyNodeID();
    FILE *fp=fopen("topology/topology.dat","r");
    if(fp==NULL)//如果失败了
    {
        perror("../topology/topology.dat");
        printf("错误！");
        exit(1);//中止程序
    }*/
    nbr_entry_t* myentry = (nbr_entry_t*)malloc(sizeof(nbr_entry_t)*topology_getNbrNum());
    memset((char *)myentry,0,sizeof(nbr_entry_t)*topology_getNbrNum());
    int * nodelist = topology_getNbrArray();
    int i = 0;
    for(i = 0 ; i < topology_getNbrNum();i++)
    {
        myentry[i].nodeID = nodelist[i];
        myentry[i].nodeIP = (unsigned long)(((unsigned long)nodelist[i])<<24)+ init_ip;
		printf("node ip = %d\n",myentry[i].nodeIP);
        myentry[i].conn = -1;
    }
   /* int i = 0;
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
            myentry[i].nodeID = temp2;
            myentry[i].nodeIP = init_ip + temp2;
            myentry[i].conn = -1;
            i++;
        }
        else if(temp1!=myid&&temp2==myid)
        {
            myentry[i].nodeID = temp1;
            myentry[i].nodeIP = init_ip + temp1;
            myentry[i].conn = -1;
            i++;
        }
    }while (!feof(fp));*/
    
  return myentry;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
    int i = 0 ;
    for(i = 0; i < topology_getNbrNum();i++)
    {
        close(nt[i].conn);
    }
    free(nt);
    return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{   int i = 0;
    for(i = 0;i < topology_getNbrNum();i ++)
    {
        if(nt[i].nodeID == nodeID)
        {
			if(nt[i].conn == -1)
			{
            nt[i].conn = conn;
			break;
			}
			else{
				return 0;
			}
        }
    }
    if(i==topology_getNbrNum())
    {
        return -1;
    }
  return 1;
}
