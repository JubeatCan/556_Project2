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
    // cout << "listenAck" << endl;

    socklen_t size;
    ackSize = recvfrom(socket_fd, (char *)ack, ACK_SIZE, MSG_DONTWAIT, (struct sockaddr *)&dest_addr, &size);
    // cout << ackSize << endl;
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
        // cout << seq_num << endl;
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
        cout << "Ack " << seq_num << endl;
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
    // buffer2 = (char *)malloc(BUFFER_SIZE);
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
        // cout << "-------------------" << endl;
        // cout << shift << " " << old_shift << endl;
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
                    cout << MAX_DATA_SIZE << " " << readBytes << " " << lastPackSize << endl;
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

                if (feof(f)) {
                    cout<< "--------------------------------" << endl;
                    cout << readBytes << endl;
                    cout << old_shift << " " << lastFrameNo << " " << lastPackSize << endl;
                    cout << "-------------------------------" << endl;
                }
            } else {
                padding = shift - old_shift;
            }
        }
        window_lock.unlock();

        u_short current_last = old_shift + padding + WINDOW_LEN - 1;
        u_short current_first = old_shift + padding;
        timeval currentTime;
        if (lastFrameNo < current_first) {
            cout << "error" << endl;
            abort();
        }
        // cout << "Starting " << current_first << " last frame " << lastFrameNo << endl;
        //send
        window_lock.lock();
        for (int i = current_first; i <= current_last && i <= lastFrameNo; i++) {
            int win_no = i - old_shift;
            gettimeofday(&currentTime, NULL);
            if (!sentWindow[win_no] || timeWindow[win_no].tv_sec == -1 || 
                (!ackWindow[win_no] && timeElapsed(currentTime, timeWindow[win_no]) > TIMEOUT)) {
                    cout << "send " << i << endl;
                bool eof;
                int datasize;
                if (i == lastFrameNo && readAll) {
                    datasize = lastPackSize;
                    eof = true;
                    cout << "send Final packet" << endl;
                } else {
                    datasize = MAX_DATA_SIZE;
                    eof = false;
                }
                memcpy(data, buffer + win_no * MAX_DATA_SIZE, datasize);
                // for (int j = 0; j < datasize; j++) {
                //     cout << data[j];
                // }
                // cout << datasize << endl;
                int frame_size = createFrame(eof, data, frame, datasize, i);
                sendto(socket_fd, frame, frame_size, 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
                gettimeofday(&currentTime, NULL);
                timeWindow[win_no] = currentTime;
                sentWindow[win_no] = true;
            }
        }
        window_lock.unlock();
    }

    ackRecv.detach();
    fclose(f);
    free(buffer);
    free(frame);
    free(data);

    // ackFilename.detach();

    // Send file data.
    
    // window_lock.lock();
    // // initialize sliding window
    // struct timeval initialTime = {-1, -1};
    // for (int i = 0; i < WINDOW_LEN * 2; i++)
    // {
    //     ackMaskWindow[i] = false;
    //     sentMaskWindow[i] = false;
    //     timeWindow[i] = initialTime;
    // }
    // window_lock.unlock();

    // // TODO: file is small and two buffer has read all.
    // int read_bytes = fread(buffer, 1, BUFFER_SIZE, f);
    // // set to the next file location
    // fseek(f, 0, SEEK_CUR);
    // fread(buffer2, 1, BUFFER_SIZE, f);
    // fseek(f, 0, SEEK_CUR);

    // bool hasReadAll = false;
    // // bool hasSentAll = false;
    // thread ackRecv(listenAck);
    // while(true)
    // {
    //     int shift = 0;
    //     // slide window if necessary
    //     int start = isBuffer1Low ? 0 : WINDOW_LEN;
    //     if(ackMaskWindow[start])
    //     {
    //         for (int i = start; i < start + WINDOW_LEN; i++)
    //         {
    //             if(!ackMaskWindow[i])
    //                 break;
    //             else
    //             {
    //                 sentMaskWindow[shift] = true;
    //             }
    //             shift++;
    //         }
    //     }
    //     // cout << shift << endl;
    //     // end the while if all frames has been acked
    //     if(hasReadAll && shift == WINDOW_LEN)
    //     {
    //         break;
    //     }

    //     // lowerhalf has been acked. read new data in
    //     if(shift >= WINDOW_LEN)
    //     {
    //         if(!hasReadAll)
    //         {
    //             window_lock.lock();

    //             if(isBuffer1Low)
    //             {
    //                 read_bytes = fread(buffer, 1, BUFFER_SIZE, f);
    //                 for (int i = 0; i < WINDOW_LEN; i++)
    //                 {
    //                     ackMaskWindow[i] = false;
    //                     sentMaskWindow[i] = false;
    //                     timeWindow[i] = initialTime;
    //                 }
    //             }
    //             else
    //             {
    //                 read_bytes = fread(buffer2, 1, BUFFER_SIZE, f);
    //                 for (int i = WINDOW_LEN; i < WINDOW_LEN * 2; i++)
    //                 {
    //                     ackMaskWindow[i] = false;
    //                     sentMaskWindow[i] = false;
    //                     timeWindow[i] = initialTime;
    //                 }
    //             }

    //             window_lock.unlock();
    //             isBuffer1Low = !isBuffer1Low;
    //             fseek(f, 0, SEEK_CUR);

    //             if(feof(f))
    //             {
    //                 hasReadAll = true;
    //             }
    //             low = shift % WINDOW_LEN;
    //             high = low;
    //         }
    //         else
    //         {
    //             // window reaches the end of data, no need to shift high
    //             low = shift % WINDOW_LEN;
    //         }
    //     }
    //     else
    //     {
    //         low = shift % WINDOW_LEN;
    //         high = low;
    //     }
        
    //     int dataSize, offset;
    //     ushort seq_no;
    //     timeval currentTime;

    //     window_lock.lock();
    //     // send frames
    //     for (int i = low; i < WINDOW_LEN; i++)
    //     {
    //         gettimeofday(&currentTime, NULL);
    //         if(!sentMaskWindow[i] || timeWindow[i].tv_sec == -1 ||
    //             (!ackMaskWindow[i] && timeElapsed(currentTime, timeWindow[i]) > TIMEOUT))
    //         {
    //             // calculate seq_no
    //             seq_no = isBuffer1Low ? i : (i + WINDOW_LEN);
                
    //             offset = i * MAX_DATA_SIZE;
    //             bool eof = false;
    //             if(read_bytes - offset < MAX_DATA_SIZE)
    //             {
    //                 dataSize = read_bytes - offset;
    //                 eof = true;
    //             }
    //             else
    //             {
    //                 dataSize = MAX_DATA_SIZE;
    //             }
    //             // dataSize = (read_bytes - offset < MAX_DATA_SIZE) ? (read_bytes - offset) : MAX_DATA_SIZE;

    //             char* tempBufferPtr = isBuffer1Low ? buffer : buffer2;
    //             memcpy(data, tempBufferPtr + offset, dataSize);
                
    //             int frame_size = createFrame(eof, data, frame, dataSize, seq_no);
    //             sendto(socket_fd, frame, frame_size, 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    //             // cout << "send data" << endl;
    //             gettimeofday(&currentTime, NULL);
    //             timeWindow[seq_no] = currentTime;
    //             sentMaskWindow[seq_no] = true;
    //         }
    //     }
    //     for (int i = 0; i < high; i++)
    //     {
    //         gettimeofday(&currentTime, NULL);
    //         if(!sentMaskWindow[i] || timeWindow[i].tv_sec == -1 ||
    //             (!ackMaskWindow[i] && timeElapsed(currentTime, timeWindow[i]) > TIMEOUT))
    //         {
    //             // calculate seq_no
    //             seq_no = isBuffer1Low ? (i + WINDOW_LEN) : i;

    //             offset = i * MAX_DATA_SIZE;
    //             bool eof = false;
    //             if(read_bytes - offset < MAX_DATA_SIZE)
    //             {
    //                 dataSize = read_bytes - offset;
    //                 eof = true;
    //             }
    //             else
    //             {
    //                 dataSize = MAX_DATA_SIZE;
    //             }

    //             char* tempBufferPtr = isBuffer1Low ? buffer2 : buffer;
    //             memcpy(data, tempBufferPtr + offset, dataSize);
                
    //             int frame_size = createFrame(eof, data, frame, dataSize, seq_no);
    //             sendto(socket_fd, frame, frame_size, 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    //             gettimeofday(&currentTime, NULL);
    //             timeWindow[seq_no] = currentTime;
    //             sentMaskWindow[seq_no] = true;
    //         }
    //     }
    //     window_lock.unlock();
    // }
    // ackRecv.detach();
    // fclose(f);
    // free(buffer);
    // free(buffer2);
    // free(frame);
    // free(data);

    return 0;
}