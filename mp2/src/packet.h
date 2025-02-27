
#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define MSS 1024  // Maximum Segment Size

struct Packet {
    uint64_t seq;
    char data[MSS];
    uint32_t len;
    bool fin;
};

#endif
