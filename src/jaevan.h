// jaevan protocol header file
#ifndef JAEVAN_H   // Include guard: Prevents double inclusion
#define JAEVAN_H
#include <stdint.h>
#include <stddef.h>

const uint8_t UtilisationKey = 0xB7;
const uint8_t SYNC1 = 0xAB;
const uint8_t SYNC2 = 0xCD;
const uint8_t DataCommand = 0x01;
const uint8_t GRdiagCommand = 0x02;
const uint8_t ESPdiagCommand = 0x03;
const uint8_t MandelbrotCommand = 0x04;

#pragma pack(push, 1)
struct JaevanPacket {
    uint8_t sync1;              // Start bytes (0xAB, 0xCD)
    uint8_t sync2;              // Start bytes (0xAB, 0xCD)
    uint8_t destination;
    uint8_t command;         // Command byte
    uint16_t dataLength;     // Length of the data payload
    uint8_t data[1023];      // Data payload (up to 1023 bytes)
    uint8_t checksumA;       // Checksum for error detection
    uint8_t checksumB;       // Checksum for error detection
};

struct Checksum {
    uint8_t A;
    uint8_t B;
};

#pragma pack(pop)

class JaevanProtocol {
    public:
        enum Direction {None, toUSB, toGR, toESP};
        void onPacket(void (*cb)(const JaevanPacket&));
        JaevanProtocol();
        void parseByte(uint8_t b);
        Checksum calculateChecksumPacket(const JaevanPacket&);

    private:
        enum State {Sync1, Sync2, Destination, Command, Length1, Length2, Payload, ChecksumA, ChecksumB};

        State state;
        uint16_t bytesRead;         // bytes Read counter
        uint8_t sync1;              // Start bytes (0xAB, 0xCD)
        uint8_t sync2;              // Start bytes (0xAB, 0xCD)
        uint8_t destination;
        uint8_t command;            // Command byte
        uint16_t dataLength;        // Length of the data payload
        uint8_t dataBuffer[1023];   // Data payload (up to 1023 bytes)
        JaevanPacket packet;
        uint8_t checksumA;          // Checksum for error detection
        uint8_t checksumB;          // Checksum for error detection

        Checksum calculateChecksum();
        void dispatchPacket();
        void (*packetCallback)(const JaevanPacket&) = nullptr;
};
#endif // JAEVAN_H


// Protocol description:
//     Start Bytes (0xAB, 0xCD) 
//     Command Byte (1 byte):
//         0x01: data
//         0x02: diagnostic GR
//         0x03: diagnostic ESP32
//     Data Length (2 bytes): Length of the data payload
//     Data Payload (up to 1023 bytes): Actual data being transmitted
//     Checksum (2 bytes): Checksum for error detection

// ESP32 Diagnostic:
//     UtilisationKey = 0xB7
