#ifndef COMMON_PACKETS_H
#define COMMON_PACKETS_H

#include <stdint.h>

#define MAGIC_1 0xAA
#define MAGIC_2 0x55

#define PKT_ODOM  0x01
#define PKT_LIDAR 0x02

#pragma pack(push, 1)

// Packet structure for Odometry telemetry
typedef struct {
    uint8_t magic1;    // 0xAA
    uint8_t magic2;    // 0x55
    uint8_t pkt_type;  // 0x01
    float   x;         // position X (meters)
    float   y;         // position Y (meters)
    float   theta;     // orientation angle (radians)
} OdomPacket;

// XV11 4-point raw packet frame structure
typedef struct {
    uint8_t  magic1;     // 0xAA
    uint8_t  magic2;     // 0x55
    uint8_t  pkt_type;   // 0x02
    uint16_t distance[360]; // mm
} LidarPacket;

LidarPacket LidarPackets;

#pragma pack(pop)

#endif // COMMON_PACKETS_H