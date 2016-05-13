#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
  return node%MAX_ROUTINGTABLE_SLOTS;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
	routingtable_t* rtable = (routingtable_t*)malloc(sizeof(routingtable_t));
	int i = 0;
	int * nodelist = topology_getNbrArray();
	for(i = 0;i<MAX_ROUTINGTABLE_SLOTS;i++)
	{
		rtable->hash[i] = NULL;
	}
	for(i = 0;i<topology_getNbrNum();i++)
	{
		routingtable_entry_t* nbrnode = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		nbrnode->destNodeID = nodelist[i];
		nbrnode->nextNodeID = nodelist[i];//????????
		nbrnode->next = NULL;
		int key = makehash(nodelist[i]);
		routingtable_entry_t* temp = rtable->hash[key];
		if(temp == NULL)
		{
			rtable->hash[key] = nbrnode;
		}else
		{
			while(temp->next!=NULL)
				temp = temp->next;
			temp->next = nbrnode;
		}
	}
  return rtable;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	free(routingtable);
  return;
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
	int key = makehash(destNodeID);
	if(routingtable->hash[key] == NULL)
	{
		routingtable_entry_t* nbrnode = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		nbrnode->destNodeID = destNodeID;
		nbrnode->nextNodeID = nextNodeID;
		nbrnode->next = NULL;
		routingtable->hash[key] = nbrnode;
	}else
	{
		routingtable_entry_t* temp = routingtable->hash[key];
        if(temp->destNodeID == destNodeID)
        {
            temp->nextNodeID = nextNodeID;
            return;
        }
		while(temp->next!=NULL)
		{
			if(temp->destNodeID == destNodeID)
			{
				temp->nextNodeID = nextNodeID;
				return;
			}
		}
		routingtable_entry_t* nbrnode = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		nbrnode->destNodeID = destNodeID;
		nbrnode->nextNodeID = nextNodeID;
		nbrnode->next = NULL;
		temp->next = nbrnode;
	}
  return;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
		int key = makehash(destNodeID);
	if(routingtable->hash[key] == NULL)
	{
		return -1;
	}else
	{
		routingtable_entry_t* temp = routingtable->hash[key];
        if(temp->destNodeID == destNodeID)
        {
            return temp->nextNodeID;
        }
		while(temp->next!=NULL)
		{
			if(temp->destNodeID == destNodeID)
			{
				return temp->nextNodeID;
			}
		}
	}
  return -1;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
	printf("print routingtable!!!!!\n");
	int i = 0;
	for(i = 0 ; i < MAX_ROUTINGTABLE_SLOTS;i++)
	{
		printf("table[%d]:",i);
		if(routingtable->hash[i]!=NULL)
		{
			routingtable_entry_t* temp = routingtable->hash[i];
			while(temp!=NULL)
			{
				printf("Link:%d,%d; ",temp->destNodeID,temp->nextNodeID);
				temp=temp->next;
			}
		}
		printf("\n");
	}
  return;
}
