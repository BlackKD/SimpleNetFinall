//Œƒº˛√˚: server/app_simple_server.c

//√Ë ˆ: ’‚ «ºÚµ•∞Ê±æµƒ∑˛ŒÒ∆˜≥Ã–Ú¥˙¬Î. ∑˛ŒÒ∆˜ ◊œ»¡¨Ω”µΩ±æµÿSIPΩ¯≥Ã. »ª∫ÛÀ¸µ˜”√stcp_server_init()≥ı ºªØSTCP∑˛ŒÒ∆˜. 
//À¸Õ®π˝¡Ω¥Œµ˜”√stcp_server_sock()∫Õstcp_server_accept()¥¥Ω®2∏ˆÃ◊Ω”◊÷≤¢µ»¥˝¿¥◊‘øÕªß∂Àµƒ¡¨Ω”. ∑˛ŒÒ∆˜»ª∫ÛΩ” ’¿¥◊‘¡Ω∏ˆ¡¨Ω”µƒøÕªß∂À∑¢ÀÕµƒ∂Ã◊÷∑˚¥Æ. 
//◊Ó∫Û, ∑˛ŒÒ∆˜Õ®π˝µ˜”√stcp_server_close()πÿ±’Ã◊Ω”◊÷, ≤¢∂œø™”Î±æµÿSIPΩ¯≥Ãµƒ¡¨Ω”.

//¥¥Ω®»’∆⁄: 2015ƒÍ

// ‰»Î: Œﬁ

// ‰≥ˆ: STCP∑˛ŒÒ∆˜◊¥Ã¨

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

//¥¥Ω®¡Ω∏ˆ¡¨Ω”, “ª∏ˆ π”√øÕªß∂À∂Àø⁄∫≈87∫Õ∑˛ŒÒ∆˜∂Àø⁄∫≈88. ¡Ì“ª∏ˆ π”√øÕªß∂À∂Àø⁄∫≈89∫Õ∑˛ŒÒ∆˜∂Àø⁄∫≈90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//‘⁄Ω” ’µΩ◊÷∑˚¥Æ∫Û, µ»¥˝15√Î, »ª∫Ûπÿ±’¡¨Ω”.
#define WAITTIME 15
#define LISTENQ 8
//’‚∏ˆ∫Ø ˝¡¨Ω”µΩ±æµÿSIPΩ¯≥Ãµƒ∂Àø⁄SIP_PORT. »Áπ˚TCP¡¨Ω” ß∞‹, ∑µªÿ-1. ¡¨Ω”≥…π¶, ∑µªÿTCPÃ◊Ω”◊÷√Ë ˆ∑˚, STCPΩ´ π”√∏√√Ë ˆ∑˚∑¢ÀÕ∂Œ.
int connectToSIP() {

	//ƒ„–Ë“™±‡–¥’‚¿Ôµƒ¥˙¬Î.
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

//’‚∏ˆ∫Ø ˝∂œø™µΩ±æµÿSIPΩ¯≥ÃµƒTCP¡¨Ω”. 
void disconnectToSIP(int sip_conn) {

  //ƒ„–Ë“™±‡–¥’‚¿Ôµƒ¥˙¬Î.
    printf("disconnectToSIP\n");
        close(sip_conn);
}

int main() {
	//”√”⁄∂™∞¸¬ µƒÀÊª˙ ˝÷÷◊”
	srand(time(NULL));

	//¡¨Ω”µΩSIPΩ¯≥Ã≤¢ªÒµ√TCPÃ◊Ω”◊÷√Ë ˆ∑˚
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//≥ı ºªØSTCP∑˛ŒÒ∆˜
	stcp_server_init(sip_conn);

	//‘⁄∂Àø⁄SERVERPORT1…œ¥¥Ω®STCP∑˛ŒÒ∆˜Ã◊Ω”◊÷ 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//º‡Ã˝≤¢Ω” ‹¿¥◊‘STCPøÕªß∂Àµƒ¡¨Ω” 
	stcp_server_accept(sockfd);

	//‘⁄∂Àø⁄SERVERPORT2…œ¥¥Ω®¡Ì“ª∏ˆSTCP∑˛ŒÒ∆˜Ã◊Ω”◊÷
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//º‡Ã˝≤¢Ω” ‹¿¥◊‘STCPøÕªß∂Àµƒ¡¨Ω” 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//Ω” ’¿¥◊‘µ⁄“ª∏ˆ¡¨Ω”µƒ◊÷∑˚¥Æ
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//Ω” ’¿¥◊‘µ⁄∂˛∏ˆ¡¨Ω”µƒ◊÷∑˚¥Æ
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd2,buf2,7);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//πÿ±’STCP∑˛ŒÒ∆˜ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//∂œø™”ÎSIPΩ¯≥Ã÷Æº‰µƒ¡¨Ω”
	disconnectToSIP(sip_conn);
}
