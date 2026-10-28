#include <cstdint>
#include <limits>

#include <unity.h>

#include "ibus_emulator.h"
#include "ibus_protocol.h"
#include "ibus_transport.h"

namespace {

class FakeTransport : public IbusTransport {
 public:
  void begin() override { ++beginCalls; }

  uint8_t addSensor(uint8_t type, uint8_t length) override {
    if (count < 4) {
      types[count] = type;
      lengths[count] = length;
    }
    ++count;
    return count;
  }

  void setSensorMeasurement(uint8_t address, int32_t value) override {
    if (address < 4) {
      values[address] = value;
    }
  }

  void poll() override { ++polls; }

  IbusCounters counters() const override { return IbusCounters{polls, 0}; }

  uint8_t count = 0;
  uint8_t types[4] = {};
  uint8_t lengths[4] = {};
  int32_t values[4] = {};
  uint32_t polls = 0;
  uint32_t beginCalls = 0;
};

const ibus::SensorDefinition& altSensor() { return ibus::kSensors[0]; }
const ibus::SensorDefinition& vspdSensor() { return ibus::kSensors[1]; }

void assertFrameEquals(const uint8_t* expected, const uint8_t* actual, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    TEST_ASSERT_EQUAL_HEX8(expected[i], actual[i]);
  }
}

void assertValidFrame(const uint8_t* frame, size_t length) {
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, ibus::frameSum(frame, length));
  TEST_ASSERT_TRUE(ibus::validateFrame(frame, length));
}

}  // namespace

void setUp(void) {}

void tearDown(void) {}

void testDiscoverResponseHasEchoAndChecksum() {
  uint8_t frame[ibus::kMaxResponseLength] = {};
  const size_t length = ibus::buildDiscoverResponse(altSensor(), frame, sizeof(frame));
  const uint8_t expected[] = {0x04, 0x81, 0x7A, 0xFF};

  TEST_ASSERT_EQUAL_UINT(size_t(4), length);
  assertFrameEquals(expected, frame, sizeof(expected));
  assertValidFrame(frame, length);
}

void testTypeResponsesExposeAltAndVspd() {
  uint8_t frame[ibus::kMaxResponseLength] = {};

  const size_t altLength = ibus::buildTypeResponse(altSensor(), frame, sizeof(frame));
  const uint8_t expectedAlt[] = {0x06, 0x91, 0x83, 0x04, 0xE1, 0xFE};
  TEST_ASSERT_EQUAL_UINT(size_t(6), altLength);
  assertFrameEquals(expectedAlt, frame, sizeof(expectedAlt));
  assertValidFrame(frame, altLength);

  const size_t vspdLength = ibus::buildTypeResponse(vspdSensor(), frame, sizeof(frame));
  TEST_ASSERT_EQUAL_UINT(size_t(6), vspdLength);
  TEST_ASSERT_EQUAL_HEX8(0x92, frame[1]);
  TEST_ASSERT_EQUAL_HEX8(0x09, frame[2]);
  TEST_ASSERT_EQUAL_HEX8(0x02, frame[3]);
  assertValidFrame(frame, vspdLength);
}

void testValueResponseLengthsAndLittleEndian() {
  uint8_t frame[ibus::kMaxResponseLength] = {};

  const size_t altLength = ibus::buildValueResponse(altSensor(), 123, frame, sizeof(frame));
  const uint8_t expectedAlt[] = {0x08, 0xA1, 0x7B, 0x00, 0x00, 0x00, 0xDB, 0xFE};
  TEST_ASSERT_EQUAL_UINT(size_t(8), altLength);
  assertFrameEquals(expectedAlt, frame, sizeof(expectedAlt));
  assertValidFrame(frame, altLength);

  const size_t vspdLength = ibus::buildValueResponse(vspdSensor(), 250, frame, sizeof(frame));
  const uint8_t expectedVspd[] = {0x06, 0xA2, 0xFA, 0x00, 0x5D, 0xFE};
  TEST_ASSERT_EQUAL_UINT(size_t(6), vspdLength);
  assertFrameEquals(expectedVspd, frame, sizeof(expectedVspd));
  assertValidFrame(frame, vspdLength);
}

void testAltitudeBoundariesEncodeSignedInt32() {
  uint8_t frame[ibus::kMaxResponseLength] = {};

  TEST_ASSERT_EQUAL_UINT(size_t(8),
                         ibus::buildValueResponse(altSensor(), std::numeric_limits<int32_t>::min(),
                                                  frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[3]);
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[4]);
  TEST_ASSERT_EQUAL_HEX8(0x80, frame[5]);
  assertValidFrame(frame, 8);

  TEST_ASSERT_EQUAL_UINT(size_t(8),
                         ibus::buildValueResponse(altSensor(), std::numeric_limits<int32_t>::max(),
                                                  frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8(0xFF, frame[2]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, frame[3]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, frame[4]);
  TEST_ASSERT_EQUAL_HEX8(0x7F, frame[5]);
  assertValidFrame(frame, 8);
}

void testVspdSignAndSaturation() {
  TEST_ASSERT_EQUAL_INT16(250, ibus::saturateVspd(250));
  TEST_ASSERT_EQUAL_INT16(-250, ibus::saturateVspd(-250));
  TEST_ASSERT_EQUAL_INT16(32767, ibus::saturateVspd(40000));
  TEST_ASSERT_EQUAL_INT16(-32768, ibus::saturateVspd(-40000));

  uint8_t frame[ibus::kMaxResponseLength] = {};
  TEST_ASSERT_EQUAL_UINT(size_t(6),
                         ibus::buildValueResponse(vspdSensor(), -250, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8(0x06, frame[2]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, frame[3]);
  assertValidFrame(frame, 6);

  TEST_ASSERT_EQUAL_UINT(size_t(6),
                         ibus::buildValueResponse(vspdSensor(), 40000, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8(0xFF, frame[2]);
  TEST_ASSERT_EQUAL_HEX8(0x7F, frame[3]);
  assertValidFrame(frame, 6);

  TEST_ASSERT_EQUAL_UINT(size_t(6),
                         ibus::buildValueResponse(vspdSensor(), -40000, frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_HEX8(0x00, frame[2]);
  TEST_ASSERT_EQUAL_HEX8(0x80, frame[3]);
  assertValidFrame(frame, 6);
}

void testInvalidFramesAreRejected() {
  uint8_t frame[ibus::kMaxResponseLength] = {};
  const size_t length = ibus::buildDiscoverResponse(altSensor(), frame, sizeof(frame));

  TEST_ASSERT_TRUE(ibus::validateFrame(frame, length));

  frame[2] ^= 0xFF;
  TEST_ASSERT_FALSE(ibus::validateFrame(frame, length));

  const size_t rebuilt = ibus::buildDiscoverResponse(altSensor(), frame, sizeof(frame));
  TEST_ASSERT_EQUAL_UINT(size_t(4), rebuilt);
  frame[0] = 0x05;
  TEST_ASSERT_FALSE(ibus::validateFrame(frame, length));

  uint8_t tooShort[] = {0x04, 0x81, 0x7A};
  TEST_ASSERT_FALSE(ibus::validateFrame(tooShort, sizeof(tooShort)));
  TEST_ASSERT_FALSE(ibus::validateFrame(nullptr, 4));
}

void testUnknownAddressIsRejected() {
  TEST_ASSERT_NOT_NULL(ibus::findSensor(1));
  TEST_ASSERT_NOT_NULL(ibus::findSensor(2));
  TEST_ASSERT_NULL(ibus::findSensor(0));
  TEST_ASSERT_NULL(ibus::findSensor(3));
  TEST_ASSERT_EQUAL_HEX8(ibus::kAltType, ibus::findSensor(1)->type);
  TEST_ASSERT_EQUAL_HEX8(ibus::kVspdType, ibus::findSensor(2)->type);
}

void testEmulatorRegistersTwoSensors() {
  FakeTransport transport;
  IbusSensorEmulator emulator(transport);

  emulator.begin();

  TEST_ASSERT_EQUAL_UINT8(2, transport.count);
  TEST_ASSERT_EQUAL_HEX8(0x83, transport.types[0]);
  TEST_ASSERT_EQUAL_UINT8(4, transport.lengths[0]);
  TEST_ASSERT_EQUAL_HEX8(0x09, transport.types[1]);
  TEST_ASSERT_EQUAL_UINT8(2, transport.lengths[1]);
  TEST_ASSERT_EQUAL_UINT8(1, emulator.altAddress());
  TEST_ASSERT_EQUAL_UINT8(2, emulator.vspdAddress());
}

void testEmulatorSendsZerosBeforeValid() {
  FakeTransport transport;
  transport.values[1] = 1234;
  transport.values[2] = 5678;
  IbusSensorEmulator emulator(transport);

  emulator.begin();
  TEST_ASSERT_EQUAL_INT32(0, transport.values[1]);
  TEST_ASSERT_EQUAL_INT32(0, transport.values[2]);

  emulator.update(VarioReading{false, 123, -250});
  TEST_ASSERT_EQUAL_INT32(0, transport.values[1]);
  TEST_ASSERT_EQUAL_INT32(0, transport.values[2]);
}

void testEmulatorUpdatesAfterValidReading() {
  FakeTransport transport;
  IbusSensorEmulator emulator(transport);
  emulator.begin();

  emulator.update(VarioReading{true, 123, -250});

  TEST_ASSERT_EQUAL_INT32(123, transport.values[1]);
  TEST_ASSERT_EQUAL_INT32(-250, transport.values[2]);
  TEST_ASSERT_EQUAL_INT32(123, emulator.altitudeCentiM());
  TEST_ASSERT_EQUAL_INT32(-250, emulator.climbRateCentiMps());
}

void testEmulatorSaturatesVspd() {
  FakeTransport transport;
  IbusSensorEmulator emulator(transport);
  emulator.begin();

  emulator.update(VarioReading{true, 0, 40000});
  TEST_ASSERT_EQUAL_INT32(32767, transport.values[2]);

  emulator.update(VarioReading{true, 0, -40000});
  TEST_ASSERT_EQUAL_INT32(-32768, transport.values[2]);
}

void testEmulatorKeepsLastValueAfterInvalidReading() {
  FakeTransport transport;
  IbusSensorEmulator emulator(transport);
  emulator.begin();

  emulator.update(VarioReading{true, 123, -250});
  emulator.update(VarioReading{false, 999, 999});

  TEST_ASSERT_EQUAL_INT32(123, transport.values[1]);
  TEST_ASSERT_EQUAL_INT32(-250, transport.values[2]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testDiscoverResponseHasEchoAndChecksum);
  RUN_TEST(testTypeResponsesExposeAltAndVspd);
  RUN_TEST(testValueResponseLengthsAndLittleEndian);
  RUN_TEST(testAltitudeBoundariesEncodeSignedInt32);
  RUN_TEST(testVspdSignAndSaturation);
  RUN_TEST(testInvalidFramesAreRejected);
  RUN_TEST(testUnknownAddressIsRejected);
  RUN_TEST(testEmulatorRegistersTwoSensors);
  RUN_TEST(testEmulatorSendsZerosBeforeValid);
  RUN_TEST(testEmulatorUpdatesAfterValidReading);
  RUN_TEST(testEmulatorSaturatesVspd);
  RUN_TEST(testEmulatorKeepsLastValueAfterInvalidReading);
  return UNITY_END();
}
