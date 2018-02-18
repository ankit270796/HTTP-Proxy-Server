#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <pthread.h>
#include <map>

using namespace std;

#define MAX_CLIENT_MSG_LEN 100
#define MAX_CLIENTS 10

struct html {
	string header;
	string content;
};

map<string, html> cache;

// Signal Handler for Child Process
void sigchld_handler(int signo);
// Thread that handles every client
void *client_handler(void *);
// Gets a webpage from the internet
html get_url(string, string);

int main(int argc, char const *argv[])
{
	int sd, new_sd;
	int err;
	int yes=1;
	int client_port;
	int bytes_recv;
	struct addrinfo addr_info, *server_info, *it;
	struct sigaction sig;
	struct sockaddr_storage client_addr;
	socklen_t sockaddr_size;
	char ip_addr[INET6_ADDRSTRLEN];
	char *msg, client_msg[MAX_CLIENT_MSG_LEN];
	void *client_ip;
	pthread_t handler;

	// Populate the address structure
	memset(&addr_info, 0, sizeof(addr_info));
	// IPv4 IPv6 independent
	addr_info.ai_family = AF_UNSPEC;
	// Use TCP
	addr_info.ai_socktype = SOCK_STREAM;
	addr_info.ai_flags = AI_PASSIVE;

	// Check command line arguments
	if (argc < 3) {
		printf("Invalid usage. Try: \n");
		printf("$ ./proxy <ip to bind> <port to bind>\n");
		return 1;
	}

	if ((err = getaddrinfo(argv[1], argv[2], &addr_info, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(err));
		return 1;
	}

	// Iterate through output of getaddrinfo() and find a port to bind to
	for (it = server_info; it != NULL; it = it->ai_next) {
		sd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (sd == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("socket()");
			continue;
		}
		if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("setsockopt()");
			return 1;
		}
		if (bind(sd, it->ai_addr, it->ai_addrlen) == -1) {
			close(sd);
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("bind()");
			// If bind was unsuccesssful, try the next port
			// Note: In this case, there happens to be only one port and
			// so the list server_info only has one element
			continue;
		}
		break;
	}

	freeaddrinfo(server_info);

	// Check if server successfully binded to the given port
	if (it == NULL) {
		fprintf(stderr, "Server failed to bind to given port.\n");
		return 1;
	}

	// Listen on the port
	if (listen(sd, MAX_CLIENTS) == -1) {
		printf("%s:%d ", __FILE__, __LINE__);
		fflush(stdout);
		perror("listen()");
		return 1;
	}

	// Register the signal handler for SIGCHLD event
	// so that the resulting process is not a zombie while exiting
	sig.sa_handler = sigchld_handler;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sig, NULL) == -1) {
		printf("%s:%d ", __FILE__, __LINE__);
		fflush(stdout);
		perror("sigaction()");
		return 1;
	}

	printf("Waiting for connections ....\n");

	while (1) {
		sockaddr_size = sizeof(client_addr);
		// Accept incoming connection
		new_sd = accept(sd, (struct sockaddr *)&client_addr, &sockaddr_size);
		if (new_sd == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("accept()");
			continue;
		}

		// Print the client IP Address and Port Number
		if ((*(struct sockaddr *)&client_addr).sa_family == AF_INET) {
			client_ip = &(((struct sockaddr_in *)&client_addr)->sin_addr);
		} else {
			client_ip = &(((struct sockaddr_in6 *)&client_addr)->sin6_addr);
		}
		client_port = ntohs((*(struct sockaddr_in *)&client_addr).sin_port);
		inet_ntop(client_addr.ss_family, client_ip, ip_addr, INET6_ADDRSTRLEN);
		printf("Connection accepted from: %s, PORT:%d\n", ip_addr, client_port);

		// Create a child thread to handle the client
		if (pthread_create(&handler, NULL, client_handler, (void *)&new_sd) < 0) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("pthread_create()");
			return 1;
		}
	}
}

// SIGCHLD handler
void sigchld_handler(int signo)
{
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

// This thread handles each client connected
void *client_handler(void *socket)
{
	int sd = *(int *)socket;
	int bytes_recv;
	int index;
	char *msg;
	char client_msg[MAX_CLIENT_MSG_LEN];
	struct html page;

	// Wait for client to send data
	while ((bytes_recv = recv(sd, client_msg, MAX_CLIENT_MSG_LEN, 0)) > 0) {
		client_msg[bytes_recv] = '\0';

		printf("Request received: %s\n", client_msg);
		index = 0;
		for (int i = 1; i < bytes_recv; i++) {
			if ((client_msg[i-1] == 'H') && (client_msg[i] == 'o')) {
				index = i;
				break;
			}
		}
		string host = "";
		string ip = "";
		string port = "";
		for (int i = index+5; i < bytes_recv; i++) {
			host += client_msg[i];
		}

		for (int i = 0; i < host.size(); i++) {
			if (host[i] == ':') {
				index = i;
				break;
			}
			ip += host[i];
		}
		for (int i = index+1; i < host.size(); i++) {
			port += host[i];
		}

		if (cache.find(host) != cache.end()) {
			page = cache[host];
			printf("In Cache!\n");
		} else {
			printf("Not in Cache!\n");
			page = get_url(ip, port);
			cache[host] = page;
		}

		string data = page.header + page.content;
		if (send(sd, data.c_str(), 1000000, 0) == -1) {
			perror("send()");
		}

		printf("Request Processed!\n");
		while(1);
		
	}
	// If client closes connection, print it on standard output
	if (bytes_recv == 0) {
		printf("Client Closed Connection ...\n");
//		printf("Client closed connection: %s, PORT:%d\n", ip_addr, client_port);
		fflush(stdout);
	}
	// If recv() fails, output with error code 
	else {
		perror("recv()");
	}

	// Free object
	close(sd);
	pthread_exit(NULL);
}


html get_url(string ip, string port)
{
	int sd;
	int err;
	int bytes_recv;
	struct addrinfo addr_info, *server_info, *it;
	char ip_addr[INET6_ADDRSTRLEN];
	char msg[MAX_CLIENT_MSG_LEN];
	void *client_ip;
	bool end;
	int index;
	struct html page;
	string request;
	string data;

	// TCP, IPv4/6
	memset(&addr_info, 0, sizeof(addr_info));
	addr_info.ai_family = AF_UNSPEC;
	addr_info.ai_socktype = SOCK_STREAM;

	page.header = "";
	page.content = "";

	// Get a Port Number for the IP
	if ((err = getaddrinfo(ip.c_str(), port.c_str(), &addr_info, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(err));
		return page;
	}


	// Check if we got a port number (In this case, we only have one choice)
	for (it = server_info; it != NULL; it = it->ai_next) {
		sd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (sd == -1) {
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("socket()");
			continue;
		}
		if (connect(sd, it->ai_addr, it->ai_addrlen) == -1) {
			close(sd);
			printf("%s:%d ", __FILE__, __LINE__);
			fflush(stdout);
			perror("connect()");
			continue;
		}
		break;
	}

	if (it == NULL) {
		fprintf(stderr, "Client failed to connect!\n");
		return page;
	}

	if ((*(struct sockaddr *)it->ai_addr).sa_family == AF_INET) {
		client_ip = &(((struct sockaddr_in *)it->ai_addr)->sin_addr);
	} else {
		client_ip = &(((struct sockaddr_in6 *)it->ai_addr)->sin6_addr);
	}

	inet_ntop(it->ai_family, client_ip, ip_addr, INET6_ADDRSTRLEN);
	printf("Client connecting to %s ...\n", ip_addr);

	freeaddrinfo(server_info);

	request = "GET / HTTP/1.0\r\nHost: ";
	request += ip;
	request += ":";
	request += port;
	request += "\r\n\r\n";

	if (send(sd, request.c_str(), request.size(), 0) == -1) {
		perror("send()");
	}


	data = "";
	while (1) {
		memset(msg, 0, strlen(msg));
		bytes_recv = recv(sd, msg, MAX_CLIENT_MSG_LEN, 0);
		data += msg;
		fflush(stdout);

		end = false;
		for (int i = 6; i < bytes_recv; i++) {
			if ((msg[i-6] == '<') && (msg[i-5] == '/'))
				if ((msg[i-4] == 'h') && (msg[i-3] == 't'))
					if ((msg[i-2] == 'm') && (msg[i-1] == 'l'))
						if (msg[i] == '>') {
							end = true;
							break;
						}
		}	

		if (end) {
			break;
		}
	}

	for (int i = 3; i < data.size(); i++) {
		if ((data[i-3] == '\r') && (data[i-2] == '\n'))
			if ((data[i-1] == '\r') && (data[i] == '\n')) {
				index = i;
				end = true;
				break;
			}
	}

	page.header = "";
	for (int i = 0; i < index; i++) {
		page.header += data[i];
	}
	page.header += "\n";

	page.content = "";
	for (int i = index+1; i < data.size(); i++) {
		page.content += data[i];
	}
	
	close(sd);
	return page;
}