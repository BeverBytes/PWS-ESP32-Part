#include "jaevan.h"

void JaevanProtocol::onPacket(void (*cb)(const JaevanPacket&, const Direction&)) {
    packetCallback = cb;
}

JaevanProtocol::JaevanProtocol(){
    state = Sync1;
    sync1 = 0;          // Start bytes (0xAB, 0xCD)
    sync2 = 0;          // Start bytes (0xAB, 0xCD)
    command = 0;        // Command byte
    dataLength = 0;     // Length of the data payload
    checksumA = 0;      // Checksum for error detection
    checksumB = 0;      // Checksum for error detection
    d = None;
}

// Derived from UBX protocol checksum calculation
Checksum JaevanProtocol::calculateChecksum() {
    Checksum CK;
    CK.A = 0;
    CK.B = 0;

    for (int i = 0; i < dataLength; i++) {
        CK.A += dataBuffer[i];
        CK.B += CK.A;
    }
    return CK;
}

void JaevanProtocol::dispatchPacket() {
    if (!(d == None)) {
        JaevanPacket p;
        p.sync1 = sync1;
        p.sync2 = sync2;
        p.command = command;
        p.dataLength = dataLength;
        for (int i = 0; i <= dataLength; i++) {
            p.data[i] = dataBuffer[i];
        }
        p.checksumA = checksumA;
        p.checksumB = checksumB;

        packetCallback(p, d);
    }
    return;
}

void JaevanProtocol::parseByte(uint8_t b) {
    switch (state) {
        case (Sync1):
            if (b == 0xAB) {
                state = Sync2;
            }
            break;
        case (Sync2):
            if (b == 0xCD) {
                state = Destination;
            } else {
                state = Sync1; // Reset if not correct
            }
            break;
        case (Destination):
            if (b == 0x01) {
                d = toUSB;
            } else if (b == 0x02) {
                d = toESP;
            } else if (b == 0x03) {
                d = toGR;
            } else {
                d = None;
            }
            state = Command;
            break;
        case (Command):
            if (b == 0x01 || b == 0x02 || b == 0x03) {
                command = b;
                state = Length1;
            } else {
                state = Sync1; // Reset if not correct
            }
        case (Length1):
            dataLength = b;
            state = Length2;
            break;
        case (Length2):
            dataLength |= ((uint16_t)b << 8);
            if (dataLength > 1023) {
                state = Sync1; // Reset if length is too long
            } else {
                state = Payload;
            }
            break;
        case (Payload):
            dataBuffer[bytesRead++] = b;
            if (bytesRead >= dataLength) {
                state = ChecksumA;
            }
            break;
        case (ChecksumA):
            bytesRead = 0;
            checksumA = b;
            state = ChecksumB;
            break;
        case (ChecksumB):
            checksumB = b;
            state = Sync1;

            Checksum CK;
            CK = calculateChecksum();
            if ((CK.A == ChecksumA) && (CK.B == ChecksumB)) {
                dispatchPacket();
            }
            break;
    }
}