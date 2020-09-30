#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
struct SBCP_Header{//the header of SBCP includes 9 bits of vrsn, 7 bits of type, 2 bytes of length, payload is the attribute
	unsigned int vrsn : 9;
	unsigned int type : 7;
	int length;
};
struct SBCP_Attribute{//the attribute of SBCP includes 2 bytes of type, 2 bytes of length, and the payload is the message
	int type;
	int length;
	char payload[512];
};
struct SBCP_Message{//the complete SBCP message includes the header and 0 or more attributes
	struct SBCP_Header header;
	struct SBCP_Attribute attribute[2];
};
struct client_info{
	char username[16];
	int fd;
};
int total_count = 0;//counting the total clients in server`s local recorded client list
struct client_info *client_list;//the the list of all clients with their information

//checks if username exist already
int check_username(char a[]){//
	int i = 0;
	for(i = 0; i < total_count ; i++){//go through all recorded clients fd
		if(!strcmp(a,client_list[i].username)){//compare the new username whether is existed in our username list
			return 1;//yes! we have username existed
		}
		if(total_count == 3){
			return 2;
		}
	}
	return 0;//no! we don`t have username existed
}

void sendACK(int accept_socket_fd){//send ACK message to the new client
	struct SBCP_Message ACK_Message;
	struct SBCP_Header ACK_Message_header;
	struct SBCP_Attribute ACK_Message_attribute;
	int i = 0;
	char temp[128];
	temp[0] = (char)(((int)'0')+ total_count);//the order of new client in the server
	temp[1] = ' ';
	temp[2] = '\0';//
	for(i =0; i<total_count-1; i++)
	{
		strcat(temp,client_list[i].username);//copy and paste all client`s username in our recorded client list into the message, as the ACK message attribute payload
		strcat(temp, ";");//add ; between username
	}
	ACK_Message_header.vrsn=3;//protocol version
	ACK_Message_header.type=7;//7 is the ACK message
	ACK_Message_attribute.type = 4;//the payload in the attribute is message
	ACK_Message_attribute.length = strlen(temp)+1;//the length of the attribute is the size of message plus 1
	strcpy(ACK_Message_attribute.payload, temp);
	ACK_Message.header = ACK_Message_header;
	ACK_Message.attribute[0] = ACK_Message_attribute;
	write(accept_socket_fd,(void *) &ACK_Message,sizeof(ACK_Message));//send the ACK message back to the new client
}

void sendNAK(int accept_socket_fd,int code){//send NAK message to the new client
	struct SBCP_Message NAK_Message;
	struct SBCP_Header NAK_Message_header;
	struct SBCP_Attribute NAK_Message_attribute;
	char temp[128];
	NAK_Message_header.vrsn =3;//protocol version is 3
	NAK_Message_header.type =5;//indicate the sbcp message type, 5 is the NAK message
	NAK_Message_attribute.type = 1;//1 means the attribute`s payload include the reason of failure
	if(code == 1){//the flag to mark this NAK is for username existed
		strcpy(temp,"the username has already existed\n");//
	}
	if(code == 2){
		strcpy(temp,"the server has reached the maximum client number\n");//
	}
	NAK_Message_attribute.length = strlen(temp);//the length of the payload is depends on the message we write
	strcpy(NAK_Message_attribute.payload, temp);
	NAK_Message.header = NAK_Message_header;
	NAK_Message.attribute[0] = NAK_Message_attribute;
	write(accept_socket_fd,(void *) &NAK_Message,sizeof(NAK_Message));//send the NAK message to the new client
	close(accept_socket_fd);//since it`s NAK message, close this client socket
}

int socketinitialization(char const *argv[]){
	struct sockaddr_in servAddr;
	struct hostent* hret;
	int listening_channel;
	listening_channel = socket(AF_INET,SOCK_STREAM,0);//create the server socket
	printf("server socket created! \n");
	bzero(&servAddr,sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	hret = gethostbyname(argv[1]);
	memcpy(&servAddr.sin_addr.s_addr, hret->h_addr,hret->h_length);
	servAddr.sin_port = htons(atoi(argv[2]));
	bind(listening_channel, (struct sockaddr*)&servAddr, sizeof(servAddr));
	printf("Server socket binded! \n");
	listen(listening_channel, 99);
	printf("Server is listening! \n");
	return listening_channel;
}

void broadcast_all(struct SBCP_Message broadcast_message,fd_set recorded_client_fd_list,int listening_channel,int accept_socket_fd, int latest_fd){
	for(int j = 0; j <= latest_fd; j++) //
	{
		if (FD_ISSET(j, &recorded_client_fd_list)) //check the broadcasting fd is valid or not
		{
		// except the listener and ourselves
			if (j != listening_channel && j != accept_socket_fd)//dont broadcast to the original sender and the server
			{
				if ((write(j,(void *) &broadcast_message,sizeof(broadcast_message))) == -1)
				{
					printf("FAIL! the server cannot broadcast message\n");
				}
			}
		}
	}
}

int main(int argc, char const *argv[])
{
	struct SBCP_Message SEND_message;
	struct SBCP_Attribute SEND_message_attribute;
	struct SBCP_Message broadcast_message;
	int accept_socket_fd;
	int username_status = 0;
	struct sockaddr_in client_channel_info;
	fd_set recorded_client_fd_list;//this is the set of descriptors, which is in the server`s local list
	fd_set monitoring_fd_list;//this is the monitoring set of descriptors
	int latest_fd;//recordeing the latest client socket
	int temp;
	int x,y;
	int current_fd;
	int index_r;
	int client_limit=0;
	int listening_channel = socketinitialization(argv);//finish the socket initialization, the server is listening the socket now.
	client_limit=atoi(argv[3]);//the maximum number of client socket
	client_list= (struct client_info *)malloc(client_limit*sizeof(struct client_info));//set engough space for the client list
	FD_SET(listening_channel, &recorded_client_fd_list);//put the server socket into our local recorded list
	latest_fd = listening_channel;//the initial valiue of our latest socket is the server socket
	
	while(1)
	{
		monitoring_fd_list = recorded_client_fd_list;//set the monitoring descriptors list as what we recorded in local
		select(latest_fd+1, &monitoring_fd_list, NULL, NULL, NULL);//the first argument is the range of fd, which is the largest fd + 1; the second argument is the fd list we are monitoing for reading message; the third value is the fd list we are monitoring for writing message; the forth value is the fd list we are monitoring for error; the fifth value is the timeout, if null, the select is blocking until the monitoring list changes, if 0 means totally not blocking, if number, block before the number times out.
		for(current_fd=0 ; current_fd<=latest_fd ; current_fd++)//the server will scan all fd to see which fd has made changes in the monitoring fd list.
		{
			if(FD_ISSET(current_fd, &monitoring_fd_list))//to verify the fd we monitoring is the valid fd, which is in our monitoing list.
			{
				printf("current_fd:%d\n",current_fd);
				if(current_fd == listening_channel)//if the changed fd is our server socket, that means there`s a new socket coming to connect, which is the new client want to JOIN
				{
					int client_channel_len = sizeof(client_channel_info);
					accept_socket_fd = accept(listening_channel,(struct sockaddr*)&client_channel_info,&client_channel_len);
					if(accept_socket_fd < 0){printf("FAIL: server cannot accept.\n");}
					else
					{
						temp = latest_fd;//to record the latest_fd
						printf("latest_fd:%d\n",latest_fd);

						FD_SET(accept_socket_fd, &recorded_client_fd_list);//put the new client socket fd into our local recorded client fd list
						if(accept_socket_fd > latest_fd){//to make sure this is the largest fd we have now
							latest_fd = accept_socket_fd;//
						}
						struct SBCP_Message JOIN_Message;
						struct SBCP_Attribute JOIN_MessageAttribute;
						char index_msg[16];
						read(accept_socket_fd,(struct SBCP_Message *) &JOIN_Message,sizeof(JOIN_Message));//read what`s the message from the client
						JOIN_MessageAttribute = JOIN_Message.attribute[0];//get the first attribute in the message, which will include the username
						strcpy(index_msg, JOIN_MessageAttribute.payload);//remember to only operate the message in the index_msg, so the message in the JOIN_Message would not change
						
						//check the user name has existed?
						username_status = check_username(index_msg);
						if(username_status == 0)//0 means there`s no username existed, so this username is availeble
						{
							strcpy(client_list[total_count].username, index_msg);//update the new client into the client list
							client_list[total_count].fd = accept_socket_fd;
							printf("client_list:%s\n", client_list[total_count].username);

							
							total_count = total_count + 1;//our total count of the clients + 1
							printf("total_count:%d\n",total_count);

							//send ACK message to this client
							sendACK(accept_socket_fd);
							printf("accept_socket_fd: %d\n",accept_socket_fd);

							//broadcast ONLINE message to all clients except this client
							printf("New client in the chat room : %s \n",client_list[total_count-1].username);
							broadcast_message.header.vrsn=3;//the protocol version
							broadcast_message.header.type=8;//8 means ONLINE message
							broadcast_message.attribute[0].type=2;//type 2 attribute means username in the payload
							strcpy(broadcast_message.attribute[0].payload,client_list[total_count-1].username);
							broadcast_all(broadcast_message,recorded_client_fd_list,listening_channel,accept_socket_fd, latest_fd);
						}
						//the new connection has the username existed, the username is not available
						else
						{
							if(username_status == 2){
								printf("The maximum number of client has reached. Close the client socket. \n");
								sendNAK(accept_socket_fd,2);
							}
							else{
								printf("Username already exists. Close the client socket. \n");
								sendNAK(accept_socket_fd, 1); // send NAK message to client, 1 is for the flag of username comflict
							}
							latest_fd = temp;//return the latest fd to the previous one
							printf("latest_fd: %d\n", latest_fd);
						
							FD_CLR(accept_socket_fd, &recorded_client_fd_list);//clear accept_socket_fd out of the recorded client list
						}
					}
				}
				//this is not a new connection, means the existed fd has change
				else
				{
					if ((index_r=read(current_fd,(struct SBCP_Message *) &SEND_message,sizeof(SEND_message))) <= 0)
					{//the error or 0 situation will cause the server close the socket
						int tp=0;
						if (index_r == 0) //client close socket, the server will receive 0 from the client
						{
							printf("current_fd: %d\n",current_fd);
							//broadcast the OFFLINE message to all other clients
						
							for(y=0;y<total_count+1;y++)//check all the clients to see the username corresponding to this client.
							{
								
								if(client_list[y].fd==current_fd)
								{
									printf("y=%d\n",y);
									tp=y;
									broadcast_message.attribute[0].type=2;//means the message attribute payload is username
									strcpy(broadcast_message.attribute[0].payload,client_list[y].username);
									printf("client: %s left\n", client_list[y].username);
								}
							}
							broadcast_message.header.vrsn=3;//protocol version
							broadcast_message.header.type=6;//offline message type
							broadcast_all(broadcast_message,recorded_client_fd_list,listening_channel,current_fd, latest_fd);
						}//end of the offline broadcast
						else if(index_r < 0)
						{
							printf("FAIL! the server cannot read. \n");
						}
						//close(current_fd); // close the client socket
						//FD_CLR(current_fd, &recorded_client_fd_list); // remove from recorded_client_fd_list set
						for(x=tp;x<total_count;x++)//left move the following fd after removing the current fd
						{
							client_list[x]=client_list[x+1];
							printf("client_list: %s\n",client_list[x].username);
						}

						close(current_fd);
						FD_CLR(current_fd, &recorded_client_fd_list);
						total_count--;//the total number of clients -1
					}//end of error or close sitaution
					//the message is coming from one client
					else
					{
						SEND_message_attribute = SEND_message.attribute[0];
						broadcast_message=SEND_message;//copy the whole message which is going to FWD
						broadcast_message.header.type=3;//3 means FWD, which original is 4 as the SEND message
						broadcast_message.attribute[1].type=2;//we use the second attribute in the SBCP message to recorded the original username
						int payloadLength=0;
						char temp[16];
						payloadLength=strlen(SEND_message_attribute.payload);
						strcpy(temp,SEND_message_attribute.payload);
						temp[payloadLength]='\0';//
						for(y=0;y<total_count;y++)//to find the corresponding username of this fd
						{
							if(client_list[y].fd==current_fd)
							{
								strcpy(broadcast_message.attribute[1].payload,client_list[y].username);
								printf("The FWDing message from client : %s is \n %s \n",client_list[y].username,temp);
							}
						}
						broadcast_all(broadcast_message,recorded_client_fd_list,listening_channel,current_fd, latest_fd);
					}//end of the message come fromm existed client
				}//end of the situation the username has existed
			}//end of checking the fd is in our monitoring list
		}//end of server scanning all fd
	}//end of server`s while loop
	close(listening_channel);
	return 0;
}
