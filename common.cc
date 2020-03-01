#include <cstdio>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

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