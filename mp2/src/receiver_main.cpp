/*
 * File:   receiver_main.cpp
 * Author: Jian-Fong Yu
 *
 * Created on
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <unordered_map>

#include "packet.h"
#include "params.h"

#define DEBUG 1

using namespace std;

class ReliableReceiver {
   private:
    unsigned short int myUDPport;
    char* destinationFile;
    int s;
    socklen_t slen;
    struct sockaddr_in si_me;
    struct sockaddr_in si_other;
    FILE* fp;
    unordered_map<uint64_t, Packet> buffer;

   public:
    ReliableReceiver(unsigned short int myUDPport, char* destinationFile);
    void init();
    void reliablyReceive();
};

ReliableReceiver::ReliableReceiver(unsigned short int myUDPport, char* destinationFile) {
    this->myUDPport = myUDPport;
    this->destinationFile = destinationFile;
    this->s = 0;
    this->slen = 0;
    this->fp = NULL;
    this->buffer.clear();
}

void ReliableReceiver::init() {
    slen = sizeof(si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        exit(1);
    }

    memset((char*)&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
        perror("bind");
        exit(1);
    }

    fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    return;
}

void ReliableReceiver::reliablyReceive() {
    init();

    Packet packet;

    while (1) {
        int recv_len = recvfrom(s, &packet, sizeof(packet), 0, (struct sockaddr*)&si_other, &slen);
        if (recv_len == -1) {
            perror("recvfrom");
            exit(1);
        }

        buffer[packet.seq] = packet;

        if (packet.fin) {
            break;
        }

        // Send ACK
        if (sendto(s, &(packet.seq), sizeof(packet.seq), 0, (struct sockaddr*)&si_other, slen) == -1) {
            perror("sendto");
            exit(1);
        }
#ifdef DEBUG
        printf("Received packet %lu\n", packet.seq);
#endif
    }

    // Write to file
    for (uint64_t i = 0; i < buffer.size(); i++) {
        fwrite(buffer[i].data, 1, MSS, fp);
    }

    fclose(fp);
    close(s);
    return;
}

/*
 *
 */
int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    ReliableReceiver receiver((unsigned short int)atoi(argv[1]), argv[2]);

    receiver.reliablyReceive();
}
