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
 
 #define DEBUG 1
 #define DEBUG_SEND 1
 // #define DEBUG_INFO 1
 #define DEBUG_NEWACK 1
 #define DEBUG_DUPACK 1
 #define DEBUG_TIMEOUT 1
 
 using namespace std;
 
 struct StateCount {
     uint64_t slow_start_count = 0;
     uint64_t congestion_avoid_count = 0;
     uint64_t fast_recovery_count = 0;
 } state_count;
 
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
     uint64_t prev_sent_seq;
     enum State { SLOW_START,
                  CONGESTION_AVOID,
                  FAST_RECOVERY } state;
     double cwnd;
     double ssthresh;
 
     vector<Packet> packets;
     unordered_map<uint64_t, bool> acked;
 
    public:
     ReliableSender(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
     void init();
     void reliablyTransfer();
     void startTimer();
     void stopTimer();
     void sendData();
     void sendNewPacket(double prev_cwnd = 0);
     void newACKHandler();
     void dupACKHandler();
     void TimeoutHandler();
     void printInfo();
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
     this->cwnd = 1.0;       // 1 window size
     this->ssthresh = 64.0;  // 64 window size
     this->prev_sent_seq = 1;
 
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
 
 void ReliableSender::printInfo() {
     // Set output color to be white
     cout << "\033[0m";
     cout << "[*] Send base: " << send_base << endl;
     cout << "[*] Dup ACK count: " << dupACKcount << endl;
     cout << "[*] State: ";
     switch (state) {
         case SLOW_START:
             state_count.slow_start_count++;
             cout << "SLOW_START" << endl;
             break;
         case CONGESTION_AVOID:
             state_count.congestion_avoid_count++;
             cout << "CONGESTION_AVOID" << endl;
             break;
         case FAST_RECOVERY:
             state_count.fast_recovery_count++;
             cout << "FAST_RECOVERY" << endl;
             break;
         default:
             cout << "UNKNOWN" << endl;
             break;
     }
     cout << "[*] Congestion window size (cwnd): " << cwnd << endl;
     cout << "[*] Slow start threshold (ssthresh): " << ssthresh << endl;
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
     double prev_cwnd = cwnd;
     switch (state) {
         case SLOW_START:
             cwnd++;
             dupACKcount = 0;
             sendNewPacket(prev_cwnd);
             break;
         case CONGESTION_AVOID:
             cwnd += 1.0 / cwnd;
             dupACKcount = 0;
             sendData();
             break;
         case FAST_RECOVERY:
             cwnd = ssthresh;
             dupACKcount = 0;
             state = CONGESTION_AVOID;
             sendNewPacket(prev_cwnd);
             break;
         default:
             break;
     }
 #ifdef DEBUG_INFO
     printInfo();
 #endif
 }
 
 void ReliableSender::dupACKHandler() {
     double prev_cwnd = cwnd;
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
             sendNewPacket(prev_cwnd);
 
             break;
         default:
             break;
     }
 #ifdef DEBUG_INFO
     printInfo();
 #endif
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
 #ifdef DEBUG_INFO
     printInfo();
 #endif
 }
 
 void ReliableSender::sendNewPacket(double prev_cwnd) {
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
     // No new packet to send
     if (cwnd - prev_cwnd < 1) {
         return;
     }
     // Send new packets within the congestion window
     uint64_t nextseqnum = prev_sent_seq + 1;
     while (nextseqnum <= num_packets && nextseqnum < send_base + cwnd) {
         // Skip if packet is already acked
         if (acked[nextseqnum] == true) {
             nextseqnum++;
             continue;
         }
 #ifdef DEBUG_SEND
         // Set output color to be gray
         cout << "\033[1;30m";
         cout << "[*] Sending packet " << nextseqnum << endl;
 #endif
         // Send packet
         if (sendto(sockfd, &packets[nextseqnum], sizeof(packets[nextseqnum]), 0, (struct sockaddr*)&si_other, slen) == -1) {
             perror("sendto");
             exit(1);
         }
         nextseqnum++;
     }
     prev_sent_seq = nextseqnum - 1;
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
 
 #ifdef DEBUG_SEND
         // Set output color to be gray
         cout << "\033[1;30m";
         cout << "[*] Sending packet " << nextseqnum << endl;
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
     prev_sent_seq = nextseqnum - 1;
 }
 
 void ReliableSender::reliablyTransfer() {
     init();
 
     cout << "[*] Sending " << num_packets << " packets" << endl;
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
             cout << "\033[1;30m";
             cout << "[*] Received FIN ACK" << endl;
             break;
         }
         // Timeout
         if (recv_len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
 #ifdef DEBUG_TIMEOUT
             // Set output color to be red
             cout << "\033[1;31m";
             cout << "[!] Timeout" << endl;
 #endif
             TimeoutHandler();
             continue;
         }
 
         if (acked[ack] == false) {
             // New ACK
             acked[ack] = true;
 #ifdef DEBUG_NEWACK
             // Set output color to be green
             cout << "\033[1;32m";
             cout << "[+] Received new ACK " << ack << endl;
 #endif
             newACKHandler();
             // } else if (acked[ack] == true) {
         } else if (acked[ack] == true) {
 #ifdef DEBUG_DUPACK
             // Set output color to be yellow
             cout << "\033[1;33m";
             cout << "[*] Received duplicate ACK " << ack << endl;
 #endif
             // Duplicate ACK
             dupACKHandler();
         }
         // Set send_base to the first encountered unacked packet
         while (acked[send_base] == true) {
             send_base++;
         }
     }
     // Set output color to be white
     cout << "\033[0m";
     cout << "[*] File transfer completed" << endl;
     close(sockfd);
     fclose(fp);
     return;
 }
 
 // Add signal handler to handle ctrl C
 void signalHandler(int signum) {
     cout << "\033[0m";
     cout << "[!] File transfer interrupted" << endl;
     // Dump state count info
     cout << "[*] State count info:" << endl;
     cout << "[*] SLOW_START: " << state_count.slow_start_count << endl;
     cout << "[*] CONGESTION_AVOID: " << state_count.congestion_avoid_count << endl;
     cout << "[*] FAST_RECOVERY: " << state_count.fast_recovery_count << endl;
 
     exit(signum);
 }
 
 int main(int argc, char** argv) {
     if (argc != 5) {
         fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
         exit(1);
     }
 
     signal(SIGINT, signalHandler);
 
     ReliableSender sender(argv[1], (unsigned short int)atoi(argv[2]), argv[3], atoll(argv[4]));
 
     sender.reliablyTransfer();
 
     return (EXIT_SUCCESS);
 }
 