#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/errno.h>
//SBCP Header
struct HeaderSBCP{
	unsigned int vrsn : 9;
	unsigned int type : 7;
	int length;
};
//SBCP Attribute
struct AttributeSBCP
{
	int type;
	int length;
	char payload[512];
};
//SBCP Message
struct MsgSBCP
{
	
	struct HeaderSBCP header;
	struct AttributeSBCP attribute[2];
};

//read message from the server
int getServerMsg(int sockfd){
	struct MsgSBCP serverMsg;
	int status = 0;
	read(sockfd, (struct MsgSBCP *) &serverMsg, sizeof(serverMsg));
	if (serverMsg.header.type == 3)
	{
		if ((serverMsg.attribute[0].payload != NULL || serverMsg.attribute[0].payload != '\0')
		&& (serverMsg.attribute[1].payload != NULL || serverMsg.attribute[1].payload != '\0')
		&& serverMsg.attribute[0].type == 4 && serverMsg.attribute[1].type == 2)
		{
			printf("FWD message from %s: %s", serverMsg.attribute[1].payload, serverMsg.attribute[0].payload);
		}
		status=0;
	}
	if (serverMsg.header.type == 5)
	{
		if ((serverMsg.attribute[0].payload != NULL || serverMsg.attribute[0].payload != '\0')
		&& serverMsg.attribute[0].type == 1)
		{
			printf("NAK message from server: %s", serverMsg.attribute[0].payload);
		}
		status=1;
	}
	if (serverMsg.header.type == 6)
	{
		if ((serverMsg.attribute[0].payload != NULL || serverMsg.attribute[0].payload != '\0')
		&& serverMsg.attribute[0].type == 2)
		{
			printf("OFFLINE message: Client (%s) is now offline.", serverMsg.attribute[0].payload);
		}
		status=0;
	}
	if (serverMsg.header.type == 7)
	{
		if ((serverMsg.attribute[0].payload != NULL || serverMsg.attribute[0].payload != '\0')
		&& serverMsg.attribute[0].type == 4)
		{
			printf("ACK message from server: (%s)", serverMsg.attribute[0].payload);
		}
		status=0;
	}
	if (serverMsg.header.type == 8)
	{
		if ((serverMsg.attribute[0].payload != NULL || serverMsg.attribute[0].payload != '\0')
		&& serverMsg.attribute[0].type == 0)//username
		{
			printf("ONLINE message: Client (%s) is now online.", serverMsg.attribute[0].payload);
		}
		status=0;
	}
	if (serverMsg.header.type == 9)
	{
		if((serverMsg.attribute[0].payload != NULL || serverMsg.attribute[0].payload != '\0')
		&& serverMsg.attribute[0].type == 2){
			printf("IDLE message: Client (%s) is idle.", serverMsg.attribute[0].payload);
		}
		status=0;
	}
	return status;
}

// initiate a JOIN with the server
void sendJoin(int sockfd, const char *arg[]){
	struct HeaderSBCP header;
	struct AttributeSBCP attr;
	struct MsgSBCP msg;
	int status = 0;
	header.vrsn = '3';
	header.type = '2';// JOIN type: 2
	attr.type = 2; // attribute: username
	attr.length = strlen(arg[1]) + 1;
	strcpy(attr.payload,arg[1]);
	msg.header = header;
	msg.attribute[0] = attr;
	write(sockfd,(void *) &msg, sizeof(msg));
	// wait for the server_reply
	sleep(1);
	status = getServerMsg(sockfd);
	if (status == 1) {
		close(sockfd);
	}
}

// accept user_input and send it to the server to wait for the broadcasting
void chat(int connectionDesc){
	struct MsgSBCP msg;
	struct AttributeSBCP clientAttr;
	int nread = 0;
	char temp[512];
	struct timeval tv;
	fd_set readfds;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);
	select(STDIN_FILENO+1, &readfds, NULL, NULL, &tv);
	if (FD_ISSET(STDIN_FILENO, &readfds))
	{
		nread = read(STDIN_FILENO, temp, sizeof(temp));
	if (nread > 0)
	{
		temp[nread] = '\0';
	}
	clientAttr.type = 4;
	strcpy(clientAttr.payload, temp);
	msg.attribute[0] = clientAttr;
	write(connectionDesc, (void *) &msg, sizeof(msg));
	}
	else
	{
		printf("Timed out!\n");
	}
}
int main(int argc, char const *argv[])
{
	if (argc == 4)
	{
		int sockfd;
		struct sockaddr_in servaddr;
		struct hostent* hret;
		fd_set master;
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_ZERO(&master);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		struct MsgSBCP *msg_to_server;
		char temp[512];
		
		struct timeval tv;
		int idle=0;
		int act;
		
		
		
		memset(&tv, 0, sizeof(struct timeval));
		if (sockfd == -1)
		{
			printf("socket creating failed!\n");
			exit(0);
		}
		else
		{
			printf("socket successfully created!\n");
		}
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		hret = gethostbyname(argv[2]);
		memcpy(&servaddr.sin_addr.s_addr, hret->h_addr, hret->h_length);
		servaddr.sin_port = htons(atoi(argv[3]));
		if (connect(sockfd,(struct sockaddr *)&servaddr, sizeof(servaddr))!=0)
		{
			printf("connecting to server failed!\n");
			exit(0);
		}
		else
		{
			printf("connected to the server!\n");
			sendJoin(sockfd, argv);
			FD_SET(sockfd, &master);
			FD_SET(STDIN_FILENO, &master);
			
			//initialize select timeout to 10 sec
			tv.tv_sec=10;
			tv.tv_usec=0;
			for(;;)
			{
				read_fds = master;
				printf("\n");
				if ((act=select(sockfd+1, &read_fds, NULL, NULL, &tv)) == -1)
				{
					perror("select");
					exit(4);
				}
				else if((act==0)&&(idle==0)){
					/* // Setup IDLE message to server
					
					msg_to_server = (struct MsgSBCP *) malloc(sizeof(struct MsgSBCP));
					
					msg_to_server->header.vrsn = 3;    // protocol version is 3
					msg_to_server->header.type = 9; // SBCP message type is SEND to send message to server
					msg_to_server->header.length = 520; // 4 bytes for vsrn and type, and 516 bytes for attribute
					msg_to_server->attribute[0].type = 0; // indicates that payload stores none
					msg_to_server->attribute[0].length = 516; // 4 bytes for type and length, and 512 bytes for message
					memset(msg_to_server->attribute[0].payload, 0, 512 * sizeof(char)); // Nothing to send to the chat room
					
					// Written operation
					if (write(sockfd, msg_to_server, sizeof(struct MsgSBCP)) < 0) {
						printf("WRITE: ERRNO: \t%s\n", strerror(errno));
						free(msg_to_server); // release malloc of msg_to_server
						return -1;
					}
					printf("Timeout...\n");
					free(msg_to_server); // release malloc of msg_to_server */
					printf("Timed out.\n");
					idle = 1; // indicate that the client enters into idle state
				}
				
				if (FD_ISSET(STDIN_FILENO, &read_fds))
				{
					chat(sockfd);
					idle = 0; // client leave the idle state after receiving message from stdin
					
					// A message is detected from stdin, so the select timeout needs to reset back to 10 sec
					tv.tv_sec = 10;
					tv.tv_usec = 0;
					
				}
				if (FD_ISSET(sockfd, &read_fds))
				{
					getServerMsg(sockfd);
				}
			}
			printf("\n initConnection ends. \n");
		}
	}
	printf("\n client closed. \n");
	return 0;
}
