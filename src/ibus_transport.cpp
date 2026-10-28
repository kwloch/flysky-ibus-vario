#include <Arduino.h>
#include <IBusBM.h>

#include "ibus_protocol.h"
#include "ibus_transport.h"

namespace {

constexpr uint32_t kIbusBaud = 115200;
constexpr uint8_t kMaxSensors = 8;

class IBusBMTransport : public IbusTransport {
 public:
  void begin() override {
    ibus_.begin(Serial1, 1, kIbusRxPin, kIbusTxPin);
    lastPolls_ = ibus_.cnt_poll;
    lastResponses_ = ibus_.cnt_sensor;
  }

  uint8_t addSensor(uint8_t type, uint8_t length) override {
    return ibus_.addSensor(type, length);
  }

  void setSensorMeasurement(uint8_t address, int32_t value) override {
    ibus_.setSensorMeasurement(address, value);
  }

  void poll() override {
    const uint8_t polls = ibus_.cnt_poll;
    const uint8_t responses = ibus_.cnt_sensor;
    counters_.polls += static_cast<uint8_t>(polls - lastPolls_);
    counters_.responses += static_cast<uint8_t>(responses - lastResponses_);
    lastPolls_ = polls;
    lastResponses_ = responses;
  }

  IbusCounters counters() const override { return counters_; }

 private:
  IBusBM ibus_;
  IbusCounters counters_{0, 0};
  uint8_t lastPolls_ = 0;
  uint8_t lastResponses_ = 0;
};

class HalfDuplexTransport : public IbusTransport {
 public:
  void begin() override {
    Serial1.begin(kIbusBaud, SERIAL_8N1, kIbusRxPin, kIbusTxPin);
    pinMode(kIbusTxPin, INPUT);
    state_ = State::kDiscard;
    lastByteMs_ = millis();
    if (active_ == nullptr) {
      active_ = this;
      timer_ = timerBegin(1000000);
      timerAttachInterrupt(timer_, &HalfDuplexTransport::onTimer);
      timerAlarm(timer_, 1000, true, 0);
    }
  }

  uint8_t addSensor(uint8_t type, uint8_t length) override {
    if (sensorCount_ < kMaxSensors) {
      sensors_[sensorCount_].type = type;
      sensors_[sensorCount_].length = length;
      sensors_[sensorCount_].value = 0;
      ++sensorCount_;
    }
    return sensorCount_;
  }

  void setSensorMeasurement(uint8_t address, int32_t value) override {
    if (address > 0 && address <= sensorCount_) {
      sensors_[address - 1].value = value;
    }
  }

  void poll() override {
    portENTER_CRITICAL(&mux_);
    if (busy_) {
      portEXIT_CRITICAL(&mux_);
      return;
    }
    busy_ = true;
    portEXIT_CRITICAL(&mux_);

    while (Serial1.available() > 0) {
      const uint32_t now = millis();
      if (now - lastByteMs_ >= 3) {
        state_ = State::kGetLength;
      }
      lastByteMs_ = now;
      const uint8_t value = static_cast<uint8_t>(Serial1.read());
      parseByte(value);
    }

    busy_ = false;
  }

  IbusCounters counters() const override {
    return IbusCounters{polls_, responses_};
  }

 private:
  enum class State { kGetLength, kGetData, kGetChecksumLow, kGetChecksumHigh, kDiscard };

  struct SensorSlot {
    uint8_t type;
    uint8_t length;
    int32_t value;
  };

  static void IRAM_ATTR onTimer() {
    if (active_ != nullptr) {
      active_->poll();
    }
  }

  void parseByte(uint8_t value) {
    switch (state_) {
      case State::kGetLength:
        if (value <= ibus::kMaxFrameLength && value > ibus::kMinFrameLength - 1) {
          ptr_ = 0;
          length_ = static_cast<uint8_t>(value - 3);
          checksum_ = static_cast<uint16_t>(0xFFFF - value);
          state_ = State::kGetData;
        } else {
          state_ = State::kDiscard;
        }
        break;

      case State::kGetData:
        if (ptr_ < sizeof(buffer_)) {
          buffer_[ptr_++] = value;
          checksum_ = static_cast<uint16_t>(checksum_ - value);
          if (ptr_ == length_) {
            state_ = State::kGetChecksumLow;
          }
        } else {
          state_ = State::kDiscard;
        }
        break;

      case State::kGetChecksumLow:
        checksumLow_ = value;
        state_ = State::kGetChecksumHigh;
        break;

      case State::kGetChecksumHigh:
        if (checksum_ == static_cast<uint16_t>((value << 8) | checksumLow_)) {
          handleFrame();
        }
        state_ = State::kDiscard;
        break;

      case State::kDiscard:
      default:
        break;
    }
  }

  void handleFrame() {
    if (length_ != 1) {
      return;
    }
    const uint8_t command = buffer_[0];
    const uint8_t address = command & 0x0F;
    const uint8_t commandType = command & 0xF0;
    if (commandType != ibus::kCommandDiscover && commandType != ibus::kCommandType &&
        commandType != ibus::kCommandValue) {
      return;
    }

    ++polls_;
    if (address == 0 || address > sensorCount_) {
      return;
    }

    const SensorSlot& slot = sensors_[address - 1];
    const ibus::SensorDefinition sensor{address, slot.type, slot.length};
    uint8_t response[ibus::kMaxResponseLength] = {};
    size_t responseLength = 0;
    switch (commandType) {
      case ibus::kCommandDiscover:
        responseLength = ibus::buildDiscoverResponse(sensor, response, sizeof(response));
        break;
      case ibus::kCommandType:
        responseLength = ibus::buildTypeResponse(sensor, response, sizeof(response));
        break;
      case ibus::kCommandValue:
        responseLength =
            ibus::buildValueResponse(sensor, slot.value, response, sizeof(response));
        break;
      default:
        break;
    }
    if (responseLength > 0) {
      send(response, responseLength);
    }
  }

  void send(const uint8_t* data, size_t length) {
    pinMode(kIbusTxPin, OUTPUT);
    Serial1.write(data, length);
    Serial1.flush();
    pinMode(kIbusTxPin, INPUT);
    ++responses_;
  }

  static HalfDuplexTransport* active_;

  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  hw_timer_t* timer_ = nullptr;
  volatile bool busy_ = false;
  volatile uint32_t polls_ = 0;
  volatile uint32_t responses_ = 0;
  SensorSlot sensors_[kMaxSensors] = {};
  uint8_t sensorCount_ = 0;
  State state_ = State::kDiscard;
  uint32_t lastByteMs_ = 0;
  uint8_t buffer_[ibus::kMaxFrameLength] = {};
  uint8_t ptr_ = 0;
  uint8_t length_ = 0;
  uint16_t checksum_ = 0;
  uint8_t checksumLow_ = 0;
};

HalfDuplexTransport* HalfDuplexTransport::active_ = nullptr;

}  // namespace

IbusTransport& createIbusTransport() {
#if defined(IBUS_TRANSPORT_HALFDUPLEX) && IBUS_TRANSPORT_HALFDUPLEX
  static HalfDuplexTransport transport;
#else
  static IBusBMTransport transport;
#endif
  return transport;
}

const char* ibusTransportName() {
#if defined(IBUS_TRANSPORT_HALFDUPLEX) && IBUS_TRANSPORT_HALFDUPLEX
  return "halfduplex";
#else
  return "IBusBM";
#endif
}
