#pragma once

#include <cstdint>

constexpr uint8_t kIbusRxPin = 4;
constexpr uint8_t kIbusTxPin = 5;

struct IbusCounters {
  uint32_t polls;
  uint32_t responses;
};

class IbusTransport {
 public:
  virtual ~IbusTransport() = default;
  virtual void begin() = 0;
  virtual uint8_t addSensor(uint8_t type, uint8_t length) = 0;
  virtual void setSensorMeasurement(uint8_t address, int32_t value) = 0;
  virtual void poll() = 0;
  virtual IbusCounters counters() const = 0;
};

IbusTransport& createIbusTransport();
const char* ibusTransportName();
