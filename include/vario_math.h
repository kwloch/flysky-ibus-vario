#pragma once

#include <cmath>
#include <cstdint>

#ifndef VARIO_TEMPERATURE_CORRECTION
#define VARIO_TEMPERATURE_CORRECTION 1
#endif

constexpr uint32_t kVarioSampleIntervalMs = 50;
constexpr uint32_t kVarioPublishIntervalMs = 200;
constexpr uint32_t kVarioMaxGapMs = 250;
constexpr uint32_t kVarioMinDtMs = 25;
constexpr uint32_t kVarioReferenceSamples = 20;
constexpr float kVarioAlpha = 0.1f;
constexpr float kVarioBeta = 0.02f;
constexpr float kVarioBarometricScaleM = 44330.0f;
constexpr float kVarioBarometricExponent = 1.0f / 5.255f;
constexpr bool kVarioTemperatureCorrectionEnabled =
    VARIO_TEMPERATURE_CORRECTION != 0;
constexpr float kVarioCelsiusOffsetK = 273.15f;
constexpr int32_t kVarioStandardTemperatureCentiC = 1500;
constexpr float kVarioStandardTemperatureK = 288.15f;
constexpr int32_t kVarioMinTemperatureCentiC = -4000;
constexpr int32_t kVarioMaxTemperatureCentiC = 8500;
constexpr float kVarioMinTemperatureK =
    static_cast<float>(kVarioMinTemperatureCentiC) / 100.0f +
    kVarioCelsiusOffsetK;
constexpr float kVarioMaxTemperatureK =
    static_cast<float>(kVarioMaxTemperatureCentiC) / 100.0f +
    kVarioCelsiusOffsetK;

struct VarioReading {
  bool valid;
  int32_t altitudeCentiM;
  int32_t climbRateCentiMps;
};

inline bool isValidTemperature(int32_t temperatureCentiC) {
  return temperatureCentiC >= kVarioMinTemperatureCentiC &&
         temperatureCentiC <= kVarioMaxTemperatureCentiC;
}

inline float temperatureCentiCToKelvin(int32_t temperatureCentiC) {
  return static_cast<float>(temperatureCentiC) / 100.0f +
         kVarioCelsiusOffsetK;
}

inline float pressureToAltitudeMeters(float pressurePa, float referencePa,
                                      float referenceTemperatureK) {
  if (pressurePa <= 0.0f || referencePa <= 0.0f) {
    return 0.0f;
  }

  float temperatureK = referenceTemperatureK;
  if (!kVarioTemperatureCorrectionEnabled ||
      !std::isfinite(temperatureK) ||
      temperatureK < kVarioMinTemperatureK ||
      temperatureK > kVarioMaxTemperatureK) {
    temperatureK = kVarioStandardTemperatureK;
  }

  return kVarioBarometricScaleM *
         (temperatureK / kVarioStandardTemperatureK) *
         (1.0f -
          std::pow(pressurePa / referencePa, kVarioBarometricExponent));
}

inline float pressureToAltitudeMeters(float pressurePa, float referencePa) {
  return pressureToAltitudeMeters(pressurePa, referencePa,
                                  kVarioStandardTemperatureK);
}

class VarioEstimator {
 public:
  void reset() {
    referencePa_ = 0.0f;
    referenceSumPa_ = 0;
    referenceSamples_ = 0;
    referenceTemperatureK_ = kVarioStandardTemperatureK;
    referenceTemperatureSumCentiC_ = 0;
    referenceTemperatureSamples_ = 0;
    altitudeM_ = 0.0f;
    climbRateMps_ = 0.0f;
    valid_ = false;
  }

  bool hasReference() const { return valid_; }

  float referencePressurePa() const { return referencePa_; }

  float referenceTemperatureK() const { return referenceTemperatureK_; }

  void update(int32_t pressurePa, uint32_t dtMs) {
    update(pressurePa, kVarioStandardTemperatureCentiC, dtMs);
  }

  void update(int32_t pressurePa, int32_t temperatureCentiC, uint32_t dtMs) {
    if (pressurePa <= 0) {
      return;
    }

    if (!kVarioTemperatureCorrectionEnabled) {
      temperatureCentiC = kVarioStandardTemperatureCentiC;
    }

    if (!valid_) {
      if (referenceSamples_ > 0 && dtMs > kVarioMaxGapMs) {
        referenceSumPa_ = pressurePa;
        referenceSamples_ = 1;
        referenceTemperatureSumCentiC_ = 0;
        referenceTemperatureSamples_ = 0;
        if (isValidTemperature(temperatureCentiC)) {
          referenceTemperatureSumCentiC_ = temperatureCentiC;
          referenceTemperatureSamples_ = 1;
        }
        return;
      }

      referenceSumPa_ += pressurePa;
      ++referenceSamples_;
      if (isValidTemperature(temperatureCentiC)) {
        referenceTemperatureSumCentiC_ += temperatureCentiC;
        ++referenceTemperatureSamples_;
      }
      if (referenceSamples_ >= kVarioReferenceSamples) {
        referencePa_ = static_cast<float>(referenceSumPa_) /
                       static_cast<float>(referenceSamples_);
        referenceTemperatureK_ =
            referenceTemperatureSamples_ > 0
                ? static_cast<float>(referenceTemperatureSumCentiC_) /
                          static_cast<float>(referenceTemperatureSamples_) /
                          100.0f +
                      kVarioCelsiusOffsetK
                : kVarioStandardTemperatureK;
        altitudeM_ = 0.0f;
        climbRateMps_ = 0.0f;
        valid_ = true;
      }
      return;
    }

    const float measuredM = pressureToAltitudeMeters(
        static_cast<float>(pressurePa), referencePa_, referenceTemperatureK_);

    if (dtMs > kVarioMaxGapMs) {
      altitudeM_ = measuredM;
      climbRateMps_ = 0.0f;
      return;
    }

    uint32_t boundedDtMs = dtMs;
    if (boundedDtMs < kVarioMinDtMs) {
      boundedDtMs = kVarioMinDtMs;
    }
    const float dtSeconds = static_cast<float>(boundedDtMs) / 1000.0f;

    const float predictedM = altitudeM_ + climbRateMps_ * dtSeconds;
    const float innovationM = measuredM - predictedM;

    altitudeM_ = predictedM + kVarioAlpha * innovationM;
    climbRateMps_ += kVarioBeta * innovationM / dtSeconds;
  }

  VarioReading reading() const {
    if (!valid_) {
      return VarioReading{false, 0, 0};
    }
    return VarioReading{
        true,
        static_cast<int32_t>(std::lround(altitudeM_ * 100.0f)),
        static_cast<int32_t>(std::lround(climbRateMps_ * 100.0f)),
    };
  }

 private:
  float referencePa_ = 0.0f;
  int64_t referenceSumPa_ = 0;
  uint32_t referenceSamples_ = 0;
  float referenceTemperatureK_ = kVarioStandardTemperatureK;
  int64_t referenceTemperatureSumCentiC_ = 0;
  uint32_t referenceTemperatureSamples_ = 0;
  float altitudeM_ = 0.0f;
  float climbRateMps_ = 0.0f;
  bool valid_ = false;
};
