/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <pthread.h>

#define PORT "3490" // the port client will be connecting to 

#define MESSAGE_LEN		1024
#define USERNAME_LEN	7

void* recv_loop(void *args) {
	int sockfd = *(int*)args;
	int bytes_recvd = -1;
	
	char buffer[MESSAGE_LEN];
	memset(buffer, MESSAGE_LEN, 0);
	
	do {
		bytes_recvd = recv(sockfd, buffer, MESSAGE_LEN, 0);
		if (bytes_recvd < 1) {
			perror("Failed to recieve message from server");
			break;
		}
		buffer[bytes_recvd] = '\0';
		
		printf("(%s): \"%s\"\n", buffer, buffer + USERNAME_LEN); fflush(stdout);
		
		memset(buffer, MESSAGE_LEN, 0);
	} while(1);
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MESSAGE_LEN];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	
	// Initialize buffer for user input
	char buffer[MESSAGE_LEN];
	memset(buffer, MESSAGE_LEN, 0);
	
	/*
	// Recieve username prompt
	int bytes_recvd = recv(sockfd, buffer, MESSAGE_LEN, 0);
	if (bytes_recvd < 0) {
		perror("Failed to recieve username prompt from server");
		close(sockfd);
		return -1;
	}
	printf("(ChatServer): %s\n", buffer);
	
	// Get and send username
	fgets(buffer, sizeof(buffer), stdin);
	if (send(sockfd, buffer, strlen(buffer)-1, 0) == -1) {
		perror("Failed to send username to server");
		close(sockfd);
		return -2;
	}
	memset(buffer, MESSAGE_LEN, 0); // Clear buffer
	*/
	
	
	
	// Create thread for recieving messages
	pthread_t recv_tid = -1;
	int tresult = pthread_create(&recv_tid, NULL, recv_loop, &sockfd);
	if (tresult != 0) {
		perror("Error: Could not create thread for client");
		close(sockfd);
	}
	pthread_detach(recv_tid);
	
	// Main input loop
	do {
		if (!fgets(buffer, sizeof(buffer), stdin)) break; // Get message
		
		if (strncmp("exit",buffer,4) == 0){
			printf("exiting...\n");
			break;
		}

		if (send(sockfd, buffer, strlen(buffer)-1, 0) == -1) { // Send message
			perror("Failed to send message to server");
		}
	} while(1);
	
	pthread_cancel(recv_tid);
	close(sockfd);

	return 0;
}


