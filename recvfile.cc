#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <chrono>
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

    char *buffer, *frame, *data, *fileName;
    buffer = (char *)malloc(BUFFER_SIZE);
    frame = (char *)malloc(MAX_FRAME_SIZE);
    data = (char *)malloc(MAX_DATA_SIZE);
    fileName = (char *)malloc(100);
    if (!buffer || !frame || !data || !fileName) {
        cerr << "Memory assign failed." << endl;
        return 1;
    }

    // Recv filename first
    int frameSize;
    bool error;
    int fileNameSize;
    u_short seq_num;
    socklen_t size = sizeof(client_addr);
    while (true) {
        fileNameSize = 0;
        cout << "receiving" << endl;
        frameSize = recvfrom(socket_fd, frame, MAX_FRAME_SIZE, MSG_WAITALL, (struct sockaddr *) &client_addr, &size);
        readFilename(frame, &error, fileName, &fileNameSize, &seq_num);
        // cout << frameSize << endl;
        if (!error && fileNameSize) {
            break;
        }
    }
    cout << "Filename done" << endl;
    // Send Ack for filename;
    char ack[ACK_SIZE];
    cout << seq_num << endl;
    createAck(seq_num, ack);
    // chrono::seconds interval( 1 );
    // for (int i = 0; i < 5; i++) {
    //     int s;
    //     socklen_t l = sizeof(client_addr);
    //     s = sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);
    //     this_thread::sleep_for(interval);
    // }
    sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);
    sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);
    sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);
    sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);
    sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);

    string fileStr(fileName);
    fileStr += ".recv";

    size_t pos = fileStr.find_last_of("/");
    if (pos != string::npos) {
        int status;
        status = system(("mkdir -p " + fileStr.substr(0, pos)).c_str());
        if (status == -1) {
            perror("Cannot create path.");
        }
    }

    FILE* f = fopen(fileStr.c_str(), "wb");
}