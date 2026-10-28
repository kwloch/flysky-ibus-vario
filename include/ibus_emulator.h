#pragma once

#include <cstdint>

#include "ibus_protocol.h"
#include "ibus_transport.h"
#include "vario_math.h"

class IbusSensorEmulator {
 public:
  explicit IbusSensorEmulator(IbusTransport& transport) : transport_(transport) {}

  void begin() {
    altAddress_ = transport_.addSensor(ibus::kAltType, ibus::kAltLength);
    vspdAddress_ = transport_.addSensor(ibus::kVspdType, ibus::kVspdLength);
    altitudeCentiM_ = 0;
    climbRateCentiMps_ = 0;
    transport_.setSensorMeasurement(altAddress_, 0);
    transport_.setSensorMeasurement(vspdAddress_, 0);
  }

  void update(const VarioReading& reading) {
    if (!reading.valid) {
      return;
    }
    altitudeCentiM_ = reading.altitudeCentiM;
    climbRateCentiMps_ = ibus::saturateVspd(reading.climbRateCentiMps);
    transport_.setSensorMeasurement(altAddress_, altitudeCentiM_);
    transport_.setSensorMeasurement(vspdAddress_, climbRateCentiMps_);
  }

  uint8_t altAddress() const { return altAddress_; }
  uint8_t vspdAddress() const { return vspdAddress_; }
  int32_t altitudeCentiM() const { return altitudeCentiM_; }
  int32_t climbRateCentiMps() const { return climbRateCentiMps_; }

 private:
  IbusTransport& transport_;
  uint8_t altAddress_ = 0;
  uint8_t vspdAddress_ = 0;
  int32_t altitudeCentiM_ = 0;
  int32_t climbRateCentiMps_ = 0;
};
