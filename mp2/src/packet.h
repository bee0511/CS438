
#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define MSS 1400  // Maximum Segment Size

// 1500 - 8 (header) - 8 (seq) - 1 (fin) - 4 (len) = 1479
struct Packet {
    uint64_t seq;    // 8 bytes
    char data[MSS];  // 1400 bytes
    bool fin;        // 1 byte
    uint32_t len;    // 4 bytes
};
#endif
