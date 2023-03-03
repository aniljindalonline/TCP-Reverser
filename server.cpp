#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>

#define PORT "8081"   // port we're listening on

void reverse(char *x, int begin, int end)
{
   char c;

   if (begin >= end)
      return;

   c          = *(x+begin);
   *(x+begin) = *(x+end);
   *(x+end)   = c;

   reverse(x, ++begin, --end);
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
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[1024];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) 
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) 
        { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) 
        {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) 
    {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) 
    {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one
    std::cout << "Reverser running on port [" << PORT << "]" << std::endl;

    // main loop
    for(;;) 
    {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) 
        {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) 
        {
            // std::cout << "file [" << basename((char*)__FILE__) << "] Line [" << __LINE__ << "] function [" << __FUNCTION__ << "] fdmax = [" << fdmax << "]" << std::endl;
            if (FD_ISSET(i, &read_fds)) 
            { // we got one!!
                // std::cout << "file [" << basename((char*)__FILE__) << "] Line [" << __LINE__ << "] function [" << __FUNCTION__ << "]" << std::endl;
                if (i == listener) 
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1) 
                    {
                        perror("accept");
                    }
                    else 
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) 
                        {    // keep track of the max
                            fdmax = newfd;
                        }
                        // printf("selectserver: new connection from [%s] on socket [%d]\n", 
                        //         inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN), 
                        //         newfd);
                    }
                }
                else 
                {
                    // std::cout << "file [" << basename((char*)__FILE__) << "] Line [" << __LINE__ << "] function [" << __FUNCTION__ << "]" << std::endl;
                    // handle data from a client
                    memset(buf, '\0', sizeof(buf));
                    if ((nbytes = recv(i, buf, sizeof(buf), MSG_WAITALL)) <= 0) 
                    {
                        // got error or connection closed by client
                        if (nbytes == 0) 
                        {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        }
                        else
                        {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } 
                    else 
                    {
                        int clnt_peer_err;
                        struct sockaddr_in addr1;
                        struct sockaddr_in serv_addr;
                        socklen_t serv_len = sizeof(serv_addr);
                        clnt_peer_err = getpeername(i, (struct sockaddr *)&addr1, &serv_len);

                        char revBuf[1024];
                        memset(revBuf, '\0', sizeof(revBuf));
                        strncpy(revBuf, buf, 1023);
                        reverse(revBuf, 0, strlen(revBuf)-1);
                        printf("[%s]:[%d]  [%s] --> [%s] \n", inet_ntoa(addr1.sin_addr), ntohs(addr1.sin_port), buf, revBuf);
                        // printf("string sent back [%s] \n", revBuf);
                        int nBytes = send(i, revBuf ,sizeof(revBuf), MSG_WAITALL);
                        // got error or connection closed by client
                        if (nbytes == 0) 
                        {
                            // connection closed
                            printf("selectserver: socket [%d] hung up\n", i);
                        }
                        else if (nbytes == -1) 
                        {
                            perror("send");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        #if 0
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) 
                        {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) 
                            {
                                // except the listener and ourselves
                                if (j != listener && j != i) 
                                {
                                    if (send(j, buf, nbytes, 0) == -1) 
                                    {
                                        perror("send");
                                    }
                                }
                            }
                        }
                        #endif
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
