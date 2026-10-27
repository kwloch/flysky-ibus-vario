#pragma once

#include <cstdint>

constexpr uint8_t kMs5611PromWords = 8;
constexpr int32_t kMs5611MinPressurePa = 10000;
constexpr int32_t kMs5611MaxPressurePa = 120000;
constexpr int32_t kMs5611MinTemperatureCentiC = -4000;
constexpr int32_t kMs5611MaxTemperatureCentiC = 8500;

struct Prom {
  uint16_t words[kMs5611PromWords];
};

struct Sample {
  int32_t pressurePa;
  int32_t temperatureCentiC;
};

enum class Ms5611Error : uint8_t {
  Ok,
  NotDetected,
  ResetFailed,
  PromReadFailed,
  PromCrcMismatch,
  PromInvalid,
  NotInitialized,
  ConversionFailed,
  AdcReadFailed,
  OutOfRange,
};

inline const char* ms5611ErrorName(Ms5611Error error) {
  switch (error) {
    case Ms5611Error::Ok:
      return "Ok";
    case Ms5611Error::NotDetected:
      return "NotDetected";
    case Ms5611Error::ResetFailed:
      return "ResetFailed";
    case Ms5611Error::PromReadFailed:
      return "PromReadFailed";
    case Ms5611Error::PromCrcMismatch:
      return "PromCrcMismatch";
    case Ms5611Error::PromInvalid:
      return "PromInvalid";
    case Ms5611Error::NotInitialized:
      return "NotInitialized";
    case Ms5611Error::ConversionFailed:
      return "ConversionFailed";
    case Ms5611Error::AdcReadFailed:
      return "AdcReadFailed";
    case Ms5611Error::OutOfRange:
      return "OutOfRange";
  }
  return "Unknown";
}

inline uint8_t crc4(Prom prom) {
  prom.words[0] &= 0x0FFF;
  prom.words[7] &= 0xFF00;

  uint16_t remainder = 0;
  for (uint8_t i = 0; i < 16; ++i) {
    if (i % 2 == 1) {
      remainder ^= prom.words[i >> 1] & 0x00FF;
    } else {
      remainder ^= prom.words[i >> 1] >> 8;
    }
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (remainder & 0x8000) {
        remainder = static_cast<uint16_t>((remainder << 1) ^ 0x3000);
      } else {
        remainder = static_cast<uint16_t>(remainder << 1);
      }
    }
  }

  return static_cast<uint8_t>((remainder >> 12) & 0x0F);
}

inline bool promCrcMatches(const Prom& prom) {
  return crc4(prom) == (prom.words[7] & 0x0F);
}

inline bool promCoefficientsValid(const Prom& prom) {
  for (uint8_t i = 1; i <= 6; ++i) {
    if (prom.words[i] == 0x0000 || prom.words[i] == 0xFFFF) {
      return false;
    }
  }
  return true;
}

inline bool promValid(const Prom& prom) {
  return promCrcMatches(prom) && promCoefficientsValid(prom);
}

inline Sample compensate(const Prom& prom, uint32_t d1, uint32_t d2) {
  const int32_t dt = static_cast<int32_t>(d2) -
                     static_cast<int32_t>(static_cast<uint32_t>(prom.words[5]) << 8);

  int64_t temperature = 2000 + ((static_cast<int64_t>(dt) * prom.words[6]) >> 23);
  int64_t offset = (static_cast<int64_t>(prom.words[2]) << 16) +
                   ((static_cast<int64_t>(prom.words[4]) * dt) >> 7);
  int64_t sensitivity = (static_cast<int64_t>(prom.words[1]) << 15) +
                        ((static_cast<int64_t>(prom.words[3]) * dt) >> 8);

  if (temperature < 2000) {
    const int64_t temperatureDelta = temperature - 2000;
    const int64_t t2 = (static_cast<int64_t>(dt) * dt) >> 31;
    int64_t offset2 = (5 * temperatureDelta * temperatureDelta) / 2;
    int64_t sensitivity2 = (5 * temperatureDelta * temperatureDelta) / 4;

    if (temperature < -1500) {
      const int64_t lowTemperatureDelta = temperature + 1500;
      offset2 += 7 * lowTemperatureDelta * lowTemperatureDelta;
      sensitivity2 += (11 * lowTemperatureDelta * lowTemperatureDelta) / 2;
    }

    temperature -= t2;
    offset -= offset2;
    sensitivity -= sensitivity2;
  }

  const int64_t pressure =
      (((static_cast<int64_t>(d1) * sensitivity) >> 21) - offset) >> 15;

  return Sample{static_cast<int32_t>(pressure), static_cast<int32_t>(temperature)};
}

inline bool inRange(const Sample& sample) {
  return sample.pressurePa >= kMs5611MinPressurePa &&
         sample.pressurePa <= kMs5611MaxPressurePa &&
         sample.temperatureCentiC >= kMs5611MinTemperatureCentiC &&
         sample.temperatureCentiC <= kMs5611MaxTemperatureCentiC;
}
