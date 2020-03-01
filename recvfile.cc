#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib> 
#include <unistd.h> 
#include <cstring> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <thread>
#include <string>
#include <vector>
#include <netdb.h>
#include "common.cc"

struct sockaddr_in server_addr, client_addr;
int socket_fd;

int main(int argc, char** argv) {
    if (argc != 3) {
        helperMessageRecv();
        return 1;
    }

    string arg;
    if (!parseRecv(argv, arg)) {
        helperMessageRecv();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr)); 
    memset(&client_addr, 0, sizeof(client_addr)); 

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(atoi(arg.c_str()));

    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "Cannot create socket." << endl;
        return 1;
    }

    if (::bind(socket_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { 
        cerr << "Cannot bind socket." << endl;
        return 1;
    }
}