#include <initializer_list>

#include <unity.h>

#include "ms5611_math.h"

namespace {

Prom datasheetProm() {
  return Prom{{0x0001, 0x9CBF, 0x903C, 0x5B15, 0x5AF2, 0x82B8, 0x6E98, 0x123B}};
}

Prom userProm() {
  return Prom{{0x089C, 0x998C, 0x8E4E, 0x5E3A, 0x585F, 0x881E, 0x6AC1, 0x6663}};
}

void fixCrc(Prom& prom) {
  prom.words[7] = static_cast<uint16_t>((prom.words[7] & 0xFF00) | crc4(prom));
}

}  // namespace

void setUp(void) {}

void tearDown(void) {}

void testCrc4MatchesDatasheet() {
  const Prom prom = datasheetProm();
  TEST_ASSERT_EQUAL_HEX8(0x0B, crc4(prom));
  TEST_ASSERT_TRUE(promCrcMatches(prom));
  TEST_ASSERT_TRUE(promCoefficientsValid(prom));
  TEST_ASSERT_TRUE(promValid(prom));
}

void testCrc4DetectsChangedCoefficient() {
  Prom prom = datasheetProm();
  prom.words[6] ^= 0x0001;
  TEST_ASSERT_FALSE(promCrcMatches(prom));
  TEST_ASSERT_TRUE(promCoefficientsValid(prom));
  TEST_ASSERT_FALSE(promValid(prom));
}

void testPromRejectsErasedCoefficientsEvenWithValidCrc() {
  for (uint8_t index = 1; index <= 6; ++index) {
    for (uint16_t value : {static_cast<uint16_t>(0x0000), static_cast<uint16_t>(0xFFFF)}) {
      Prom prom = datasheetProm();
      prom.words[index] = value;
      fixCrc(prom);
      TEST_ASSERT_TRUE(promCrcMatches(prom));
      TEST_ASSERT_FALSE(promCoefficientsValid(prom));
      TEST_ASSERT_FALSE(promValid(prom));
    }
  }
}

void testCompensationNominalDatasheetVector() {
  const Sample sample = compensate(datasheetProm(), 9085466, 8569150);
  TEST_ASSERT_EQUAL_INT32(100009, sample.pressurePa);
  TEST_ASSERT_EQUAL_INT32(2007, sample.temperatureCentiC);
  TEST_ASSERT_TRUE(inRange(sample));
}

void testCompensationSecondOrderBelowTwentyDegrees() {
  const Sample sample = compensate(datasheetProm(), 9085466, 8000000);
  TEST_ASSERT_EQUAL_INT32(95989, sample.pressurePa);
  TEST_ASSERT_EQUAL_INT32(-62, sample.temperatureCentiC);
  TEST_ASSERT_TRUE(inRange(sample));
}

void testCompensationSecondOrderBelowMinusFifteenDegrees() {
  const Sample sample = compensate(datasheetProm(), 9085466, 7500000);
  TEST_ASSERT_EQUAL_INT32(91910, sample.pressurePa);
  TEST_ASSERT_EQUAL_INT32(-2130, sample.temperatureCentiC);
  TEST_ASSERT_TRUE(inRange(sample));
}

void testCompensationMs5607IsDoubleMs5611() {
  Prom prom = datasheetProm();
  const Prom before = prom;
  const Sample ms5611 = compensate(prom, 9085466, 8569150);
  const Sample ms5607 = compensateMs5607(prom, 9085466, 8569150);

  TEST_ASSERT_EQUAL_INT32(100009, ms5611.pressurePa);
  TEST_ASSERT_EQUAL_INT32(2007, ms5611.temperatureCentiC);
  TEST_ASSERT_EQUAL_INT32(200018, ms5607.pressurePa);
  TEST_ASSERT_EQUAL_INT32(2007, ms5607.temperatureCentiC);
  TEST_ASSERT_FALSE(inRange(ms5607));

  for (uint8_t i = 0; i < kMs5611PromWords; ++i) {
    TEST_ASSERT_EQUAL_HEX16(before.words[i], prom.words[i]);
  }
}

void testCompensationMs5607UserVector() {
  const Sample sample = compensateMs5607(userProm(), 6476028, 9086870);
  TEST_ASSERT_EQUAL_INT32(98206, sample.pressurePa);
  TEST_ASSERT_EQUAL_INT32(2541, sample.temperatureCentiC);
  TEST_ASSERT_TRUE(inRange(sample));
}

void testSelectedCompensationUsesBuildVariant() {
#if PRESSURE_SENSOR_MS5607
  const Sample sample = compensateSelected(userProm(), 6476028, 9086870);
  TEST_ASSERT_EQUAL_INT32(98206, sample.pressurePa);
  TEST_ASSERT_EQUAL_INT32(2541, sample.temperatureCentiC);
#else
  const Sample sample = compensateSelected(datasheetProm(), 9085466, 8569150);
  TEST_ASSERT_EQUAL_INT32(100009, sample.pressurePa);
  TEST_ASSERT_EQUAL_INT32(2007, sample.temperatureCentiC);
#endif
}

void testRangeAcceptsInclusiveBoundaries() {
  TEST_ASSERT_TRUE(inRange(Sample{10000, -4000}));
  TEST_ASSERT_TRUE(inRange(Sample{120000, 8500}));
  TEST_ASSERT_TRUE(inRange(Sample{10000, 8500}));
  TEST_ASSERT_TRUE(inRange(Sample{120000, -4000}));
}

void testRangeRejectsOutOfBounds() {
  TEST_ASSERT_FALSE(inRange(Sample{9999, 0}));
  TEST_ASSERT_FALSE(inRange(Sample{120001, 0}));
  TEST_ASSERT_FALSE(inRange(Sample{101325, -4001}));
  TEST_ASSERT_FALSE(inRange(Sample{101325, 8501}));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testCrc4MatchesDatasheet);
  RUN_TEST(testCrc4DetectsChangedCoefficient);
  RUN_TEST(testPromRejectsErasedCoefficientsEvenWithValidCrc);
  RUN_TEST(testCompensationNominalDatasheetVector);
  RUN_TEST(testCompensationSecondOrderBelowTwentyDegrees);
  RUN_TEST(testCompensationSecondOrderBelowMinusFifteenDegrees);
  RUN_TEST(testCompensationMs5607IsDoubleMs5611);
  RUN_TEST(testCompensationMs5607UserVector);
  RUN_TEST(testSelectedCompensationUsesBuildVariant);
  RUN_TEST(testRangeAcceptsInclusiveBoundaries);
  RUN_TEST(testRangeRejectsOutOfBounds);
  return UNITY_END();
}
