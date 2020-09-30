#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// HEADER TYPE
#define JOIN 2
#define SEND 4
#define FWD 3
#define ACK 7
#define NAK 5
#define ONLINE 8
#define OFFLINE 6
#define IDLE 9

// ATTRIBUTE TYPE
#define USERNAME 2
#define MESSAGE 4
#define REASON 1
#define CLIENT_COUNT 3

struct SBCP_header{
	unsigned int vrsn : 9;
	unsigned int type : 7;
	int length;
};
struct SBCP_attribute{
	int type;
	int length;
	char payload[512];
};
struct SBCP_message{
	struct SBCP_header header;
	struct SBCP_attribute attribute[2];
};
struct SBCP_client_info{
	char username[16];
	int fd;
};

int totalClientNum = 0;//compute the total client number
struct SBCP_client_info *clientLst;//the the list of all clients with their information

//checks if username exist already
int checkUsername(char a[], int maxClientNum){//
	int i = 0;
	while(i<totalClientNum){//go through all recorded clients fd
		if(!strcmp(a,clientLst[i].username)){//compare the new username whether is existed in our username list
			return 1;//yes! we have username existed
		}
		if(totalClientNum == maxClientNum){
			return 2;
		}
		i++;
	}
	return 0;//no! we don`t have username existed
}

void sendACK(int acceptFd){//send ACK message to the new client
	struct SBCP_message ACKMsg;
	struct SBCP_header ACKMsgHeader;
	struct SBCP_attribute ACKMsgAttr;
	int i = 0;
	char temp[128];
	temp[0] = (char)(((int)'0')+ totalClientNum);//the order of new client in the server
	temp[1] = ' ';
	temp[2] = '\0';//
	for(i =0; i<totalClientNum-1; i++)
	{
		strcat(temp,clientLst[i].username);
		strcat(temp, ";");
	}
	ACKMsgHeader.vrsn=FWD;
	ACKMsgHeader.type=ACK;
	ACKMsgAttr.type = MESSAGE;
	ACKMsgAttr.length = strlen(temp)+1;
	strcpy(ACKMsgAttr.payload, temp);
	ACKMsg.header = ACKMsgHeader;
	ACKMsg.attribute[0] = ACKMsgAttr;
	write(acceptFd,(void *) &ACKMsg,sizeof(ACKMsg));
}

void sendNAK(int acceptFd,int flag){
	struct SBCP_message NAKMsg;
	struct SBCP_header NAKMsgHeader;
	struct SBCP_attribute NAKMsgAttr;
	char temp[128];
	NAKMsgHeader.vrsn =FWD;
	NAKMsgHeader.type =NAK;
	NAKMsgAttr.type = REASON;
	if(flag == 1){
		strcpy(temp,"Username has already existed.\n");//
	}
	if(flag == 2){
		strcpy(temp,"Server has reached the maximum client number.\n");//
	}
	NAKMsgAttr.length = strlen(temp);
	strcpy(NAKMsgAttr.payload, temp);
	NAKMsg.header = NAKMsgHeader;
	NAKMsg.attribute[0] = NAKMsgAttr;
	write(acceptFd,(void *) &NAKMsg,sizeof(NAKMsg));
	close(acceptFd);
}

void sendIDLE(int currFd){
	struct SBCP_message IDLEMsg;
	struct SBCP_header IDLEMsgHeader;
	struct SBCP_attribute IDLEMsgAttr;
	char temp[128];
	IDLEMsgHeader.vrsn=3;
	IDLEMsgHeader.type=IDLE;
	IDLEMsgAttr.type=USERNAME;
	strcpy(temp, "User has been idle.\n");
	IDLEMsgAttr.length=strlen(temp);
	strcpy(IDLEMsgAttr.payload, temp);
	IDLEMsg.header=IDLEMsgHeader;
	IDLEMsg.attribute[0]=IDLEMsgAttr;
	write(currFd, (void *)&IDLEMsg, sizeof(IDLEMsg));
}

int socketinitialization(char const *argv[]){
	struct sockaddr_in serverAddress;
	struct hostent* hret;
	int listenFd;
	listenFd = socket(AF_INET,SOCK_STREAM,0);
	printf("Server socket created. \n");
	bzero(&serverAddress,sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	hret = gethostbyname(argv[1]);
	memcpy(&serverAddress.sin_addr.s_addr, hret->h_addr,hret->h_length);
	serverAddress.sin_port = htons(atoi(argv[2]));
	bind(listenFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
	printf("Server socket binded. \n");
	listen(listenFd, 99);
	printf("Server is listening... \n");
	return listenFd;
}

void broadcast(struct SBCP_message broadcastMsg,fd_set RecordedClientFdLst,int listenFd,int acceptFd, int latestFd){
	for(int i = 0; i <= latestFd; i++) 
	{
		if (FD_ISSET(i, &RecordedClientFdLst)) 
		{
			// except the listener and ourselves
			if (i != listenFd && i != acceptFd)
			{
				if ((write(i,(void *) &broadcastMsg,sizeof(broadcastMsg))) == -1)
				{
					printf("Fail. Server cannot broadcast.\n");
				}
			}
		}
	}
}

int main(int argc, char const *argv[])
{
	struct SBCP_message sendMsg;
	struct SBCP_attribute sendMsg_attribute;
	struct SBCP_message broadcastMsg;
	int acceptFd;
	int usernameFlag = 0;
	struct sockaddr_in clientInfo;
	fd_set RecordedClientFdLst;
	fd_set monitorFdLst;
	int latestFd;
	int temp;
	int x,y;
	int currFd;
	int index_r;
	int maxClientNum=0;
	int listenFd = socketinitialization(argv);
	maxClientNum=atoi(argv[3]);
	clientLst= (struct SBCP_client_info *)malloc(maxClientNum*sizeof(struct SBCP_client_info));
	FD_SET(listenFd, &RecordedClientFdLst);
	latestFd = listenFd;
	
	for(;;)
	{
		monitorFdLst = RecordedClientFdLst;
		select(latestFd+1, &monitorFdLst, NULL, NULL, NULL);
		for(currFd=0 ; currFd<=latestFd ; currFd++)
		{
			if(FD_ISSET(currFd, &monitorFdLst))
			{
				if(currFd == listenFd)
				{
					int clientSize = sizeof(clientInfo);
					acceptFd = accept(listenFd,(struct sockaddr*)&clientInfo,&clientSize);
					if(acceptFd < 0){printf("Server acception failed.\n");}
					else
					{
						temp = latestFd;
						FD_SET(acceptFd, &RecordedClientFdLst);
						if(acceptFd > latestFd){
						latestFd = acceptFd;
						}
						struct SBCP_message joinMsg;
						struct SBCP_attribute joinMsgAttribute;
						char indexMsg[16];
						read(acceptFd,(struct SBCP_message *) &joinMsg,sizeof(joinMsg));
						joinMsgAttribute = joinMsg.attribute[0];
						strcpy(indexMsg, joinMsgAttribute.payload);
						
						
						usernameFlag = checkUsername(indexMsg, maxClientNum);
						if(usernameFlag == 0)
						{
							strcpy(clientLst[totalClientNum].username, indexMsg);//update the new client into the client list
							clientLst[totalClientNum].fd = acceptFd;
							
							totalClientNum ++;//our total count of the clients + 1
							//send ACK message to this client
							sendACK(acceptFd);
							//broadcast ONLINE message to all clients except this client
							printf("New client (%s) is in the chat room.\n",clientLst[totalClientNum-1].username);
							broadcastMsg.header.vrsn=3;//the protocol version
							broadcastMsg.header.type=ONLINE;//8 means ONLINE message
							broadcastMsg.attribute[0].type=USERNAME;//type 2 attribute means username in the payload
							strcpy(broadcastMsg.attribute[0].payload,clientLst[totalClientNum-1].username);
							broadcast(broadcastMsg,RecordedClientFdLst,listenFd,acceptFd, latestFd);
						}
						//the new connection has the username existed, the username is not available
						else
						{
							if(usernameFlag == 2){
								printf("Max number of users exceeded. Connection denied. \n");
								sendNAK(acceptFd,2);
							}
							else{
								printf("This username has already existed. Connection denied. \n");
								sendNAK(acceptFd, 1); // send NAK message to client, 1 is for the flag of username comflict
							}
							latestFd = temp;//return the latest fd to the previous one
							FD_CLR(acceptFd, &RecordedClientFdLst);//clear acceptFd out of the recorded client list
						}
					}
				}
				//this is not a new connection, means the existed fd has change
				else
				{
					if(sendMsg.header.type==IDLE){//the client is idle
						sendMsg_attribute = sendMsg.attribute[0];
						broadcastMsg=sendMsg;//copy the whole message which is going to FWD
						broadcastMsg.header.type=FWD;//3 means FWD, which original is 4 as the SEND message
						broadcastMsg.attribute[1].type=USERNAME;
						int payloadLength=0;
						char temp[128];
						payloadLength=strlen(sendMsg_attribute.payload);
						strcpy(temp,sendMsg_attribute.payload);
						temp[payloadLength]='\0';
						for(y=0;y<totalClientNum;y++)//to find the corresponding username of this fd
						{
							if(clientLst[y].fd==currFd)
							{
								strcpy(broadcastMsg.attribute[1].payload,clientLst[y].username);
								printf("IDLE message from client (%s). \n",clientLst[y].username);
							}
						}
						sendIDLE(currFd);
						broadcast(broadcastMsg,RecordedClientFdLst,listenFd,currFd, latestFd);
					}
					if ((index_r=read(currFd,(struct SBCP_message *) &sendMsg,sizeof(sendMsg))) <= 0)
					{//the error or 0 situation will cause the server close the socket
						int tp=0;
						if (index_r == 0) //client close socket, the server will receive 0 from the client
						{
							//broadcast the OFFLINE message to all other clients
							for(y=0;y<totalClientNum+1;y++)//check all the clients to see the username corresponding to this client.
							{
								if(clientLst[y].fd==currFd)
								{
									tp=y;
									broadcastMsg.attribute[0].type=USERNAME;//means the message attribute payload is username
									strcpy(broadcastMsg.attribute[0].payload,clientLst[y].username);
									printf("Client (%s) has left the chat room.\n", clientLst[y].username);
								}
							}
							broadcastMsg.header.vrsn=3;
							broadcastMsg.header.type=OFFLINE;//offline message type
							broadcast(broadcastMsg,RecordedClientFdLst,listenFd,currFd, latestFd);
						}//end of the offline broadcast
						else if(index_r < 0)
						{
							printf("Server reading failed. \n");
						}
						close(currFd); // close the client socket
						FD_CLR(currFd, &RecordedClientFdLst); // remove from RecordedClientFdLst set
						
						while(tp<totalClientNum){
							clientLst[tp]=clientLst[tp+1];
							tp++;
						}
						totalClientNum--;//the total number of clients -1
					}//end of error or close sitaution
					//the message is coming from one client
					else
					{
						sendMsg_attribute = sendMsg.attribute[0];
						broadcastMsg=sendMsg;//copy the whole message which is going to FWD
						broadcastMsg.header.type=FWD;//3 means FWD, which original is 4 as the SEND message
						broadcastMsg.attribute[1].type=USERNAME;
						int payloadLength=0;
						char temp[128];
						payloadLength=strlen(sendMsg_attribute.payload);
						strcpy(temp,sendMsg_attribute.payload);
						temp[payloadLength]='\0';
						for(y=0;y<totalClientNum;y++)//to find the corresponding username of this fd
						{
							if(clientLst[y].fd==currFd)
							{
								strcpy(broadcastMsg.attribute[1].payload,clientLst[y].username);
								printf("FWD message from client (%s) is: %s \n",clientLst[y].username,temp);
							}
						}
						broadcast(broadcastMsg,RecordedClientFdLst,listenFd,currFd, latestFd);
					}
				}
			}
		}
	}
	close(listenFd);
	return 0;
}
