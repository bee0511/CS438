/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <queue>
#include <cmath>
#include <errno.h>
#include <cstdio>

#define MSS 1024    // Maximum Segment Size
#define TIMEOUT 500000 // Timeout in milliseconds
using namespace std;

struct Packet {
    uint64_t seq;
    uint64_t ack;
    char data[MSS];
    uint64_t len;
    enum PacketType {DATA, ACK, FIN, FINACK} type;
};

class ReliableSender {
private:
    char* hostname;
    unsigned short int hostUDPport;
    char* filename;
    unsigned long long int bytesToTransfer;
    FILE *fp;
    int sockfd;
    int slen;
    struct sockaddr_in si_other;

    uint64_t num_packets;
    uint64_t seq;
    uint64_t num_sent;
    uint64_t num_recv;
    uint64_t dupACKcount;
    enum State {SLOW_START, CONGESTION_AVOID, FAST_RECOVERY} state;
    uint64_t cwnd;
    uint64_t ssthresh;


public:
    ReliableSender(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
    void init();
    void reliablyTransfer();

};

ReliableSender::ReliableSender(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    this->hostname = hostname;
    this->hostUDPport = hostUDPport;
    this->filename = filename;
    this->bytesToTransfer = bytesToTransfer;

    this->fp = NULL;
    this->sockfd = 0;
    this->slen = 0;

    this->num_packets = ceil((double)bytesToTransfer / MSS);
    this->seq = 0;
    this->num_sent = 0;
    this->num_recv = 0;
    this->dupACKcount = 0;
    this->state = SLOW_START;
    this->cwnd = MSS;
    this->ssthresh = 64 * MSS;  // 64KB
}

void ReliableSender::init(){
    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket");
        exit(1);
    }

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    // Set timeout for the socket
    // Ref: https://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }


}

void ReliableSender::reliablyTransfer() {
    init();

    printf("Closing the socket\n");
    close(sockfd);
    return;
}

int main(int argc, char** argv) {

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }

    ReliableSender sender(argv[1], (unsigned short int) atoi(argv[2]), argv[3], atoll(argv[4]));

    sender.reliablyTransfer();


    return (EXIT_SUCCESS);
}


