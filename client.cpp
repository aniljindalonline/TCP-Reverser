#include <iostream>
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

#define PORT "8081"

#define MAXDATASIZE 1024

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
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr,"usage: client <input string>\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("localhost", /*argv[1],*/ PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: [%s]\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    int count = -1;
    for(p = servinfo, count = 1; p != NULL; p = p->ai_next, count++) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            // std::cout << "file [" << basename((char*)__FILE__) << "] Line [" << __LINE__ << "] function [" << __FUNCTION__ << "] Connection attempt [" << count << "] Error [" << errno << "] msg [" << strerror(errno) << "]" << std::endl;
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) 
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    // printf("client: connecting to [%s]\n", s);

    freeaddrinfo(servinfo); // all done with this structure
    memset(buf, '\0', MAXDATASIZE);
    strncpy(buf, argv[1], MAXDATASIZE-1);
    // std::cout << "file [" << basename((char*)__FILE__) << "] Line [" << __LINE__ << "] function [" << __FUNCTION__ << "] buf to send = [" << buf << "]" << std::endl;
    if ((numbytes = send(sockfd, buf, MAXDATASIZE, MSG_WAITALL)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    // std::cout << "file [" << basename((char*)__FILE__) << "] Line [" << __LINE__ << "] function [" << __FUNCTION__ << "] numbytes sent = [" << numbytes << "]" << std::endl;
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, MSG_WAITALL)) == -1) 
    {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';
    printf("\n[%s]\n",buf);
    close(sockfd);
    return 0;
}
