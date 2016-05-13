//ÎÄ¼þÃû: server/app_stress_server.c

//ÃèÊö: ÕâÊÇÑ¹Á¦²âÊÔ°æ±¾µÄ·þÎñÆ÷³ÌÐò´úÂë. ·þÎñÆ÷Ê×ÏÈÁ¬½Óµ½±¾µØSIP½ø³Ì. È»ºóËüµ÷ÓÃstcp_server_init()³õÊ¼»¯STCP·þÎñÆ÷.
//ËüÍ¨¹ýµ÷ÓÃstcp_server_sock()ºÍstcp_server_accept()´´½¨Ì×½Ó×Ö²¢µÈ´ýÀ´×Ô¿Í»§¶ËµÄÁ¬½Ó. ËüÈ»ºó½ÓÊÕÎÄ¼þ³¤¶È. 
//ÔÚÕâÖ®ºó, Ëü´´½¨Ò»¸ö»º³åÇø, ½ÓÊÕÎÄ¼þÊý¾Ý²¢½«Ëü±£´æµ½receivedtext.txtÎÄ¼þÖÐ.
//×îºó, ·þÎñÆ÷Í¨¹ýµ÷ÓÃstcp_server_close()¹Ø±ÕÌ×½Ó×Ö, ²¢¶Ï¿ªÓë±¾µØSIP½ø³ÌµÄÁ¬½Ó.

//´´½¨ÈÕÆÚ: 2015Äê

//ÊäÈë: ÎÞ

//Êä³ö: STCP·þÎñÆ÷×´Ì¬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "stcp_server.h"

//´´½¨Ò»¸öÁ¬½Ó, Ê¹ÓÃ¿Í»§¶Ë¶Ë¿ÚºÅ87ºÍ·þÎñÆ÷¶Ë¿ÚºÅ88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88
//ÔÚ½ÓÊÕµÄÎÄ¼þÊý¾Ý±»±£´æºó, ·þÎñÆ÷µÈ´ý15Ãë, È»ºó¹Ø±ÕÁ¬½Ó.
#define WAITTIME 15
#define LISTENQ 8
//Õâ¸öº¯ÊýÁ¬½Óµ½±¾µØSIP½ø³ÌµÄ¶Ë¿ÚSIP_PORT. Èç¹ûTCPÁ¬½ÓÊ§°Ü, ·µ»Ø-1. Á¬½Ó³É¹¦, ·µ»ØTCPÌ×½Ó×ÖÃèÊö·û, STCP½«Ê¹ÓÃ¸ÃÃèÊö·û·¢ËÍ¶Î.
int connectToSIP() {

    struct sockaddr_in servaddr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //AF_INET for ipv4; SOCK_STREAM for byte stream
    if(sockfd < 0) {
        printf("Socket error!\n");
        return 0;
    }
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = get_local_ip();
    servaddr.sin_port = htons(SIP_PORT);
    //connect to the server
    if(connect(sockfd, (struct sockaddr* )&servaddr, sizeof(servaddr)) < 0) {//创建套接字连接服务器
        printf("Link Wrong!\n");
        exit(1);
    }
    else
        printf("Link Success!\n");
    connfd = sockfd;
    return sockfd;
}

//Õâ¸öº¯Êý¶Ï¿ªµ½±¾µØSIP½ø³ÌµÄTCPÁ¬½Ó. 
void disconnectToSIP(int sip_conn) {

  //ÄãÐèÒª±àÐ´ÕâÀïµÄ´úÂë.
    printf("disconnectToSIP\n");
        close(sip_conn);
}

int main() {
	//ÓÃÓÚ¶ª°üÂÊµÄËæ»úÊýÖÖ×Ó
	srand(time(NULL));

	//Á¬½Óµ½SIP½ø³Ì²¢»ñµÃTCPÌ×½Ó×ÖÃèÊö·û
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//³õÊ¼»¯STCP·þÎñÆ÷
	stcp_server_init(sip_conn);

	//ÔÚ¶Ë¿ÚSERVERPORT1ÉÏ´´½¨STCP·þÎñÆ÷Ì×½Ó×Ö 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//¼àÌý²¢½ÓÊÜÀ´×ÔSTCP¿Í»§¶ËµÄÁ¬½Ó 
	stcp_server_accept(sockfd);

	//Ê×ÏÈ½ÓÊÕÎÄ¼þ³¤¶È, È»ºó½ÓÊÕÎÄ¼þÊý¾Ý
	int fileLen;
	stcp_server_recv(sockfd,&fileLen,sizeof(int));
	char* buf = (char*) malloc(fileLen);
	stcp_server_recv(sockfd,buf,fileLen);

	//½«½ÓÊÕµ½µÄÎÄ¼þÊý¾Ý±£´æµ½ÎÄ¼þreceivedtext.txtÖÐ
	FILE* f;
	f = fopen("receivedtext.txt","w");
	fwrite(buf,fileLen,1,f);
	fclose(f);
	free(buf);

	//µÈ´ýÒ»»á¶ù
	sleep(WAITTIME);

	//¹Ø±ÕSTCP·þÎñÆ÷ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//¶Ï¿ªÓëSIP½ø³ÌÖ®¼äµÄÁ¬½Ó
	disconnectToSIP(sip_conn);
}
