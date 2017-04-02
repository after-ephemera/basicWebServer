//
// Created by Jk Jensen on 3/29/17.
//

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <netdb.h>
#include<sstream>
#include <sys/stat.h>

#define BUFFER_MAX	1024

struct stat stat_buf;

int create_server_socket(char* port, int protocol);
void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len);
void split(const std::string &s, char delim);

int main() {

    int sock = create_server_socket(strdup("8080"), SOCK_STREAM);
    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client == -1) {
            perror("accept");
            continue;
        }
        handle_client(client, client_addr, client_addr_len);
    }
}

void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len) {
    char buffer[BUFFER_MAX];
    char client_hostname[NI_MAXHOST];
    char client_port[NI_MAXSERV];
    int ret = getnameinfo((struct sockaddr*)&client_addr, addr_len, client_hostname,
                          NI_MAXHOST, client_port, NI_MAXSERV, 0);
    if (ret != 0) {
        fprintf(stderr, "Failed in getnameinfo: %s\n", gai_strerror(ret));
    }
    printf("Got a connection from %s:%s\n", client_hostname, client_port);
    while (1) {
        int bytes_read = recv(sock, (void*)buffer, BUFFER_MAX-1, 0);
        // Once recv() returns 0, the transmission is complete.
        if (bytes_read == 0) {
            std::cout << ("Peer disconnected\n\n");
//            close(sock);
            break;
        }
        if (bytes_read < 0) {
            perror("recv");
            continue;
        }
        buffer[bytes_read] = '\0';
        printf("received %d bytes:\n\n", bytes_read);
//        send(sock, buffer, strlen(reinterpret_cast<const char*>("hello world"))+1, 0);
    }
    printf("\nTotal message:\n%s", buffer);
    // Extract Headers
//    char verb[32], proto[32], s[32];
//    sprintf(&(buffer[0]), "%s %s %s\n", verb, s, proto);
//    printf("Verb: %s Proto: %s\n", verb, proto);
    char delim = ' ';
    std::stringstream ss;
    ss.str(buffer);

    std::string verb, path, protocol;
    std::getline(ss, verb, delim);
    std::getline(ss, path, delim);
    std::getline(ss, protocol, delim);

    printf("Verb: %s, Path: %s, Protocol: %s\n", verb.c_str(), path.c_str(), protocol.c_str());
    if(verb.compare("GET") == 0) {
//        std::cout << "Get!\n";
        if(path.compare("/") == 0){
            std::cout << "Getting index.html.\n";
            int file;
            file = open("www/index.html", O_RDONLY);
            if(file == -1) std::cout << "Failed to open file\n";
            std::string item;
            fstat(file, &stat_buf);
            off_t offset = 0;
            int i;
            char b[25];
            read(file, b, 24);
            printf("Yeah Sending file of size %d\n%s\n", stat_buf.st_size, b);
            i = sendfile(sock, file, &offset, stat_buf.st_size);
            fprintf(stderr, "error from sendfile: %s\n", strerror(errno));
            printf("%d bytes sent!\n", i);
        } else{
            // Get the file from the www folder.
//            send(sock, buffer, strlen(reinterpret_cast<const char*>("hello world"))+1, 0);
        }
    }

    // TODO: Extract Body
    // TODO: Formulate response
    close(sock);
    return;
}

void split(const std::string &s, char delim) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        printf("Delimited: %s\n", item.c_str());
    }
}

int create_server_socket(char* port, int protocol) {
    int sock;
    int ret;
    int optval = 1;
    struct addrinfo hints;
    struct addrinfo* addr_ptr;
    struct addrinfo* addr_list;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = protocol;
    /* AI_PASSIVE for filtering out addresses on which we
     * can't use for servers
     *
     * AI_ADDRCONFIG to filter out address types the system
     * does not support
     *
     * AI_NUMERICSERV to indicate port parameter is a number
     * and not a string
     *
     * */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
    /*
     *  On Linux binding to :: also binds to 0.0.0.0
     *  Null is fine for TCP, but UDP needs both
     *  See https://blog.powerdns.com/2012/10/08/on-binding-datagram-udp-sockets-to-the-any-addresses/
     */
    ret = getaddrinfo(protocol == SOCK_DGRAM ? "::" : NULL, port, &hints, &addr_list);
    if (ret != 0) {
        fprintf(stderr, "Failed in getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for (addr_ptr = addr_list; addr_ptr != NULL; addr_ptr = addr_ptr->ai_next) {
        sock = socket(addr_ptr->ai_family, addr_ptr->ai_socktype, addr_ptr->ai_protocol);
        if (sock == -1) {
            perror("socket");
            continue;
        }

        // Allow us to quickly reuse the address if we shut down (avoiding timeout)
        ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (ret == -1) {
            perror("setsockopt");
            close(sock);
            continue;
        }

        ret = bind(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen);
        if (ret == -1) {
            perror("bind");
            close(sock);
            continue;
        }
        break;
    }
    freeaddrinfo(addr_list);
    if (addr_ptr == NULL) {
        fprintf(stderr, "Failed to find a suitable address for binding\n");
        exit(EXIT_FAILURE);
    }

    if (protocol == SOCK_DGRAM) {
        return sock;
    }
    // Turn the socket into a listening socket if TCP
    ret = listen(sock, SOMAXCONN);
    if (ret == -1) {
        perror("listen");
        close(sock);
        exit(EXIT_FAILURE);
    }

    return sock;
}


