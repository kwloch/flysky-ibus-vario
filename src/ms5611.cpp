#include "ms5611.h"

namespace {

constexpr uint8_t kCmdReset = 0x1E;
constexpr uint8_t kCmdConvertD1Osr4096 = 0x48;
constexpr uint8_t kCmdConvertD2Osr4096 = 0x58;
constexpr uint8_t kCmdAdcRead = 0x00;
constexpr uint8_t kCmdPromRead = 0xA0;
constexpr uint16_t kConversionDelayMs = 10;
constexpr uint16_t kResetDelayMs = 3;

}  // namespace

MS5611::MS5611(TwoWire& wire, uint8_t address) : wire_(wire), address_(address) {}

bool MS5611::writeCommand(uint8_t command) {
  wire_.beginTransmission(address_);
  wire_.write(command);
  return wire_.endTransmission() == 0;
}

bool MS5611::readBytes(uint8_t command, uint8_t* data, size_t length) {
  wire_.beginTransmission(address_);
  wire_.write(command);
  if (wire_.endTransmission() != 0) {
    return false;
  }

  if (wire_.requestFrom(address_, length) != length) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    if (wire_.available() == 0) {
      return false;
    }
    data[i] = static_cast<uint8_t>(wire_.read());
  }
  return true;
}

bool MS5611::readPromWord(uint8_t index, uint16_t& word) {
  uint8_t data[2] = {0, 0};
  if (!readBytes(static_cast<uint8_t>(kCmdPromRead + 2 * index), data, sizeof(data))) {
    return false;
  }
  word = static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
  return true;
}

bool MS5611::readAdc(uint32_t& value) {
  uint8_t data[3] = {0, 0, 0};
  if (!readBytes(kCmdAdcRead, data, sizeof(data))) {
    return false;
  }
  value = (static_cast<uint32_t>(data[0]) << 16) |
          (static_cast<uint32_t>(data[1]) << 8) | static_cast<uint32_t>(data[2]);
  return true;
}

Ms5611Error MS5611::begin() {
  initialized_ = false;
  hasSample_ = false;

  wire_.beginTransmission(address_);
  if (wire_.endTransmission() != 0) {
    lastError_ = Ms5611Error::NotDetected;
    return lastError_;
  }

  if (!writeCommand(kCmdReset)) {
    lastError_ = Ms5611Error::ResetFailed;
    return lastError_;
  }
  delay(kResetDelayMs);

  for (uint8_t i = 0; i < kMs5611PromWords; ++i) {
    if (!readPromWord(i, prom_.words[i])) {
      lastError_ = Ms5611Error::PromReadFailed;
      return lastError_;
    }
  }

  if (!promCrcMatches(prom_)) {
    lastError_ = Ms5611Error::PromCrcMismatch;
    return lastError_;
  }

  if (!promCoefficientsValid(prom_)) {
    lastError_ = Ms5611Error::PromInvalid;
    return lastError_;
  }

  initialized_ = true;
  lastError_ = Ms5611Error::Ok;
  return lastError_;
}

Ms5611Error MS5611::read() {
  if (!initialized_) {
    lastError_ = Ms5611Error::NotInitialized;
    return lastError_;
  }

  if (!writeCommand(kCmdConvertD1Osr4096)) {
    lastError_ = Ms5611Error::ConversionFailed;
    return lastError_;
  }
  delay(kConversionDelayMs);

  uint32_t d1 = 0;
  if (!readAdc(d1)) {
    lastError_ = Ms5611Error::AdcReadFailed;
    return lastError_;
  }

  if (!writeCommand(kCmdConvertD2Osr4096)) {
    lastError_ = Ms5611Error::ConversionFailed;
    return lastError_;
  }
  delay(kConversionDelayMs);

  uint32_t d2 = 0;
  if (!readAdc(d2)) {
    lastError_ = Ms5611Error::AdcReadFailed;
    return lastError_;
  }

  const Sample next = compensateSelected(prom_, d1, d2);
  if (!inRange(next)) {
    lastError_ = Ms5611Error::OutOfRange;
    return lastError_;
  }

  sample_ = next;
  hasSample_ = true;
  lastError_ = Ms5611Error::Ok;
  return lastError_;
}
