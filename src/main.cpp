#include <Arduino.h>
#include <Wire.h>

#include "ibus_emulator.h"
#include "ibus_transport.h"
#include "ms5611.h"
#include "vario_math.h"

#ifndef PRESSURE_SENSOR_DIAGNOSTIC
#define PRESSURE_SENSOR_DIAGNOSTIC 1
#endif

namespace {

constexpr uint8_t kSdaPin = 0;
constexpr uint8_t kSclPin = 1;
constexpr uint32_t kI2cHz = 400000;
constexpr uint8_t kAddress = 0x77;
constexpr uint32_t kSampleIntervalMs = kVarioSampleIntervalMs;
constexpr uint32_t kVarioLogIntervalMs = kVarioPublishIntervalMs;
constexpr uint32_t kPressureLogIntervalMs = 1000;
constexpr uint32_t kIbusLogIntervalMs = 1000;
constexpr uint32_t kRetryIntervalMs = 1000;

MS5611 sensor(Wire, kAddress);
VarioEstimator vario;
IbusTransport& ibusTransport = createIbusTransport();
IbusSensorEmulator ibusEmulator(ibusTransport);
bool initialized = false;
uint32_t nextInitAttemptMs = 0;
uint32_t nextSampleMs = 0;
uint32_t nextVarioLogMs = 0;
uint32_t nextPressureLogMs = 0;
uint32_t nextIbusLogMs = 0;
uint32_t lastSampleMs = 0;

bool timeReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void logBoot() {
  Serial.printf("MS5611: boot addr=0x%02X SDA=%u SCL=%u %lukHz\n", kAddress, kSdaPin,
                kSclPin, static_cast<unsigned long>(kI2cHz / 1000));
}

void logIbusBoot() {
  Serial.printf("IBUS: transport=%s rx=%u tx=%u alt=0x%02X vspd=0x%02X\n",
                ibusTransportName(), kIbusRxPin, kIbusTxPin, ibus::kAltType,
                ibus::kVspdType);
}

void logIbusCounters(uint32_t nowMs) {
  const IbusCounters counters = ibusTransport.counters();
  Serial.printf("IBUS: t=%lu poll=%lu resp=%lu\n", static_cast<unsigned long>(nowMs),
                static_cast<unsigned long>(counters.polls),
                static_cast<unsigned long>(counters.responses));
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

#if PRESSURE_SENSOR_DIAGNOSTIC
void logDiagnostic(uint32_t nowMs) {
  const Prom& prom = sensor.prom();
  const Sample ms5611 = compensate(prom, sensor.rawD1(), sensor.rawD2());
  const Sample ms5607 = compensateMs5607(prom, sensor.rawD1(), sensor.rawD2());
  const int32_t t5611 = ms5611.temperatureCentiC;
  const int32_t t5607 = ms5607.temperatureCentiC;
  const int32_t t5611Magnitude = t5611 < 0 ? -t5611 : t5611;
  const int32_t t5607Magnitude = t5607 < 0 ? -t5607 : t5607;

  Serial.printf(
      "DIAG: t=%lu D1=%lu D2=%lu P5611=%ld Pa P5607=%ld Pa "
      "T5611=%s%ld.%02ld C T5607=%s%ld.%02ld C\n",
      static_cast<unsigned long>(nowMs),
      static_cast<unsigned long>(sensor.rawD1()),
      static_cast<unsigned long>(sensor.rawD2()),
      static_cast<long>(ms5611.pressurePa),
      static_cast<long>(ms5607.pressurePa), t5611 < 0 ? "-" : "",
      static_cast<long>(t5611Magnitude / 100),
      static_cast<long>(t5611Magnitude % 100), t5607 < 0 ? "-" : "",
      static_cast<long>(t5607Magnitude / 100),
      static_cast<long>(t5607Magnitude % 100));
}
#endif

void logVario(uint32_t nowMs) {
  const VarioReading reading = vario.reading();
  ibusEmulator.update(reading);
  if (!reading.valid) {
    return;
  }

  const int32_t altitude = reading.altitudeCentiM;
  const int32_t climbRate = reading.climbRateCentiMps;
  const int32_t altitudeMagnitude = altitude < 0 ? -altitude : altitude;
  const int32_t climbRateMagnitude = climbRate < 0 ? -climbRate : climbRate;

  Serial.printf("VARIO: t=%lu alt=%s%ld.%02ld m vspd=%s%ld.%02ld m/s\n",
                static_cast<unsigned long>(nowMs), altitude < 0 ? "-" : "",
                static_cast<long>(altitudeMagnitude / 100),
                static_cast<long>(altitudeMagnitude % 100), climbRate < 0 ? "-" : "",
                static_cast<long>(climbRateMagnitude / 100),
                static_cast<long>(climbRateMagnitude % 100));
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

void scheduleAfterInit(uint32_t now) {
  vario.reset();
  lastSampleMs = now;
  nextSampleMs = now + kSampleIntervalMs;
  nextVarioLogMs = now + kVarioLogIntervalMs;
  nextPressureLogMs = now + kPressureLogIntervalMs;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  ibusTransport.begin();
  ibusEmulator.begin();
  logIbusBoot();

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cHz);
  delay(100);

  logBoot();
  initialized = tryInit();

  const uint32_t now = millis();
  if (initialized) {
    scheduleAfterInit(now);
  }
  nextInitAttemptMs = now + kRetryIntervalMs;
  nextIbusLogMs = now + kIbusLogIntervalMs;
}

void loop() {
  ibusTransport.poll();
  const uint32_t now = millis();

  if (timeReached(now, nextIbusLogMs)) {
    logIbusCounters(now);
    nextIbusLogMs = now + kIbusLogIntervalMs;
  }

  if (!initialized) {
    if (timeReached(now, nextInitAttemptMs)) {
      initialized = tryInit();
      const uint32_t attemptMs = millis();
      if (initialized) {
        scheduleAfterInit(attemptMs);
      }
      nextInitAttemptMs = attemptMs + kRetryIntervalMs;
    }
    return;
  }

  if (timeReached(now, nextSampleMs)) {
    const uint32_t sampleMs = millis();
    const Ms5611Error error = sensor.read();
    if (error == Ms5611Error::Ok) {
      const uint32_t dtMs = sampleMs - lastSampleMs;
      const Sample& sample = sensor.sample();
      vario.update(sample.pressurePa, sample.temperatureCentiC, dtMs);
      lastSampleMs = sampleMs;
    } else {
      Serial.printf("MS5611: t=%lu read FAILED %s; continuing\n",
                    static_cast<unsigned long>(sampleMs), ms5611ErrorName(error));
    }
    nextSampleMs = sampleMs + kSampleIntervalMs;
    if (timeReached(millis(), nextSampleMs)) {
      nextSampleMs = millis() + kSampleIntervalMs;
    }
  }

  if (timeReached(now, nextVarioLogMs)) {
    const uint32_t logMs = millis();
    logVario(logMs);
    nextVarioLogMs = logMs + kVarioLogIntervalMs;
  }

  if (timeReached(now, nextPressureLogMs)) {
    const uint32_t logMs = millis();
    if (sensor.hasSample()) {
      logSample(logMs);
#if PRESSURE_SENSOR_DIAGNOSTIC
      if (sensor.lastError() == Ms5611Error::Ok) {
        logDiagnostic(logMs);
      }
#endif
    }
    nextPressureLogMs = logMs + kPressureLogIntervalMs;
  }
}
