#include <Arduino.h>
#include <Wire.h>

#include "ms5611.h"

namespace {

constexpr uint8_t kSdaPin = 0;
constexpr uint8_t kSclPin = 1;
constexpr uint32_t kI2cHz = 400000;
constexpr uint8_t kAddress = 0x77;
constexpr uint32_t kReadIntervalMs = 1000;
constexpr uint32_t kRetryIntervalMs = 1000;

MS5611 sensor(Wire, kAddress);
bool initialized = false;
uint32_t nextInitAttemptMs = 0;
uint32_t nextReadMs = 0;

void logBoot() {
  Serial.printf("MS5611: boot addr=0x%02X SDA=%u SCL=%u %lukHz\n", kAddress, kSdaPin,
                kSclPin, static_cast<unsigned long>(kI2cHz / 1000));
}

void logInitOk() {
  const Prom& prom = sensor.prom();
  Serial.printf("MS5611: init OK CRC=0x%02X PROM=%04X %04X %04X %04X %04X %04X %04X %04X\n",
                crc4(prom), prom.words[0], prom.words[1], prom.words[2], prom.words[3],
                prom.words[4], prom.words[5], prom.words[6], prom.words[7]);
}

void logSample(uint32_t nowMs) {
  const Sample& sample = sensor.sample();
  const int32_t temperature = sample.temperatureCentiC;
  const int32_t magnitude = temperature < 0 ? -temperature : temperature;
  Serial.printf("MS5611: t=%lu P=%ld Pa T=%s%ld.%02ld C\n", static_cast<unsigned long>(nowMs),
                static_cast<long>(sample.pressurePa), temperature < 0 ? "-" : "",
                static_cast<long>(magnitude / 100), static_cast<long>(magnitude % 100));
}

bool tryInit() {
  const Ms5611Error error = sensor.begin();
  if (error == Ms5611Error::Ok) {
    logInitOk();
    return true;
  }

  Serial.printf("MS5611: t=%lu init FAILED %s; retry in %lu ms\n",
                static_cast<unsigned long>(millis()), ms5611ErrorName(error),
                static_cast<unsigned long>(kRetryIntervalMs));
  return false;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cHz);
  delay(100);

  logBoot();
  initialized = tryInit();

  const uint32_t now = millis();
  nextInitAttemptMs = now + kRetryIntervalMs;
  nextReadMs = now + kReadIntervalMs;
}

void loop() {
  const uint32_t now = millis();

  if (!initialized) {
    if (static_cast<int32_t>(now - nextInitAttemptMs) >= 0) {
      initialized = tryInit();
      nextInitAttemptMs = millis() + kRetryIntervalMs;
    }
    return;
  }

  if (static_cast<int32_t>(now - nextReadMs) >= 0) {
    const Ms5611Error error = sensor.read();
    if (error == Ms5611Error::Ok) {
      logSample(millis());
    } else {
      Serial.printf("MS5611: t=%lu read FAILED %s; continuing\n",
                    static_cast<unsigned long>(millis()), ms5611ErrorName(error));
    }
    nextReadMs = millis() + kReadIntervalMs;
  }
}
