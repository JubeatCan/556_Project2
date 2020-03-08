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
#include <memory>
#include "common.cc"

int socket_fd;
struct sockaddr_in dest_addr, client_addr;

mutex window_lock;
mutex fileName_lock;

void listenFilename(bool *filenameFlag) {
    char ack[ACK_SIZE];
    int ackSize;
    bool error;
    int seq_num;
    cout << "listenAck" << endl;

    socklen_t size;
    ackSize = recvfrom(socket_fd, (char *)ack, ACK_SIZE, 0, (struct sockaddr *)&dest_addr, &size);
    cout << ackSize << endl;
    if (ackSize <= 0) {
        return;
    }
    readAck(ack, &error, &seq_num);
    cout << seq_num << endl;
    fileName_lock.lock();
    if (!error && seq_num == 2 * WINDOW_LEN) {
        *filenameFlag = true;
    }
    fileName_lock.unlock();
}

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

    // Check file.
    if (access(arg[1].c_str(), F_OK) == -1) {
        cerr << "File not exists." << endl;
        return 1;
    }

    // Initialize buffer
    FILE* f = fopen(arg[1].c_str(), "rb");
    const char* filename = arg[1].c_str();
    char *buffer, *frame, *data;
    buffer = (char *)malloc(BUFFER_SIZE);
    frame = (char *)malloc(MAX_FRAME_SIZE);
    data = (char *)malloc(MAX_DATA_SIZE);
    if (!buffer || !frame || !data) {
        cerr << "Memory assign failed." << endl;
        return 1;
    }

    // Send filename first.
    bool filename_sent = false;
    // thread ackFilename(listenFilename, &filename_sent);
    bool filename_help = filename_sent;
    while (!filename_help) {
        int data_size = arg[1].length() + 1;
        // Special Seq No.
        u_short seq_no = 2 * WINDOW_LEN;
        memcpy(data, filename, data_size);
        int frame_size = createFrame(true, data, frame, data_size, seq_no);
        sendto(socket_fd, frame, frame_size, 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));

        listenFilename(&filename_help);
        cout << "Send" << endl;
    }
    // ackFilename.detach();

    // Send file data.
    
}