// Covers the Wisotronic component: TLV framing, checksum validation, tag-driven payload routing, and sensor/cover
// handling.

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "esphome/components/wisotronic/wisotronic.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

namespace esphome::wisotronic::testing {

namespace {

// A UART stand-in that hands the component bytes queued in rx.
class FakeUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  std::vector<uint8_t> rx;
};

// Probe subclass that exposes the protected stream_to_packet for direct feeding.
class WisotronicProbe : public WisotronicCustomComponent {
 public:
  explicit WisotronicProbe(FakeUART *uart) : WisotronicCustomComponent(uart) {}
  void feed(const std::vector<uint8_t> &bytes) {
    for (uint8_t b : bytes)
      this->stream_to_packet(b);
  }
};

// Frame and TLV builders following the observed wire format:
//   0x23 <class> 0xff 0x1c 0x10 0x01 0x00 0x01 <L> <payload> <checksum>
// Checksum is the additive sum (low byte) of every byte before the checksum itself.
inline std::vector<uint8_t> build_frame(uint8_t class_byte, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame;
  frame.push_back(0x23);
  frame.push_back(class_byte);
  frame.push_back(0xff);
  frame.push_back(0x1c);
  frame.push_back(0x10);
  frame.push_back(0x01);
  frame.push_back(0x00);
  frame.push_back(0x01);
  frame.push_back(static_cast<uint8_t>(payload.size()));
  frame.insert(frame.end(), payload.begin(), payload.end());
  uint8_t checksum = 0;
  for (uint8_t b : frame)
    checksum += b;
  frame.push_back(checksum);
  return frame;
}

inline std::vector<uint8_t> tlv(uint8_t tag, const std::vector<uint8_t> &val) {
  std::vector<uint8_t> out;
  out.push_back(tag);
  out.push_back(static_cast<uint8_t>(val.size()));
  out.insert(out.end(), val.begin(), val.end());
  return out;
}

inline std::vector<uint8_t> tlv(uint8_t tag, uint8_t v) { return tlv(tag, std::vector<uint8_t>{v}); }

// Expected sensor states, matching the formulas in wisotronic.cpp.
constexpr float expected_control_unit_temp(float raw) { return raw / 2.0f - 25.0f; }
constexpr float expected_outdoor_temp(float raw) { return raw / 2.0f - 25.0f; }
constexpr float expected_indoor_temp(float raw) { return raw / 2.0f - 25.0f; }
constexpr float expected_humidity(float raw) { return raw / 2.0f; }
constexpr float expected_wind_speed(float raw) { return raw / 5.0f; }
constexpr float expected_rain(float raw) { return static_cast<float>(raw); }
constexpr float expected_photo1(float raw) { return raw / 2.0f * 1e3f; }
constexpr float expected_photo2(float raw) { return raw / 2.0f * 1e3f; }
constexpr float expected_photo3(float raw) { return raw / 2.0f * 1e3f; }
constexpr float expected_twilight(float raw) { return raw * 2.0f; }

constexpr float expected_cover_position_awning(float raw) { return 1.0f - raw / 200.0f; }
constexpr float expected_cover_position_window(float raw) { return raw / 200.0f; }
constexpr float expected_cover_position_blind(float raw) { return 1.0f - raw / 200.0f; }
constexpr float expected_cover_tilt_blind(int8_t raw) { return 0.5f - static_cast<int8_t>(raw) / 160.0f; }

}  // namespace

TEST(WisotronicTest, ControlUnitTemperature) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor cu_temp;
  probe.set_control_unit_temperature_sensor(&cu_temp);
  probe.setup();

  probe.feed(build_frame(0x00, tlv(0x00, 0x7E)));

  EXPECT_FLOAT_EQ(cu_temp.state, expected_control_unit_temp(0x7E));
}

TEST(WisotronicTest, OutdoorTemperature) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor out_temp;
  probe.set_outdoor_temperature_sensor(&out_temp);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x08, 0x32)));

  EXPECT_FLOAT_EQ(out_temp.state, expected_outdoor_temp(0x32));
}

TEST(WisotronicTest, IndoorTemperature) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor in_temp;
  probe.set_indoor_temperature_sensor(&in_temp);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x05, 0x64)));

  EXPECT_FLOAT_EQ(in_temp.state, expected_indoor_temp(0x64));
}

TEST(WisotronicTest, Humidity) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor humidity_sensor;
  probe.set_indoor_humidity_sensor(&humidity_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x09, 0x64)));

  EXPECT_FLOAT_EQ(humidity_sensor.state, expected_humidity(0x64));
}

TEST(WisotronicTest, WindSpeed) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor wind_speed_sensor;
  probe.set_wind_speed_sensor(&wind_speed_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x03, 0x11)));

  EXPECT_FLOAT_EQ(wind_speed_sensor.state, expected_wind_speed(0x11));
}

TEST(WisotronicTest, Rain) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor rain_sensor;
  probe.set_rain_sensor(&rain_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x04, 0x19)));

  EXPECT_FLOAT_EQ(rain_sensor.state, expected_rain(0x19));
}

TEST(WisotronicTest, Photo1Brightness) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor photo1_sensor;
  probe.set_photo1_brightness_sensor(&photo1_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x0a, 0xc8)));

  EXPECT_FLOAT_EQ(photo1_sensor.state, expected_photo1(0xc8));
}

TEST(WisotronicTest, Photo2Brightness) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor photo2_sensor;
  probe.set_photo2_brightness_sensor(&photo2_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x0b, 0x12)));

  EXPECT_FLOAT_EQ(photo2_sensor.state, expected_photo2(0x12));
}

TEST(WisotronicTest, Photo3Brightness) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor photo3_sensor;
  probe.set_photo3_brightness_sensor(&photo3_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x0c, 0xf0)));

  EXPECT_FLOAT_EQ(photo3_sensor.state, expected_photo3(0xf0));
}

TEST(WisotronicTest, TwilightBrightness) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor twilight_sensor;
  probe.set_twilight_brightness_sensor(&twilight_sensor);
  probe.setup();

  probe.feed(build_frame(0xfd, tlv(0x0d, 0x1f)));

  EXPECT_FLOAT_EQ(twilight_sensor.state, expected_twilight(0x1f));
}

TEST(WisotronicTest, AwningCover) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  WisotronicCover awning;
  awning.set_parent(&probe);
  probe.set_awning_cover(&awning);
  probe.setup();

  // tag 0x00, len 3, data 00 00 64 -> position = 1 - 0x64/200
  probe.feed(build_frame(0xfd, tlv(0x00, {0x00, 0x00, 0x64})));

  EXPECT_FLOAT_EQ(awning.position, expected_cover_position_awning(0x64));
}

TEST(WisotronicTest, WindowCover) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  WisotronicCover window;
  window.set_parent(&probe);
  probe.set_window_cover(&window);
  probe.setup();

  // tag 0x01, len 3, data 00 00 C8 -> position = 0xC8/200
  probe.feed(build_frame(0xfd, tlv(0x01, {0x00, 0x00, 0xC8})));

  EXPECT_FLOAT_EQ(window.position, expected_cover_position_window(0xC8));
}

TEST(WisotronicTest, Blind1Cover) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  WisotronicCover blind1;
  blind1.set_parent(&probe);
  probe.set_blind1_cover(&blind1);
  probe.setup();

  // tag 0x02, len 5, data: pos1=00 tilt1=00 pos2=00 tilt2=00
  probe.feed(build_frame(0xfd, tlv(0x02, {0x00, 0x00, 0x00, 0x00, 0x00})));

  EXPECT_FLOAT_EQ(blind1.position, expected_cover_position_blind(0x00));
}

TEST(WisotronicTest, Blind2Cover) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  WisotronicCover blind2;
  blind2.set_parent(&probe);
  probe.set_blind2_cover(&blind2);
  probe.setup();

  // tag 0x02, len 5, data: pos1=00 tilt1=00 pos2=00 tilt2=00
  probe.feed(build_frame(0xfd, tlv(0x02, {0x00, 0x00, 0x00, 0x00, 0x00})));

  EXPECT_FLOAT_EQ(blind2.position, expected_cover_position_blind(0x00));
}

TEST(WisotronicTest, Blind1PositionOnlyTag) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  WisotronicCover blind1;
  blind1.set_parent(&probe);
  probe.set_blind1_cover(&blind1);
  probe.setup();

  // tag 0x12, len 1, data 0x00 -> position = 1 - 0/200 = 1.0
  probe.feed(build_frame(0xfd, tlv(0x12, 0x00)));

  EXPECT_FLOAT_EQ(blind1.position, expected_cover_position_blind(0x00));
}

TEST(WisotronicTest, Blind1TiltOnlyTag) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  WisotronicCover blind1;
  blind1.set_parent(&probe);
  probe.set_blind1_cover(&blind1);
  probe.setup();

  // tag 0x13, len 1, data 0xff -> tilt = 0.5 - (-1)/160 = 0.50625
  probe.feed(build_frame(0xfd, tlv(0x13, 0xff)));

  EXPECT_FLOAT_EQ(blind1.tilt, expected_cover_tilt_blind(static_cast<int8_t>(0xff)));
}

TEST(WisotronicTest, BadChecksumDropsFrame) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor cu_temp;
  probe.set_control_unit_temperature_sensor(&cu_temp);
  probe.setup();

  std::vector<uint8_t> frame = build_frame(0x00, tlv(0x00, 0x7E));
  frame.back() = (frame.back() + 1) & 0xFF;  // corrupt the checksum
  probe.feed(frame);

  EXPECT_TRUE(std::isnan(cu_temp.state));
}

TEST(WisotronicTest, MultipleTagsInOneFrame) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  sensor::Sensor in_temp;
  probe.set_indoor_temperature_sensor(&in_temp);
  sensor::Sensor rain_sensor;
  probe.set_rain_sensor(&rain_sensor);
  probe.setup();

  std::vector<uint8_t> payload;
  std::vector<uint8_t> temp_tlv = tlv(0x05, 0x64);  // indoor temp 0x64 -> 31.0
  std::vector<uint8_t> rain_tlv = tlv(0x04, 0x19);  // rain 0x19 -> 25
  payload.insert(payload.end(), temp_tlv.begin(), temp_tlv.end());
  payload.insert(payload.end(), rain_tlv.begin(), rain_tlv.end());

  probe.feed(build_frame(0xfd, payload));

  EXPECT_FLOAT_EQ(in_temp.state, expected_indoor_temp(0x64));
  EXPECT_FLOAT_EQ(rain_sensor.state, expected_rain(0x19));
}

TEST(WisotronicTest, EmptyPayloadFrameIsIgnored) {
  FakeUART uart;
  WisotronicProbe probe(&uart);
  probe.setup();

  // L = 0 means no TLVs; the parser must not crash or publish anything.
  probe.feed(build_frame(0xfd, {}));

  SUCCEED();
}

}  // namespace esphome::wisotronic::testing
