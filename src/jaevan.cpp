#include "jaevan.h"

void JaevanProtocol::onPacket(void (*cb)(const JaevanPacket&)) {
    packetCallback = cb;
}

JaevanProtocol::JaevanProtocol(){
    state = Sync1;
    bytesRead = 0;
    sync1 = 0;          // Start bytes (0xAB, 0xCD)
    sync2 = 0;          // Start bytes (0xAB, 0xCD)
    destination = None; // destination
    command = 0;        // Command byte
    dataLength = 0;     // Length of the data payload
    checksumA = 0;      // Checksum for error detection
    checksumB = 0;      // Checksum for error detection
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

Checksum JaevanProtocol::calculateChecksumPacket(const JaevanPacket& p) {
    Checksum CK;
    CK.A = 0;
    CK.B = 0;

    for (int i = 0; i < p.dataLength; i++) {
        CK.A += p.data[i];
        CK.B += CK.A;
    }
    return CK;
}

void JaevanProtocol::dispatchPacket() {
    if (destination != None && packetCallback != nullptr) {
        packet.sync1 = sync1;
        packet.sync2 = sync2;
        packet.destination = destination;
        packet.command = command;
        packet.dataLength = dataLength;
        for (uint16_t i = 0; i < dataLength; i++) {
            packet.data[i] = dataBuffer[i];
        }
        packet.checksumA = checksumA;
        packet.checksumB = checksumB;

        packetCallback(packet);
    }
    return;
}

void JaevanProtocol::parseByte(uint8_t b) {
    switch (state) {
        case (Sync1):
            if (b == 0xAB) {
                sync1 = b;
                state = Sync2;
            }
            break;
        case (Sync2):
            if (b == 0xCD) {
                sync2 = b;
                state = Destination;
            } else {
                state = Sync1; // Reset if not correct
            }
            break;
        case (Destination):
            destination = b;
            if (b == 0x01) {
                destination = toUSB;
            } else if (b == 0x02) {
                destination = toESP;
            } else if (b == 0x03) {
                destination = toGR;
            } else {
                destination = None;
            }
            state = Command;
            break;
        case (Command):
            if (b == 0x01 || b == 0x02 || b == 0x03 || b == 0x04) {
                command = b;
                state = Length1;
            } else {
                state = Sync1; // Reset if not correct
                break;
            }
            break;
        case (Length1):
            dataLength = b;
            state = Length2;
            break;
        case (Length2):
            dataLength = (dataLength << 8) + b;
            if (dataLength > 1023) {
                state = Sync1; // Reset if length is too long
            } else {
                bytesRead = 0;
                state = dataLength == 0 ? ChecksumA : Payload;
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
            if ((CK.A == checksumA) && (CK.B == checksumB)) {
                dispatchPacket();
            }
            break;
    }
}