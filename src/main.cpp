#include <Arduino.h>
#include <HardwareSerial.h>
#include "jaevan.h"

bool USBUARTbusy = false;
bool GRUARTbusy = false;
bool QueueDiagnostic = false;

HWCDC USB = Serial;
HardwareSerial GR = Serial1;

JaevanProtocol Jaevan;

void sendPacket(const JaevanPacket& p, const JaevanProtocol::Direction& d) {
  switch (d) {
    case (JaevanProtocol::Direction::None):
        break;
    case (JaevanProtocol::Direction::toGR):
        GRUARTbusy = true;
        GR.write(p.sync1);
        GR.write(p.sync2);
        GR.write(p.command);
        GR.write(p.dataLength);
        for (int i = 0; i <= p.dataLength; i++) {
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
        USB.write(p.command);
        USB.write(p.dataLength);
        for (int i = 0; i <= p.dataLength; i++) {
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

void handlePacket(const JaevanPacket& p, const JaevanProtocol::Direction& d) {
  if (d == JaevanProtocol::Direction::toESP) {
    // handle packets
    return;
  } else {
    sendPacket(p, d);
  }
  return;
}

hw_timer_t *utilisationTimer = NULL;

void utilisationDiagnostic() {
  if (!USBUARTbusy) {
    JaevanPacket p;
    p.sync1 = SYNC1;
    p.sync2 = SYNC2;
    p.command = ESPdiagCommand;
    p.dataLength = 5;
    p.data[0] = UtilisationKey;
    float Utilisation = 0.0f; //TODO calculate utilistation
    uint8_t *rewrite;
    rewrite = reinterpret_cast<uint8_t*>(&Utilisation);
    for (int i = 0; i <= sizeof(rewrite); i++) {
      p.data[i+1] = rewrite[i];
    }
    sendPacket(p, JaevanProtocol::Direction::toUSB);


  } else {
    QueueDiagnostic = true;
  }
  return;
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
  timerAttachInterrupt(utilisationTimer, &utilisationDiagnostic);
  timerAlarm(utilisationTimer, 1000000, true, 0);

  Jaevan.onPacket(handlePacket);
}

void loop() {
  uint16_t USBRXbufferIndex = 0;
  uint16_t GRRXbufferIndex = 0;
  while (USB.available() > 0) {
    Jaevan.parseByte(USB.read());
  }

  while (GR.available() > 0) {
    Jaevan.parseByte(GR.read());
  }
}