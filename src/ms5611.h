#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "ms5611_math.h"

class MS5611 {
 public:
  explicit MS5611(TwoWire& wire, uint8_t address = 0x77);

  Ms5611Error begin();
  Ms5611Error read();

  bool hasSample() const { return hasSample_; }
  const Sample& sample() const { return sample_; }
  const Prom& prom() const { return prom_; }
  uint32_t rawD1() const { return d1_; }
  uint32_t rawD2() const { return d2_; }
  Ms5611Error lastError() const { return lastError_; }

 private:
  bool writeCommand(uint8_t command);
  bool readBytes(uint8_t command, uint8_t* data, size_t length);
  bool readPromWord(uint8_t index, uint16_t& word);
  bool readAdc(uint32_t& value);

  TwoWire& wire_;
  uint8_t address_;
  Prom prom_{};
  Sample sample_{};
  uint32_t d1_ = 0;
  uint32_t d2_ = 0;
  bool initialized_ = false;
  bool hasSample_ = false;
  Ms5611Error lastError_ = Ms5611Error::Ok;
};
