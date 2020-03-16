#ifndef COMMON_CC
#define COMMON_CC

#include <cstdio>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <netinet/in.h>
#include <sys/time.h>

#define WINDOW_LEN (8 * 128)
#define SPNUM (0)
#define BUFFER_SIZE (1024 * 8 * 128 / 2)
#define MAX_DATA_SIZE (BUFFER_SIZE/WINDOW_LEN)

#define MAX_FRAME_SIZE (MAX_DATA_SIZE + sizeof(u_short) + sizeof(u_short) + sizeof(uint32_t) + 2)
#define ACK_SIZE (sizeof(u_short) + sizeof(u_short))

#define LAST_ACK (next_frame_expected - 1)

using namespace std;

u_short checksum(u_short *buf, int count) {
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++; 
        }
    }
    return ~(sum & 0xFFFF);
}

void readAck(char* ack, bool* error, u_short* seq_num) {
    u_short n_seqNo;
    memcpy(&n_seqNo, ack, 2);
    *seq_num = ntohs(n_seqNo);
    u_short ck = checksum((u_short *)ack, 2/2);
    u_short cko;
    memcpy(&cko, ack+2, 2);
    if (ck != ntohs(cko)) {
        *error = true;
    } else {
        *error = false;
    }
}

void readFilename(char* frame, bool* error, char* fileName, int* fileNameSize, u_short* seqNo) {
    u_short seq_num;
    memcpy(&seq_num, frame, sizeof(u_short));
    *seqNo = ntohs(seq_num);
    
    uint32_t size;
    memcpy(&size, frame+2, 4);
    int actual_size = ntohl(size);
    
    memcpy(fileName, frame+8, actual_size);
    
    u_short cko;
    memcpy(&cko, frame + 8 + actual_size, 2);

    if (checksum((u_short *)frame, (actual_size + 8)/2) == ntohs(cko) && *seqNo == SPNUM) {
        *error = false;
        *fileNameSize = actual_size;
    } else {
        *error = true;
    }
}

int createFrame(bool eof, char* data, char* frame, int data_size, u_short seq_no) {
    u_short n_seqNo = htons(seq_no);
    uint32_t n_dataSize = htonl(data_size);
    memcpy(frame, &n_seqNo, sizeof(u_short)); // + 2
    memcpy(frame + 2, &n_dataSize, 4);
    frame[6] = eof ? 0x0:0x1; // + 4
    frame[7] = eof ? 0x0:0x1;
    memcpy(frame + 8, data, data_size);
    u_short ck = htons(checksum((u_short *)frame, (data_size + 8)/2));

    memcpy(frame + data_size + 8, &ck, sizeof(u_short));
    return data_size + 10;
}

bool readFrame(char* frame, char* data, int* data_size, u_short* seq_num, bool* eot) {

    u_short seq_num_temp;
    memcpy(&seq_num_temp, frame, sizeof(u_short));
    *seq_num = ntohs(seq_num_temp);

    uint32_t net_data_size;
    memcpy(&net_data_size, frame + 2, 4);
    *data_size = ntohl(net_data_size);

    *eot = frame[6] == 0x0 ? true: false;


    memcpy(data, frame + 8, *data_size);
    
    u_short cks_temp;
    memcpy(&cks_temp, frame + 8 + *data_size, 2);
    return ntohs(cks_temp) == checksum((u_short *) frame, (*data_size + 8) / 2);

}


void createAck(u_short seq_num, char* ack) {
    u_short nSeq = htons(seq_num);
    memcpy(ack, &nSeq, 2);
    u_short ck = htons(checksum((u_short *)ack, 2/2));
    memcpy(ack + 2, &ck, 2);
}

void helperMessageSend() {
    cerr    << "Usage for sendfile:\n"
            << "sendfile -r <recv host>:<recv port> -f <subdir>/<filename>\n"
            << "<recv host> (Required) The IP address of the remote host in a.b.c.d format.\n"
            << "<recv port> (Required) The UDP port of the remote host.\n"
            << "<subdir> (Required) The local subdirectory where the ﬁle is located.\n"
            << "<filename> (Required) The name of the ﬁle to be sent.\n";
}

void helperMessageRecv() {
    cerr    << "Usage for recvfile:\n"
            << "recvfile -p <recv port>\n"
            << "<recv port> (Required) The UDP port to listen on.\n";
}

bool parseRecv(char** argv, string& arg) {
    string temp = argv[1];
    if (temp == "-p") {
        arg = argv[2];
    } else {
        return false;
    }

    return true;
}

bool parseSend(char** argv, vector<string>& arg) {
    int i = 1;
    string temp = argv[i];
    if (temp == "-r") {
        arg[0] = argv[++i];
        temp = argv[++i];
        if (temp != "-f") {
            return false;
        } else {
            arg[1] = argv[++i];
        }
    } else if (temp == "-f") {
        arg[1] = argv[++i];
        temp = argv[++i];
        if (temp != "-r") {
            return false;
        } else {
            arg[0] = argv[++i];
        }
    } else {
        return false;
    }
    return true;
}


int timeElapsed(timeval lhs, timeval rhs)
{
    return (lhs.tv_sec - rhs.tv_sec) * 1000 + (lhs.tv_usec - rhs.tv_usec) / 1000;
}

#endif

