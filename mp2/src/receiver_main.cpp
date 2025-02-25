/* 
 * File:   receiver_main.c
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

class ReliableReceiver {
private:
    unsigned short int myUDPport;
    char* destinationFile;
    int s;
    int slen;
    struct sockaddr_in si_me;
    struct sockaddr_in si_other;
public:
    ReliableReceiver(unsigned short int myUDPport, char* destinationFile);
    void reliablyReceive();
};

ReliableReceiver::ReliableReceiver(unsigned short int myUDPport, char* destinationFile) {
    this->myUDPport = myUDPport;
    this->destinationFile = destinationFile;
}

void ReliableReceiver::reliablyReceive() {
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket");
        exit(1);
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1){
        perror("bind");
        exit(1);
    }

    /* Now receive data and send acknowledgements */

    close(s);
    printf("%s received.", destinationFile);
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

    ReliableReceiver receiver((unsigned short int) atoi(argv[1]), argv[2]);
    
    receiver.reliablyReceive();
}

