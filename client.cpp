#include <stdio.h>
#include <string.h>
#include <string>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

using namespace std;

#define MAX_CLIENT_MSG_LEN 1000

struct html {
	string header;
	string content;
};

html get_url(string, string);

int main(int argc, char const *argv[])
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

	// Check Input Parameters
	if (argc < 4) {
		printf("Invalid usage. Try: \n");
		printf("$ ./client <proxy address> <proxy port> <URL to retrieve>\n");
		return 1;
	}
	
	// TCP, IPv4/6
	memset(&addr_info, 0, sizeof(addr_info));
	addr_info.ai_family = AF_UNSPEC;
	addr_info.ai_socktype = SOCK_STREAM;

	// Get a Port Number for the IP
	if ((err = getaddrinfo(argv[1], argv[2], &addr_info, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(err));
		return 1;
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
		return 2;
	}

	if ((*(struct sockaddr *)it->ai_addr).sa_family == AF_INET) {
		client_ip = &(((struct sockaddr_in *)it->ai_addr)->sin_addr);
	} else {
		client_ip = &(((struct sockaddr_in6 *)it->ai_addr)->sin6_addr);
	}

	inet_ntop(it->ai_family, client_ip, ip_addr, INET6_ADDRSTRLEN);
	printf("Client connecting to %s ...\n", ip_addr);

	freeaddrinfo(server_info);


	string request = "GET / HTTP/1.0\r\nHost: ";
	request += argv[3];
	struct html page;

	if (send(sd, request.c_str(), request.size(), 0) == -1) {
		perror("send()");
	}

	while(1) {
		memset(msg, 0, strlen(msg));
		bytes_recv = recv(sd, msg, MAX_CLIENT_MSG_LEN, 0);

		end = false;
		index = 0;
		for (int i = 3; i < bytes_recv; i++) {
			if ((msg[i-3] == '\r') && (msg[i-2] == '\n'))
				if ((msg[i-1] == '\r') && (msg[i] == '\n')) {
					index = i;
					end = true;
					break;
				}
		}

		page.header = "";
		for (int i = 0; i < index; i++) {
			page.header += msg[i];
		}

		if (end) {
			end = false;
			page.content = "";
			for (int i = index+1; i < bytes_recv; i++) {
				page.content += msg[i];
			}
			break;
		}
	}

	while (1) {
		memset(msg, 0, strlen(msg));
		bytes_recv = recv(sd, msg, MAX_CLIENT_MSG_LEN, 0);
		page.content += msg;

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

	printf("%s\n", page.header.c_str());
	fflush(stdout);
	FILE *fp;
	fp = fopen("file.html", "w");
	fputs(page.content.c_str(), fp);
	fclose(fp);

	close(sd);
	return 0;
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

	request = "GET / HTTP/1.1\r\nHost: ";
	request += ip;
	request += ":";
	request += port;
	request += "\r\n\r\n";

	if (send(sd, request.c_str(), request.size(), 0) == -1) {
		perror("send()");
	}

	while(1) {
		memset(msg, 0, strlen(msg));
		bytes_recv = recv(sd, msg, MAX_CLIENT_MSG_LEN, 0);

		end = false;
		index = 0;
		for (int i = 3; i < bytes_recv; i++) {
			if ((msg[i-3] == '\r') && (msg[i-2] == '\n'))
				if ((msg[i-1] == '\r') && (msg[i] == '\n')) {
					index = i;
					end = true;
					break;
				}
		}

		page.header = "";
		for (int i = 0; i < index; i++) {
			page.header += msg[i];
		}

		if (end) {
			end = false;
			page.content = "";
			for (int i = index+1; i < bytes_recv; i++) {
				page.content += msg[i];
			}
			break;
		}
	}

	while (1) {
		memset(msg, 0, strlen(msg));
		bytes_recv = recv(sd, msg, MAX_CLIENT_MSG_LEN, 0);
		page.content += msg;

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

	close(sd);
	return page;
}