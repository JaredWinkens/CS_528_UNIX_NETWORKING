/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fstream>
#include <string>
#include "ollama.hpp"

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MESSAGE_LEN 4096 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void chat(int sockfd){
    Ollama my_server("http://localhost:11434");
    // Initialize message buffer
	char server_response[MESSAGE_LEN];
	memset (server_response, MESSAGE_LEN, 0);

    ollama::response last_response;

    // Main loop (recv -> broadcast -> repeat)
	do {
		// Recieve a response from the server
		int bytes_recvd = recv(sockfd, server_response, MESSAGE_LEN-1, 0);
		if (bytes_recvd < 1) {
			perror("Failed to recieve message from client");
			break;
		}
		// Add termination character		
	    server_response[bytes_recvd] = '\0';
		
        // Print server's response
        printf("%s\n", "--------------------------------------------------------------");
		printf("SERVER: %s\n", server_response);
        printf("%s\n", "--------------------------------------------------------------");

        sleep(2);
        // Generate client response from server response
        last_response = my_server.generate("llama3.2", server_response, last_response);
		std::string output = last_response.as_json()["response"];

        // Print server response
        printf("%s\n", "--------------------------------------------------------------");
        printf("CLIENT: %s\n", output.c_str());
        printf("%s\n", "--------------------------------------------------------------");

		// Relay message to current client
		if (send(sockfd, output.c_str(), output.length(), 0) == -1)
			perror("Failed to relay message to client");
		
	} while (1);
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
	
	chat(sockfd);

	close(sockfd);

	return 0;
}


