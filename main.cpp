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
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define BUFFER_MAX	1024

using namespace std;

struct stat stat_buf;

int create_server_socket(char* port, int protocol);
void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len);
void handle_sigchld(int sig);
string getDateString();
string getBasicHeadersString();
string getContentType(string fileType);

pid_t pid;

int main() {
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror(0);
        exit(1);
    }

    /* the status integer used to store information about dead children */
    int status;

    int sock = create_server_socket(strdup("8080"), SOCK_STREAM);
    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        cout << "Waiting on accept...\n";
        int client = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
        pid = fork();

        if (pid < 0) {
            printf("Fork failed!\n");
            // 500 Error
        }
        else if (pid == 0) { // CHILD


            if (client == -1) {
                perror("accept");
                continue;
            }
            handle_client(client, client_addr, client_addr_len);

            exit(1);
        }
        else { // PARENT
            close(client);

            /**
              * waitpid typically stops (blocks) the process until a child has exited
             * or been stopped. Using the WNOHANG parameter here makes waitpid return
             * immediately instead. This allows us to do other things while we wait
             * for the child to terminate (such as print things out).
             * -1 here means we're waiting for any child to die (rather than a specific
             *  PID)
             **/
//            while (waitpid(-1, &status, ) == 0) {
//                fprintf(stderr,"Waiting for my kid to die...\n");
//                cout << "Waiting for my kid to die...\n";
//                sleep(1); // now we sleep
//            }
        }
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
    printf("\nGot a connection from %s:%s\n", client_hostname, client_port);
    while (1) {
        int bytes_read = recv(sock, (void*)buffer, BUFFER_MAX-1, 0);
        // Once recv() returns 0, the transmission is complete.
        if (bytes_read == 0) {
            cout << ("Peer disconnected\n\n");
//            close(sock);
            break;
        }
        if (bytes_read < 0) {
            perror("recv");
            continue;
        }
        printf("received %d bytes:\n\n", bytes_read);
    }
    printf("\nTotal message:\n%s", buffer);
    // Extract Headers
    char delim = ' ';
    stringstream ss;
    ss.str(buffer);

    string verb, path, protocol;
    getline(ss, verb, delim);
    getline(ss, path, delim);
    getline(ss, protocol, '\r');

    if(verb.compare("GET") != 0){
        perror("501 Error");
        // Give a 501 Error
        // handle501Error();
    }

    if(verb.compare("") == 0 || path.compare("") == 0|| protocol.compare("") == 0){
        perror("Error parsing data");
    }

//    printf("Verb: [%s], Path: [%s], Protocol: [%s]\n", verb.c_str(), path.c_str(), protocol.c_str());
    if(verb.compare("GET") == 0) {
//        cout << "Get!\n";
        if(path.compare("/") == 0){
            cout << "Getting index.html.\n";
            int file;
            file = open("www/index.html", O_RDONLY);
            if(file == -1) cout << "Failed to open file\n";
            fstat(file, &stat_buf);
            off_t offset = 0;
            int bytesWritten;
//            char b[25];
//            read(file, b, 24);
//            printf("Sending file of size %d\n%s\n", stat_buf.st_size, b);
            string headers = getBasicHeadersString();
            headers += "Content-Type: text/html\r\n";
            // TODO: Content length
            // TODO: Last Modified (then append \r\n\r\n)
            // TODO: Send() headers first

            while(1) {
                bytesWritten = sendfile(sock, file, &offset, stat_buf.st_size);
//                fprintf(stderr, "error from sendfile: %s\n", strerror(errno));
                printf("%d bytes sent!\n", bytesWritten);

                if(bytesWritten == 0){
                    break;
                }
                if(bytesWritten < 0){
                    perror("send");
                    continue;
                }
            }
        } else{
            // Get the file from the www folder.
//            send(sock, buffer, strlen(reinterpret_cast<const char*>("hello world"))+1, 0);
            cout << "Getting " << path << " file\n";
            int file;
            char buf[1024];
            sprintf(buf, "%s%s", "www", path.c_str());
            file = open(buf, O_RDONLY);
            if(file == -1) cout << "Failed to open file\n";
            fstat(file, &stat_buf);
            off_t offset = 0;
            int bytesWritten;
            string headers = getBasicHeadersString();

            string fileType = path.substr(path.find("."));
//            cout << fileType << " is the filetype\n";
            headers += "Content-Type: " + getContentType(fileType);
            cout << "Headers: " << headers << "\n";
            // TODO: Content length
            // TODO: Last Modified (then append \r\n\r\n)
            // TODO: Send() headers first

            while(1){
                bytesWritten = sendfile(sock, file, &offset, stat_buf.st_size);
                printf("%d bytes sent!\n", bytesWritten);

                if(bytesWritten == 0){
                    break;
                }
                if(bytesWritten < 0){
                    perror("send");
                    return;
                }
            }
        }
    }
    close(sock);
//    perror("Close");
    return;
}

void handle_sigchld(int sig) {
//    cout << "Got sigchld\n";
    int saved_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = saved_errno;
//    cout << "Finished sigchld\n";
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

string getDateString(){
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[256];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime (buffer,180,"%a, %d %b %Y %H:%M:%S %Z",timeinfo);
    string buf = buffer;
    return buf;
//    printf("Date string: %s\n", buffer);
}

string getBasicHeadersString(){
    string headers = "Date: ";
    headers += getDateString() + "\r\n";
    headers += "Server: Alfred\r\n";
    return headers;
}

string getContentType(string fileType){
    if(fileType.compare(".html") == 0){
        return string("text/html");
    } else if(fileType.compare(".jpeg") == 0 || fileType.compare(".jpg") == 0){
        return string("image/jpeg");
    } else if(fileType.compare(".pdf") == 0){
        return string("application/pdf");
    } else if(fileType.compare(".txt") == 0){
        return string("text/plain");
    } else if(fileType.compare(".png") == 0){
        return string("image/png");
    } else if(fileType.compare(".gif") == 0){
        return string("image/gif");
    } else{
        return string("text/plain");
    }
}


