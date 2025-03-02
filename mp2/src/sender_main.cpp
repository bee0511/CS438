/*
 * File:   sender_main.cpp
 * Author: Jian-Fong Yu
 *
 * Created on 3/2/2025
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
 
 #define DEBUG_SEND 1
 // #define DEBUG_INFO 1
 // #define DEBUG_NEWACK 1
 // #define DEBUG_DUPACK 1
 // #define DEBUG_TIMEOUT 1
 
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
     uint64_t prev_sent_seq;
     uint64_t last_packet_byte;
     uint64_t dupACKcount;
     enum State { SLOW_START,
                  CONGESTION_AVOID,
                  FAST_RECOVERY } state;
     double cwnd;
     double ssthresh;
 
     unordered_map<uint64_t, bool> acked;
 
     void init();
     void startTimer();
     Packet getPacket(uint64_t seq, uint64_t len, bool fin);
     void sendPacket(Packet packet);
     void transmitPackets(bool isRetransmit);
     void newACKHandler(const uint64_t ack);
     void dupACKHandler(const uint64_t ack);
     void TimeoutHandler();
 
    public:
     ReliableSender(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
     ~ReliableSender();
 
     void reliablyTransfer();
 
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
 
     this->num_packets = bytesToTransfer / MSS;
     if (this->num_packets < (bytesToTransfer + MSS - 1) / MSS) {
         this->last_packet_byte = bytesToTransfer % MSS;
     }
     this->send_base = 1;
     this->dupACKcount = 0;
     this->state = SLOW_START;
     this->cwnd = 1.0;       // 1 window size
     this->ssthresh = 64.0;  // 64 window size
     this->prev_sent_seq = 1;
     this->acked.clear();
 }
 
 ReliableSender::~ReliableSender() {
     if (sockfd != 0) {
         close(sockfd);
     }
     if (fp != nullptr) {
         fclose(fp);
     }
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
 }
 
 void ReliableSender::printInfo() {
     cout << "\033[0m";  // Set output color to be white
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
 
 Packet ReliableSender::getPacket(uint64_t seq, uint64_t len, bool fin) {
     Packet packet;
     packet.seq = seq;
     packet.fin = fin;
     packet.len = len;
 
     size_t read_len;
     uint64_t offset = (fin) ? num_packets * MSS : (seq - 1) * MSS;
     size_t bytes_to_read = (fin) ? last_packet_byte : len;
 
     if (fseek(fp, offset, SEEK_SET) != 0) {
         perror("fseek");
         exit(1);
     }
     read_len = fread(packet.data, 1, bytes_to_read, fp);
     if (read_len < bytes_to_read && ferror(fp)) {
         perror("fread");
         exit(1);
     }
 
 #ifdef DEBUG_SEND
     if (fin) {
         cout << "\033[1;30m";  // Set output color to be gray
         cout << "[*] Read " << read_len << " bytes for the last packet" << endl;
     }
 #endif
 
     return packet;
 }
 
 void ReliableSender::sendPacket(Packet packet) {
 #ifdef DEBUG_SEND
     cout << "\033[1;30m";  // Set output color to be gray
     cout << "[*] Sending packet " << packet.seq << endl;
 #endif
     if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&si_other, slen) == -1) {
         perror("sendto");
         exit(1);
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
 
 void ReliableSender::newACKHandler(const uint64_t ack) {
     acked[ack] = true;
 #ifdef DEBUG_NEWACK
     cout << "\033[1;32m";  // Set output color to be green
     cout << "[+] Received new ACK " << ack << endl;
 #endif
     switch (state) {
         case SLOW_START:
             cwnd++;
             dupACKcount = 0;
             transmitPackets(false);
             break;
         case CONGESTION_AVOID:
             cwnd += 1.0 / cwnd;
             dupACKcount = 0;
             transmitPackets(false);
             break;
         case FAST_RECOVERY:
             cwnd = ssthresh;
             dupACKcount = 0;
             state = CONGESTION_AVOID;
             transmitPackets(false);
             break;
         default:
             break;
     }
 #ifdef DEBUG_INFO
     printInfo();
 #endif
 }
 
 void ReliableSender::dupACKHandler(const uint64_t ack) {
 #ifdef DEBUG_DUPACK
     cout << "\033[1;33m";  // Set output color to be yellow
     cout << "[*] Received duplicate ACK " << ack << endl;
 #endif
     switch (state) {
         case SLOW_START:
         case CONGESTION_AVOID:
             dupACKcount++;
             if (dupACKcount == 3) {
                 ssthresh = cwnd / 2;
                 cwnd = ssthresh + 3;
                 state = FAST_RECOVERY;
                 transmitPackets(true);
             }
             break;
         case FAST_RECOVERY:
             cwnd++;
             transmitPackets(false);
             break;
         default:
             break;
     }
 #ifdef DEBUG_INFO
     printInfo();
 #endif
 }
 
 void ReliableSender::TimeoutHandler() {
 #ifdef DEBUG_TIMEOUT
     cout << "\033[1;31m";  // Set output color to be red
     if (send_base == num_packets + 1) {
         cout << "[!] Timeout for FIN packet" << endl;
     } else {
         cout << "[!] Timeout for packet: " << send_base << endl;
     }
 #endif
     switch (state) {
         case SLOW_START:
         case CONGESTION_AVOID:
             ssthresh = cwnd / 2;
             cwnd = TIMEOUT_CWND_DEFAULT;
             dupACKcount = 0;
             state = SLOW_START;
             transmitPackets(true);
             break;
         case FAST_RECOVERY:
             ssthresh = cwnd / 2;
             cwnd = 1;
             dupACKcount = 0;
             state = SLOW_START;
             transmitPackets(true);
             break;
         default:
             break;
     }
 #ifdef DEBUG_INFO
     printInfo();
 #endif
 }
 
 void ReliableSender::transmitPackets(bool isRetransmit) {
     startTimer();
     if (send_base > num_packets) {
         // Send FIN packet
         sendPacket(getPacket(0, last_packet_byte, true));
         return;
     }
 
     // Start from the beginning of the congestion window or the new packet
     uint64_t nextseqnum = (isRetransmit) ? send_base : prev_sent_seq + 1;
 
     while (nextseqnum <= num_packets && nextseqnum < send_base + cwnd) {
         // Skip if packet is already acked
         if (acked[nextseqnum] == true) {
             nextseqnum++;
             continue;
         }
 
         sendPacket(getPacket(nextseqnum, MSS, false));
         nextseqnum++;
     }
     prev_sent_seq = nextseqnum - 1;
 }
 
 void ReliableSender::reliablyTransfer() {
     init();
 
     transmitPackets(false);
     startTimer();
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
             TimeoutHandler();
             continue;
         }
 
         if (acked[ack] == false) {
             // New ACK
             newACKHandler(ack);
         } else {
             // Duplicate ACK
             dupACKHandler(ack);
         }
 
         // Set send_base to the first encountered unacked packet
         while (acked[send_base] == true) {
             send_base++;
         }
     }
 
     cout << "\033[0m";  // Set output color to be white
     cout << "[*] File transfer completed" << endl;
     return;
 }
 
 // Add signal handler to handle ctrl+C
 void signalHandler(int signum) {
     cout << "\033[0m";  // Set output color to be white
     cout << "[!] File transfer interrupted" << endl;
 #ifdef DEBUG_INFO
     // Dump state count info
     cout << "[*] State count info:" << endl;
     cout << "[*] SLOW_START: " << state_count.slow_start_count << endl;
     cout << "[*] CONGESTION_AVOID: " << state_count.congestion_avoid_count << endl;
     cout << "[*] FAST_RECOVERY: " << state_count.fast_recovery_count << endl;
 #endif
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
 