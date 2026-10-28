#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace ibus {

constexpr uint8_t kAltType = 0x83;
constexpr uint8_t kAltLength = 4;
constexpr uint8_t kVspdType = 0x09;
constexpr uint8_t kVspdLength = 2;

constexpr uint8_t kCommandDiscover = 0x80;
constexpr uint8_t kCommandType = 0x90;
constexpr uint8_t kCommandValue = 0xA0;

constexpr uint8_t kMinFrameLength = 4;
constexpr uint8_t kMaxFrameLength = 0x20;
constexpr size_t kMaxResponseLength = 8;

struct SensorDefinition {
  uint8_t address;
  uint8_t type;
  uint8_t length;
};

constexpr SensorDefinition kSensors[] = {
    {1, kAltType, kAltLength},
    {2, kVspdType, kVspdLength},
};

constexpr size_t kSensorCount = sizeof(kSensors) / sizeof(kSensors[0]);

inline const SensorDefinition* findSensor(uint8_t address) {
  for (size_t i = 0; i < kSensorCount; ++i) {
    if (kSensors[i].address == address) {
      return &kSensors[i];
    }
  }
  return nullptr;
}

inline uint16_t checksum(const uint8_t* bytes, size_t length) {
  uint16_t sum = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    sum = static_cast<uint16_t>(sum - bytes[i]);
  }
  return sum;
}

inline uint16_t frameSum(const uint8_t* frame, size_t length) {
  if (frame == nullptr || length < 2) {
    return 0;
  }
  uint16_t sum = 0;
  for (size_t i = 0; i + 2 < length; ++i) {
    sum = static_cast<uint16_t>(sum + frame[i]);
  }
  const uint16_t chk =
      static_cast<uint16_t>(frame[length - 2] | (frame[length - 1] << 8));
  return static_cast<uint16_t>(sum + chk);
}

inline bool validateFrame(const uint8_t* frame, size_t length) {
  if (frame == nullptr) {
    return false;
  }
  if (length < kMinFrameLength || length > kMaxFrameLength) {
    return false;
  }
  if (frame[0] != length) {
    return false;
  }
  return frameSum(frame, length) == 0xFFFF;
}

inline int16_t saturateVspd(int32_t value) {
  if (value > std::numeric_limits<int16_t>::max()) {
    return std::numeric_limits<int16_t>::max();
  }
  if (value < std::numeric_limits<int16_t>::min()) {
    return std::numeric_limits<int16_t>::min();
  }
  return static_cast<int16_t>(value);
}

inline void writeLittleEndian(uint32_t value, uint8_t* out, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    out[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
  }
}

inline size_t buildDiscoverResponse(const SensorDefinition& sensor, uint8_t* out,
                                    size_t capacity) {
  constexpr size_t length = 4;
  if (out == nullptr || capacity < length) {
    return 0;
  }
  out[0] = static_cast<uint8_t>(length);
  out[1] = static_cast<uint8_t>(kCommandDiscover | sensor.address);
  const uint16_t chk = checksum(out, 2);
  out[2] = static_cast<uint8_t>(chk & 0xFF);
  out[3] = static_cast<uint8_t>(chk >> 8);
  return length;
}

inline size_t buildTypeResponse(const SensorDefinition& sensor, uint8_t* out,
                                size_t capacity) {
  constexpr size_t length = 6;
  if (out == nullptr || capacity < length) {
    return 0;
  }
  out[0] = static_cast<uint8_t>(length);
  out[1] = static_cast<uint8_t>(kCommandType | sensor.address);
  out[2] = sensor.type;
  out[3] = sensor.length;
  const uint16_t chk = checksum(out, 4);
  out[4] = static_cast<uint8_t>(chk & 0xFF);
  out[5] = static_cast<uint8_t>(chk >> 8);
  return length;
}

inline size_t buildValueResponse(const SensorDefinition& sensor, int32_t value,
                                 uint8_t* out, size_t capacity) {
  if (out == nullptr || (sensor.length != 2 && sensor.length != 4)) {
    return 0;
  }
  const size_t length = static_cast<size_t>(sensor.length) + 4;
  if (capacity < length) {
    return 0;
  }
  out[0] = static_cast<uint8_t>(length);
  out[1] = static_cast<uint8_t>(kCommandValue | sensor.address);
  if (sensor.length == 2) {
    writeLittleEndian(static_cast<uint16_t>(saturateVspd(value)), out + 2, 2);
  } else {
    writeLittleEndian(static_cast<uint32_t>(value), out + 2, 4);
  }
  const uint16_t chk = checksum(out, length - 2);
  out[length - 2] = static_cast<uint8_t>(chk & 0xFF);
  out[length - 1] = static_cast<uint8_t>(chk >> 8);
  return length;
}

}  // namespace ibus
