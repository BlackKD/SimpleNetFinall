
#include "seg.h"

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
  return 0;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
  return 0;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
  return 0;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
  return 0;
}

int seglost(seg_t* segPtr) {
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
      return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
		//	//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
	segment->header.checksum = 0;
	int mychecksum = sizeof(stcp_hdr_t) + strlen(segment->data);
	//printf("%d\n",mychecksum);
	unsigned char *temp  = (unsigned char*)malloc(mychecksum);
	memcpy(temp,segment,mychecksum);
	/*if(((int)checksum/2 )* 2 != checksum)
	{
		checksum++;
		char *temp = (char *)malloc(checksum);
		memset(temp,0,checksum);
		memcpy(temp,(char *)segment,checksum);
	}
	else
	{
		char *temp = (char *)malloc(checksum);
		memset(temp,0,checksum);
		memcpy(temp,(char *)segment,checksum);
	}
	*/
	unsigned long cksum=0;
	while(mychecksum>1)
	{
	cksum+=*(unsigned short int *)temp;
	temp +=2;
	//printf("%d\n",cksum);
	mychecksum-=sizeof(unsigned short int);
	}
	if(mychecksum)
	{
	cksum+=*(unsigned char *)temp;
	}
	while (cksum>>16)
		cksum=(cksum>>16)+(cksum & 0xffff);
	printf("cksum %d\n",(unsigned short int)(~cksum));
	return (unsigned short int)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{       
		int checksum = sizeof(stcp_hdr_t) + strlen(segment->data);
		//printf("checkchecksum! %d\n",checksum);
		//printf("header.check %d\n",segment->header.checksum);
	    unsigned char *temp = (unsigned char*)malloc(checksum);
		memcpy(temp,segment,checksum);
		//printf("temp->checksum: %d\n", *(unsigned short int*)(temp + 22));
	/*if(((int)checksum/2 )* 2 != checksum)
	{
		checksum++;
		char *temp = (char *)malloc(checksum);
		memset(temp,0,checksum);
		memcpy(temp,(char *)segment,checksum);
	}
	else
	{
		char *temp = (char *)malloc(checksum);
		memset(temp,0,checksum);
		memcpy(temp,(char *)segment,checksum);
	}
	*/
	unsigned long cksum=0;
	while(checksum>1)
	{
	//printf("!%d\n",*(unsigned short int*)temp);
	cksum+=*(unsigned short int *)temp;
	temp += 2;
	//printf("%d\n",cksum);
	checksum-=sizeof(unsigned short int);
	}
	if(checksum)
	{
	cksum+=*(unsigned char *)temp;
	}
	
	while (cksum>>16)
		cksum=(cksum>>16)+(cksum & 0xffff);
	printf("checkchecksum %d\n",~cksum);
  	if( (unsigned short)(~cksum) != 0 )
	{
		// TODO:
		printf("check sum error!\n");

		return -1;
		//return 0;
	}
	else
	{
		return 0;
	}
}
