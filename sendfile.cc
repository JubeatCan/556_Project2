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
#include <climits>
#include "common.cc"

int socket_fd;
struct sockaddr_in dest_addr, client_addr;

mutex window_lock;

const int TIMEOUT = 1000;
timeval timeWindow[WINDOW_LEN * 2];
bool ackWindow[WINDOW_LEN * 2];
bool sentWindow[WINDOW_LEN * 2];
u_short old_shift = 0;
u_short shift = 1;

    bool sentAll = false;
    bool readAll = false;
    u_short lastFrameNo = USHRT_MAX;

void listenFilename(bool *filenameFlag) {
    char ack[ACK_SIZE];
    int ackSize;
    bool error;
    u_short seq_num;

    socklen_t size;
    ackSize = recvfrom(socket_fd, (char *)ack, ACK_SIZE, MSG_DONTWAIT, (struct sockaddr *)&dest_addr, &size);
    if (ackSize <= 0) {
        return;
    }
    readAck(ack, &error, &seq_num);
    if (!error && seq_num == SPNUM) {
        *filenameFlag = true;
    }
}

void listenAck()
{
    char ack[ACK_SIZE];
    bool error;
    int ackSize;
    u_short seq_num;

    // listen ack from reciever
    while(true)
    {
        socklen_t size;
        ackSize = recvfrom(socket_fd, (char *)ack, ACK_SIZE, MSG_WAITALL, (struct sockaddr *) &dest_addr, &size);
        readAck(ack, &error, &seq_num);
        window_lock.lock();

        if(!error && seq_num != 0 && seq_num >= shift)
        {
            shift = seq_num + 1;
            for (int i = 0; i <= seq_num - old_shift; i++)
            {
                ackWindow[i] = true;
            }
        }

        if (readAll && lastFrameNo == seq_num) {
            sentAll = true;
        }
        window_lock.unlock();
    }
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
    char *buffer, *frame, *data, *buffer2;
    buffer = (char *)malloc(BUFFER_SIZE * 2);
    frame = (char *)malloc(MAX_FRAME_SIZE);
    data = (char *)malloc(MAX_DATA_SIZE);
    if (!buffer || !frame || !data) {
        cerr << "Memory assign failed." << endl;
        return 1;
    }

    // Send filename first.
    bool filename_sent = false;
    bool filename_help = filename_sent;
    while (!filename_help) {
        int data_size = arg[1].length() + 1;
        // Special Seq No.
        u_short seq_no = SPNUM;
        memcpy(data, filename, data_size);
        int frame_size = createFrame(true, data, frame, data_size, seq_no);
        sendto(socket_fd, frame, frame_size, 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));

        listenFilename(&filename_help);
         cout << "Send filename" << endl;
    }
    cout << "Filename done" << endl;

    struct timeval initialTime = {-1, 1};
    thread ackRecv(listenAck);
    int lastPackSize = 0;
    int padding = 0;

    while (!sentAll) {
        // Shift window and reset if needed
        
        window_lock.lock();
        if (shift != old_shift) {
            if ((shift - old_shift >= WINDOW_LEN || old_shift == 0) && !readAll) {
                for (int i = 0; i < WINDOW_LEN * 2; i++) {
                    ackWindow[i] = false;
                    timeWindow[i] = initialTime;
                    sentWindow[i] = false;
                }
                padding = 0;
                old_shift = shift;

                fseek(f, (shift - 1) * MAX_DATA_SIZE, SEEK_SET);
                int readBytes = fread(buffer, 1, BUFFER_SIZE * 2, f);

                if (feof(f)) {
                    readAll = true;
                    lastPackSize = ((readBytes % MAX_DATA_SIZE) == 0 ? MAX_DATA_SIZE : (readBytes % MAX_DATA_SIZE));
                } else {
                    lastPackSize = MAX_DATA_SIZE;
                }
                if (readBytes == BUFFER_SIZE * 2) {
                    lastFrameNo = old_shift + WINDOW_LEN * 2 - 1;
                } else if (readBytes % MAX_DATA_SIZE == 0) {
                    lastFrameNo = old_shift + readBytes / MAX_DATA_SIZE - 1;
                } else {
                    lastFrameNo = old_shift + readBytes / MAX_DATA_SIZE;
                }

                // if (feof(f)) {
                //     cout<< "--------------------------------" << endl;
                //     cout << readBytes << endl;
                //     cout << old_shift << " " << lastFrameNo << " " << lastPackSize << endl;
                //     cout << "-------------------------------" << endl;
                // }
            } else {
                padding = shift - old_shift;
            }
        }
        window_lock.unlock();

        if (readAll && lastFrameNo == shift - 1) {
            sentAll = true;
            continue;
        }
        u_short current_last = old_shift + padding + WINDOW_LEN - 1;
        u_short current_first = old_shift + padding;
        timeval currentTime;
        if (lastFrameNo < current_first) {
            cout << "error" << endl;
            abort();
        }
        //send
        for (int i = current_first; i <= current_last && i <= lastFrameNo; i++) {
            int win_no = i - old_shift;
            gettimeofday(&currentTime, NULL);
            if (!sentWindow[win_no] || timeWindow[win_no].tv_sec == -1 || 
                (!ackWindow[win_no] && timeElapsed(currentTime, timeWindow[win_no]) > TIMEOUT)) {
                bool eof;
                int datasize;
                if (i == lastFrameNo && readAll) {
                    datasize = lastPackSize;
                    eof = true;
                } else {
                    datasize = MAX_DATA_SIZE;
                    eof = false;
                }
                memcpy(data, buffer + win_no * MAX_DATA_SIZE, datasize);
                cout << "[send data] " << (i-1) * MAX_DATA_SIZE << " (" << datasize << ")" << endl;
                int frame_size = createFrame(eof, data, frame, datasize, i);
                sendto(socket_fd, frame, frame_size, 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
                gettimeofday(&currentTime, NULL);
                timeWindow[win_no] = currentTime;
                sentWindow[win_no] = true;
            }
        }
    }

    ackRecv.detach();
    fclose(f);
    free(buffer);
    free(frame);
    free(data);

    cout << "[completed]" << endl;

    return 0;
}