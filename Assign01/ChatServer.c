/*
** server.c -- a stream socket server demo
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <pthread.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

#define MESSAGE_LEN		1024
#define USERNAME_LEN	16 // Unused ATM



// Structure to store client info. Acts as a node in a circularly linked list of Clients.
struct Client {
	int sockfd;
	pthread_t thread_id;
	volatile struct Client *next;
	char name[USERNAME_LEN];
};

// Stores the "first" client, allowing for access to all clients
volatile struct Client *FIRST;


// Prints the connected clients to the server terminal
void query_clients() {
	if (FIRST == NULL) {
		printf("NO CLIENTS CONNECTED\n\n");
		return;
	}

	printf("CONNECTED CLIENTS:\n");
	volatile struct Client *tmp = FIRST;
	do {
		printf("\tClient Pointer (relative) = %ld\n", (long)(tmp-FIRST));
		if (!(tmp = tmp->next)) {
			printf("ERROR: Client is NULL; This WILL cause major problems.\n");
		}
	} while (tmp != FIRST);
	printf("END OF CLIENT LIST\n\n");
};


// Relays a message to all other clients
void relay(struct Client *sender, char *message, int bytes_recvd) {
	if (sender-> next == sender) {
		if (send(sender->sockfd, "You are alone, child.", 22, 0) == -1) {
			perror("Failed to notify client of their lonliness");
		}
		return;
	}

	volatile struct Client *curr = sender;
	printf("Target value: %ld\n", (long)sender);
	do {
		// Move to next client, check if NULL
		if (!(curr = curr->next)) {
			break; // Not sure what to do if a client is NULL. I think it's just donzo at that point
		}
		// Relay message to current client
		if (send(curr->sockfd, message, MESSAGE_LEN, 0) == -1)
			perror("Failed to relay message to client");
	} while (curr->next != sender); // Break when next client is the sender
};


// Disconnect client and handle cleanup
void disconnect_client(volatile struct Client *client) {
	// If this is the only client
	if (client == client->next) {
		FIRST = NULL;
	}
	else {
		// If we're removing the "first" client, assign a new "first" client
		if (client == FIRST) FIRST = client->next;
		
		// Notify all clients of the disconnect
		volatile struct Client *curr = client;
		do { 
			// Move to next client, check if NULL
			if (!(curr = curr->next)) {
				break; // Not sure what to do if a client is NULL. I think it's just donzo at that point
			}
			// Notify current client of disconnect
			if (send(curr->sockfd, "(CLIENT) has disconnected", 25, 0) == -1)
				perror("Failed to notify of disconnect");
		} while (curr->next != client); // Break when next client is disconnecting client
		
		// curr->next == client, client->"previous" == curr
		curr->next = client->next; // Cut client out of the linked list
	}
	
	// Close socket
	close(client->sockfd);
	// Free memory
	free(client);
	
	query_clients();
};


// Wait to receive a message, then relay to the other clients
void* client_loop(void* args) {
	// Get pointer to client info
	volatile struct Client *this_client = (volatile struct Client*)args;

	// Initialize message buffer
	char buffer[MESSAGE_LEN];
	memset(buffer, MESSAGE_LEN, 0);

	int bytes_recvd = -1;

	// Main loop
	do {
		// Recieve a message from this_client
		bytes_recvd = recv(this_client->sockfd, buffer, MESSAGE_LEN, 0);
		if (bytes_recvd < 1) {
			perror("Failed to recieve message from client");
			break;
		}
		// Add termination character
		buffer[bytes_recvd] = '\0';
		
		printf("(Client): \"%s\"\n", buffer);
		
		// Relay message to other clients
		relay(this_client, buffer, bytes_recvd);
		
	} while (1);
	
	disconnect_client(this_client);
};

// Connects a new client to the chatroom
void connect_client(volatile struct Client *client) {
	// If first client to connect, assign as FIRST
	if (FIRST == NULL) {
		client->next = client;
		FIRST = client;
		printf("FIRST CLIENT CONNECTING\n");
	}
	// Otherwise, insert self just after FIRST
	else {
		client->next = FIRST->next;
		FIRST->next = client;
	}
	
	// Create thread for client, pass in client_loop() as the func, and new_client as the parameter
	pthread_t client_tid;
	int tresult = pthread_create(&(client->thread_id), NULL, client_loop, client);
	
	// If thread creation failed
	if (tresult != 0) {
		perror("Error: Could not create thread for client");
		close(client->sockfd);
		free(client);
	}

	pthread_detach(client_tid); // Can't remember what this does lmao
	
	query_clients();
};





void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");
	
	FIRST = NULL;

	while(1) {  // main accept() loop
		// Allocate memory for new client data
		volatile struct Client *new_client = (volatile struct Client*)malloc(sizeof(struct Client));
		
		new_client->next = new_client;
	
		sin_size = sizeof their_addr;
		new_client->sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_client->sockfd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);
		
		connect_client(new_client);
	}

	return 0;
}


