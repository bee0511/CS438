/*
 * File:   sender_main.cpp
 * Author: Jian-Fong Yu
 *
 * Created on
 */

 #include <arpa/inet.h>
 #include <errno.h>
 #include <netinet/in.h>
 #include <pthread.h>
 #include <signal.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/socket.h>
 #include <sys/stat.h>
 #include <sys/time.h>
 #include <sys/types.h>
 #include <unistd.h>
 
 #include <cmath>
 #include <iostream>
 #include <unordered_map>
 #include <vector>
 
 #include "packet.h"
 #include "params.h"
 
//  #define DEBUG 1
 
 using namespace std;
 
 class ReliableSender {
    private:
     char* hostname;
     unsigned short int hostUDPport;
     char* filename;
     unsigned long long int bytesToTransfer;
     FILE* fp;
     int sockfd;
     socklen_t slen;
     struct sockaddr_in si_other;
 
     uint64_t num_packets;
     uint64_t send_base;
     uint64_t dupACKcount;
     enum State { SLOW_START,
                  CONGESTION_AVOID,
                  FAST_RECOVERY } state;
     uint64_t cwnd;
     uint64_t ssthresh;
 
     vector<Packet> packets;
     unordered_map<uint64_t, bool> acked;
 
    public:
     ReliableSender(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
     void init();
     void reliablyTransfer();
     void startTimer();
     void stopTimer();
     void sendData();
     void newACKHandler();
     void dupACKHandler();
     void TimeoutHandler();
 };
 
 ReliableSender::ReliableSender(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
     this->hostname = hostname;
     this->hostUDPport = hostUDPport;
     this->filename = filename;
     this->bytesToTransfer = bytesToTransfer;
 
     this->fp = NULL;
     this->sockfd = 0;
     this->slen = 0;
 
     this->num_packets = (bytesToTransfer + MSS - 1) / MSS;
     this->send_base = 1;
     this->dupACKcount = 0;
     this->state = SLOW_START;
     this->cwnd = 1;       // 1 window size
     this->ssthresh = 64;  // 64 window size
 
     this->acked.clear();
 }
 
 void ReliableSender::init() {
     // Open the file
     fp = fopen(filename, "rb");
     if (fp == NULL) {
         printf("Could not open file to send.");
         exit(1);
     }
 
     slen = sizeof(si_other);
 
     if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
         perror("socket");
         exit(1);
     }
 
     memset((char*)&si_other, 0, sizeof(si_other));
     si_other.sin_family = AF_INET;
     si_other.sin_port = htons(hostUDPport);
     if (inet_aton(hostname, &si_other.sin_addr) == 0) {
         fprintf(stderr, "inet_aton() failed\n");
         exit(1);
     }
 
     // Initialize packets
     packets.resize(num_packets + 1);
     for (uint64_t i = 1; i <= num_packets; i++) {
         packets[i].seq = i;
         size_t bytesRead = fread(packets[i].data, 1, MSS, fp);
         packets[i].len = bytesRead;
         packets[i].fin = false;
         if (bytesRead < MSS) {
             packets[i].data[bytesRead] = '\0';
         }
     }
 }
 
 void ReliableSender::startTimer() {
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
 
 void ReliableSender::stopTimer() {
     // Remove timeout for the socket
     struct timeval timeout;
     timeout.tv_sec = 0;
     timeout.tv_usec = 0;
     if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
         perror("setsockopt failed");
         exit(1);
     }
 }
 
 void ReliableSender::newACKHandler() {
     switch (state) {
         case SLOW_START:
             cwnd++;
             dupACKcount = 0;
             sendData();
             break;
         case CONGESTION_AVOID:
             cwnd += 1 / cwnd;
             dupACKcount = 0;
             sendData();
             break;
         case FAST_RECOVERY:
             cwnd = ssthresh;
             dupACKcount = 0;
             state = CONGESTION_AVOID;
             sendData();
             break;
         default:
             break;
     }
 }
 
 void ReliableSender::dupACKHandler() {
     switch (state) {
         case SLOW_START:
         case CONGESTION_AVOID:
             dupACKcount++;
             if (dupACKcount == 3) {
                 ssthresh = cwnd / 2;
                 cwnd = ssthresh + 3;
                 state = FAST_RECOVERY;
                 sendData();
             }
             break;
         case FAST_RECOVERY:
             cwnd++;
             sendData();
             break;
         default:
             break;
     }
 }
 
 void ReliableSender::TimeoutHandler() {
     switch (state) {
         case SLOW_START:
         case CONGESTION_AVOID:
         case FAST_RECOVERY:
             ssthresh = cwnd / 2;
             cwnd = 1;
             dupACKcount = 0;
             state = SLOW_START;
             sendData();
             break;
         default:
             break;
     }
 }
 
 void ReliableSender::sendData() {
     if (send_base > num_packets) {
         Packet finPacket;
         finPacket.seq = 0;
         finPacket.len = 0;
         finPacket.fin = true;
         if (sendto(sockfd, &finPacket, sizeof(finPacket), 0, (struct sockaddr*)&si_other, slen) == -1) {
             perror("sendto");
             exit(1);
         }
         return;
     }
 
     // Send packets within the congestion window
     uint64_t nextseqnum = send_base;
     while (nextseqnum <= num_packets && nextseqnum < send_base + cwnd) {
         // Skip if packet is already acked
         if (acked[nextseqnum] == true) {
             nextseqnum++;
             continue;
         }
 
 #ifdef DEBUG
         printf("[*] Sending packet %lu\n", nextseqnum);
 #endif
         // Send packet
         if (sendto(sockfd, &packets[nextseqnum], sizeof(packets[nextseqnum]), 0, (struct sockaddr*)&si_other, slen) == -1) {
             perror("sendto");
             exit(1);
         }
 
         // First packet in the window
         if (send_base == nextseqnum) {
             // Keep track of the first packet in the window
             startTimer();
         }
         nextseqnum++;
     }
 }
 
 void ReliableSender::reliablyTransfer() {
     init();
 
 #ifdef DEBUG
     cout << "[*] Sending " << num_packets << " packets" << endl;
 #endif
     sendData();
     uint64_t ack;
     while (true) {
         if (cwnd >= ssthresh && state == SLOW_START) {
             state = CONGESTION_AVOID;
         }
 
         // Receive ACK
         int recv_len = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&si_other, &slen);
         if (recv_len == -1 && (errno != EAGAIN || errno != EWOULDBLOCK)) {
             perror("recvfrom");
             exit(1);
         }
         if (ack == 0) {
 #ifdef DEBUG
             cout << "[*] Received FIN ACK" << endl;
 #endif
             break;
         }
         // Timeout
         if (recv_len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
 #ifdef DEBUG
             cout << "[!] Timeout" << endl;
 #endif
             TimeoutHandler();
             continue;
         }
 
         if (acked[ack] == false) {
             // New ACK
             acked[ack] = true;
             newACKHandler();
         } else {
             // Duplicate ACK
             dupACKHandler();
         }
 
         if (ack == send_base) {
             send_base++;
         }
     }
 
     printf("[*] Closing the socket\n");
     close(sockfd);
     fclose(fp);
     return;
 }
 
 int main(int argc, char** argv) {
     if (argc != 5) {
         fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
         exit(1);
     }
 
     ReliableSender sender(argv[1], (unsigned short int)atoi(argv[2]), argv[3], atoll(argv[4]));
 
     sender.reliablyTransfer();
 
     return (EXIT_SUCCESS);
 }
 