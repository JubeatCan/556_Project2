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

int socket_fd;
struct sockaddr_in dest_addr, client_addr;

int main(int argc, char** argv) {
    if (argc != 5) {
        helperMessageSend();
        return 1;
    }

    vector<string> arg(2);
    if(!parseSend(argv, arg)) {
        helperMessageSend();
        return 1;
    }


    struct hostent *dest_ent;
    memset(&dest_addr, 0, sizeof(dest_addr)); 
    memset(&client_addr, 0, sizeof(client_addr)); 
    string delim = ":";
    size_t pStart = 0, pEnd, del_len = delim.length();
    string token;

    // Set Socket
    if ((pEnd = arg[0].find(delim, pStart)) == string::npos) {
        helperMessageSend();
        return 1;
    } else {
        token = arg[0].substr(pStart, pEnd - pStart);
        pStart = pEnd + del_len;
        dest_ent = gethostbyname(token.c_str());
        if (!dest_ent) {
            cerr << "Host cannot get: " << token << endl;
            return 1;
        }
        token = arg[0].substr(pStart);

        // Destination
        dest_addr.sin_family = AF_INET;
        bcopy(dest_ent->h_addr, (char *)&dest_addr.sin_addr, dest_ent->h_length);
        dest_addr.sin_port = htons(atoi(token.c_str()));
        
        // Client
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = INADDR_ANY; 
        client_addr.sin_port = htons(0);

        if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            cerr << "Cannot create socket." << endl;
            return 1;
        }

        if (::bind(socket_fd, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            cerr << "Cannot bind." << endl;
            return 1;
        }
    }

    // Set File
    if (access(arg[1].c_str(), F_OK) == -1) {
        cerr << "File not exists." << endl;
        return 1;
    }

    FILE* f = fopen(arg[1].c_str(), "rb");
    
}