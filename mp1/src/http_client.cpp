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

#include <iostream>
#include <string>

#define PORT "3490" // the port client will be connecting to 

#define BUF_SIZE 1024 // max number of bytes we can get at once 

#define DEBUG 1

using namespace std;

struct host_info {
	string hostname;
    string port;
    string path;
};

// Parse URL
struct host_info parse_url(string url) {
    if (url.find("http://") == string::npos) {
        fprintf(stderr, "Invalid URL format\n");
        exit(1);
    }
    struct host_info info;
    int index = 7;
    while(index < url.length()){
        // Get hostname
        if (url[index] == '/' || url[index] == ':') {
            info.hostname = url.substr(7, index - 7);
            break;
        }
        index++;
    }

    // Get port
    if (url[index] == ':') {
        index++;
        int start = index;
        while(url[index] != '/') {
            index++;
        }
        info.port = url.substr(start, index - start);
    } else {
        info.port = "80";
    }

    // Get path
    info.path = url.substr(index, url.length() - index);

#ifdef DEBUG
	// Print parsed URL
	cout << "Hostname: " << info.hostname << endl;
    cout << "Port: " << info.port << endl;
    cout << "Path: " << info.path << endl;
#endif

	return info;
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
    char buf[BUF_SIZE];
    struct addrinfo hints, *servinfo = NULL, *p = NULL;
    int rv;
    char s[INET6_ADDRSTRLEN];
    FILE *output_file;

    if (argc != 2) {
        fprintf(stderr,"usage: %s http://hostname[:port]/path/to/file\n", argv[0]);
        exit(1);
    }

    // Parse URL
	struct host_info info = parse_url(string(argv[1]));

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(info.hostname.c_str(), info.port.c_str(), &hints, &servinfo)) != 0) {
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
            close(sockfd);
            perror("client: connect");
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

    // Send HTTP GET request
    string request = "GET " + info.path + " HTTP/1.1\r\nHost: " + info.hostname + "\r\nConnection: Keep-Alive\r\n\r\n";
    
    if (send(sockfd, request.c_str(), strlen(request.c_str()), 0) == -1) {
        perror("send");
        exit(1);
    }

    // Open output file
    output_file = fopen("output", "w");
    if (output_file == NULL) {
        perror("fopen");
        exit(1);
    }

    // Receive response and write to file
    while ((numbytes = recv(sockfd, buf, BUF_SIZE-1, 0)) > 0) {
        buf[numbytes] = '\0';
        fprintf(output_file, "%s", buf);
    }

    if (numbytes == -1) {
        perror("recv");
        exit(1);
    }

    fclose(output_file);
    close(sockfd);

    return 0;
}
