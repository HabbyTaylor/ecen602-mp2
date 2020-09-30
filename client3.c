#include <stdio.h>
#include "string.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdlib.h>




// PAYLOAD LENGTH
#define MAXLINE 512
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

struct SBCP_ATTRIBUTE{
    unsigned int type : 16;
    unsigned int length : 16;
    char payload[512]; // maximum size of MESSAGE equals to 512 bytes
};

struct SBCP_MSG{
    unsigned int vrsn : 9;
    unsigned int type : 7;
    unsigned int length : 16;
    struct SBCP_ATTRIBUTE attribute[2];
};


int main(int argc, char **argv){
    
    if (argc != 4) {
        errno = EPERM;
        printf("CORRECT FORMAT: ./client USERNAME SERVER_NAME SERVER_PORT \n");
        printf("INPUT_ERROR: ERRNO: \t%s\n", strerror(errno));
        return -1;
    }
    char* username = argv[1];
    char* server_name = argv[2];
    char* _server_port = argv[3];
    
    int server_port = -1;
    int socketfd;
    char input[MAXLINE];
    struct sockaddr_in server_addr;
    
    struct SBCP_MSG *msg_to_server;
    struct SBCP_MSG *msg_from_server;
    struct timeval tv;
    fd_set master;
	fd_set readfd;
	
	int act;
	int idle = 0; // variable indicates whether the client is idle or not
    
    // Reset
    memset(&tv, 0, sizeof(struct timeval));
    
    
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("SOCKET_OPEN: ERRNO: \t%s\n", strerror(errno));
        return -1;
    }
    printf("Open Socket succeed\n");
    
    // Input the server port to be connected
    server_port = atoi(_server_port); // Transform format of server port from string to integer
    printf("Server port set is: %d\n", server_port);
    
    memset(&server_addr, 0, sizeof(server_addr)); // Set all bits of server address to zero
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_name, &server_addr.sin_addr) < 0) {
        printf("ADDR_TRANS: ERRNO: \t%s\n", strerror(errno));
        return -1;
    }
    printf("Server name connected to is: %s\n", server_name);
    // Connect the socket to the server
    if ((connect(socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0) {
        printf("CONNECT: RRNO: \t%s\n", strerror(errno));
        return -1;
    }
    
    // Send JOIN message to server
    msg_to_server = (struct SBCP_MSG *) malloc(sizeof(struct SBCP_MSG));
    
    msg_to_server->vrsn = 3;    // protocol version is 3
    msg_to_server->type = JOIN; // SBCP message type is JOIN to join server
    msg_to_server->length = 24; // 4 bytes for vsrn and type, and 20 bytes for attribute
    msg_to_server->attribute[0].type = USERNAME; // indicates that payload stores username
    msg_to_server->attribute[0].length = 20; // 4 bytes for type and length, and 16 bytes for username
    strcpy(msg_to_server->attribute[0].payload, username); // copy string username to payload
    
    printf("Connecting to the chat room......\n");
    if ((write(socketfd, msg_to_server, sizeof (struct SBCP_MSG))) < 0) {
        printf("JOIN: ERRNO: \t%s\n", strerror(errno));
        free(msg_to_server); // release malloc of msg_to_server
        return -1;
    }
    
    printf("Welcome to the chat room\n");
    printf("Press Ctrl + c to exit...\n");
    
    free(msg_to_server); // release malloc of msg_to_server
	
	// Initialize select timeout to 10 sec
	tv.tv_sec = 10;
	tv.tv_usec = 0;
    
    while (1) {
        // Setup select function
        FD_ZERO(&master); // Clear the set before using file descriptor set "master"
        
        FD_SET(fileno(stdin), &master); // Add stdin file descriptor to the select set
        FD_SET(socketfd, &master); // Add socket file descroptor to the select set
		
		
        if ((act = select(socketfd + 1, &master, NULL, NULL, &tv)) < 0) {
            printf("MASTER SELECT: ERRNO: \t%s\n", strerror(errno));
            return -1;
        }
		else if ((act == 0) && (idle == 0)) { // timeout when return 0 and the client is not in IDLE state currently
			// Setup IDLE message to server
            msg_to_server = (struct SBCP_MSG *) malloc(sizeof(struct SBCP_MSG));
            
            msg_to_server->vrsn = 3;    // protocol version is 3
            msg_to_server->type = IDLE; // SBCP message type is SEND to send message to server
            msg_to_server->length = 520; // 4 bytes for vsrn and type, and 516 bytes for attribute
            msg_to_server->attribute[0].type = 0; // indicates that payload stores none
            msg_to_server->attribute[0].length = 516; // 4 bytes for type and length, and 512 bytes for message
            memset(msg_to_server->attribute[0].payload, 0, 512 * sizeof(char)); // Nothing to send to the chat room
			
            // Written operation
            if (write(socketfd, msg_to_server, sizeof(struct SBCP_MSG)) < 0) {
                printf("WRITE: ERRNO: \t%s\n", strerror(errno));
                free(msg_to_server); // release malloc of msg_to_server
                return -1;
            }
            printf("Timeout...\n");
            free(msg_to_server); // release malloc of msg_to_server
			idle = 1; // indicate that the client enters into idle state
		}
		// When user input to the terminal
		if (FD_ISSET(fileno(stdin), &master)) {
			
			// Input string into the client
			fgets(input, sizeof(input), stdin);
            
			// Setup SEND message to server
			msg_to_server = (struct SBCP_MSG *) malloc(sizeof(struct SBCP_MSG));
			msg_to_server->vrsn = 3;    // protocol version is 3
			msg_to_server->type = SEND; // SBCP message type is SEND to send message to server
			msg_to_server->length = 520; // 4 bytes for vsrn and type, and 516 bytes for attribute
			msg_to_server->attribute[0].type = MESSAGE; // indicates that payload stores message
			msg_to_server->attribute[0].length = 516; // 4 bytes for type and length, and 512 bytes for message
			strcpy(msg_to_server->attribute[0].payload, input); // copy string input to payload
            
			// Written operation
			if (write(socketfd, msg_to_server, sizeof(struct SBCP_MSG)) < 0) {
				printf("WRITE: ERRNO: \t%s\n", strerror(errno));
				free(msg_to_server); // release malloc of msg_to_server
				return -1;
			}
			free(msg_to_server); // release malloc of msg_to_server
			idle = 0; // client leave the idle state after receiving message from stdin
			
			// A message is detected from stdin, so the select timeout needs to reset back to 10 sec
			tv.tv_sec = 10;
			tv.tv_usec = 0;
        
		}
			
        
        // When receiving message from server
		else if (FD_ISSET(socketfd, &master)) {
			msg_from_server = (struct SBCP_MSG *) malloc(sizeof(struct SBCP_MSG));
			// Read messages from the socket
			if ((read(socketfd , msg_from_server, sizeof(struct SBCP_MSG))) < 0) {
				printf("READ: ERRNO: \t%s\n", strerror(errno));
				return -1;
			}
            
			// Check type of message from server
			
			// print the attribute directly when receive forward message from server
			if (msg_from_server->type == FWD) {  
				if (msg_from_server->attribute[0].type == MESSAGE) {
					printf("%s\n", msg_from_server->attribute[0].payload);
				}
			}
			// when receiving ACK message from server, print the number of clients connected to the server, 
			// all the client name belonging to the server respectively
			else if (msg_from_server->type == ACK) { 
				if (msg_from_server->attribute[0].type == CLIENT_COUNT){
					printf("Number of clients in the chat room: %s\n", msg_from_server->attribute[0].payload);
				}
				if (msg_from_server->attribute[1].type == MESSAGE) {
					printf("Users in the chat room: %s\n", msg_from_server->attribute[1].payload);
				}
			}
			// when receiving NAK message from server, print the reason why the client could not join the server
			else if (msg_from_server->type == NAK) {
				if (msg_from_server->attribute[0].type == REASON){
					printf("CANNOT JOIN THE CHAT ROOM: %s\n", msg_from_server->attribute[0].payload);
				}
				close(socketfd);
				return 0;
			}
			// ONLINE message indicates that a new client joins the server, and the client will print the name of
			// the new cleint 
			else if (msg_from_server->type == ONLINE) {
				if (msg_from_server->attribute[0].type == USERNAME){
					printf("%s just joined into the chat room...\n", msg_from_server->attribute[0].payload);
				}	
			}
			// OFFNLINE message indicates that a client leaves the server, and the client will print the name of
			// the left cleint 
			else if (msg_from_server->type == OFFLINE) {
				if (msg_from_server->attribute[0].type == USERNAME){
					printf("%s just leaved the chat room...\n", msg_from_server->attribute[0].payload);
				}
			}
			// IDLE message indicates that a client has not sent any message for 10 sec , and the client 
			// will print the name of the idle client
			else if (msg_from_server->type == IDLE) {
				if (msg_from_server->attribute[0].type == USERNAME){
					printf("%s is idle...\n", msg_from_server->attribute[0].payload);
				}
			}
			free(msg_from_server); // release malloc of msg_from_server
		}
        // Reset char array for next turn
        memset(input, 0, sizeof(input));
    }
    
}

