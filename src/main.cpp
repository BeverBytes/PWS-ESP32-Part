#include <Arduino.h>
#include <HardwareSerial.h>
#include "jaevan.h"

bool USBUARTbusy = false;
bool GRUARTbusy = false;
volatile bool diagnosticPending = false;

HWCDC USB = Serial;
HardwareSerial GR = Serial1;

JaevanProtocol Jaevan;

void sendPacket(const JaevanPacket& p) {
  switch (p.destination) {
    case (JaevanProtocol::Direction::None):
        break;
    case (JaevanProtocol::Direction::toGR):
        GRUARTbusy = true;
        GR.write(p.sync1);
        GR.write(p.sync2);
        GR.write(0x03);
        GR.write(p.command);
        GR.write((p.dataLength >> 8) & 0xFF);
        GR.write(p.dataLength & 0xFF);
        for (int i = 0; i < p.dataLength; i++) {
          GR.write(p.data[i]);
        }
        GR.write(p.checksumA);
        GR.write(p.checksumB);
        GRUARTbusy = false;
        break;
    case (JaevanProtocol::Direction::toUSB):
        USBUARTbusy = true;
        USB.write(p.sync1);
        USB.write(p.sync2);
        USB.write(0x01);
        USB.write(p.command);
        USB.write((p.dataLength >> 8) & 0xFF);
        USB.write(p.dataLength & 0xFF);
        for (int i = 0; i < p.dataLength; i++) {
          USB.write(p.data[i]);
        }
        USB.write(p.checksumA);
        USB.write(p.checksumB);
        USBUARTbusy = false;
        break;
    default:
        break;
  };
  return;
}

void handlePacket(const JaevanPacket& p) {
  if (p.destination == JaevanProtocol::Direction::toESP) {
    // handle packets
    return;
  } else {
    sendPacket(p);
  }
  return;
}

hw_timer_t *utilisationTimer = NULL;

void ARDUINO_ISR_ATTR queueUtilisationDiagnostic() {
  diagnosticPending = true;
}

void utilisationDiagnostic() {
  static JaevanPacket p;
  p.sync1 = SYNC1;
  p.sync2 = SYNC2;
  p.destination = JaevanProtocol::Direction::toUSB;
  p.command = ESPdiagCommand;
  p.dataLength = 5;
  p.data[0] = UtilisationKey;
  float utilisation = 0.0f; // TODO: calculate utilisation
  const uint8_t *bytes = reinterpret_cast<const uint8_t*>(&utilisation);
  for (size_t i = 0; i < sizeof(utilisation); i++) {
    p.data[i + 1] = bytes[i];
  }
  Checksum checksum = Jaevan.calculateChecksumPacket(p);
  p.checksumA = checksum.A;
  p.checksumB = checksum.B;
  sendPacket(p);
}

byte USBreadBuffer[1030];
byte GRreadBuffer[1030];

void setup() {
  USB.setRxBufferSize(1130);
  GR.setRxBufferSize(1130);
  USB.setTxBufferSize(1130);
  GR.setTxBufferSize(1130);
  USB.begin(230400);
  GR.begin(115200, SERIAL_8E1, 20, 21, true, 11000UL);

  // ESP32 Utilisation calculation
  utilisationTimer = timerBegin(1000000);
  timerAttachInterrupt(utilisationTimer, &queueUtilisationDiagnostic);
  timerAlarm(utilisationTimer, 1000000, true, 0);

  Jaevan.onPacket(handlePacket);
}

void loop() {
  if (diagnosticPending && !USBUARTbusy) {
    diagnosticPending = false;
    utilisationDiagnostic();
  }

  while (USB.available() > 0) {
    Jaevan.parseByte(USB.read());
  }

  while (GR.available() > 0) {
    Jaevan.parseByte(GR.read());
  }
}