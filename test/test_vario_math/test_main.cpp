#include <cmath>
#include <cstdint>

#include <unity.h>

#include "vario_math.h"

namespace {

constexpr float kReferencePa = 101325.0f;
constexpr uint32_t kDtMs = 50;
constexpr int kConvergenceIterations = 300;

int32_t pressureForAltitudePa(float altitudeM, float referencePa = kReferencePa) {
  const float pressure =
      referencePa * std::pow(1.0f - altitudeM / kVarioBarometricScaleM, 5.255f);
  return static_cast<int32_t>(std::lround(pressure));
}

void establishReference(VarioEstimator& vario,
                        int32_t pressurePa = static_cast<int32_t>(kReferencePa)) {
  for (uint32_t i = 0; i < kVarioReferenceSamples; ++i) {
    vario.update(pressurePa, kDtMs);
  }
}

void settleAtAltitude(VarioEstimator& vario, float altitudeM) {
  const int32_t pressurePa = pressureForAltitudePa(altitudeM);
  for (int i = 0; i < kConvergenceIterations; ++i) {
    vario.update(pressurePa, kDtMs);
  }
}

}  // namespace

void setUp(void) {}

void tearDown(void) {}

void testReferenceIsAveragedAndStartsAtZero() {
  VarioEstimator vario;
  vario.reset();

  for (uint32_t i = 0; i < kVarioReferenceSamples - 1; ++i) {
    vario.update(101000, kDtMs);
  }
  TEST_ASSERT_FALSE(vario.hasReference());

  vario.update(101020, kDtMs);

  TEST_ASSERT_TRUE(vario.hasReference());
  TEST_ASSERT_EQUAL_FLOAT(101001.0f, vario.referencePressurePa());

  const VarioReading reading = vario.reading();
  TEST_ASSERT_TRUE(reading.valid);
  TEST_ASSERT_EQUAL_INT32(0, reading.altitudeCentiM);
  TEST_ASSERT_EQUAL_INT32(0, reading.climbRateCentiMps);
}

void testNoReadingBeforeReferenceWindow() {
  VarioEstimator vario;
  vario.reset();

  for (uint32_t i = 0; i < kVarioReferenceSamples - 1; ++i) {
    vario.update(static_cast<int32_t>(kReferencePa), kDtMs);
  }

  const VarioReading reading = vario.reading();
  TEST_ASSERT_FALSE(reading.valid);
  TEST_ASSERT_EQUAL_INT32(0, reading.altitudeCentiM);
  TEST_ASSERT_EQUAL_INT32(0, reading.climbRateCentiMps);
}

void testPressureToAltitudeKnownValues() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                           pressureToAltitudeMeters(kReferencePa, kReferencePa));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000.0f,
                           pressureToAltitudeMeters(89876.0f, kReferencePa));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -1000.0f,
                           pressureToAltitudeMeters(113927.0f, kReferencePa));
}

void testAltitudeSignAboveAndBelowReference() {
  VarioEstimator above;
  above.reset();
  establishReference(above);
  settleAtAltitude(above, 100.0f);
  const VarioReading aboveReading = above.reading();
  TEST_ASSERT_TRUE(aboveReading.altitudeCentiM > 0);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f,
                           static_cast<float>(aboveReading.altitudeCentiM) / 100.0f);

  VarioEstimator below;
  below.reset();
  establishReference(below);
  settleAtAltitude(below, -100.0f);
  const VarioReading belowReading = below.reading();
  TEST_ASSERT_TRUE(belowReading.altitudeCentiM < 0);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -100.0f,
                           static_cast<float>(belowReading.altitudeCentiM) / 100.0f);
}

void testStationaryPressureKeepsZero() {
  VarioEstimator vario;
  vario.reset();
  establishReference(vario);

  for (int i = 0; i < kConvergenceIterations; ++i) {
    vario.update(static_cast<int32_t>(kReferencePa), kDtMs);
  }

  const VarioReading reading = vario.reading();
  TEST_ASSERT_EQUAL_INT32(0, reading.altitudeCentiM);
  TEST_ASSERT_EQUAL_INT32(0, reading.climbRateCentiMps);
}

void testClimbRateMeetsLatencyBudget() {
  VarioEstimator vario;
  vario.reset();
  establishReference(vario);

  VarioReading at700Ms{};
  VarioReading at1000Ms{};
  for (int step = 1; step <= 20; ++step) {
    const float trueAltitudeM = 2.0f * static_cast<float>(step) * 0.05f;
    vario.update(pressureForAltitudePa(trueAltitudeM), kDtMs);
    if (step == 14) {
      at700Ms = vario.reading();
    }
    if (step == 20) {
      at1000Ms = vario.reading();
    }
  }

  TEST_ASSERT_GREATER_OR_EQUAL_INT32(180, at700Ms.climbRateCentiMps);
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 2.0f,
                           static_cast<float>(at1000Ms.altitudeCentiM) / 100.0f);
}

void testSingleOutlierIsAttenuated() {
  VarioEstimator vario;
  vario.reset();
  establishReference(vario);

  vario.update(pressureForAltitudePa(0.1f), kDtMs);

  const VarioReading reading = vario.reading();
  TEST_ASSERT_TRUE(std::abs(reading.altitudeCentiM) < 5);
  TEST_ASSERT_TRUE(std::abs(reading.climbRateCentiMps) <= 50);
}

void testAlternatingNoiseIsAttenuated() {
  VarioEstimator vario;
  vario.reset();
  establishReference(vario);

  for (int sample = 0; sample < 40; ++sample) {
    const float altitudeM = (sample % 2 == 0) ? 0.1f : -0.1f;
    vario.update(pressureForAltitudePa(altitudeM), kDtMs);
    const VarioReading reading = vario.reading();
    TEST_ASSERT_TRUE(std::abs(reading.climbRateCentiMps) <= 30);
  }
}

void testGapResetsWithoutSpikeAndKeepsReference() {
  VarioEstimator vario;
  vario.reset();
  establishReference(vario);
  const float referencePa = vario.referencePressurePa();

  vario.update(pressureForAltitudePa(50.0f), 400);

  TEST_ASSERT_EQUAL_FLOAT(referencePa, vario.referencePressurePa());
  const VarioReading reading = vario.reading();
  TEST_ASSERT_EQUAL_INT32(0, reading.climbRateCentiMps);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f,
                           static_cast<float>(reading.altitudeCentiM) / 100.0f);
}

void testOutputIsRoundedToCentimeters() {
  VarioEstimator vario;
  vario.reset();
  establishReference(vario);
  settleAtAltitude(vario, 12.34f);

  const int32_t pressurePa = pressureForAltitudePa(12.34f);
  const float measuredM =
      pressureToAltitudeMeters(static_cast<float>(pressurePa), kReferencePa);
  const int32_t expectedCentiM =
      static_cast<int32_t>(std::lround(measuredM * 100.0f));

  const VarioReading reading = vario.reading();
  TEST_ASSERT_EQUAL_INT32(expectedCentiM, reading.altitudeCentiM);
  TEST_ASSERT_EQUAL_INT32(0, reading.climbRateCentiMps);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testReferenceIsAveragedAndStartsAtZero);
  RUN_TEST(testNoReadingBeforeReferenceWindow);
  RUN_TEST(testPressureToAltitudeKnownValues);
  RUN_TEST(testAltitudeSignAboveAndBelowReference);
  RUN_TEST(testStationaryPressureKeepsZero);
  RUN_TEST(testClimbRateMeetsLatencyBudget);
  RUN_TEST(testSingleOutlierIsAttenuated);
  RUN_TEST(testAlternatingNoiseIsAttenuated);
  RUN_TEST(testGapResetsWithoutSpikeAndKeepsReference);
  RUN_TEST(testOutputIsRoundedToCentimeters);
  return UNITY_END();
}
