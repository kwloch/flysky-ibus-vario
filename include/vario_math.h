#pragma once

#include <cmath>
#include <cstdint>

constexpr uint32_t kVarioSampleIntervalMs = 50;
constexpr uint32_t kVarioPublishIntervalMs = 200;
constexpr uint32_t kVarioMaxGapMs = 250;
constexpr uint32_t kVarioMinDtMs = 25;
constexpr float kVarioAlpha = 0.4f;
constexpr float kVarioBeta = 0.2f;
constexpr float kVarioBarometricScaleM = 44330.0f;
constexpr float kVarioBarometricExponent = 1.0f / 5.255f;

struct VarioReading {
  bool valid;
  int32_t altitudeCentiM;
  int32_t climbRateCentiMps;
};

inline float pressureToAltitudeMeters(float pressurePa, float referencePa) {
  if (pressurePa <= 0.0f || referencePa <= 0.0f) {
    return 0.0f;
  }
  return kVarioBarometricScaleM *
         (1.0f - std::pow(pressurePa / referencePa, kVarioBarometricExponent));
}

class VarioEstimator {
 public:
  void reset() {
    referencePa_ = 0.0f;
    altitudeM_ = 0.0f;
    climbRateMps_ = 0.0f;
    valid_ = false;
  }

  bool hasReference() const { return valid_; }

  float referencePressurePa() const { return referencePa_; }

  void update(int32_t pressurePa, uint32_t dtMs) {
    if (pressurePa <= 0) {
      return;
    }

    if (!valid_) {
      referencePa_ = static_cast<float>(pressurePa);
      altitudeM_ = 0.0f;
      climbRateMps_ = 0.0f;
      valid_ = true;
      return;
    }

    const float measuredM =
        pressureToAltitudeMeters(static_cast<float>(pressurePa), referencePa_);

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
  float altitudeM_ = 0.0f;
  float climbRateMps_ = 0.0f;
  bool valid_ = false;
};
