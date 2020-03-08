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

    /* build server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(atoi(arg.c_str()));

    /* setup passive open in server */
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
    int frame_size;
    int data_size;
    int filename_size;
    bool filename_error;
    u_short seq_num;
    socklen_t size = sizeof(client_addr);
    while (true) {
        filename_size = 0;
        cout << "receiving" << endl;
        frame_size = recvfrom(socket_fd, frame, MAX_FRAME_SIZE, MSG_WAITALL, (struct sockaddr *) &client_addr, &size);
        readFilename(frame, &filename_error, fileName, &filename_size, &seq_num);
        // cout << frameSize << endl;
        if (!filename_error && filename_size) {
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

    FILE *file = fopen(fileStr.c_str(), "wb");
    
    /* receive frames until the last one */
    char * buffer1 = (char *) malloc(BUFFER_SIZE / 2);  // seq_no [0 : WINDOW_LEN - 1] store here
    char * buffer2 = (char *) malloc(BUFFER_SIZE / 2);  // seq_no [WINDOW_LEN : WINDOW_LEN * 2 - 1] store here

    int recv_count1 = 0;    // count how many frame stored in buffer1
    int recv_count2 = 0;    // count how many frame stored in buffer2

    u_short next_frame_expected = 0; // seq_no of next frame expected
    bool frame_error;
    bool is_last;
    
    bool window_recv_mask[WINDOW_LEN];
    for (int i = 0; i < WINDOW_LEN; i++) {
        window_recv_mask[i] = false;
    }
    
    while (true) {
        frame_size = recvfrom(socket_fd, frame, MAX_FRAME_SIZE, MSG_WAITALL, (struct sockaddr *) &client_addr, &size);
        frame_error = readFrame(frame, data, &data_size, &seq_num, &is_last);

        int idx = (seq_num - next_frame_expected + 2 * WINDOW_LEN) % (2 * WINDOW_LEN);

        // if frame has error or not in current recv_window, drop the frame
        // send ack for last one (or do nothing)
        if (frame_error || idx >= WINDOW_LEN) {
            createAck(next_frame_expected - 1, ack);
            sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);
            continue;
        }

        // if seq_no == next frame expected, slide the window
        if (seq_num == next_frame_expected) {
            // calcuate how many window slot shifted
            int window_shift = 1;
            for (int i = 1; i < WINDOW_LEN; i++) {
                if (!window_recv_mask[i]) break;
                window_shift++;
            }

            for (int i = 0; i < WINDOW_LEN - window_shift; i++) {
                window_recv_mask[i] = window_recv_mask[i + window_shift];
            }

            for (int i = WINDOW_LEN - window_shift; i < WINDOW_LEN; i++) {
                window_recv_mask[i] = false;
            }
            // reset next_frame_expected
            next_frame_expected = (next_frame_expected + window_shift) % (2 * WINDOW_LEN);

        }
        // else just mark it received
        else {
            if (!window_recv_mask[idx]) {
                window_recv_mask[idx] = true;
            }
        }

        // send ack
        createAck(next_frame_expected - 1, ack);
        sendto(socket_fd, ack, ACK_SIZE, 0, (const struct sockaddr *) &client_addr, size);

        // copy data to corresponding buffer, check whether buffer is full
        size_t buffer_shift;
        if (seq_num < WINDOW_LEN) {
            buffer_shift = seq_num * MAX_DATA_SIZE;
            memcpy(buffer1 + buffer_shift, data, data_size);
            
            // if buffer1 is full, write to file, reset buffer1
            if (++recv_count1 == WINDOW_LEN) {
                fwrite(buffer1, 1, BUFFER_SIZE / 2, file);
                memset(buffer1, 0, BUFFER_SIZE / 2);
                recv_count1 = 0;
            }
        } 
        else {
            buffer_shift = (seq_num - WINDOW_LEN) * MAX_DATA_SIZE;
            memcpy(buffer2 + buffer_shift, data, data_size);
            
            // if buffer2 is full, write to file, reset buffer2
            if (++recv_count2 == WINDOW_LEN) {
                fwrite(buffer2, 1, BUFFER_SIZE / 2, file);
                memset(buffer2, 0, BUFFER_SIZE / 2);
                recv_count2 = 0;
            }
        }
        

        // TODO - deal with last frame
    }


}